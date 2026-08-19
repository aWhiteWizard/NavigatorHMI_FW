// B-4: HmiFrame——框架控件组件（标题 + 边框）
import QtQuick 2.15

Rectangle {
    id: root
    width: 160
    height: 100
    color: fillColor !== "" ? fillColor : "transparent"
    border.color: strokeColor !== "" ? strokeColor : "#999999"
    border.width: strokeThickness > 0 ? strokeThickness : 1

    property string objectName: ""
    property string boundTag: ""
    property string title: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string titleColor: "#000000"
    property double fontSize: 13

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 22
        color: "#E0E0E0"
        visible: root.title !== ""

        Text {
            anchors.fill: parent
            anchors.margins: 4
            text: root.title
            color: root.titleColor
            font.pixelSize: root.fontSize
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
