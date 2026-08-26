/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.cpp
 * @Description: 数据采集引擎实现——Modbus TCP 轮询读 + 写通道（首版）
 */
#include "runtime/acquisition.h"
#include "runtime/datamanager.h"

#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

namespace navihmi {

Acquisition::Acquisition(QObject* parent)
    : QObject(parent)
{
}

Acquisition::~Acquisition()
{
    if (m_client) {
        m_client->disconnectDevice();
        delete m_client;
        m_client = nullptr;
    }
}

void Acquisition::setProject(const Project& proj)
{
    m_project = proj;
    m_tags.clear();
    // 解析 Tag.source = "modbus://{从站}/{寄存器}" → 采集任务
    for (const auto& tag : m_project.tags) {
        if (!tag.source.startsWith(QStringLiteral("modbus://")))
            continue;
        ModbusTag mt;
        mt.tagName = tag.name;
        mt.dataType = int(tag.dataType);
        mt.scanMs = tag.scanIntervalMs > 0 ? tag.scanIntervalMs : 1000;   // 默认 1s
        mt.deadband = tag.deadband;
        const QString uri = tag.source.mid(QStringLiteral("modbus://").size());
        // "slave/reg"（可选带前缀: tcp://host:port/slave/reg 简化: 连接参数走 deviceName → connectionInfo）
        const QStringList parts = uri.split(QLatin1Char('/'));
        if (parts.size() >= 1 && !parts[0].isEmpty())
            mt.slave = parts[0].toInt();
        if (parts.size() >= 2 && !parts[1].isEmpty())
            mt.reg = quint16(parts[1].toUInt());
        if (mt.slave < 1 || mt.slave > kModbusSlaveMax)
            continue;
        mt.conn = connInfoForDevice(tag.deviceName);
        if (mt.conn.isEmpty()) {
            // 无设备连接信息 → 默认连接（环境变量可覆盖: NAVIHMI_MODBUS_HOST/PORT——PC 模拟从站验证用）
            mt.conn.insert(QStringLiteral("host"), QString::fromLocal8Bit(qgetenv("NAVIHMI_MODBUS_HOST").isEmpty()
                ? "127.0.0.1" : qgetenv("NAVIHMI_MODBUS_HOST")));
            mt.conn.insert(QStringLiteral("port"), QString::fromLocal8Bit(qgetenv("NAVIHMI_MODBUS_PORT").isEmpty()
                ? kModbusDefaultPort : qgetenv("NAVIHMI_MODBUS_PORT")));
        }
        m_tags.append(mt);
    }
    if (m_tags.isEmpty()) {
        qInfo().noquote() << "Acquisition: 无 modbus 变量（source 非 modbus://）, 采集未启动";
        return;
    }
    // 审查 BLOCKER-1(2026-08-24 H 循环): m_conn 赋值——否则 ensureConnected 恒不连接,
    // 采集引擎空转（编译/启动通过但永不建立 Modbus 连接）
    m_conn = m_tags.first().conn;
    // J-3 审查修复: 工程重载重置退避时间戳（旧工程失败时间戳不应抑制新工程首连/吞首条日志）
    m_lastConnectFailMs = 0;
    m_lastConnectWarnMs = 0;
    m_connectedDevice.clear();
    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(100);   // 100ms 调度（过程数据 ≤100ms 目标）
        connect(m_timer, &QTimer::timeout, this, &Acquisition::tick);
        m_timer->start();
    }
    qInfo().noquote() << "Acquisition: 采集启动" << m_tags.size() << "个 modbus 变量";
}

void Acquisition::setDataManager(DataManager* dm)
{
    m_dataManager = dm;
}

QHash<QString, QString> Acquisition::connInfoForDevice(const QString& deviceName) const
{
    if (deviceName.isEmpty())
        return {};
    for (const auto& dev : m_project.devices) {
        if (dev.name == deviceName && !dev.connectionInfo.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(dev.connectionInfo.toUtf8());
            if (doc.isObject()) {
                QHash<QString, QString> c;
                const QJsonObject o = doc.object();
                for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                    c.insert(it.key(), it.value().toString());
                return c;
            }
        }
    }
    return {};
}

void Acquisition::ensureConnected()
{
    if (!m_client) {
        m_client = new QModbusTcpClient(this);
        connect(m_client, &QModbusClient::stateChanged,
                this, [this](QModbusDevice::State s) {
            if (s == QModbusDevice::ConnectedState) {
                m_connecting = false;
                qInfo().noquote() << "Acquisition: Modbus TCP 已连接" << m_conn.value("host") << m_conn.value("port");
            } else if (s == QModbusDevice::UnconnectedState) {
                m_connecting = false;
            }
        });
        connect(m_client, &QModbusDevice::errorOccurred, this,
                [this](QModbusDevice::Error e) {
            if (e == QModbusDevice::NoError) return;
            // J-3 审查修复: 仅未连接状态（连接阶段）错误才记连接失败/打日志——
            // 连接成功后轮询读超时/协议错误会传播到 client，不应算连接失败（误导日志 + 刷新时间戳推迟断线重连）
            if (m_client->state() == QModbusDevice::ConnectedState)
                return;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            m_lastConnectFailMs = nowMs;
            if (nowMs - m_lastConnectWarnMs >= 5000) {
                m_lastConnectWarnMs = nowMs;
                qWarning().noquote() << "Acquisition: Modbus 连接失败（错误" << int(e)
                                     << "），5s 后重试" << m_conn.value("host") << m_conn.value("port");
            }
        });
    }
    if (m_client->state() != QModbusDevice::ConnectedState && !m_connecting) {
        // 取第一个任务连接参数（首版单连接; 多设备场景后续扩展连接池）
        if (!m_tags.isEmpty() && !m_conn.isEmpty()) {
            // J-3: 重连退避——失败后 5s 内不重试（MINOR-5；无 modbus 变量时 tick 提前返回零开销不变）
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (nowMs - m_lastConnectFailMs < 5000)
                return;
            m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_conn.value("port", kModbusDefaultPort).toInt());
            m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, m_conn.value("host", "127.0.0.1"));
            m_client->setTimeout(kModbusTimeoutMs);   // Modbus 请求超时（2026-08-26 魔法数字整改命名）
            m_client->setNumberOfRetries(1);
            m_connecting = true;
            m_client->connectDevice();
        }
    }
}

void Acquisition::tick()
{
    if (m_tags.isEmpty() || !m_dataManager)
        return;
    ensureConnected();
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
        return;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const auto& tag : m_tags) {
        if (nowMs - tag.lastReadMs >= tag.scanMs) {
            readTag(&tag);
        }
    }
}

void Acquisition::readTag(const ModbusTag* tag)
{
    if (!m_client || !tag)
        return;
    // 读保持寄存器（Float/Int32 2 寄存器, 其余 1）
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
                                const QVariant v = decodeValue(t, raw);
                                // 审查 MAJOR-4: lastReadMs 无条件更新——否则 deadband 拦截后
                                // 时间戳陈旧, 100ms 轮询风暴（无视 scanMs）
                                t.lastReadMs = QDateTime::currentMSecsSinceEpoch();
                                // deadband 防抖: 值变化超死区才写 DataManager
                                bool write = t.deadband <= 0 || !t.lastVal.isValid()
                                             || qAbs(v.toDouble() - t.lastVal.toDouble()) >= t.deadband;
                                if (write) {
                                    t.lastVal = v;
                                    if (m_dataManager)
                                        m_dataManager->setValue(t.tagName, v);
                                }
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

QVariant Acquisition::decodeValue(const ModbusTag& tag, quint16 raw) const
{
    switch (tag.dataType) {
    case int(TagDataType::Bool):
        return bool(raw != 0);
    case int(TagDataType::Int16):
        return int(qint16(raw));
    case int(TagDataType::Uint16):
        return int(raw);
    case int(TagDataType::Float): {
        // 简化: 单寄存器 16bit 存放大 100 倍整数（首版协议约定; 32bit float 双寄存器后续）
        return double(raw) / 100.0;
    }
    case int(TagDataType::Int32):
        return int(raw);
    default:
        return double(raw);
    }
}

void Acquisition::handleValueWritten(const QString& tagName, const QVariant& value)
{
    // H-8 写通道: DataManager 写 modbus 来源变量 → 同步写设备（main.cpp 连接 valueChanged 调用）
    for (const auto& tag : m_tags)
        if (tag.tagName == tagName) {
            writeTag(tagName, value);
            return;
        }
}

void Acquisition::writeTag(const QString& tagName, const QVariant& value)
{
    // H-8 写通道（DataManager → modbus 设备）——由 main.cpp DataManager valueChanged 联动调用
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
        return;
    for (const auto& tag : m_tags) {
        if (tag.tagName == tagName) {
            QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, tag.reg, 1);
            // 审查 MAJOR-6: 按类型编码——Float 按 /100 约定反向缩放（读侧放大 100 倍存单寄存器）
            quint16 raw = 0;
            switch (tag.dataType) {
            case int(TagDataType::Float):
                raw = quint16(qRound(value.toDouble() * 100.0) & 0xFFFF);
                break;
            case int(TagDataType::Bool):
                raw = value.toBool() ? 1 : 0;
                break;
            default:
                raw = quint16(value.toInt() & 0xFFFF);
                break;
            }
            unit.setValue(0, raw);
            m_client->sendWriteRequest(unit, tag.slave);
            qInfo().noquote() << "Acquisition: 写 modbus" << tagName
                              << "slave=" << tag.slave << "reg=" << tag.reg << "val=" << value << "raw=" << raw;
            return;
        }
    }
}

} // namespace navihmi
