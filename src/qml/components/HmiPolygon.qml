// B-4: HmiPolygon——多边形控件组件（points 顶点坐标数组）
import QtQuick 2.15

Item {
    id: root
    width: 100
    height: 100
    clip: true

    property string objectName: ""
    property string boundTag: ""
    property var points: []           // [{x,y}, ...]
    property string fillColor: "transparent"
    property string strokeColor: "#000000"
    property double strokeThickness: 1

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            if (root.points.length < 3) return
            ctx.beginPath()
            ctx.moveTo(root.points[0].x, root.points[0].y)
            for (var i = 1; i < root.points.length; i++)
                ctx.lineTo(root.points[i].x, root.points[i].y)
            ctx.closePath()
            ctx.fillStyle = root.fillColor
            ctx.fill()
            ctx.strokeStyle = root.strokeColor
            ctx.lineWidth = root.strokeThickness
            ctx.stroke()
        }
    }

    onPointsChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
