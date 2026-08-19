// B-4: HmiTextList——文本列表控件组件（content 逗号分隔项）
import QtQuick 2.15

Rectangle {
    id: root
    width: 120
    height: 60
    color: fillColor !== "" ? fillColor : "#FFFFFF"
    border.color: strokeColor !== "" ? strokeColor : "#CCCCCC"
    border.width: strokeThickness > 0 ? strokeThickness : 1

    property string objectName: ""
    property string boundTag: ""
    property string content: ""
    property int defaultIndex: 0
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#000000"
    property double fontSize: 13

    signal hmiValueChanged()

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 2
        clip: true
        model: root.content.split(",")
        highlightFollowsCurrentItem: true
        currentIndex: root.defaultIndex
        delegate: Rectangle {
            width: list.width
            height: 20
            color: list.currentIndex === index ? "#E3F2FD" : "transparent"
            Text {
                anchors.fill: parent
                anchors.margins: 4
                text: modelData
                color: root.textColor
                font.pixelSize: root.fontSize
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    list.currentIndex = index
                    root.hmiValueChanged()
                }
            }
        }
    }
}
