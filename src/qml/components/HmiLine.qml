// B-4: HmiLine——线控件组件（x2/y2 端点, 旋转实现）
import QtQuick 2.15

Item {
    id: root
    width: 100
    height: 40
    clip: true

    property string objectName: ""
    property string boundTag: ""
    property double x2: 100
    property double y2: 0
    property string strokeColor: "#000000"
    property double strokeThickness: 1
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string title: ""

    Rectangle {
        id: lineBody
        height: root.strokeThickness > 0 ? root.strokeThickness : 1
        color: root.strokeColor
        width: Math.sqrt(root.x2 * root.x2 + root.y2 * root.y2)
        anchors.centerIn: parent
        transform: Rotation {
            angle: Math.atan2(root.y2, root.x2) * 180 / Math.PI
        }
    }
}
