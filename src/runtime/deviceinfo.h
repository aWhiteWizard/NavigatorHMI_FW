/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\deviceinfo.h
 * @Description: 设备信息真实读取（/proc /sys /网卡）——B6-6 数据真实化
 *               注入 QML context property "deviceInfo"，导航页 IP/MAC/版本/内核/运行时间显示真实值
 */
#pragma once

#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QTimer>

namespace navihmi {

/// ip/ifconfig 命令等待超时（ms）——超时回落 0.0.0.0（2026-08-26 魔法数字整改命名）。
constexpr int kCmdWaitTimeoutMs = 1500;

/// OTA 安装固件打包时刻标记文件（writer=otaupdater::markOtaInstalled / reader=deviceinfo::otaTimestamp
/// 单点共享防拼写漂移——2026-09-04 调试 OTA：PC 端同版覆盖判断 = 包打包时刻 > 此文件记录）
inline constexpr const char* kOtaInstalledTsFile = "/etc/navigatorhmi/ota-installed-ts";

class DeviceInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY infoChanged)
    Q_PROPERTY(QString macAddress READ macAddress NOTIFY infoChanged)
    Q_PROPERTY(QString kernelVersion READ kernelVersion NOTIFY infoChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString bootloaderVersion READ bootloaderVersion CONSTANT)
public:
    explicit DeviceInfo(QObject* parent = nullptr);

    QString ipAddress() const;
    QString macAddress() const;
    QString kernelVersion() const;
    QString appVersion() const;           // 编译期版本（CMake project VERSION）
    QString bootloaderVersion() const;    // 占位（当前无读取来源）
    /// 同步阻塞解析 eth0 IP（ip 命令 waitForFinished ≤1.5s + ifconfig 兜底）——SSH CLI / HTTP 端点用
    /// （低频命令路径，阻塞可接受；QML 用异步 ipAddress() 防 GUI 卡顿）。结果缓存 m_ip（QML 后续读一致）。
    QString ipAddressBlocking() const;
    /// 当前生效固件的 OTA 打包时刻（/etc/navigatorhmi/ota-installed-ts，Unix 秒字符串；
    /// 未 OTA 装过/旧固件无标记 → "0"）——2026-09-04 调试 OTA：版本恒 v1.1.0，PC 端按打包时刻先后判断是否可覆盖
    QString otaTimestamp() const;

    /// 运行时间（/proc/uptime 秒 → "Xd Xh Ym"），QML Timer 周期调用
    Q_INVOKABLE QString uptimeText() const;

signals:
    void infoChanged();

private:
    void ensureLoaded() const;            // 惰性加载 MAC/IP/内核（缓存）
    static QString readFile(const char* path);   // 读文件首行（trim）
    // W-C（F4）：resolveIp 异步化——QML 首次绑定 ipAddress 不再同步阻塞（原 waitForFinished 1.5s 卡 GUI）；
    // ip 命令异步 + 1500ms 超时杀进程；ip 失败/超时回落 ifconfig（同样异步，链式）
    void startIpResolve();
    mutable bool m_loaded = false;
    mutable bool m_ipStarted = false;
    mutable bool m_ipFallback = false;    // ip 命令已失败/超时（走 ifconfig 回落）
    mutable QProcess* m_ipProc = nullptr; // parent(this) 自动回收（QObject 树）
    mutable QTimer* m_ipTimer = nullptr;
    mutable QString m_ip;
    mutable QString m_mac;
    mutable QString m_kernel;
};

} // namespace navihmi
