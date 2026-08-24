// B-4: HmiIoField——输入框控件组件（绑变量, 信号 hmiInput）
import QtQuick 2.15

Rectangle {
    id: root
    width: 100
    height: 40
    color: fillColor !== "" ? fillColor : "#FFFFFF"
    border.color: strokeColor !== "" ? strokeColor : "#999999"
    border.width: strokeThickness > 0 ? strokeThickness : 1
    radius: 2

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property string content: ""
    property bool isReadOnly: false
    // D+ 审查修复(2b3d04bd): 生成器按 boundTag 类型输出 inputMethodHints(整型→数字/浮点坐标→数字+小数点/字符串→全键盘)
    property int inputMethodHints: 0
    // 用户 2026-08-22: 布尔变量 → 双按钮选择(开/关), 非文本输入; 生成器对 Bool tag 输出 true
    property bool isBoolean: false
    // F 循环(2026-08-23 用户): GPS 坐标 iofield——显示态转度分秒+方位(如 30°15'30"N),
    // 编辑态回小数可输入, 提交双格式解析; 生成器对 Gps tag 输出 true
    property bool isGps: false
    // GPS 编辑中(聚焦)显示小数, 非编辑显示度分秒——避免用户输入时被格式转换打断
    property bool gpsEditing: false
    // 用户 2026-08-22: 点击空白不改用户已输入内容——编辑中标记, 变量回写不覆盖用户输入(提交后恢复跟随)
    property bool editing: false
    property string textColor: "#000000"
    property double fontSize: 14
    property string fontFamily: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
     property string text: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
     property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
     property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
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

    signal hmiInput()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiValueChanged()
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // ── 布尔模式（isBoolean=true）: 显示当前值(0/1), 点击弹二值键盘(0/1 两键) ──
    // 用户 2026-08-22 澄清: 控件保持显示, 弹出的"键盘"只有 0/1 两个键(非控件变双按钮)
    property var contentRoot: root.Window ? root.Window.contentItem : null  // 声明处缓存(JS handler 用)

    Text {
        id: boolText
        visible: root.isBoolean
        anchors.fill: parent
        anchors.margins: 6
        text: root.isOn ? "1" : "0"
        color: root.textColor
        font.pixelSize: root.fontSize
        font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    MouseArea {
        id: boolClick
        visible: root.isBoolean
        anchors.fill: parent
        onClicked: root.showBoolPad()
    }

    // ── 二值键盘（0/1 两键, 挂窗口 contentItem, 同 textlist 自建下拉模式）──
    Rectangle {
        id: boolPad
        visible: false
        z: 10000
        width: 160
        height: 72
        color: "#F0F0F0"
        border.color: "#888888"
        border.width: 1
        radius: 6
        clip: true
        Row {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4
            Rectangle {
                width: (parent.width - 4) / 2
                height: parent.height
                radius: 4
                color: !root.isOn ? "#D32F2F" : "#EEEEEE"
                border.color: "#999999"
                Text { anchors.centerIn: parent; text: "0"; color: !root.isOn ? "white" : "#333333"; font.pixelSize: 22; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.pickBoolean(false) }
            }
            Rectangle {
                width: (parent.width - 4) / 2
                height: parent.height
                radius: 4
                color: root.isOn ? "#4CAF50" : "#EEEEEE"
                border.color: "#999999"
                Text { anchors.centerIn: parent; text: "1"; color: root.isOn ? "white" : "#333333"; font.pixelSize: 22; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.pickBoolean(true) }
            }
        }
    }
    // 全屏关闭层（面板打开时挂 contentItem, 点外部关闭）
    MouseArea {
        id: boolDismiss
        visible: false
        z: 9999
        onClicked: root.hideBoolPad()
    }

    function showBoolPad() {
        if (!contentRoot) return
        boolPad.parent = contentRoot
        boolDismiss.parent = contentRoot
        boolDismiss.x = 0; boolDismiss.y = 0
        boolDismiss.width = contentRoot.width
        boolDismiss.height = contentRoot.height
        var p = root.mapToItem(null, 0, 0)
        boolPad.x = p.x
        boolPad.y = p.y + root.height
        if (boolPad.y + boolPad.height > contentRoot.height)
            boolPad.y = Math.max(0, p.y - boolPad.height)
        boolPad.x = Math.max(0, Math.min(boolPad.x, contentRoot.width - boolPad.width))
        boolPad.visible = true
        boolDismiss.visible = true
    }
    function hideBoolPad() {
        boolPad.visible = false
        boolDismiss.visible = false
    }
    Component.onDestruction: {
        hideBoolPad()
        if (boolPad.parent !== root) boolPad.parent = root
        if (boolDismiss.parent !== root) boolDismiss.parent = root
    }

    // ── GPS 度分秒转换（F 循环 2026-08-23，对齐 PC 端 GeoPoint 契约）──
    // 契约（NavigatorHMI.Core/Models/GeoPoint.cs）：坐标对 经度,纬度（WGS84 小数度内部存储）；
    // DMS 方向前缀放最前、经度在前纬度在后："E104°3'30\", N30°40'20\"";
    // 基准值括号包裹 "(E104°3'30\", N30°40'20\")"; 小数度 "104.0583, 30.6722"; 秒 2 位小数。
    // 生成器对 Gps tag 输出 isGps: true；本组件显示态转 DMS 前缀式, 编辑态回小数, 提交双格式解析。

    // 格式化单个坐标：方向前缀 + 度分秒（秒 2 位小数, 60 进位——与 GeoPoint.FormatCoord 一致）
    function formatCoord(value, isLongitude) {
        var prefix = value >= 0 ? (isLongitude ? "E" : "N") : (isLongitude ? "W" : "S")
        var abs = Math.abs(value)
        var deg = Math.floor(abs)
        var minF = (abs - deg) * 60
        var min = Math.floor(minF)
        var sec = Math.round((minF - min) * 6000) / 100
        if (sec >= 60) { sec = 0; min++ }
        if (min >= 60) { min = 0; deg++ }
        return prefix + deg + "°" + min + "'" + sec.toFixed(2) + '"'
    }

    // 解析单个坐标（DMS 或小数度，可带方向前缀；经度 ±180 / 纬度 ±90 校验——对齐 GeoPoint.TryParseCoord）
    function parseCoord(s, isLongitude) {
        var str = String(s).trim()
        if (str.length === 0) return null
        var sign = 1
        var idx = 0
        var c0 = str.charAt(0)
        if (isLongitude) {
            if (c0 === "W" || c0 === "w") { sign = -1; idx = 1 }
            else if (c0 === "E" || c0 === "e") { idx = 1 }
        } else {
            if (c0 === "S" || c0 === "s") { sign = -1; idx = 1 }
            else if (c0 === "N" || c0 === "n") { idx = 1 }
        }
        // 前缀与位置错配（经度位 N/S、纬度位 E/W）：不消费前缀, 后续含字母解析失败 → 拒绝
        var numPart = str.substring(idx).trim()
        if (numPart.length === 0) return null
        var result = null
        if (numPart.indexOf("°") >= 0 || numPart.indexOf("度") >= 0) {
            result = parseDms(numPart)
        } else {
            // 整串校验后 parseFloat（对齐 PC 端 double.TryParse 严格性——parseFloat 宽容接受尾部垃圾）
            if (!/^-?\d+(?:\.\d+)?$/.test(numPart.trim())) return null
            var n = parseFloat(numPart)
            if (!isNaN(n)) result = n
        }
        if (result === null) return null
        var v = sign * result
        return (isLongitude ? (v >= -180 && v <= 180) : (v >= -90 && v <= 90)) ? v : null
    }

    // 解析度分秒：度°分'秒" / 度°分' / 度°（含中文变体）
    function parseDms(s) {
        var m = s.match(/^\s*(\d+(?:\.\d+)?)\s*[°度]\s*(\d+(?:\.\d+)?)\s*['′分]\s*(\d+(?:\.\d+)?)\s*["″秒]\s*$/)
        var deg, min = 0, sec = 0
        if (m) {
            deg = parseFloat(m[1]); min = parseFloat(m[2]); sec = parseFloat(m[3])
        } else {
            m = s.match(/^\s*(\d+(?:\.\d+)?)\s*[°度]\s*(\d+(?:\.\d+)?)\s*['′分]\s*$/)
            if (m) { deg = parseFloat(m[1]); min = parseFloat(m[2]) }
            else {
                m = s.match(/^\s*(\d+(?:\.\d+)?)\s*[°度]\s*$/)
                if (!m) return null
                deg = parseFloat(m[1])
            }
        }
        if (min >= 60 || sec >= 60) return null
        return deg + min / 60 + sec / 3600
    }

    // 显示：content(基准值/小数度) → DMS 前缀式 "E104°3'30\", N30°40'20\""
    function toDms(v) {
        var g = parseGpsPair(v)
        if (!g) return v                    // 无法解析原样显示（不破坏用户输入）
        return formatCoord(g.lng, true) + ", " + formatCoord(g.lat, false)
    }
    // 解析坐标对（基准值括号/小数度逗号分隔），成功返回 {lng, lat} 否则 null
    function parseGpsPair(v) {
        var s = String(v).trim()
        if (s.charAt(0) === "(" && s.charAt(s.length - 1) === ")")
            s = s.substring(1, s.length - 1).trim()
        var parts = s.split(/[,，]/)
        if (parts.length !== 2) return null
        var lng = parseCoord(parts[0].trim(), true)
        var lat = parseCoord(parts[1].trim(), false)
        if (lng === null || lat === null) return null
        return { lng: lng, lat: lat }
    }
    // 提交解析：DMS/小数度（可带括号）→ 基准值括号格式 "(E104°3'30\", N30°40'20\")"（对齐 GeoPoint.ToBaseValue）
    function fromDms(s) {
        var g = parseGpsPair(s)
        if (!g) return s                    // 无法解析原样返回（校验由 PC 端/数据层把关）
        return "(" + formatCoord(g.lng, true) + ", " + formatCoord(g.lat, false) + ")"
    }
    // 编辑态小数显示：content(括号基准值/DMS/小数度) → "lng,lat" 小数（可编辑; 精度 8 位与生成器一致）
    function toDecimal(v) {
        var g = parseGpsPair(v)
        if (!g) return v
        return g.lng.toFixed(8).replace(/\.?0+$/, "") + "," + g.lat.toFixed(8).replace(/\.?0+$/, "")
    }
    // 显示文本统一入口: GPS 编辑态显示小数(可输入), 非编辑态显示 DMS 前缀式, 其余原样
    function displayText() {
        if (root.isGps) {
            if (root.gpsEditing) return root.toDecimal(root.content)
            return root.toDms(root.content)
        }
        return root.content
    }

    TextInput {
        id: input
        visible: !root.isBoolean
        anchors.fill: parent
        anchors.margins: 6
        // H-4(2026-08-24 用户 Check): 经纬度显示溢出框外——clip 限制内容在框内,
        // horizontalAlignment 绑 hAlign（默认左对齐, 从头显示; TextInput 无 elide, clip 右侧截断即可）
        clip: true
        horizontalAlignment: root.hAlign === "Right" ? Text.AlignRight : Text.AlignLeft
        text: root.displayText()
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: root.fontFamily !== "" ? root.fontFamily : "sans-serif"
        readOnly: root.isReadOnly
        inputMethodHints: root.inputMethodHints
        verticalAlignment: Text.AlignVCenter
        // F 循环: GPS 编辑态切换——聚焦显示小数(可输入), 失焦转回度分秒;
        // 用户编辑会破坏 text 绑定(见 4_bugs qml-binding-assign-destroys), 故 GPS 模式
        // 焦点切换直接手动同步显示, 不依赖绑定重算;
        // editing 守卫(D+ 规则: 点击空白不提交、编辑内容保留): 有未提交编辑时失焦不覆盖
        onFocusChanged: {
            if (root.isGps) {
                var wasEditing = root.gpsEditing
                root.gpsEditing = (focus && !root.isReadOnly)
                if (root.isReadOnly) return
                if (focus) {
                    if (!root.editing) input.text = root.toDecimal(root.content)
                } else if (wasEditing && !root.editing) {
                    input.text = root.toDms(root.content)
                }
            }
        }
        onTextEdited: root.editing = true  // 用户 2026-08-22: 用户编辑标记——变量回写不覆盖输入
        onAccepted: {
            // F 循环: GPS 提交双格式解析(度分秒 或 纯小数); 写变量统一存括号基准值格式
            // "(E104°3'30\", N30°40'20\")"——对齐 PC 端 BaseValueValidator.Normalize→GeoPoint.ToBaseValue
            if (root.isGps) root.content = root.fromDms(text)
            else root.content = text
            // R1: 输入提交写变量（状态持久化）
            if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
                dataManager.setValue(root.boundTag, root.content)
            root.hmiInput()
            root.editing = false  // 提交后恢复变量跟随
            // D+8: 回车确认 → 退出输入态 + 关键盘（用户: 回车/点击空白即关闭键盘; 提交仅回车触发）
            input.focus = false
            Qt.inputMethod.hide()
        }
    }

    // 二值键盘选择(0/1) → 写变量 + 关面板
    function pickBoolean(v) {
        root.isOn = v
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
            dataManager.setValue(root.boundTag, v)
        root.hideBoolPad()
        root.hmiValueChanged()
        root.hmiInput()
    }
    function boolFromValue(v) {
        return (v === true || String(v) === "true" || String(v) === "1")
    }

    // R1: 状态持久化——初始化 + 外部跟随（用户 2026-08-22: 编辑中(editing)不覆盖用户输入, 提交后恢复跟随）
    Component.onCompleted: {
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) {
                if (root.isBoolean) root.isOn = root.boolFromValue(v)
                else root.content = String(v)
            }
        }
    }
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag) {
                if (root.isBoolean) {
                    root.isOn = root.boolFromValue(value)
                } else if (!root.editing) {
                    root.content = String(value)
                    // F 循环: GPS 回写——聚焦编辑态显示小数, 非编辑态显示度分秒+方位;
                    // （text 绑定可能已因用户编辑破坏, 手动同步）
                    input.text = root.isGps
                            ? (root.gpsEditing ? root.toDecimal(root.content) : root.toDms(root.content))
                            : root.content
                }
            }
        }
    }
}
