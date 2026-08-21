// B-4: HmiSwitch——开关控件组件（isOn/onText/offText, 信号 hmiOn/hmiOff）
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    radius: 5
    color: isOn ? "#4CAF50" : "#9E9E9E"
    border.color: strokeColor !== "" ? strokeColor : "transparent"
    border.width: strokeThickness > 0 ? strokeThickness : 0

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property bool isOn: false
    property string labelOn: "开"
    property string labelOff: "关"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#FFFFFF"
     property string text: ""  // 并集字段容忍(生成器统一输出)
     property string content: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
     property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
     property double fontSize: 0  // 并集字段容忍(生成器统一输出)
     property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
     property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
     property string imagePath: ""  // 并集字段容忍(生成器统一输出)
     property string stretchMode: ""  // 并集字段容忍(生成器统一输出)
     property string listRef: ""  // 并集字段容忍(生成器统一输出)
     property int defaultIndex: 0  // 并集字段容忍(生成器统一输出)
     property double value: 0  // 并集字段容忍(生成器统一输出)
     property double min: 0  // 并集字段容忍(生成器统一输出)
     property double max: 0  // 并集字段容忍(生成器统一输出)
     property string fillStyle: "Solid"  // 并集字段容忍(生成器统一输出)
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
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiOn()
    signal hmiOff()

    Text {
        id: swText
        anchors.fill: parent
        text: root.isOn ? root.labelOn : root.labelOff
        color: root.textColor
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.isOn = !root.isOn
            root.hmiValueChanged()
            if (root.isOn) root.hmiOn(); else root.hmiOff()
            if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
        }
    }
}
