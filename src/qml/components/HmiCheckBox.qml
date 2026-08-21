// B-4: HmiCheckBox——复选框控件组件
import QtQuick 2.15

Rectangle {
    id: root
    width: 120
    height: 30
    color: "transparent"

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property bool isChecked: false
    property string text: "Check"
    property string textColor: "#000000"
    property double fontSize: 14
    property string fontFamily: ""
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""
     property string content: ""  // 并集字段容忍(生成器统一输出)
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
     property bool isReadOnly: false  // 并集字段容忍(生成器统一输出)
     property double x2: 0  // 并集字段容忍(生成器统一输出)
     property double y2: 0  // 并集字段容忍(生成器统一输出)
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

    signal hmiClicked()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiValueChanged()

    Rectangle {
        id: box
        width: 18
        height: 18
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        border.color: "#666666"
        border.width: 1
        radius: 3
        color: root.isChecked ? "#2196F3" : "#FFFFFF"

        Text {
            anchors.centerIn: parent
            text: "✓"
            color: "white"
            font.pixelSize: 12
            visible: root.isChecked
        }
    }

    Text {
        id: label
        anchors.left: box.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.isChecked = !root.isChecked
            // R1: 交互写变量（状态持久化）
            if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
                dataManager.setValue(root.boundTag, root.isChecked)
            root.hmiClicked()
            root.hmiValueChanged()
        }
    }

    // R1: 状态持久化——初始化 + 外部跟随
    Component.onCompleted: {
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) root.isChecked = String(v) === "true" || String(v) === "1"
        }
    }
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag)
                root.isChecked = String(value) === "true" || String(value) === "1"
        }
    }
}
