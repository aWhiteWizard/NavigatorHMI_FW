// B-4: HmiIoField——输入框控件组件（绑变量, 信号 hmiInput）
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    color: fillColor !== "" ? fillColor : "#FFFFFF"
    border.color: strokeColor !== "" ? strokeColor : "#999999"
    border.width: strokeThickness > 0 ? strokeThickness : 1
    radius: 2

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property string content: ""
    property bool isReadOnly: false
    // D+ 审查修复(2b3d04bd): 生成器按 boundTag 类型输出 inputMethodHints(整型→数字/浮点坐标→数字+小数点/字符串→全键盘)
    property int inputMethodHints: 0
    // 用户 2026-08-22: 布尔变量 → 双按钮选择(开/关), 非文本输入; 生成器对 Bool tag 输出 true
    property bool isBoolean: false
    // 用户 2026-08-22: 点击空白不改用户已输入内容——编辑中标记, 变量回写不覆盖用户输入(提交后恢复跟随)
    property bool editing: false
    property string textColor: "#000000"
    property double fontSize: 14
    property string fontFamily: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
     property string text: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
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

    signal hmiInput()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiValueChanged()
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // ── 布尔模式（isBoolean=true）: 显示当前值(0/1), 点击弹二值键盘(0/1 两键) ──
    // 用户 2026-08-22 澄清: 控件保持显示, 弹出的"键盘"只有 0/1 两个键(非控件变双按钮)
    property var contentRoot: root.Window ? root.Window.contentItem : null  // 声明处缓存(JS handler 用)

    Text {
        id: boolText
        visible: root.isBoolean
        anchors.fill: parent
        anchors.margins: 6
        text: root.isOn ? "1" : "0"
        color: root.textColor
        font.pixelSize: root.fontSize
        font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    MouseArea {
        id: boolClick
        visible: root.isBoolean
        anchors.fill: parent
        onClicked: root.showBoolPad()
    }

    // ── 二值键盘（0/1 两键, 挂窗口 contentItem, 同 textlist 自建下拉模式）──
    Rectangle {
        id: boolPad
        visible: false
        z: 10000
        width: 160
        height: 72
        color: "#F0F0F0"
        border.color: "#888888"
        border.width: 1
        radius: 6
        clip: true
        Row {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4
            Rectangle {
                width: (parent.width - 4) / 2
                height: parent.height
                radius: 4
                color: !root.isOn ? "#D32F2F" : "#EEEEEE"
                border.color: "#999999"
                Text { anchors.centerIn: parent; text: "0"; color: !root.isOn ? "white" : "#333333"; font.pixelSize: 22; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.pickBoolean(false) }
            }
            Rectangle {
                width: (parent.width - 4) / 2
                height: parent.height
                radius: 4
                color: root.isOn ? "#4CAF50" : "#EEEEEE"
                border.color: "#999999"
                Text { anchors.centerIn: parent; text: "1"; color: root.isOn ? "white" : "#333333"; font.pixelSize: 22; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.pickBoolean(true) }
            }
        }
    }
    // 全屏关闭层（面板打开时挂 contentItem, 点外部关闭）
    MouseArea {
        id: boolDismiss
        visible: false
        z: 9999
        onClicked: root.hideBoolPad()
    }

    function showBoolPad() {
        if (!contentRoot) return
        boolPad.parent = contentRoot
        boolDismiss.parent = contentRoot
        boolDismiss.x = 0; boolDismiss.y = 0
        boolDismiss.width = contentRoot.width
        boolDismiss.height = contentRoot.height
        var p = root.mapToItem(null, 0, 0)
        boolPad.x = p.x
        boolPad.y = p.y + root.height
        if (boolPad.y + boolPad.height > contentRoot.height)
            boolPad.y = Math.max(0, p.y - boolPad.height)
        boolPad.x = Math.max(0, Math.min(boolPad.x, contentRoot.width - boolPad.width))
        boolPad.visible = true
        boolDismiss.visible = true
    }
    function hideBoolPad() {
        boolPad.visible = false
        boolDismiss.visible = false
    }
    Component.onDestruction: {
        hideBoolPad()
        if (boolPad.parent !== root) boolPad.parent = root
        if (boolDismiss.parent !== root) boolDismiss.parent = root
    }

    TextInput {
        id: input
        visible: !root.isBoolean
        anchors.fill: parent
        anchors.margins: 6
        text: root.content
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
        readOnly: root.isReadOnly
        inputMethodHints: root.inputMethodHints
        verticalAlignment: Text.AlignVCenter
        onTextEdited: root.editing = true  // 用户 2026-08-22: 用户编辑标记——变量回写不覆盖输入
        onAccepted: {
            root.content = text
            // R1: 输入提交写变量（状态持久化）
            if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
                dataManager.setValue(root.boundTag, root.content)
            root.hmiInput()
            root.editing = false  // 提交后恢复变量跟随
            // D+8: 回车确认 → 退出输入态 + 关键盘（用户: 回车/点击空白即关闭键盘; 提交仅回车触发）
            input.focus = false
            Qt.inputMethod.hide()
        }
    }

    // 二值键盘选择(0/1) → 写变量 + 关面板
    function pickBoolean(v) {
        root.isOn = v
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
            dataManager.setValue(root.boundTag, v)
        root.hideBoolPad()
        root.hmiValueChanged()
        root.hmiInput()
    }
    function boolFromValue(v) {
        return (v === true || String(v) === "true" || String(v) === "1")
    }

    // R1: 状态持久化——初始化 + 外部跟随（用户 2026-08-22: 编辑中(editing)不覆盖用户输入, 提交后恢复跟随）
    Component.onCompleted: {
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) {
                if (root.isBoolean) root.isOn = root.boolFromValue(v)
                else root.content = String(v)
            }
        }
    }
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag) {
                if (root.isBoolean) {
                    root.isOn = root.boolFromValue(value)
                } else if (!root.editing) {
                    root.content = String(value)
                    input.text = root.content  // 编辑未进行时同步显示（text 绑定可能已因用户编辑破坏）
                }
            }
        }
    }
}
