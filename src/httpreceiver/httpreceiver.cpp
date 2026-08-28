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

#include "runtime/runtimebus.h"
#include "runtime/deviceinfo.h"
#include "runtime/storageinfo.h"

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
}

QString HttpReceiver::deviceModel() const
{
    if (!m_bus) return QStringLiteral("NavigatorHMI-7");
    const Project& proj = m_bus->project();
    // 按工程分辨率推导（7寸 1024×600 / 4寸 720×720——与 device-profile 一致）
    if (proj.deviceWidth == 720 && proj.deviceHeight == 720)
        return QStringLiteral("NavigatorHMI-4");
    return QStringLiteral("NavigatorHMI-7");
}

QString HttpReceiver::deviceSizeInch() const
{
    return deviceModel() == QLatin1String("NavigatorHMI-4") ? QStringLiteral("4寸") : QStringLiteral("7寸");
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

    // 3. 解压到临时目录
    const QString extractDir = tmpDir.filePath(QStringLiteral("x"));
    if (!QDir().mkpath(extractDir)) return QStringLiteral("解压目录创建失败");
    QZipReader reader(zipPath);
    if (!reader.extractAll(extractDir)) return QStringLiteral("容器解压失败（非 ZIP 或损坏）");

    // 4. manifest 校验：app 条目 SHA256 与实际文件比对
    QJsonArray manifest;
    if (!readManifest(extractDir, manifest)) return QStringLiteral("manifest.json 缺失或损坏");
    bool appFound = false;
    QString appTarget;   // 校验通过的 app 条目 target（拷贝源统一用 manifest 路径，防硬编码漂移）
    for (const QJsonValue& v : manifest) {
        const QJsonObject entry = v.toObject();
        const QString type = entry.value(QStringLiteral("type")).toString();
        const QString target = entry.value(QStringLiteral("target")).toString();
        if (type != QLatin1String("app")) continue;
        if (target.isEmpty() || !safeRelPath(target)) return QStringLiteral("manifest app target 非法");
        const QString filePath = extractDir + QLatin1Char('/') + target;
        if (!QFileInfo::exists(filePath)) return QStringLiteral("app 主包缺失: %1").arg(target);
        const QString expectSha = entry.value(QStringLiteral("sha256")).toString().toLower();
        const QString actualSha = sha256OfFile(filePath);
        if (expectSha.isEmpty() || actualSha != expectSha)
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
    if (!QFile::rename(appTmp, appPath)) {
        QFile::remove(appTmp);
        return QStringLiteral("app 原子替换失败（当前工程未改动）");
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
