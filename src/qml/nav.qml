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
    property var onLoadProjectFile: null    // 存储管理: 加载选中文件
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
                onLoadFile: function(path) {
                    if (navRoot.onLoadProjectFile) navRoot.onLoadProjectFile(path)
                }
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
        // 内容: 5 项信息
        property var entries: [
            { label: "IP", value: "192.168.1.146" },
            { label: "MAC", value: "00:11:22:33:44:55" },
            { label: "版本", value: "v1.1.0" },
            { label: "内核", value: "6.1.141" },
            { label: "运行", value: "72h 15m" }
        ]

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
        ScrollView {
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
                width: parent.width
                columns: parent.width > 400 ? 2 : 1
                spacing: 4
                Repeater {
                    model: infoBox.entries
                    delegate: Text {
                        width: (parent.width > 400 ? parent.width / 2 : parent.width) - 6
                        text: modelData.label + ":  " + modelData.value
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

        ScrollView {
            anchors.fill: parent
            clip: true

            Column {
                width: parent.width
                spacing: 8
                Grid {
                    columns: parent.parent.width > 400 ? 2 : 1
                    spacing: 8
                    width: parent.width
                    Repeater {
                        model: [
                            { label: "IP 地址", value: "192.168.1.146" },
                            { label: "MAC", value: "00:11:22:33:44:55" },
                            { label: "软件版本", value: "v1.1.0" },
                            { label: "Bootloader", value: "v1.04" },
                            { label: "内核", value: "6.1.141" },
                            { label: "运行时间", value: "72h 15m" }
                        ]
                        delegate: Rectangle {
                            width: (parent.width > 400 ? parent.width / 2 : parent.width) - 4
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
                                    text: modelData.value
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
    // 存储管理页（SD/USB 检测 + 文件列表 + 加载）
    // ═══════════════════════════════════════════════════
    component StoragePage: Item {
        id: storagePageRoot
        property bool isDark: false
        signal loadFile(string path)

        Column {
            anchors.fill: parent
            spacing: 12

            // 存储状态（两列）
            Grid {
                columns: 2
                spacing: 12
                width: parent.width
                Rectangle {
                    width: parent.parent.width / 2 - 6
                    height: 48
                    radius: 8
                    color: storagePageRoot.isDark ? "#3D3D3D" : "#FFFFFF"
                    border.color: storagePageRoot.isDark ? "#555" : "#DDD"
                    Text { anchors.centerIn: parent; text: "SD 卡: ✅ 已插入 (16GB)"; font.pixelSize: 13; color: storagePageRoot.isDark ? "#EEE" : "#333" }
                }
                Rectangle {
                    width: parent.parent.width / 2 - 6
                    height: 48
                    radius: 8
                    color: storagePageRoot.isDark ? "#3D3D3D" : "#FFFFFF"
                    border.color: storagePageRoot.isDark ? "#555" : "#DDD"
                    Text { anchors.centerIn: parent; text: "USB: ❌ 未插入"; font.pixelSize: 13; color: storagePageRoot.isDark ? "#EEE" : "#333" }
                }
            }

            // 工程文件列表
            Rectangle {
                width: parent.width
                height: parent.height - 72
                radius: 8
                color: parent.isDark ? "#3D3D3D" : "#FFFFFF"
                border.color: parent.isDark ? "#555" : "#DDD"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text { text: "工程文件列表"; color: parent.isDark ? "#CCC" : "#555"; font.pixelSize: 13; font.bold: true }

                    // 文件项（模拟 SD 卡文件; 实际扫描后续接）
                    Repeater {
                        model: [
                            { name: "产线监控.navihmi", size: "2.3MB" },
                            { name: "测试工程.navihmi", size: "0.8MB" },
                            { name: "demo.navihmi", size: "1.1MB" }
                        ]
                        delegate: Rectangle {
                            width: parent.width
                            height: 36
                            radius: 4
                            color: mouse.pressed ? "#E3F2FD" : "transparent"
                            // 文件名 + 大小（左侧）
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 60   // 留出右侧加载按钮空间
                                spacing: 8
                                Text { text: "📄"; font.pixelSize: 16; verticalAlignment: Text.AlignVCenter }
                                Text { text: modelData.name; color: storagePageRoot.isDark ? "#EEE" : "#333"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                                Text { text: modelData.size; color: "#999"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                            }
                            // 加载按钮（右侧, 独立定位）
                            Rectangle {
                                width: 48; height: 22; radius: 4; color: "#1565C0"
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                Text { anchors.centerIn: parent; text: "加载"; color: "white"; font.pixelSize: 11 }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: storagePageRoot.loadFile("/mnt/user/userdata/app.navihmi")
                                }
                            }
                            MouseArea {
                                id: mouse
                                anchors.fill: parent
                            }
                        }
                    }
                }
            }
        }
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
                    Text { text: "IP: 192.168.1.146    MAC: 00:11:22:33:44:55"; color: "#999"; font.pixelSize: 11 }
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
