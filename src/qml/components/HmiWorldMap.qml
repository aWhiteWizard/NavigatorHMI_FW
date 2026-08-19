// B-4: HmiWorldMap——世界地图组件（作业点/范围点/Web Mercator 换算）
// 契约: latMin/latMax/lngMin/lngMax 显示范围; workPoints/workRange 作业点/范围点
//       runtimeBus.emitEvent("__worldmap__", 0) = 地图级 onClick → screen_switch
import QtQuick 2.15

Rectangle {
    id: root
    width: 1024
    height: 600
    color: "#DEEBF7"   // 地图底色（浅蓝）

    // ── 模型配置 ──
    property double latMin: 30.55
    property double latMax: 30.72
    property double lngMin: 103.90
    property double lngMax: 104.15
    property int zoomLevel: 12
    property bool showGlobalOverlay: false
    property bool viewLocked: false
    property var workPoints: []        // [{name, lng, lat, boundTag}]
    property var workRange: []         // [{lng, lat, boundTag}]
    // runtimeBus 由 C++ setContextProperty 注入（不能声明同名 property 遮蔽）

    signal hmiClicked()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()

    // ── Web Mercator 换算 (与 PC 端 MapViewportMath 一致) ──
    readonly property double R: 6378137.0
    function mercX(lng) { return lng * R * Math.PI / 180.0 }
    function mercY(lat) { return R * Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360)) }

    // 显示范围 → 视口中心 + 分辨率（按 bounds 自适应，含 10% padding）
    readonly property double viewMinX: mercX(lngMin)
    readonly property double viewMaxX: mercX(lngMax)
    readonly property double viewMinY: mercY(latMin)
    readonly property double viewMaxY: mercY(latMax)
    readonly property double centerX: (viewMinX + viewMaxX) / 2
    readonly property double centerY: (viewMinY + viewMaxY) / 2
    readonly property double resolution: Math.max(
        (viewMaxX - viewMinX) / (width * 0.9),
        (viewMaxY - viewMinY) / (height * 0.9))

    // 经纬度 → 屏幕坐标
    function toScreenX(lng) { return (mercX(lng) - centerX) / resolution + width / 2 }
    function toScreenY(lat) { return (centerY - mercY(lat)) / resolution + height / 2 }

    // ── 地图底图（网格 + 边界, 模拟瓦片）──
    Canvas {
        id: mapCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            // 背景
            ctx.fillStyle = "#DEEBF7"
            ctx.fillRect(0, 0, width, height)
            // 网格（每 0.05 度）
            ctx.strokeStyle = "#B0C4DE"
            ctx.lineWidth = 0.5
            var dLng = 0.05, dLat = 0.05
            for (var lng = Math.floor(root.lngMin / dLng) * dLng; lng <= root.lngMax; lng += dLng) {
                var sx = root.toScreenX(lng)
                if (sx < -50 || sx > width + 50) continue
                ctx.beginPath()
                ctx.moveTo(sx, 0); ctx.lineTo(sx, height)
                ctx.stroke()
            }
            for (var lat = Math.floor(root.latMin / dLat) * dLat; lat <= root.latMax; lat += dLat) {
                var sy = root.toScreenY(lat)
                if (sy < -50 || sy > height + 50) continue
                ctx.beginPath()
                ctx.moveTo(0, sy); ctx.lineTo(width, sy)
                ctx.stroke()
            }
            // 边界框
            ctx.strokeStyle = "#4682B4"
            ctx.lineWidth = 2
            ctx.strokeRect(1, 1, width - 2, height - 2)
            // 地名标注（成都周边参考点）
            ctx.fillStyle = "#555555"
            ctx.font = "11px sans-serif"
            var labels = [
                { name: "成都", lng: 104.0667, lat: 30.5728 },
                { name: "天府广场", lng: 104.0657, lat: 30.6570 },
                { name: "东站", lng: 104.1423, lat: 30.6294 }
            ]
            for (var i = 0; i < labels.length; i++) {
                var lx = root.toScreenX(labels[i].lng)
                var ly = root.toScreenY(labels[i].lat)
                if (lx > 10 && lx < width - 10 && ly > 10 && ly < height - 10) {
                    ctx.fillText(labels[i].name, lx + 6, ly - 4)
                }
            }
        }
    }

    // ── 作业范围（红点 + 多边形）──
    Canvas {
        id: rangeCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            if (root.workRange.length < 3) return
            // 多边形
            ctx.beginPath()
            for (var i = 0; i < root.workRange.length; i++) {
                var sx = root.toScreenX(root.workRange[i].lng)
                var sy = root.toScreenY(root.workRange[i].lat)
                if (i === 0) ctx.moveTo(sx, sy); else ctx.lineTo(sx, sy)
            }
            ctx.closePath()
            ctx.fillStyle = "rgba(255, 0, 0, 0.08)"
            ctx.fill()
            ctx.strokeStyle = "#D32F2F"
            ctx.lineWidth = 2
            ctx.stroke()
            // 顶点红点
            ctx.fillStyle = "#D32F2F"
            for (var j = 0; j < root.workRange.length; j++) {
                var rx = root.toScreenX(root.workRange[j].lng)
                var ry = root.toScreenY(root.workRange[j].lat)
                ctx.beginPath()
                ctx.arc(rx, ry, 5, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }

    // ── 作业点（蓝点 + 名称）──
    Repeater {
        model: root.workPoints
        delegate: Item {
            x: root.toScreenX(modelData.lng) - 8
            y: root.toScreenY(modelData.lat) - 8
            width: 16
            height: 16

            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "#1565C0"
                border.color: "white"
                border.width: 2
            }
            Text {
                anchors.top: parent.bottom
                anchors.topMargin: 2
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.name
                font.pixelSize: 10
                color: "#0D47A1"
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (root.runtimeBus)
                        root.runtimeBus.emitEvent("__worldmap__", 0)   // 地图级 onClick
                }
            }
        }
    }

    // ── 地图点击（空白处 → 地图级事件）──
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (root.viewLocked) return
            if (root.runtimeBus)
                root.runtimeBus.emitEvent("__worldmap__", 0)
        }
    }

    onWorkPointsChanged: rangeCanvas.requestPaint()
    onWorkRangeChanged: rangeCanvas.requestPaint()
    Component.onCompleted: { mapCanvas.requestPaint(); rangeCanvas.requestPaint() }
}
