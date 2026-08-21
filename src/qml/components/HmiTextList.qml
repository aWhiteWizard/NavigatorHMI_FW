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
     property string textDecoration: "None"
    property string boundTag: ""
    property string content: ""
    property int defaultIndex: 0
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#000000"
    property double fontSize: 13
     property string text: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
     property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
     property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
     property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
     property string imagePath: ""  // 并集字段容忍(生成器统一输出)
     property string stretchMode: ""  // 并集字段容忍(生成器统一输出)
     property string listRef: ""  // 并集字段容忍(生成器统一输出)
     property double value: 0  // 并集字段容忍(生成器统一输出)
     property double min: 0  // 并集字段容忍(生成器统一输出)
     property double max: 0  // 并集字段容忍(生成器统一输出)
     property string fillStyle: "Solid"  // 并集字段容忍(生成器统一输出)
     property bool isOn: false  // 并集字段容忍(生成器统一输出)
     property bool isChecked: false  // 并集字段容忍(生成器统一输出)
     property bool isReadOnly: false  // 并集字段容忍(生成器统一输出)
     property double x2: 0  // 并集字段容忍(生成器统一输出)
     property double y2: 0  // 并集字段容忍(生成器统一输出)
     property string title: ""  // 并集字段容忍(生成器统一输出)
     property string dtText: ""  // 并集字段容忍(生成器统一输出)
     property string dtFormat: ""  // 并集字段容忍(生成器统一输出)
     property int windowType: 0  // 并集字段容忍(生成器统一输出)
     property string winTitle: ""  // 并集字段容忍(生成器统一输出)
     property bool showTitleBar: true  // 并集字段容忍(生成器统一输出)
     property bool showHistory: false  // 并集字段容忍(生成器统一输出)
     property string selectedTag: ""  // 并集字段容忍(生成器统一输出)
     property double cardWidth: 0  // 并集字段容忍(生成器统一输出)
     property double cardHeight: 0  // 并集字段容忍(生成器统一输出)
     property bool showUserName: false  // 并集字段容忍(生成器统一输出)
     property bool showRole: false  // 并集字段容忍(生成器统一输出)
     property bool showMode: false  // 并集字段容忍(生成器统一输出)
     property bool cardShowNumber: false  // 并集字段容忍(生成器统一输出)
     property bool cardShowStatus: false  // 并集字段容忍(生成器统一输出)
     property bool cardShowLocation: false  // 并集字段容忍(生成器统一输出)
     property string boundDevice: ""  // 并集字段容忍(生成器统一输出)

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
                    // R1: 交互写变量（状态持久化）
                    if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
                        dataManager.setValue(root.boundTag, index)
                    root.hmiValueChanged()
                }
            }
        }
    }

    // R1: 状态持久化——初始化 + 外部跟随
    Component.onCompleted: {
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) {
                var n = parseInt(String(v))
                if (!isNaN(n)) list.currentIndex = n
            }
        }
    }
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag) {
                var n = parseInt(String(value))
                if (!isNaN(n)) list.currentIndex = n
            }
        }
    }
}
