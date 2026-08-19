// B-4: HmiRectangle——矩形控件组件
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 60
    color: fillColor !== "" ? fillColor : "transparent"
    border.color: strokeColor !== "" ? strokeColor : "transparent"
    border.width: strokeThickness > 0 ? strokeThickness : 0

    property string objectName: ""
    property string boundTag: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
}
