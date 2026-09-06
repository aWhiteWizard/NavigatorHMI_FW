/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_tcp_driver.h
 * @Description: Modbus TCP 采集驱动（V-1 2026-09-06 从 Acquisition 1:1 迁入，行为不变）
 *               连接参数: Tag.deviceName → DeviceConfig.connectionInfo JSON（host/port）
 *               Tag.source: modbus://{从站}/{寄存器}（float 按 /100 放大单寄存器协议约定）
 */
#pragma once

#include <QList>
#include <QHash>
#include <QString>
#include <QVariant>
#include "drivers/driver_iface.h"

class QModbusTcpClient;

namespace navihmi {

/// Modbus 请求超时（ms）——影响读/写成败判定（2026-08-26 魔法数字整改命名）。
constexpr int kModbusTimeoutMs = 500;
/// Modbus 从站地址上限（协议标准 1-247）。
constexpr int kModbusSlaveMax = 247;
/// Modbus 默认端口（IANA 标准）。
constexpr char kModbusDefaultPort[] = "502";

/// Modbus TCP 驱动实现（协议层：连接/读写/解码；周期调度与 deadband 归 Manager）
class ModbusTcpDriver : public IDriver
{
    Q_OBJECT
public:
    explicit ModbusTcpDriver(QObject* parent = nullptr);
    ~ModbusTcpDriver() override;

    QString protocol() const override { return QStringLiteral("modbus_tcp"); }
    bool configure(const QList<DriverTagInfo>& tags) override;
    void start() override;
    void stop() override;
    void poll(qint64 nowMs) override;
    bool writeValue(const QString& tagName, const QVariant& value) override;

private:
    // 采集任务项（configure 时按 source 细解析——原 Acquisition::ModbusTag）
    struct ModbusTag {
        QString tagName;
        int slave = 1;
        quint16 reg = 0;
        int dataType = 0;        // TagDataType
        int scanMs = 0;
        double deadband = 0;     // 原 Acquisition 读路径用（回传 Manager 判断——保留字段供 configure 聚合）
        qint64 lastReadMs = 0;
    };

    void ensureConnected();
    void readTag(const ModbusTag* tag);
    QVariant decodeValue(const ModbusTag& tag, quint16 raw) const;

    QList<ModbusTag> m_tags;
    QModbusTcpClient* m_client = nullptr;
    bool m_connecting = false;
    QHash<QString, QString> m_conn;   // 当前连接参数（首任务；单连接语义保留——多连接 V+1 扩展）
    qint64 m_lastConnectFailMs = 0;   // J-3: 最近连接失败时间戳（重连退避 5s）
    qint64 m_lastConnectWarnMs = 0;   // J-3: 最近连接失败告警时间戳（日志降频）
};

/// Modbus TCP 驱动工厂 + extern C 注册入口（宏裁剪: NAVIHMI_HAVE_MODBUS_DRIVER 关时不注册）
class ModbusTcpDriverFactory : public IDriverFactory
{
public:
    QString protocol() const override { return QStringLiteral("modbus_tcp"); }
    IDriver* create(QObject* parent) override { return new ModbusTcpDriver(parent); }
};

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MODBUS_DRIVER
extern "C" void init_driver_modbus_tcp();
#endif
