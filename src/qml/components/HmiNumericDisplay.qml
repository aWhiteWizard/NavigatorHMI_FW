// B-5: HmiNumericDisplay——数值显示控件组件（绑变量显示 DataManager 实时值）
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    color: fillColor !== "" ? fillColor : "#F0F0F0"
    border.color: strokeColor !== "" ? strokeColor : "#CCCCCC"
    border.width: strokeThickness > 0 ? strokeThickness : 1
    radius: 2

    property string objectName: ""
    property string boundTag: ""
    property double value: 0
    property string fontFamily: ""
    property double fontSize: 16
    property string textColor: "#000000"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string hAlign: "Center"

    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()

    Text {
        id: numText
        anchors.fill: parent
        anchors.margins: 4
        // 绑定变量 → DataManager 实时值; 否则自身 value
        text: boundTag !== "" && dataManager !== undefined && dataManager !== null
              ? dataManager.value(boundTag) + ""
              : root.value.toFixed(2)
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
        font.bold: true
        horizontalAlignment: root.hAlign === "Left" ? Text.AlignLeft
                          : root.hAlign === "Right" ? Text.AlignRight
                          : Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    // DataManager 值变化 → 刷新（C++ 注入 context property）
    Connections {
        target: dataManager !== undefined && dataManager !== null ? dataManager : null
        onValueChanged: function(tagName, value) {
            if (tagName === root.boundTag)
                root.hmiValueChanged()
        }
    }
}
