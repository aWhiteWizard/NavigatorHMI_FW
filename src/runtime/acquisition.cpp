/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.cpp
 * @Description: 数据采集管理实现（V-1 2026-09-06 Manager 化；Y-4 2026-09-10 多协议列表化——
 *               modbus_tcp + mqtt 并存，tag 按 source 前缀分发各驱动；MQTT 事件驱动 + Modbus 轮询并存）
 */
#include "runtime/acquisition.h"
#include "runtime/datamanager.h"
#include "drivers/driver_iface.h"
#include "drivers/driver_registry.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

namespace navihmi {

Acquisition::Acquisition(QObject* parent)
    : QObject(parent)
{
    registerBuiltinDrivers();   // 幂等：注册内置驱动工厂（宏裁剪的驱动此处不注册）
}

Acquisition::~Acquisition()
{
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
    destroyDrivers();
}

void Acquisition::destroyDrivers()
{
    for (auto* d : m_drivers) {
        d->stop();
        delete d;   // IDriver QObject——stop/清理由析构
    }
    m_drivers.clear();
    m_driverConns.clear();   // Z 循环：driver↔连接映射同步清
}

void Acquisition::stop()
{
    // V-4 F11：主壳 Shutdown 显式停止（停调度 timer + 驱动断连；不 delete——析构兜底）
    if (m_timer)
        m_timer->stop();
    for (auto* d : m_drivers)
        d->stop();
}

QString Acquisition::protocolForSource(const QString& source) const
{
    if (source.startsWith(QStringLiteral("modbus://"))) return QStringLiteral("modbus_tcp");
    if (source.startsWith(QStringLiteral("mqtt://"))) return QStringLiteral("mqtt");
    return QString();
}

void Acquisition::setProject(const Project& proj)
{
    // 🟡1（reviewer Z-5）：先销毁旧驱动再替换 m_project——驱动 m_cfg/connCfg 指向旧 Project.connections，
    // 若先 m_project = proj（旧 connections 析构）则 stop()→disconnected→emitConnState→lambda 读悬垂 cfg
    destroyDrivers();
    if (m_timer) {
        m_timer->stop();
    }
    m_statusTagWarned.clear();
    m_project = proj;
    m_lastVal.clear();

    // tag 解析分组：
    //  - modbus：按协议桶（modbus:// → modbus_tcp——连接参数 deviceName → DeviceConfig JSON）
    //  - mqtt：Z 循环多连接——每 MqttConnectionConfig 一驱动；tag 归属 = 出现在该连接 bindings 的 mqtt:// tag
    QHash<QString, QList<DriverTagInfo>> byProtocol;
    QList<QPair<const MqttConnectionConfig*, QList<DriverTagInfo>>> mqttGroups;
    QList<DriverTagInfo> mqttTagInfos;   // 工程全部 mqtt:// tag（先收集再按连接归属）
    for (const auto& tag : m_project.tags) {
        const QString proto = protocolForSource(tag.source);
        if (proto.isEmpty())
            continue;   // 内部变量/未知来源不采集
        DriverTagInfo info;
        info.name = tag.name;
        info.source = tag.source;
        info.deviceName = tag.deviceName;
        info.dataType = int(tag.dataType);
        info.scanMs = tag.scanIntervalMs;
        info.deadband = tag.deadband;
        if (proto == QLatin1String("mqtt")) {
            mqttTagInfos.append(info);   // 连接归属由 bindings 决定（Z 循环——tag 不加归属字段）
        } else {
            info.conn = connInfoForDevice(info.deviceName);
            byProtocol[proto].append(info);
        }
    }
    // Z 循环：mqtt 按连接分组（连接归属——Binding 挂哪棵 Topic 树即属哪个连接；enableMqtt 关 → 不建任何连接对象）
    const bool mqttEnabled = m_project.mqtt.hasMqttSettings && m_project.mqtt.enableMqtt;
    if (mqttEnabled) {
        for (const auto& conn : m_project.mqtt.connections) {
            QList<DriverTagInfo> connTags;
            for (const auto& info : mqttTagInfos) {
                bool bound = false;
                for (const auto& b : conn.bindings)
                    if (b.tagName == info.name) { bound = true; break; }
                if (!bound) continue;
                // 🔴1（reviewer Z-5）：归属连接时注入 mqttConn（mqtt_driver configure 唯一消费端——
                // 不注入则 m_cfg 恒 nullptr → 驱动不建，订阅/发布/StatusTag 全链路失效）
                DriverTagInfo ti = info;
                ti.mqttConn = &conn;
                connTags.append(ti);
            }
            if (connTags.isEmpty()) {
                qInfo().noquote() << "Acquisition: MQTT 连接" << conn.name << "无绑定变量（映射未建），跳过";
                continue;
            }
            mqttGroups.append(qMakePair(&conn, connTags));
        }
    }
    if (byProtocol.isEmpty() && mqttGroups.isEmpty()) {
        qInfo().noquote() << "Acquisition: 无采集来源变量, 采集未启动";
        return;
    }

    // 建驱动：modbus 按协议桶（同 Y 语义）→ mqtt 每连接一实例（一协议多驱动——Z 循环）
    auto startDriver = [&](const QString& proto, const QList<DriverTagInfo>& tags,
                           const MqttConnectionConfig* connCfg) -> IDriver* {
        auto* driver = createDriverForProtocol(proto, this);
        if (!driver) {
            qWarning().noquote() << "Acquisition: 无可用驱动" << proto << "（未注册/被裁剪）——"
                                 << tags.size() << "个变量跳过";
            return nullptr;
        }
        connect(driver, &IDriver::valueRead, this, &Acquisition::onDriverValueRead);
        connect(driver, &IDriver::connectionError, this,
                [](const QString& msg) { qWarning().noquote() << "Acquisition:" << msg; });
        // Z 循环：MQTT 4 态连接状态 → 本连接 statusTag 回写（0-3——proto MqttConnectionState；modbus 不 emit 此信号）
        if (connCfg) {
            connect(driver, &IDriver::connectionStateChanged, this,
                    [this, driver](int state) {
                // 🟡3（reviewer Z-5）：经 m_driverConns 查本连接 cfg（成员有消费点——driver↔连接映射）
                auto it = m_driverConns.constFind(driver);
                if (it == m_driverConns.constEnd())
                    return;
                const QString st = (*it)->statusTag;
                if (st.isEmpty() || !m_dataManager)
                    return;
                // 🟡6（reviewer Z-5）：statusTag 变量须工程已声明（DataManager::setValue 对未声明 tag 静默 return）——
                // 首现告警防静默失效（QSet 去重：每 tag 只告警一次）
                bool declared = false;
                for (const auto& t : m_project.tags)
                    if (t.name == st) { declared = true; break; }
                if (!declared) {
                    if (!m_statusTagWarned.contains(st)) {
                        m_statusTagWarned.insert(st);
                        qWarning().noquote() << "Acquisition: MQTT 连接" << (*it)->name
                                             << "的 StatusTag \"" << st << "\" 未在工程声明——状态不回写";
                    }
                    return;
                }
                m_dataManager->setValue(st, state);
            });
        }
        if (!driver->configure(tags)) {
            qInfo().noquote() << "Acquisition: 驱动配置无有效采集项" << proto;
            delete driver;
            return nullptr;
        }
        m_drivers.append(driver);
        if (connCfg) m_driverConns.insert(driver, connCfg);   // 🟡复审：先登记再 start——首连 Connecting(1) 状态经 map 可写
        driver->start();
        qInfo().noquote() << "Acquisition: 驱动启动 protocol=" << proto
                          << "conn=" << (connCfg ? connCfg->name : QStringLiteral("-"))
                          << "tags=" << tags.size();
        return driver;
    };
    for (auto it = byProtocol.constBegin(); it != byProtocol.constEnd(); ++it)
        startDriver(it.key(), it.value(), nullptr);
    for (const auto& g : mqttGroups)
        startDriver(QStringLiteral("mqtt"), g.second, g.first);
    if (m_drivers.isEmpty()) {
        qInfo().noquote() << "Acquisition: 无驱动成功启动（协议均不可用）";
        return;
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(100);   // 100ms 调度（过程数据 ≤100ms 目标，原语义保留）
        connect(m_timer, &QTimer::timeout, this, &Acquisition::tick);
    }
    m_timer->start();
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

void Acquisition::tick()
{
    if (!m_dataManager || m_drivers.isEmpty())
        return;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto* d : m_drivers)
        d->poll(nowMs);
}

void Acquisition::onDriverValueRead(const QString& tagName, const QVariant& value, double deadband)
{
    if (!m_dataManager)
        return;
    // deadband 防抖（跨协议统一——原 Acquisition readTag 内判断上移 Manager）
    const auto it = m_lastVal.constFind(tagName);
    const bool haveLast = it != m_lastVal.constEnd();
    const bool write = deadband <= 0 || !haveLast
                       || qAbs(value.toDouble() - it.value().toDouble()) >= deadband;
    if (write) {
        m_lastVal.insert(tagName, value);
        m_dataManager->setValue(tagName, value);
    }
}

void Acquisition::handleValueWritten(const QString& tagName, const QVariant& value)
{
    // 写通道: DataManager 写采集来源变量 → 路由到对应驱动（Y-4 多协议分发 + Z 循环多连接——
    // mqtt 广播到全部 MQTT 驱动（每驱动自行判断本连接发布绑定——tag 无发布映射则忽略，防跨连接误发）；
    // modbus 首协议匹配（单实例）；main.cpp 连接 valueChanged 调用）
    for (const auto& tag : m_project.tags) {
        if (tag.name != tagName)
            continue;
        const QString proto = protocolForSource(tag.source);
        if (proto.isEmpty())
            return;   // 内部变量——无驱动写
        if (proto == QLatin1String("mqtt")) {
            // Z 循环：tag 可被多个连接绑定（不同 broker 各发各的 topic）——广播全部 mqtt 驱动
            for (auto* d : m_drivers)
                if (d->protocol() == QLatin1String("mqtt"))
                    d->writeValue(tagName, value);
            return;
        }
        for (auto* d : m_drivers)
            if (d->protocol() == proto) { d->writeValue(tagName, value); return; }
        return;
    }
}

} // namespace navihmi
