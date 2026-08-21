// B-4: HmiButton——按钮控件组件（生成器 HmiButton 引用）
// 契约: 属性 = 模型字段; 信号 hmiClicked/hmiPressed/hmiReleased (生成器事件占位映射)
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    color: fillColor !== "" ? fillColor : "#EEEEEE"
    border.color: strokeColor !== "" ? strokeColor : "#888888"
    border.width: strokeThickness > 0 ? strokeThickness : 1
    radius: 3

    // ── 模型属性（与生成器输出一致）──
    property string objectName: ""
    property string boundTag: ""
    property string text: "Button"
    property string fontFamily: ""
    property double fontSize: 16
    property string fontWeight: "Normal"
    property string fontStyle: "Normal"
    property string textDecoration: "None"
    property string textColor: "#000000"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
     property string content: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
     property string imagePath: ""  // 并集字段容忍(生成器统一输出)
     property string stretchMode: ""  // 并集字段容忍(生成器统一输出)
     property string listRef: ""  // 并集字段容忍(生成器统一输出)
     property int defaultIndex: 0  // 并集字段容忍(生成器统一输出)
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

    // ── 事件信号（生成器 onHmiClicked 等映射）──
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiScreenLoad()
    signal hmiScreenUnload()

    Text {
        id: label
        anchors.fill: parent
        anchors.margins: 4
        text: root.text
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
        font.bold: root.fontWeight === "Bold"
        font.italic: root.fontStyle === "Italic"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: {
            label.opacity = 0.8
            root.hmiClicked()
            label.opacity = 1.0
        }
        onPressed: {
            root.color = Qt.darker(root.color, 1.1)
            root.hmiPressed()
            if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
        }
        onReleased: {
            root.color = root.fillColor !== "" ? root.fillColor : "#EEEEEE"
            root.hmiReleased()
            if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
        }
    }
}
