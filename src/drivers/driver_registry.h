/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\driver_registry.h
 * @Description: 采集驱动注册表（V-1 2026-09-06 驱动插件化——工厂登记/按协议创建/宏裁剪）
 *               注册: registerBuiltinDrivers() 由 Acquisition 首次使用前调用（幂等）
 *               裁剪: 各 driver 的 extern C init 由编译宏保护（关宏不注册——链接级裁剪 V+1 补 FILTER）
 */
#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include "drivers/driver_iface.h"

namespace navihmi {

/// 登记驱动工厂（所有权由注册表持有；同协议后注册覆盖映射，旧工厂保留至进程结束）
void registerDriverFactory(std::unique_ptr<IDriverFactory> factory);

/// 按协议创建驱动（未注册返回 nullptr）
IDriver* createDriverForProtocol(const QString& protocol, QObject* parent);

/// 已注册协议列表（诊断/调试用）
QStringList registeredProtocols();

/// 注册全部内置驱动（幂等——Acquisition 构造调用；宏裁剪的驱动此处不注册）
void registerBuiltinDrivers();

} // namespace navihmi
