/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_tcp_driver.cpp
 * @Description: Modbus TCP 采集驱动实现（AB-2 2026-09-11——公共逻辑上提 ModbusDriverBase；
 *              本文件仅剩 TCP 传输层，V-1 行为不变回归锚）
 */
#include "drivers/modbus_tcp_driver.h"

#include <QModbusTcpClient>
#include <QDebug>
#include <memory>   // std::make_unique（注册入口）

namespace navihmi {

namespace {
/// 生效主机名：PC 侧契约键为 ip（DeviceConfig/DeviceEditDialog/CLI 三处一致），
/// 兼容历史 host 键与 NAVIHMI_MODBUS_HOST 环境变量兜底（AB-2 审查 🔴-2：原只读 host → 恒 127.0.0.1）
QString effectiveHost(const QHash<QString, QString>& conn)
{
    const QString ip = conn.value(QStringLiteral("ip")).trimmed();
    if (!ip.isEmpty())
        return ip;
    const QString host = conn.value(QStringLiteral("host")).trimmed();
    if (!host.isEmpty())
        return host;
    return QStringLiteral("127.0.0.1");
}

/// 生效端口：缺省 502（值为空/0/非数一律回落默认，防 NetworkPortParameter=0）
int effectivePort(const QHash<QString, QString>& conn)
{
    bool ok = false;
    const int port = conn.value(QStringLiteral("port")).toInt(&ok);
    return (ok && port > 0 && port <= 65535) ? port : QString::fromLatin1(kModbusDefaultPort).toInt();
}
} // namespace

QModbusClient* ModbusTcpDriver::createClient()
{
    return new QModbusTcpClient(this);
}

void ModbusTcpDriver::applyConnectionParameters(QModbusClient* client)
{
    auto* tcp = static_cast<QModbusTcpClient*>(client);
    tcp->setConnectionParameter(QModbusDevice::NetworkPortParameter, effectivePort(m_conn));
    tcp->setConnectionParameter(QModbusDevice::NetworkAddressParameter, effectiveHost(m_conn));
}

QString ModbusTcpDriver::targetDescription() const
{
    return QStringLiteral("%1:%2").arg(effectiveHost(m_conn)).arg(effectivePort(m_conn));
}

bool ModbusTcpDriver::ensureConnectionParams()
{
    if (!m_conn.isEmpty()) {
        // 键存在但非 ip/host（如旧工程只写 port）也要能连——记一条生效参数便于排障
        qInfo().noquote() << logTag() << ": 连接目标" << targetDescription();
        return true;
    }
    // 首 tag 无连接信息 → 默认连接（环境变量可覆盖: NAVIHMI_MODBUS_HOST/PORT——PC 模拟从站验证用）
    m_conn.insert(QStringLiteral("ip"), QString::fromLocal8Bit(qgetenv("NAVIHMI_MODBUS_HOST").isEmpty()
        ? "127.0.0.1" : qgetenv("NAVIHMI_MODBUS_HOST")));
    m_conn.insert(QStringLiteral("port"), QString::fromLocal8Bit(qgetenv("NAVIHMI_MODBUS_PORT").isEmpty()
        ? kModbusDefaultPort : qgetenv("NAVIHMI_MODBUS_PORT")));
    qInfo().noquote() << logTag() << ": 无设备连接信息，使用默认/环境连接目标" << targetDescription();
    return true;
}

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MODBUS_DRIVER
#include "drivers/driver_registry.h"
extern "C" void init_driver_modbus_tcp()
{
    navihmi::registerDriverFactory(std::make_unique<navihmi::ModbusTcpDriverFactory>());
}
#endif
