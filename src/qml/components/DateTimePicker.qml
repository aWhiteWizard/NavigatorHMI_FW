// W-E DateTime 日历/时间选择器（2026-09-07 用户规格：左日历年月日 + 右时分秒上下箭头 + 双击编辑 + 点外确认）
// v3（reviewer 复审修复）：浮层生命周期对齐 HmiTextList/HmiImage 先例——contentRoot 声明处缓存（JS 内访问
// Window attached 抛 TypeError——仓库 MAJOR-5 实证）；关闭只 visible=false 保留 contentRoot parent（二次打开
// 自然成立）；Component.onDestruction 恢复宿主防孤儿；commitAndClose 前显式提交活动编辑器（防编辑值丢失）。
import QtQuick 2.15
import QtQuick.Window 2.15

Item {
    id: root
    visible: false
    property var selectedDate: new Date()
    property bool interacted: false
    signal confirmed(string text)

    // 声明处缓存（JS handler/函数内访问 Window attached 会 TypeError——HmiTextList/HmiImage 先例实证）
    property var contentRoot: root.Window ? root.Window.contentItem : null
    property var homeParent: parent          // 创建时宿主（HmiDateTime 内）；onDestruction 恢复用

    function pad(n) { return n < 10 ? "0" + n : "" + n }
    function stdText() {
        var d = root.selectedDate
        return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate())
             + " " + pad(d.getHours()) + ":" + pad(d.getMinutes()) + ":" + pad(d.getSeconds())
    }
    function touchDate() { root.interacted = true; root.selectedDate = new Date(root.selectedDate.getTime()) }
    function daysInMonth(y, m) { return new Date(y, m, 0).getDate() }

    /// 显式提交活动编辑器（F4：点确定/点外时 TextInput 不失焦则不触发 editingFinished——值会丢）
    function commitActiveEdit() {
        if (hEdit.visible) { hEdit.applyEdit() }
        if (mEdit.visible) { mEdit.applyEdit() }
        if (sEdit.visible) { sEdit.applyEdit() }
    }
    function openPicker(initialText) {
        var d = new Date()
        var s = initialText === undefined || initialText === null ? "" : String(initialText)
        var m = s.match(/(\d{4})[-/.](\d{1,2})[-/.](\d{1,2})[ T](\d{1,2}):(\d{1,2})(?::(\d{1,2}))?/)
        if (m) d = new Date(+m[1], +m[2] - 1, +m[3], +m[4], +m[5], m[6] ? +m[6] : 0)
        root.selectedDate = d
        root.interacted = false
        calPane.syncView()
        if (root.contentRoot) {
            root.parent = root.contentRoot   // 浮层挂窗口 contentItem（全屏）
            root.width = root.contentRoot.width
            root.height = root.contentRoot.height
        }
        root.visible = true
    }
    function commitAndClose() {
        root.commitActiveEdit()              // 先落编辑值（F4）
        if (root.interacted)
            root.confirmed(root.stdText())
        root.visible = false                 // 保留 parent 于 contentRoot（二次打开自然成立）
    }

    // 浮层根被销毁（宿主画面切换/Stop/下载工程）时若仍挂窗口层 → 恢复宿主一并销毁（防孤儿残影——先例 onDestruction）
    Component.onDestruction: {
        if (root.parent === root.contentRoot && root.homeParent)
            root.parent = root.homeParent
    }

    // ── 全屏点外确认层（z 9999 面板下）──
    MouseArea {
        anchors.fill: parent
        z: 9998
        onClicked: root.commitAndClose()
    }

    // ── 面板（屏中，z 10000）──
    Rectangle {
        id: panel
        z: 10000
        width: 620
        height: 440
        radius: 8
        color: "#FFFFFF"
        border.color: "#1382B1"; border.width: 2
        anchors.centerIn: parent

        // ═══ 左：日历 ═══
        Rectangle {
            id: calPane
            x: 12; y: 12
            width: 380; height: panel.height - 60
            color: "#F7FBFD"; radius: 6
            property int viewYear: root.selectedDate.getFullYear()
            property int viewMonth: root.selectedDate.getMonth() + 1
            function syncView() { viewYear = root.selectedDate.getFullYear(); viewMonth = root.selectedDate.getMonth() + 1 }
            function prevMonth() { if (viewMonth === 1) { viewMonth = 12; viewYear-- } else viewMonth-- }
            function nextMonth() { if (viewMonth === 12) { viewMonth = 1; viewYear++ } else viewMonth++ }
            function goToday() {
                var d = new Date()
                viewYear = d.getFullYear(); viewMonth = d.getMonth() + 1
                root.interacted = true
                root.selectedDate = new Date(d.getFullYear(), d.getMonth(), d.getDate(),
                    root.selectedDate.getHours(), root.selectedDate.getMinutes(), root.selectedDate.getSeconds())
            }
            function cellAt(index) {
                var firstDow = new Date(viewYear, viewMonth - 1, 1).getDay()
                var dayNum = index - firstDow + 1
                var dim = daysInMonth(viewYear, viewMonth)
                var inMonth = dayNum >= 1 && dayNum <= dim
                var sd = root.selectedDate
                var sel = inMonth && sd.getFullYear() === viewYear && sd.getMonth() === viewMonth - 1 && sd.getDate() === dayNum
                return { day: dayNum, inMonth: inMonth, sel: sel }
            }

            Rectangle {
                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                height: 44; color: "#1382B1"; radius: 6
                Text { anchors.centerIn: parent; text: calPane.viewYear + " 年 " + calPane.viewMonth + " 月"
                       color: "white"; font.pixelSize: 18; font.bold: true }
                Rectangle { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter
                    width: 40; height: 30; radius: 4; color: "#2A9ACB"
                    Text { anchors.centerIn: parent; text: "◀"; color: "white"; font.pixelSize: 16 }
                    MouseArea { anchors.fill: parent; onClicked: calPane.prevMonth() } }
                Rectangle { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                    width: 40; height: 30; radius: 4; color: "#2A9ACB"
                    Text { anchors.centerIn: parent; text: "▶"; color: "white"; font.pixelSize: 16 }
                    MouseArea { anchors.fill: parent; onClicked: calPane.nextMonth() } }
            }
            Grid {
                anchors.top: parent.top; anchors.topMargin: 48
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 7
                Repeater {
                    model: ["日", "一", "二", "三", "四", "五", "六"]
                    Text { width: 52; height: 24; horizontalAlignment: Text.AlignHCenter
                           verticalAlignment: Text.AlignVCenter; text: modelData; color: "#666"; font.pixelSize: 13 }
                }
            }
            Grid {
                id: dayGrid
                anchors.top: parent.top; anchors.topMargin: 76
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 7; spacing: 2
                Repeater {
                    model: 42
                    Rectangle {
                        width: 50; height: 34; radius: 4
                        color: calPane.cellAt(index).inMonth
                            ? (calPane.cellAt(index).sel ? "#1382B1" : "#FFFFFF") : "#E5E5E5"
                        Text {
                            anchors.centerIn: parent
                            text: calPane.cellAt(index).inMonth ? calPane.cellAt(index).day : ""
                            color: calPane.cellAt(index).sel ? "white" : "#333"
                            font.pixelSize: 14
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var c = calPane.cellAt(index)
                                if (c.inMonth) {
                                    root.interacted = true
                                    root.selectedDate = new Date(calPane.viewYear, calPane.viewMonth - 1, c.day,
                                        root.selectedDate.getHours(), root.selectedDate.getMinutes(), root.selectedDate.getSeconds())
                                }
                            }
                        }
                    }
                }
            }
        }

        // ═══ 右：时间（时/分/秒，▲▼ 独立区 + 值双击编辑）═══
        Column {
            x: 404; y: 12
            spacing: 8
            // 时
            Rectangle {
                width: 190; height: 58; radius: 6; color: "#FFFFFF"; border.color: "#DDD"
                Text {
                    x: 14; y: 13; width: 110; height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: "时  " + root.pad(root.selectedDate.getHours())
                    font.pixelSize: 24; font.bold: true; color: "#333"
                    // Q1 修复（N+42）：编辑态隐藏原值——TextInput 独占显示，防「在原字符上叠蓝色修改字符」
                    visible: !hEdit.visible
                }
                // ▲▼ 箭头列（独立区，不被双击层覆盖）
                Column {
                    anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Rectangle { width: 40; height: 22; radius: 3; color: "#DDEFF7"
                        Text { anchors.centerIn: parent; text: "▲"; font.pixelSize: 12; color: "#1382B1" }
                        MouseArea { anchors.fill: parent; onClicked: { var v = root.selectedDate.getHours() + 1; root.selectedDate.setHours(v > 23 ? 0 : v); root.touchDate() } } }
                    Rectangle { width: 40; height: 22; radius: 3; color: "#DDEFF7"
                        Text { anchors.centerIn: parent; text: "▼"; font.pixelSize: 12; color: "#1382B1" }
                        MouseArea { anchors.fill: parent; onClicked: { var v = root.selectedDate.getHours() - 1; root.selectedDate.setHours(v < 0 ? 23 : v); root.touchDate() } } }
                }
                // 值双击编辑（限左区，避开箭头列）
                MouseArea {
                    x: 0; y: 0; width: 128; height: 58
                    onDoubleClicked: { hEdit.visible = true; hEdit.text = root.pad(root.selectedDate.getHours()); hEdit.forceActiveFocus(); Qt.callLater(hEdit.selectAll) }   // Q1：callLater 延后 selectAll——visible+focus 同帧 selectAll 可能失效（无蓝底可见）
                }
                TextInput {
                    id: hEdit
                    visible: false
                    x: 14; y: 12; width: 104; height: 32   // 与原 Text 同位（编辑独占显示区）
                    font.pixelSize: 24; font.bold: true; color: "#1382B1"
                    selectionColor: "#1382B1"      // Q1：全选蓝底可见（eglfs 无平台默认选中高亮）
                    selectedTextColor: "#FFFFFF"   // Q1：选中文字白
                    cursorColor: "#1382B1"
                    verticalAlignment: Text.AlignVCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 23 }
                    function applyEdit() {
                        var n = parseInt(text)
                        if (!isNaN(n)) { root.selectedDate.setHours(Math.min(Math.max(n, 0), 23)); root.touchDate() }
                        visible = false
                    }
                    onEditingFinished: hEdit.applyEdit()
                }
            }
            // 分
            Rectangle {
                width: 190; height: 58; radius: 6; color: "#FFFFFF"; border.color: "#DDD"
                Text {
                    x: 14; y: 13; width: 110; height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: "分  " + root.pad(root.selectedDate.getMinutes())
                    font.pixelSize: 24; font.bold: true; color: "#333"
                    visible: !mEdit.visible   // Q1（N+42）：编辑态隐藏原值
                }
                Column {
                    anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Rectangle { width: 40; height: 22; radius: 3; color: "#DDEFF7"
                        Text { anchors.centerIn: parent; text: "▲"; font.pixelSize: 12; color: "#1382B1" }
                        MouseArea { anchors.fill: parent; onClicked: { var v = root.selectedDate.getMinutes() + 1; root.selectedDate.setMinutes(v > 59 ? 0 : v); root.touchDate() } } }
                    Rectangle { width: 40; height: 22; radius: 3; color: "#DDEFF7"
                        Text { anchors.centerIn: parent; text: "▼"; font.pixelSize: 12; color: "#1382B1" }
                        MouseArea { anchors.fill: parent; onClicked: { var v = root.selectedDate.getMinutes() - 1; root.selectedDate.setMinutes(v < 0 ? 59 : v); root.touchDate() } } }
                }
                MouseArea {
                    x: 0; y: 0; width: 128; height: 58
                    onDoubleClicked: { mEdit.visible = true; mEdit.text = root.pad(root.selectedDate.getMinutes()); mEdit.forceActiveFocus(); Qt.callLater(mEdit.selectAll) }   // Q1
                }
                TextInput {
                    id: mEdit
                    visible: false
                    x: 14; y: 12; width: 104; height: 32
                    font.pixelSize: 24; font.bold: true; color: "#1382B1"
                    selectionColor: "#1382B1"      // Q1：全选蓝底
                    selectedTextColor: "#FFFFFF"
                    cursorColor: "#1382B1"
                    verticalAlignment: Text.AlignVCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 59 }
                    function applyEdit() {
                        var n = parseInt(text)
                        if (!isNaN(n)) { root.selectedDate.setMinutes(Math.min(Math.max(n, 0), 59)); root.touchDate() }
                        visible = false
                    }
                    onEditingFinished: mEdit.applyEdit()
                }
            }
            // 秒
            Rectangle {
                width: 190; height: 58; radius: 6; color: "#FFFFFF"; border.color: "#DDD"
                Text {
                    x: 14; y: 13; width: 110; height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: "秒  " + root.pad(root.selectedDate.getSeconds())
                    font.pixelSize: 24; font.bold: true; color: "#333"
                    visible: !sEdit.visible   // Q1（N+42）：编辑态隐藏原值
                }
                Column {
                    anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Rectangle { width: 40; height: 22; radius: 3; color: "#DDEFF7"
                        Text { anchors.centerIn: parent; text: "▲"; font.pixelSize: 12; color: "#1382B1" }
                        MouseArea { anchors.fill: parent; onClicked: { var v = root.selectedDate.getSeconds() + 1; root.selectedDate.setSeconds(v > 59 ? 0 : v); root.touchDate() } } }
                    Rectangle { width: 40; height: 22; radius: 3; color: "#DDEFF7"
                        Text { anchors.centerIn: parent; text: "▼"; font.pixelSize: 12; color: "#1382B1" }
                        MouseArea { anchors.fill: parent; onClicked: { var v = root.selectedDate.getSeconds() - 1; root.selectedDate.setSeconds(v < 0 ? 59 : v); root.touchDate() } } }
                }
                MouseArea {
                    x: 0; y: 0; width: 128; height: 58
                    onDoubleClicked: { sEdit.visible = true; sEdit.text = root.pad(root.selectedDate.getSeconds()); sEdit.forceActiveFocus(); Qt.callLater(sEdit.selectAll) }   // Q1
                }
                TextInput {
                    id: sEdit
                    visible: false
                    x: 14; y: 12; width: 104; height: 32
                    font.pixelSize: 24; font.bold: true; color: "#1382B1"
                    selectionColor: "#1382B1"      // Q1：全选蓝底
                    selectedTextColor: "#FFFFFF"
                    cursorColor: "#1382B1"
                    verticalAlignment: Text.AlignVCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 59 }
                    function applyEdit() {
                        var n = parseInt(text)
                        if (!isNaN(n)) { root.selectedDate.setSeconds(Math.min(Math.max(n, 0), 59)); root.touchDate() }
                        visible = false
                    }
                    onEditingFinished: sEdit.applyEdit()
                }
            }
        }

        // ═══ 底：今天 / 确定 ═══
        Row {
            anchors.bottom: parent.bottom; anchors.bottomMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16
            Rectangle { width: 120; height: 36; radius: 6; color: "#EEEEEE"
                Text { anchors.centerIn: parent; text: "今天"; color: "#333"; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: calPane.goToday() } }
            Rectangle { width: 120; height: 36; radius: 6; color: "#1382B1"
                Text { anchors.centerIn: parent; text: "确定"; color: "white"; font.pixelSize: 14 }
                MouseArea { anchors.fill: parent; onClicked: root.commitAndClose() } }
        }
    }
}
