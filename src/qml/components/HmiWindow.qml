// B-4: HmiWindow——窗口控件组件（userview/alarmview/robotlist 三模板）
// 契约: windowType 0=UserView 1=AlarmView 2=RobotList
import QtQuick 2.15

Rectangle {
    id: root
    width: 200
    height: 120
    color: "#FAFAFA"
    border.color: "#999999"
    border.width: 1
    radius: 3

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property int windowType: 0
    property string winTitle: ""
    property bool showTitleBar: true
    property bool showHistory: false
    property string selectedTag: ""
    property double cardWidth: 0
    property double cardHeight: 0
    property bool showUserName: false
    property bool showRole: false
    property bool showMode: false
    property bool cardShowNumber: false
    property bool cardShowStatus: false
    property bool cardShowLocation: false
    property string boundDevice: ""
    // 通用字段（生成器并集输出; proto:36 title 复用承载边框色）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""
     property string text: ""  // 并集字段容忍(生成器统一输出)
     property string content: ""  // 并集字段容忍(生成器统一输出)
     property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
     property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
     property double fontSize: 0  // 并集字段容忍(生成器统一输出)
     property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
     property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
     property string textColor: ""  // 并集字段容忍(生成器统一输出)
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
     property bool isReadOnly: false  // 并集字段容忍(生成器统一输出)
     property double x2: 0  // 并集字段容忍(生成器统一输出)
     property double y2: 0  // 并集字段容忍(生成器统一输出)
     property string dtText: ""  // 并集字段容忍(生成器统一输出)
     property string dtFormat: ""  // 并集字段容忍(生成器统一输出)

    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // 标题栏
    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.showTitleBar ? 22 : 0
        color: "#1565C0"
        visible: root.showTitleBar
        clip: true

        Text {
            anchors.fill: parent
            anchors.margins: 4
            text: root.winTitle !== "" ? root.winTitle : defaultTitle()
            color: "white"
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }

    function defaultTitle() {
        return root.windowType === 0 ? "用户视图"
             : root.windowType === 1 ? "报警视图"
             : "机器人列表"
    }

    // 内容区
    Item {
        id: content
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true

        // UserView (0): 用户名/角色/模式
        Column {
            visible: root.windowType === 0
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4
            Text { text: root.showUserName ? "用户: 管理员" : ""; visible: root.showUserName; font.pixelSize: 12 }
            Text { text: root.showRole ? "角色: 管理员" : ""; visible: root.showRole; font.pixelSize: 12 }
            Text { text: root.showMode ? "模式: 运行" : ""; visible: root.showMode; font.pixelSize: 12 }
            Text { text: "登录/注销"; visible: root.winTitle === "" && root.windowType === 0; font.pixelSize: 12; color: "#1565C0" }
        }

        // AlarmView (1): 活动报警列表
        Column {
            visible: root.windowType === 1
            anchors.fill: parent
            anchors.margins: 6
            spacing: 2
            Text { text: "活动报警:"; font.pixelSize: 12; font.bold: true }
            Text { text: "  [高] 温度超限"; color: "#D32F2F"; font.pixelSize: 11 }
            Text { text: "  [警] 压力偏低"; color: "#F57C00"; font.pixelSize: 11 }
            Rectangle {
                width: 60; height: 18; radius: 3; color: "#1565C0"
                Text { anchors.centerIn: parent; text: "全部确认"; color: "white"; font.pixelSize: 10 }
                MouseArea { anchors.fill: parent; onClicked: root.hmiAck() }
            }
        }

        // RobotList (2): 卡片网格
        GridView {
            visible: root.windowType === 2
            anchors.fill: parent
            anchors.margins: 4
            cellWidth: root.cardWidth > 0 ? root.cardWidth : 60
            cellHeight: root.cardHeight > 0 ? root.cardHeight : 50
            model: 3
            delegate: Rectangle {
                width: root.cardWidth > 0 ? root.cardWidth : 56
                height: root.cardHeight > 0 ? root.cardHeight : 46
                radius: 3
                color: "#E3F2FD"
                border.color: "#90CAF9"
                border.width: 1
                Column {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 2
                    Text { text: root.cardShowNumber ? "R0" + (index + 1) : ""; visible: root.cardShowNumber; font.pixelSize: 10; font.bold: true }
                    Text { text: root.cardShowStatus ? "运行" : ""; visible: root.cardShowStatus; font.pixelSize: 9; color: "#4CAF50" }
                    Text { text: root.cardShowLocation ? "A区-" + (index + 1) + "号位" : ""; visible: root.cardShowLocation; font.pixelSize: 9 }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.hmiSelect()
                }
            }
        }
    }
}
