// B-5: HmiLine——线控件组件（x2/y2 = 终点绝对坐标, 起点 = 控件 x/y）
// 语义: 与 PC 端一致（x2/y2 是相对画面左上角的终点坐标, 非相对中心）
import QtQuick 2.15

Item {
    id: root
    width: 100
    height: 40
    clip: true

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property double x2: 100
    property double y2: 0
    property string strokeColor: "#000000"
    property double strokeThickness: 1
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string title: ""
     property string text: ""  // 并集字段容忍(生成器统一输出)
     property string content: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
     property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
     property double fontSize: 0  // 并集字段容忍(生成器统一输出)
     property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
     property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
     property string textColor: ""  // 并集字段容忍(生成器统一输出)
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

    // 起点 = 控件原点 (0,0), 终点 = (x2, y2) 相对控件; 线段从 (0,0) 到 (x2-x, y2-y) 局部坐标
    // 画布内直接用: 线从 (0,0) 画到 (dx, dy)
    readonly property double dx: root.x2 > 0 ? root.x2 : 0
    readonly property double dy: root.y2 > 0 ? root.y2 : 0

    Canvas {
        id: lineCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.strokeColor
            ctx.lineWidth = root.strokeThickness > 0 ? root.strokeThickness : 1
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(root.dx, root.dy)
            ctx.stroke()
        }
    }

    onDxChanged: lineCanvas.requestPaint()
    onDyChanged: lineCanvas.requestPaint()
    Component.onCompleted: lineCanvas.requestPaint()
}
