import QtQuick 2.15
import QtQuick.Controls 2.15
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
    color: "#203864"

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
    Loader {
        id: screenLoader
        anchors.fill: parent
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
        source: "nav.qml"
        visible: source !== "" && !mainShell.runtimeActive
        // 接线导航按钮回调 + 设备尺寸传递
        onLoaded: {
            navLoader.item.onStartProject = function() { mainShell.startProject() }
            navLoader.item.onCalibrate = function() { mainShell.userInteracted = true; console.log("校准: 待实现") }
            navLoader.item.onDeviceInfo = function() { console.log("设备信息: 待实现") }
            navLoader.item.onSystemManage = function() { console.log("系统管理: 待实现") }
            navLoader.item.deviceWidth = mainShell.deviceWidth
            navLoader.item.deviceHeight = mainShell.deviceHeight
            // B6-8: 用户操作导航 → 取消 3 秒自动开工程
            navLoader.item.onUserAction = function() { mainShell.userInteracted = true }
        }
    }

    // ── 画面切换 ──
    function switchTo(index) {
        if (index < 0 || index >= screenFiles.length) return
        currentIndex = index
        // ⑪候选A: 当前画面同步唯一入口（startProject/switchToName/switchTo 全路径经此）
        if (runtimeBus) runtimeBus.setCurrentScreenByName(screenFiles[index].name)
        screenLoader.source = "file://" + screenFiles[index].file
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
        navLoader.visible = false
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

    Component.onCompleted: {
        // B6-8: 有工程由 autoStartTimer（running 绑定 hasProject && !userInteracted）3 秒后自动进入；
        // 注入晚于 onCompleted（hasProject 此时仍 false），无需在此启动 Timer
    }
}
