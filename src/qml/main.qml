import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.VirtualKeyboard
import "components"

// ═══════════════════════════════════════════════════════════
// NavigatorHMI FW 主壳（自适应: 工程 device_width/height 驱动, 7寸 1024×600 / 4寸 720×720）
// 结构: Loader 加载当前画面 + 全局叠加层 + 运行时事件总线
// 导航: 无工程 → 导航界面; 有工程 → 先导航界面, 3 秒后自动进入 startScreen（用户操作则取消, 自己点开始工程）
// ═══════════════════════════════════════════════════════════
Window {
    id: mainShell
    width: 1024
    height: 600
    visible: true
    title: qsTr("NavigatorHMI")
    color: "#0F5278"   // B6-14 主题色系深色（原 #203864）

    // ── 设备尺寸（C++ 按工程注入, 7寸 1024×600 / 4寸 720×720 等比缩放）──
    property int deviceWidth: 1024
    property int deviceHeight: 600
    onDeviceWidthChanged: { width = deviceWidth }
    onDeviceHeightChanged: { height = deviceHeight }

    // ── 运行时事件总线（C++ setContextProperty 注入；QML 只发事件，ActionRunner 执行动作）──
    // 注意: 不能声明同名 property, 否则遮蔽 context property 导致 runtimeBus 为 null

    // ── 画面状态 ──
    property bool hasProject: false
    property bool runtimeActive: false   // 运行时激活（进工程 true / Stop false）——导航页可见性依据
    property string startScreen: ""
    property var screenFiles: []        // [{name, file}]
    property int currentIndex: -1

    // ── VNC 镜像持续渲染驱动（C++ 置 vncMirrorActive；1px 颜色微变动画强制渲染循环连续跑帧）──
    property bool vncMirrorActive: false
    Rectangle {
        id: vncPulseItem
        x: 0; y: 0; width: 1; height: 1
        z: 100
        color: "#000000"
    }
    ColorAnimation {
        target: vncPulseItem
        property: "color"
        from: "#000000"; to: "#010101"
        duration: 33
        loops: Animation.Infinite
        running: mainShell.vncMirrorActive
    }

    // ── 启动逻辑（B6-8: 有工程先进导航, 3 秒后自动开工程; 用户操作则取消自动进入）──
    property bool userInteracted: false
    property bool showNoProjectDialog: false
    Timer {
        id: autoStartTimer
        interval: 3000
        running: mainShell.hasProject && !mainShell.userInteracted
        repeat: false
        onTriggered: mainShell.startProject()
    }

    // ── 画面区（Loader 加载当前画面 QML）──
    // 用户 2026-08-22: 键盘弹出时画面上移(kbOffset)让输入框不被键盘挡住; 关闭恢复
    Loader {
        id: screenLoader
        anchors.fill: parent
        y: -mainShell.kbOffset
        Behavior on y { NumberAnimation { duration: 150 } }
    }

    // ── 全局叠加层（每画面可见: Stop Runtime 等）──
    property string overlayFile: ""
    Loader {
        id: overlayLoader
        anchors.fill: parent
        source: mainShell.overlayFile !== "" ? "file://" + mainShell.overlayFile : ""
        visible: source !== ""
    }

    // ── 导航界面（无工程 / 未进入运行时 时显示——B6-8: 冷启动有工程也先显示导航页 3 秒）──
    Loader {
        id: navLoader
        anchors.fill: parent
        y: -mainShell.kbOffset
        Behavior on y { NumberAnimation { duration: 150 } }
        source: "nav.qml"
        visible: source !== "" && !mainShell.runtimeActive
        // 接线导航按钮回调 + 设备尺寸传递
        onLoaded: {
            navLoader.item.startProjectHandler = function() { mainShell.startProject() }
            navLoader.item.calibrateHandler = function() { mainShell.userInteracted = true; if (deviceInfo) deviceInfo.runCalibrate() }
            navLoader.item.deviceInfoHandler = function() { console.log("设备信息: 待实现") }
            navLoader.item.systemManageHandler = function() { console.log("系统管理: 待实现") }
            navLoader.item.deviceWidth = mainShell.deviceWidth
            navLoader.item.deviceHeight = mainShell.deviceHeight
            // B6-8: 用户操作导航 → 取消 3 秒自动开工程（userAction 是信号, 用 connect 而非赋值）
            navLoader.item.userAction.connect(function() { mainShell.userInteracted = true })
        }
    }

    // ── 画面切换 ──
    function switchTo(index) {
        if (index < 0 || index >= screenFiles.length) return
        currentIndex = index
        // ⑪候选A: 当前画面同步唯一入口（startProject/switchToName/switchTo 全路径经此）
        if (runtimeBus) runtimeBus.setCurrentScreenByName(screenFiles[index].name)
        screenLoader.source = "file://" + screenFiles[index].file
        // VNC 脏矩形：切页 → 全屏报告（西门子 dirty-rect 模式；QML 生产端报告变化区域）
        if (vncMirror) vncMirror.markDirty(0, 0, deviceWidth, deviceHeight)
    }

    function switchToName(name) {
        for (var i = 0; i < screenFiles.length; i++) {
            if (screenFiles[i].name === name) {
                switchTo(i)
                return true
            }
        }
        return false
    }

    // ── 停止运行 → 返回导航 ──
    function stopRuntime() {
        hasProject = false
        runtimeActive = false
        screenLoader.source = ""
        if (runtimeBus) runtimeBus.resetScreens()   // ⑪候选A: 清画面匹配, 防旧画面索引幽灵匹配
        // VNC 脏矩形：回导航 → 全屏报告
        if (vncMirror) vncMirror.markDirty(0, 0, deviceWidth, deviceHeight)
    }

    // ── 开始工程 → 进入 startScreen ──
    function startProject() {
        userInteracted = true
        if (screenFiles.length === 0) {
            // 无工程（B6-8）：提示并留在导航页
            showNoProjectDialog = true
            return
        }
        runtimeActive = true
        // 注意: 不能手动设 navLoader.visible —— 其 visible 绑定
        // `!mainShell.runtimeActive`，此处赋值会破坏绑定，Stop Runtime 后导航页无法再显示
        // 默认 startScreen 或第一个自定义画面
        var target = startScreen
        if (target === "" || !switchToName(target)) {
            if (screenFiles.length > 0) switchTo(0)
        }
    }

    // ── 无工程提示弹窗 ──
    Rectangle {
        z: 100
        anchors.fill: parent
        color: "#80000000"
        visible: mainShell.showNoProjectDialog
        Rectangle {
            width: 360
            height: 130
            radius: 10
            anchors.centerIn: parent
            color: "white"
            Column {
                anchors.centerIn: parent
                spacing: 14
                Text { text: "当前没有可打开的工程\n请先在存储管理中加载工程"; font.pixelSize: 14; color: "#333"; horizontalAlignment: Text.AlignHCenter }
                Rectangle {
                    width: 90; height: 32; radius: 6; color: "#1382B1"
                    anchors.horizontalCenter: parent.horizontalCenter
                    Text { anchors.centerIn: parent; text: "确定"; color: "white"; font.pixelSize: 13 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: mainShell.showNoProjectDialog = false
                    }
                }
            }
        }
    }

    // ── D+8: 点击空白处 → 退出输入状态 + 关键盘 ──
    // 屏幕 QML 根是 Item(无背景 MouseArea), 空白点击事件穿透到本层；
    // z:-50 置于所有画面控件之下, 仅接收未被控件消费的点击
    // 审查修复(2b3d04bd): 不写 inputPanel.active(它是 alias→Keyboard.active→Qt.inputMethod.visible
    // 的绑定属性, 赋值会销毁绑定导致键盘再无法弹出), 仅 Qt.inputMethod.hide() 经 visible 绑定自然收起
    MouseArea {
        id: dismissKeyboardArea
        anchors.fill: parent
        z: -50
        onClicked: mainShell.dismissKeyboard()
    }

    function dismissKeyboard() {
        Qt.inputMethod.hide()
        // 用户 2026-08-22: 键盘收起时编辑模式一并退出(失焦)——hide() 保留焦点, 需让渡焦点使 TextInput 失焦;
        // 提交仍仅回车触发(点击空白不提交, 编辑内容保留)
        if (screenLoader.item) screenLoader.item.forceActiveFocus()
        if (navLoader.item) navLoader.item.forceActiveFocus()
    }

    // ── R4: Qt VirtualKeyboard 屏上键盘（数字→数字键盘 / 文字→全键盘含中英拼音）
    // 需显式声明 InputPanel 才会显示（QT_IM_MODULE=qtvirtualkeyboard 由 C++ 设置）
    // z 序置顶：键盘盖在画面之上；锚定底部：输入控件聚焦时自动弹出
    InputPanel {
        id: inputPanel
        z: 200
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: active
        // 用户 2026-08-22: 键盘弹出 → 画面上移让输入框可见; 关闭恢复
        onActiveChanged: {
            if (active) mainShell.adjustForKeyboard()
            else mainShell.restoreForKeyboard()
        }
    }

    // 键盘弹出时画面上移量（0 = 不动）
    property int kbOffset: 0
    function adjustForKeyboard() {
        var kbTop = height - inputPanel.height   // 键盘顶边(窗口坐标)
        var fi = activeFocusItem
        if (!fi || !fi.mapToItem) { kbOffset = 0; return }
        var pos = fi.mapToItem(mainShell.contentItem, 0, fi.height)  // 输入框底边
        var bottom = pos.y
        if (bottom > kbTop) {
            var off = bottom - kbTop + 10        // 上移到键盘上方留 10px
            var top = pos.y - fi.height          // 输入框顶边
            if (off > top - 10) off = Math.max(0, top - 10)   // 防移出屏幕: 顶边至少留 10px
            kbOffset = off
        } else {
            kbOffset = 0                         // 未被遮挡不动
        }
    }
    function restoreForKeyboard() { kbOffset = 0 }

    Component.onCompleted: {
        // B6-8: 有工程由 autoStartTimer（running 绑定 hasProject && !userInteracted）3 秒后自动进入；
        // 注入晚于 onCompleted（hasProject 此时仍 false），无需在此启动 Timer
    }
}
