/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\driver_registry.cpp
 * @Description: 采集驱动注册表实现（std::vector 持所有权 + QHash 存裸指针映射——
 *               QHash<QString, unique_ptr> 值类型拷贝受限（2026-09-06 编译修正））
 */
#include "drivers/driver_registry.h"
#include "drivers/modbus_tcp_driver.h"   // init_driver_modbus_tcp（extern C，文件尾全局声明）
#include "drivers/mqtt_driver.h"         // Y-4: init_driver_mqtt（NAVIHMI_HAVE_MQTT_DRIVER 裁剪）

#include <QHash>
#include <vector>

namespace navihmi {

namespace {

struct DriverRegistry {
    std::vector<std::unique_ptr<IDriverFactory>> owners;      // 所有权
    QHash<QString, IDriverFactory*> byProtocol;               // 协议 → 工厂（裸指针）
};

DriverRegistry& registry()
{
    static DriverRegistry s_registry;
    return s_registry;
}

} // namespace

void registerDriverFactory(std::unique_ptr<IDriverFactory> factory)
{
    if (!factory)
        return;
    auto& r = registry();
    IDriverFactory* raw = factory.get();
    r.byProtocol.insert(raw->protocol(), raw);
    r.owners.push_back(std::move(factory));
}

IDriver* createDriverForProtocol(const QString& protocol, QObject* parent)
{
    auto& r = registry();
    auto it = r.byProtocol.constFind(protocol);
    return it == r.byProtocol.constEnd() ? nullptr : it.value()->create(parent);
}

QStringList registeredProtocols()
{
    auto& r = registry();
    return r.byProtocol.keys();
}

void registerBuiltinDrivers()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
#ifdef NAVIHMI_HAVE_MODBUS_DRIVER
    init_driver_modbus_tcp();
#endif
#ifdef NAVIHMI_HAVE_MQTT_DRIVER
    init_driver_mqtt();   // Y-4: MQTT 驱动（qtmqtt 链接级——宏裁剪时关宏不链接）
#endif
}

} // namespace navihmi
