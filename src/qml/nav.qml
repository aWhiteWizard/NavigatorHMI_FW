import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// ═══════════════════════════════════════════════════════════
// B-5 导航界面（system-manager 设计 + 用户布局定稿 2026-08-19）
// 结构: 左侧可收起导航(200↔60) + 右侧 StackLayout 分页
// 页面: 首页(开始工程/触摸校准两区铺满 + 设备信息) / 设备信息 / 存储管理 / 通信控制
// 自适应: 等比缩放(7/4寸) + 收起/展开内容区均匀变化 + 两列→一列→滚动
// ═══════════════════════════════════════════════════════════
Item {
    id: navRoot
    width: 1024
    height: 600

    // 主壳注入回调
    property var onStartProject: null
    property var onCalibrate: null
    property var onDeviceInfo: null
    property var onSystemManage: null
    property int deviceWidth: 1024
    property int deviceHeight: 600

    // 导航状态
    property bool navExpanded: true
    readonly property int navWidth: navExpanded ? 200 : 60

    // ── 左侧导航栏 ──
    Rectangle {
        id: navBar
        width: navRoot.navWidth
        height: parent.height
        color: "#1B2A4A"
        Behavior on width { NumberAnimation { duration: 150 } }

        Column {
            id: navItems
            anchors.top: parent.top
            anchors.topMargin: 10
            width: parent.width
            spacing: 2

            NavItem { icon: "🏠"; label: "首页"; page: "home"; expanded: navRoot.navExpanded }
            NavItem { icon: "ℹ️"; label: "设备信息"; page: "info"; expanded: navRoot.navExpanded }
            NavItem { icon: "💾"; label: "存储管理"; page: "storage"; expanded: navRoot.navExpanded }
            NavItem { icon: "📡"; label: "通信控制"; page: "network"; expanded: navRoot.navExpanded }
        }

        // 底部: 日夜 + 收起
        Column {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            width: parent.width
            spacing: 2
            NavItem {
                icon: navRoot.isDark ? "☀️" : "🌙"
                label: navRoot.isDark ? "日间" : "夜间"
                page: "dark"
                expanded: navRoot.navExpanded
                onClicked: navRoot.isDark = !navRoot.isDark
            }
            NavItem {
                icon: "◀▶"
                label: navRoot.navExpanded ? "收起" : "展开"
                page: "toggle"
                expanded: navRoot.navExpanded
                onClicked: navRoot.navExpanded = !navRoot.navExpanded
            }
        }
    }

    // ── 右侧内容区（分页）──
    Rectangle {
        id: contentArea
        anchors.left: navBar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: navRoot.isDark ? "#2D2D2D" : "#F5F6FA"
        Behavior on color { ColorAnimation { duration: 200 } }

        StackLayout {
            id: pages
            anchors.fill: parent
            anchors.margins: 16
            currentIndex: navRoot.currentPageIndex

            // 0: 首页
            HomePage {
                width: pages.width
                height: pages.height
                isDark: navRoot.isDark
                onStartClicked: { if (navRoot.onStartProject) navRoot.onStartProject() }
                onCalibrateClicked: { if (navRoot.onCalibrate) navRoot.onCalibrate() }
            }
            // 1: 设备信息
            DeviceInfoPage {
                width: pages.width
                height: pages.height
                isDark: navRoot.isDark
            }
            // 2: 存储管理
            StoragePage {
                width: pages.width
                height: pages.height
                isDark: navRoot.isDark
            }
            // 3: 通信控制
            NetworkPage {
                width: pages.width
                height: pages.height
                isDark: navRoot.isDark
            }
        }
    }

    // 日夜主题
    property bool isDark: false

    // 当前页索引
    property int currentPageIndex: 0

    // B6-8: 用户操作信号（导航交互 → 主壳取消 3 秒自动开工程）
    signal userAction()

    // ── 导航项组件 ──
    component NavItem: Rectangle {
        id: navItem
        width: parent.width
        height: 48
        radius: 4
        color: navRoot.currentPageIndex === navItemIndex ? "#2A4A8A" : "transparent"

        property string icon: ""
        property string label: ""
        property string page: ""
        property bool expanded: true
        signal clicked()

        // 页索引映射（供高亮）
        readonly property int navItemIndex: page === "home" ? 0
            : page === "info" ? 1
            : page === "storage" ? 2
            : page === "network" ? 3 : -1

        Row {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Text {
                width: 32
                height: 48
                text: navItem.icon
                font.pixelSize: 22
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                width: navItem.expanded ? navItem.width - 50 : 0
                height: 48
                text: navItem.label
                color: "white"
                font.pixelSize: 15
                verticalAlignment: Text.AlignVCenter
                visible: navItem.expanded
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                navRoot.userAction()   // B6-8: 任何导航交互视为用户操作
                if (navItem.page === "dark" || navItem.page === "toggle") {
                    navItem.clicked()
                } else if (navItem.navItemIndex >= 0) {
                    navRoot.currentPageIndex = navItem.navItemIndex
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════
    // 首页: 开始工程/触摸校准两区铺满 + 设备信息两列
    // ═══════════════════════════════════════════════════
    component HomePage: Item {
        id: homePageRoot
        property bool isDark: false
        property int infoHeight: 110
        signal startClicked()
        signal calibrateClicked()

        Column {
            anchors.fill: parent
            spacing: 16

            // 开始工程（上半区, 铺满横宽）
            BigButton {
                width: parent.width
                height: (parent.height - 32 - parent.parent.infoHeight) * 0.45
                text: "开 始 工 程"
                isDark: homePageRoot.isDark
                onClicked: parent.parent.startClicked()
            }
            // 触摸校准（下半区）
            BigButton {
                width: parent.width
                height: (parent.height - 32 - parent.parent.infoHeight) * 0.45
                text: "触 摸 校 准"
                isDark: homePageRoot.isDark
                onClicked: parent.parent.calibrateClicked()
            }
            // 设备信息（两列, 自适应降列+滚动）
            DeviceInfoBox {
                id: infoBox
                width: parent.width
                height: parent.parent.infoHeight
                isDark: homePageRoot.isDark
            }
        }
    }

    // ── 大按钮（圆角, 铺满）──
    component BigButton: Rectangle {
        id: bigBtn
        radius: 12
        color: mouse.pressed ? (isDark ? "#3A5A9A" : "#2A4A8A") : (isDark ? "#1B2A4A" : "#1B2A4A")
        border.color: "#3A5A9A"
        border.width: 1

        property string text: ""
        property bool isDark: false
        signal clicked()

        Text {
            anchors.centerIn: parent
            text: bigBtn.text
            color: "white"
            font.pixelSize: Math.max(20, parent.width / 12)
            font.bold: true
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            onClicked: bigBtn.clicked()
        }
    }

    // ═══════════════════════════════════════════════════
    // 设备信息两列框（自适应: 放不下→一列→滚动）
    // ═══════════════════════════════════════════════════
    component DeviceInfoBox: Rectangle {
        id: infoBox
        radius: 8
        color: isDark ? "#3D3D3D" : "#FFFFFF"
        border.color: isDark ? "#555" : "#DDD"
        border.width: 1

        property bool isDark: false
        // B6-6: 内容真实读取（deviceInfo context property）；运行时间动态刷新
        property var entries: [
            { label: "IP", value: deviceInfo ? deviceInfo.ipAddress : "—" },
            { label: "MAC", value: deviceInfo ? deviceInfo.macAddress : "—" },
            { label: "版本", value: deviceInfo ? deviceInfo.appVersion : "—" },
            { label: "内核", value: deviceInfo ? deviceInfo.kernelVersion : "—" },
            { label: "运行", value: "" }
        ]
        property string uptimeText: deviceInfo ? deviceInfo.uptimeText() : "—"
        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: infoBox.uptimeText = deviceInfo ? deviceInfo.uptimeText() : "—"
        }

        Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 8
            anchors.leftMargin: 12
            text: "设备信息"
            color: infoBox.isDark ? "#CCC" : "#555"
            font.pixelSize: 13
            font.bold: true
        }

        // 两列（宽度足）或一列（不足）, 一列过长滚动
        // B6-4: 内容宽度绑 availableWidth（QQC2 ScrollView 视口可用宽）——parent.width(Flickable)
        //       在 ScrollView 内宽度行为不可靠，曾致 Text width 负值不渲染（"只有框没有信息"）
        ScrollView {
            id: devInfoScroll
            anchors.top: parent.top
            anchors.topMargin: 30
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            clip: true

            Grid {
                width: devInfoScroll.availableWidth
                columns: devInfoScroll.availableWidth > 400 ? 2 : 1
                spacing: 4
                Repeater {
                    model: infoBox.entries
                    delegate: Text {
                        width: (devInfoScroll.availableWidth > 400 ? devInfoScroll.availableWidth / 2 : devInfoScroll.availableWidth) - 6
                        text: modelData.label === "运行"
                              ? modelData.label + ":  " + infoBox.uptimeText
                              : modelData.label + ":  " + modelData.value
                        color: infoBox.isDark ? "#EEE" : "#333"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════
    // 设备信息页（独立页, 与首页信息框同数据, 两列→一列→滚动）
    // ═══════════════════════════════════════════════════
    component DeviceInfoPage: Item {
        id: deviceInfoPageRoot
        property bool isDark: false
        // B6-6: 运行时间动态刷新（Timer 每秒）
        property string uptimeText: deviceInfo ? deviceInfo.uptimeText() : "—"
        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: deviceInfoPageRoot.uptimeText = deviceInfo ? deviceInfo.uptimeText() : "—"
        }

        // B6-5: 内容宽度绑 availableWidth——收起/展开导航时卡片宽度跟随视口均匀拉长
        //       （parent.width(Flickable) 宽度不跟随视口 → 之前"平移而非均匀拉长"）
        ScrollView {
            id: devInfoPageScroll
            anchors.fill: parent
            clip: true

            Column {
                width: devInfoPageScroll.availableWidth
                spacing: 8
                Grid {
                    columns: devInfoPageScroll.availableWidth > 400 ? 2 : 1
                    spacing: 8
                    width: devInfoPageScroll.availableWidth
                    Repeater {
                        model: [
                            { label: "IP 地址", value: deviceInfo ? deviceInfo.ipAddress : "—" },
                            { label: "MAC", value: deviceInfo ? deviceInfo.macAddress : "—" },
                            { label: "软件版本", value: deviceInfo ? deviceInfo.appVersion : "—" },
                            { label: "Bootloader", value: deviceInfo ? deviceInfo.bootloaderVersion : "—" },
                            { label: "内核", value: deviceInfo ? deviceInfo.kernelVersion : "—" },
                            { label: "运行时间", value: "" }
                        ]
                        delegate: Rectangle {
                            width: (devInfoPageScroll.availableWidth > 400 ? devInfoPageScroll.availableWidth / 2 : devInfoPageScroll.availableWidth) - 4
                            height: 40
                            radius: 6
                            color: deviceInfoPageRoot.isDark ? "#3D3D3D" : "#FFFFFF"
                            border.color: deviceInfoPageRoot.isDark ? "#555" : "#DDD"
                            border.width: 1
                            Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Text {
                                    width: 90
                                    text: modelData.label
                                    color: deviceInfoPageRoot.isDark ? "#999" : "#888"
                                    font.pixelSize: 13
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    text: modelData.label === "运行时间" ? deviceInfoPageRoot.uptimeText : modelData.value
                                    color: deviceInfoPageRoot.isDark ? "#EEE" : "#333"
                                    font.pixelSize: 13
                                    font.bold: true
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════
    // 存储管理页（来源切换 + 真实扫描 + 加载替换默认工程）
    // B6-7: 来源卡片可点选中高亮(内存/SD/USB) + 真实状态 + 列表真实扫描
    //       加载=复制替换默认文件 + 进度条 + 完成弹窗; 主题用 storagePageRoot.isDark
    // ═══════════════════════════════════════════════════
    component StoragePage: Item {
        id: storagePageRoot
        property bool isDark: false
        property int currentSource: 0          // 0=内存 1=SD 2=USB
        property string selFile: ""            // 选中文件路径
        property string selName: ""            // 选中文件名
        property bool replacing: false         // 替换进行中（进度条）
        property int replaceProgress: 0        // 0~100
        property string replaceMsg: ""         // 完成弹窗文本
        property bool showReplaceDialog: false // 完成弹窗

        readonly property var sourceDirs: [
            { name: "内存", dir: "/mnt/user/userdata", status: "内置存储" },
            { name: "SD 卡", dir: "/mnt/sdcard", status: storageInfo ? storageInfo.sdStatusText() : "—" },
            { name: "USB", dir: "/mnt/udisk", status: storageInfo ? storageInfo.usbStatusText() : "—" }
        ]

        ListModel { id: fileListModel }

        // 按来源刷新列表
        function refresh() {
            selFile = ""
            selName = ""
            fileListModel.clear()
            if (!storageInfo) return
            var items = storageInfo.listProjects(sourceDirs[currentSource].dir)
            for (var i = 0; i < items.length; i++) fileListModel.append(items[i])
        }

        // 加载=复制替换默认工程文件 → 进度条 → 完成弹窗
        function doReplace(path, name) {
            if (!storageInfo || replacing) return
            replacing = true
            replaceProgress = 0
            replaceMsg = ""
            // 实际替换（文件小, 同步瞬时完成）; 进度条动画模拟过程
            var ok = storageInfo.replaceDefaultProject(path)
            replaceMsg = ok ? "已替换为 " + name : "替换失败"
            progressAnim.start()
        }

        // 进度条动画（400ms 走完 → 弹窗）
        PropertyAnimation {
            id: progressAnim
            target: storagePageRoot
            property: "replaceProgress"
            to: 100
            duration: 400
            onFinished: {
                replacing = false
                showReplaceDialog = true
            }
        }

        Column {
            anchors.fill: parent
            spacing: 12

            // 来源卡片（可点, 选中高亮主题色边框）
            Grid {
                columns: 3
                spacing: 12
                width: parent.width
                Repeater {
                    model: storagePageRoot.sourceDirs
                    delegate: Rectangle {
                        width: (parent.width - 24) / 3
                        height: 48
                        radius: 8
                        color: storagePageRoot.isDark ? "#3D3D3D" : "#FFFFFF"
                        border.color: storagePageRoot.currentSource === index ? "#1382B1"
                                        : (storagePageRoot.isDark ? "#555" : "#DDD")
                        border.width: storagePageRoot.currentSource === index ? 2 : 1
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text { text: modelData.name; font.pixelSize: 13; font.bold: true
                                   color: storagePageRoot.isDark ? "#EEE" : "#333" }
                            Text { text: modelData.status; font.pixelSize: 11; color: "#999" }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                storagePageRoot.currentSource = index
                                storagePageRoot.refresh()
                            }
                        }
                    }
                }
            }

            // 工程文件列表
            Rectangle {
                width: parent.width
                height: parent.height - 72
                radius: 8
                color: storagePageRoot.isDark ? "#3D3D3D" : "#FFFFFF"   // B6-7: 主题修正(原 parent.isDark 恒白)
                border.color: storagePageRoot.isDark ? "#555" : "#DDD"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text { text: "工程文件列表（" + sourceDirs[currentSource].name + "）"
                           color: storagePageRoot.isDark ? "#CCC" : "#555"; font.pixelSize: 13; font.bold: true }

                    Repeater {
                        model: fileListModel
                        delegate: Rectangle {
                            width: parent.width
                            height: 40
                            radius: 4
                            color: storagePageRoot.selFile === modelData.path ? "#E3F2FD" : "transparent"
                            // 文件名 + 大小（左侧）
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 76   // 留出右侧加载按钮空间(加大)
                                spacing: 8
                                Text { text: "📄"; font.pixelSize: 16; verticalAlignment: Text.AlignVCenter }
                                Text { text: modelData.name; color: storagePageRoot.isDark ? "#EEE" : "#333"
                                       font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                                Text { text: modelData.sizeText; color: "#999"; font.pixelSize: 11
                                       verticalAlignment: Text.AlignVCenter }
                            }
                            // 加载按钮（B6-7: 加大 60×28, z:1 保证不被行 MouseArea 拦截）
                            Rectangle {
                                z: 1
                                width: 60; height: 28; radius: 4; color: "#1382B1"
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                Text { anchors.centerIn: parent; text: "加载"; color: "white"; font.pixelSize: 12 }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: storagePageRoot.doReplace(modelData.path, modelData.name)
                                }
                            }
                            // 行点击=选中（下层; 按钮 z:1 在上层可点）
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    storagePageRoot.selFile = modelData.path
                                    storagePageRoot.selName = modelData.name
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── 替换进度条覆盖层 ──
        Rectangle {
            z: 100
            anchors.fill: parent
            color: "#80000000"
            visible: storagePageRoot.replacing
            Rectangle {
                width: parent.width * 0.7
                height: 90
                radius: 10
                anchors.centerIn: parent
                color: "white"
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Text { text: "正在替换默认工程文件..."; font.pixelSize: 14; color: "#333" }
                    Rectangle {
                        width: 260; height: 14; radius: 7; color: "#EEE"
                        Rectangle {
                            width: parent.width * storagePageRoot.replaceProgress / 100
                            height: 14; radius: 7; color: "#1382B1"
                        }
                    }
                }
            }
        }

        // ── 替换完成弹窗 ──
        Rectangle {
            z: 101
            anchors.fill: parent
            color: "#80000000"
            visible: storagePageRoot.showReplaceDialog
            Rectangle {
                width: parent.width * 0.6
                height: 130
                radius: 10
                anchors.centerIn: parent
                color: "white"
                Column {
                    anchors.centerIn: parent
                    spacing: 14
                    Text { text: storagePageRoot.replaceMsg; font.pixelSize: 15; color: "#333" }
                    Rectangle {
                        width: 90; height: 32; radius: 6; color: "#1382B1"
                        anchors.horizontalCenter: parent.horizontalCenter
                        Text { anchors.centerIn: parent; text: "确定"; color: "white"; font.pixelSize: 13 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                storagePageRoot.showReplaceDialog = false
                                storagePageRoot.refresh()
                            }
                        }
                    }
                }
            }
        }

        Component.onCompleted: refresh()
    }

    // ═══════════════════════════════════════════════════
    // 通信控制页（MQTT + 网络开关）
    // ═══════════════════════════════════════════════════
    component NetworkPage: Item {
        id: networkPageRoot
        property bool isDark: false
        property bool mqttOn: true
        property bool netOn: true

        Column {
            anchors.fill: parent
            spacing: 16

            // MQTT 开关
            Rectangle {
                width: parent.width
                height: 90
                radius: 8
                color: networkPageRoot.isDark ? "#3D3D3D" : "#FFFFFF"
                border.color: networkPageRoot.isDark ? "#555" : "#DDD"
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Row {
                        spacing: 12
                        Text { text: "MQTT 通信:"; color: networkPageRoot.isDark ? "#EEE" : "#333"; font.pixelSize: 14; font.bold: true }
                        ToggleSwitch {
                            isOn: networkPageRoot.mqttOn
                            onToggled: networkPageRoot.mqttOn = !networkPageRoot.mqttOn
                        }
                        Text { text: networkPageRoot.mqttOn ? "🟢 已连接" : "⚪ 已关闭"; color: networkPageRoot.mqttOn ? "#4CAF50" : "#999"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    }
                    Text { text: "Broker: 192.168.1.50:1883    主题: hmi/device001/#"; color: "#999"; font.pixelSize: 11 }
                }
            }

            // 网络开关
            Rectangle {
                width: parent.width
                height: 90
                radius: 8
                color: networkPageRoot.isDark ? "#3D3D3D" : "#FFFFFF"
                border.color: networkPageRoot.isDark ? "#555" : "#DDD"
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Row {
                        spacing: 12
                        Text { text: "网络接口:"; color: networkPageRoot.isDark ? "#EEE" : "#333"; font.pixelSize: 14; font.bold: true }
                        ToggleSwitch {
                            isOn: networkPageRoot.netOn
                            onToggled: networkPageRoot.netOn = !networkPageRoot.netOn
                        }
                        Text { text: networkPageRoot.netOn ? "🟢 已连接" : "🔴 已断开"; color: networkPageRoot.netOn ? "#4CAF50" : "#D32F2F"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    }
                    Text { text: "IP: " + (deviceInfo ? deviceInfo.ipAddress : "—") + "    MAC: " + (deviceInfo ? deviceInfo.macAddress : "—"); color: "#999"; font.pixelSize: 11 }
                }
            }
        }
    }

    // ── 开关组件 ──
    component ToggleSwitch: Rectangle {
        id: toggle
        width: 52
        height: 28
        radius: 14
        color: isOn ? "#4CAF50" : "#999"

        property bool isOn: false
        signal toggled()

        Rectangle {
            width: 24
            height: 24
            radius: 12
            color: "white"
            x: toggle.isOn ? toggle.width - 26 : 2
            y: 2
            Behavior on x { NumberAnimation { duration: 120 } }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: toggle.toggled()
        }
    }
}
