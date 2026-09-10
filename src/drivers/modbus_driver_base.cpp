/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_driver_base.cpp
 * @Description: Modbus 双传输公共基类实现（AB-2 2026-09-11——自 modbus_tcp_driver.cpp 1:1 上提，
 *              TCP 行为不变回归锚；RTU 复用同一套 tag 解析/解码/写校验/调度/退避）
 */
#include "drivers/modbus_driver_base.h"
#include "runtime/projectmodel.h"   // TagDataType 枚举（decodeValue/读写编码用）

#include <QModbusClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>
#include <QDateTime>
#include <cmath>    // Y-7: std::isfinite（Float 写值范围校验）

namespace navihmi {

ModbusDriverBase::ModbusDriverBase(QObject* parent)
    : IDriver(parent)
{
}

ModbusDriverBase::~ModbusDriverBase()
{
    if (m_client) {
        m_client->disconnectDevice();
        delete m_client;
        m_client = nullptr;
    }
}

bool ModbusDriverBase::configure(const QList<DriverTagInfo>& tags)
{
    m_tags.clear();
    m_conn.clear();
    bool haveFirstConn = false;   // conn 严格取**首个有效 tag**（含空——尾部 env 兜底对齐基线语义；
                                  // AB-3 起每实例只收本设备 tag，首 tag 即本设备连接参数）
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
            m_conn = tag.conn;   // 连接参数: Manager 已展开 deviceName→connectionInfo
            haveFirstConn = true;
        }
        m_tags.append(mt);
    }
    if (m_tags.isEmpty()) {
        qInfo().noquote() << logTag() << ": 无有效 modbus 变量（source 非 modbus:// 或从站地址非法）";
        return false;
    }
    // AB-2：传输相关兜底/校验下放子类（TCP: 环境变量+默认端口；RTU: 必须显式串口路径）
    if (!ensureConnectionParams()) {
        qWarning().noquote() << logTag() << ": 连接参数不可用——本驱动不启动（变量数" << m_tags.size() << "）";
        m_tags.clear();
        return false;
    }
    m_lastConnectFailMs = 0;   // J-3: 工程重载重置退避（旧失败时间戳不抑制新工程首连）
    m_lastConnectWarnMs = 0;
    m_lastReadFailMs = 0;      // ⚪-2（复审）：读失败统计一并重置（防打印上一工程的累计数）
    m_readFailCount = 0;
    m_lastPortResetMs = 0;
    m_lastInFlightWarnMs = 0;
    return true;
}

void ModbusDriverBase::start()
{
    ensureConnected();
}

void ModbusDriverBase::stop()
{
    if (m_client) {
        m_client->disconnectDevice();
        m_connecting = false;
    }
    clearAllInFlight();   // 🔴-1：停止后清在途（重载工程不会带着卡死标记）
}

void ModbusDriverBase::ensureConnected()
{
    if (m_tags.isEmpty())
        return;
    if (!m_client) {
        if (m_clientCreateFailed)
            return;   // 🟡-1：已判定传输不可用——不再重复尝试/告警（防 100ms 轮询刷屏）
        m_client = createClient();
        if (!m_client) {
            m_clientCreateFailed = true;
            qWarning().noquote() << logTag() << ": 传输层不可用（client 创建失败）——采集未启动";
            return;
        }
        connect(m_client, &QModbusClient::stateChanged, this,
                [this](QModbusDevice::State s) {
            if (s == QModbusDevice::ConnectedState) {
                m_connecting = false;
                qInfo().noquote() << logTag() << ": 已连接" << targetDescription();
                emit stateChanged(true);
            } else if (s == QModbusDevice::UnconnectedState) {
                m_connecting = false;
                clearAllInFlight();   // 🔴-1：断连路径兜底清在途（abort 已让 reply finish 时此处幂等）
                emit stateChanged(false);
            }
        });
        connect(m_client, &QModbusDevice::errorOccurred, this,
                [this](QModbusDevice::Error e) {
            if (e == QModbusDevice::NoError) return;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_client->state() == QModbusDevice::ConnectedState) {
                // 🔴-1（复审 2026-09-11，板端 Qt 6.4.3 源码实证）：**reply 错误不传播到 client**
                // （qmodbusclient.cpp processQueueElement 只 set reply 错误；全文无 reply→client 连线），
                // 故到达 client 且仍处 ConnectedState 的错误只能是**端口/传输级**（Read/Write/ConnectionError）
                // ——原「一律 return」会让状态卡在 Connected：既不重连（ensureConnected 判 state==Connected 直接跳过），
                // 在途 reply 又永不 finish（RTU 重试路径写失败后不再武装响应定时器）→ 该变量永久静默停采。
                // 处置：节流断开重连（断连会 abort 队列 reply → finished → 清在途），余下交既有 5s 退避重连。
                if (nowMs - m_lastPortResetMs >= 5000) {
                    m_lastPortResetMs = nowMs;
                    m_lastConnectFailMs = nowMs;   // 复用 J-3 退避：复位后不立即重连
                    qWarning().noquote() << logTag() << ": 端口级错误（错误" << int(e)
                                         << "）——断开重连" << targetDescription();
                    m_client->disconnectDevice();
                }
                return;
            }
            // 未连接状态（连接阶段）错误 → 记连接失败 + 5s 降频告警
            m_lastConnectFailMs = nowMs;
            if (nowMs - m_lastConnectWarnMs >= 5000) {
                m_lastConnectWarnMs = nowMs;
                qWarning().noquote() << logTag() << ": 连接失败（错误" << int(e)
                                     << "），5s 后重试" << targetDescription();
                emit connectionError(QStringLiteral("%1: 连接失败 %2").arg(logTag(), targetDescription()));
            }
        });
    }
    if (m_client->state() != QModbusDevice::ConnectedState && !m_connecting && !m_tags.isEmpty()) {
        // J-3: 重连退避——失败后 5s 内不重试
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastConnectFailMs < 5000)
            return;
        applyConnectionParameters(m_client);
        m_client->setTimeout(kModbusTimeoutMs);
        m_client->setNumberOfRetries(1);
        m_connecting = true;
        m_client->connectDevice();
    }
}

void ModbusDriverBase::poll(qint64 nowMs)
{
    if (m_tags.isEmpty())
        return;
    ensureConnected();
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
        return;
    for (auto& tag : m_tags) {
        if (tag.inFlight) {
            // 🔴-1（复审 2026-09-11）：**在途看门狗**——Qt RTU 重试路径存在「reply 永不 finished」窗口
            // （写失败后不再武装响应定时器 + 端口级错误不改 state），仅靠 finished 清标记会导致该变量
            // 永久静默停采且零日志。超时即复位标记（最多丢一次采样），并降频告警留痕。
            // 守卫 inFlightSinceMs > 0：未置位（异常路径）时绝不误触发。
            const qint64 limitMs = qMax<qint64>(3 * tag.scanMs, 2000);
            if (tag.inFlightSinceMs > 0 && nowMs - tag.inFlightSinceMs > limitMs) {
                const qint64 elapsed = nowMs - tag.inFlightSinceMs;
                tag.inFlight = false;
                tag.inFlightSinceMs = 0;
                if (nowMs - m_lastInFlightWarnMs >= 5000) {   // 独立节流槽：不与读失败日志互相压制
                    m_lastInFlightWarnMs = nowMs;
                    qWarning().noquote() << logTag() << ": 读" << tag.tagName
                                         << "在途请求超时未回（" << elapsed
                                         << "ms）——重置在途标记" << targetDescription();
                }
            } else {
                continue;
            }
        }
        if (nowMs - tag.lastReadMs >= tag.scanMs)
            readTag(&tag);
    }
}

void ModbusDriverBase::clearAllInFlight()
{
    for (auto& t : m_tags)
        t.inFlight = false;
}

void ModbusDriverBase::readTag(const ModbusTag* tag)
{
    if (!m_client || !tag)
        return;
    // 读保持寄存器（Float/Int32 2 寄存器, 其余 1——原 Acquisition 语义保留）
    const int count = (tag->dataType == int(TagDataType::Float)
                       || tag->dataType == int(TagDataType::Int32)) ? 2 : 1;
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, tag->reg, quint16(count));
    const QString tagName = tag->tagName;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto& t : m_tags)
        if (t.tagName == tagName) { t.inFlight = true; t.inFlightSinceMs = nowMs; break; }   // 🟡-4/🔴：成对置位+记时
    if (auto* reply = m_client->sendReadRequest(unit, tag->slave)) {
        if (!reply->isFinished()) {
            // Qt 6: 每请求独立 reply, finished 回调处理（lambda 捕获 tag 副本防悬垂）
            const ModbusTag captured = *tag;
            connect(reply, &QModbusReply::finished, this, [this, reply, captured]() {
                clearInFlight(captured.tagName);
                if (reply->error() == QModbusDevice::NoError) {
                    const auto result = reply->result();
                    if (result.isValid() && result.values().size() >= 1) {
                        reportReadSuccess();
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
                    } else {
                        reportReadFailure(captured.tagName, QStringLiteral("响应无效（寄存器数据为空）"));
                    }
                } else {
                    reportReadFailure(captured.tagName, QStringLiteral("错误 %1").arg(int(reply->error())));
                }
                reply->deleteLater();
            });
        } else {
            clearInFlight(tagName);
            // 立即完成：非 NoError 才算失败（Qt 同步错误路径）
            if (reply->error() != QModbusDevice::NoError)
                reportReadFailure(tagName, QStringLiteral("错误 %1").arg(int(reply->error())));
            reply->deleteLater();   // 立即完成（错误）直接释放
        }
    } else {
        clearInFlight(tagName);
        reportReadFailure(tagName, QStringLiteral("请求入队失败（未连接/参数非法）"));
    }
}

void ModbusDriverBase::clearInFlight(const QString& tagName)
{
    for (auto& t : m_tags)
        if (t.tagName == tagName) { t.inFlight = false; break; }
}

/// 🟡-3（AB-2 审查）：读失败不再静默——5s 降频告警 + 累计计数；恢复时汇总一条
void ModbusDriverBase::reportReadFailure(const QString& tagName, const QString& reason)
{
    ++m_readFailCount;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastReadFailMs >= 5000) {
        m_lastReadFailMs = nowMs;
        qWarning().noquote() << logTag() << ": 读" << tagName << "失败（" << reason << "）——累计"
                             << m_readFailCount << "次" << targetDescription();
    }
}

void ModbusDriverBase::reportReadSuccess()
{
    if (m_readFailCount > 0) {
        qInfo().noquote() << logTag() << ": 读取恢复（此前累计失败" << m_readFailCount << "次）"
                          << targetDescription();
        m_readFailCount = 0;
    }
}

QVariant ModbusDriverBase::decodeValue(const ModbusTag& tag, quint16 raw) const
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

bool ModbusDriverBase::writeValue(const QString& tagName, const QVariant& value)
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
                    qWarning().noquote() << logTag() << ": 写" << tagName << "Float 值越界/非法（" << d
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
                    qWarning().noquote() << logTag() << ": 写" << tagName << "整型值越界（" << v
                                         << "）——拒绝写入";
                    return false;
                }
                raw = quint16(v & 0xFFFF);
                break;
            }
            }
            unit.setValue(0, raw);
            // Y-7：写回复错误检查（原 sendWriteRequest 丢弃 reply——写失败静默；补 reply 错误日志——
            // 写失败事件驱动频率有限，逐条记录不设降频）
            const int slave = tag.slave;
            const quint16 reg = tag.reg;
            const QString tName = tagName;
            if (auto* reply = m_client->sendWriteRequest(unit, tag.slave)) {
                if (reply->isFinished()) {
                    if (reply->error() != QModbusDevice::NoError)
                        qWarning().noquote() << logTag() << ": 写" << tName
                                             << "slave=" << slave << "reg=" << reg << "失败（错误"
                                             << int(reply->error()) << "）";
                    reply->deleteLater();
                } else {
                    connect(reply, &QModbusReply::finished, this, [this, reply, tName, slave, reg]() {
                        if (reply->error() != QModbusDevice::NoError)
                            qWarning().noquote() << logTag() << ": 写" << tName
                                                 << "slave=" << slave << "reg=" << reg << "失败（错误"
                                                 << int(reply->error()) << "）";
                        reply->deleteLater();
                    });
                }
            }
            qInfo().noquote() << logTag() << ": 写" << tagName
                              << "slave=" << tag.slave << "reg=" << tag.reg << "val=" << value << "raw=" << raw;
            return true;
        }
    }
    return false;
}

} // namespace navihmi
