import QtQuick 2.15
import QtQuick.Controls 2.15

// ═══════════════════════════════════════════════════════════
// 导航界面（无工程时显示）
// 开始工程 / 校准 / 设备信息 / 系统管理（system-manager 设计）
// ═══════════════════════════════════════════════════════════
Item {
    id: navRoot
    width: 1024
    height: 600

    // 主壳注入: 开始工程回调
    property var onStartProject: null
    property var onCalibrate: null
    property var onDeviceInfo: null
    property var onSystemManage: null

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#1B2A4A"

        Text {
            anchors.centerIn: parent
            text: "NavigatorHMI"
            color: "white"
            font.pixelSize: 22
            font.bold: true
        }
    }

    Column {
        id: menu
        anchors.centerIn: parent
        spacing: 20

        NavButton {
            text: "开始工程"
            onClicked: {
                if (navRoot.onStartProject) navRoot.onStartProject()
            }
        }
        NavButton {
            text: "触摸校准"
            onClicked: {
                if (navRoot.onCalibrate) navRoot.onCalibrate()
            }
        }
        NavButton {
            text: "设备信息"
            onClicked: {
                if (navRoot.onDeviceInfo) navRoot.onDeviceInfo()
            }
        }
        NavButton {
            text: "系统管理"
            onClicked: {
                if (navRoot.onSystemManage) navRoot.onSystemManage()
            }
        }
    }

    // 底部状态
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: "#0F1A33"
        Text {
            anchors.centerIn: parent
            text: "NavigatorHMI FW v1.1 — 无工程"
            color: "#8899BB"
            font.pixelSize: 12
        }
    }

    // ── 导航按钮组件 ──
    component NavButton: Rectangle {
        id: btn
        width: 320
        height: 56
        radius: 6
        color: mouse.pressed ? "#2A4A8A" : "#1B2A4A"
        border.color: "#3A5A9A"
        border.width: 1

        property string text: ""
        signal clicked()

        Text {
            anchors.centerIn: parent
            text: btn.text
            color: "white"
            font.pixelSize: 18
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            onClicked: btn.clicked()
        }
    }
}
