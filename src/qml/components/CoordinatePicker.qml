// X-2（2026-09-08 用户规格——GPS CoordinatePicker）：GPS 变量输入用地图选点器
// 窗口内显示工程世界地图底图（worldmap_bg.png，宿主注入 backgroundImage）+ 点击选点画十字 + 气泡 DMS 经纬度
// + 确定（写回 GPS tag）/ 取消（丢弃）。浮层生命周期/交互对齐 DateTimePicker 先例（contentRoot 挂载/点外不关/确定取消）。
// 映射：Web Mercator 归一 over 工程 bounds（lngMin..lngMax/latMin..latMax 由宿主注入——与画面 HmiWorldMap 同底图同坐标系）
import QtQuick 2.15
import QtQuick.Window 2.15

Item {
    id: root
    visible: false
    signal confirmed(string text)   // text = "lng,lat" 十进制（宿主写回 GPS tag——对齐现有 content 契约）

    // 声明处缓存（JS handler/函数内访问 Window attached 会 TypeError——HmiTextList/HmiImage 先例实证）
    property var contentRoot: root.Window ? root.Window.contentItem : null
    property var homeParent: parent          // 创建时宿主（HmiIoField 内）；onDestruction 恢复用

    // ── 地图上下文（宿主注入——qmlgenerator 给 GPS IO Field 生成时填）──
    property string backgroundImage: ""      // 工程世界地图底图绝对路径（worldmap_bg.png；空 = 无底图）
    property real lngMin: 0
    property real lngMax: 0
    property real latMin: 0
    property real latMax: 0
    property bool mapReady: root.backgroundImage !== "" && root.lngMax > root.lngMin && root.latMax > root.latMin
    // X Check（2026-09-10 用户）：叠加作业范围 + 作业点——固定点 "name,lng,lat"；绑变量点 "name,0,0,boundTag"
    property string gpsMapRange: ""
    property string gpsMapPoints: ""
    property var rangePts: []    // 解析后 [{lng,lat}]
    property var workPts: []     // [{name, lng, lat, boundTag}]——绑变量点坐标由 dataManager 动态刷新
    function coordDec(s) {
        // GPS tag 值坐标解析：十进制 "104.1423" 或 DMS "E104°3'30\""（W/S 负）——对齐 FW/HmiWorldMap 契约
        s = String(s).trim()
        var neg = (s.indexOf("W") >= 0 || s.indexOf("S") >= 0)
        var m = s.match(/([0-9.]+)°([0-9.]+)'([0-9.]+)"/)
        var v
        if (m) v = parseFloat(m[1]) + parseFloat(m[2]) / 60 + parseFloat(m[3]) / 3600
        else { v = parseFloat(s); if (isNaN(v)) return NaN }
        return neg ? -v : v
    }
    function parseGpsLayer() {
        root.rangePts = []
        if (root.gpsMapRange !== "") {
            var ps = String(root.gpsMapRange).split("|")
            for (var i = 0; i < ps.length; i++) {
                var c = ps[i].split(",")
                if (c.length >= 2) root.rangePts.push({ lng: parseFloat(c[0]), lat: parseFloat(c[1]) })
            }
        }
        root.workPts = []
        if (root.gpsMapPoints !== "") {
            ps = String(root.gpsMapPoints).split("|")
            for (i = 0; i < ps.length; i++) {
                c = ps[i].split(",")
                if (c.length >= 4)      // 绑变量点 "name,0,0,boundTag"——坐标运行时读（refreshDynamicPoints）
                    root.workPts.push({ name: c[0], lng: parseFloat(c[1]), lat: parseFloat(c[2]), boundTag: c[3] })
                else if (c.length >= 3) // 固定点 "name,lng,lat"
                    root.workPts.push({ name: c[0], lng: parseFloat(c[1]), lat: parseFloat(c[2]), boundTag: "" })
            }
        }
        root.refreshDynamicPoints()
    }
    // 绑变量作业点 → dataManager 读 GPS tag 动态坐标（值格式 "(E…, N…)" 括号 DMS 或 "lng,lat" 十进制）
    function refreshDynamicPoints() {
        if (typeof dataManager === "undefined" || dataManager === null || dataManager === undefined) return
        var changed = false
        for (var i = 0; i < root.workPts.length; i++) {
            var p = root.workPts[i]
            if (p.boundTag === "" || p.boundTag === undefined) continue
            if (!dataManager.hasTag(p.boundTag)) continue
            var raw = dataManager.value(p.boundTag)
            if (raw === undefined || raw === null || String(raw).trim() === "") { if (p.lng !== 0 || p.lat !== 0) { p.lng = 0; p.lat = 0; changed = true } continue }
            var s = String(raw).trim()
            if (s.charAt(0) === "(" && s.charAt(s.length - 1) === ")") s = s.substring(1, s.length - 1).trim()
            var parts = s.split(/[,，]/)
            if (parts.length >= 2) {
                var lng = root.coordDec(parts[0]), lat = root.coordDec(parts[1])
                if (!isNaN(lng) && !isNaN(lat) && (lng !== p.lng || lat !== p.lat)) { p.lng = lng; p.lat = lat; changed = true }
            }
        }
        if (changed && layerCanvas) layerCanvas.requestPaint()
    }
    // 绑变量点动态刷新（GPS 值变化轮询——工程绑变量作业点移动实时体现）
    Timer {
        id: dynTimer
        interval: 500
        repeat: true
        running: root.visible && root.mapReady
        onTriggered: root.refreshDynamicPoints()
    }

    // 选点状态
    property real pickLng: 0
    property real pickLat: 0
    property bool picked: false

    readonly property real earthRadius: 6378137.0
    // 底图 = PC 合成（1024×600 画布，工程 bounds 外 +10% padding 每侧，Web Mercator center/resolution 口径——
    // 对齐 WorldMapScreenshotGenerator/HmiWorldMap，保证选点坐标与画面世界地图/底图影像重合；🔴-1 reviewer）
    readonly property real kPad: 0.1
    readonly property real bgW: 1024
    readonly property real bgH: 600
    readonly property real pbLngMin: root.lngMin - (root.lngMax - root.lngMin) * root.kPad
    readonly property real pbLngMax: root.lngMax + (root.lngMax - root.lngMin) * root.kPad
    readonly property real pbLatMin: root.latMin - (root.latMax - root.latMin) * root.kPad
    readonly property real pbLatMax: root.latMax + (root.latMax - root.latMin) * root.kPad
    function mercX(lng) { return (lng + 180.0) / 360.0 * 2 * Math.PI * root.earthRadius - Math.PI * root.earthRadius }
    function mercY(lat) { return Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360)) * root.earthRadius }
    readonly property real vMinX: root.mercX(root.pbLngMin)
    readonly property real vMaxX: root.mercX(root.pbLngMax)
    readonly property real vMinY: root.mercY(root.pbLatMin)
    readonly property real vMaxY: root.mercY(root.pbLatMax)
    readonly property real cX: (root.vMinX + root.vMaxX) / 2
    readonly property real cY: (root.vMinY + root.vMaxY) / 2
    readonly property real res: Math.max((root.vMaxX - root.vMinX) / (root.bgW * 0.9),
                                         (root.vMaxY - root.vMinY) / (root.bgH * 0.9))
    // 经纬度 → 底图像素（1024×600 基准）
    function mapX(lng) { return (root.mercX(lng) - root.cX) / root.res + root.bgW / 2 }
    function mapY(lat) { return root.bgH / 2 - (root.mercY(lat) - root.cY) / root.res }
    // 底图像素 → 组件地图区显示坐标（Stretch 等比？mapArea 固定——底图 Stretch 填满 → 像素线性缩放）
    function toX(lng) { return root.mapX(lng) / root.bgW * mapArea.width }
    function toY(lat) { return root.mapY(lat) / root.bgH * mapArea.height }
    // 组件坐标 → 经纬度（反函数）
    function screenLng(sx) {
        var px = sx / mapArea.width * root.bgW
        var v = (px - root.bgW / 2) * root.res + root.cX
        return v * 180.0 / (Math.PI * root.earthRadius)   // mercX 反（线性）
    }
    function screenLat(sy) {
        var py = sy / mapArea.height * root.bgH
        var v = root.cY - (py - root.bgH / 2) * root.res
        return Math.atan(Math.sinh(v / root.earthRadius)) * 180 / Math.PI
    }
    function pad(n) { return n < 10 ? "0" + n : "" + n }
    function dmsAbs(deg) {
        var a = Math.abs(deg)
        var d = Math.floor(a)
        var mf = (a - d) * 60
        var m = Math.floor(mf)
        var s = Math.round((mf - m) * 6000) / 100   // 秒 2 位小数
        if (s >= 60) { s = 0; m++ }
        if (m >= 60) { m = 0; d++ }
        return d + "°" + m + "'" + s.toFixed(2) + "\""
    }
    function bubbleText() {
        var lngEW = root.pickLng >= 0 ? "E" : "W"
        var latNS = root.pickLat >= 0 ? "N" : "S"
        return lngEW + root.dmsAbs(root.pickLng) + "  " + latNS + root.dmsAbs(root.pickLat)
    }

    function openPicker(initialText) {
        root.picked = false   // 🔵 reviewer：先清残留十字（content 被外部清空后重开不显示陈旧选点/确定不可写回）
        if (root.contentRoot) {
            root.parent = root.contentRoot
            root.width = root.contentRoot.width
            root.height = root.contentRoot.height
        }
        root.parseGpsLayer()   // X Check：解析作业范围/作业点叠加（注入在 openPicker 前由宿主设置）
        // 预填：initialText（"lng,lat" 十进制）→ 地图选点初始十字
        if (initialText !== undefined && initialText !== null && String(initialText).length > 0) {
            var parts = String(initialText).split(",")
            if (parts.length >= 2) {
                var l = parseFloat(parts[0]); var t = parseFloat(parts[1])
                if (!isNaN(l) && !isNaN(t)) { root.pickLng = l; root.pickLat = t; root.picked = true }
            }
        }
        root.visible = true
        if (layerCanvas) layerCanvas.requestPaint()   // 可见后重绘叠加层（范围/作业点）
    }
    function commitAndClose() {
        if (root.picked)
            root.confirmed(root.pickLng.toFixed(8) + "," + root.pickLat.toFixed(8))
        root.visible = false
    }
    function cancelAndClose() {
        root.visible = false   // 丢弃本次选点（不 confirmed）
    }

    // 浮层根被销毁（宿主画面切换/Stop/下载工程）时若仍挂窗口层 → 恢复宿主一并销毁（防孤儿残影——先例 onDestruction）
    Component.onDestruction: {
        if (root.parent === root.contentRoot && root.homeParent)
            root.parent = root.homeParent
    }

    // ── 全屏点外层：点外不关闭（只有 确定/取消 关闭——N+45 语义对齐 DateTimePicker）──
    MouseArea {
        anchors.fill: parent
        z: 9998
        // 无动作（吞点击防穿透）
    }

    // ── 面板（屏中 620×440——同 DateTimePicker 尺寸）──
    Rectangle {
        id: panel
        z: 10000
        width: 620
        height: 440
        radius: 8
        color: "#FFFFFF"
        border.color: "#1382B1"; border.width: 2
        anchors.centerIn: parent

        Text {
            anchors.top: parent.top; anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            text: "选择位置（点击地图选点）"
            font.pixelSize: 15; font.bold: true; color: "#333333"
        }

        // ── 地图区 ──
        Rectangle {
            id: mapArea
            anchors.top: parent.top; anchors.topMargin: 44
            anchors.left: parent.left; anchors.leftMargin: 14
            anchors.right: parent.right; anchors.rightMargin: 14
            anchors.bottom: parent.bottom; anchors.bottomMargin: 56
            radius: 4
            clip: true
            color: "#DEEBF7"
            border.color: "#CCCCCC"; border.width: 1

            Image {
                anchors.fill: parent
                source: root.mapReady ? "file://" + root.backgroundImage : ""
                fillMode: Image.Stretch
                visible: root.mapReady
            }
            // X Check（2026-09-10 用户）：作业范围多边形（蓝描边——X-5 范围色）+ 作业点（红点+名称——X-5 点色）叠加
            Canvas {
                id: layerCanvas
                anchors.fill: parent
                visible: root.mapReady
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    // 作业范围多边形
                    if (root.rangePts.length >= 3) {
                        ctx.beginPath()
                        var started = false
                        for (var i = 0; i < root.rangePts.length; i++) {
                            var px = root.toX(root.rangePts[i].lng)
                            var py = root.toY(root.rangePts[i].lat)
                            if (!started) { ctx.moveTo(px, py); started = true } else { ctx.lineTo(px, py) }
                        }
                        if (started) ctx.closePath()
                        ctx.strokeStyle = "#1565C0"
                        ctx.lineWidth = 2.5
                        ctx.stroke()
                    }
                    // 作业点（红点 + 名称；0,0 无值点跳过——绑变量点未取到值不画）
                    for (i = 0; i < root.workPts.length; i++) {
                        if (root.workPts[i].lng === 0 && root.workPts[i].lat === 0) continue
                        px = root.toX(root.workPts[i].lng)
                        py = root.toY(root.workPts[i].lat)
                        ctx.beginPath()
                        ctx.arc(px, py, 5, 0, Math.PI * 2)
                        ctx.fillStyle = "#D32F2F"
                        ctx.fill()
                        ctx.strokeStyle = "white"
                        ctx.lineWidth = 1.5
                        ctx.stroke()
                        if (root.workPts[i].name !== "") {
                            ctx.font = "10px sans-serif"
                            ctx.fillStyle = "#B71C1C"
                            ctx.fillText(root.workPts[i].name, px + 7, py - 5)
                        }
                    }
                }
            }
            // 无底图兜底（X-4：正常工程被 PC 编译校验拦截——此处防手工部署绕过）
            Text {
                anchors.centerIn: parent
                text: "无地图底图（工程需含世界地图并划定作业范围）"
                font.pixelSize: 13
                color: "#999999"
                visible: !root.mapReady
            }

            // 十字线（选点标记）
            Item {
                visible: root.picked
                x: root.toX(root.pickLng) - 10
                y: root.toY(root.pickLat) - 10
                width: 20
                height: 20
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 2; height: parent.height; color: "#D32F2F" }
                Rectangle { anchors.verticalCenter: parent.verticalCenter; height: 2; width: parent.width; color: "#D32F2F" }
                Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: "#D32F2F" }
            }
            // 经纬度气泡（十字右上）
            Rectangle {
                visible: root.picked
                x: Math.min(root.toX(root.pickLng) + 12, mapArea.width - 150)
                y: Math.max(root.toY(root.pickLat) - 30, 2)
                width: 148
                height: 22
                radius: 4
                color: "#2A2A2A"
                Text {
                    anchors.centerIn: parent
                    text: root.bubbleText()
                    font.pixelSize: 11
                    color: "white"
                }
            }
            // 选点点击
            MouseArea {
                anchors.fill: parent
                enabled: root.mapReady
                onClicked: {
                    root.pickLng = root.screenLng(mouse.x)
                    root.pickLat = root.screenLat(mouse.y)
                    root.picked = true
                }
            }
        }

        // ── 底部：取消 / 确定（点外不关）──
        Row {
            anchors.bottom: parent.bottom; anchors.bottomMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16
            Rectangle { width: 100; height: 36; radius: 6; color: "#F5F5F5"; border.color: "#CCCCCC"; border.width: 1
                Text { anchors.centerIn: parent; text: "取消"; color: "#666666"; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: root.cancelAndClose() } }
            Rectangle { width: 100; height: 36; radius: 6; color: "#1382B1"
                Text { anchors.centerIn: parent; text: "确定"; color: "white"; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; enabled: root.picked; onClicked: root.commitAndClose() } }
        }
    }
}
