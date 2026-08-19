/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\runtimebus.h
 * @Description: 运行时事件总线——QML 只发事件（emitEvent），C++ ActionRunner 执行动作
 *               事件路由：控件事件 → 查模型 WidgetEvent → 执行 Actions
 * B-4: 首版——事件收集 + 动作执行（TagWrite/ScreenSwitch 等核心动作）
 */
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>
#include "runtime/projectmodel.h"

namespace navihmi {

class DataManager;

class RuntimeBus : public QObject
{
    Q_OBJECT
public:
    explicit RuntimeBus(QObject* parent = nullptr);

    /// 设置运行时工程（QML 事件路由的依据）
    void setProject(const Project& proj);
    /// 设置数据管理器（TagWrite 等动作写值用）
    void setDataManager(DataManager* dm);
    /// 画面切换回调（主壳注入：切到指定画面名）
    std::function<void(const QString&)> onScreenSwitch;
    /// 返回导航回调（Stop Runtime）
    std::function<void()> onStopRuntime;
    /// 当前画面切换（⑪候选A: 控件事件匹配范围 = 当前画面 + 上一画面 + 全局画面）
    /// 上一画面兼容 OnScreenUnload（画面卸载瞬间 currentScreen 已更新，旧画面事件仍可命中）
    /// 同步入口唯一 = main.qml switchTo() 内调用（startProject/switchToName/switchTo(0) 全路径经此）
    Q_INVOKABLE void setCurrentScreenByName(const QString& name);
    /// 重置画面匹配状态（Stop Runtime 回导航时调用，防旧画面索引幽灵匹配）
    Q_INVOKABLE void resetScreens();

public slots:
    /// QML 事件入口：objectName 控件事件 → 查模型 → 执行动作
    /// objectName 空 = 世界地图级事件
    void emitEvent(const QString& objectName, int eventType);

private:
    void executeAction(const EventAction& action, const Widget* widget);
    Project m_project;
    DataManager* m_dataManager = nullptr;
    int m_currentScreen = -1;    // 当前画面索引（⑪候选A: 控件事件匹配范围）
    int m_previousScreen = -1;   // 上一画面索引（兼容 OnScreenUnload 卸载瞬间）
};

} // namespace navihmi
