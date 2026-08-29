import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings
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
    // 注意: 不能用 anchors.fill(会覆盖 y 绑定)——手动 width/height + y 绑定
    Loader {
        id: screenLoader
        x: 0
        y: -mainShell.kbOffset
        width: parent.width
        height: parent.height
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
        x: 0
        y: -mainShell.kbOffset
        width: parent.width
        height: parent.height
        Behavior on y { NumberAnimation { duration: 150 } }
        source: "nav.qml"
        visible: source !== "" && !mainShell.runtimeActive
        // 接线导航按钮回调 + 设备尺寸传递
        onLoaded: {
            navLoader.item.startProjectHandler = function() { mainShell.startProject() }
            navLoader.item.calibrateHandler = function() {
                mainShell.userInteracted = true
                // E 循环: 校准集成进 FW——overlay 在 FW 主窗口内渲染, VNC 全程不断;
                // 进入校准模式后由 CalibrationOverlay 采集 5 点(Qt 层坐标, 本地/VNC 统一)
                if (touchCalibrator) touchCalibrator.startCalibration(mainShell.deviceWidth, mainShell.deviceHeight)
            }
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
        // G-0: ObjectManager 当前画面同步（空 screenName 寻址的默认上下文）
        if (objectManager) objectManager.setCurrentScreen(screenFiles[index].name)
        screenLoader.source = "file://" + screenFiles[index].file
        // VNC 脏矩形：切页 → 全屏报告（西门子 dirty-rect 模式；QML 生产端报告变化区域）
        if (vncMirror) vncMirror.markDirty(0, 0, deviceWidth, deviceHeight)
    }

    // ── K-9: 设备闪烁覆盖层（POST /api/blink → setBlink；亮灭交替约 1s，多设备定位）──
    property bool blinkActive: false
    Rectangle {
        id: blinkOverlay
        anchors.fill: parent
        z: 999
        color: "#000000"
        opacity: 0
        visible: mainShell.blinkActive
        Behavior on opacity { NumberAnimation { duration: 120 } }
        Timer {
            id: blinkTimer
            interval: 500
            repeat: true
            running: mainShell.blinkActive
            onTriggered: blinkOverlay.opacity = blinkOverlay.opacity > 0 ? 0 : 0.85
        }
    }

    function setBlink(enable) {
        blinkActive = enable
        if (!enable) blinkOverlay.opacity = 0
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
        // G-0: ObjectManager 画面上下文清空（控件注销由各控件 onDestruction 完成）
        if (objectManager) objectManager.setCurrentScreen("")
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

    // ── M-3 ④: 下载工程屏幕进度条（退导航→进度→100% 停 1.5s→自动打开新工程）──
    // 信号源: C++ HttpReceiver::transferProgress → invokeMethod showTransferProgress
    // Y1（审查）：Rectangle 不消费鼠标事件——全屏空 MouseArea 吞点击（半透明遮罩下 nav 页不可点，
    // 防安装期间误开旧工程/残留无工程弹窗）
    Rectangle {
        id: downloadOverlay
        z: 300
        anchors.fill: parent
        color: "#B0000000"
        visible: false
        MouseArea {
            anchors.fill: parent
            enabled: downloadOverlay.visible
        }
        Rectangle {
            width: 420
            height: 120
            radius: 10
            anchors.centerIn: parent
            color: "#F0F7FB"
            Column {
                anchors.centerIn: parent
                spacing: 10
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "正在下载工程…"
                    font.pixelSize: 15
                    color: "#1B4E6B"
                }
                Rectangle {
                    width: 340; height: 14; radius: 7
                    color: "#D5E5EE"
                    Rectangle {
                        id: downloadBarFill
                        width: parent.width * (downloadPercent / 100)
                        height: parent.height
                        radius: 7
                        color: "#1382B1"
                    }
                }
                Text {
                    id: downloadStageText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "准备中…"
                    font.pixelSize: 12
                    color: "#5A7A8C"
                }
            }
        }
    }
    property int downloadPercent: 0
    property bool downloadPrevRuntimeActive: false   // Y2：下载前运行态（失败恢复用）
    Timer {
        id: downloadOpenTimer
        interval: 1500   // 进度满后停留 1.5s 再自动打开（M-3 ④ 用户期望 1~2 秒）
        repeat: false
        onTriggered: {
            downloadOverlay.visible = false
            mainShell.showNoProjectDialog = false   // Y1：防残留无工程弹窗盖在新工程上
            mainShell.startProject()   // 新工程已由 C++ loadAndInject 注入（screenFiles/hasProject）
        }
    }
    // M-3 ④: C++ 进度回调——首次(5%)退导航回首页; 100% 后停 1.5s 自动打开; <0 失败恢复
    function showTransferProgress(percent, stage) {
        if (percent < 0) {
            downloadOpenTimer.stop()
            downloadOverlay.visible = false
            // Y2（审查）：失败恢复——回到下载前的运行态（此前在运行 → 重新打开旧工程）
            if (downloadPrevRuntimeActive && screenFiles.length > 0) {
                mainShell.startProject()
            }
            downloadPrevRuntimeActive = false
            return
        }
        if (downloadOverlay.visible === false) {
            downloadOverlay.visible = true
            downloadPrevRuntimeActive = mainShell.runtimeActive
            // 退导航回首页（与 Stop Runtime 同语义；screenLoader 清空、runtimeActive=false）
            mainShell.stopRuntime()
        }
        // Y5（审查）：非 100% 时停旧 timer（新传输开始/进度中断时防旧 timer 提前触发 startProject）
        if (percent < 100) downloadOpenTimer.stop()
        downloadPercent = percent
        downloadStageText.text = stage
        if (percent >= 100) {
            downloadOpenTimer.start()
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
        // F 循环(2026-08-23): 候选词栏不计入 inputPanel.height(qtvk Keyboard.qml L71
        // 仅 wordCandidateList.alwaysVisible 时计入; 默认 false 候选词栏以 y:-height 上浮在
        // 键盘顶边之上)——上移量需额外让出候选词栏高度, 否则中文拼音候选词行压住输入框
        var wcv = inputPanel.keyboard ? inputPanel.keyboard.wordCandidateView : null
        // alwaysVisible=true 时键盘高度已含候选词栏(Keyboard.qml L71), 需排除避免双计
        var alwaysVisible = VirtualKeyboardSettings.wordCandidateList && VirtualKeyboardSettings.wordCandidateList.alwaysVisible
        var candidateH = (wcv && wcv.visibleCondition && !alwaysVisible) ? wcv.height : 0
        var kbTop = height - inputPanel.height - candidateH   // 键盘顶边(含候选词栏, 窗口坐标)
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
    // F 循环(2026-08-23): 候选词栏在拼音输入过程中动态出现/消失(高度/可见性变化),
    // 联动重算上移量——否则候选词栏出现后输入框又被压住
    Connections {
        target: inputPanel.keyboard ? inputPanel.keyboard.wordCandidateView : null
        function onHeightChanged() { if (inputPanel.active) mainShell.adjustForKeyboard() }
        function onVisibleConditionChanged() { if (inputPanel.active) mainShell.adjustForKeyboard() }
    }

    Component.onCompleted: {
        // B6-8: 有工程由 autoStartTimer（running 绑定 hasProject && !userInteracted）3 秒后自动进入；
        // 注入晚于 onCompleted（hasProject 此时仍 false），无需在此启动 Timer
        // E 循环（2026-08-22 用户）: 键盘语言只保留中文/英文——activeLocales 控制键盘切换的语言
        // （多语言时键盘底部显示 globe 切换键; 去掉韩/日/泰等多余语言）
        VirtualKeyboardSettings.activeLocales = ["zh_CN", "en_US"]
        VirtualKeyboardSettings.locale = "zh_CN"
        // F 循环(2026-08-23): styleLoader 同步加载(Keyboard 实例化即就绪)——兜底显式置
        // languagePopupListEnabled=false(默认已 false, 保险), 与 onStyleChanged 双保险
        if (inputPanel.keyboard && inputPanel.keyboard.style)
            inputPanel.keyboard.style.languagePopupListEnabled = false
    }
    // F 循环(2026-08-23 用户): 语言切换去右上角自绘按钮, 改键盘自带 ChangeLanguageKey(地球图标)
    // 直接点击切换中/英——languagePopupListEnabled=false 时 ChangeLanguageKey 点击走
    // changeInputLanguage() 在 activeLocales 间循环切换(qtvk Keyboard.qml L1768-1778)
    Connections {
        target: inputPanel.keyboard
        function onStyleChanged() {
            if (inputPanel.keyboard.style)
                inputPanel.keyboard.style.languagePopupListEnabled = false
        }
    }

    // ═══════════════════════════════════════════════════════════
    // E 循环（2026-08-22 用户拍板: 校准集成进 FW, VNC 全程不断）:
    // 触摸校准 overlay——渲染在 FW 主窗口内 → VncMirror frameSwapped 抓帧天然覆盖 → VNC 可见;
    // 坐标采集走 Qt 层(QML MouseArea 点击坐标): 本地触摸(evdevtouch)与 VNC 注入
    // (QWindowSystemInterface)统一到达 → 远程(经 VNC)也能点十字完成校准。
    // 5 点(四角+中心) → C++ TouchCalibrator 最小二乘 → 质量门(误差<=20px) →
    // 矩阵≈单位不写 pointercal(保持直读) / 否则写入并重启 FW 让 tslib 生效。
    // ═══════════════════════════════════════════════════════════
    Rectangle {
        id: calibOverlay
        anchors.fill: parent
        z: 1000          // 高于键盘(200)/语言按钮(300), 校准期间屏蔽一切下层交互
        color: "#F5F5F5"
        visible: touchCalibrator ? touchCalibrator.active : false

        // 审查 7e0143b8: 校准状态变化(进校准/十字移动/结果页)必须报告 VNC 脏矩形——
        // 否则 VNC 远程端延迟 ≤1.5s 才见变化(且触发全帧兜底读回 792ms 阻塞渲染线程,
        // 正是 nav.qml 长按节流 2.5x 的来源)。stateChanged 一处覆盖全部状态变化
        Connections {
            target: touchCalibrator
            function onStateChanged() {
                if (vncMirror) vncMirror.markDirty(0, 0, mainShell.deviceWidth, mainShell.deviceHeight)
            }
        }

        // ── 顶部标题 + 提示 ──
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 90
            color: "#1382B1"
            Column {
                anchors.centerIn: parent
                spacing: 4
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "触摸校准"
                    color: "white"
                    font.pixelSize: 24
                    font.bold: true
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: touchCalibrator ? touchCalibrator.statusText : ""
                    color: "#EAF5FA"
                    font.pixelSize: 14
                }
            }
        }

        // ── 点数进度 ──
        Text {
            anchors.top: parent.top
            anchors.topMargin: 100
            anchors.horizontalCenter: parent.horizontalCenter
            text: (touchCalibrator && touchCalibrator.active && !touchCalibrator.done)
                  ? (touchCalibrator.pointIndex + 1) + " / " + touchCalibrator.pointCount : ""
            color: "#666666"
            font.pixelSize: 18
            font.bold: true
        }

        // ── 取消按钮（校准采集阶段可取消; 完成页用下方结果按钮）──
        Rectangle {
            id: calibCancelBtn
            z: 3
            anchors.top: parent.top
            anchors.topMargin: 100
            anchors.right: parent.right
            anchors.rightMargin: 20
            width: 90; height: 36
            radius: 6
            color: "#DDDDDD"
            visible: touchCalibrator && touchCalibrator.active && !touchCalibrator.done
            Text {
                anchors.centerIn: parent
                text: "取消"
                color: "#333333"
                font.pixelSize: 14
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (touchCalibrator) touchCalibrator.cancelCalibration()
                    if (vncMirror) vncMirror.markDirty(0, 0, mainShell.deviceWidth, mainShell.deviceHeight)
                }
            }
        }

        // ── 采集 MouseArea（全屏, 点十字即采集; 取消按钮 z 更高不受影响）──
        MouseArea {
            id: calibCaptureArea
            anchors.fill: parent
            z: 1
            enabled: touchCalibrator && touchCalibrator.active && !touchCalibrator.done
            // 本地触摸 / VNC 注入鼠标事件统一在此采集（Qt 层坐标 = 设备原始坐标, 无 pointercal 时直读）
            // 全屏采集: 触摸偏移的设备(需校准的)点十字时 Qt 层坐标偏离目标, 若加"距十字 60px 忽略"
            // 校验会挡住偏移>60px 的设备永远无法校准; 偏移触摸点 5 十字→拟合偏移矩阵→质量门(残差)把关
            onClicked: {
                if (touchCalibrator) touchCalibrator.captureAt(mouse.x, mouse.y)
            }
        }

        // ── 当前十字（中心点 + 十字线 + 外圆, 深色可见于白底）──
        Item {
            z: 2
            x: (touchCalibrator ? touchCalibrator.pointX : 0) - 40
            y: (touchCalibrator ? touchCalibrator.pointY : 0) - 40
            width: 80; height: 80
            visible: touchCalibrator && touchCalibrator.active && !touchCalibrator.done
            Rectangle {
                x: 40 - 25
                y: 40 - 1
                width: 50; height: 2
                color: "#222222"
            }
            Rectangle {
                x: 40 - 1
                y: 40 - 25
                width: 2; height: 50
                color: "#222222"
            }
            Rectangle {
                x: 0; y: 0
                width: 80; height: 80
                radius: 40
                color: "transparent"
                border.color: "#E03030"
                border.width: 3
            }
        }

        // ── 结果页（done: 显示质量/写入结论 + 确定/重启按钮）──
        Rectangle {
            z: 4
            anchors.centerIn: parent
            width: parent.width * 0.8
            height: 200
            radius: 10
            color: "white"
            border.color: "#DDDDDD"
            border.width: 1
            visible: touchCalibrator && touchCalibrator.done
            Column {
                anchors.centerIn: parent
                spacing: 20
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width * 0.9
                    text: touchCalibrator ? touchCalibrator.resultText : ""
                    color: "#333333"
                    font.pixelSize: 16
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16
                    // 重启生效（写入 pointercal 后; 重启后 main 读 pointercal 启用 tslib, VNC 由新进程恢复）
                    Rectangle {
                        width: 120; height: 40; radius: 6
                        color: "#1382B1"
                        visible: touchCalibrator && touchCalibrator.restartNeeded
                        Text {
                            anchors.centerIn: parent
                            text: "重启生效"
                            color: "white"
                            font.pixelSize: 15
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (touchCalibrator) touchCalibrator.restartFw()
                            }
                        }
                    }
                    // 确定（未写文件 / 质量差: 直接退出校准模式回 HMI）
                    Rectangle {
                        width: 120; height: 40; radius: 6
                        color: touchCalibrator && touchCalibrator.restartNeeded ? "#999999" : "#1382B1"
                        Text {
                            anchors.centerIn: parent
                            text: touchCalibrator && touchCalibrator.restartNeeded ? "稍后重启" : "确定"
                            color: "white"
                            font.pixelSize: 15
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (touchCalibrator) touchCalibrator.cancelCalibration()
                                if (vncMirror) vncMirror.markDirty(0, 0, mainShell.deviceWidth, mainShell.deviceHeight)
                            }
                        }
                    }
                }
            }
        }
    }
}
