/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\runtimebus.cpp
 * @Description: 运行时事件总线实现——事件 → 动作执行
 */
#include "runtime/runtimebus.h"

#include <QDebug>
#include <QMetaObject>

namespace navihmi {

RuntimeBus::RuntimeBus(QObject* parent)
    : QObject(parent)
{
}

void RuntimeBus::setProject(const Project& proj)
{
    m_project = proj;
}

void RuntimeBus::emitEvent(const QString& objectName, int eventType)
{
    const EventType et = static_cast<EventType>(eventType);
    const Widget* widget = nullptr;
    QList<WidgetEvent> events;

    // 找到控件（遍历画面）
    const Screen* wmScreen = nullptr;
    for (const auto& sc : m_project.screens) {
        if (sc.type == ScreenType::WorldMap)
            wmScreen = &sc;
        for (const auto& w : sc.widgets) {
            if (w.objectName == objectName) {
                widget = &w;
                events = w.events;
                break;
            }
        }
        if (widget) break;
    }

    // 世界地图级事件（objectName 空或 "__worldmap__"）
    if (!widget && (objectName.isEmpty() || objectName == "__worldmap__")) {
        if (wmScreen)
            events = m_project.worldMap.events;
    }

    // 匹配事件类型，执行动作
    for (const auto& ev : events) {
        if (ev.type == et) {
            for (const auto& action : ev.actions)
                executeAction(action, widget);
        }
    }
}

void RuntimeBus::executeAction(const EventAction& action, const Widget* widget)
{
    Q_UNUSED(widget)
    const auto& p = action.parameters;
    switch (action.type) {
    case ActionType::ScreenSwitch: {
        const QString target = p.value("target_screen");
        if (!target.isEmpty() && onScreenSwitch)
            onScreenSwitch(target);
        break;
    }
    case ActionType::RunCommand: {
        const QString cmd = p.value("command");
        if (cmd == "stop_runtime") {
            if (onStopRuntime) onStopRuntime();
        } else {
            qInfo() << "RuntimeBus: run_command" << cmd;
        }
        break;
    }
    case ActionType::TagWrite:
        qInfo() << "RuntimeBus: tag_write" << p.value("tag_name") << "=" << p.value("value");
        break;
    case ActionType::ShowPopup:
        qInfo() << "RuntimeBus: show_popup" << p.value("title");
        break;
    case ActionType::SendNotification:
        qInfo() << "RuntimeBus: send_notification" << p.value("topic") << p.value("message");
        break;
    case ActionType::SetProperty:
        qInfo() << "RuntimeBus: set_property" << p.value("widget") << p.value("key") << p.value("value");
        break;
    case ActionType::ScreenPrev:
    case ActionType::ScreenNext:
    case ActionType::TagAdd:
    case ActionType::TagSubtract:
    case ActionType::TagToggle:
    case ActionType::SetBit:
    case ActionType::ResetBit:
    case ActionType::SetDatetime:
    case ActionType::GetDatetime:
    case ActionType::AcknowledgeAlarm:
    case ActionType::SetSystemTime:
        // B-4 首版：记录日志（联动动作后续循环细化）
        qInfo() << "RuntimeBus: action" << int(action.type) << p;
        break;
    }
}

} // namespace navihmi
