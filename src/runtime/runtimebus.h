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
#include <QElapsedTimer>
#include <functional>
#include "runtime/projectmodel.h"

namespace navihmi {

class DataManager;
class ObjectManager;

class RuntimeBus : public QObject
{
    Q_OBJECT
public:
    explicit RuntimeBus(QObject* parent = nullptr);

    /// 设置运行时工程（QML 事件路由的依据）
    void setProject(const Project& proj);
    /// 只读工程模型（I-1 SSH CLI 命令服务数据源：screens/tags/alarms）
    const Project& project() const { return m_project; }
    /// 当前画面名（I-1 CLI screen current；无工程/未进入返回空）
    QString currentScreenName() const;
    /// 设置数据管理器（TagWrite 等动作写值用）
    void setDataManager(DataManager* dm);
    /// 设置对象管理器（G-0: set_property 等动作经 ObjectManager 跨画面寻址执行）
    void setObjectManager(ObjectManager* om);
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
    /// payload（H-7/M8）：可选事件负载（onAck 报警编号 / onSelect 机器人编号等；旧调用不传=空）
    /// sourceScreen（2026-08-30 用户 Check 修复）：QML 生成器传事件来源画面名——同名控件不再连动：
    ///   来源画面有同名控件 → 只执行来源画面动作（如画面一按钮1=返回地图，不再连带触发全局画面按钮1=StopRuntime）；
    ///   来源画面无同名控件 → 回退 当前画面 + 上一画面 + 全局画面 匹配（旧语义兼容）
    void emitEvent(const QString& objectName, int eventType, const QString& payload = QString(),
                   const QString& sourceScreen = QString());

private:
    void executeAction(const EventAction& action, const Widget* widget, const QString& sourceScreen);
    /// 动作风暴熔断（F9 V-3 2026-09-06 环路双保险②）：1s 窗口计数超阈值 → 熔断 1s 丢弃动作
    /// （防 OnValueChange + 增值类动作（TagAdd/Toggle 等）配置回环风暴——源头同值防抖在 DataManager
    ///  层已存在为保险①）；返回 true = 熔断期（executeAction 丢弃本次动作）；单调时钟（QElapsedTimer）
    ///  不随 SetSystemTime 改墙钟漂移；告警最小间隔 5s（cpp-coding §5 降频）；setProject 重置（工程重载
    ///  不被旧风暴抑制；resetScreens 不清——熔断 1s 自愈 + 回导航后无事件流，惰性自愈可接受）
    bool guardActionStorm();
    Project m_project;
    DataManager* m_dataManager = nullptr;
    ObjectManager* m_objectManager = nullptr;
    int m_currentScreen = -1;    // 当前画面索引（⑪候选A: 控件事件匹配范围）
    int m_previousScreen = -1;   // 上一画面索引（兼容 OnScreenUnload 卸载瞬间）
    QElapsedTimer m_stormWindow;      // 计数窗口（1s 滑动）
    QElapsedTimer m_fuseTimer;        // 熔断计时（1s 后自愈）
    bool m_fuseActive = false;        // 熔断中
    int m_actionCount = 0;            // 窗口内动作执行计数
    qint64 m_lastStormWarnMs = 0;     // 最近告警墙钟（5s 节流）
};

} // namespace navihmi
