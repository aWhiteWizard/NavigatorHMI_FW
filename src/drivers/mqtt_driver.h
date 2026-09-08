/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\mqtt_driver.h
 * @Description: MQTT 采集驱动（Y-4 2026-09-10 ④通信批——qtmqtt QMqttClient，挂 V-1 插件框架）
 *               数据流（订阅）：broker 消息 → 订阅 topic 组匹配 → JSON 解析（按 MqttBinding 字段名）
 *                               → valueRead(tagName, 值) → Acquisition deadband + DataManager.setValue
 *               数据流（发布）：writeValue(tagName, value)（DataManager.valueChanged → handleValueWritten 转发）
 *                               → 找发布绑定 → 组 JSON → publish；poll 周期发布（含初始全量）
 *               连接参数：tag.deviceName → DeviceConfig MQTT connection_info JSON（Acquisition connInfoForDevice 展开）
 *               订阅/发布映射：MqttSettings（Acquisition 经 DriverTagInfo.mqtt 注入）
 *               断线重连：1s→60s 退避（失败次数指数）+ 启动失败 3s 重试 + 连续 20 次报警
 */
#pragma once

#include <QList>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <QByteArray>
#include <QJsonValue>
#include "drivers/driver_iface.h"
#include "runtime/projectmodel.h"   // MqttSettings/MqttTopicDirection/MqttJsonTemplate（Y-4）

class QMqttClient;

namespace navihmi {

/// MQTT 驱动实现（协议层：QMqttClient 连接 + 订阅解析/发布组包；deadband 归 Manager）
class MqttDriver : public IDriver
{
    Q_OBJECT
public:
    explicit MqttDriver(QObject* parent = nullptr);
    ~MqttDriver() override;

    QString protocol() const override { return QStringLiteral("mqtt"); }
    bool configure(const QList<DriverTagInfo>& tags) override;
    void start() override;
    void stop() override;
    void poll(qint64 nowMs) override;
    bool writeValue(const QString& tagName, const QVariant& value) override;

private:
    /// 连接参数（首个有效 mqtt tag 的 conn——Acquisition 已展开 deviceName → DeviceConfig JSON）
    struct ConnParams {
        QString host;        // broker 主机/IP
        int port = 1883;
        QString clientId;    // 空 = 自动生成
        QString username;
        QString password;    // 加密包（FW 端解密——本轮匿名空）
        int keepAliveSec = 60;
    };

    /// 发布任务项（发布 topic + 周期 + 绑定 tag/字段）
    struct PublishJob {
        QString topicName;      // MqttSettings topic 配置名
        QString topic;          // 实际 topic 路径
        int qos = 0;
        bool retain = false;
        int intervalMs = 0;     // 周期（0 = 仅按需）
        MqttJsonTemplate jsonTemplate = MqttJsonTemplate::Kv;
        QStringList tagNames;   // 绑定的数据源变量（字段名 = Binding.fieldName 平行）
        QStringList fieldNames;
        qint64 lastPublishMs = 0;
        bool publishedInitial = false;
    };

    /// 订阅任务项（订阅 topic + 其下绑定 tag/字段——消息到达按字段解析写 tag）
    struct SubscribeJob {
        QString topicName;
        QString topic;          // 含通配符的订阅 topic
        int qos = 0;
        QStringList tagNames;   // 该 topic 下绑定（订阅方向 = 写入目标）
        QStringList fieldNames;
    };

    void ensureConnected();
    void ParseAndDispatch(const QByteArray& message, const QString& sourceTopic);
    QVariant JsonValueToTagType(const QJsonValue& v, int tagDataType, bool* ok) const;
    QByteArray buildPublishPayload(const PublishJob& job, const QHash<QString, QVariant>& tagValues) const;
    void publishJob(PublishJob& job);
    QString nextClientId() const;

    ConnParams m_conn;
    QList<DriverTagInfo> m_allTags;      // configure 全量（供订阅解析查 tag 类型）
    QHash<QString, QVariant> m_lastValues;   // 发布现值缓存（writeValue 回流——周期/按需发布数据源）
    QList<PublishJob> m_publishJobs;
    QList<SubscribeJob> m_subscribeJobs;
    const MqttSettings* m_mqtt = nullptr;
    QMqttClient* m_client = nullptr;
    bool m_connecting = false;
    int m_failCount = 0;                 // 连续失败（20 次报警阈值）
    qint64 m_nextRetryMs = 0;            // 下次重连时刻（1s→60s 退避）
    qint64 m_lastFailWarnMs = 0;         // 报警降频（20 次阈值内的日志节流）
    bool m_connected = false;
};

/// MQTT 驱动工厂 + extern C 注册入口（宏裁剪: NAVIHMI_HAVE_MQTT_DRIVER 关时不注册）
class MqttDriverFactory : public IDriverFactory
{
public:
    QString protocol() const override { return QStringLiteral("mqtt"); }
    IDriver* create(QObject* parent) override { return new MqttDriver(parent); }
};

} // namespace navihmi

#ifdef NAVIHMI_HAVE_MQTT_DRIVER
extern "C" void init_driver_mqtt();
#endif
