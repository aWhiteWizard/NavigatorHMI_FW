import QtQuick 2.15
import QtQuick.Controls 2.15

// ═══════════════════════════════════════════════════════════
// NavigatorHMI FW 主窗口（800×480 设备屏 / Windows 仿真器同尺寸）
// 结构：顶部导航栏 + 画面区 + 底部报警横幅
// 控件渲染、变量绑定、报警由 runtime/ui 模块提供（迭代接入）
// ═══════════════════════════════════════════════════════════
Window {
    id: root
    width: 800
    height: 480
    visible: true
    title: qsTr("NavigatorHMI")
    color: "#203864"

    // ── 顶部导航栏（画面切换按钮，由 hmi.screens 填充）──
    Rectangle {
        id: navBar
        height: 40
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#1B2A4A"
        Row {
            id: navRow
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4
            // TODO: Repeater { model: hmi.screens } → 画面切换按钮
        }
    }

    // ── 画面区（控件渲染，由 hmi.widgets 填充）──
    Item {
        id: screenArea
        anchors.top: navBar.bottom
        anchors.bottom: alarmBanner.top
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true
        // TODO: Repeater { model: hmi.widgets } → 按控件类型渲染
    }

    // ── 报警横幅（Active 报警显示：规则名 + 值 + 等级）──
    Rectangle {
        id: alarmBanner
        height: 32
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#3A2A2A"
        visible: false
        Text {
            id: alarmText
            anchors.fill: parent
            color: "#FF6B6B"
            font.pixelSize: 16
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
        }
        // TODO: 绑定 hmi.activeAlarmText；多条报警时可滚动/轮播
    }
}
