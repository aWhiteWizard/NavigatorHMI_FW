// B-4: HmiCheckBox——复选框控件组件
import QtQuick 2.15

Rectangle {
    id: root
    width: 120
    height: 30
    color: "transparent"

    property string objectName: ""
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
            root.hmiClicked()
            root.hmiValueChanged()
        }
    }
}
