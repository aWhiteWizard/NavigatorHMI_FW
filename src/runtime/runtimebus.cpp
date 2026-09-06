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
#include <QDateTime>   // F9 V-3 熔断时间窗
#include <cmath>   // S-7 TagStep fmod

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
    // F9 V-3: 熔断状态重置（旧工程风暴不应抑制新工程动作）
    m_fuseActive = false;
    m_stormWindow.invalidate();
    m_fuseTimer.invalidate();
    m_actionCount = 0;
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

void RuntimeBus::emitEvent(const QString& objectName, int eventType, const QString& payload,
                           const QString& sourceScreen)
{
    const EventType et = static_cast<EventType>(eventType);
    // TraceLog（B6-10）: 事件入口 trace——QML 点击 → C++ 事件路由全链路可查
    // （2026-08-26 规范整改: 默认关，与 ObjectManager 一致——审查 N6 大工程启动刷屏教训; NAVIHMI_TRACE=1 开启）
    const bool trace = qEnvironmentVariableIntValue("NAVIHMI_TRACE") != 0;
    if (trace)
        qInfo().noquote() << "[TRACE] emitEvent obj=" << objectName
                          << "type=" << int(et)
                          << "payload=" << payload
                          << "src=" << sourceScreen;   // 2026-08-30: 来源画面（同名控件精确匹配）

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

    // 控件事件：⑪候选A（用户定）——匹配范围 = 当前画面 + 上一画面(OnScreenUnload 兼容) + 全局画面(Template)。
    // 2026-08-30 用户 Check 修复（同名控件连动）：QML 生成器带 sourceScreen——
    //   来源画面有同名控件 → **只执行来源画面动作**（画面一按钮1=返回地图 不再连带触发 全局画面按钮1=StopRuntime）；
    //   来源画面无同名控件 → 回退 ⑪候选A（当前/上一/全局）——overlay 模板按钮独立点击仍触发自己的 stop_runtime
    auto executeWidgetEvents = [&](const Screen& sc, const Widget& w, int& hitOut) {
        for (const auto& ev : w.events) {
            if (ev.type != et) continue;
            // I-3: condition 条件不满足 → 跳过（value 关键字取该控件绑定变量当前值）
            if (!ev.condition.trimmed().isEmpty()
                && !ExprEngine::eval(ev.condition, m_dataManager, &w)) {
                if (trace)
                    qInfo().noquote() << "[COND] skip 条件不满足:" << ev.condition
                                      << "widget=" << w.objectName;
                continue;
            }
            ++hitOut;
            if (trace)
                qInfo().noquote() << "[TRACE]   screen=" << sc.name
                                  << "widget=" << w.objectName << "type=" << int(et);
            for (const auto& action : ev.actions)
                executeAction(action, &w, sc.name);   // G-0: 事件源画面传入动作上下文
        }
    };

    int hit = 0;
    // 1) 来源画面精确匹配（QML 生成器传 sourceScreen：控件所在画面）
    // 审查 🟡 注释：sourceHit 按"来源画面存在同名控件"置位——同名控件事件 condition 不满足时
    // 事件吞掉**不回退**（防全局画面同名控件误触发，新语义更安全；与"来源画面有同名控件 → 只执行来源画面动作"字面一致）
    bool sourceHit = false;
    if (!sourceScreen.isEmpty()) {
        for (int i = 0; i < m_project.screens.size(); ++i) {
            const auto& sc = m_project.screens[i];
            if (sc.name != sourceScreen) continue;
            for (const auto& w : sc.widgets) {
                if (w.objectName != objectName) continue;
                sourceHit = true;
                executeWidgetEvents(sc, w, hit);
            }
            break;
        }
        if (trace && sourceHit)
            qInfo().noquote() << "[TRACE]   sourceScreen 精确命中:" << sourceScreen;
    }
    // 2) 来源画面未命中（或未传来源）→ 回退 ⑪候选A：当前 + 上一 + 全局画面
    if (!sourceHit) {
        for (int i = 0; i < m_project.screens.size(); ++i) {
            const auto& sc = m_project.screens[i];
            if (i != m_currentScreen && i != m_previousScreen
                && sc.type != ScreenType::Template)
                continue;
            for (const auto& w : sc.widgets) {
                if (w.objectName != objectName) continue;
                executeWidgetEvents(sc, w, hit);
            }
        }
    }
    if (trace)
        qInfo().noquote() << "[TRACE]   widget events hit=" << hit
                          << "(current=" << (m_currentScreen >= 0 ? m_project.screens[m_currentScreen].name : "-")
                          << " previous=" << (m_previousScreen >= 0 ? m_project.screens[m_previousScreen].name : "-") << ")";
}

/// 动作类型名称（EventMeta F9 V-3 2026-09-06：动作名集中登记——trace/自检/新增动作漏登记排查用；
/// 与 projectmodel.h ActionType 枚举一一对应（19 值 0..18），switch 全覆盖（无 default——编译器 -Wswitch 提示漏项））
static const char* actionTypeName(ActionType t)   // 文件内静态辅助（cpp-coding §2 自由函数 static）
{
    switch (t) {
    case ActionType::TagWrite: return "TagWrite";
    case ActionType::ScreenSwitch: return "ScreenSwitch";
    case ActionType::SetProperty: return "SetProperty";
    case ActionType::RunCommand: return "RunCommand";
    case ActionType::ShowPopup: return "ShowPopup";
    case ActionType::SendNotification: return "SendNotification";
    case ActionType::ScreenPrev: return "ScreenPrev";
    case ActionType::ScreenNext: return "ScreenNext";
    case ActionType::TagAdd: return "TagAdd";
    case ActionType::TagSubtract: return "TagSubtract";
    case ActionType::TagToggle: return "TagToggle";
    case ActionType::SetBit: return "SetBit";
    case ActionType::ResetBit: return "ResetBit";
    case ActionType::SetDatetime: return "SetDatetime";
    case ActionType::GetDatetime: return "GetDatetime";
    case ActionType::AcknowledgeAlarm: return "AcknowledgeAlarm";
    case ActionType::SetSystemTime: return "SetSystemTime";
    case ActionType::StopRuntime: return "StopRuntime";
    case ActionType::TagStep: return "TagStep";
    }
    return "Unknown";
}

/// 动作风暴阈值（次/秒）——正常画面事件操作远低于此；超限判环熔断（F9 V-3 2026-09-06）
inline constexpr int kActionStormThresholdPerSec = 200;

/// 动作风暴熔断（F9 V-3 环路双保险②）——返回 true = 熔断期（调用方丢弃动作）。
/// 单调时钟（QElapsedTimer）防 SetSystemTime 改墙钟漂移；告警最小间隔 5s（cpp-coding §5 降频）；
/// 单窗口 200/s 即熔断（同步递归型回环在 <1s 爆发——单窗口截断；一次性合法突发 >200/s 罕见，
/// 熔断 1s 自愈，装载类单发动作不受影响——边界注释 2026-09-06）。
bool RuntimeBus::guardActionStorm()
{
    if (m_fuseActive) {
        if (m_fuseTimer.elapsed() >= 1000) {
            m_fuseActive = false;          // 1s 自愈：清窗从 0 计数
            m_stormWindow.restart();
        }
        return true;   // 熔断期：丢弃动作（静默——触发告警已打过，不刷屏）
    }
    if (!m_stormWindow.isValid() || m_stormWindow.elapsed() >= 1000) {
        m_stormWindow.restart();
        m_actionCount = 0;
    }
    if (++m_actionCount > kActionStormThresholdPerSec) {
        m_fuseActive = true;
        m_fuseTimer.restart();
        m_actionCount = 0;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastStormWarnMs >= 5000) {   // 告警 5s 节流（持续风暴不刷屏）
            m_lastStormWarnMs = nowMs;
            qWarning().noquote() << "RuntimeBus: 事件动作执行风暴(>"
                                 << kActionStormThresholdPerSec
                                 << "次/秒)疑似环路——已熔断1s；检查 OnValueChange/写值类动作回环配置"
                                 << "(如 OnValueChange 绑定 TagAdd/TagToggle 自身变量的增值自激)";
        }
        return true;
    }
    return false;
}

void RuntimeBus::executeAction(const EventAction& action, const Widget* widget, const QString& sourceScreen)
{
    if (guardActionStorm())
        return;   // 熔断期：动作被丢弃（reviewer 🔴 2026-09-06：熔断必须在此落地——guard 只做判定）
    Q_UNUSED(widget)
    if (qEnvironmentVariableIntValue("NAVIHMI_TRACE") != 0)
        qInfo().noquote() << "[TRACE]   action=" << actionTypeName(action.type);
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
    case ActionType::StopRuntime: {
        // M-3: 运行时停止（独立动作类型；旧工程 run_command=stop_runtime 兼容路径保留在上方 RunCommand）
        if (onStopRuntime) onStopRuntime();
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
    case ActionType::TagStep: {   // S-7（2026-09-05 用户拍板格子回绕）：result = min + ((cur-min+step) mod span)
        // span = max-min+1（含端点）；负步长同样回绕（低于 min 余量从 max 续减）——6+1→0、5+3→1（用户语义）
        const QString tagName = p.value("tag_name");
        if (m_dataManager && !tagName.isEmpty()) {
            double cur = m_dataManager->value(tagName).toDouble();
            const double step = p.value("step").toDouble();
            const double minV = p.value("min").toDouble();
            const double maxV = p.value("max").toDouble();
            if (maxV >= minV) {
                const double span = maxV - minV + 1.0;
                double off = cur - minV + step;
                // 正数模（C++ % 负数负余数——需归正）
                off = fmod(off, span);
                if (off < 0) off += span;
                m_dataManager->setValue(tagName, minV + off);
            } else {
                qWarning().noquote() << "RuntimeBus: TagStep min>max 参数错误" << p;
            }
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
