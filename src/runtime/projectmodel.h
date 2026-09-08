/*
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
#include <QPointF>

namespace navihmi {

// ── 枚举（值 = navihmi.proto 枚举值） ──
enum class ScreenType { Template = 0, WorldMap = 1, Custom = 2 };
enum class NavPosition { Top = 0, Bottom = 1 };
enum class TagDataType { Bool = 0, Int16 = 1, Uint16 = 2, Int32 = 3, Float = 4, String = 5, DateTime = 6, Gps = 7 };
enum class AlarmType { High = 0, Low = 1, RateChange = 2, Deviation = 3 };
enum class Severity { Emergency = 0, Important = 1, Warning = 2, Info = 3 };
enum class ListType { Text = 0, Image = 1, Video = 2 };  // S-5: Video 列表（项 = 视频源地址串；值 = navihmi.proto ListType）
enum class ProtocolType { ModbusRtu = 0, ModbusTcp = 1, Mqtt = 2 };
enum class WidgetType {
    Button = 0, Text = 1, Label = 2, Rectangle = 3, Image = 4, NumericDisplay = 5,
    Switch = 6, Line = 7, Circle = 8, Ellipse = 9, IoField = 10,
    CheckBox = 11, TextList = 12, Frame = 13, ProgressBar = 14,
    DateTime = 15, Window = 16, Polygon = 17,
    TrendView = 18,   // P-4：趋势图（2026-09-02）
    HistoryView = 19   // P-5：历史记录（2026-09-02，枚举一次到位）
};
enum class EventType {
    OnClick = 0, OnPress = 1, OnRelease = 2, OnValueChange = 3,
    OnAlarmTrigger = 4, OnAlarmAck = 5, OnAlarmClear = 6,
    OnScreenLoad = 7, OnScreenUnload = 8, OnTimer = 9,
    OnSystemStart = 10, OnSystemShutdown = 11, OnInput = 12,
    OnOn = 13, OnOff = 14, OnProgressComplete = 15,
    OnUserChanged = 16, OnAck = 17, OnSelect = 18
};
enum class ActionType {
    TagWrite = 0, ScreenSwitch = 1, SetProperty = 2,
    RunCommand = 3, ShowPopup = 4, SendNotification = 5,
    ScreenPrev = 6, ScreenNext = 7, TagAdd = 8, TagSubtract = 9,
    TagToggle = 10, SetBit = 11, ResetBit = 12,
    SetDatetime = 13, GetDatetime = 14, AcknowledgeAlarm = 15, SetSystemTime = 16,
    StopRuntime = 17,   // 运行时停止（退出当前工程回导航首页；M-3）
    TagStep = 18   // S-7 变量循环步进（格子回绕 min/max/step；6+1→0、5+3→1）
};
enum class AlarmTriggerMode { Threshold = 0, OnRising = 1, OnFalling = 2, OnChange = 3 };
enum class AlarmCategory { System = 0, User = 1, Error = 2 };
enum class WindowType { UserView = 0, AlarmView = 1, RobotList = 2 };

/// 事件优先级（V-3c 2026-09-06，与 proto WidgetEvent.priority 对齐；Normal=0 零值默认——
/// Low 因默认零值无法低于 Normal，仅名义保留，实际三档 Normal/High/Emergency）
enum class EventPriority { Normal = 0, High = 1, Emergency = 2, Low = 3 };

// ── 模型 ──

struct EventAction {
    ActionType type = ActionType::TagWrite;
    QHash<QString, QString> parameters;
};

struct WidgetEvent {
    EventType type = EventType::OnClick;
    QString condition;
    QList<EventAction> actions;
    int priority = 0;   // 事件优先级（V-3c：EventPriority 数值，0=Normal 默认）
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
    QString labelOn, labelOff;
    double x2 = 0, y2 = 0;
    QString title;
    // DateTime (15)
    QString dtText, dtFormat;
    // 窗口控件 (16)
    WindowType windowType = WindowType::UserView;
    QString winTitle;
    bool showTitleBar = true, showHistory = false;
    QString selectedTag;
    double cardWidth = 0, cardHeight = 0;
    bool showUserName = false, showRole = false, showMode = false;
    bool cardShowNumber = false, cardShowStatus = false, cardShowLocation = false;
    QString boundDevice;
    // RobotSlots: 用 QList<RobotSlotBinding> 简化——name 键 = IdTag
    QList<QHash<QString, QString>> robotSlots;   // 每项: id/status/location/detail/oper
    // 多边形 (17)
    QList<QPointF> points;                       // 顶点画面坐标
    // 趋势图 (18, P-4 2026-09-02)
    int trendMode = 0;              // 0=时间-数据 1=变量A-B
    QString trendTagA, trendTagB;
    int sampleIntervalMs = 0;       // 采样间隔 ms（0 = 用默认 1000）
    int timeWindowSeconds = 0;      // 时间窗 s（0 = 用默认 60）
    QString lineColor;              // 曲线颜色 CSS
    double lineWidth = 0;           // 曲线粗细（0 = 用默认 1.5）
    int refreshRateMs = 0;          // 刷新率 ms（0 = 用默认 500）
    // 历史记录 (19, P-5 2026-09-02)
    QStringList historyTags;         // HistoryView 变量列表（多变量，运行时切换）
    QString historyDbPath;           // HistoryView 数据库路径（空=设备端默认 navihmi_history.db）
    QStringList historyTagTitles;    // Q-6(2026-09-04): 各变量列显示名（平行 historyTags[i]；空=显示变量名——老工程兼容）
    // 窗口控件扩展 (P-5)
    int displayMode = 0;             // WindowWidget AlarmView 显示模式：0=当前报警 1=报警缓冲区（DisplayMode 权威）
    // Frame 视频 (P-6 2026-09-02)
    bool showVideo = false;          // Frame 视频模式（checkbox——true=视频；false/无源=普通 Frame）
    QString videoSource;             // Frame 视频源（本地路径打包 / rtsp:// URL 不入包）
    // R-4 (2026-09-05 用户 Check): Frame 播放控制变量（布尔驱动播放/暂停；空=未绑定）
    QString playTag;                 // true=播放 false=暂停（点击控件翻转并写回）
    // S-4/S-5 (2026-09-05 用户拍板): Frame 视频源列表（Video 型列表）+ 视频源选择整型变量
    QString videoListRef;            // 视频源列表名（空=未选走单源 videoSource）
    QString videoIndexTag;           // 视频源选择变量（整型非负——变量值=列表项索引切源；空=播首项/单源）
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
    QString deviceName;      // 关联设备名（空 = 无）
    QString group;           // 变量分组（Y-5 PC 契约 + Y-4 FW 解析透传——运行时不做分组逻辑）
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
    AlarmTriggerMode triggerMode = AlarmTriggerMode::Threshold;
    AlarmCategory category = AlarmCategory::User;
    int priority = 0;
    bool ackRequired = true;
    QString ackGroup;
    QString colorOverride;
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

// 经纬度点
struct GeoPoint {
    double longitude = 0;    // 经度（东正西负）
    double latitude = 0;     // 纬度（北正南负）
};

// 世界地图作业点
struct MapWorkPoint {
    QString name;
    GeoPoint fixedPoint;     // 固定经纬度（有 boundTag 时以变量为准）
    QString boundTag;        // 绑定 GPS 变量名（空 = 用 fixedPoint）
};

// 世界地图作业范围点（围栏顶点）
struct WorkRangePoint {
    GeoPoint fixedPoint;
    QString boundTag;
};

struct WorldMapConfig {
    double latMin = 0, latMax = 0, lngMin = 0, lngMax = 0;
    QString tileSource;
    int zoomLevel = 0;
    bool showGlobalOverlay = false;
    QList<MapWorkPoint> workPoints;          // 作业点（红点——X-5 颜色对齐 PC：作业点红/范围蓝）
    QList<WorkRangePoint> workRangePoints;   // 作业范围点（围栏顶点）
    QList<WidgetEvent> events;               // 点击切换画面事件（地图级）
    bool viewLocked = false;                 // 锁定预览
};

struct UserGroup {
    QString name;
    QList<int> permissions;   // UserPermission 值列表
};

struct UserAccount {
    QString userName;
    QString passwordHash;     // SHA256 Hex
    QString groupName;
    bool mustChangePassword = false;
};

struct SecuritySettings {
    int minPasswordLength = 6;
    bool requireDigit = false;
    bool requireLetter = false;
    bool requireUpperLower = false;
    bool requireSpecial = false;
    int passwordMaxAgeDays = 0;
    int failedLoginLockout = 0;
    int lockMinutes = 0;
};

// ═══════════════════════════════════════════════════════════════
// Y-2/Y-4 MQTT 三层映射运行时模型（2026-09-10 ④通信批；与 proto MqttSettings 对齐）
// 连接参数真源 = DeviceConfig MQTT connection_info JSON（PC 裁决——MqttConfig 承载 UI 规范化，
// FW MqttDriver 实际连接参数从 DeviceConfig JSON 键值来（Acquisition connInfoForDevice 展开）
// ═══════════════════════════════════════════════════════════════

enum class MqttTopicDirection { Publish = 0, Subscribe = 1 };
enum class MqttJsonTemplate { Kv = 0, KvWithTimestamp = 1 };

struct MqttTopicConfig {
    QString name;              // 配置名（工程内唯一，Binding 引用锚）
    MqttTopicDirection direction = MqttTopicDirection::Publish;
    QString topic;             // topic 路径
    int qos = 0;
    bool retain = false;
    int publishIntervalMs = 0; // 周期发布间隔（0 = 仅按需）
    MqttJsonTemplate jsonTemplate = MqttJsonTemplate::Kv;
    QString responseTopic;
};

struct MqttBindingConfig {
    QString topicName;         // 所属 MqttTopicConfig.name
    QString tagName;           // 变量名（订阅=写入目标，发布=数据源）
    QString fieldName;         // JSON 字段名
};

struct MqttSettings {
    bool enableMqtt = false;   // 总开关（禁用：FW 不建连接对象）
    int schemaVersion = 1;
    QList<MqttTopicConfig> topics;
    QList<MqttBindingConfig> bindings;
    bool hasMqttSettings = false;   // proto 有 mqtt_settings 消息（缺省=未配置——区分「未配置」与「配了但开关关」）
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
    int deviceWidth = 0;      // 设备宽（像素，proto 7）
    int deviceHeight = 0;     // 设备高（像素，proto 8）
    QString deviceModel;      // 工程目标设备型号（proto 22；PC 端写入。设备身份由设备自身配置决定，与工程无关——2026-08-30 用户 Check 指正）
    QList<UserAccount> users;
    QList<UserGroup> groups;
    SecuritySettings security;
    bool enableVnc = false;   // VNC 镜像开关（proto 21；默认关=零开销，无人车场景不启）
    MqttSettings mqtt;        // Y-2/Y-4：MQTT 三层映射（hasMqttSettings 区分未配置）

    /// 按名称查画面（找不到返回 nullptr）
    const Screen* ScreenByName(const QString& name) const;
    /// 按名称查变量（找不到返回 nullptr）
    const Tag* TagByName(const QString& name) const;
    /// 按名称查报警规则（找不到返回 nullptr）
    const AlarmRule* AlarmByName(const QString& name) const;
    /// 按「类型 + 名称」查列表（U-2 2026-09-06 三类型命名空间独立——跨类型同名用 type 区分；找不到返回 nullptr）
    const ListDef* ListByName(const QString& name, ListType type = ListType::Text) const;
    /// 启动画面（无则取第一个 Custom 画面）
    const Screen* StartScreenModel() const;
};

} // namespace navihmi
