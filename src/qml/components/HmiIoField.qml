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
    property string boundTag: ""
    property string content: ""
    property bool isReadOnly: false
    property string textColor: "#000000"
    property double fontSize: 14
    property string fontFamily: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0

    signal hmiInput()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiValueChanged()

    TextInput {
        id: input
        anchors.fill: parent
        anchors.margins: 6
        text: root.content
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
        readOnly: root.isReadOnly
        verticalAlignment: Text.AlignVCenter
        onAccepted: {
            root.content = text
            root.hmiInput()
        }
    }
}
