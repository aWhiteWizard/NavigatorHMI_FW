/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\deviceinfo.h
 * @Description: 设备信息真实读取（/proc /sys /网卡）——B6-6 数据真实化
 *               注入 QML context property "deviceInfo"，导航页 IP/MAC/版本/内核/运行时间显示真实值
 */
#pragma once

#include <QObject>
#include <QString>

namespace navihmi {

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

    /// 运行时间（/proc/uptime 秒 → "Xd Xh Ym"），QML Timer 周期调用
    Q_INVOKABLE QString uptimeText() const;

signals:
    void infoChanged();

private:
    void ensureLoaded() const;            // 惰性加载 MAC/IP/内核（缓存）
    static QString readFile(const char* path);   // 读文件首行（trim）
    static QString resolveIp();           // ip/ifconfig 解析 eth0 IPv4
    mutable bool m_loaded = false;
    mutable QString m_ip;
    mutable QString m_mac;
    mutable QString m_kernel;
};

} // namespace navihmi
