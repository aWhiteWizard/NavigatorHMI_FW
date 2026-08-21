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
    // R3: 工程自带瓦片根目录（ZIP 工程包解压出的 tiles/, 内含 z/x/y.png）；空=用模拟底图
    property string tileBasePath: ""
    // runtimeBus 由 C++ setContextProperty 注入（不能声明同名 property 遮蔽）

    signal hmiClicked()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()

    // ── Web Mercator 换算 (与 PC 端 MapViewportMath 一致) ──
    readonly property double earthRadius: 6378137.0
    function mercX(lng) { return lng * earthRadius * Math.PI / 180.0 }
    function mercY(lat) { return earthRadius * Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360)) }

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

    // ── B6-9: 作业点坐标——boundTag 非空时取 DataManager 变量值（"lng,lat"，DMS 或十进制），否则 fixedPoint ──
    // DMS 例: "(E104°8'32.28\", N30°37'45.84\")" → 104.1423 / 30.6294; W/S 为负
    function dmsToDec(s) {
        s = s.trim()
        var neg = (s.indexOf("W") >= 0 || s.indexOf("S") >= 0)
        var m = s.match(/([0-9.]+)°([0-9.]+)'([0-9.]+)"/)
        if (m) {
            var v = parseFloat(m[1]) + parseFloat(m[2]) / 60 + parseFloat(m[3]) / 3600
            return neg ? -v : v
        }
        return parseFloat(s)   // 兜底十进制
    }
    function pointLng(p) {
        if (p.boundTag && p.boundTag !== "" && dataManager && dataManager.hasTag(p.boundTag)) {
            var parts = String(dataManager.value(p.boundTag)).split(",")
            if (parts.length >= 2) {
                var v = root.dmsToDec(parts[0])
                if (!isNaN(v)) return v
            }
        }
        return p.lng
    }
    function pointLat(p) {
        if (p.boundTag && p.boundTag !== "" && dataManager && dataManager.hasTag(p.boundTag)) {
            var parts = String(dataManager.value(p.boundTag)).split(",")
            if (parts.length >= 2) {
                var v = root.dmsToDec(parts[1])
                if (!isNaN(v)) return v
            }
        }
        return p.lat
    }

    // ── R3: 工程自带瓦片层（ZIP 工程包解压出的 tiles/ 目录; 空则用下方模拟底图）──
    // 瓦片为 Web Mercator z/x/y.png, 按当前 bounds+zoom 计算可见瓦片范围, 逐片 Image 铺贴
    Item {
        id: tileLayer
        anchors.fill: parent
        visible: root.tileBasePath !== ""

        // Web Mercator 瓦片坐标（标准公式, 与下载脚本一致）
        function tileX(lng, z) { return Math.floor((lng + 180.0) / 360.0 * Math.pow(2, z)) }
        function tileY(lat, z) {
            var r = lat * Math.PI / 180.0
            return Math.floor((1.0 - Math.log(Math.tan(r) + 1.0 / Math.cos(r)) / Math.PI) / 2.0 * Math.pow(2, z))
        }
        // 该瓦片在屏幕上的位置（世界坐标 → 视口偏移）
        function tileScreenX(tx, z) {
            var world = tx / Math.pow(2, z) * 2 * Math.PI * root.earthRadius
            return (world - root.viewMinX) / root.resolution
        }
        function tileScreenY(ty, z) {
            var world = ty / Math.pow(2, z) * 2 * Math.PI * root.earthRadius
            return (root.viewMinY - world) / root.resolution + root.height
        }
        function tilePixelSize(z) {
            // 单瓦片 256px; 世界宽 2^z*256 → 每瓦片世界米
            var worldPerTile = 2 * Math.PI * root.earthRadius / Math.pow(2, z)
            return worldPerTile / root.resolution
        }

        // 可见瓦片集合（按 zoomLevel 取整层; 动态重算）
        property var tiles: []
        function rebuildTiles() {
            if (root.tileBasePath === "") { tiles = []; return }
            var z = root.zoomLevel
            if (z < 1) z = 1
            var list = []
            var tx0 = tileX(root.lngMin, z), tx1 = tileX(root.lngMax, z)
            var ty0 = tileY(root.latMax, z), ty1 = tileY(root.latMin, z)   // y 北小南大
            for (var tx = tx0; tx <= tx1; tx++) {
                for (var ty = ty0; ty <= ty1; ty++) {
                    list.push({ z: z, x: tx, y: ty })
                }
            }
            tiles = list
        }
        // tileBasePath 在生成 QML 时固定（ZIP 解压路径不变），onCompleted 一次重建即可；
        // reload 替换工程时 main.qml 重建整屏 → 组件重新实例化 → onCompleted 再跑
        Component.onCompleted: rebuildTiles()

        Repeater {
            model: tileLayer.tiles
            delegate: Image {
                x: tileLayer.tileScreenX(modelData.x, modelData.z)
                y: tileLayer.tileScreenY(modelData.y, modelData.z)
                width: tileLayer.tilePixelSize(modelData.z)
                height: tileLayer.tilePixelSize(modelData.z)
                source: "file://" + root.tileBasePath + "/" + modelData.z + "/" + modelData.x + "/" + modelData.y + ".png"
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    // ── 地图底图（网格 + 边界, 模拟瓦片; 有工程瓦片时底色/网格被瓦片覆盖）──
    Canvas {
        id: mapCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            // 有工程瓦片时：不画蓝底/网格（真实地图已覆盖），仅保留地名/河流/环线标注
            if (root.tileBasePath === "") {
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
                { name: "东站", lng: 104.1423, lat: 30.6294 },
                { name: "双流机场", lng: 103.9471, lat: 30.5785 },
                { name: "成都北站", lng: 104.0727, lat: 30.7000 },
                { name: "锦江区", lng: 104.0833, lat: 30.6500 },
                { name: "青羊区", lng: 104.0556, lat: 30.6747 },
                { name: "武侯区", lng: 104.0433, lat: 30.6417 }
            ]
            for (var i = 0; i < labels.length; i++) {
                var lx = root.toScreenX(labels[i].lng)
                var ly = root.toScreenY(labels[i].lat)
                if (lx > 10 && lx < width - 10 && ly > 10 && ly < height - 10) {
                    ctx.fillText(labels[i].name, lx + 6, ly - 4)
                }
            }
            // 锦江/府河示意（成都母亲河, 简化为折线）
            ctx.strokeStyle = "#7EC8E3"
            ctx.lineWidth = 3
            ctx.beginPath()
            var river = [
                { lng: 104.0280, lat: 30.6950 },  // 西北
                { lng: 104.0500, lat: 30.6780 },
                { lng: 104.0657, lat: 30.6570 },  // 天府广场
                { lng: 104.0800, lat: 30.6400 },
                { lng: 104.0980, lat: 30.6250 },  // 东南
                { lng: 104.1150, lat: 30.6080 }
            ]
            for (var r = 0; r < river.length; r++) {
                var rx = root.toScreenX(river[r].lng)
                var ry = root.toScreenY(river[r].lat)
                if (r === 0) ctx.moveTo(rx, ry); else ctx.lineTo(rx, ry)
            }
            ctx.stroke()
            // 环线示意（一环/二环, 简化为同心椭圆）
            ctx.strokeStyle = "#F5B041"
            ctx.lineWidth = 1.5
            ctx.setLineDash([6, 4])
            var centers = [
                { lng: 104.0667, lat: 30.6570, dx: 0.012, dy: 0.012 },  // 一环
                { lng: 104.0667, lat: 30.6570, dx: 0.020, dy: 0.020 }   // 二环
            ]
            for (var c = 0; c < centers.length; c++) {
                ctx.beginPath()
                ctx.ellipse(root.toScreenX(centers[c].lng),
                            root.toScreenY(centers[c].lat),
                            Math.abs(root.toScreenX(centers[c].lng + centers[c].dx) - root.toScreenX(centers[c].lng)),
                            Math.abs(root.toScreenY(centers[c].lat + centers[c].dy) - root.toScreenY(centers[c].lat)),
                            0, 0, Math.PI * 2)
                ctx.stroke()
            }
            ctx.setLineDash([])
            }   // if (root.tileBasePath === "") — 有瓦片时不画蓝底/网格（标注仍在）
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
            x: root.toScreenX(root.pointLng(modelData)) - 8
            y: root.toScreenY(root.pointLat(modelData)) - 8
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
                    // B6-1: 裸 runtimeBus（context property 沿作用域链解析）——id 限定访问 root.runtimeBus 恒 undefined
                    if (runtimeBus)
                        runtimeBus.emitEvent("__worldmap__", 0)   // 地图级 onClick
                }
            }
        }
    }

    // ── 地图点击（空白处 → 地图级事件）──
    // B6-2: z:-1 置于作业点之下——否则后声明全屏 MouseArea 拦截作业点点击
    MouseArea {
        z: -1
        anchors.fill: parent
        onClicked: {
            if (root.viewLocked) return
            if (runtimeBus)
                runtimeBus.emitEvent("__worldmap__", 0)
        }
    }

    onWorkPointsChanged: {
        rangeCanvas.requestPaint()
        if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
    }
    onWorkRangeChanged: {
        rangeCanvas.requestPaint()
        if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
    }
    Component.onCompleted: { mapCanvas.requestPaint(); rangeCanvas.requestPaint() }
}
