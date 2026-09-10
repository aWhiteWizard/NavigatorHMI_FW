/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\mqtt_driver.cpp
 * @Description: MQTT 采集驱动实现（Y-4 2026-09-10——qtmqtt QMqttClient）
 *               安全/纪律：日志脱敏（密码/私钥不落日志）；连接参数 tag 变化 debounce 重连（V1.2 动态参数，
 *               本轮固定连接——重连 = 断线退避）；发布 JSON 单点组包（kv/timestamp 两模板）
 */
#include "drivers/mqtt_driver.h"
#include "runtime/projectmodel.h"

#include <QMqttClient>
#include <QMqttTopicName>
#include <QMqttTopicFilter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <QDateTime>
#include <QRandomGenerator>

namespace navihmi {

namespace {
/// MQTT 连续失败报警阈值（执行书 Y-4：20 次）。
constexpr int kFailWarnThreshold = 20;
/// 断线重连最小/最大退避（1s → 60s）。
constexpr qint64 kMinRetryMs = 1000;
constexpr qint64 kMaxRetryMs = 60000;
/// 匿名默认 clientId 前缀（无设备 clientId 时生成——防 broker 冲突）。
constexpr char kDefaultClientPrefix[] = "navihmi-";

/// MQTT topic 通配符匹配（订阅 filter 含 +/#；实际 topic 逐级匹配——MQTT 3.1.1 §4.7.1）。
bool topicMatches(const QString& filter, const QString& actual)
{
    const QStringList f = filter.split(QLatin1Char('/'));
    const QStringList a = actual.split(QLatin1Char('/'));
    for (int i = 0; i < f.size(); ++i) {
        if (f[i] == QLatin1String("#")) return true;      // 多级通配（剩余全匹配）
        if (i >= a.size()) return false;                  // filter 比实际长
        if (f[i] == QLatin1String("+")) continue;         // 单级通配
        if (f[i] != a[i]) return false;
    }
    return f.size() == a.size();
}
} // namespace

MqttDriver::MqttDriver(QObject* parent)
    : IDriver(parent)
{
}

MqttDriver::~MqttDriver()
{
    if (m_client) {
        m_client->disconnectFromHost();
        delete m_client;
        m_client = nullptr;
    }
}

bool MqttDriver::configure(const QList<DriverTagInfo>& tags)
{
    m_allTags = tags;
    m_publishJobs.clear();
    m_subscribeJobs.clear();
    m_conn = {};
    m_cfg = nullptr;
    m_failCount = 0;
    m_nextRetryMs = 0;

    for (const auto& tag : tags) {
        if (!tag.source.startsWith(QStringLiteral("mqtt://")))
            continue;   // Manager 已按连接分组，双保险过滤
        // Z 循环：连接参数内联本连接 cfg（Acquisition 按连接注入 mqttConn——弃 Y 时代 DeviceConfig JSON 展开）
        m_cfg = tag.mqttConn;
        if (m_cfg) {
            m_conn.host = m_cfg->broker;
            m_conn.port = m_cfg->port > 0 ? m_cfg->port : 1883;
            m_conn.clientId = m_cfg->clientId;
            m_conn.username = m_cfg->username;
            m_conn.password = m_cfg->password;   // nhfw1: 加密包——本轮匿名空（有密码解密 V1.2/凭据批）；日志绝不打印
            m_conn.keepAliveSec = m_cfg->keepAliveSec >= 0 ? m_cfg->keepAliveSec : 60;
            // 🟡4（reviewer Z-5）：version=1(MQTT5)/enableTls=true 非默认值 FW 未实现——显式告警防静默降级错连
            // （本轮按 3.1.1/无 TLS 连接——V1.2 实现前 PC 侧不放开）
            if (m_cfg->version != 0)
                qWarning().noquote() << "MqttDriver: 连接" << m_cfg->name << "version=" << m_cfg->version
                                     << "（5.0）FW 本轮不支持——按 3.1.1 连接（V1.2 实现）";
            if (m_cfg->enableTls)
                qWarning().noquote() << "MqttDriver: 连接" << m_cfg->name << "enableTls=true 本轮不支持——按无 TLS 连接（V1.2 实现）";
        }
        break;   // 首 tag 即带本连接 cfg（每驱动实例只属一个连接——Acquisition 分组保证）
    }
    if (!m_cfg) {
        qInfo().noquote() << "MqttDriver: 无本连接配置（mqttConn 未注入），不启动";
        return false;
    }
    // 连接参数兜底（模拟/测试环境——环境变量覆盖）
    if (m_conn.host.isEmpty()) {
        m_conn.host = qEnvironmentVariable("NAVIHMI_MQTT_HOST", "127.0.0.1");
        bool okP = false;
        int p = qEnvironmentVariable("NAVIHMI_MQTT_PORT", "1883").toInt(&okP);
        m_conn.port = (okP && p > 0) ? p : 1883;
    }

    // 按本连接 cfg 建订阅/发布任务（Bindings 引用本连接 TopicName 归组——连接归属：Binding 挂哪棵 Topic 树即属该连接）
    QHash<QString, QList<const MqttBindingConfig*>> bindingsByTopic;
    for (const auto& b : m_cfg->bindings)
        bindingsByTopic[b.topicName].append(&b);
    for (const auto& tc : m_cfg->topics) {
        const auto bs = bindingsByTopic.value(tc.name);
        if (tc.direction == MqttTopicDirection::Subscribe) {
            SubscribeJob j;
            j.topicName = tc.name;
            j.topic = tc.topic;
            j.qos = tc.qos;
            for (const auto* b : bs) { j.tagNames.append(b->tagName); j.fieldNames.append(b->fieldName); }
            if (j.tagNames.isEmpty())
                qInfo().noquote() << "MqttDriver: 订阅 topic" << tc.name << "无绑定变量（只收不写?）";
            m_subscribeJobs.append(j);
        } else {
            PublishJob j;
            j.topicName = tc.name;
            j.topic = tc.topic;
            j.qos = tc.qos;
            j.retain = tc.retain;
            j.intervalMs = tc.publishIntervalMs;
            j.jsonTemplate = tc.jsonTemplate;
            for (const auto* b : bs) { j.tagNames.append(b->tagName); j.fieldNames.append(b->fieldName); }
            m_publishJobs.append(j);
        }
    }
    if (m_subscribeJobs.isEmpty() && m_publishJobs.isEmpty()) {
        qInfo().noquote() << "MqttDriver: 连接" << m_cfg->name << "无 Topic/Binding（映射未建），不启动";
        return false;
    }
    qInfo().noquote() << "MqttDriver: 配置就绪 连接=" << m_cfg->name << "host=" << m_conn.host
                      << "port=" << m_conn.port << "sub=" << m_subscribeJobs.size() << "pub=" << m_publishJobs.size();
    return true;
}

void MqttDriver::start()
{
    ensureConnected();
}

void MqttDriver::stop()
{
    if (m_client) {
        m_client->disconnectFromHost();
        m_connecting = false;
        m_connected = false;
    }
    // Y-4 reviewer 🟡12：stop 重置失败计数/退避（stop→start 同实例路径防御——失败历史不抑制新启动首连）
    m_failCount = 0;
    m_nextRetryMs = 0;
}

void MqttDriver::ensureConnected()
{
    if (!m_cfg)
        return;   // Z 循环：configure 未注入本连接配置（Acquisition 只在 enableMqtt 时建驱动——此处防御）
    if (!m_client) {
        m_client = new QMqttClient(this);
        connect(m_client, &QMqttClient::connected, this, [this]() {
            m_connected = true;
            m_failCount = 0;
            m_nextRetryMs = 0;
            qInfo().noquote() << "MqttDriver: 已连接" << m_cfg->name << m_conn.host << m_conn.port;
            emit stateChanged(true);
            emitConnState(int(MqttConnState::Connected));   // Z：StatusTag 4 态回写（2=已连接）
            // 连接成功 → 订阅全部订阅 topic（含通配符）
            for (const auto& j : m_subscribeJobs) {
                auto* sub = m_client->subscribe(QMqttTopicFilter(j.topic), quint8(j.qos));
                qInfo().noquote() << "MqttDriver: 订阅" << j.topic << "qos=" << j.qos
                                  << "sub=" << (sub ? "ok" : "null");
                // Y-8 诊断：订阅状态变化（Subscribed/Unsubscribed/Error——SUBACK 确认与否）
                if (sub) {
                    connect(sub, &QMqttSubscription::stateChanged, this,
                            [this, j](QMqttSubscription::SubscriptionState st) {
                        qInfo().noquote() << "MqttDriver: 订阅状态" << j.topic << "→" << int(st);
                    });
                }
            }
            // 周期发布：初始全量（启动即发一次）
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            for (auto& j : m_publishJobs) {
                j.lastPublishMs = now;
                if (j.intervalMs > 0) publishJob(j);
            }
        });
        connect(m_client, &QMqttClient::disconnected, this, [this]() {
            m_connected = false;
            m_connecting = false;
            qInfo().noquote() << "MqttDriver: 已断开（将退避重连）";
            emit stateChanged(false);
            emitConnState(int(MqttConnState::Disconnected));   // Z：4 态回写（0=未连接）
            // Y-4 reviewer 🟡3：断线不计失败计数——失败统一走 errorChanged（防同一次失败双计数，20 次报警按真实失败推进）
        });
        connect(m_client, &QMqttClient::messageReceived, this,
                [this](const QByteArray& message, const QMqttTopicName& topic) {
            ParseAndDispatch(message, topic.name());
        });
        // 连接失败（errorChanged + 未连接）——失败计数/退避/报警（reviewer 🟡4：退避统一 1s×2^n 上限 60s）
        connect(m_client, &QMqttClient::errorChanged, this,
                [this](QMqttClient::ClientError e) {
            if (m_connected || e == QMqttClient::NoError) return;
            m_failCount++;
            m_connecting = false;   // 🟡7（reviewer Z-5）：失败须清 connecting——若 broker 只发 errorChanged 不发 disconnected，
                                    // 不清则 m_connecting 卡 true → 永不重试（Y 遗留，4 态后影响面变大）
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            m_nextRetryMs = now + qMin(kMaxRetryMs, kMinRetryMs * (1LL << qMin(6, m_failCount)));
            emitConnState(int(MqttConnState::Error));   // Z：4 态回写（3=错误/连接失败退避中）
            // 降频日志（防高频刷屏）；连续 20 次报警（防静默失败——执行书 Y-4）
            if (m_failCount % 5 == 1 || m_failCount == kFailWarnThreshold) {
                qWarning().noquote() << "MqttDriver: 连接失败（连续" << m_failCount << "次，错误"
                                     << int(e) << "）——" << (m_failCount >= kFailWarnThreshold ? "已达报警阈值" : "退避重连中");
                if (m_failCount >= kFailWarnThreshold)
                    emit connectionError(QStringLiteral("MQTT 连续 %1 次连接失败（连接: %2 host: %3）")
                                             .arg(m_failCount).arg(m_cfg ? m_cfg->name : QString()).arg(m_conn.host));
            }
        });
        m_client->setHostname(m_conn.host);
        m_client->setPort(quint16(m_conn.port));
        if (!m_conn.clientId.isEmpty()) m_client->setClientId(m_conn.clientId);
        else m_client->setClientId(nextClientId());
        if (!m_conn.username.isEmpty()) {
            m_client->setUsername(m_conn.username);
            // 密码：加密包——FW 解密后设置（本轮匿名空；有密码场景 V1.2/凭据批解密——绝不明文日志）
            if (!m_conn.password.isEmpty())
                qWarning().noquote() << "MqttDriver: 密码为加密包，本轮匿名联调不消费（解密待凭据批）——已忽略";
        }
        m_client->setKeepAlive(quint16(m_conn.keepAliveSec));
    }
    if (m_client->state() == QMqttClient::Connected || m_connecting)
        return;
    // 退避门控
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < m_nextRetryMs) return;
    m_connecting = true;
    emitConnState(int(MqttConnState::Connecting));   // Z：4 态回写（1=连接中——发起连接尝试）
    m_client->connectToHost();
}

void MqttDriver::emitConnState(int state)
{
    if (state == m_lastState)
        return;   // 状态未变不重复广播（StatusTag 少写）
    m_lastState = state;
    emit connectionStateChanged(state);
}

void MqttDriver::ParseAndDispatch(const QByteArray& message, const QString& sourceTopic)
{
    if (m_subscribeJobs.isEmpty()) return;
    // Y-8 联调诊断：收到消息即记录（成功路径原静默——联调期留 info 便于确认订阅链路；正式可降 debug）
    qInfo().noquote() << "MqttDriver: 收到消息 topic=" << sourceTopic << "len=" << message.size();
    // 匹配订阅 job（通配符 filter——可能多 filter 命中同一实际 topic，全部处理 reviewer 🟡7）
    QList<const SubscribeJob*> matched;
    for (const auto& j : m_subscribeJobs)
        if (topicMatches(j.topic, sourceTopic)) matched.append(&j);
    if (matched.isEmpty()) return;
    // 解析 JSON（对象根）
    const QJsonDocument doc = QJsonDocument::fromJson(message);
    if (!doc.isObject()) {
        qWarning().noquote() << "MqttDriver: 订阅" << sourceTopic << "消息非 JSON 对象（长度" << message.size() << "）";
        return;
    }
    const QJsonObject obj = doc.object();
    // 每个匹配 job 按自身绑定字段解析（tagName 平行 fieldNames）
    for (const auto* job : matched) {
        for (int i = 0; i < job->fieldNames.size(); ++i) {
            const QString field = job->fieldNames.at(i);
            const QJsonValue v = obj.value(field);
            if (v.isUndefined() || v.isNull()) continue;   // 字段缺失/空 → 跳过（不写旧值）
            const DriverTagInfo* tagDef = nullptr;
            for (const auto& t : m_allTags)
                if (t.name == job->tagNames.at(i)) { tagDef = &t; break; }
            if (!tagDef) continue;
            bool ok = false;
            const QVariant tv = JsonValueToTagType(v, tagDef->dataType, &ok);
            if (!ok) {
                qWarning().noquote() << "MqttDriver: 字段" << field << "值转数据类型" << tagDef->dataType << "失败";
                continue;
            }
            emit valueRead(tagDef->name, tv, tagDef->deadband);   // deadband 归 Manager 判断
        }
    }
}

QVariant MqttDriver::JsonValueToTagType(const QJsonValue& v, int tagDataType, bool* ok) const
{
    *ok = true;
    switch (tagDataType) {
    case int(TagDataType::Bool):
        if (v.isBool()) return v.toBool();
        if (v.isDouble()) return v.toDouble() != 0.0;
        if (v.isString()) {
            const QString s = v.toString();
            if (s == "true" || s == "1") return true;
            if (s == "false" || s == "0") return false;
        }
        break;
    case int(TagDataType::Int16): case int(TagDataType::Uint16):
    case int(TagDataType::Int32):
        if (v.isDouble()) return int(v.toDouble());
        if (v.isString()) { bool b = false; int n = v.toString().toInt(&b); if (b) return n; }
        break;
    case int(TagDataType::Float):
        if (v.isDouble()) return v.toDouble();
        if (v.isString()) { bool b = false; double d = v.toString().toDouble(&b); if (b) return d; }
        break;
    case int(TagDataType::String):
        if (v.isString()) return v.toString();
        if (v.isDouble()) return QString::number(v.toDouble(), 'g', 15);
        if (v.isBool()) return v.toBool() ? "true" : "false";
        break;
    default:
        break;   // DateTime/Gps 不支持 MQTT 直接写（V1.2）
    }
    *ok = false;
    return QVariant();
}

QByteArray MqttDriver::buildPublishPayload(const PublishJob& job, const QHash<QString, QVariant>& tagValues) const
{
    QJsonObject obj;
    for (int i = 0; i < job.tagNames.size(); ++i) {
        if (!tagValues.contains(job.tagNames.at(i))) continue;
        const QVariant val = tagValues.value(job.tagNames.at(i));
        const QJsonValue jv = val.typeId() == QMetaType::Bool ? QJsonValue(val.toBool())
            : (val.metaType().id() == QMetaType::Double || val.metaType().id() == QMetaType::Float ? QJsonValue(val.toDouble())
            : (val.metaType().id() == QMetaType::Int || val.metaType().id() == QMetaType::LongLong ? QJsonValue(val.toDouble())
            : QJsonValue(val.toString())));
        obj.insert(job.fieldNames.at(i), jv);
    }
    if (job.jsonTemplate == MqttJsonTemplate::KvWithTimestamp) {
        // 时间戳（ISO8601 本地）——模板枚举两端同步（proto MQTT_TPL_KV_TIMESTAMP=1）
        obj.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void MqttDriver::publishJob(PublishJob& job)
{
    if (!m_client || m_client->state() != QMqttClient::Connected) return;
    // 现值来自 m_lastValues（writeValue 回流缓存——按需发布为主；周期发布最近已知值）
    // 注意：周期发布全量需 DataManager 现值桥——本驱动不持 DataManager，按需（writeValue）为主，
    // 周期仅重发缓存（首连 initial 触发同此）——缺口记录（V1.2 DataManager 桥）
    if (job.tagNames.isEmpty()) return;
    bool anyCached = false;
    QHash<QString, QVariant> vals;
    for (const auto& tn : job.tagNames)
        if (m_lastValues.contains(tn)) { vals.insert(tn, m_lastValues.value(tn)); anyCached = true; }
    if (!anyCached) return;   // 无缓存值（启动即周期但从未写）——跳过本次
    const QByteArray payload = buildPublishPayload(job, vals);
    m_client->publish(QMqttTopicName(job.topic), payload, quint8(job.qos), job.retain);
}

void MqttDriver::poll(qint64 nowMs)
{
    // Y-4 reviewer 🔴1：每 100ms tick 驱动重连（对齐 Modbus ensureConnected 模式）——
    // 否则首连失败/断线后退避到期无触发源（断线重连整套死代码）
    if (!m_cfg)
        return;   // Z 循环：本连接配置（Acquisition 保证 enableMqtt 才建）
    ensureConnected();
    if (!m_client || m_client->state() != QMqttClient::Connected) return;
    // 周期发布（intervalMs > 0 且到期）
    for (auto& j : m_publishJobs) {
        if (j.intervalMs <= 0) continue;
        if (nowMs - j.lastPublishMs >= j.intervalMs) {
            j.lastPublishMs = nowMs;
            publishJob(j);
        }
    }
}

bool MqttDriver::writeValue(const QString& tagName, const QVariant& value)
{
    if (!m_client || m_client->state() != QMqttClient::Connected) return false;
    // 缓存现值（周期发布数据源）——DataManager.valueChanged → handleValueWritten → 本入口
    m_lastValues.insert(tagName, value);
    // 按需发布：找含该 tag 的发布 job（按需为主——tag 变化立即发）
    bool published = false;
    for (auto& j : m_publishJobs) {
        if (j.tagNames.contains(tagName)) {
            publishJob(j);
            published = true;
        }
    }
    return published;   // 该 tag 不在发布映射 → 非发布数据源（订阅写入回流不发布——防环）
}

QString MqttDriver::nextClientId() const
{
    return QString::fromLatin1(kDefaultClientPrefix)
        + QString::number(QRandomGenerator::global()->generate(), 16);
}

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MQTT_DRIVER
#include "drivers/driver_registry.h"
extern "C" void init_driver_mqtt()
{
    navihmi::registerDriverFactory(std::make_unique<navihmi::MqttDriverFactory>());
}
#endif
