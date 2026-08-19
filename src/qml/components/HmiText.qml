// B-4: HmiText——文本控件组件
// 契约: content 显示文本; 信号 hmiScreenLoad/hmiScreenUnload (画面事件)
import QtQuick 2.15

Text {
    id: root
    width: 100
    height: 30
    text: content
    color: textColor !== "" ? textColor : "#000000"
    font.pixelSize: fontSize > 0 ? fontSize : 14
    font.family: fontFamily !== "" ? fontFamily : "sans-serif"
    font.bold: fontWeight === "Bold"
    font.italic: fontStyle === "Italic"
    horizontalAlignment: hAlign === "Center" ? Text.AlignHCenter
                      : hAlign === "Right" ? Text.AlignRight
                      : Text.AlignLeft
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight

    property string objectName: ""
    property string boundTag: ""
    property string content: ""
    property string fontFamily: ""
    property double fontSize: 14
    property string fontWeight: "Normal"
    property string fontStyle: "Normal"
    property string textDecoration: "None"
    property string textColor: ""
    property string hAlign: "Left"
    // 通用字段（生成器并集输出, 组件容忍无关属性）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""

    signal hmiScreenLoad()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiScreenUnload()

    Component.onCompleted: hmiScreenLoad()
    Component.onDestruction: hmiScreenUnload()
}
