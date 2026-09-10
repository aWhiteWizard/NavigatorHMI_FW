/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_rtu_driver.h
 * @Description: Modbus RTU 采集驱动（AB-2 2026-09-11 新增）——USB-RS485/串口从站采集，
 *              与 Modbus TCP 共用 ModbusDriverBase（tag 解析/解码/写校验/调度/退避）。
 *              传输层: QModbusRtuSerialClient（Qt6 SerialBus；板端 libQt6SerialBus 已验证含该类）。
 *              连接参数（DeviceConfig.connection_info）: port（设备端串口路径，如 /dev/ttyUSB0，必填）
 *                                                      baud（默认 9600）
 *                                                      可选 parity(no/even/odd) / dataBits(5-8) / stopBits(1|2)
 *                                                      ——缺省即 8N1（PT100 模块出厂默认）
 *              Tag.source: modbus://{从站}/{寄存器}（从站号每请求下发，与 TCP 一致）
 */
#pragma once

#include "drivers/modbus_driver_base.h"

class QModbusClient;

namespace navihmi {

/// Modbus RTU 驱动（串口传输层；无连接信息时不启动——不猜默认串口，防误开别的端口）
class ModbusRtuDriver : public ModbusDriverBase
{
    Q_OBJECT
public:
    explicit ModbusRtuDriver(QObject* parent = nullptr) : ModbusDriverBase(parent) {}

    QString protocol() const override { return QStringLiteral("modbus_rtu"); }

protected:
    QModbusClient* createClient() override;
    void applyConnectionParameters(QModbusClient* client) override;
    QString targetDescription() const override;
    bool ensureConnectionParams() override;
    QString logTag() const override { return QStringLiteral("ModbusRtuDriver"); }
};

/// Modbus RTU 驱动工厂 + extern C 注册入口（与 TCP 同宏裁剪: NAVIHMI_HAVE_MODBUS_DRIVER）
class ModbusRtuDriverFactory : public IDriverFactory
{
public:
    QString protocol() const override { return QStringLiteral("modbus_rtu"); }
    IDriver* create(QObject* parent) override { return new ModbusRtuDriver(parent); }
};

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MODBUS_DRIVER
extern "C" void init_driver_modbus_rtu();
#endif
