/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_tcp_driver.cpp
 * @Description: Modbus TCP 采集驱动实现（V-1 2026-09-06 从 Acquisition 1:1 迁入——行为不变回归锚）
 *               值回传: valueRead(tagName, 解码值, deadband)——deadband 判断归 Manager（跨协议统一）
 *               连接失败退避/日志降频 = J-3 原语义保留
 */
#include "drivers/modbus_tcp_driver.h"
#include "runtime/projectmodel.h"   // TagDataType 枚举（decodeValue/读写编码用）

#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>
#include <QDateTime>
#include <cmath>    // Y-7: std::isfinite（Float 写值范围校验）
#include <memory>

namespace navihmi {

ModbusTcpDriver::ModbusTcpDriver(QObject* parent)
    : IDriver(parent)
{
}

ModbusTcpDriver::~ModbusTcpDriver()
{
    if (m_client) {
        m_client->disconnectDevice();
        delete m_client;
        m_client = nullptr;
    }
}

bool ModbusTcpDriver::configure(const QList<DriverTagInfo>& tags)
{
    m_tags.clear();
    m_conn.clear();
    bool haveFirstConn = false;   // 复审 🟡（2026-09-06）：conn 严格取**首个有效 tag**（含空——尾部 env 兜底对齐基线
                                  // m_tags.first().conn + 逐 tag env 填充语义；后续 tag conn 不干扰单连接目标）
    for (const auto& tag : tags) {
        if (!tag.source.startsWith(QStringLiteral("modbus://")))
            continue;   // Manager 已按协议分组，此处双保险过滤
        ModbusTag mt;
        mt.tagName = tag.name;
        mt.dataType = tag.dataType;
        mt.scanMs = tag.scanMs > 0 ? tag.scanMs : 1000;   // 默认 1s
        mt.deadband = tag.deadband;
        const QString uri = tag.source.mid(QStringLiteral("modbus://").size());
        const QStringList parts = uri.split(QLatin1Char('/'));
        if (parts.size() >= 1 && !parts[0].isEmpty())
            mt.slave = parts[0].toInt();
        if (parts.size() >= 2 && !parts[1].isEmpty())
            mt.reg = quint16(parts[1].toUInt());
        if (mt.slave < 1 || mt.slave > kModbusSlaveMax)
            continue;
        // Y-7：寄存器地址范围校验（Modbus 保持寄存器 0-65535；越界 → 跳过该变量防读写越界）
        if (parts.size() >= 2 && !parts[1].isEmpty() && parts[1].toUInt() > 0xFFFF)
            continue;
        if (!haveFirstConn) {
            m_conn = tag.conn;   // 连接参数: Manager 已展开 deviceName→connectionInfo（首个有效 tag 决定连接目标）
            haveFirstConn = true;
        }
        m_tags.append(mt);
    }
    if (m_tags.isEmpty()) {
        qInfo().noquote() << "ModbusTcpDriver: 无有效 modbus 变量（source 非 modbus:// 或从站地址非法）";
        return false;
    }
    if (m_conn.isEmpty()) {
        // 首 tag 无连接信息 → 默认连接（环境变量可覆盖: NAVIHMI_MODBUS_HOST/PORT——PC 模拟从站验证用）
        m_conn.insert(QStringLiteral("host"), QString::fromLocal8Bit(qgetenv("NAVIHMI_MODBUS_HOST").isEmpty()
            ? "127.0.0.1" : qgetenv("NAVIHMI_MODBUS_HOST")));
        m_conn.insert(QStringLiteral("port"), QString::fromLocal8Bit(qgetenv("NAVIHMI_MODBUS_PORT").isEmpty()
            ? kModbusDefaultPort : qgetenv("NAVIHMI_MODBUS_PORT")));
    }
    m_lastConnectFailMs = 0;   // J-3: 工程重载重置退避（旧失败时间戳不抑制新工程首连）
    m_lastConnectWarnMs = 0;
    return true;
}

void ModbusTcpDriver::start()
{
    ensureConnected();
}

void ModbusTcpDriver::stop()
{
    if (m_client) {
        m_client->disconnectDevice();
        m_connecting = false;
    }
}

void ModbusTcpDriver::ensureConnected()
{
    if (!m_client) {
        m_client = new QModbusTcpClient(this);
        connect(m_client, &QModbusClient::stateChanged, this,
                [this](QModbusDevice::State s) {
            if (s == QModbusDevice::ConnectedState) {
                m_connecting = false;
                qInfo().noquote() << "ModbusTcpDriver: 已连接" << m_conn.value("host") << m_conn.value("port");
                emit stateChanged(true);
            } else if (s == QModbusDevice::UnconnectedState) {
                m_connecting = false;
                emit stateChanged(false);
            }
        });
        connect(m_client, &QModbusDevice::errorOccurred, this,
                [this](QModbusDevice::Error e) {
            if (e == QModbusDevice::NoError) return;
            // J-3 审查修复: 仅未连接状态（连接阶段）错误才记连接失败/打日志——
            // 连接成功后轮询读超时/协议错误传播到 client 不应算连接失败（误导日志 + 刷新时间戳推迟断线重连）
            if (m_client->state() == QModbusDevice::ConnectedState)
                return;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            m_lastConnectFailMs = nowMs;
            if (nowMs - m_lastConnectWarnMs >= 5000) {
                m_lastConnectWarnMs = nowMs;
                qWarning().noquote() << "ModbusTcpDriver: 连接失败（错误" << int(e)
                                     << "），5s 后重试" << m_conn.value("host") << m_conn.value("port");
                // 复审 🟡（2026-09-06）：connectionError 随 5s 降频分支发射——原在分支外无条件发
                // 导致 Manager 中继与 driver 自身告警双日志重复（绕过 J-3 降频精神）
                emit connectionError(QStringLiteral("Modbus TCP 连接失败: %1").arg(m_conn.value("host")));
            }
        });
    }
    if (m_client->state() != QModbusDevice::ConnectedState && !m_connecting && !m_tags.isEmpty()) {
        // J-3: 重连退避——失败后 5s 内不重试
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastConnectFailMs < 5000)
            return;
        m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_conn.value("port", kModbusDefaultPort).toInt());
        m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, m_conn.value("host", "127.0.0.1"));
        m_client->setTimeout(kModbusTimeoutMs);
        m_client->setNumberOfRetries(1);
        m_connecting = true;
        m_client->connectDevice();
    }
}

void ModbusTcpDriver::poll(qint64 nowMs)
{
    if (m_tags.isEmpty())
        return;
    ensureConnected();
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
        return;
    for (const auto& tag : m_tags) {
        if (nowMs - tag.lastReadMs >= tag.scanMs)
            readTag(&tag);
    }
}

void ModbusTcpDriver::readTag(const ModbusTag* tag)
{
    if (!m_client || !tag)
        return;
    // 读保持寄存器（Float/Int32 2 寄存器, 其余 1——原 Acquisition 语义保留）
    const int count = (tag->dataType == int(TagDataType::Float)
                       || tag->dataType == int(TagDataType::Int32)) ? 2 : 1;
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, tag->reg, quint16(count));
    if (auto* reply = m_client->sendReadRequest(unit, tag->slave)) {
        if (!reply->isFinished()) {
            // Qt 6: 每请求独立 reply, finished 回调处理（lambda 捕获 tag 副本防悬垂）
            const ModbusTag captured = *tag;
            connect(reply, &QModbusReply::finished, this, [this, reply, captured]() {
                if (reply->error() == QModbusDevice::NoError) {
                    const auto result = reply->result();
                    if (result.isValid() && result.values().size() >= 1) {
                        const quint16 raw = result.values().at(0);
                        for (auto& t : m_tags) {
                            if (t.tagName == captured.tagName) {
                                // MAJOR-4 语义保留: 读到即更新 lastReadMs（防 100ms 轮询风暴）
                                t.lastReadMs = QDateTime::currentMSecsSinceEpoch();
                                const QVariant v = decodeValue(t, raw);
                                emit valueRead(t.tagName, v, t.deadband);   // deadband 归 Manager 判断
                                break;
                            }
                        }
                    }
                }
                reply->deleteLater();
            });
        } else {
            reply->deleteLater();   // 立即完成（错误）直接释放
        }
    }
}

QVariant ModbusTcpDriver::decodeValue(const ModbusTag& tag, quint16 raw) const
{
    switch (tag.dataType) {
    case int(TagDataType::Bool):
        return bool(raw != 0);
    case int(TagDataType::Int16):
        return int(qint16(raw));
    case int(TagDataType::Uint16):
        return int(raw);
    case int(TagDataType::Float): {
        // 简化协议: 单寄存器 16bit 放大 100 倍整数（原 Acquisition 约定，行为不变）
        return double(raw) / 100.0;
    }
    case int(TagDataType::Int32):
        return int(raw);
    default:
        return double(raw);
    }
}

bool ModbusTcpDriver::writeValue(const QString& tagName, const QVariant& value)
{
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
        return false;
    for (const auto& tag : m_tags) {
        if (tag.tagName == tagName) {
            // Y-7：寄存器范围校验在 configure 完成（quint16 类型 + configure 拦 >65535 原始值）——此处无需重复
            QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, tag.reg, 1);
            // 按类型编码——Float 按 /100 约定反向缩放（原 Acquisition MAJOR-6 语义）
            quint16 raw = 0;
            switch (tag.dataType) {
            case int(TagDataType::Float): {
                // Y-7：Float 编码侧防溢出（/100 放大后 quint16 域 ±327.67——越界/NaN/Inf 拒绝防静默截断写坏设备；
                // 负值回读不对称（读端无符号 /100 解码）系既有 MAJOR-6 约定，本批不改编码语义）
                const double d = value.toDouble();
                if (!std::isfinite(d) || d < -327.68 || d > 327.67) {
                    qWarning().noquote() << "ModbusTcpDriver: 写" << tagName << "Float 值越界/非法（" << d
                                         << "，编码侧支持 ±327.67 内）——拒绝写入";
                    return false;
                }
                raw = quint16(qRound(d * 100.0) & 0xFFFF);
                break;
            }
            case int(TagDataType::Bool):
                raw = value.toBool() ? 1 : 0;
                break;
            default: {
                // Y-7：整型值范围校验（Int16 -32768~32767 / Uint16 0~65535——防符号位静默翻转写错）
                const long long v = value.toLongLong();
                bool okRange = tag.dataType == int(TagDataType::Int16) ? (v >= -32768 && v <= 32767)
                    : (v >= 0 && v <= 65535);   // Uint16/Int32 写低 16 位按 Uint16 范围
                if (!okRange) {
                    qWarning().noquote() << "ModbusTcpDriver: 写" << tagName << "整型值越界（" << v
                                         << "）——拒绝写入";
                    return false;
                }
                raw = quint16(v & 0xFFFF);
                break;
            }
            }
            unit.setValue(0, raw);
            // Y-7：写回复错误检查（原 sendWriteRequest 丢弃 reply——写失败静默；补 reply 错误日志——
            // 写失败事件驱动频率有限，逐条记录不设降频；reviewer 🟡 立即分支日志补 slave/reg）
            const int slave = tag.slave;
            const quint16 reg = tag.reg;
            const QString tName = tagName;
            if (auto* reply = m_client->sendWriteRequest(unit, tag.slave)) {
                if (reply->isFinished()) {
                    if (reply->error() != QModbusDevice::NoError)
                        qWarning().noquote() << "ModbusTcpDriver: 写" << tName
                                             << "slave=" << slave << "reg=" << reg << "失败（错误"
                                             << int(reply->error()) << "）";
                    reply->deleteLater();
                } else {
                    connect(reply, &QModbusReply::finished, this, [this, reply, tName, slave, reg]() {
                        if (reply->error() != QModbusDevice::NoError)
                            qWarning().noquote() << "ModbusTcpDriver: 写" << tName
                                                 << "slave=" << slave << "reg=" << reg << "失败（错误"
                                                 << int(reply->error()) << "）";
                        reply->deleteLater();
                    });
                }
            }
            qInfo().noquote() << "ModbusTcpDriver: 写" << tagName
                              << "slave=" << tag.slave << "reg=" << tag.reg << "val=" << value << "raw=" << raw;
            return true;
        }
    }
    return false;
}

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MODBUS_DRIVER
#include "drivers/driver_registry.h"
extern "C" void init_driver_modbus_tcp()
{
    navihmi::registerDriverFactory(std::make_unique<navihmi::ModbusTcpDriverFactory>());
}
#endif
