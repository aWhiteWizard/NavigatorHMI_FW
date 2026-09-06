/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.cpp
 * @Description: 数据采集管理实现（V-1 2026-09-06 Manager 化——协议逻辑已迁 drivers/）
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
    delete m_driver;   // IDriver QObject——stop/清理由析构（ModbusTcpDriver 析构断开连接）
    m_driver = nullptr;
}

void Acquisition::stop()
{
    // V-4 F11：主壳 Shutdown 显式停止（停调度 timer + 驱动断连；不 delete——析构兜底）
    if (m_timer)
        m_timer->stop();
    if (m_driver)
        m_driver->stop();
}

void Acquisition::setProject(const Project& proj)
{
    m_project = proj;
    m_lastVal.clear();

    // 重建驱动（工程重载/切工程：旧驱动 stop 删除）
    if (m_driver) {
        m_driver->stop();
        delete m_driver;
        m_driver = nullptr;
    }
    if (m_timer) {
        m_timer->stop();
    }

    // tag 解析分组（V-1: 仅收 modbus:// —— 基线语义；mqtt:// V+1 驱动落地后放开并支持多协议分发）
    QList<DriverTagInfo> driverTags;
    for (const auto& tag : m_project.tags) {
        if (!tag.source.startsWith(QStringLiteral("modbus://")))
            continue;   // 仅采集来源 modbus；内部变量/mqtt（V+1 前无驱动）跳过
        DriverTagInfo info;
        info.name = tag.name;
        info.source = tag.source;
        info.deviceName = tag.deviceName;
        info.dataType = int(tag.dataType);
        info.scanMs = tag.scanIntervalMs;
        info.deadband = tag.deadband;
        info.conn = connInfoForDevice(tag.deviceName);
        driverTags.append(info);
    }
    if (driverTags.isEmpty()) {
        qInfo().noquote() << "Acquisition: 无采集来源变量（source 非 modbus://）, 采集未启动";
        return;
    }

    // V-1: 按协议路由创建驱动（本版单协议 modbus_tcp；跨协议分发 + 多驱动 V+1 扩展）
    const QString protocol = QStringLiteral("modbus_tcp");
    m_driver = createDriverForProtocol(protocol, this);
    if (!m_driver) {
        qWarning().noquote() << "Acquisition: 无可用驱动" << protocol << "（未注册/被裁剪）";
        return;
    }
    connect(m_driver, &IDriver::valueRead, this, &Acquisition::onDriverValueRead);
    connect(m_driver, &IDriver::connectionError, this,
            [](const QString& msg) { qWarning().noquote() << "Acquisition:" << msg; });

    if (!m_driver->configure(driverTags)) {
        qInfo().noquote() << "Acquisition: 驱动配置无有效采集项" << protocol;
        delete m_driver;
        m_driver = nullptr;
        return;
    }
    m_driver->start();

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(100);   // 100ms 调度（过程数据 ≤100ms 目标，原语义保留）
        connect(m_timer, &QTimer::timeout, this, &Acquisition::tick);
    }
    m_timer->start();
    qInfo().noquote() << "Acquisition: 采集启动 protocol=" << protocol
                      << "tags=" << driverTags.size();
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
    if (!m_driver || !m_dataManager)
        return;
    m_driver->poll(QDateTime::currentMSecsSinceEpoch());
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
    // 写通道: DataManager 写采集来源变量 → driver.writeValue（main.cpp 连接 valueChanged 调用）
    if (m_driver)
        m_driver->writeValue(tagName, value);
}

} // namespace navihmi
