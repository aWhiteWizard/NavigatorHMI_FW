/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\runtimebus.cpp
 * @Description: 运行时事件总线实现——事件 → 动作执行
 */
#include "runtime/runtimebus.h"
#include "runtime/datamanager.h"

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

void RuntimeBus::setDataManager(DataManager* dm)
{
    m_dataManager = dm;
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
    case ActionType::TagWrite: {
        // B-5: 实际写 DataManager（变量实时值 → QML 组件刷新）
        const QString tagName = p.value("tag_name");
        if (m_dataManager && !tagName.isEmpty()) {
            const QString val = p.value("value");
            if (!val.isEmpty()) {
                // 按目标变量类型解析（简化：数字优先，否则字符串）
                const Tag* tag = m_project.tagByName(tagName);
                QVariant v = val;
                if (tag && (tag->dataType == TagDataType::Float
                            || tag->dataType == TagDataType::Int16
                            || tag->dataType == TagDataType::Uint16
                            || tag->dataType == TagDataType::Int32)) {
                    bool ok = false;
                    double d = val.toDouble(&ok);
                    if (ok) v = d;
                } else if (tag && tag->dataType == TagDataType::Bool) {
                    v = (val == "1" || val == "true" || val == "TRUE" || val == "True");
                }
                m_dataManager->setValue(tagName, v);
            }
        }
        break;
    }
    case ActionType::ShowPopup:
        qInfo() << "RuntimeBus: show_popup" << p.value("title") << p.value("message");
        break;
    case ActionType::SendNotification:
        qInfo() << "RuntimeBus: send_notification" << p.value("topic") << p.value("message");
        break;
    case ActionType::SetProperty:
        // B-5: 属性修改——跨组件寻址后续（ObjectManager）；当前记录目标
        qInfo() << "RuntimeBus: set_property" << p.value("widget") << p.value("key") << p.value("value");
        break;
    case ActionType::TagAdd:
    case ActionType::TagSubtract: {
        const QString tagName = p.value("tag_name");
        if (m_dataManager && !tagName.isEmpty()) {
            double cur = m_dataManager->value(tagName).toDouble();
            double delta = p.value("value").toDouble();
            m_dataManager->setValue(tagName, cur + (action.type == ActionType::TagAdd ? delta : -delta));
        }
        break;
    }
    case ActionType::TagToggle: {
        const QString tagName = p.value("tag_name");
        if (m_dataManager && !tagName.isEmpty()) {
            bool cur = m_dataManager->value(tagName).toBool();
            m_dataManager->setValue(tagName, !cur);
        }
        break;
    }
    case ActionType::SetBit:
    case ActionType::ResetBit: {
        const QString tagName = p.value("tag_name");
        if (m_dataManager && !tagName.isEmpty())
            m_dataManager->setValue(tagName, action.type == ActionType::SetBit);
        break;
    }
    case ActionType::ScreenPrev:
    case ActionType::ScreenNext:
        // 导航栈前进/后退（后续：主壳维护栈；当前切换相邻画面）
        qInfo() << "RuntimeBus: screen" << (action.type == ActionType::ScreenPrev ? "prev" : "next");
        break;
    case ActionType::SetDatetime:
    case ActionType::GetDatetime:
    case ActionType::AcknowledgeAlarm:
    case ActionType::SetSystemTime:
        // 记录日志（时间/报警系统后续细化）
        qInfo() << "RuntimeBus: action" << int(action.type) << p;
        break;
    }
}

} // namespace navihmi
