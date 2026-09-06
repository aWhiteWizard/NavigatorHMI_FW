// B-4/F-1: HmiDateTime——日期时间控件组件
// F-1（2026-09-06 用户报告「DateTime 时间不变」修复）：
// ① 未绑定 → 按 dtFormat 1Hz 实时系统时间（Qt.formatDateTime 真按格式，原实现硬编码 yyyy-MM-dd 忽略 dtFormat）；
// ② 绑定 boundTag（DATETIME 变量）→ 显示 DataManager 实时值（1Hz 刷新 + Connections 即时，对齐 HmiNumericDisplay）；
// ③ dtText 不再参与显示（PC D2 起不再下发占位 Text；旧产物残留忽略 → 防死时间）。
import QtQuick 2.15

Text {
    id: root
    width: 160
    height: 30
    // 绑定 → DataManager 实时值；未绑定 → dtFormat 实时系统时间（formatNow 兜底非法/空格式）
    text: root.boundTag !== "" && dataManager !== undefined && dataManager !== null && dataManager.hasTag(root.boundTag)
          ? String(dataManager.value(root.boundTag))
          : root.formatNow()
    color: textColor !== "" ? textColor : "#000000"
    font.pixelSize: fontSize > 0 ? fontSize : 14
    font.family: fontFamily !== "" ? fontFamily : "sans-serif"
    horizontalAlignment: hAlign === "Center" ? Text.AlignHCenter
                      : hAlign === "Right" ? Text.AlignRight
                      : Text.AlignLeft
    verticalAlignment: Text.AlignVCenter

    property string objectName: ""
    property string textDecoration: "None"
    property string boundTag: ""
    property string dtText: ""     // 旧字段保留（序列化/旧产物兼容）；不再参与显示（F-1）
    property string dtFormat: "yyyy-MM-dd HH:mm:ss"
    property string textColor: ""
    property double fontSize: 14
    property string fontFamily: ""
    property string hAlign: "Center"
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""
    property string content: ""  // 并集字段容忍(生成器统一输出)
    property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
    property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
    property string imagePath: ""  // 并集字段容忍(生成器统一输出)
    property string stretchMode: ""  // 并集字段容忍(生成器统一输出)
    property string listRef: ""  // 并集字段容忍(生成器统一输出)
    property int defaultIndex: 0  // 并集字段容忍(生成器统一输出)
    property double value: 0  // 并集字段容忍(生成器统一输出)
    property double min: 0  // 并集字段容忍(生成器统一输出)
    property double max: 0  // 并集字段容忍(生成器统一输出)
    property string fillStyle: "Solid"  // 并集字段容忍(生成器统一输出)
    property bool isOn: false  // 并集字段容忍(生成器统一输出)
    property bool isChecked: false  // 并集字段容忍(生成器统一输出)
    property bool isReadOnly: false  // 并集字段容忍(生成器统一输出)
    property double x2: 0  // 并集字段容忍(生成器统一输出)
    property double y2: 0  // 并集字段容忍(生成器统一输出)
    property int windowType: 0  // 并集字段容忍(生成器统一输出)
    property string winTitle: ""  // 并集字段容忍(生成器统一输出)
    property bool showTitleBar: true  // 并集字段容忍(生成器统一输出)
    property bool showHistory: false  // 并集字段容忍(生成器统一输出)
    property string selectedTag: ""  // 并集字段容忍(生成器统一输出)
    property double cardWidth: 0  // 并集字段容忍(生成器统一输出)
    property double cardHeight: 0  // 并集字段容忍(生成器统一输出)
    property bool showUserName: false  // 并集字段容忍(生成器统一输出)
    property bool showRole: false  // 并集字段容忍(生成器统一输出)
    property bool showMode: false  // 并集字段容忍(生成器统一输出)
    property bool cardShowNumber: false  // 并集字段容忍(生成器统一输出)
    property bool cardShowStatus: false  // 并集字段容忍(生成器统一输出)
    property bool cardShowLocation: false  // 并集字段容忍(生成器统一输出)
    property string boundDevice: ""  // 并集字段容忍(生成器统一输出)
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    /// 生效格式：dtFormat 空/非法 → 默认 "yyyy-MM-dd HH:mm:ss" 兜底（防 Format 空致固定文本）
    property string effectiveFormat: {
        var f = root.dtFormat !== "" ? root.dtFormat : "yyyy-MM-dd HH:mm:ss"
        // 格式合法性轻校验：至少含一个格式符，否则回退默认（Qt.formatDateTime 遇未知符原样输出，不抛）
        if (f.indexOf("y") < 0 && f.indexOf("M") < 0 && f.indexOf("d") < 0
            && f.indexOf("H") < 0 && f.indexOf("h") < 0 && f.indexOf("m") < 0 && f.indexOf("s") < 0)
            return "yyyy-MM-dd HH:mm:ss"
        return f
    }

    function formatNow() {
        return Qt.formatDateTime(new Date(), root.effectiveFormat)
    }

    // 1Hz 实时刷新（未绑定系统时间 / 绑定 tag 值都经此重算——dataManager.value 无依赖跟踪需显式刷新；
    // Connections 提供绑定场景即时性，Timer 保底）
    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            root.text = root.boundTag !== "" && dataManager !== undefined && dataManager !== null
                        && dataManager.hasTag(root.boundTag)
                        ? String(dataManager.value(root.boundTag))
                        : root.formatNow()
            if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
        }
    }

    // 绑定变量变化 → 即时刷新（对齐 HmiNumericDisplay/HmiLabel 模式）
    Connections {
        target: dataManager !== undefined && dataManager !== null ? dataManager : null
        onValueChanged: function(tagName, value) {
            if (tagName === root.boundTag && root.boundTag !== "") {
                root.text = String(value)
                root.hmiValueChanged()
                if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
            }
        }
    }
}
