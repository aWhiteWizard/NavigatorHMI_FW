/*
 * @FilePath: \NavigatorHMI_FW\src\httpreceiver\httpreceiver.cpp
 * @Description: K-8b FW 接收端实现——qthttpserver 路由 + TransferSession 单客户端 + 容器校验 + 原子落盘
 */
#include "httpreceiver.h"

#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QDateTime>
#include <QUuid>

#include <unistd.h>   // ::rename（L-A4：POSIX 原子覆盖替代 QFile::rename）
#include <cerrno>     // errno

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

constexpr int kMaxUploadBytes = 64 * 1024 * 1024;   // 上传上限 64MB（device-profile uploadSizeLimitMB 默认）
constexpr int kTransferPort = 80;                    // HTTP 默认端口

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

    // POST /api/transfer——工程部署容器上传（单客户端串行）
    m_server.route(QStringLiteral("/api/transfer"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) { return handleTransfer(request); });

    // K-9：POST /api/vnc {enable}——VNC 运行时启停（设备面板对等；proto enable_vnc=21 启动默认值，运行时指令覆盖）
    m_server.route(QStringLiteral("/api/vnc"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) { return handleVnc(request); });

    // K-9：POST /api/blink {enable}——设备闪烁（屏幕亮灭交替 ~1s；多设备定位）
    m_server.route(QStringLiteral("/api/blink"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) { return handleBlink(request); });
}

QString HttpReceiver::deviceModel() const
{
    // K-9 评论2：不写死——无工程时按设备默认分辨率查型号表（设备本身型号），工程加载后走工程字段/查表
    // 2026-08-30 用户评论：有工程但型号/分辨率皆空 = 错误工程 → 返回"未知"（不静默兜底）
    if (!m_bus) {
        Project stub;
        stub.deviceWidth = kDefaultDeviceWidth;
        stub.deviceHeight = kDefaultDeviceHeight;
        return deviceModelFor(stub);
    }
    const QString model = deviceModelFor(m_bus->project());
    return model.isEmpty() ? QStringLiteral("未知（错误工程：无型号且无有效分辨率）") : model;
}

QString HttpReceiver::deviceSizeInch() const
{
    if (!m_bus) {
        Project stub;
        stub.deviceWidth = kDefaultDeviceWidth;
        stub.deviceHeight = kDefaultDeviceHeight;
        return deviceSizeInchFor(stub);
    }
    const QString inch = deviceSizeInchFor(m_bus->project());
    return inch.isEmpty() ? QStringLiteral("未知（错误工程）") : inch;
}

QHttpServerResponse HttpReceiver::handleDeviceInfo()
{
    const QString ip = m_deviceInfo ? m_deviceInfo->ipAddress() : QString();
    const QString fw = m_deviceInfo ? m_deviceInfo->appVersion() : QString();
    const QJsonObject obj{
        { QStringLiteral("model"), deviceModel() },
        { QStringLiteral("sizeInch"), deviceSizeInch() },
        { QStringLiteral("id"), ip },
        { QStringLiteral("version"), fw },      // 固件版本（术语口径：/api/device/info version=固件版本）
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

QHttpServerResponse HttpReceiver::handleTransfer(const QHttpServerRequest& request)
{
    // 并发上限 1（单客户端串行；多余请求拒绝并报错——服务约束）
    if (!m_transferActive.testAndSetAcquire(false, true)) {
        const QJsonObject err{
            { QStringLiteral("code"), QStringLiteral("TRANSFER_BUSY") },
            { QStringLiteral("message"), QStringLiteral("已有传输任务进行中") },
            { QStringLiteral("stage"), QStringLiteral("Upload") },
        };
        QHttpServerResponse resp(QJsonDocument(err).toJson(QJsonDocument::Compact),
                                 QHttpServerResponse::StatusCode::Conflict);
        resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
        return resp;
    }

    const QByteArray body = request.body();
    QString projectPath;
    const QString error = receiveAndInstall(body, projectPath);
    m_transferActive.storeRelease(false);

    if (!error.isEmpty()) {
        qWarning().noquote() << "传输失败:" << error;
        const QJsonObject err{
            { QStringLiteral("code"), QStringLiteral("TRANSFER_FAILED") },
            { QStringLiteral("message"), error },
            { QStringLiteral("stage"), QStringLiteral("Install") },
        };
        QHttpServerResponse resp(QJsonDocument(err).toJson(QJsonDocument::Compact),
                                 QHttpServerResponse::StatusCode::BadRequest);
        resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
        return resp;
    }

    // 校验通过、已落盘 → 通知主程序重载工程（下载事务性：失败路径不动当前工程）
    emit projectPackageReady(projectPath);

    const QJsonObject ok{
        { QStringLiteral("code"), QStringLiteral("SUCCESSFUL_REBOOT") },
        { QStringLiteral("message"), QStringLiteral("部署成功，工程重载中") },
        { QStringLiteral("stage"), QStringLiteral("Finish") },
    };
    QHttpServerResponse resp(QJsonDocument(ok).toJson(QJsonDocument::Compact), QHttpServerResponse::StatusCode::Ok);
    resp.setHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
    return resp;
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

QString HttpReceiver::receiveAndInstall(const QByteArray& body, QString& projectPathOut)
{
    // 1. 尺寸/空包校验
    if (body.isEmpty()) return QStringLiteral("空上传体");
    if (body.size() > kMaxUploadBytes)
        return QStringLiteral("上传超过大小上限（%1 MB）").arg(kMaxUploadBytes / (1024 * 1024));

    // 2. body → 临时 zip
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) return QStringLiteral("临时目录创建失败");
    const QString zipPath = tmpDir.filePath(QStringLiteral("transfer.zip"));
    {
        QFile zf(zipPath);
        if (!zf.open(QIODevice::WriteOnly)) return QStringLiteral("临时文件写入失败");
        if (zf.write(body) != body.size()) return QStringLiteral("临时文件写入不完整");
    }

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
        int extracted = 0;
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
    for (const QJsonValue& v : manifest) {
        const QJsonObject entry = v.toObject();
        const QString type = entry.value(QStringLiteral("type")).toString();
        const QString target = entry.value(QStringLiteral("target")).toString();
        if (type != QLatin1String("res")) continue;
        if (target.isEmpty() || !safeRelPath(target))
            return QStringLiteral("manifest res target 非法: %1").arg(target);
        const QString srcFile = extractDir + QLatin1Char('/') + target;
        if (!QFileInfo::exists(srcFile))
            return QStringLiteral("res 资源缺失: %1").arg(target);
        const QString dstFile = deployDir + QLatin1Char('/') + target;
        if (!QDir().mkpath(QFileInfo(dstFile).absolutePath()))
            return QStringLiteral("res 落盘目录创建失败: %1").arg(target);
        if (!QFile::copy(srcFile, dstFile))
            return QStringLiteral("res 落盘失败: %1").arg(target);
    }

    // 6. 清理临时（QTemporaryDir 析构自动删）+ 返回
    projectPathOut = appPath;
    qInfo().noquote() << "工程容器接收完成: app=" << appPath;
    return QString();
}

} // namespace navihmi
