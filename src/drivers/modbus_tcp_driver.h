/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_tcp_driver.h
 * @Description: Modbus TCP 采集驱动（AB-2 2026-09-11——公共逻辑上提 ModbusDriverBase，
 *              本类只保留 TCP 传输层：QModbusTcpClient + host/port 连接参数 + 目标描述）
 *              连接参数: Tag.deviceName → DeviceConfig.connectionInfo JSON（host/port）
 *              Tag.source: modbus://{从站}/{寄存器}（float 按 /100 放大单寄存器协议约定）
 *              行为回归锚: V-1/y-7 语义不变（纯搬家）。
 */
#pragma once

#include "drivers/modbus_driver_base.h"

class QModbusClient;

namespace navihmi {

/// Modbus TCP 驱动（协议层：连接/读写/解码；周期调度与 deadband 归 Manager）
class ModbusTcpDriver : public ModbusDriverBase
{
    Q_OBJECT
public:
    explicit ModbusTcpDriver(QObject* parent = nullptr) : ModbusDriverBase(parent) {}

    QString protocol() const override { return QStringLiteral("modbus_tcp"); }

protected:
    QModbusClient* createClient() override;
    void applyConnectionParameters(QModbusClient* client) override;
    QString targetDescription() const override;
    bool ensureConnectionParams() override;
    QString logTag() const override { return QStringLiteral("ModbusTcpDriver"); }
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
