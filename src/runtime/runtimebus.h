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

class RuntimeBus : public QObject
{
    Q_OBJECT
public:
    explicit RuntimeBus(QObject* parent = nullptr);

    /// 设置运行时工程（QML 事件路由的依据）
    void setProject(const Project& proj);
    /// 画面切换回调（主壳注入：切到指定画面名）
    std::function<void(const QString&)> onScreenSwitch;
    /// 返回导航回调（Stop Runtime）
    std::function<void()> onStopRuntime;

public slots:
    /// QML 事件入口：objectName 控件事件 → 查模型 → 执行动作
    /// objectName 空 = 世界地图级事件
    void emitEvent(const QString& objectName, int eventType);

private:
    void executeAction(const EventAction& action, const Widget* widget);
    Project m_project;
    int m_currentScreen = -1;
};

} // namespace navihmi
