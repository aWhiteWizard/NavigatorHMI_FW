// P-5: HmiHistoryView——历史记录控件（2026-09-02, v1.1-design §5.3 ③）
// 显示选定变量的历史记录列表（DataLogger.queryTagHistory）；变量列表多变量——运行时顶部下拉切换选变量。
// 数据库路径属性：FW 单库架构，非默认值由 DataLogger.setDbPath 告警（P-5 首版恒用默认库）。
// 并集字段容忍 + 19 事件信号（生成器统一输出防 Cannot assign）。
import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: 360
    height: 200
    color: "#FFFFFF"
    border.color: "#B0B8C4"
    border.width: 1
    radius: 2
    clip: true

    // ── 历史专有属性（生成器输出）──
    property string objectName: ""
    property var historyTags: []      // 变量列表（多变量）
    property string historyDbPath: "" // 数据库路径（空=默认）

    // ── 并集字段容忍（生成器统一输出，组件各自忽略）──
    property string boundTag: ""
    property string text: ""
    property string content: ""
    property string hAlign: "Left"
    property string fontFamily: ""
    property double fontSize: 12
    property string fontWeight: "Normal"
    property string fontStyle: "Normal"
    property string textDecoration: "None"
    property string textColor: "#000000"
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string imagePath: ""
    property string stretchMode: ""
    property string listRef: ""
    property int defaultIndex: 0
    property double value: 0
    property double min: 0
    property double max: 100
    property string fillStyle: "Solid"
    property bool isOn: false
    property string labelOn: ""
    property string labelOff: ""
    property double x2: 0
    property double y2: 0
    property string title: ""
    property string dtText: ""
    property string dtFormat: ""
    property int windowType: 0
    property int displayMode: 0
    property string winTitle: ""
    property bool showTitleBar: true
    property string selectedTag: ""
    property int cardWidth: 0
    property int cardHeight: 0
    property bool isChecked: false
    property bool isReadOnly: false
    property string boundDevice: ""
    property int trendMode: 0
    property string trendTagA: ""
    property string trendTagB: ""

    // ── 19 事件信号 ──
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // 当前选中变量 + 历史行
    property string currentTag: historyTags.length > 0 ? historyTags[0] : ""
    property var historyRows: []
    property bool loaded: false

    function loadHistory(tagName) {
        if (tagName === "") { historyRows = []; return }
        currentTag = tagName
        historyRows = dataLogger ? dataLogger.queryTagHistory(tagName, 200) : []   // 最近 200 条（时间正序 旧→新）
        // 展示顺序时间正序（顶部最旧）——List 顶部起始即所见，无需定位（审查 🔵）
    }

    // 标题栏：变量下拉 + 标题
    Rectangle {
        id: titleBar
        height: 24
        width: parent.width
        color: "#F0F2F5"
        border.color: "#D0D6DE"
        border.width: 1
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: "变量历史"
            font.pixelSize: 10
            color: "#555"
            font.bold: true
        }
        // 变量下拉（多变量切换——用户拍板：运行时下拉切换选变量看历史）
        ComboBox {
            id: tagCombo
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(parent.width - 80, 160)
            height: 18
            model: root.historyTags
            currentIndex: root.historyTags.indexOf(root.currentTag) >= 0 ? root.historyTags.indexOf(root.currentTag) : 0
            font.pixelSize: 9
            visible: root.historyTags.length > 1
            onActivated: root.loadHistory(root.historyTags[currentIndex])
        }
    }

    // 表头（时间/值）
    Rectangle {
        id: listHeader
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 16
        color: "#ECEFF1"
        Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "时间"; font.pixelSize: 9; color: "#666"; font.bold: true }
        Text { anchors.right: parent.right; anchors.rightMargin: 30; anchors.verticalCenter: parent.verticalCenter; text: "值"; font.pixelSize: 9; color: "#666"; font.bold: true }
    }

    ListView {
        id: listView
        anchors.top: listHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: root.historyRows
        delegate: Rectangle {
            width: listView.width
            height: 18
            color: index % 2 === 0 ? "#FFFFFF" : "#F5F5F5"
            property var rowData: modelData
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: rowData.ts
                font.pixelSize: 9
                color: "#444"
                width: parent.width - 70
                elide: Text.ElideRight
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: rowData.value
                font.pixelSize: 9
                color: "#1565C0"
                font.bold: true
            }
        }
        // 空态
        Text {
            anchors.centerIn: parent
            text: root.historyTags.length === 0 ? "未配置变量（属性面板填变量列表）" : "暂无历史数据（变量开始采样后出现）"
            font.pixelSize: 11
            color: "#999999"
            visible: root.historyRows.length === 0
        }
    }

    // 首次可见加载（组件加载时 db 可能未就绪；延迟一拍）
    Component.onCompleted: {
        if (dataLogger !== undefined && dataLogger !== null && !root.loaded) {
            // 数据库路径：与设备默认库比较（审查 🔵——避免非默认路径误告警）；默认/空则直接用
            var defPath = dataLogger.dbPath ? dataLogger.dbPath() : ""
            if (root.historyDbPath !== "" && root.historyDbPath !== "navihmi_history.db" && root.historyDbPath !== defPath)
                dataLogger.setDbPath(root.historyDbPath)
            root.loadHistory(root.currentTag)
            root.loaded = true
        }
    }
}
