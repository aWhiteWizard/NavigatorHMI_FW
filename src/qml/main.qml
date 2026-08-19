import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"

// ═══════════════════════════════════════════════════════════
// NavigatorHMI FW 主壳（1024×600 设备屏）
// 结构: Loader 加载当前画面 + 全局叠加层 + 运行时事件总线
// 导航: 无工程 → 导航界面; 有工程 → startScreen 进入
// ═══════════════════════════════════════════════════════════
Window {
    id: mainShell
    width: 1024
    height: 600
    visible: true
    title: qsTr("NavigatorHMI")
    color: "#203864"

    // ── 运行时事件总线（C++ setContextProperty 注入；QML 只发事件，ActionRunner 执行动作）──
    // 注意: 不能声明同名 property, 否则遮蔽 context property 导致 runtimeBus 为 null

    // ── 画面状态 ──
    property bool hasProject: false
    property string startScreen: ""
    property var screenFiles: []        // [{name, file}]
    property int currentIndex: -1

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

    // ── 导航界面（无工程时显示）──
    Loader {
        id: navLoader
        anchors.fill: parent
        source: "nav.qml"
        visible: source !== "" && !mainShell.hasProject
        // 接线导航按钮回调
        onLoaded: {
            navLoader.item.onStartProject = function() { mainShell.startProject() }
            navLoader.item.onCalibrate = function() { console.log("校准: 待实现") }
            navLoader.item.onDeviceInfo = function() { console.log("设备信息: 待实现") }
            navLoader.item.onSystemManage = function() { console.log("系统管理: 待实现") }
        }
    }

    // ── 画面切换 ──
    function switchTo(index) {
        if (index < 0 || index >= screenFiles.length) return
        currentIndex = index
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
        screenLoader.source = ""
        navLoader.visible = true
    }

    // ── 开始工程 → 进入 startScreen ──
    function startProject() {
        hasProject = true
        navLoader.visible = false
        // 默认 startScreen 或第一个自定义画面
        var target = startScreen
        if (target === "" || !switchToName(target)) {
            if (screenFiles.length > 0) switchTo(0)
        }
    }

    Component.onCompleted: {
        // 有工程时自动进入（转换器加载后由 C++ 调用 startProject）
        if (hasProject) startProject()
    }
}
