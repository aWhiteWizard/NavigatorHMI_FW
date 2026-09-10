/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.cpp
 * @Description: 数据采集管理实现（V-1 2026-09-06 Manager 化；Y-4 2026-09-10 多协议列表化——
 *               modbus_tcp + mqtt 并存，tag 按 source 前缀分发各驱动；MQTT 事件驱动 + Modbus 轮询并存；
 *               AB-2 2026-09-11：Modbus 双传输——modbus:// 按 tag 所属设备协议路由 modbus_tcp/modbus_rtu）
 */
#include "runtime/acquisition.h"
#include "runtime/datamanager.h"
#include "drivers/driver_iface.h"
#include "drivers/driver_registry.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <cmath>   // std::isfinite（连接参数数值守卫）

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
    m_driverDevices.clear(); // AB-3：driver↔设备映射同步清
}

void Acquisition::stop()
{
    // V-4 F11：主壳 Shutdown 显式停止（停调度 timer + 驱动断连；不 delete——析构兜底）
    if (m_timer)
        m_timer->stop();
    for (auto* d : m_drivers)
        d->stop();
}

ProtocolType Acquisition::protocolOfDevice(const QString& deviceName) const
{
    if (!deviceName.isEmpty()) {
        for (const auto& dev : m_project.devices)
            if (dev.name == deviceName)
                return dev.protocol;
    }
    // 设备未知/未关联（含无 deviceName 的裸 modbus:// 变量）→ 兼容 Z/Y 行为：按 TCP 处理
    return ProtocolType::ModbusTcp;
}

QString Acquisition::protocolForTag(const Tag& tag) const
{
    if (tag.source.startsWith(QStringLiteral("mqtt://")))
        return QStringLiteral("mqtt");
    if (tag.source.startsWith(QStringLiteral("modbus://"))) {
        // AB-2（2026-09-11）：Modbus 不再一律 TCP——按 tag 所属设备的协议分流
        // （AB-3 起再按「协议 + 设备」分实例，支持一协议多从站）
        return protocolOfDevice(tag.deviceName) == ProtocolType::ModbusRtu
            ? QStringLiteral("modbus_rtu") : QStringLiteral("modbus_tcp");
    }
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
    m_modbusNoDeviceWarned = 0;
    m_project = proj;
    m_lastVal.clear();
    m_ringBack.clear();   // AB-5：工程重载清回环标记

    // tag 解析分组：
    //  - modbus：AB-3（2026-09-11）按「协议 + 设备」分桶——每设备一驱动实例（一协议多从站/多设备）；
    //            协议由设备 ProtocolType 决定（modbus_rtu / modbus_tcp），连接参数 deviceName → DeviceConfig JSON
    //  - mqtt：Z 循环多连接——每 MqttConnectionConfig 一驱动；tag 归属 = 出现在该连接 bindings 的 mqtt:// tag
    struct ModbusGroup {
        QString proto;
        QString device;              // 空 = 裸 modbus:// 变量（无关联设备——走驱动默认连接）
        QList<DriverTagInfo> tags;
    };
    QList<ModbusGroup> modbusGroups;
    QHash<QString, int> modbusGroupIndex;   // "proto\x1fdevice" → modbusGroups 下标
    QList<QPair<const MqttConnectionConfig*, QList<DriverTagInfo>>> mqttGroups;
    QList<DriverTagInfo> mqttTagInfos;   // 工程全部 mqtt:// tag（先收集再按连接归属）
    for (const auto& tag : m_project.tags) {
        const QString proto = protocolForTag(tag);
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
            if (info.deviceName.isEmpty()) {
                // AB-2/3 诊断：无归属设备的 modbus:// 变量 → 回落 modbus_tcp 默认连接（旧行为）。
                // 正确做法 = PC 侧给变量设置所属设备（Tag.device_name，proto 9）——RTU/TCP 与多设备
                // 连接参数都靠它定位；此处只告警一次防静默失效（reviewer 关注点：静默回落难排查）
                if (m_modbusNoDeviceWarned < 3) {
                    ++m_modbusNoDeviceWarned;
                    qWarning().noquote() << "Acquisition: 变量" << info.name
                                         << "未关联设备（Tag.device_name 为空）——按 Modbus TCP 默认连接处理；"
                                            "请在上位机变量表选择所属设备";
                }
            }
            const QString key = proto + QLatin1Char('\x1f') + tag.deviceName;
            int idx = modbusGroupIndex.value(key, -1);
            if (idx < 0) {
                idx = modbusGroups.size();
                modbusGroupIndex.insert(key, idx);
                ModbusGroup g;
                g.proto = proto;
                g.device = tag.deviceName;
                modbusGroups.append(g);
            }
            modbusGroups[idx].tags.append(info);
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
    if (modbusGroups.isEmpty() && mqttGroups.isEmpty()) {
        qInfo().noquote() << "Acquisition: 无采集来源变量, 采集未启动";
        return;
    }
    // AB-3 前置告警：同一串口被多个 RTU 设备占用（两个 QModbusRtuSerialClient 抢同一 tty 必然互扰）
    {
        QHash<QString, QString> rtuPortOwner;   // port → 设备名（首占者）
        for (const auto& g : modbusGroups) {
            if (g.proto != QLatin1String("modbus_rtu"))
                continue;
            const QString port = g.tags.isEmpty() ? QString() : g.tags.first().conn.value(QStringLiteral("port"));
            if (port.isEmpty())
                continue;
            auto it = rtuPortOwner.constFind(port);
            if (it == rtuPortOwner.constEnd())
                rtuPortOwner.insert(port, g.device);
            else
                qWarning().noquote() << "Acquisition: 串口" << port << "被多个 RTU 设备占用（"
                                     << *it << "与" << g.device << "）——同一串口应挂在同一设备下按从站号区分";
        }
    }

    // 建驱动：modbus 每（协议,设备）一实例（AB-3）→ mqtt 每连接一实例（一协议多驱动——Z 循环）
    auto startDriver = [&](const QString& proto, const QList<DriverTagInfo>& tags,
                           const MqttConnectionConfig* connCfg, const QString& device) -> IDriver* {
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
        if (connCfg) m_driverConns.insert(driver, connCfg);            // 🟡复审：先登记再 start——首连 Connecting(1) 状态经 map 可写
        if (!connCfg) m_driverDevices.insert(driver, device);          // AB-3：driver↔设备（modbus 写值定位）
        driver->start();
        qInfo().noquote() << "Acquisition: 驱动启动 protocol=" << proto
                          << "conn=" << (connCfg ? connCfg->name : (device.isEmpty() ? QStringLiteral("-") : device))
                          << "tags=" << tags.size();
        return driver;
    };
    for (const auto& g : modbusGroups)
        startDriver(g.proto, g.tags, nullptr, g.device);   // AB-3：每（协议,设备）一实例
    for (const auto& g : mqttGroups)
        startDriver(QStringLiteral("mqtt"), g.second, g.first, QString());
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
                for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
                    // 🔴AB-2 审查（2026-09-11）：**按 JSON 值类型取值**——原一律 toString()，
                    // 而 QJsonValue::toString() 对非 String（PC 侧 numeric port/baud）返回空串 →
                    // TCP 连到 127.0.0.1:0、RTU 波特率静默回落 9600（跨端契约取值缺陷，两端 Qt/C# 实证）
                    switch (it.value().type()) {
                    case QJsonValue::String:
                        c.insert(it.key(), it.value().toString());
                        break;
                    case QJsonValue::Double: {
                        const double d = it.value().toDouble();
                        // 整数值按整数串（port/baud/slaveId 均为整数语义）；非整数保留小数
                        // 🟡-A（复审 2026-09-11）：先做有限性/范围守卫——qint64(非有限或超范围 double) 是 UB
                        const bool integral = std::isfinite(d) && d >= -9.0e18 && d <= 9.0e18 && (d == qint64(d));
                        c.insert(it.key(), integral ? QString::number(qint64(d))
                                                    : QString::number(d, 'g', 15));
                        break;
                    }
                    case QJsonValue::Bool:
                        c.insert(it.key(), it.value().toBool() ? QStringLiteral("true") : QStringLiteral("false"));
                        break;
                    default:
                        break;   // Null/Array/Object 不作为连接参数键值
                    }
                }
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
        m_ringBack.insert(tagName, value);   // AB-5：挂一次性回环标记（供 handleValueWritten 抑制写回）
        m_dataManager->setValue(tagName, value);
    }
}

void Acquisition::handleValueWritten(const QString& tagName, const QVariant& value)
{
    // 写通道: DataManager 写采集来源变量 → 路由到对应驱动（Y-4 多协议分发 + Z 循环多连接——
    // mqtt 广播到全部 MQTT 驱动（每驱动自行判断本连接发布绑定——tag 无发布映射则忽略，防跨连接误发）；
    // modbus（AB-3）按「协议 + 所属设备」定位实例——多设备同协议时不再「首个协议匹配」误写别家）
    for (const auto& tag : m_project.tags) {
        if (tag.name != tagName)
            continue;
        const QString proto = protocolForTag(tag);
        if (proto.isEmpty())
            return;   // 内部变量——无驱动写
        // AB-5 联调发现（2026-09-11）：**读回环抑制（仅 modbus）**——采集写入 DataManager 的值经
        // main.cpp 的 valueChanged→handleValueWritten 连线回流本函数，若不识别会把刚读到的值当"用户写"
        // 回写设备（PT100 只读寄存器 0x0000/0x0001 会发非法 FC06）。
        // 实现 = **一次性标记**（onDriverValueRead 在 setValue 前挂 m_ringBack）：首次到达的写消费标记，
        // 且仅当值与采集值相同才抑制 → 用户写回旧值/连写同值不会被永久吞掉（reviewer 🟡-F 复核用例）。
        // 不含 mqtt：MQTT writeValue 兼作周期发布缓存入口（mqtt_driver.cpp writeValue），抑制会停更缓存。
        // 不额外打日志：回环是每次值变化的正常路径，逐条记录会刷屏（失败/异常路径的日志见 reportReadFailure）。
        if (proto != QLatin1String("mqtt")) {
            const auto rb = m_ringBack.constFind(tagName);
            if (rb != m_ringBack.constEnd()) {
                // ⚠️ 顺序关键：**先取值再 remove**——QHash::remove 会使指向该元素的迭代器失效，
                // 先 remove 后 rb.value() 即悬垂访问（2026-09-11 实测 SIGSEGV 崩溃循环的根因）
                const QVariant ringVal = rb.value();
                m_ringBack.remove(tagName);            // 无论是否命中都消费（一次性）
                if (ringVal == value)
                    return;                            // 采集回环 → 抑制本次写回
            }
        }
        if (proto == QLatin1String("mqtt")) {
            // Z 循环：tag 可被多个连接绑定（不同 broker 各发各的 topic）——广播全部 mqtt 驱动
            for (auto* d : m_drivers)
                if (d->protocol() == QLatin1String("mqtt"))
                    d->writeValue(tagName, value);
            return;
        }
        for (auto* d : m_drivers) {
            if (d->protocol() != proto)
                continue;
            const auto it = m_driverDevices.constFind(d);
            const QString driverDevice = (it == m_driverDevices.constEnd()) ? QString() : it.value();
            if (driverDevice != tag.deviceName)
                continue;   // 非本设备实例——跳过（裸变量 deviceName 为空，匹配 device 为空的实例）
            d->writeValue(tagName, value);
            return;
        }
        // 兜底：设备映射缺失（理论上不发生——上游分组同源）——按协议写首个实例，防写值静默丢弃
        for (auto* d : m_drivers)
            if (d->protocol() == proto) { d->writeValue(tagName, value); return; }
        return;
    }
}

} // namespace navihmi
