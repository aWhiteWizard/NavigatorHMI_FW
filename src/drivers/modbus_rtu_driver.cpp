/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_rtu_driver.cpp
 * @Description: Modbus RTU 采集驱动实现（AB-2 2026-09-11）——只实现串口传输层；
 *              公共采集面（tag 解析/解码/写校验/轮询/退避）在 ModbusDriverBase。
 *              串口参数来源 = DeviceConfig.connection_info（PC 侧 ModbusRTU 对话框写入）；
 *              默认 8N1；波特率缺省 9600；无 port 直接判定配置不可用（不启动，防误开端口）。
 */
#include "drivers/modbus_rtu_driver.h"

#include <QModbusRtuSerialClient>
#include <QSerialPort>
#include <QDebug>
#include <memory>   // std::make_unique（注册入口）

namespace navihmi {

namespace {

/// 解析可选校验位（"no"/"none"/"even"/"odd"；缺省 NoParity）
QSerialPort::Parity parityFromString(const QString& s, bool* known)
{
    const QString v = s.trimmed().toLower();
    *known = true;
    if (v.isEmpty() || v == QLatin1String("no") || v == QLatin1String("none") || v == QLatin1String("n"))
        return QSerialPort::NoParity;
    if (v == QLatin1String("even") || v == QLatin1String("e"))
        return QSerialPort::EvenParity;
    if (v == QLatin1String("odd") || v == QLatin1String("o"))
        return QSerialPort::OddParity;
    *known = false;
    return QSerialPort::NoParity;
}

/// 解析可选停止位（1/2；缺省 1）
QSerialPort::StopBits stopBitsFromInt(int n, bool* known)
{
    *known = true;
    if (n == 1)
        return QSerialPort::OneStop;
    if (n == 2)
        return QSerialPort::TwoStop;
    *known = false;
    return QSerialPort::OneStop;
}

} // namespace

bool ModbusRtuDriver::ensureConnectionParams()
{
    const QString port = m_conn.value(QStringLiteral("port")).trimmed();
    if (port.isEmpty()) {
        qWarning().noquote() << logTag() << ": 设备连接参数缺串口路径 port——不启动"
                             << "（RTU 不使用默认串口，请在设备配置里显式指定，如 /dev/ttyUSB0）";
        return false;
    }
    // 波特率白名单与 PC 侧 ModbusRtuRules.AllowedBaudRates 一致（1200/2400/4800/9600/19200/38400/57600/115200）
    static const int kAllowedBaud[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
    const QString baudRaw = m_conn.value(QStringLiteral("baud")).trimmed();
    const int baud = baudRaw.toInt();
    bool baudOk = false;
    for (const int b : kAllowedBaud)
        if (b == baud) { baudOk = true; break; }
    if (!baudOk) {
        // 键缺失/空 → 默认（静默）；键存在但非法 → 告警 + 回落（AB-2 审查 🔴-1：原一律静默，掩盖契约取值缺陷）
        if (!baudRaw.isEmpty())
            qWarning().noquote() << logTag() << ": 波特率取值非法（\"" << baudRaw << "\"）——回落默认"
                                 << kModbusRtuDefaultBaud;
        m_conn.insert(QStringLiteral("baud"), QString::number(kModbusRtuDefaultBaud));
    }
    // 🟡-2（AB-2 审查）：校验位/数据位/停止位在此一次性规范化 + 告警（applyConnectionParameters 每次重连都会调用，
    // 原把校验与告警放在那里 → 每次重连重复刷告警）
    bool known = true;
    parityFromString(m_conn.value(QStringLiteral("parity")), &known);
    if (!known) {
        qWarning().noquote() << logTag() << ": 校验位取值非法"
                             << m_conn.value(QStringLiteral("parity")) << "——按无校验(8N1)处理";
        m_conn.insert(QStringLiteral("parity"), QStringLiteral("none"));
    }
    const QString dataBitsRaw = m_conn.value(QStringLiteral("dataBits")).trimmed();
    if (!dataBitsRaw.isEmpty() && (dataBitsRaw.toInt() < 5 || dataBitsRaw.toInt() > 8)) {
        qWarning().noquote() << logTag() << ": 数据位取值非法" << dataBitsRaw << "——按 8 处理";
        m_conn.insert(QStringLiteral("dataBits"), QStringLiteral("8"));
    }
    const QString stopRaw = m_conn.value(QStringLiteral("stopBits")).trimmed();
    if (!stopRaw.isEmpty()) {
        bool stopKnown = true;
        stopBitsFromInt(stopRaw.toInt(), &stopKnown);
        if (!stopKnown) {
            qWarning().noquote() << logTag() << ": 停止位取值非法" << stopRaw << "——按 1 处理";
            m_conn.insert(QStringLiteral("stopBits"), QStringLiteral("1"));
        }
    }
    qInfo().noquote() << logTag() << ": 串口参数" << targetDescription()
                      << "databits=" << m_conn.value(QStringLiteral("dataBits"), QStringLiteral("8"))
                      << "parity=" << m_conn.value(QStringLiteral("parity"), QStringLiteral("none"))
                      << "stopbits=" << m_conn.value(QStringLiteral("stopBits"), QStringLiteral("1"));
    return true;
}

QModbusClient* ModbusRtuDriver::createClient()
{
    return new QModbusRtuSerialClient(this);
}

void ModbusRtuDriver::applyConnectionParameters(QModbusClient* client)
{
    auto* rtu = static_cast<QModbusRtuSerialClient*>(client);
    rtu->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                m_conn.value(QStringLiteral("port")).trimmed());
    rtu->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                m_conn.value(QStringLiteral("baud"),
                                             QString::number(kModbusRtuDefaultBaud)).toInt());
    bool parityKnown = true;
    const QSerialPort::Parity parity = parityFromString(m_conn.value(QStringLiteral("parity")), &parityKnown);
    rtu->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                parityKnown ? parity : QSerialPort::NoParity);
    const int dataBits = m_conn.value(QStringLiteral("dataBits"), QStringLiteral("8")).toInt();
    rtu->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                (dataBits >= 5 && dataBits <= 8) ? QSerialPort::DataBits(dataBits)
                                                                 : QSerialPort::Data8);
    bool stopKnown = true;
    const QSerialPort::StopBits stop =
        stopBitsFromInt(m_conn.value(QStringLiteral("stopBits"), QStringLiteral("1")).toInt(), &stopKnown);
    rtu->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                stopKnown ? stop : QSerialPort::OneStop);
    // 注意：不在连接参数里下发 slaveId——从站号随每个请求（sendReadRequest/sendWriteRequest）走，
    // 与 TCP 一致（同一驱动可服务本设备多个从站地址的变量）。
}

QString ModbusRtuDriver::targetDescription() const
{
    return QStringLiteral("%1@%2").arg(m_conn.value(QStringLiteral("port")).trimmed(),
                                       m_conn.value(QStringLiteral("baud"),
                                                    QString::number(kModbusRtuDefaultBaud)));
}

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MODBUS_DRIVER
#include "drivers/driver_registry.h"
extern "C" void init_driver_modbus_rtu()
{
    navihmi::registerDriverFactory(std::make_unique<navihmi::ModbusRtuDriverFactory>());
}
#endif
