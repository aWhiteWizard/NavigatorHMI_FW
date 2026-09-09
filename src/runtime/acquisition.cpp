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
    m_project = proj;
    m_lastVal.clear();

    // 重建驱动（工程重载/切工程：旧驱动 stop 删除——多协议全清）
    destroyDrivers();
    if (m_timer) {
        m_timer->stop();
    }

    // tag 解析分组：按协议分桶（modbus:// → modbus_tcp；mqtt:// → mqtt；空 = 内部变量跳过）
    QHash<QString, QList<DriverTagInfo>> byProtocol;
    for (const auto& tag : m_project.tags) {
        const QString proto = protocolForSource(tag.source);
        if (proto.isEmpty())
            continue;   // 内部变量/未知来源不采集
        DriverTagInfo info;
        info.name = tag.name;
        info.source = tag.source;
        // Y Check 裁决（2026-09-11）：MQTT 连接参数真源 = MqttSettings.deviceName（选定设备）——
        // 非空时所有 mqtt tag 统一用选定设备展开连接（工程级单 MQTT 连接语义；旧工程 deviceName 空回退 tag.deviceName）
        if (proto == QLatin1String("mqtt") && !m_project.mqtt.deviceName.isEmpty())
            info.deviceName = m_project.mqtt.deviceName;
        else
            info.deviceName = tag.deviceName;
        info.dataType = int(tag.dataType);
        info.scanMs = tag.scanIntervalMs;
        info.deadband = tag.deadband;
        info.conn = connInfoForDevice(info.deviceName);
        info.mqtt = &m_project.mqtt;   // Y-4: MQTT 三层映射注入（MqttDriver configure 消费）
        byProtocol[proto].append(info);
    }
    if (byProtocol.isEmpty()) {
        qInfo().noquote() << "Acquisition: 无采集来源变量, 采集未启动";
        return;
    }

    // 按协议建驱动（注册表路由；未注册/被裁剪协议 → 跳过并告警）
    for (auto it = byProtocol.constBegin(); it != byProtocol.constEnd(); ++it) {
        const QString proto = it.key();
        auto* driver = createDriverForProtocol(proto, this);
        if (!driver) {
            qWarning().noquote() << "Acquisition: 无可用驱动" << proto << "（未注册/被裁剪）——" << it.value().size() << "个变量跳过";
            continue;
        }
        connect(driver, &IDriver::valueRead, this, &Acquisition::onDriverValueRead);
        connect(driver, &IDriver::connectionError, this,
                [](const QString& msg) { qWarning().noquote() << "Acquisition:" << msg; });
        if (!driver->configure(it.value())) {
            qInfo().noquote() << "Acquisition: 驱动配置无有效采集项" << proto;
            delete driver;
            continue;
        }
        driver->start();
        m_drivers.append(driver);
        qInfo().noquote() << "Acquisition: 驱动启动 protocol=" << proto
                          << "tags=" << it.value().size();
    }
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
    // 写通道: DataManager 写采集来源变量 → 路由到对应协议驱动（Y-4 多协议分发——
    // 按 tag.source 前缀找驱动；main.cpp 连接 valueChanged 调用）
    for (const auto& tag : m_project.tags) {
        if (tag.name != tagName)
            continue;
        const QString proto = protocolForSource(tag.source);
        if (proto.isEmpty())
            return;   // 内部变量——无驱动写
        for (auto* d : m_drivers)
            if (d->protocol() == proto) { d->writeValue(tagName, value); return; }
        return;
    }
}

} // namespace navihmi
