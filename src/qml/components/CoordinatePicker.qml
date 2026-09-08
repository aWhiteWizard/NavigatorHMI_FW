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
        // 预填：initialText（"lng,lat" 十进制）→ 地图选点初始十字
        if (initialText !== undefined && initialText !== null && String(initialText).length > 0) {
            var parts = String(initialText).split(",")
            if (parts.length >= 2) {
                var l = parseFloat(parts[0]); var t = parseFloat(parts[1])
                if (!isNaN(l) && !isNaN(t)) { root.pickLng = l; root.pickLat = t; root.picked = true }
            }
        }
        root.visible = true
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
