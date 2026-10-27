// B-5: HmiLine——线控件组件（x2/y2 = 终点绝对坐标, 起点 = 控件 x/y）
// 语义: 与 PC 端一致（x2/y2 是相对画面左上角的终点坐标, 非相对中心）
import QtQuick 2.15

Item {
    id: root
    width: 100
    height: 40
    // 不 clip：负方向线段（向左/向上）会画到 Item 边界之外，clip 会把它整段裁掉
    //（Item 尺寸 = PC 派生的 |dx|×|dy| 命中框，不含负方向外扩）

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
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // 坐标语义（AG 2026-10-27 修）：PC 端 LineWidget：X/Y = **起点**（第一点击点），
    //   X2/Y2 = 终点相对起点的**有符号**偏移（可为负——向左/向上拖拽），Width/Height = |dx|/|dy|（仅命中框）。
    //   本组件的 Item 就摆在起点，故线从局部 (0,0) 画到 (dx,dy)。
    //   ⚠️ 修前写的是 `x2 > 0 ? x2 : 0`——负偏移被截成 0 ⇒ 向左/向上的线段退化成零长点，设备端看不到
    //   （PC 端 WPF Line 用同一组正值/负值都画，所以 PC 正常）。
    readonly property double dx: root.x2
    readonly property double dy: root.y2
    // 描边外半条会被 Canvas 纹理边界裁掉（与 HmiPolygon 同一坑）⇒ 画布按"线段实际外接框 + pad"布局，
    // 并把绘制原点平移到画布内，使任意方向（含负 dx/dy）的线段都完整可见。
    readonly property double linePad: Math.max(root.strokeThickness, 0) / 2 + 1
    readonly property double lineMinX: Math.min(0, root.dx)
    readonly property double lineMinY: Math.min(0, root.dy)

    Canvas {
        id: lineCanvas
        x: root.lineMinX - root.linePad
        y: root.lineMinY - root.linePad
        width: Math.abs(root.dx) + 2 * root.linePad
        height: Math.abs(root.dy) + 2 * root.linePad
        onPaint: {
            var ctx = getContext("2d")
            // ⚠️ 硬顺序：Qt 6 的 reset() 复位变换/绘图状态并清空 save 栈 ⇒ save/translate 必须在 reset 之后
            ctx.reset()
            ctx.strokeStyle = root.strokeColor
            ctx.lineWidth = root.strokeThickness > 0 ? root.strokeThickness : 1
            ctx.save()
            ctx.translate(-root.lineMinX + root.linePad, -root.lineMinY + root.linePad)
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(root.dx, root.dy)
            ctx.stroke()
            ctx.restore()
        }
    }

    onDxChanged: lineCanvas.requestPaint()
    onDyChanged: lineCanvas.requestPaint()
    Component.onCompleted: lineCanvas.requestPaint()
}
