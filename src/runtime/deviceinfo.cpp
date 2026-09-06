/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\deviceinfo.cpp
 * @Description: 设备信息真实读取实现（B6-6）
 *               RK3562: /sys/class/net/eth0/address(MAC)、ip/ifconfig(IP)、/proc/version(内核)、/proc/uptime(运行)
 *               Windows 仿真: 返回占位值（保持仿真器可用）
 */
#include "runtime/deviceinfo.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>

namespace navihmi {

namespace {
// 版本号（与 CMake project VERSION 对齐；无宏时兜底）
// 2026-09-04：v1.1.0 = 调试基线（用户定：调试期每次编译 FW 版本恒 v1.1.0，OTA 包唯一性靠打包时刻——见 otaTimestamp）
#ifndef NAVIGATORHMI_VERSION_STR
#define NAVIGATORHMI_VERSION_STR "1.1.0"
#endif
} // namespace

DeviceInfo::DeviceInfo(QObject* parent)
    : QObject(parent)
{
}

QString DeviceInfo::appVersion() const
{
    return QStringLiteral("v") + QStringLiteral(NAVIGATORHMI_VERSION_STR);
}

QString DeviceInfo::otaTimestamp() const
{
    // 审查 🔵（2026-09-04）：内容非数字防御——损坏/半写残留返回 "0"（PC 端归 0 放行，fail-safe）
    const QString t = readFile(kOtaInstalledTsFile);
    if (t.isEmpty()) return QStringLiteral("0");
    bool ok = false;
    const quint64 n = t.toULongLong(&ok);
    return ok ? QString::number(n) : QStringLiteral("0");
}

QString DeviceInfo::bootloaderVersion() const
{
    // 当前无 u-boot 版本读取来源，保持占位（后续可从 /proc/device-tree 或 u-boot env 读）
    return QStringLiteral("v1.04");
}

QString DeviceInfo::readFile(const char* path)
{
    QFile f(QString::fromLatin1(path));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QString line = QString::fromUtf8(f.readLine()).trimmed();
    f.close();
    return line;
}

void DeviceInfo::startIpResolve()
{
    // W-C（F4）异步化：QProcess 异步 + 1500ms 超时（杀进程防挂起）；不阻塞 QML 线程
    if (m_ipProc)   // 在途（含回落）不重复启动
        return;
    const bool fallback = m_ipFallback;
    m_ipProc = new QProcess(this);
    m_ipTimer = new QTimer(this);
    m_ipTimer->setSingleShot(true);
    connect(m_ipTimer, &QTimer::timeout, this, [this]() {
        if (m_ipProc && m_ipProc->state() != QProcess::NotRunning) {
            m_ipProc->kill();       // finished 信号随后触发解析/回落（无输出 → 回落 ifconfig）
        }
    });
    connect(m_ipProc, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (m_ipTimer) {
            m_ipTimer->stop();
            m_ipTimer = nullptr;
        }
        QProcess* proc = m_ipProc;
        m_ipProc = nullptr;
        if (!proc)
            return;
        const bool wasFallback = m_ipFallback;
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        const QRegularExpression rx = wasFallback
            ? QRegularExpression(QStringLiteral("inet\\s+addr:([0-9.]+)"))
            : QRegularExpression(QStringLiteral("inet\\s+([0-9.]+)/"));
        const auto m = rx.match(out);
        if (m.hasMatch()) {
            m_ip = m.captured(1);
            emit infoChanged();
            return;
        }
        // ip 失败/无匹配 → 回落 ifconfig（仅一次）；仍失败保持空（QML 显示占位）
        if (!wasFallback) {
            m_ipFallback = true;
            startIpResolve();
        }
    });
    m_ipTimer->start(kCmdWaitTimeoutMs);
    const QString prog = fallback ? QStringLiteral("ifconfig") : QStringLiteral("ip");
    const QStringList args = fallback ? QStringList{ QStringLiteral("eth0") }
                                      : QStringList{ QStringLiteral("-4"), QStringLiteral("addr"),
                                                     QStringLiteral("show"), QStringLiteral("eth0") };
    m_ipProc->start(prog, args);
}

void DeviceInfo::ensureLoaded() const
{
    if (m_loaded)
        return;
    m_loaded = true;
#if defined(Q_OS_WIN)
    m_mac = QStringLiteral("00:11:22:33:44:55");   // 仿真占位
    m_kernel = QStringLiteral("6.1.141");
    m_ip = QStringLiteral("192.168.1.146");
#else
    m_mac = readFile("/sys/class/net/eth0/address");
    const QString ver = readFile("/proc/version");   // "Linux version 6.1.141 (gcc...) ..."
    const QRegularExpression re(QStringLiteral("Linux version\\s+(\\S+)"));
    const auto m = re.match(ver);
    m_kernel = m.hasMatch() ? m.captured(1) : ver;
#endif
}

QString DeviceInfo::ipAddressBlocking() const
{
    // W-C（F4）：同步路径（SSH CLI / HTTP）——原 waitForFinished 逻辑保留；结果缓存 m_ip 供 QML 后续读一致。
    // 审查 🟡-2：m_ip 已缓存（QML 异步解析完成/先前 CLI 解析过）→ 直接返回，零进程零冻结主线程
    // （blocking 调用方 QLocalServer/qthttpserver 处理器均在 GUI 主线程——每次重跑 ip 命令会冻结重绘）
    if (!m_ip.isEmpty())
        return m_ip;
#if defined(Q_OS_WIN)
    return QStringLiteral("192.168.1.146");
#else
    // 优先 ip 命令（iproute2 / busybox ip 均可）
    QProcess ip;
    ip.start(QStringLiteral("ip"), { QStringLiteral("-4"), QStringLiteral("addr"),
                                     QStringLiteral("show"), QStringLiteral("eth0") });
    if (ip.waitForFinished(kCmdWaitTimeoutMs)) {
        const QString out = QString::fromUtf8(ip.readAllStandardOutput());
        const QRegularExpression re(QStringLiteral("inet\\s+([0-9.]+)/"));
        const auto m = re.match(out);
        if (m.hasMatch()) {
            m_ip = m.captured(1);
            return m_ip;
        }
    }
    // 兜底 ifconfig eth0
    QProcess ic;
    ic.start(QStringLiteral("ifconfig"), { QStringLiteral("eth0") });
    if (ic.waitForFinished(kCmdWaitTimeoutMs)) {
        const QString out = QString::fromUtf8(ic.readAllStandardOutput());
        const QRegularExpression re(QStringLiteral("inet\\s+addr:([0-9.]+)"));
        const auto m = re.match(out);
        if (m.hasMatch()) {
            m_ip = m.captured(1);
            return m_ip;
        }
    }
    return m_ip.isEmpty() ? QStringLiteral("0.0.0.0") : m_ip;
#endif
}

QString DeviceInfo::ipAddress() const
{
    ensureLoaded();
    // W-C（F4）：首次访问触发异步解析（不阻塞 QML）；infoChanged 回填后 QML 绑定自动刷新。
    // 若 CLI/HTTP 已同步解析（ipAddressBlocking 缓存 m_ip）则直接返回
    if (m_ip.isEmpty() && !m_ipStarted) {
        m_ipStarted = true;
        const_cast<DeviceInfo*>(this)->startIpResolve();
    }
    return m_ip;
}

QString DeviceInfo::macAddress() const
{
    ensureLoaded();
    return m_mac;
}

QString DeviceInfo::kernelVersion() const
{
    ensureLoaded();
    return m_kernel;
}

QString DeviceInfo::uptimeText() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("72h 15m");   // 仿真占位
#else
    bool ok = false;
    const double secs = readFile("/proc/uptime").section(QLatin1Char(' '), 0, 0).toDouble(&ok);
    if (!ok)
        return QStringLiteral("--");
    const qint64 total = qint64(secs);
    const qint64 days = total / 86400;
    const qint64 hours = (total % 86400) / 3600;
    const qint64 mins = (total % 3600) / 60;
    if (days > 0)
        return QStringLiteral("%1d %2h %3m").arg(days).arg(hours).arg(mins);
    return QStringLiteral("%1h %2m").arg(hours).arg(mins);
#endif
}

} // namespace navihmi
