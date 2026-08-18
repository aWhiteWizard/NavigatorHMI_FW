/*
 * @Author: aWhiteWizard www.123518341@qq.com
 * @FilePath: \NavigatorHMI_FW\src\runtime\projectmodel.h
 * @Description: 运行时模型（从 .navihmi 契约解析后的内存模型）
 *               与 fw/proto/navihmi.proto 对应；QML/报警/渲染只依赖本模型，不直接碰 protobuf
 */
#pragma once

#include <QString>
#include <QVector>
#include <QList>
#include <QHash>
#include <QDateTime>

namespace navihmi {

// ── 枚举（值 = navihmi.proto 枚举值） ──
enum class ScreenType { Template = 0, WorldMap = 1, Custom = 2 };
enum class NavPosition { Top = 0, Bottom = 1 };
enum class TagDataType { Bool = 0, Int16 = 1, Uint16 = 2, Int32 = 3, Float = 4, String = 5 };
enum class AlarmType { High = 0, Low = 1, RateChange = 2, Deviation = 3 };
enum class Severity { Emergency = 0, Important = 1, Warning = 2, Info = 3 };
enum class ListType { Text = 0, Image = 1 };
enum class ProtocolType { ModbusRtu = 0, ModbusTcp = 1, Mqtt = 2 };
enum class WidgetType {
    Button = 0, Text = 1, Label = 2, Rectangle = 3, Image = 4, NumericDisplay = 5,
    Switch = 6, Line = 7, Circle = 8, Ellipse = 9, IoField = 10,
    CheckBox = 11, TextList = 12, Frame = 13, ProgressBar = 14
};
enum class EventType {
    OnClick = 0, OnPress = 1, OnRelease = 2, OnValueChange = 3,
    OnAlarmTrigger = 4, OnAlarmAck = 5, OnAlarmClear = 6,
    OnScreenLoad = 7, OnScreenUnload = 8, OnTimer = 9,
    OnSystemStart = 10, OnSystemShutdown = 11
};
enum class ActionType {
    TagWrite = 0, ScreenSwitch = 1, SetProperty = 2,
    RunCommand = 3, ShowPopup = 4, SendNotification = 5
};

// ── 模型 ──

struct EventAction {
    ActionType type = ActionType::TagWrite;
    QHash<QString, QString> parameters;
};

struct WidgetEvent {
    EventType type = EventType::OnClick;
    QString condition;
    QList<EventAction> actions;
};

/// 控件（扁平结构，同 navihmi.proto Widget）
struct Widget {
    // 基类
    double x = 0, y = 0, width = 0, height = 0;
    QString objectName;
    QString boundTag;
    QList<WidgetEvent> events;
    // 类型判别
    WidgetType type = WidgetType::Rectangle;
    // 各类型字段
    QString text, content, hAlign;
    QString fontFamily, fontWeight, fontStyle, textDecoration;
    double fontSize = 0;
    QString textColor, fillColor, strokeColor;
    double strokeThickness = 0;
    QString imagePath, stretchMode;
    QString listRef;
    int defaultIndex = 0;
    double value = 0, min = 0, max = 0;
    QString fillStyle;
    bool isOn = false, isChecked = false, isReadOnly = false;
    QString onText, offText;
    double x2 = 0, y2 = 0;
    QString title;
};

struct Screen {
    QString name;
    double width = 0, height = 0;
    ScreenType type = ScreenType::Custom;
    QList<Widget> widgets;
    bool isGlobal = false;
    bool showInNav = true;
    int navOrder = 0;
};

struct Tag {
    QString name;
    TagDataType dataType = TagDataType::Float;
    QString unit;
    QString source;          // modbus:// 或 mqtt://，空 = 内部变量
    int scanIntervalMs = 0;
    double deadband = 0;
    QString description;
    QString baseValue;       // 设计态基准值
};

struct AlarmRule {
    QString name;
    QString tagName;
    AlarmType type = AlarmType::High;
    double threshold = 0;
    double deadband = 0;
    int delayMs = 0;
    Severity level = Severity::Warning;
    QString message;
};

struct ListDef {
    QString name;
    ListType type = ListType::Text;
    QStringList items;
};

struct DeviceConfig {
    QString name;
    ProtocolType protocol = ProtocolType::ModbusRtu;
    QString connectionInfo;   // JSON
};

struct WorldMapConfig {
    double latMin = 0, latMax = 0, lngMin = 0, lngMax = 0;
    QString tileSource;
    int zoomLevel = 0;
    bool showGlobalOverlay = false;
};

/// 工程根模型
struct Project {
    QString name;
    QDateTime createTime, lastModifiedTime;
    QString version;
    QList<Screen> screens;
    QList<Tag> tags;
    QList<AlarmRule> alarms;
    QList<DeviceConfig> devices;
    QList<ListDef> lists;
    WorldMapConfig worldMap;
    bool showNavigationBar = true;
    NavPosition navigationPosition = NavPosition::Top;
    QString startScreen;
    int formatVersion = 0;    // 契约版本（1 = 当前）

    /// 按名称查画面（找不到返回 nullptr）
    const Screen* screenByName(const QString& name) const;
    /// 按名称查变量（找不到返回 nullptr）
    const Tag* tagByName(const QString& name) const;
    /// 按名称查报警规则（找不到返回 nullptr）
    const AlarmRule* alarmByName(const QString& name) const;
    /// 按名称查列表（找不到返回 nullptr）
    const ListDef* listByName(const QString& name) const;
    /// 启动画面（无则取第一个 Custom 画面）
    const Screen* startScreenModel() const;
};

} // namespace navihmi
