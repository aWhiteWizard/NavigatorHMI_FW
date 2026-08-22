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
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace navihmi {

namespace {
// 版本号（与 CMake project VERSION 对齐；无宏时兜底）
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

QString DeviceInfo::resolveIp()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("192.168.1.146");   // 仿真占位
#else
    // 优先 ip 命令（iproute2 / busybox ip 均可）
    QProcess ip;
    ip.start(QStringLiteral("ip"), { QStringLiteral("-4"), QStringLiteral("addr"),
                                     QStringLiteral("show"), QStringLiteral("eth0") });
    if (ip.waitForFinished(1500)) {
        const QString out = QString::fromUtf8(ip.readAllStandardOutput());
        const QRegularExpression re(QStringLiteral("inet\\s+([0-9.]+)/"));
        const auto m = re.match(out);
        if (m.hasMatch())
            return m.captured(1);
    }
    // 兜底 ifconfig eth0
    QProcess ic;
    ic.start(QStringLiteral("ifconfig"), { QStringLiteral("eth0") });
    if (ic.waitForFinished(1500)) {
        const QString out = QString::fromUtf8(ic.readAllStandardOutput());
        const QRegularExpression re(QStringLiteral("inet\\s+addr:([0-9.]+)"));
        const auto m = re.match(out);
        if (m.hasMatch())
            return m.captured(1);
    }
    return QStringLiteral("0.0.0.0");
#endif
}

void DeviceInfo::ensureLoaded() const
{
    if (m_loaded)
        return;
    m_loaded = true;
#if defined(Q_OS_WIN)
    m_mac = QStringLiteral("00:11:22:33:44:55");   // 仿真占位
    m_kernel = QStringLiteral("6.1.141");
#else
    m_mac = readFile("/sys/class/net/eth0/address");
    const QString ver = readFile("/proc/version");   // "Linux version 6.1.141 (gcc...) ..."
    const QRegularExpression re(QStringLiteral("Linux version\\s+(\\S+)"));
    const auto m = re.match(ver);
    m_kernel = m.hasMatch() ? m.captured(1) : ver;
#endif
    m_ip = resolveIp();
}

QString DeviceInfo::ipAddress() const
{
    ensureLoaded();
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

void DeviceInfo::runCalibrate()
{
#if defined(Q_OS_WIN)
    qInfo() << "触摸校准: 仿真器无 tslib（跳过）";
#else
    // B6-12: tslib 触摸校准（startDetached 不阻塞 HMI）
    // D+ 修复: ts_calibrate 需 TSLIB_TSDEVICE 指向触摸设备——板子触摸是 ft5x06 (/dev/input/event3,
    // 非 GT911)；自动探测 input 设备名含 touch/ft5x06 的 event 节点，探测失败回退 event3
    qInfo() << "触摸校准: 启动 ts_calibrate";
    QString tsDevice = QStringLiteral("/dev/input/event3");   // 默认 ft5x06
    QDir inputDir(QStringLiteral("/sys/class/input"));
    const auto events = inputDir.entryList(QStringList() << QStringLiteral("event*"), QDir::Dirs);
    for (const auto& ev : events) {
        QFile nameFile(inputDir.filePath(ev + QStringLiteral("/device/name")));
        if (nameFile.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(nameFile.readAll()).trimmed().toLower();
            if (name.contains(QStringLiteral("touch")) || name.contains(QStringLiteral("ft5x06"))) {
                tsDevice = QStringLiteral("/dev/input/") + ev;
                break;
            }
        }
    }
    qInfo().noquote() << "触摸校准: tslib 设备 =" << tsDevice;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TSLIB_TSDEVICE"), tsDevice);
    env.insert(QStringLiteral("TSLIB_CONFFILE"), QStringLiteral("/etc/ts.conf"));
    env.insert(QStringLiteral("TSLIB_CALIBFILE"), QStringLiteral("/etc/pointercal"));
    QProcess p;
    p.setProgram(QStringLiteral("ts_calibrate"));
    p.setProcessEnvironment(env);
    // 审查修复(2b3d04bd): p.startDetached("ts_calibrate") 命中静态重载(p 从未启动,
    // p.state() 恒 NotRunning → ok 恒 false 误报失败)；改成员无参版, 用已设置的 program/env
    const bool ok = p.startDetached();
    if (!ok)
        qWarning() << "触摸校准: ts_calibrate 启动失败（rootfs 是否安装 tslib？）";
#endif
}

} // namespace navihmi
