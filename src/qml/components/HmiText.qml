// B-4: HmiText——文本控件组件
// 契约: content 显示文本; 信号 hmiScreenLoad/hmiScreenUnload (画面事件)
import QtQuick 2.15

// AH 修复（2026-10-30）：PC 端 TextWidget 走 CreateTextContentTemplate，WrapWithBorder(bindFillBackground: true)
// ⇒ Border.Background = FillColor（默认 #EEEEEE）；而本组件根元素是 Text（不能自绘背景）⇒ 设备端无底色。
// 改为 Rectangle 根承载底色 + 内层 Text 承载文本（文本控件已下线，但旧工程里的 Text 控件仍会渲染）。
Rectangle {
    id: root
    width: 100
    height: 30
    color: root.fillColor !== "" ? root.fillColor : "transparent"

    Text {
        id: textItem
        anchors.fill: parent
        text: root.content
        // 前景自适应（对齐 PC SafeTextColorConverter）
        color: (root.textColor === "" || root.textColor === "Transparent" || root.textColor === root.fillColor) ? "#000000" : root.textColor
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

    property string objectName: ""
    property string boundTag: ""
    property string content: ""
    property string fontFamily: ""
    property double fontSize: 14
    property string fontWeight: "Normal"
    property string fontStyle: "Normal"
    property string textDecoration: "None"
    property string textColor: ""
    property string hAlign: "Left"
    // 通用字段（生成器并集输出, 组件容忍无关属性）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""
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

    signal hmiScreenLoad()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiScreenUnload()
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    Component.onCompleted: hmiScreenLoad()
    Component.onDestruction: hmiScreenUnload()
}
