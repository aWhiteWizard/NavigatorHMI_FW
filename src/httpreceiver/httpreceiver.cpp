/*
 * @FilePath: \NavigatorHMI_FW\src\httpreceiver\httpreceiver.cpp
 * @Description: K-8b FW 接收端实现——qthttpserver 路由 + TransferSession 单客户端 + 容器校验 + 原子落盘
 */
#include "httpreceiver.h"

#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QHttpServerResponder>   // M-3 ④：异步传输响应器（responder 形式路由）
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>   // T-1a（2026-09-05）：部署前磁盘空闲查询（cap=disk_free×2/3 动态上限）
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QDateTime>
#include <QUuid>

#include <unistd.h>   // ::rename（L-A4：POSIX 原子覆盖替代 QFile::rename）
#include <cerrno>     // errno
#include <thread>     // M-3 ④（R1）：receiveAndInstall 后台线程执行（避免冻结 GUI 事件循环）
#include <memory>     // std::make_unique/move（responder 异步持有）

#include "runtime/runtimebus.h"
#include "runtime/deviceinfo.h"
#include "runtime/storageinfo.h"
#include "runtime/vncmirror.h"     // K-9：/api/vnc 端点
#include "runtime/devicemeta.h"    // K-9：设备身份推导单点
#include "runtime/fwconfig.h"      // K-9 评论3：VNC 端口配置单点

// qzipreader_p.h（private 头——CMakeLists 已含 QtGui_PRIVATE_INCLUDE_DIRS，R3 先例）
#include <QtGui/private/qzipreader_p.h>

namespace navihmi {

namespace {

constexpr int kTransferPort = 80;                    // HTTP 默认端口
constexpr int kMaxLegacySinglePostBytes = 256 * 1024 * 1024;   // T-1a：整包（无分块头）单 POST 内存兜底 256MB（2G 内存设备安全线——审查 🟡 5：512MB 逼近 OOM + tmpfs 980M 解压上限；数百 MB 部署一律分块）

/// T-1a：工程部署目标分区剩余字节（cap=disk_free×2/3 计算源——与落盘同分区：/mnt/user/userdata）
qint64 deployDiskFreeBytes()
{
    const QString deployDir = StorageInfo::defaultProjectPath().section(QLatin1Char('/'), 0, -2);
    const QStorageInfo si(deployDir);
    if (!si.isValid() || !si.isReady()) return -1;
    return si.bytesFree();
}
qint64 deployDiskTotalBytes()
{
    const QString deployDir = StorageInfo::defaultProjectPath().section(QLatin1Char('/'), 0, -2);
    const QStorageInfo si(deployDir);
    if (!si.isValid() || !si.isReady()) return -1;
    return si.bytesTotal();
}

/// 读 manifest.json（容器条目数组）——返回条目 map: name → {type,target,size,sha256,version}
bool readManifest(const QString& dir, QJsonArray& out)
{
    QFile f(dir + QStringLiteral("/manifest.json"));
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return false;
    out = doc.array();
    return true;
}

/// 文件 SHA256（hex 小写）
QString sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return QString::fromLatin1(hash.result().toHex());
}

/// 防路径穿越 + 跨平台：target 必须相对、只用 / 分隔（拒 \ 反斜杠——Windows 打包产物，同 zip-backslash 负样本根因）、无 .. 段
bool safeRelPath(const QString& target)
{
    if (target.isEmpty() || target.startsWith(QLatin1Char('/')) || target.contains(QLatin1Char('\\'))) return false;
    const QStringList parts = target.split(QLatin1Char('/'));
    for (const QString& p : parts) {
        if (p == QLatin1String("..") || p.isEmpty()) return false;
    }
    return true;
}

} // namespace

HttpReceiver::HttpReceiver(RuntimeBus* bus, DeviceInfo* devInfo, QObject* parent)
    : QObject(parent)
    , m_bus(bus)
    , m_deviceInfo(devInfo)
{
    // T-1a：分块会话空闲 TTL（审查 🔴 1-2）——块间超时自动清理，PC 断连后设备不永久拒收
    m_chunkTimer.setSingleShot(true);
    m_chunkTimer.setInterval(30 * 1000);
    connect(&m_chunkTimer, &QTimer::timeout, this, &HttpReceiver::onChunkTimeout);
    setupRoutes();
}

HttpReceiver::~HttpReceiver() = default;

void HttpReceiver::setVncMirror(VncMirror* vm) { m_vncMirror = vm; }   // K-9

bool HttpReceiver::start()
{
    if (!m_server.listen(QHostAddress::Any, kTransferPort)) {
        qWarning().noquote() << "HTTP 接收端监听失败（端口" << kTransferPort << "被占用或权限不足）——FW 继续正常运行";
        return false;
    }
    qInfo().noquote() << "HTTP 接收端就绪: 监听" << kTransferPort << "（/api/device/info · /api/version · /api/transfer）";
    return true;
}

void HttpReceiver::setupRoutes()
{
    // GET /api/device/info——型号/尺寸/ID/固件版本（A1 连接测试三分数据源 + 扫描发现）
    m_server.route(QStringLiteral("/api/device/info"), QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest&) { return handleDeviceInfo(); });

    // GET /api/version——工程版本（CheckVersion 三分语义：version=工程版本口径）
    m_server.route(QStringLiteral("/api/version"), QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest&) { return handleVersion(); });

    // D-B4：GET /api/progress——PC 轮询设备端下载/安装进度（真同步：PC 进度条显示设备实际处理进度）
    m_server.route(QStringLiteral("/api/progress"), QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest&) { return handleProgress(); });

    // Q-1（2026-09-04）：GET /api/log——设备运行日志尾部（/tmp/navihmi.log 由 S99qt-test 重定向；
    // 诊断 QML import/运行时错误用——PC 拉取免 SSH）。未鉴权——局域网调试口可接受（设备管理面本就开放 HTTP）
    m_server.route(QStringLiteral("/api/log"), QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest&) { return handleLog(); });

    // POST /api/transfer——工程部署容器上传（单客户端串行）
    // M-3 ④（R1）：responder 异步形式——qthttpserver 6.4 处理器运行在服务器对象所在线程（=GUI 主线程），
    // receiveAndInstall 秒级耗时若同步执行会冻结事件循环（进度条无法重绘）；改后台线程执行 + 主线程收尾
    m_server.route(QStringLiteral("/api/transfer"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request, QHttpServerResponder&& responder) { handleTransfer(request, std::move(responder)); });

    // K-9：POST /api/vnc {enable}——VNC 运行时启停（设备面板对等；proto enable_vnc=21 启动默认值，运行时指令覆盖）
    m_server.route(QStringLiteral("/api/vnc"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) { return handleVnc(request); });

    // K-9：POST /api/blink {enable}——设备闪烁（屏幕亮灭交替 ~1s；多设备定位）
    m_server.route(QStringLiteral("/api/blink"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) { return handleBlink(request); });
}

QString HttpReceiver::deviceModel() const
{
    // 2026-08-30 用户 Check 指正：设备型号 = 设备自身硬件身份（物理屏默认分辨率查表），
    // 与工程内容无关——PC 需要时向设备要（/api/device/info），不因加载工程有无型号而改变
    return deviceModelFor();
}

QString HttpReceiver::deviceSizeInch() const
{
    return deviceSizeInchFor();
}

QHttpServerResponse HttpReceiver::handleDeviceInfo()
{
    const QString ip = m_deviceInfo ? m_deviceInfo->ipAddress() : QString();
    const QString fw = m_deviceInfo ? m_deviceInfo->appVersion() : QString();
    // 2026-09-04 调试 OTA：firmware_ts = 当前生效固件 OTA 打包时刻（未 OTA 装过/旧固件 "0"）——
    // PC 端版本前置对调试包（恒 v1.1.0）改按打包时刻先后判断（后打的包可覆盖前一个）
    const QString fwTs = m_deviceInfo ? m_deviceInfo->otaTimestamp() : QStringLiteral("0");
    // T-1a（2026-09-05）：部署目标分区磁盘（PC 上传前预检 cap=disk_free×2/3——动态上限，替代固定 64MB）
    const qint64 diskFree = deployDiskFreeBytes();
    const qint64 diskTotal = deployDiskTotalBytes();
    const QJsonObject obj{
        { QStringLiteral("model"), deviceModel() },
        { QStringLiteral("sizeInch"), deviceSizeInch() },
        { QStringLiteral("id"), ip },
        { QStringLiteral("version"), fw },      // 固件版本（术语口径：/api/device/info version=固件版本）
        { QStringLiteral("firmware_ts"), fwTs },   // 当前固件 OTA 打包时刻（调试同版覆盖判断用）
        { QStringLiteral("disk_total"), diskTotal },   // 部署分区总字节（-1=查询失败）
        { QStringLiteral("disk_free"), diskFree },     // 部署分区剩余字节（cap = ×2/3，用户拍板 2026-09-05）
    };
    QHttpServerResponse resp(QJsonDocument(obj).toJson(QJsonDocument::Compact), QHttpServerResponse::StatusCode::Ok);
    resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
    return resp;
}

QHttpServerResponse HttpReceiver::handleVersion()
{
    QString version;
    if (m_bus) version = m_bus->project().version;
    const QJsonObject obj{
        { QStringLiteral("version"), version },   // 工程版本（CheckVersion 三分语义）
    };
    QHttpServerResponse resp(QJsonDocument(obj).toJson(QJsonDocument::Compact), QHttpServerResponse::StatusCode::Ok);
    resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
    return resp;
}

/// D-B4：GET /api/progress——PC 轮询设备端下载/安装进度（真同步）。
/// 返回 { progress: 0~100（-1=无/失败）, stage: 阶段描述（由 progress 派生）, active: 是否传输中 }。
/// progress 原子读取（receiveAndInstall 后台线程写，本 handler 服务器线程读——QAtomicInteger 无数据竞争）；
/// stage 由 progress 派生——后台线程只写原子 int，不共享 QString（防数据竞争，审查 🔴 修复）。
QHttpServerResponse HttpReceiver::handleProgress()
{
    const int pct = m_lastProgress.loadRelaxed();
    const QJsonObject obj{
        { QStringLiteral("progress"), pct },
        { QStringLiteral("stage"), stageForProgress(pct) },
        { QStringLiteral("active"), m_transferActive.loadRelaxed() },
    };
    return jsonResponse(obj, QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse HttpReceiver::handleLog()
{
    // Q-1（2026-09-04）：读 /tmp/navihmi.log 尾部（最近 32KB——完整日志可能很大，诊断取尾部最新错误段）；
    // 文件不存在/空 → 返回空文本（不报错——设备可能由非 S99 方式启动无重定向日志）
    const QString logPath = QStringLiteral("/tmp/navihmi.log");
    const qint64 kTailBytes = 32 * 1024;
    QFile f(logPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return QHttpServerResponse(QStringLiteral(""), QHttpServerResponse::StatusCode::Ok);
    }
    QByteArray tail;
    if (f.size() > kTailBytes) {
        f.seek(f.size() - kTailBytes);
        tail = f.read(kTailBytes);
        const int nl = tail.indexOf('\n');
        if (nl > 0) tail = tail.mid(nl + 1);   // 对齐行首（避免截半行）
    } else {
        tail = f.readAll();
    }
    f.close();
    QHttpServerResponse resp(tail, QHttpServerResponse::StatusCode::Ok);
    resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("text/plain; charset=utf-8"));
    return resp;
}

/// D-B4：进度值 → 阶段描述（派生，不共享跨线程 QString；与 transferProgress 信号各 emit 点阶段一致）
/// T-1a（2026-09-05）：进度刻度统一——接收段 0~70（分块块进度/整包接收完成 70）、安装段 70~100
///（解压 70→85、写入 85→99、完成 100）
QString HttpReceiver::stageForProgress(int pct)
{
    if (pct < 0) return QStringLiteral("传输失败");
    if (pct < 70) return QStringLiteral("接收中…");
    if (pct < 85) return QStringLiteral("解压安装包…");
    if (pct < 100) return QStringLiteral("写入资源…");
    return QStringLiteral("安装完成");
}

void HttpReceiver::handleTransfer(const QHttpServerRequest& request, QHttpServerResponder&& responder)
{
    // 🔴 修复（2026-09-06 用户 218MB 分块部署实测）：分块请求**豁免并发锁续块误拒**——原入口无条件
    // `testAndSetAcquire` 并发上限 1：分块会话首块置锁后，**同会话续块 POST 再进本入口被忙检查 409
    // TRANSFER_BUSY**（PC 诊断日志：chunk 0/61 CHUNK_OK → chunk 1/61 409 BUSY）→ PC 第 2 块即失败。
    // 修复：分块请求——锁空 → 首块占锁；锁占 → 仅同会话续块（X-Tf-Id == m_chunkId）放行，否则 busy。
    // 整包路径忙检查保持原样（整包与分块会话互斥）。
    const bool chunkedReq = !request.value(QByteArrayLiteral("X-Tf-Total")).isEmpty()
        || !request.value(QByteArrayLiteral("x-tf-total")).isEmpty();
    if (chunkedReq) {
        const QString reqId = QString::fromLatin1(request.value(QByteArrayLiteral("X-Tf-Id")));
        if (m_transferActive.loadRelaxed()) {
            if (reqId.isEmpty() || m_chunkId.isEmpty() || reqId != m_chunkId) {
                // 锁被整包/另一分块会话占用，且非本会话续块 → busy
                const QJsonObject err{
                    { QStringLiteral("code"), QStringLiteral("TRANSFER_BUSY") },
                    { QStringLiteral("message"), QStringLiteral("已有传输任务进行中") },
                    { QStringLiteral("stage"), QStringLiteral("Upload") },
                };
                responder.write(QJsonDocument(err).toJson(QJsonDocument::Compact),
                                QByteArrayLiteral("application/json"),
                                QHttpServerResponder::StatusCode::Conflict);
                return;
            }
            // 同会话续块 → 放行（锁保持；handleChunkedTransfer 校验 index/顺序/cap）
        } else {
            m_transferActive.storeRelease(true);   // 分块首块：占锁
        }
        handleChunkedTransfer(request, std::move(responder));
        return;
    }

    // 整包路径：并发上限 1（单客户端串行；多余请求拒绝并报错——服务约束）
    if (!m_transferActive.testAndSetAcquire(false, true)) {
        const QJsonObject err{
            { QStringLiteral("code"), QStringLiteral("TRANSFER_BUSY") },
            { QStringLiteral("message"), QStringLiteral("已有传输任务进行中") },
            { QStringLiteral("stage"), QStringLiteral("Upload") },
        };
        responder.write(QJsonDocument(err).toJson(QJsonDocument::Compact),
                        QByteArrayLiteral("application/json"),
                        QHttpServerResponder::StatusCode::Conflict);
        return;
    }

    // T-1a（2026-09-05）：无分块头 = 整包旧路径/第三方 → 下方原流程（≤kMaxLegacySinglePostBytes 内存兜底）

    const QByteArray body = request.body();
    // 整包内存兜底（T-1a：旧整包路径上限 64MB→256MB 内存安全线——64MB 为分块前瓶颈；仍超提示用分块）
    if (body.size() > kMaxLegacySinglePostBytes) {
        m_transferActive.storeRelease(false);
        const QJsonObject err{
            { QStringLiteral("code"), QStringLiteral("TRANSFER_TOO_LARGE") },
            { QStringLiteral("message"), QStringLiteral("上传超过整包内存上限（%1 MB）——请使用分块上传")
                .arg(kMaxLegacySinglePostBytes / (1024 * 1024)) },
            { QStringLiteral("stage"), QStringLiteral("Upload") },
        };
        responder.write(QJsonDocument(err).toJson(QJsonDocument::Compact),
                        QByteArrayLiteral("application/json"),
                        QHttpServerResponder::StatusCode::PayloadTooLarge);
        return;
    }
    // R 排查（2026-09-05）：工程被 96B「测试工程」覆盖来源成谜（用户未操作部署但设备收多次容器）——
    // 接收入口打来源 IP + 大小 + 魔数（NHFW=固件 / PK=工程容器），下次部署可精确定位谁在发
    qInfo().noquote() << "HttpReceiver 接收: from=" << request.remoteAddress().toString()
                      << " size=" << body.size()
                      << " magic=" << (body.size() >= 4 ? QByteArray(body.constData(), 4).toHex() : QByteArray("--"));
    // M-3 ④：接收开始 → QML 屏幕进度条（退导航→进度→满停→自动打开）
    m_lastProgress.storeRelaxed(70);   // T-1a：整包接收完成 = 进入安装段刻度起点（0~70 接收段仅分块路径实时走）
    emit transferProgress(70, QStringLiteral("接收完成，开始安装"));
    m_responder = std::make_unique<QHttpServerResponder>(std::move(responder));

    // M-3 ④（R1 修复）：receiveAndInstall 移出 GUI 线程——qthttpserver 6.4 处理器跑在服务器对象线程
    // （=main() 所在线程），1220 瓦片 demo 解压/校验/落盘秒级耗时，同步执行冻结事件循环 →
    // 进度条无法重绘、触摸/VNC 无响应；后台线程执行安装，完成后回主线程写响应（QTcpSocket 非线程安全）
    std::thread([this, body]() {
        QString projectPath;
        bool isFirmware = false;
        const QString error = receiveAndInstall(body, projectPath, &isFirmware);
        QMetaObject::invokeMethod(this, [this, error, projectPath, isFirmware]() {
            finishTransfer(error, projectPath, isFirmware);
        }, Qt::QueuedConnection);
    }).detach();
}

/// T-1a（2026-09-05 T 循环）：分块上传会话——qthttpserver 6.4 请求体整读无流式 API → PC 分 N 块循环 POST。
/// 请求头：X-Tf-Id（会话 uuid）、X-Tf-Index（0..N-1）、X-Tf-Total（N）、X-Tf-Size（总字节，首块必带）。
/// 首块（index=0）建会话（deployDir/.transfer/<id>.zip，/tmp 为 tmpfs 980M 放不下大包）+ cap=disk_free×2/3 预检；
/// 续块校验会话 id + 顺序 → append 落盘（单块内存 4MB 级——大包不受内存限制）+ 块进度实时写；
/// 末块（index=total-1）保留 responder 转后台 receiveAndInstallFromZip 安装链（刻度 70→100）。
/// 非末块同步写小块响应（落盘毫秒级不冻结 GUI）；异常/新会话 → abortChunkSession 清理。
/// 并发：m_transferActive 由 handleTransfer 统一置位（单客户端串行），本函数只做会话状态机。
void HttpReceiver::handleChunkedTransfer(const QHttpServerRequest& request, QHttpServerResponder&& responder)
{
    auto fail = [&](const QString& code, const QString& message, QHttpServerResponder::StatusCode st) {
        // 审查 🔴 1-1：失败必须复位进度并发 -1 失败信号——否则 QML 进度遮罩常显吞触摸 + 轮询残留旧值
        m_lastProgress.storeRelaxed(-1);
        emit transferProgress(-1, message);
        m_transferActive.storeRelease(false);
        m_chunkTimer.stop();   // T-1a：会话清理连带停 TTL
        abortChunkSession();
        const QJsonObject err{
            { QStringLiteral("code"), code },
            { QStringLiteral("message"), message },
            { QStringLiteral("stage"), QStringLiteral("Upload") },
        };
        responder.write(QJsonDocument(err).toJson(QJsonDocument::Compact),
                        QByteArrayLiteral("application/json"), st);
    };

    const QString id = QString::fromLatin1(request.value(QByteArrayLiteral("X-Tf-Id")));
    const int total = QString::fromLatin1(request.value(QByteArrayLiteral("X-Tf-Total"))).toInt();
    const int index = QString::fromLatin1(request.value(QByteArrayLiteral("X-Tf-Index"))).toInt();
    const qint64 totalSize = QString::fromLatin1(request.value(QByteArrayLiteral("X-Tf-Size"))).toLongLong();
    const QByteArray body = request.body();
    // 审查 🟡（2026-09-05 FW 复审）：X-Tf-Id 路径净化——仅 [A-Za-z0-9_-]（QUuid N 格式 32 hex）——
    // 恶意 id（如 ../../x）拼入 .transfer/<id>.zip 可路径穿越写/删任意文件（root 进程，局域网未鉴权面）
    const auto validIdChars = [](const QString& s) {
        if (s.isEmpty() || s.size() > 64) return false;
        for (const QChar c : s)
            if (!c.isLetterOrNumber() && c != QLatin1Char('-') && c != QLatin1Char('_'))
                return false;
        return true;
    };
    if (!validIdChars(id) || total <= 0 || index < 0 || index >= total) {
        fail(QStringLiteral("TRANSFER_INVALID"),
             QStringLiteral("分块上传头非法（X-Tf-Id/Index/Total）"), QHttpServerResponder::StatusCode::BadRequest);
        return;
    }
    // 审查 🟡 3-1：单块 body 上限（qthttpserver 整读进内存后才进 handler——恶意巨块撑爆内存/直写盘；
    // 正常客户端 4MB 块远低于此）
    if (body.size() > kMaxLegacySinglePostBytes) {
        fail(QStringLiteral("TRANSFER_CHUNK_TOO_LARGE"),
             QStringLiteral("单块超过内存上限（%1 MB）").arg(kMaxLegacySinglePostBytes / (1024 * 1024)),
             QHttpServerResponder::StatusCode::PayloadTooLarge);
        return;
    }

    // ── 首块：建会话 + cap 预检 ──
    if (index == 0) {
        abortChunkSession();   // 上一会话残留（异常未清）→ 清理重建
        m_chunkId = id;
        m_chunkTotal = total;
        m_chunkTotalSize = totalSize;
        m_chunkReceivedSize = body.size();
        m_chunkNextIndex = 1;
        // cap 预检（用户拍板 2026-09-05：上传量 ≤ 设备最大存储 2/3——部署分区空闲 ×2/3；
        // disk_free 查询失败（cap<=0）时放行由累计写失败兜底）
        const qint64 cap = deployDiskFreeBytes() / 3 * 2;
        if (m_chunkTotalSize <= 0 || (cap > 0 && (m_chunkTotalSize > cap || body.size() > cap))) {
            fail(QStringLiteral("TRANSFER_TOO_LARGE"),
                 QStringLiteral("部署包超过设备剩余空间 2/3（需 %1 MB / 上限 %2 MB）")
                     .arg(m_chunkTotalSize / (1024 * 1024)).arg(cap > 0 ? cap / (1024 * 1024) : 0),
                 QHttpServerResponder::StatusCode::PayloadTooLarge);
            return;
        }
        const QString deployDir = StorageInfo::defaultProjectPath().section(QLatin1Char('/'), 0, -2);
        const QString dir = deployDir + QStringLiteral("/.transfer");
        if (!QDir().mkpath(dir)) {
            fail(QStringLiteral("TRANSFER_FAILED"), QStringLiteral("分块目录创建失败"),
                 QHttpServerResponder::StatusCode::InternalServerError);
            return;
        }
        m_chunkZipPath = dir + QLatin1Char('/') + id + QStringLiteral(".zip");
        QFile f(m_chunkZipPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(body) != body.size()) {
            fail(QStringLiteral("TRANSFER_FAILED"), QStringLiteral("分块首块写入失败"),
                 QHttpServerResponder::StatusCode::InternalServerError);
            return;
        }
        qInfo().noquote() << "HttpReceiver 分块会话开始: id=" << id
                          << " total=" << total << " size=" << m_chunkTotalSize;
    } else {
        // ── 续块：会话/顺序/总量校验 + cap 累计 + append ──
        if (id != m_chunkId || total != m_chunkTotal || index != m_chunkNextIndex || m_chunkZipPath.isEmpty()) {
            fail(QStringLiteral("TRANSFER_CHUNK_MISMATCH"),
                 QStringLiteral("分块会话不匹配或乱序（期望块 %1/%2）").arg(m_chunkNextIndex).arg(m_chunkTotal),
                 QHttpServerResponder::StatusCode::Conflict);
            return;
        }
        m_chunkReceivedSize += body.size();
        const qint64 cap = deployDiskFreeBytes() / 3 * 2;
        if (cap > 0 && m_chunkReceivedSize > cap) {   // 双保险（首块谎报大小场景）
            fail(QStringLiteral("TRANSFER_TOO_LARGE"), QStringLiteral("接收量超过设备剩余空间 2/3"),
                 QHttpServerResponder::StatusCode::PayloadTooLarge);
            return;
        }
        QFile f(m_chunkZipPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append) || f.write(body) != body.size()) {
            fail(QStringLiteral("TRANSFER_FAILED"), QStringLiteral("分块写入失败"),
                 QHttpServerResponder::StatusCode::InternalServerError);
            return;
        }
        ++m_chunkNextIndex;
    }
    m_chunkTimer.start();   // T-1a：TTL 刷新（块间空闲 30s 超时自动清理——审查 🔴 1-2）

    // ── 块进度（接收段 0~70 实时——已收块 = m_chunkNextIndex）──
    const int pct = m_chunkNextIndex * 70 / m_chunkTotal;
    m_lastProgress.storeRelaxed(pct);
    emit transferProgress(pct, pct >= 70 ? QStringLiteral("接收完成，开始安装") : QStringLiteral("接收中…"));

    if (index == m_chunkTotal - 1) {   // 审查 🟡 1-3：末块判定以会话总量（m_chunkTotal）而非请求头 total
        // ── 末块：保留 responder → 后台安装链（70→100 刻度）──
        m_responder = std::make_unique<QHttpServerResponder>(std::move(responder));
        const QString zipPath = m_chunkZipPath;
        // 审查 🟡 1-4：交棒清空 m_chunkZipPath（zipPath 已拷局部）——abort 误删保护自足（不再依赖并发锁巧合）
        m_chunkZipPath.clear();
        m_chunkId.clear();
        m_chunkTotal = 0;
        m_chunkTimer.stop();   // 交棒安装链——TTL 使命完成
        std::thread([this, zipPath]() {
            QString projectPath;
            const QString error = receiveAndInstallFromZip(zipPath, projectPath);
            if (!error.isEmpty() && !QFile::remove(zipPath))   // 审查 🟡 6：安装失败显式删分块 zip（成功路径 FromZip step6 已删）
                qWarning().noquote() << "分块 zip 清理失败（安装错误路径）: " << zipPath;
            QMetaObject::invokeMethod(this, [this, error, projectPath]() {
                finishTransfer(error, projectPath);
            }, Qt::QueuedConnection);
        }).detach();
    } else {
        // 非末块：同步小块响应（落盘毫秒级不冻结事件循环）
        const QJsonObject ok{
            { QStringLiteral("code"), QStringLiteral("CHUNK_OK") },
            { QStringLiteral("progress"), pct },
            { QStringLiteral("stage"), QStringLiteral("Upload") },
        };
        responder.write(QJsonDocument(ok).toJson(QJsonDocument::Compact),
                        QByteArrayLiteral("application/json"),
                        QHttpServerResponder::StatusCode::Ok);
    }
}

/// T-1a：分块会话清理（失败/新会话重建时；主线程）——删临时 zip + 复位会话状态。
/// 注：m_transferActive 由调用方（fail lambda/handleTransfer）管理；交棒后 m_chunkZipPath/m_chunkId 已清空，
/// abort 不会误删安装链使用中的 zip（自足保护——审查 🟡 1-4）。
void HttpReceiver::abortChunkSession()
{
    m_chunkTimer.stop();
    if (!m_chunkZipPath.isEmpty()) {
        QFile::remove(m_chunkZipPath);
        m_chunkZipPath.clear();
    }
    m_chunkId.clear();
    m_chunkTotalSize = 0;
    m_chunkReceivedSize = 0;
    m_chunkNextIndex = 0;
    m_chunkTotal = 0;
}

/// T-1a：分块会话空闲超时（审查 🔴 1-2）——块间 30s 无新块：PC 断连/崩溃后自动清理（删 zip + 复位
/// 状态 + 释放并发锁 + 进度 -1），设备不永久拒收（新会话首块可正常开始）。
void HttpReceiver::onChunkTimeout()
{
    if (m_chunkId.isEmpty()) return;   // 无活动分块会话（已交棒/已清）
    qWarning().noquote() << "HttpReceiver 分块会话空闲超时，清理: id=" << m_chunkId;
    m_lastProgress.storeRelaxed(-1);
    emit transferProgress(-1, QStringLiteral("分块传输超时中断"));
    m_transferActive.storeRelease(false);
    abortChunkSession();
}

/// M-3 ④：主线程收尾——写 HTTP 响应 + 释放并发锁 + 信号（成功→projectPackageReady；失败→progress(-1)）
/// D1 审查 🔴：isFirmware=true（.fw OTA 传输）时成功**不触发 projectPackageReady**（工程重载链）——
/// OTA 走 firmwarePackageReady + otaupdater 安装重启；仅 .navihmi/zip 工程部署触发工程重载
/// 批 D 收尾（httreceiver「假成功」修复）：.fw OTA 校验通过 = staging 就绪 ≠ 安装成功——此处成功分支
/// **不立即写响应**（m_pendingFirmwareInstall=true，响应器保留），等 OtaUpdater.installFinished →
/// completeFirmwareInstall 以真实结果写响应（原实现校验通过即报成功，install 失败静默 → PC 误判升级成功）
void HttpReceiver::finishTransfer(const QString& error, const QString& projectPath, bool isFirmware)
{
    m_transferActive.storeRelease(false);
    if (!error.isEmpty()) {
        qWarning().noquote() << "传输失败:" << error;
        m_lastProgress.storeRelaxed(-1);   // D-B4：失败 → 进度复位（PC 轮询得 -1 判定失败）
        emit transferProgress(-1, error);   // M-3 ④：失败 → QML 进度条恢复（隐藏）
        const QJsonObject err{
            { QStringLiteral("code"), QStringLiteral("TRANSFER_FAILED") },
            { QStringLiteral("message"), error },
            { QStringLiteral("stage"), QStringLiteral("Install") },
        };
        if (m_responder)
            m_responder->write(QJsonDocument(err).toJson(QJsonDocument::Compact),
                               QByteArrayLiteral("application/json"),
                               QHttpServerResponder::StatusCode::BadRequest);
        m_responder.reset();
        m_pendingFirmwareInstall = false;
    } else if (isFirmware) {
        // .fw OTA：staging 就绪（校验通过），但安装未执行/未完成——**defer 响应**，等 installFinished
        qInfo().noquote() << ".fw 固件 staging 就绪，等待 OTA 安装结果回传（httreceiver deferred 响应）";
        m_pendingFirmwareInstall = true;
        // 保留 m_responder（不 reset）——completeFirmwareInstall 写真实结果后释放
    } else {
        // 校验通过、已落盘 → 通知主程序重载工程（下载事务性：失败路径不动当前工程）
        emit projectPackageReady(projectPath);
        const QJsonObject ok{
            { QStringLiteral("code"), QStringLiteral("SUCCESSFUL_REBOOT") },
            { QStringLiteral("message"), QStringLiteral("部署成功，工程重载中") },
            { QStringLiteral("stage"), QStringLiteral("Finish") },
        };
        if (m_responder)
            m_responder->write(QJsonDocument(ok).toJson(QJsonDocument::Compact),
                               QByteArrayLiteral("application/json"),
                               QHttpServerResponder::StatusCode::Ok);
        m_responder.reset();
    }
}

/// 批 D 收尾：.fw OTA 安装结果回调（main.cpp 桥接 OtaUpdater.installFinished）——写真实传输响应。
/// 主线程调用（installFinished 经 connect 队列到主线程——与 responder 写线程一致，防 QTcpSocket 跨线程）。
void HttpReceiver::completeFirmwareInstall(const QString& error)
{
    writeFirmwareTransferResult(error);
    m_pendingFirmwareInstall = false;
}

void HttpReceiver::writeFirmwareTransferResult(const QString& error)
{
    if (!error.isEmpty()) {
        qWarning().noquote() << "OTA 安装失败:" << error;
        m_lastProgress.storeRelaxed(-1);
        emit transferProgress(-1, error);
        const QJsonObject err{
            { QStringLiteral("code"), QStringLiteral("OTA_INSTALL_FAILED") },
            { QStringLiteral("message"), error },
            { QStringLiteral("stage"), QStringLiteral("Install") },
        };
        if (m_responder)
            m_responder->write(QJsonDocument(err).toJson(QJsonDocument::Compact),
                               QByteArrayLiteral("application/json"),
                               QHttpServerResponder::StatusCode::BadRequest);
    } else {
        qInfo().noquote() << "OTA 安装成功——响应 PC";
        const QJsonObject ok{
            { QStringLiteral("code"), QStringLiteral("SUCCESSFUL_REBOOT") },
            { QStringLiteral("message"), QStringLiteral("固件安装完成，重启中") },
            { QStringLiteral("stage"), QStringLiteral("Finish") },
        };
        if (m_responder)
            m_responder->write(QJsonDocument(ok).toJson(QJsonDocument::Compact),
                               QByteArrayLiteral("application/json"),
                               QHttpServerResponder::StatusCode::Ok);
    }
    m_responder.reset();
}

QHttpServerResponse HttpReceiver::handleVnc(const QHttpServerRequest& request)
{
    // K-9：VNC 运行时启停（enable: true/false）——不重启工程
    bool enable = false;
    const QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (doc.isObject())
        enable = doc.object().value(QStringLiteral("enable")).toBool(false);
    if (!m_vncMirror)
        return jsonResponse(QJsonObject{ { QStringLiteral("code"), QStringLiteral("VNC_UNAVAILABLE") },
                                         { QStringLiteral("message"), QStringLiteral("VNC 镜像未初始化") } },
                            QHttpServerResponse::StatusCode::ServiceUnavailable);
    if (enable) {
        // K-9 评论3：端口走 fwconfig（配置/环境变量），不再写死 5900
        const int port = navihmi::vncPort();
        if (!m_vncMirror->start(quint16(port)))
            return jsonResponse(QJsonObject{ { QStringLiteral("code"), QStringLiteral("VNC_FAILED") },
                                             { QStringLiteral("message"), QStringLiteral("VNC 启动失败（端口占用？）") } },
                                QHttpServerResponse::StatusCode::Conflict);
        qInfo().noquote() << "VNC 运行时启动（" << port << "）";
    } else {
        m_vncMirror->stop();
        qInfo().noquote() << "VNC 运行时停止";
    }
    return jsonResponse(QJsonObject{ { QStringLiteral("code"), QStringLiteral("OK") },
                                     { QStringLiteral("vnc"), enable } },
                        QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse HttpReceiver::handleBlink(const QHttpServerRequest& request)
{
    // K-9：设备闪烁（enable: true/false）——QML 覆盖层亮灭交替 ~1s
    bool enable = false;
    const QJsonDocument doc = QJsonDocument::fromJson(request.body());
    if (doc.isObject())
        enable = doc.object().value(QStringLiteral("enable")).toBool(false);
    emit blinkRequested(enable);
    qInfo().noquote() << "设备闪烁指令:" << (enable ? "on" : "off");
    return jsonResponse(QJsonObject{ { QStringLiteral("code"), QStringLiteral("OK") },
                                     { QStringLiteral("blink"), enable } },
                        QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse HttpReceiver::jsonResponse(const QJsonObject& obj, QHttpServerResponse::StatusCode status)
{
    QHttpServerResponse resp(QJsonDocument(obj).toJson(QJsonDocument::Compact), status);
    resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
    return resp;
}

QString HttpReceiver::receiveAndInstall(const QByteArray& body, QString& projectPathOut, bool* isFirmware)
{
    // 1. 尺寸/空包校验（整包路径内存兜底——分块路径不经本函数，由 handleChunkedTransfer 直入 FromZip）
    if (body.isEmpty()) return QStringLiteral("空上传体");
    if (body.size() > kMaxLegacySinglePostBytes)
        return QStringLiteral("上传超过整包内存上限（%1 MB）——请使用分块上传").arg(kMaxLegacySinglePostBytes / (1024 * 1024));

    // 1b. D1：.fw 固件包嗅探（NHFW 魔数）——走 OTA 安装（校验 header sha → 组件表 → staging → 触发安装），
    //     不走工程部署 zip 路径；.navihmi/deploy.zip（PK 魔数）保持原路径
    //     复审 🔴：必须置位 isFirmware——finishTransfer 据此跳过 projectPackageReady（防 .fw 假工程重载）
    //     T-1a：.fw 保持整包路径（≤256MB 内存兜底；rootfs 全量 17~26MB 级远低于），分块仅工程容器
    if (body.size() >= 4 && qstrncmp(body.constData(), "NHFW", 4) == 0) {
        if (isFirmware) *isFirmware = true;
        return handleFirmwarePackage(body, projectPathOut);
    }

    // 2. body → 临时 zip → 共用安装链（安装逻辑单一来源：receiveAndInstallFromZip）
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) return QStringLiteral("临时目录创建失败");
    const QString zipPath = tmpDir.filePath(QStringLiteral("transfer.zip"));
    {
        QFile zf(zipPath);
        if (!zf.open(QIODevice::WriteOnly)) return QStringLiteral("临时文件写入失败");
        if (zf.write(body) != body.size()) return QStringLiteral("临时文件写入不完整");
    }
    return receiveAndInstallFromZip(zipPath, projectPathOut);
}

/// T-1a（2026-09-05）：从已落盘 zip 执行安装链——分块末块入口 + 整包路径共用（解压逻辑单一来源）。
/// 安装段进度刻度 70~100（解压 70→85、写入 85→99、完成 100——接收段 0~70 由分块块进度/整包入口负责）。
/// 仅工程容器（.fw 由 body 版 NHFW 嗅探分流，不经本函数）；后台线程执行（只碰局部/线程安全成员）。
QString HttpReceiver::receiveAndInstallFromZip(const QString& zipPath, QString& projectPathOut)
{
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) return QStringLiteral("临时目录创建失败");

    // 3. 解压到临时目录——逐条提取（fileInfoList+fileData，对齐 main.cpp extractZipAll 成功路径）
    //    K-9 评论/4_bugs：QZipReader::extractAll 对 PC 打包 zip（.NET ZipArchive）不兼容（板子 unzip 可解但
    //    extractAll 失败）；逐条 fileData 读取经 main.cpp 工程包解压验证可靠（2026-08-30 L 循环修复）
    //    穿越防护沿用 main.cpp 的 rel.contains("..")（此处条目来自 manifest 打包白名单外的 res 资源，
    //    与 receiveAndInstall 后续 safeRelPath 白名单校验互补；L-A1 审查确认边界）
    const QString extractDir = tmpDir.filePath(QStringLiteral("x"));
    if (!QDir().mkpath(extractDir)) return QStringLiteral("解压目录创建失败");
    {
        QZipReader reader(zipPath);
        if (!reader.exists()) return QStringLiteral("容器解压失败（非 ZIP 或损坏）");
        const auto entries = reader.fileInfoList();
        // Y4：分母预统计 = 实际文件条目数（entries 含 isDir 目录条目——若混入分母，目录多的 zip
        // extracted 永达不到 totalEntries，末次 45% 进度不发且百分比偏低）
        int fileEntries = 0;
        for (const auto& entry : entries)
            if (!entry.isDir) ++fileEntries;
        int extracted = 0;
        const int totalEntries = fileEntries;
        for (const auto& entry : entries) {
            if (entry.isDir) continue;
            const QString rel = entry.filePath;
            if (rel.contains(QLatin1String(".."))) continue;   // 防路径穿越（与 main.cpp 一致）
            const QString target = extractDir + QLatin1Char('/') + rel;
            QFileInfo fi(target);
            QDir().mkpath(fi.absolutePath());
            QFile f(target);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                // 写入完整性检查（L-A1 审查 🔴）：fileData 为空或写入不完整 → 解压失败返回，
                // 不静默落盘损坏条目（防 SHA256 校验绕过坏 res）
                const QByteArray data = reader.fileData(rel);
                if (data.isEmpty() || f.write(data) != data.size())
                    return QStringLiteral("容器解压失败（条目写入不完整: %1）").arg(rel);
                f.close();
                ++extracted;
                // M-3 ④：解压进度——T-1a 刻度统一（安装段 70 → 85；0~70 接收段归分块块进度/整包入口）
                if (totalEntries > 0 && (extracted % 16 == 0 || extracted == totalEntries)) {
                    const int pct = 70 + 15 * extracted / totalEntries;
                    m_lastProgress.storeRelaxed(pct);   // D-B4：同步记录供轮询
                    emit transferProgress(pct, QStringLiteral("解压安装包…"));
                }
            } else {
                return QStringLiteral("容器解压失败（无法创建: %1）").arg(target);
            }
        }
        if (extracted == 0) return QStringLiteral("容器解压失败（空容器，无文件条目）");
        reader.close();
        qInfo().noquote() << "部署容器解压完成:" << extracted << "files ->" << extractDir;
    }

    // 4. manifest 校验：app 条目 SHA256 与实际文件比对
    QJsonArray manifest;
    if (!readManifest(extractDir, manifest)) return QStringLiteral("manifest.json 缺失或损坏");
    bool appFound = false;
    QString appTarget;   // 校验通过的 app 条目 target（拷贝源统一用 manifest 路径，防硬编码漂移）
    for (const QJsonValue& v : manifest) {
        const QJsonObject entry = v.toObject();
        // 大小写容错（L-A1 联调发现）：PC 端旧产物 PascalCase "Type"/"Target"/"Sha256"，新产物 CamelCase（JsonNamingPolicy）
        const QString type = entry.value(QStringLiteral("type")).toString().isEmpty()
            ? entry.value(QStringLiteral("Type")).toString() : entry.value(QStringLiteral("type")).toString();
        const QString target = entry.value(QStringLiteral("target")).toString().isEmpty()
            ? entry.value(QStringLiteral("Target")).toString() : entry.value(QStringLiteral("target")).toString();
        if (type != QLatin1String("app")) continue;
        if (target.isEmpty() || !safeRelPath(target)) return QStringLiteral("manifest app target 非法");
        const QString filePath = extractDir + QLatin1Char('/') + target;
        if (!QFileInfo::exists(filePath)) return QStringLiteral("app 主包缺失: %1").arg(target);
        const QString expectSha = entry.value(QStringLiteral("sha256")).toString().isEmpty()
            ? entry.value(QStringLiteral("Sha256")).toString() : entry.value(QStringLiteral("sha256")).toString();
        const QString actualSha = sha256OfFile(filePath);
        if (expectSha.isEmpty() || actualSha != expectSha.toLower())
            return QStringLiteral("app 主包 SHA256 校验失败");
        appFound = true;
        appTarget = target;
        break;
    }
    if (!appFound) return QStringLiteral("manifest 无 app 条目");

    // 5. 原子落盘到工程目录（defaultProjectPath：app.navihmi 单文件原子替换；res/ 按 target 落盘）
    const QString deployDir = StorageInfo::defaultProjectPath().section(QLatin1Char('/'), 0, -2);
    if (!QDir().mkpath(deployDir)) return QStringLiteral("工程目录创建失败");

    const QString appPath = StorageInfo::defaultProjectPath();
    const QString appTmp = appPath + QStringLiteral(".tmp");
    {
        QFile src(extractDir + QLatin1Char('/') + appTarget);
        QFile dst(appTmp);
        if (!src.open(QIODevice::ReadOnly) || !dst.open(QIODevice::WriteOnly)) {
            QFile::remove(appTmp);   // 清理对称：失败不留 .tmp 残留
            return QStringLiteral("app 落盘失败（临时）");
        }
        const QByteArray data = src.readAll();
        if (dst.write(data) != data.size()) {
            src.close();
            dst.close();
            QFile::remove(appTmp);
            return QStringLiteral("app 落盘写入失败（短写）");
        }
        src.close();
        dst.close();
    }
    // 原子替换（POSIX rename 原子覆盖已存在目标——禁止先删后 rename：失败时旧工程必须完好）
    // L-A4（2026-08-30 联调）：QFile::rename 在目标已存在时实测失败（板子 busybox mv -f 覆盖成功但
    // QFile::rename 返回 false）——改用系统 ::rename()（POSIX 语义原子覆盖，同文件系统内保证原子性；
    // appTmp 与 appPath 同在 /mnt/user/userdata，无跨 fs EXDEV 问题）
    if (::rename(appTmp.toLocal8Bit().constData(), appPath.toLocal8Bit().constData()) != 0) {
        const int err = errno;
        QFile::remove(appTmp);
        return QStringLiteral("app 原子替换失败（errno=%1，当前工程未改动）").arg(err);
    }

    // res/ 资源按 target 落盘（白名单 fail-closed：非法 target 报错而非静默跳过；copy 失败报错）
    const QJsonArray manifestArr = manifest;
    int resTotal = 0;
    for (const QJsonValue& v : manifestArr) {   // 分母 = res 条目数（manifest 含 1 条 app——不能混入分母）
        const QJsonObject entry = v.toObject();
        // Y6：type 大小写容错（与 app 条目 L-A1 双读同模式——旧 PascalCase 产物兼容）
        const QString etype = entry.value(QStringLiteral("type")).toString().isEmpty()
            ? entry.value(QStringLiteral("Type")).toString() : entry.value(QStringLiteral("type")).toString();
        if (etype == QLatin1String("res")) ++resTotal;
    }
    // Y3（B2 CONFLICT_SOFT，对齐 J-2 ZIP 分支 removeRecursively）：**无条件清理工程目录旧 tiles/**
    // ——内部内存只保留一个可显示工程（用户 2026-08-30 方案：换工程只走组态下载/SD/USB，内存不保留多工程）：
    // 新包含瓦片 → 清旧后落新；新包无瓦片 → 清旧（防单文件探测命中陈旧瓦片，世界地图显示错配瓦片/校验假阴性）
    {
        QDir oldTiles(deployDir + QStringLiteral("/tiles"));
        if (oldTiles.exists()) {
            // 审查 🟡：检查返回值——清理失败时旧瓦片残留 → 单文件探测命中 → 用户实测问题①（错配瓦片）复现；
            // app 已原子替换无法回滚，至少失败可见不静默（fail-closed 报错，对齐 res 落盘 L454 风格）
            if (!oldTiles.removeRecursively())
                return QStringLiteral("旧瓦片目录清理失败: %1").arg(oldTiles.absolutePath());
            qInfo().noquote() << "已清理旧瓦片目录（内部内存单工程原则）:" << oldTiles.absolutePath();
        }
    }
    int resDone = 0;
    for (const QJsonValue& v : manifestArr) {
        const QJsonObject entry = v.toObject();
        // Y6：type/target 大小写容错（双读，同 app 条目）
        const QString type = entry.value(QStringLiteral("type")).toString().isEmpty()
            ? entry.value(QStringLiteral("Type")).toString() : entry.value(QStringLiteral("type")).toString();
        const QString target = entry.value(QStringLiteral("target")).toString().isEmpty()
            ? entry.value(QStringLiteral("Target")).toString() : entry.value(QStringLiteral("target")).toString();
        if (type != QLatin1String("res")) continue;
        if (target.isEmpty() || !safeRelPath(target))
            return QStringLiteral("manifest res target 非法: %1").arg(target);
        const QString srcFile = extractDir + QLatin1Char('/') + target;
        if (!QFileInfo::exists(srcFile))
            return QStringLiteral("res 资源缺失: %1").arg(target);
        const QString dstFile = deployDir + QLatin1Char('/') + target;
        if (!QDir().mkpath(QFileInfo(dstFile).absolutePath()))
            return QStringLiteral("res 落盘目录创建失败: %1").arg(target);
        // 跨端缺陷修复（M-3 ① 审查知会）：QFile::copy 目标已存在时失败（不覆盖）——重复部署同资源/瓦片
        // （1200+ 条目）全失败；先删目标再 copy（幂等覆盖，与重部署兼容；失败仍报错不留半成品）
        if (QFileInfo::exists(dstFile) && !QFile::remove(dstFile))
            return QStringLiteral("res 旧文件清理失败: %1").arg(target);
        if (!QFile::copy(srcFile, dstFile))
            return QStringLiteral("res 落盘失败: %1").arg(target);
        ++resDone;
        // M-3 ④：落盘进度——T-1a 刻度统一（写入 85 → 99）
        if (resTotal > 0 && (resDone % 16 == 0 || resDone == resTotal)) {
            const int pct = 85 + 14 * resDone / resTotal;
            m_lastProgress.storeRelaxed(pct);   // D-B4：同步记录供轮询
            emit transferProgress(pct, QStringLiteral("写入资源…"));
        }
    }

    // 6. 清理临时（QTemporaryDir 析构自动删；分块源 zip（.transfer/<id>.zip）显式删——成功安装不留残留；
    //    remove 罕见 I/O 错 → qWarning 可见（兜底职责 448 失败路径 / 本处成功路径）
    if (!QFile::remove(zipPath))
        qWarning().noquote() << "分块 zip 清理失败（安装成功路径，残留无害）: " << zipPath;
    projectPathOut = appPath;
    m_lastProgress.storeRelaxed(100);   // D-B4：安装完成
    emit transferProgress(100, QStringLiteral("安装完成"));   // M-3 ④：满进度 → QML 停 1~2s 后自动打开
    qInfo().noquote() << "工程容器接收完成: app=" << appPath;
    return QString();
}

/// D1：.fw 固件包处理（NHFW 魔数嗅探分支，receiveAndInstall 入口分流）。
/// 流程：header 校验（magic 已验 / version 16B / timestamp 8B / count 4B / payload sha 64B）
///     → payload 实际 SHA256 与 header 比对（防篡改，验收 7）
///     → 组件表定长解析（每项 184B：name32+type16+target48+size8+sha64+version16；count 与表长核对）
///     → 逐组件 payload 偏移/长度核对 + 组件 SHA256 比对（表内 sha）
///     → 写 staging（/tmp/navihmi_ota_staging/，保留原 .fw 字节）→ 触发 firmwarePackageReady(stagingPath)
///     → otaupdater 安装（分区写/备份/回滚由 otaupdater 负责，本处只做接收校验）
/// 返回空串=成功（stagingPathOut 输出 staging 路径）；非空=失败原因。
QString HttpReceiver::handleFirmwarePackage(const QByteArray& body, QString& stagingPathOut)
{
    // 1. header 校验（magic 已由调用方嗅探；定长字段布局见 FwPackageBuilder——单一事实源）
    const int kHeaderSize = 128;
    const int kEntrySize = 184;
    if (body.size() < kHeaderSize)
        return QStringLiteral(".fw 固件包损坏（不足 header 128B）");
    // version(16B, 偏移 4) / timestamp(8B, 偏移 20) / count(4B, 偏移 28, LE) / payloadSha(64B, 偏移 32)
    quint32 count = 0;
    {
        const QByteArray cntBytes = body.mid(28, 4);
        for (int i = 0; i < 4; ++i)
            count |= quint32(quint8(cntBytes.at(i))) << (8 * i);   // LE
    }
    if (count == 0 || count > 32)
        return QStringLiteral(".fw 组件数非法: %1").arg(count);
    const qint64 tableSize = qint64(count) * kEntrySize;
    if (body.size() < kHeaderSize + tableSize)
        return QStringLiteral(".fw 组件表不完整（count=%1）").arg(count);

    // 2. payload sha 校验（header 偏移 32 的 64B hex vs payload 实际 SHA256——防篡改）
    {
        const QByteArray headerSha = body.mid(32, 64).trimmed();
        const QByteArray payload = body.mid(kHeaderSize + tableSize);
        const QByteArray actualSha = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
        if (headerSha != actualSha)
            return QStringLiteral(".fw payload SHA256 校验失败（包被篡改或损坏）");
    }

    // 3. 组件表逐项核对（name/type/target 定长 + size 与 payload 偏移范围 + 组件 sha 比对）
    qint64 payloadOffset = kHeaderSize + tableSize;
    QStringList components;
    for (quint32 i = 0; i < count; ++i) {
        const int off = kHeaderSize + int(i) * kEntrySize;
        const QByteArray name = body.mid(off, 32).trimmed();
        const QByteArray type = body.mid(off + 32, 16).trimmed();
        // size(8B, 偏移 off+96, LE)
        quint64 size = 0;
        for (int b = 0; b < 8; ++b)
            size |= quint64(quint8(body.at(off + 96 + b))) << (8 * b);
        // 审查 🟡：无符号比较防 quint64→qint64 溢出绕过（size ≥ 2^63 时 qint64 变负）
        if (size == 0 || size > quint64(body.size()) - quint64(payloadOffset))
            return QStringLiteral(".fw 组件 %1 长度越界（size=%2）").arg(QString::fromLatin1(name)).arg(size);
        const QByteArray compSha = body.mid(off + 104, 64).trimmed();
        const QByteArray compData = body.mid(payloadOffset, qint64(size));
        const QByteArray actualCompSha = QCryptographicHash::hash(compData, QCryptographicHash::Sha256).toHex();
        if (compSha != actualCompSha)
            return QStringLiteral(".fw 组件 %1 SHA256 校验失败").arg(QString::fromLatin1(name));
        components << QString::fromLatin1(type) + QLatin1Char('/') + QString::fromLatin1(name);
        payloadOffset += qint64(size);
    }

    // 4. 写 staging（保留原 .fw 字节供 otaupdater 分区安装）——先清旧 staging 防残留
    const QString stagingDir = QStringLiteral("/tmp/navihmi_ota_staging");
    if (!QDir().mkpath(stagingDir))
        return QStringLiteral("OTA staging 目录创建失败");
    const QString stagingPath = stagingDir + QStringLiteral("/firmware.fw");
    {
        QFile f(stagingPath);
        if (f.exists() && !f.remove())
            return QStringLiteral("OTA staging 旧文件清理失败");
        if (!f.open(QIODevice::WriteOnly))
            return QStringLiteral("OTA staging 写入失败");
        if (f.write(body) != body.size())
            return QStringLiteral("OTA staging 写入不完整");
    }
    m_lastProgress.storeRelaxed(100);
    emit transferProgress(100, QStringLiteral("固件校验通过，开始安装"));
    stagingPathOut = stagingPath;
    emit firmwarePackageReady(stagingPath);
    qInfo().noquote() << ".fw 固件包校验通过，staging=" << stagingPath << "组件=" << components.join(QLatin1Char(','));
    return QString();
}

} // namespace navihmi
