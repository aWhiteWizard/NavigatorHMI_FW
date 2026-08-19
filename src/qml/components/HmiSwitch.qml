// B-4: HmiSwitch——开关控件组件（isOn/onText/offText, 信号 hmiOn/hmiOff）
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    radius: 5
    color: isOn ? "#4CAF50" : "#9E9E9E"
    border.color: strokeColor !== "" ? strokeColor : "transparent"
    border.width: strokeThickness > 0 ? strokeThickness : 0

    property string objectName: ""
    property string boundTag: ""
    property bool isOn: false
    property string onText: "开"
    property string offText: "关"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#FFFFFF"

    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiOn()
    signal hmiOff()

    Text {
        id: swText
        anchors.fill: parent
        text: root.isOn ? root.onText : root.offText
        color: root.textColor
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.isOn = !root.isOn
            root.hmiValueChanged()
            if (root.isOn) root.hmiOn(); else root.hmiOff()
        }
    }
}
