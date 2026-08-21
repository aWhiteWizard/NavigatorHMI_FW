// B-5: HmiLabel——标签控件组件（绑变量显示实时值, DataManager 驱动）
// 契约: text = 固定文本; boundTag 绑定变量时显示 DataManager 实时值
// 修正(2026-08-20): Text 根重复声明内建 text 属性导致 "Property value set
//  multiple times" + Type unavailable；改为 Rectangle + Text 子元素（对齐 HmiButton）。
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 30
    color: "transparent"

    // ── 模型属性（与生成器输出一致）──
    property string objectName: ""
    property string boundTag: ""
    property string text: "Label"
    property string fontFamily: ""
    property double fontSize: 14
    property string fontWeight: "Normal"
    property string fontStyle: "Normal"
    property string textDecoration: "None"
    property string textColor: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string hAlign: "Left"
    property string title: ""
     property string content: ""  // 并集字段容忍(生成器统一输出)
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

    // ── 事件信号（生成器 onHmi* 映射）──
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    Text {
        id: label
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        // 绑变量 → DataManager 实时值; 否则固定文本
        text: root.boundTag !== "" && dataManager !== undefined && dataManager !== null
              ? dataManager.value(root.boundTag) + ""
              : root.text
        color: root.textColor !== "" ? root.textColor : "#000000"
        font.pixelSize: root.fontSize > 0 ? root.fontSize : 14
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
        font.bold: root.fontWeight === "Bold"
        font.italic: root.fontStyle === "Italic"
        horizontalAlignment: root.hAlign === "Center" ? Text.AlignHCenter
                            : root.hAlign === "Right" ? Text.AlignRight
                            : Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.hmiClicked()
        onPressed: root.hmiPressed()
        onReleased: root.hmiReleased()
    }

    // DataManager 值变化 → 刷新显示（C++ 注入 context property）
    Connections {
        target: dataManager !== undefined && dataManager !== null ? dataManager : null
        onValueChanged: function(tagName, value) {
            if (tagName === root.boundTag) {
                label.text = value + ""
                root.hmiValueChanged()
            }
        }
    }
}
