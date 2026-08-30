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
    property double latMin: 0   // 2026-08-30：默认 0（与生成器"未配置=全 0"占位口径一致；成都兜底仅在 computeBounds 无点时）
    property double latMax: 0
    property double lngMin: 0
    property double lngMax: 0
    property int zoomLevel: 12   // 默认瓦片缩放级别（工程可覆盖；L140 取瓦片用）
    property bool showGlobalOverlay: false
    property bool viewLocked: false
    property var workPoints: []        // [{name, lng, lat, boundTag}]
    property var workRange: []         // [{lng, lat, boundTag}]
    // R3: 工程自带瓦片根目录（ZIP 工程包解压出的 tiles/, 内含 z/x/y.png）；空=用模拟底图
    property string tileBasePath: ""
    // N-1（2026-08-30 用户定方案替代瓦片铺贴）：锁定视角底图（PC 编译时按 FW computeBounds 同口径视口拼好的单张 PNG，
    // 随工程包下发 worldmap_bg.png）——设备端不缩放，显示底图 + 叠加作业点/范围点；有底图时瓦片层/模拟底图隐藏
    property string backgroundImage: ""
    // runtimeBus 由 C++ setContextProperty 注入（不能声明同名 property 遮蔽）

    signal hmiClicked()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // ── Web Mercator 换算 (与 PC 端 MapViewportMath 一致) ──
    readonly property double earthRadius: 6378137.0
    function mercX(lng) { return lng * earthRadius * Math.PI / 180.0 }
    function mercY(lat) { return earthRadius * Math.log(Math.tan(Math.PI / 4 + lat * Math.PI / 360)) }

    // ── 视口 bounds：工程未配置（全 0）时按作业点/范围点包围盒自适应（2026-08-30 用户 Check 修复：
    //   用户工程未配置范围 → 原兜底成都视口错位、作业范围显示不出来）──
    function computeBounds() {
        // 工程配置了有效范围（latMax>latMin && lngMax>lngMin）→ 直接用；部分配置（仅某值 0）也视为未配置 → 自适应（审查 ⚪）
        if (root.latMax > root.latMin && root.lngMax > root.lngMin)
            return { latMin: root.latMin, latMax: root.latMax, lngMin: root.lngMin, lngMax: root.lngMax }
        // 未配置 → 作业点 + 范围点包围盒（含 boundTag 解析）
        var minLng = 1e9, maxLng = -1e9, minLat = 1e9, maxLat = -1e9
        var i, p, lng, lat
        for (i = 0; i < root.workPoints.length; i++) {
            p = root.workPoints[i]
            lng = root.pointLng(p); lat = root.pointLat(p)
            if (isNaN(lng) || isNaN(lat)) continue
            if (lng === 0 && lat === 0) continue   // 空坐标点跳过（未配置固定值且变量无值）
            minLng = Math.min(minLng, lng); maxLng = Math.max(maxLng, lng)
            minLat = Math.min(minLat, lat); maxLat = Math.max(maxLat, lat)
        }
        for (i = 0; i < root.workRange.length; i++) {
            p = root.workRange[i]
            lng = root.pointLng(p); lat = root.pointLat(p)   // N-5：范围点与作业点同路径（boundTag 解析，未绑回退固定值）
            if (isNaN(lng) || isNaN(lat) || (lng === 0 && lat === 0)) continue
            minLng = Math.min(minLng, lng); maxLng = Math.max(maxLng, lng)
            minLat = Math.min(minLat, lat); maxLat = Math.max(maxLat, lat)
        }
        if (minLng > maxLng || minLat > maxLat)
            return { latMin: 30.55, latMax: 30.72, lngMin: 103.90, lngMax: 104.15 }   // 无任何点 → 兜底成都
        // 最小跨度 + 10% padding（对齐 PC 端 TryFitWorldMapViewport）
        var spanLng = Math.max(maxLng - minLng, 0.01)
        var spanLat = Math.max(maxLat - minLat, 0.01)
        return {
            latMin: minLat - spanLat * 0.1, latMax: maxLat + spanLat * 0.1,
            lngMin: minLng - spanLng * 0.1, lngMax: maxLng + spanLng * 0.1
        }
    }
    // viewBounds 为**加载时一次性拟合**（readonly 绑定在组件完成期求值，生成器静态数组已赋值）：
    // boundTag 变量运行时值变化不重算视口；作业点/范围点同为加载时一次性快照（Q_INVOKABLE 取值无依赖跟踪，
    // N-5 审查修正表述——点不随变量运行时移动；如需跟随需 Connections/Q_PROPERTY NOTIFY 订阅，超本批范围）
    readonly property var viewBounds: root.computeBounds()

    // 显示范围 → 视口中心 + 分辨率（按 bounds 自适应，含 10% padding）
    readonly property double viewMinX: mercX(viewBounds.lngMin)
    readonly property double viewMaxX: mercX(viewBounds.lngMax)
    readonly property double viewMinY: mercY(viewBounds.latMin)
    readonly property double viewMaxY: mercY(viewBounds.latMax)
    readonly property double centerX: (viewMinX + viewMaxX) / 2
    readonly property double centerY: (viewMinY + viewMaxY) / 2
    readonly property double resolution: Math.max(
        (viewMaxX - viewMinX) / (width * 0.9),
        (viewMaxY - viewMinY) / (height * 0.9))

    // 经纬度 → 屏幕坐标
    function toScreenX(lng) { return (mercX(lng) - centerX) / resolution + width / 2 }
    function toScreenY(lat) { return (centerY - mercY(lat)) / resolution + height / 2 }
    // 屏幕坐标 → 经纬度（toScreenX/Y 反函数, 瓦片范围计算用）
    function screenToLng(sx) { return ((sx - width / 2) * resolution + centerX) * 180.0 / (earthRadius * Math.PI) }
    function screenToLat(sy) {
        var y = centerY - (sy - height / 2) * resolution
        return Math.atan(Math.sinh(y / earthRadius)) * 180.0 / Math.PI
    }

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

    // ── R3: 工程自带瓦片层（ZIP 工程包解压出的 tiles/ 目录; 空则用下方模拟底图；N-1：有锁定底图时隐藏）──
    // 瓦片为 Web Mercator z/x/y.png, 按当前 bounds+zoom 计算可见瓦片范围, 逐片 Image 铺贴
    Item {
        id: tileLayer
        anchors.fill: parent
        visible: root.tileBasePath !== "" && root.backgroundImage === ""

        // Web Mercator 瓦片坐标（标准公式, 与下载脚本一致）
        function tileX(lng, z) { return Math.floor((lng + 180.0) / 360.0 * Math.pow(2, z)) }
        function tileY(lat, z) {
            var r = lat * Math.PI / 180.0
            return Math.floor((1.0 - Math.log(Math.tan(r) + 1.0 / Math.cos(r)) / Math.PI) / 2.0 * Math.pow(2, z))
        }
        // 该瓦片在屏幕上的位置（世界坐标 → 视口偏移）
        // 标准 Web Mercator 瓦片世界坐标（x 范围 [0,2πR] 从 180°W 起）→ 转 root 的 mercX/mercY 系（±πR，0 经线为中心）
        // 平移：x 减 0.5 个世界宽；y 用 0.5- 翻转（瓦片 ty 北大南小，世界 y 北大南小）
        // 屏幕定位基准必须与 root.toScreenX/Y 一致（centerX/centerY + width/2），不能混用 viewMinX（D+ 修复错位）
        function tileScreenX(tx, z) {
            var world = (tx / Math.pow(2, z) - 0.5) * 2 * Math.PI * root.earthRadius
            return (world - root.centerX) / root.resolution + root.width / 2
        }
        function tileScreenY(ty, z) {
            var world = (0.5 - ty / Math.pow(2, z)) * 2 * Math.PI * root.earthRadius
            return (root.centerY - world) / root.resolution + root.height / 2
        }
        function tilePixelSize(z) {
            // 单瓦片 256px; 世界宽 2^z*256 → 每瓦片世界米
            var worldPerTile = 2 * Math.PI * root.earthRadius / Math.pow(2, z)
            return worldPerTile / root.resolution
        }

        // 可见瓦片集合（按屏幕四角经纬度反推, 覆盖全屏; 动态重算）
        property var tiles: []
        function rebuildTiles() {
            if (root.tileBasePath === "") { tiles = []; return }
            var z = root.zoomLevel
            if (z < 1) z = 1
            var list = []
            // 屏幕四角经纬度（视口实际覆盖范围, 比 bounds 更准确——bounds 只到 104.15,
            // 但视口 1024 宽对应更宽的经度范围, 只按 bounds 算会缺右列瓦片 → 右 1/3 灰）
            var lngLeft = root.screenToLng(0)
            var lngRight = root.screenToLng(root.width)
            var latTop = root.screenToLat(0)
            var latBottom = root.screenToLat(root.height)
            var tx0 = tileX(lngLeft, z), tx1 = tileX(lngRight, z)
            var ty0 = tileY(latTop, z), ty1 = tileY(latBottom, z)
            for (var tx = tx0; tx <= tx1; tx++) {
                for (var ty = ty0; ty <= ty1; ty++) {
                    list.push({ z: z, x: tx, y: ty })
                }
            }
            tiles = list
        }
        // tileBasePath 在生成 QML 时固定（ZIP 解压路径不变），onCompleted 一次重建即可；
        // reload 替换工程时 main.qml 重建整屏 → 组件重新实例化 → onCompleted 再跑
        Component.onCompleted: {
            rebuildTiles()
            if (tileLayer.tiles.length > 0) {
                // 瓦片层已重建（J-2 瓦片校验闭环：缺瓦片走工程级 qWarning + 模拟底图角标）
            }
        }

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

    // ── 锁定视角底图（N-1：PC 拼好的单张 PNG；有底图时显示，瓦片层/模拟底图隐藏）──
    Image {
        id: bgImage
        anchors.fill: parent
        source: root.backgroundImage !== "" ? "file://" + root.backgroundImage : ""
        visible: root.backgroundImage !== ""
        fillMode: Image.Stretch
        // z:0（与作业点/范围点同层，后声明在上；根 Rectangle color 最底，底图可覆盖）
        // N+18 审查：原 z:-2 修改与缓存清理协同上板——Qt 语义子项（含负 z）在父背景之上，
        // 底图不显示真因是 QML 磁盘缓存致 backgroundImage 注入失效（visible=false），z 序非主因；z:0 保留（与点层序更直观）
        z: 0
    }

    // ── 地图底图（网格 + 边界, 模拟瓦片; 有工程瓦片/锁定底图时底色/网格被覆盖）──
    Canvas {
        id: mapCanvas
        anchors.fill: parent
        visible: root.backgroundImage === ""   // N-1：有锁定底图时隐藏模拟底图
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            // 有工程瓦片时：不画蓝底/网格（真实地图已覆盖），仅保留地名/河流/环线标注
            if (root.tileBasePath === "") {
            // 背景
            ctx.fillStyle = "#DEEBF7"
            ctx.fillRect(0, 0, width, height)
            // 网格（每 0.05 度）——用 viewBounds（自适应视口）而非 root.lngMin/lngMax：
            // 工程未配置 bounds 时生成器输出 0 占位，若用 lngMin/lngMax 网格只画 0 度线（屏幕外）→ 自适应模式无网格（审查 🟡）
            ctx.strokeStyle = "#B0C4DE"
            ctx.lineWidth = 0.5
            var dLng = 0.05, dLat = 0.05
            for (var lng = Math.floor(root.viewBounds.lngMin / dLng) * dLng; lng <= root.viewBounds.lngMax; lng += dLng) {
                var sx = root.toScreenX(lng)
                if (sx < -50 || sx > width + 50) continue
                ctx.beginPath()
                ctx.moveTo(sx, 0); ctx.lineTo(sx, height)
                ctx.stroke()
            }
            for (var lat = Math.floor(root.viewBounds.latMin / dLat) * dLat; lat <= root.viewBounds.latMax; lat += dLat) {
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
            // 多边形（N-5：范围点复用 pointLng/pointLat——boundTag 解析，与作业点同路径）
            ctx.beginPath()
            var started = false
            for (var i = 0; i < root.workRange.length; i++) {
                var pl = root.pointLng(root.workRange[i]); var pt = root.pointLat(root.workRange[i])
                // N-5 复审：与 computeBounds 同守卫——解析后 (0,0)/isNaN 顶点跳过（未配置坐标不参与绘制，防退化多边形伪影）
                if (isNaN(pl) || isNaN(pt) || (pl === 0 && pt === 0)) continue
                var sx = root.toScreenX(pl)
                var sy = root.toScreenY(pt)
                if (!started) { ctx.moveTo(sx, sy); started = true } else { ctx.lineTo(sx, sy) }
            }
            if (started) ctx.closePath()
            ctx.fillStyle = "rgba(255, 0, 0, 0.08)"
            ctx.fill()
            ctx.strokeStyle = "#D32F2F"
            ctx.lineWidth = 2
            ctx.stroke()
            // 顶点红点
            ctx.fillStyle = "#D32F2F"
            for (var j = 0; j < root.workRange.length; j++) {
                var rl = root.pointLng(root.workRange[j]); var rt = root.pointLat(root.workRange[j])
                // N-5 复审：同守卫——(0,0)/isNaN 顶点不画红点（与多边形路径一致）
                if (isNaN(rl) || isNaN(rt) || (rl === 0 && rt === 0)) continue
                var rx = root.toScreenX(rl)
                var ry = root.toScreenY(rt)
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
                    // 审查 🟡：补全 4 参（显式空 payload/sourceScreen）——规避 QML 调用 C++ 槽默认参数不确定性
                    if (runtimeBus)
                        runtimeBus.emitEvent("__worldmap__", 0, "", "")   // 地图级 onClick
                }
            }
        }
    }

    // ── 地图点击（空白处 → 地图级事件）──
    // B6-2: z:-1 置于作业点之下——否则后声明全屏 MouseArea 拦截作业点点击
    // 2026-08-30 用户 Check 修复：移除 `if (root.viewLocked) return`——
    // ViewLocked（锁定预览）语义 = 禁平移/缩放，**不拦截点击切换事件**（PC 端 L1456 明示
    // "切换仅 FW 端执行"，设计态 ViewLocked 只短路平移；原 FW 实现连点击一起禁 → 用户点地图跳转失效）
    MouseArea {
        z: -1
        anchors.fill: parent
        onClicked: {
            if (runtimeBus)
                runtimeBus.emitEvent("__worldmap__", 0, "", "")   // 地图级 onClick
        }
    }

    // J-2: 无瓦片角标——工程无 tiles/ 数据且无锁定底图时右下角提示「模拟底图（无瓦片）」（视觉可见校验）
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 6
        anchors.bottomMargin: 4
        text: "模拟底图（无瓦片）"
        font.pixelSize: 9
        color: "#888888"
        visible: root.tileBasePath === "" && root.backgroundImage === ""
        z: 10
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
