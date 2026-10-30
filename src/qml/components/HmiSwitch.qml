// B-4: HmiSwitch——开关控件组件（isOn/onText/offText, 信号 hmiOn/hmiOff）
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    radius: 5
    // AH 修复（2026-10-30）：PC 端 Switch 模板把 Button.Background 绑到 FillColor（默认 #EEEEEE），
    // 开关状态由 OnText/OffText 文本表达；FW 原来只用状态色、忽略 fillColor ⇒ 与 PC 不一致。
    // 现在：配置了 fillColor 就用它（对齐 PC），没配则保留原状态色（向后兼容，仍能一眼看出通断）。
    color: root.fillColor !== "" ? root.fillColor : (root.isOn ? "#4CAF50" : "#9E9E9E")
    border.color: strokeColor !== "" ? strokeColor : "transparent"
    border.width: strokeThickness > 0 ? strokeThickness : 0

    property string objectName: ""
    property string textDecoration: "None"
    property string boundTag: ""
    property bool isOn: false
    property string labelOn: "开"
    property string labelOff: "关"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#FFFFFF"
    property string text: ""  // 并集字段容忍(生成器统一输出)
    property string content: ""  // 并集字段容忍(生成器统一输出)
    property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
    property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
    property double fontSize: 0  // 并集字段容忍(生成器统一输出)
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
    property bool isChecked: false  // 并集字段容忍(生成器统一输出)
    property bool isReadOnly: false  // 并集字段容忍(生成器统一输出)
    property double x2: 0  // 并集字段容忍(生成器统一输出)
    property double y2: 0  // 并集字段容忍(生成器统一输出)
    property string title: ""  // 并集字段容忍(生成器统一输出)
    property string dtText: ""  // 并集字段容忍(生成器统一输出)
    property string dtFormat: ""  // 并集字段容忍(生成器统一输出)
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

    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiOn()
    signal hmiOff()
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiInput()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    Text {
        id: swText
        anchors.fill: parent
        text: root.isOn ? root.labelOn : root.labelOff
        // AH 修复（2026-10-30）：对齐 PC 的 SafeTextColorConverter 前景自适应
        color: (root.textColor === "" || root.textColor === "Transparent" || root.textColor === root.fillColor) ? "#000000" : root.textColor
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.isOn = !root.isOn
            // R1: 交互写变量（状态持久化——切画面重建后从变量恢复）
            if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
                dataManager.setValue(root.boundTag, root.isOn)
            root.hmiValueChanged()
            if (root.isOn) root.hmiOn(); else root.hmiOff()
            if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
        }
    }

    // R1: 状态持久化——加载时从变量初始化 + 外部变量变化跟随（setValue 防回环由 DataManager 同值跳过）
    Component.onCompleted: {
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) root.isOn = String(v) === "true" || String(v) === "1"
        }
    }
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag)
                root.isOn = String(value) === "true" || String(value) === "1"
        }
    }
}
