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
            if (!root.hasData) {
                ctx.fillStyle = "#BBBBBB"
                ctx.font = "11px sans-serif"
                ctx.textAlign = "center"
                ctx.fillText("（无数据——未绑定变量或未开始采样）", width / 2, height / 2)
                return
            }
            var pen = root.lineColor !== "" ? root.lineColor : "#1E90FF"
            ctx.strokeStyle = pen
            ctx.lineWidth = root.lineWidth > 0 ? root.lineWidth : 1.5
            ctx.beginPath()
            if (root.trendMode === 1) {
                // 变量A-B 散点：连线 + 点（x/y 各自 min-max 归一）
                var minX = Number.MAX_VALUE, maxX = -Number.MAX_VALUE
                var minY = Number.MAX_VALUE, maxY = -Number.MAX_VALUE
                for (var i = 0; i < root.xyPoints.length; ++i) {
                    var p = root.xyPoints[i]
                    if (p.x < minX) minX = p.x
                    if (p.x > maxX) maxX = p.x
                    if (p.y < minY) minY = p.y
                    if (p.y > maxY) maxY = p.y
                }
                var spanX = (maxX - minX) || 1
                var spanY = (maxY - minY) || 1
                var pad = 10
                for (var j = 0; j < root.xyPoints.length; ++j) {
                    var q = root.xyPoints[j]
                    var px = pad + (q.x - minX) / spanX * (width - 2 * pad)
                    var py = height - pad - (q.y - minY) / spanY * (height - 2 * pad)
                    if (j === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
            } else {
                // 时间-数据：按时间窗归一 x（最近 timeWindowSeconds），y min-max
                var minV = Number.MAX_VALUE, maxV = -Number.MAX_VALUE
                var t0 = Date.now() - root.timeWindowSeconds * 1000
                for (var k = 0; k < root.samples.length; ++k) {
                    var s = root.samples[k]
                    if (s.v < minV) minV = s.v
                    if (s.v > maxV) maxV = s.v
                }
                var spanV = (maxV - minV) || 1
                var pad2 = 10
                var first = true
                for (var m = 0; m < root.samples.length; ++m) {
                    var sm = root.samples[m]
                    // 历史回放样本 t 为索引（无真实时间戳）——按等分画
                    var tx, tv
                    if (root.historyMode) { tx = pad2 + m / Math.max(1, root.samples.length - 1) * (width - 2 * pad2); tv = Number(sm.v) }
                    else { tx = pad2 + (sm.t - t0) / (root.timeWindowSeconds * 1000) * (width - 2 * pad2); tv = sm.v }
                    var pyy = height - pad2 - (tv - minV) / spanV * (height - 2 * pad2)
                    if (first) { ctx.moveTo(tx, pyy); first = false }
                    else ctx.lineTo(tx, pyy)
                }
            }
            ctx.stroke()
        }
    }
}
