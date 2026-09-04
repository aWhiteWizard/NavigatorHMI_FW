// P-4: HmiTrendView——趋势图控件（2026-09-02, v1.1-design §5.3 C1/C2）
// 时间-数据模式（trendMode=0）：单变量实时曲线——Connections dataManager.valueChanged 增量入环形缓冲（非轮询——F17）；
//   历史回放（点按「实时/历史」切换）：dataLogger.queryTagHistory(trendTagA, 500) 显示降采样历史。
// 变量A-B 模式（trendMode=1）：变量B 随变量A 变化的 X-Y 散点（时间戳对齐——A 变化时取 B 当前值画点）。
// 并集字段容忍：声明其它控件通用属性（生成器统一输出）防 Cannot assign；19 事件信号防生成器任意事件。
import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: 300
    height: 150
    color: "#FFFFFF"
    border.color: "#B0B8C4"
    border.width: 1
    radius: 2
    clip: true

    // ── 趋势专有属性（生成器输出）──
    property string objectName: ""
    property int trendMode: 0          // 0=时间-数据 1=变量A-B
    property string trendTagA: ""
    property string trendTagB: ""
    property int sampleIntervalMs: 1000
    property int timeWindowSeconds: 60
    property string lineColor: "#1E90FF"
    property double lineWidth: 1.5
    property int refreshRateMs: 500

    // ── 并集字段容忍（生成器统一输出，组件各自忽略）──
    property string boundTag: ""
    property string text: ""
    property string content: ""
    property string hAlign: "Left"
    property string fontFamily: ""
    property double fontSize: 12
    property string fontWeight: "Normal"
    property string fontStyle: "Normal"
    property string textDecoration: "None"
    property string textColor: "#000000"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string imagePath: ""
    property string stretchMode: ""
    property string listRef: ""
    property int defaultIndex: 0
    property double value: 0
    property double min: 0
    property double max: 100
    property string fillStyle: "Solid"
    property bool isOn: false
    property string labelOn: ""
    property string labelOff: ""
    property double x2: 0
    property double y2: 0
    property string title: ""
    property string dtText: ""
    property string dtFormat: ""
    property int windowType: 0
    property string winTitle: ""
    property bool showTitleBar: true
    property string selectedTag: ""
    property int cardWidth: 0
    property int cardHeight: 0
    property bool isChecked: false
    property bool isReadOnly: false
    property string boundDevice: ""

    // ── 19 事件信号（生成器任意事件防 Cannot assign）──
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // ── 数据缓冲 ──
    // 实时采样：{t(ms 单调时钟), v} 环形数组；XY 模式：{x, y}
    property var samples: []
    property var xyPoints: []
    property int maxSamples: Math.max(10, Math.round(timeWindowSeconds * 1000 / Math.max(sampleIntervalMs, 100)))
    property bool historyMode: false   // 运行时点按切换（用户 2026-09-02 拍板：模式设计态定死、历史回放运行时切换）
    property bool hasData: false

    // 标题
    Rectangle {
        id: titleBar
        height: 20
        width: parent.width
        color: "#F0F2F5"
        border.color: "#D0D6DE"
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: (historyMode ? "📜 历史" : "📈 实时")
                  + " " + root.trendTagA + (root.trendMode === 1 ? " ↔ " + root.trendTagB : "")
            font.pixelSize: 11
            color: "#444"
        }
    }
    // 实时/历史切换按钮（右上角；仅绑变量时历史可用）
    Button {
        id: modeBtn
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 2
        width: 44
        height: 16
        padding: 0
        text: historyMode ? "实时" : "历史"
        font.pixelSize: 9
        visible: root.trendTagA !== "" && root.trendMode === 0
        background: Rectangle { color: "#DDDDDD"; radius: 2 }
        onClicked: {
            if (!historyMode) {
                // 切历史：拉 DataLogger 降采样（500 点基线）
                historyMode = true
                samples = []
                if (dataLogger !== undefined && dataLogger !== null) {
                    var hist = dataLogger.queryTagHistory(root.trendTagA, 500)
                    for (var i = 0; i < hist.length; ++i) {
                        samples.push({ t: i, v: Number(hist[i].value) })
                    }
                    hasData = samples.length > 1   // 单点不画（防等分 0/0 NaN——审查 🟡）
                }
                markDirtyArea()
                chartCanvas.requestPaint()
            } else {
                historyMode = false
                samples = []          // 清空历史残留点（防 t<t0 负坐标参与实时 y 归一——审查 🟡）
                xyPoints = []
                hasData = false
                markDirtyArea()       // 对称标脏（VNC 端即时刷新——复审 🟡）
                chartCanvas.requestPaint()
            }
        }
    }

    // VNC 脏区标记（趋势每 tick 重绘须标脏，否则 VNC 客户端该区域不刷新——审查 🟡；HmiNumericDisplay 等值变化控件同惯例）
    function markDirtyArea() {
        if (vncMirror !== undefined && vncMirror !== null)
            vncMirror.markDirty(root.x, root.y, root.width, root.height)
    }

    // 重绘节流（refreshRateMs 合并高频变化请求——审查 🟡：值变化仅置位，按 refreshRateMs 合并重绘）
    property bool paintPending: false
    function requestThrottledPaint() {
        if (paintPending) return
        paintPending = true
        redrawTimer.restart()
    }
    Timer {
        id: redrawTimer
        interval: Math.max(50, root.refreshRateMs)
        repeat: false
        onTriggered: { root.paintPending = false; markDirtyArea(); chartCanvas.requestPaint() }
    }

    // 采样定时器：按 sampleIntervalMs 主动采样（时间-数据实时曲线的等间隔点；Connections 变化仅触发重绘——
    // 等间隔采样入样 + maxSamples 公式（window/sampleInterval）才自洽——审查 🟡 双写入修复）
    Timer {
        id: sampleTimer
        interval: Math.max(100, root.sampleIntervalMs)
        running: !root.historyMode && root.trendMode === 0 && root.trendTagA !== ""
        repeat: true
        onTriggered: {
            if (dataManager === undefined || dataManager === null) return
            var v = Number(dataManager.value(root.trendTagA))   // QVariant QML 解包为 JS 原生值——用 Number() 非 .toDouble()（审查 🔴）
            samples.push({ t: Date.now(), v: v })
            if (samples.length > root.maxSamples) samples.shift()
            hasData = samples.length > 1
            requestThrottledPaint()
        }
    }
    // 变量A 值变化 → 仅触发重绘（入样由 Timer 等间隔做——双写入已修；XY 模式：A 变化即时入 XY 点 + 取 B 当前值对齐时间戳）
    Connections {
        target: dataManager
        enabled: dataManager !== undefined && dataManager !== null && !root.historyMode
        function onValueChanged(tagName, newValue) {
            if (tagName !== root.trendTagA) return
            if (root.trendMode === 1) {
                // 变量A-B 散点：A 变化 → 取 B 当前值（若 B 绑同 tag 则用新值）
                var bv = root.trendTagB === tagName ? Number(newValue)
                         : (dataManager !== undefined && dataManager !== null ? Number(dataManager.value(root.trendTagB)) : 0)
                xyPoints.push({ x: Number(newValue), y: bv })
                if (xyPoints.length > root.maxSamples) xyPoints.shift()
                hasData = xyPoints.length > 1
            }
            requestThrottledPaint()
        }
    }

    // 曲线绘制
    Canvas {
        id: chartCanvas
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#FFFFFF"
            ctx.fillRect(0, 0, width, height)
            // Q-5③（2026-09-04）：坐标轴 + 网格——绘图区预留轴标签位（左 Y 刻度 / 下 X 刻度）
            var axL = 36, axR = 8, axT = 6, axB = 16
            var pw = width - axL - axR
            var ph = height - axT - axB
            // 轴/网格样式
            ctx.strokeStyle = "#D8DEE6"
            ctx.fillStyle = "#888888"
            ctx.font = "9px sans-serif"
            if (!root.hasData) {
                // 空数据：画空框 + 提示（轴刻度占位 0-100/时分占位）
                ctx.strokeStyle = "#E0E0E0"
                ctx.strokeRect(axL, axT, pw, ph)
                ctx.fillStyle = "#BBBBBB"
                ctx.font = "11px sans-serif"
                ctx.textAlign = "center"
                ctx.fillText("（无数据——未绑定变量或未开始采样）", width / 2, height / 2)
                return
            }
            // 值域（模式相关）
            var minV = Number.MAX_VALUE, maxV = -Number.MAX_VALUE, spanV = 1
            var minX = Number.MAX_VALUE, maxX = -Number.MAX_VALUE, spanX = 1
            var i
            if (root.trendMode === 1) {
                for (i = 0; i < root.xyPoints.length; ++i) {
                    var pp = root.xyPoints[i]
                    if (pp.x < minX) minX = pp.x
                    if (pp.x > maxX) maxX = pp.x
                }
                for (i = 0; i < root.xyPoints.length; ++i) {
                    var pp2 = root.xyPoints[i]
                    if (pp2.y < minV) minV = pp2.y
                    if (pp2.y > maxV) maxV = pp2.y
                }
                spanX = (maxX - minX) || 1
                spanV = (maxV - minV) || 1
            } else {
                for (i = 0; i < root.samples.length; ++i) {
                    var ss = root.samples[i]
                    if (ss.v < minV) minV = ss.v
                    if (ss.v > maxV) maxV = ss.v
                }
                spanV = (maxV - minV) || 1
            }
            // 画网格 + Y 轴刻度（4 分位，右对齐 axL 左侧）
            ctx.lineWidth = 1
            ctx.textAlign = "right"
            var yMax = root.trendMode === 1 ? maxV : maxV
            var yMin = root.trendMode === 1 ? minV : minV
            for (i = 0; i <= 4; ++i) {
                var frac = i / 4
                var gy = axT + ph - frac * ph
                ctx.strokeStyle = "#ECF0F4"
                ctx.beginPath()
                ctx.moveTo(axL, gy)
                ctx.lineTo(width - axR, gy)
                ctx.stroke()
                var val = yMin + frac * (yMax - yMin)
                ctx.fillStyle = "#777777"
                ctx.fillText(trimNum(val), axL - 4, gy + 3)
            }
            // X 轴：时间模式 = 时间窗 4 等分时刻；历史/XY = 等分序号/数值
            ctx.textAlign = "center"
            for (i = 0; i <= 4; ++i) {
                var gx = axL + pw * (i / 4)
                ctx.strokeStyle = "#ECF0F4"
                ctx.beginPath()
                ctx.moveTo(gx, axT)
                ctx.lineTo(gx, axT + ph)
                ctx.stroke()
                var xlabel
                if (root.trendMode === 0 && !root.historyMode) {
                    var t = Date.now() - root.timeWindowSeconds * 1000 + (i / 4) * root.timeWindowSeconds * 1000
                    xlabel = timeLabel(t)
                } else if (root.trendMode === 1) {
                    xlabel = trimNum(minX + (i / 4) * spanX)
                } else {
                    xlabel = (i === 0 ? "0" : "")   // 历史等分不标时间（无真实时间戳）
                }
                if (xlabel !== "")
                    ctx.fillText(xlabel, gx, height - 4)
            }
            // 轴框
            ctx.strokeStyle = "#C8D0DA"
            ctx.strokeRect(axL, axT, pw, ph)
            // 曲线（绘图区内）
            var pen = root.lineColor !== "" ? root.lineColor : "#1E90FF"
            ctx.strokeStyle = pen
            ctx.lineWidth = root.lineWidth > 0 ? root.lineWidth : 1.5
            ctx.beginPath()
            var j, q, px, py, m, sm, tx, tv, pyy, first
            if (root.trendMode === 1) {
                for (j = 0; j < root.xyPoints.length; ++j) {
                    q = root.xyPoints[j]
                    px = axL + (q.x - minX) / spanX * pw
                    py = axT + ph - (q.y - minV) / spanV * ph
                    if (j === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
            } else {
                first = true
                for (m = 0; m < root.samples.length; ++m) {
                    sm = root.samples[m]
                    if (root.historyMode) { tx = axL + m / Math.max(1, root.samples.length - 1) * pw; tv = Number(sm.v) }
                    else { tx = axL + Math.max(0, Math.min(1, (sm.t - (Date.now() - root.timeWindowSeconds * 1000)) / (root.timeWindowSeconds * 1000))) * pw; tv = sm.v }
                    pyy = axT + ph - (tv - minV) / spanV * ph
                    if (first) { ctx.moveTo(tx, pyy); first = false }
                    else ctx.lineTo(tx, pyy)
                }
            }
            ctx.stroke()
        }
        // 数值标签（去尾零/限位）
        function trimNum(v) {
            var r = Math.round(v * 100) / 100
            return String(r)
        }
        // 毫秒时间 → HH:MM:SS 刻度标签
        function timeLabel(ms) {
            var d = new Date(ms)
            var h = d.getHours(), mi = d.getMinutes(), s = d.getSeconds()
            function p2(n) { return n < 10 ? "0" + n : String(n) }
            return p2(h) + ":" + p2(mi) + ":" + p2(s)
        }
    }
}
