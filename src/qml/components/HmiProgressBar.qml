// B-4: HmiProgressBar——进度条控件组件（value/min/max, 信号 hmiProgressComplete）
import QtQuick 2.15

Rectangle {
    id: root
    width: 160
    height: 30
    color: "#E0E0E0"
    radius: 3
    border.color: strokeColor !== "" ? strokeColor : "transparent"
    border.width: strokeThickness > 0 ? strokeThickness : 0

    property string objectName: ""
    property string boundTag: ""
    property double value: 0
    property double min: 0
    property double max: 100
    property string fillStyle: "Solid"
    property string fillColor: "#4CAF50"
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#000000"
    property double fontSize: 12

    signal hmiProgressComplete()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiValueChanged()

    property double progress: max > min ? Math.min(1.0, Math.max(0.0, (value - min) / (max - min))) : 0

    Rectangle {
        id: fill
        width: root.width * root.progress
        height: parent.height
        radius: 3
        color: root.fillColor
        clip: true
    }

    // 绑定变量 → DataManager 实时值作为进度
    property double boundValue: boundTag !== "" && dataManager !== undefined && dataManager !== null
              ? dataManager.value(boundTag).toDouble() : value
    property double progress: max > min ? Math.min(1.0, Math.max(0.0, (boundValue - min) / (max - min))) : 0

    Rectangle {
        id: fill
        width: root.width * root.progress
        height: parent.height
        radius: 3
        color: root.fillColor
        clip: true
    }

    Text {
        id: pctText
        anchors.fill: parent
        text: Math.round(root.progress * 100) + "%"
        color: root.textColor
        font.pixelSize: root.fontSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    onProgressChanged: {
        root.hmiValueChanged()
        if (progress >= 1.0) root.hmiProgressComplete()
    }

    // DataManager 值变化 → 刷新（C++ 注入 context property）
    Connections {
        target: dataManager !== undefined && dataManager !== null ? dataManager : null
        onValueChanged: function(tagName) {
            if (tagName === root.boundTag) {
                root.value = dataManager.value(tagName).toDouble()
            }
        }
    }
}
