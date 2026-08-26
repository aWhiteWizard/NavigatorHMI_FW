/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\runtimebus.cpp
 * @Description: 运行时事件总线实现——事件 → 动作执行
 */
#include "runtime/runtimebus.h"
#include "runtime/datamanager.h"
#include "runtime/objectmanager.h"
#include "runtime/expressionengine.h"   // I-3: 事件 condition 条件求值

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
    // 工程重载（替换默认工程文件后 reload）: 画面索引失效, 重置匹配状态
    m_currentScreen = -1;
    m_previousScreen = -1;
}

void RuntimeBus::setDataManager(DataManager* dm)
{
    m_dataManager = dm;
}

QString RuntimeBus::currentScreenName() const
{
    if (m_currentScreen >= 0 && m_currentScreen < m_project.screens.size())
        return m_project.screens[m_currentScreen].name;
    return QString();
}

void RuntimeBus::setObjectManager(ObjectManager* om)
{
    m_objectManager = om;
}

void RuntimeBus::setCurrentScreenByName(const QString& name)
{
    // ⑪候选A（用户定）: 切换画面时更新匹配范围——previous=current, current=目标
    m_previousScreen = m_currentScreen;
    m_currentScreen = -1;
    for (int i = 0; i < m_project.screens.size(); ++i) {
        if (m_project.screens[i].name == name) {
            m_currentScreen = i;
            break;
        }
    }
}

void RuntimeBus::resetScreens()
{
    // Stop Runtime 回导航：清空画面匹配（防旧画面索引幽灵匹配）
    m_currentScreen = -1;
    m_previousScreen = -1;
}

void RuntimeBus::emitEvent(const QString& objectName, int eventType, const QString& payload)
{
    const EventType et = static_cast<EventType>(eventType);
    // TraceLog（B6-10）: 事件入口 trace——QML 点击 → C++ 事件路由全链路可查
    // （2026-08-26 规范整改: 默认关，与 ObjectManager 一致——审查 N6 大工程启动刷屏教训; NAVIHMI_TRACE=1 开启）
    const bool trace = qEnvironmentVariableIntValue("NAVIHMI_TRACE") != 0;
    if (trace)
        qInfo().noquote() << "[TRACE] emitEvent obj=" << objectName
                          << "type=" << int(et)
                          << "payload=" << payload;   // H-7(M8): 事件负载（报警/机器人编号）

    // 世界地图级事件（objectName 空或 "__worldmap__"）
    if (objectName.isEmpty() || objectName == "__worldmap__") {
        int hit = 0;
        for (const auto& ev : m_project.worldMap.events) {
            if (ev.type == et) {
                // I-3: condition 条件不满足 → 跳过该事件（空条件=无条件）
                if (!ev.condition.trimmed().isEmpty()
                    && !ExprEngine::eval(ev.condition, m_dataManager, nullptr)) {
                    if (trace)
                        qInfo().noquote() << "[COND] skip 世界地图 条件不满足:" << ev.condition;
                    continue;
                }
                ++hit;
                for (const auto& action : ev.actions)
                    executeAction(action, nullptr, QStringLiteral("__worldmap__"));
            }
        }
        if (trace)
            qInfo().noquote() << "[TRACE]   worldMap events hit=" << hit;
        return;
    }

    // 控件事件：⑪候选A（用户定）——仅匹配 当前画面 + 上一画面(OnScreenUnload 兼容) + 全局画面(Template)，
    // 消除其他自定义画面同名控件误触发（如画面A/画面B 同名 button_1 互不连动；
    // 注: 全局 Template 画面恒匹配，若与 overlay 控件同名仍会连动——demo 工程 Stop(button_1) 与画面A 同名属此，勿再改）
    int hit = 0;
    for (int i = 0; i < m_project.screens.size(); ++i) {
        const auto& sc = m_project.screens[i];
        if (i != m_currentScreen && i != m_previousScreen
            && sc.type != ScreenType::Template)
            continue;
        for (const auto& w : sc.widgets) {
            if (w.objectName == objectName) {
                for (const auto& ev : w.events) {
                    if (ev.type == et) {
                        // I-3: condition 条件不满足 → 跳过（value 关键字取该控件绑定变量当前值）
                        if (!ev.condition.trimmed().isEmpty()
                            && !ExprEngine::eval(ev.condition, m_dataManager, &w)) {
                            if (trace)
                                qInfo().noquote() << "[COND] skip 条件不满足:" << ev.condition
                                                  << "widget=" << w.objectName;
                            continue;
                        }
                        ++hit;
                        if (trace)
                            qInfo().noquote() << "[TRACE]   screen=" << sc.name
                                              << "widget=" << w.objectName << "type=" << int(et);
                        for (const auto& action : ev.actions)
                            executeAction(action, &w, sc.name);   // G-0: 事件源画面传入动作上下文
                    }
                }
            }
        }
    }
    if (trace)
        qInfo().noquote() << "[TRACE]   widget events hit=" << hit
                          << "(current=" << (m_currentScreen >= 0 ? m_project.screens[m_currentScreen].name : "-")
                          << " previous=" << (m_previousScreen >= 0 ? m_project.screens[m_previousScreen].name : "-") << ")";
}

void RuntimeBus::executeAction(const EventAction& action, const Widget* widget, const QString& sourceScreen)
{
    Q_UNUSED(widget)
    const auto& p = action.parameters;
    switch (action.type) {
    case ActionType::ScreenSwitch: {
        const QString target = p.value("target_screen");
        // TraceLog（B6-10）: 切换动作打目标（ScreenSwitch 原无日志——B-5 排查痛点）
        // （2026-08-26 规范整改: 默认关，与 emitEvent/ObjectManager 一致; NAVIHMI_TRACE=1 开启）
        if (qEnvironmentVariableIntValue("NAVIHMI_TRACE") != 0)
            qInfo().noquote() << "[TRACE]   action=ScreenSwitch target=" << target;
        if (!target.isEmpty() && onScreenSwitch)
            onScreenSwitch(target);
        break;
    }
    case ActionType::RunCommand: {
        const QString cmd = p.value("command");
        if (cmd == "stop_runtime") {
            if (onStopRuntime) onStopRuntime();
        } else {
            qInfo().noquote() << "RuntimeBus: run_command" << cmd;
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
                const Tag* tag = m_project.TagByName(tagName);
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
        qInfo().noquote() << "RuntimeBus: show_popup" << p.value("title") << p.value("message");
        break;
    case ActionType::SendNotification:
        qInfo().noquote() << "RuntimeBus: send_notification" << p.value("topic") << p.value("message");
        break;
    case ActionType::SetProperty: {
        // G-0: 属性修改实装——经 ObjectManager 跨画面寻址设置 QML 控件属性
        // 参数契约：screen_name(可选, 空=事件源画面[审查 M2: 与 RuntimeBus 当前+上一+Template
        // 匹配口径对齐, 防"切画面后 OnScreenUnload 事件写错到新画面同名控件"]) + widget_name/widget(兼容旧键) + key + value
        const QString screenName = p.value("screen_name").isEmpty() ? sourceScreen : p.value("screen_name");
        const QString widgetName = p.value("widget_name").isEmpty() ? p.value("widget") : p.value("widget_name");
        const QString key = p.value("key");
        const QString value = p.value("value");
        if (m_objectManager && !widgetName.isEmpty() && !key.isEmpty()) {
            const bool ok = m_objectManager->setProperty(screenName, widgetName, key, value);
            // 审查 N-4/N-5(复审): 未加载画面控件/已注销(上一画面幽灵匹配)属正常生命周期,
            // 失败降 qInfo 不刷告警; 真错误（拼错控件名/未知键）靠 NAVIHMI_TRACE=1 排查
            if (!ok)
                qInfo().noquote() << "RuntimeBus: set_property 未生效(控件未加载/已注销/未知键) screen="
                                  << screenName << "widget=" << widgetName << "key=" << key << "value=" << value;
        } else {
            qInfo().noquote() << "RuntimeBus: set_property" << widgetName << key << value;
        }
        break;
    }
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
        qInfo().noquote() << "RuntimeBus: screen" << (action.type == ActionType::ScreenPrev ? "prev" : "next");
        break;
    case ActionType::SetDatetime:
    case ActionType::GetDatetime:
    case ActionType::AcknowledgeAlarm:
    case ActionType::SetSystemTime:
        // 记录日志（时间/报警系统后续细化）
        qInfo().noquote() << "RuntimeBus: action" << int(action.type) << p;
        break;
    }
}

} // namespace navihmi
