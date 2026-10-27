// HmiPolygon——多边形控件组件（points 顶点坐标数组；B-4 引入，AG 2026-10-27 修正渲染坐标系）
//
// ⚠️ **坐标契约（必须遵守，违反即静默错位/不可见）**
//   · `points` 是**画面根坐标**（= PC 端"画布绝对坐标"，P7 语义：PolygonWidgetCreator 明写"绝对坐标，不再归一化"）
//   · `x/y/width/height` 是顶点**包围盒**（PC 派生，用于命中框/选中框）
//   · 本组件尺寸 = 包围盒，Canvas 原点在 Item 自身左上角 ⇒ 绘制前必须 `translate(-x, -y)` 把画面根坐标换到局部坐标
//   · **前提**：父项原点 == 画面根原点（生成器把控件平铺在 screenRoot / overlayRoot 下，二者都无 x/y 偏移）。
//     若将来引入容器/分组（控件 x/y 变成容器内局部坐标），则必须改成
//     `var o = root.mapToItem(screenRoot, 0, 0); ctx.translate(-o.x, -o.y)`，
//     否则会**重复减偏移**导致静默错位。
//
// 修前（AG 前）直接拿画面根坐标当局部坐标画 ⇒ 图形整体偏 (x,y)、随即越出 Item 被裁掉 ⇒ 设备端完全看不见；
// PC 画布用自己的模板按绝对坐标绘制所以 PC 正常 —— 这是该缺陷长期未被发现的原因。
import QtQuick 2.15

Item {
    id: root
    width: 100
    height: 100
    // 不做 clip：顶点必然落在包围盒边界上，描边以路径为中心外扩 strokeThickness/2，
    // clip 会把外侧半条边裁掉（厚描边时肉眼可见"缺一半边框"）。本组件无子项、绘制内容最多外溢半个线宽。

    property string objectName: ""
    property string textDecoration: "None"
    property string boundTag: ""
    property var points: []           // [{x,y}, ...]
    property string fillColor: "transparent"
    property string strokeColor: "#000000"
    // 默认 0 = 不画描边：与 PC 端语义对齐——生成器对 strokeThickness==0 **不输出该行**，
    // 若此处默认 1，则"用户把描边设为 0"的多边形在设备端仍会多出一条 1px 黑边（复审 CONFLICT_SOFT）
    property double strokeThickness: 0
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

    Canvas {
        id: canvas
        anchors.fill: parent
        // 复审 🔴1（实证修正）：描边外半条被裁的**不是** Item.clip，而是 **Canvas 自身纹理边界**
        //（实测：仅删 Item.clip 前后蓝色描边像素数完全相同 = 507）。正确修法是把 Canvas 向外扩 pad，
        //  并在 translate 里补回 pad —— 顶点才不会被画到纹理边界外。
        readonly property double pad: Math.max(root.strokeThickness, 0) / 2 + 1
        anchors.margins: -pad
        onPaint: {
            var ctx = getContext("2d")
            // ⚠️ 硬顺序：Qt 6 的 reset() 会复位变换矩阵与绘图状态**并清空 save/restore 栈** ⇒
            //   save()/translate() 必须在 reset() 之后；若先 save 后 reset，translate 被清掉
            //   ⇒ 图形回退成"画面根坐标当局部坐标"＝ AG 修前的不可见状态。
            ctx.reset()
            // 顶点合法性守卫（复审 🟡3）：顶点 <3 或含 NaN/Infinity 时 moveTo 会让整条路径失效
            // （Qt 静默忽略 ⇒ 图形消失且无报错）。对齐 HmiWorldMap 的 isNaN 守卫风格。
            if (!root.points || root.points.length < 3) return
            for (var k = 0; k < root.points.length; k++) {
                var pk = root.points[k]
                if (!pk || !isFinite(pk.x) || !isFinite(pk.y)) {
                    console.warn("[HmiPolygon] 非法顶点，跳过绘制 objectName=" + root.objectName + " idx=" + k)
                    return
                }
            }
            // AG（2026-10-27）：顶点是画面根坐标 ⇒ 平移到本 Canvas 局部坐标系再绘制。
            // Canvas 原点在 (root.x - pad, root.y - pad)（因 anchors.margins = -pad）⇒ 平移量 = -x + pad。
            ctx.save()
            ctx.translate(-root.x + pad, -root.y + pad)
            ctx.beginPath()
            ctx.moveTo(root.points[0].x, root.points[0].y)
            for (var i = 1; i < root.points.length; i++)
                ctx.lineTo(root.points[i].x, root.points[i].y)
            ctx.closePath()
            // 复审 🔴3：配色守卫对齐 HmiRectangle/HmiCircle（空值不喂给 Canvas）；
            // 描边仅在 strokeThickness > 0 时画 —— PC 端 0 表示"不画"（生成器对 0 不输出该行），
            // 而 Canvas 的 lineWidth=0 在 Qt 里是 1px hairline ⇒ 不加守卫会多出一条黑边。
            if (root.fillColor && root.fillColor !== "" && root.fillColor !== "transparent") {
                ctx.fillStyle = root.fillColor
                ctx.fill()
            }
            if (root.strokeColor && root.strokeColor !== "" && root.strokeThickness > 0) {
                ctx.strokeStyle = root.strokeColor
                ctx.lineWidth = root.strokeThickness
                ctx.stroke()
            }
            ctx.restore()
        }
    }

    onPointsChanged: canvas.requestPaint()
    // AG（2026-10-27）：translate 依赖 x/y ⇒ 位置变化必须重绘（生成器按 objectName→x→y→…→points 顺序赋值，
    // points 最后赋 ⇒ 正常路径由 onPointsChanged 收口；这两个 handler 是防属性顺序调整的廉价保险）。
    // ⚠️ width/height 变化无需手动重绘：Canvas anchors.fill 父项，尺寸变化时 Qt 自动触发重绘（复审 🟡1）。
    onXChanged: canvas.requestPaint()
    onYChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
