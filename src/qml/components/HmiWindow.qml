// B-4: HmiWindow——窗口控件组件（userview/alarmview/robotlist 三模板）
// G-1a(2026-08-23): UserView 重写——登录/注销/⚙管理（改用户名/密码/组），与 PC 端用户系统一一对应
// 契约: windowType 0=UserView 1=AlarmView 2=RobotList
import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: 200
    height: 120
    // 审查 M7: fillColor 渲染生效（PC set_property 键）; borderColor 走 proto:36 title 槽位（PC 既有契约:
    // ProjectGenerator WindowWidget 写 title=BorderColor, FW 解析 title）——border 绑定 title, 补全边框色链路
    color: root.fillColor !== "" ? root.fillColor : "#FAFAFA"
    border.color: root.title !== "" ? root.title : "#999999"
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
    // G-1c: robotSlots 逐组变量绑定（每组 {id,status,location,detail,oper} = 变量名）
    property var robotSlots: []
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
    signal hmiClicked()
    signal hmiPressed()
    signal hmiReleased()
    signal hmiValueChanged()
    signal hmiAlarmTrigger()
    signal hmiAlarmAck()
    signal hmiAlarmClear()
    signal hmiTimer()
    signal hmiSystemStart()
    signal hmiSystemShutdown()
    signal hmiScreenLoad()
    signal hmiScreenUnload()
    signal hmiInput()
    signal hmiOn()
    signal hmiOff()
    signal hmiProgressComplete()

    // G-1a: 用户系统接线——登录/注销触发 onUserChanged 事件（PC 端 EventType.OnUserChanged）
    Connections {
        target: userSystem
        function onUserChanged(userName) { root.hmiUserChanged() }
    }

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
            anchors.rightMargin: 26   // 给 ⚙ 管理图标让位
            text: root.winTitle !== "" ? root.winTitle : defaultTitle()
            color: "white"
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
        // G-1b: AlarmView 标题栏右侧活动报警数
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            text: (root.windowType === 1 && alarmEngine) ? "(" + alarmEngine.activeCount + ")" : ""
            color: "#FFEB3B"
            font.pixelSize: 11
            font.bold: true
            visible: root.windowType === 1
        }

        // ⚙ 管理图标（仅当前登录用户有 UserManage 权限显示；UserView 专属）
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 5
            width: 18; height: 18
            radius: 9
            color: "transparent"
            visible: root.windowType === 0 && userSystem && userSystem.canManage
            Text {
                anchors.centerIn: parent
                text: "⚙"
                color: "white"
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    manageDialog.userName = userSystem.currentUserName()
                    manageDialog.openDialog()
                }
            }
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

        // ══════════ UserView (0): 用户系统（G-1a 重写）══════════
        // 紧凑布局（窗口高 120 装下 头像+信息+按钮）：头像与用户名同行
        Item {
            visible: root.windowType === 0
            anchors.fill: parent

            // 头像 + 用户名（同行，横向排布）
            Rectangle {
                id: avatarBox
                anchors.top: parent.top
                anchors.topMargin: 4
                anchors.left: parent.left
                anchors.leftMargin: 8
                width: 28; height: 28
                radius: 14
                color: userSystem && userSystem.loggedIn ? "#1565C0" : "#BBBBBB"
                Text {
                    anchors.centerIn: parent
                    text: userSystem && userSystem.loggedIn
                          ? userSystem.userName.length > 0 ? userSystem.userName.charAt(0).toUpperCase() : "?"
                          : "?"
                    color: "white"
                    font.pixelSize: 14
                    font.bold: true
                }
            }
            // 用户名（showUserName 开关）
            Text {
                anchors.left: avatarBox.right
                anchors.leftMargin: 6
                anchors.verticalCenter: avatarBox.verticalCenter
                text: userSystem && userSystem.loggedIn ? userSystem.userName : "未登录"
                visible: root.showUserName
                font.pixelSize: 12
                font.bold: userSystem && userSystem.loggedIn
                color: userSystem && userSystem.loggedIn ? "#333333" : "#999999"
                elide: Text.ElideRight
                width: parent.width - avatarBox.x - avatarBox.width - 14
            }
            // 强制改密提示（初始管理员首次登录后）——右上角右对齐, 避开左侧角色/模式行
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.top: parent.top
                anchors.topMargin: 2
                text: (userSystem && userSystem.loggedIn && userSystem.mustChangePassword)
                      ? "⚠ 需修改初始密码" : ""
                color: "#E65100"
                font.pixelSize: 9
                visible: userSystem && userSystem.loggedIn && userSystem.mustChangePassword
            }

            // 角色/模式（showRole/showMode 开关；未登录不显示）
            Column {
                anchors.top: avatarBox.bottom
                anchors.topMargin: 4
                anchors.left: parent.left
                anchors.leftMargin: 8
                spacing: 1
                Text {
                    text: userSystem && userSystem.loggedIn
                          ? "角色: " + userSystem.group : ""
                    visible: root.showRole
                    font.pixelSize: 10
                    color: "#666666"
                }
                Text {
                    text: userSystem && userSystem.loggedIn ? "模式: 运行" : ""
                    visible: root.showMode
                    font.pixelSize: 10
                    color: "#666666"
                }
            }

            // 操作按钮（按登录态切换：未登录→[登录]，已登录→[注销]）
            Rectangle {
                id: loginBtn
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                anchors.horizontalCenter: parent.horizontalCenter
                width: 60; height: 22
                radius: 4
                color: (userSystem && userSystem.loggedIn) ? "#E53935" : "#1565C0"
                Text {
                    anchors.centerIn: parent
                    text: (userSystem && userSystem.loggedIn) ? "注销" : "登录"
                    color: "white"
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (userSystem && userSystem.loggedIn) {
                            userSystem.logout()
                        } else {
                            loginDialog.openDialog()
                        }
                    }
                }
            }
        }

        // ══════════ AlarmView (1): 活动报警列表（G-1b 重写——变量阈值驱动模拟源）══════════
        Item {
            visible: root.windowType === 1
            anchors.fill: parent
            clip: true

            // 表头：全选 + 列名（时间/级别/内容）
            Rectangle {
                id: alarmHeader
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 16
                color: "#ECEFF1"
                // 表头全选（勾选全部未确认）
                Rectangle {
                    id: headerCheck
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12; height: 12
                    radius: 2
                    color: headerCheck.checked ? "#1565C0" : "white"
                    border.color: "#999999"
                    border.width: 1
                    property bool checked: false
                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        color: "white"
                        font.pixelSize: 9
                        visible: headerCheck.checked
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            headerCheck.checked = !headerCheck.checked
                        }
                    }
                }
                Text {
                    anchors.left: headerCheck.right
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    text: "时间"
                    font.pixelSize: 9
                    color: "#666666"
                    font.bold: true
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 52
                    anchors.verticalCenter: parent.verticalCenter
                    text: "级别"
                    font.pixelSize: 9
                    color: "#666666"
                    font.bold: true
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 78
                    anchors.verticalCenter: parent.verticalCenter
                    text: "内容"
                    font.pixelSize: 9
                    color: "#666666"
                    font.bold: true
                }
            }

            // 报警列表（ScrollView + Column，时间倒序由 AlarmEngine 保证）
            ListView {
                id: alarmList
                anchors.top: alarmHeader.bottom
                anchors.bottom: ackBar.top
                anchors.left: parent.left
                anchors.right: parent.right
                clip: true
                model: alarmEngine ? alarmEngine.activeAlarms : []
                delegate: Rectangle {
                    width: alarmList.width
                    height: 17
                    color: index % 2 === 0 ? "#FFFFFF" : "#F5F5F5"
                    border.color: alarmData.acked ? "#CCCCCC" : "#DDDDDD"
                    border.width: 1
                    property var alarmData: modelData

                    // 审查 MINOR: 行勾选框已无消费方（批量确认=ackAll）——移除死 UI, 仅保留行确认按钮
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 3
                        anchors.verticalCenter: parent.verticalCenter
                        text: alarmData.time
                        font.pixelSize: 9
                        color: alarmData.acked ? "#AAAAAA" : "#444444"
                    }
                    // 级别色标（紧急红/重要橙/警告黄/提示蓝）
                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 52
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 10
                        radius: 2
                        color: alarmData.level === 0 ? "#D32F2F"
                             : alarmData.level === 1 ? "#F57C00"
                             : alarmData.level === 2 ? "#F9A825"
                             : "#1976D2"
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 78
                        anchors.verticalCenter: parent.verticalCenter
                        text: alarmData.message
                        font.pixelSize: 9
                        color: alarmData.acked ? "#AAAAAA" : "#333333"
                        elide: Text.ElideRight
                        width: parent.width - 120
                    }
                    // 行确认按钮（仅未确认）
                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 26; height: 13
                        radius: 2
                        color: alarmData.acked ? "#CCCCCC" : "#1565C0"
                        visible: !alarmData.acked
                        Text {
                            anchors.centerIn: parent
                            text: "确认"
                            color: "white"
                            font.pixelSize: 8
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (alarmEngine) alarmEngine.ackAlarm(alarmData.id)
                                root.hmiAck()
                            }
                        }
                    }
                }
                // 空态
                Text {
                    anchors.centerIn: parent
                    text: "暂无活动报警"
                    font.pixelSize: 11
                    color: "#999999"
                    visible: (alarmEngine ? alarmEngine.activeAlarms.length : 0) === 0
                }
            }

            // 底部：批量确认（表头全选后点此批量确认）
            Rectangle {
                id: ackBar
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 20
                color: "#ECEFF1"
                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 56; height: 15
                    radius: 2
                    color: headerCheck.checked ? "#1565C0" : "#AAAAAA"
                    Text {
                        anchors.centerIn: parent
                        text: "批量确认"
                        color: "white"
                        font.pixelSize: 8
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: headerCheck.checked
                        onClicked: {
                            // 审查 M3: 表头全选=确认全部活动报警（含不可见行; 逐条 ACK 由 AlarmEngine 发）
                            if (alarmEngine) alarmEngine.ackAll()
                            headerCheck.checked = false
                            root.hmiAck()
                        }
                    }
                }
            }
        }

        // ══════════ RobotList (2): 机器人卡片网格（G-1c 重写——逐组变量绑定）══════════
        // 显示规则（DESIGN-WINDOWS L97-107 用户确认）：编号变量有值→显示卡片；状态/位置无值→置灰；
        // 状态 0=空闲/1=运行/2=故障 着色；异常值(>2/<0/JSON失败)→红框标注；点击→详情弹窗+操作+onSelect
        Item {
            id: robotListRoot
            visible: root.windowType === 2
            anchors.fill: parent
            clip: true

            // 卡片数据快照（变量值变化时刷新——dataManager.value() 无依赖跟踪, 需显式重算）
            property var cards: []
            property bool anyCard: false

            function refreshCards() {
                var arr = []
                for (var i = 0; i < root.robotSlots.length; i++) {
                    var s = root.robotSlots[i]
                    var idVal = (dataManager && s.id !== "") ? dataManager.value(s.id) : ""
                    var statusVal = (dataManager && s.status !== "") ? dataManager.value(s.status) : ""
                    var locVal = (dataManager && s.location !== "") ? dataManager.value(s.location) : ""
                    arr.push({
                        idVar: s.id || "", statusVar: s.status || "", locVar: s.location || "",
                        detailVar: s.detail || "", operVar: s.oper || "",
                        idVal: idVal, statusVal: statusVal, locVal: locVal
                    })
                }
                cards = arr
                // 任一编号变量有值 → 显示列表（无任何编号 → 空态）
                anyCard = false
                for (var j = 0; j < arr.length; j++)
                    if (arr[j].idVal !== "" && arr[j].idVal !== null && arr[j].idVal !== undefined) { anyCard = true; break }
            }
            Component.onCompleted: refreshCards()

            Connections {
                target: dataManager
                function onValueChanged(tagName, value) {
                    // 仅本窗口绑定变量变化时刷新（iofield 改状态/位置/编号 → 卡片更新）
                    for (var i = 0; i < root.robotSlots.length; i++) {
                        var s = root.robotSlots[i]
                        if (s.id === tagName || s.status === tagName || s.location === tagName) {
                            robotListRoot.refreshCards()
                            return
                        }
                    }
                }
            }

            // 空态（无任何编号变量有值）
            Text {
                anchors.centerIn: parent
                text: "暂无机器人数据"
                font.pixelSize: 11
                color: "#999999"
                visible: !robotListRoot.anyCard
            }

            GridView {
                anchors.fill: parent
                anchors.margins: 4
                visible: robotListRoot.anyCard
                cellWidth: root.cardWidth > 0 ? root.cardWidth : 58
                cellHeight: root.cardHeight > 0 ? root.cardHeight : 48
                model: robotListRoot.cards
                delegate: Rectangle {
                    id: card
                    width: root.cardWidth > 0 ? root.cardWidth : 54
                    height: root.cardHeight > 0 ? root.cardHeight : 44
                    radius: 3
                    // 状态色：0=空闲灰 / 1=运行绿 / 2=故障红；无值=置灰；异常值=白底红框
                    property var cardData: modelData
                    property string statusText: ""
                    property string statusColor: "#90A4AE"
                    property bool statusAbnormal: false

                    function computeStatus() {
                        var v = cardData.statusVal
                        if (v === "" || v === null || v === undefined) {
                            statusText = "离线"; statusColor = "#B0BEC5"
                        } else {
                            var n = Number(v)   // 审查 MINOR: Number() + isNaN——非数值串 "abc" → NaN 判异常
                            if (!isNaN(n) && n === 0) { statusText = "空闲"; statusColor = "#90A4AE" }
                            else if (!isNaN(n) && n === 1) { statusText = "运行"; statusColor = "#4CAF50" }
                            else if (!isNaN(n) && n === 2) { statusText = "故障"; statusColor = "#D32F2F" }
                            else { statusText = "异常:" + v; statusColor = "#D32F2F" }
                        }
                        // 审查 M5(DESIGN L103): 异常标注三源——状态越界/非数值/位置无值（离线态=状态无值不标红, 置灰即常态）
                        var stOk = (v !== "" && v !== null && v !== undefined)
                        var stNum = stOk ? Number(v) : NaN
                        statusAbnormal = (stOk && (isNaN(stNum) || stNum > 2 || stNum < 0))
                                         || (stOk && (cardData.locVal === "" || cardData.locVal === null || cardData.locVal === undefined))
                    }
                    Component.onCompleted: computeStatus()
                    onCardDataChanged: computeStatus()

                    color: cardData.statusVal === "" || cardData.statusVal === null || cardData.statusVal === undefined
                           ? "#ECEFF1" : "#E3F2FD"
                    border.color: statusAbnormal ? "#D32F2F" : (cardData.statusVal === "" ? "#B0BEC5" : "#90CAF9")
                    border.width: statusAbnormal ? 2 : 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 2
                        spacing: 1
                        // 编号（cardShowNumber）
                        Text {
                            text: root.cardShowNumber && cardData.idVal !== "" ? "R" + cardData.idVal : ""
                            visible: root.cardShowNumber && cardData.idVal !== ""
                            font.pixelSize: 9; font.bold: true
                            color: statusAbnormal ? "#D32F2F" : "#333333"
                        }
                        // 状态（cardShowStatus；状态色点）
                        Row {
                            visible: root.cardShowStatus
                            spacing: 3
                            Rectangle {
                                width: 7; height: 7; radius: 4
                                color: card.statusColor
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: card.statusText
                                font.pixelSize: 8
                                color: statusAbnormal ? "#D32F2F" : "#555555"
                            }
                        }
                        // 位置（cardShowLocation；审查 MINOR: 异常时位置文字标红定位问题源）
                        Text {
                            text: root.cardShowLocation && cardData.locVal !== "" ? "" + cardData.locVal : ""
                            visible: root.cardShowLocation && cardData.locVal !== ""
                            font.pixelSize: 8
                            color: statusAbnormal ? "#D32F2F" : "#777777"
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            // 详情弹窗 + onSelect + SelectedTag 写入（DESIGN-WINDOWS L105）
                            robotDetail.cardData = card.cardData
                            robotDetail.openDialog()
                            root.hmiSelect()
                            if (root.selectedTag !== "" && dataManager)
                                dataManager.setValue(root.selectedTag, "" + cardData.idVal)
                        }
                    }
                }
            }
        }

        // ══════════ 机器人详情弹窗（G-1c）══════════
        Rectangle {
            id: robotDetail
            anchors.fill: parent
            z: 50
            color: "#80000000"
            visible: false

            property var cardData: null

            function openDialog() { visible = true }
            function closeDialog() { visible = false }

            // 详情 JSON 解析（修复: function 放顶层, Column 内声明序引用会 ReferenceError）
            function detailText(detailVar) {
                if (!detailVar || detailVar === "") return "无详细信息"
                var raw = dataManager ? dataManager.value(detailVar) : ""
                if (raw === "" || raw === null || raw === undefined) return "无详细信息"
                var s = "" + raw
                try {
                    var obj = JSON.parse(s)
                    s = ""
                    if (obj.category) s += "类别: " + obj.category + "\n"
                    if (obj.model) s += "型号: " + obj.model + "\n"
                    if (obj.production_date) s += "生产日期: " + obj.production_date + "\n"
                    if (obj.factory_date) s += "出厂日期: " + obj.factory_date + "\n"
                    if (obj.task) s += "当前任务: " + obj.task + "\n"
                    if (obj.params) s += "参数: " + JSON.stringify(obj.params) + "\n"
                    if (s === "") s = raw
                } catch (e) {
                    s = raw   // 非 JSON 原文显示
                }
                return s
            }

            Rectangle {
                width: Math.min(parent.width - 8, 240)
                // 审查 M4: 高度钳制到窗口内（防 clip 裁掉底部操作按钮）
                height: Math.min(detailColumn.implicitHeight + 16, parent.height - 6)
                anchors.centerIn: parent
                radius: 6
                color: "white"
                Column {
                    id: detailColumn
                    width: parent.width
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    spacing: 4
                    // 标题行：标题 + 右上角 × 关闭（紧凑布局, 防按钮被窗口高度裁切）
                    Row {
                        width: parent.width
                        spacing: 4
                        Text {
                            width: parent.width - 24
                            anchors.verticalCenter: parent.verticalCenter
                            text: robotDetail.cardData && robotDetail.cardData.idVal !== ""
                                  ? "机器人 R" + robotDetail.cardData.idVal : "机器人详情"
                            font.pixelSize: 12; font.bold: true; color: "#333333"
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            width: 18; height: 18; radius: 3; color: "#E0E0E0"
                            Text { anchors.centerIn: parent; text: "✕"; font.pixelSize: 10; color: "#555555" }
                            MouseArea { anchors.fill: parent; onClicked: robotDetail.closeDialog() }
                        }
                    }
                    // 状态/位置（取自身绑定变量）
                    Text {
                        width: parent.width
                        text: robotDetail.cardData
                              ? "状态: " + robotDetail.cardData.statusVal + "  位置: " + robotDetail.cardData.locVal
                              : ""
                        font.pixelSize: 9; color: "#666666"
                        wrapMode: Text.WordWrap
                    }
                    // 详细信息 JSON 解析（型号/日期/参数/task）
                    Text {
                        width: parent.width
                        text: robotDetail.cardData ? robotDetail.detailText(robotDetail.cardData.detailVar) : ""
                        font.pixelSize: 9; color: "#444444"
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                    // 操作按钮（写 OperTag 变量 "下线:R01" 等——控制器执行, HMI 只下发）
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6
                        Rectangle {
                            width: 48; height: 20; radius: 3; color: "#E53935"
                            Text { anchors.centerIn: parent; text: "下线"; color: "white"; font.pixelSize: 9 }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: sendOper("下线")
                            }
                        }
                        Rectangle {
                            width: 48; height: 20; radius: 3; color: "#4CAF50"
                            Text { anchors.centerIn: parent; text: "上线"; color: "white"; font.pixelSize: 9 }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: sendOper("上线")
                            }
                        }
                        Rectangle {
                            width: 48; height: 20; radius: 3; color: "#F57C00"
                            Text { anchors.centerIn: parent; text: "删除"; color: "white"; font.pixelSize: 9 }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: sendOper("删除")
                            }
                        }
                        function sendOper(type) {
                            if (robotDetail.cardData && robotDetail.cardData.operVar !== "" && dataManager)
                                dataManager.setValue(robotDetail.cardData.operVar,
                                    type + ":R" + robotDetail.cardData.idVal)
                        }
                    }
                }
            }
        }
    }

    // ══════════ 登录弹窗（G-1a）══════════
    Rectangle {
        id: loginDialog
        anchors.fill: parent
        z: 50
        color: "#80000000"
        visible: false

        property string userName: ""
        property string password: ""
        property string errText: ""

        function openDialog() {
            userName = ""; password = ""; errText = ""
            // 审查 M3(2026-08-23 G-1a): TextInput text 绑定在用户编辑后销毁, 属性重置不再传导——
            // 必须显式同步输入框 text, 否则取消后重开残留上次输入
            userNameInput.text = ""
            passwordInput.text = ""
            visible = true
            // 延迟聚焦弹键盘（等可见性生效）
            userNameInput.forceActiveFocus()
        }
        function closeDialog() { visible = false; Qt.inputMethod.hide() }

        Rectangle {
            width: Math.min(parent.width - 12, 240)
            height: loginColumn.implicitHeight + 20
            anchors.centerIn: parent
            radius: 6
            color: "white"
            Column {
                id: loginColumn
                width: parent.width
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                spacing: 8
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "用户登录"
                    font.pixelSize: 14; font.bold: true; color: "#333333"
                }
                TextInput {
                    id: userNameInput
                    width: parent.width
                    height: 26
                    color: "#333333"
                    font.pixelSize: 12
                    clip: true
                    text: loginDialog.userName
                    onTextChanged: loginDialog.userName = text
                    Rectangle { anchors.fill: parent; z: -1; color: "#F2F2F2"; radius: 3 }
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        text: "用户名"
                        color: "#AAAAAA"
                        font.pixelSize: 12
                        visible: userNameInput.text === ""
                    }
                }
                TextInput {
                    id: passwordInput
                    width: parent.width
                    height: 26
                    color: "#333333"
                    font.pixelSize: 12
                    clip: true
                    echoMode: TextInput.Password
                    text: loginDialog.password
                    onTextChanged: loginDialog.password = text
                    Rectangle { anchors.fill: parent; z: -1; color: "#F2F2F2"; radius: 3 }
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        text: "密码"
                        color: "#AAAAAA"
                        font.pixelSize: 12
                        visible: passwordInput.text === ""
                    }
                }
                Text {
                    width: parent.width
                    text: loginDialog.errText
                    color: "#D32F2F"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    visible: loginDialog.errText !== ""
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12
                    Rectangle {
                        width: 70; height: 26; radius: 4; color: "#1565C0"
                        Text { anchors.centerIn: parent; text: "确定"; color: "white"; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var err = userSystem.login(loginDialog.userName, loginDialog.password)
                                if (err === "") {
                                    loginDialog.closeDialog()
                                } else {
                                    loginDialog.errText = err
                                }
                            }
                        }
                    }
                    Rectangle {
                        width: 70; height: 26; radius: 4; color: "#BBBBBB"
                        Text { anchors.centerIn: parent; text: "取消"; color: "white"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: loginDialog.closeDialog() }
                    }
                }
            }
        }
    }

    // ══════════ 用户管理弹窗（G-1a：改用户名/密码/组）══════════
    Rectangle {
        id: manageDialog
        anchors.fill: parent
        z: 50
        color: "#80000000"
        visible: false

        property string userName: ""
        property string newName: ""
        property string newPassword: ""
        property string newGroup: ""
        property string errText: ""

        function openDialog() {
            // 审查 M1(2026-08-23 G-1a): 组预填 = 所选用户当前组（对齐 PC Prefill）——
            // 原实现恒填第一个组（预置=管理员）会静默改组/提权
            newName = ""; newPassword = ""; errText = ""
            newGroup = userSystem ? userSystem.groupOf(userName) : ""
            if (newGroup === "") newGroup = userSystem && userSystem.groupNames().length > 0 ? userSystem.groupNames()[0] : ""
            // 审查 M3: 显式同步输入框 text（绑定销毁后属性不再传导）
            mgrNameInput.text = ""
            mgrPasswordInput.text = ""
            visible = true
        }
        function closeDialog() { visible = false; Qt.inputMethod.hide() }
        function refreshUser() {
            // 切换用户时同步组预填（对齐当前用户所属组）+ 清输入
            newName = ""; newPassword = ""; errText = ""
            newGroup = userSystem ? userSystem.groupOf(userName) : ""
            if (newGroup === "") newGroup = userSystem && userSystem.groupNames().length > 0 ? userSystem.groupNames()[0] : ""
            mgrNameInput.text = ""
            mgrPasswordInput.text = ""
        }

        Rectangle {
            width: Math.min(parent.width - 8, 260)
            height: manageColumn.implicitHeight + 20
            anchors.centerIn: parent
            radius: 6
            color: "white"
            Column {
                id: manageColumn
                width: parent.width
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                spacing: 7
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "用户管理"
                    font.pixelSize: 14; font.bold: true; color: "#333333"
                }

                // 选择用户（左右切换）
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    Rectangle {
                        width: 22; height: 22; radius: 3; color: "#E0E0E0"
                        Text { anchors.centerIn: parent; text: "◀"; font.pixelSize: 12; color: "#333333" }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var names = userSystem.userNames()
                                if (names.length === 0) return
                                var idx = names.indexOf(manageDialog.userName)
                                idx = (idx - 1 + names.length) % names.length
                                manageDialog.userName = names[idx]
                                manageDialog.refreshUser()
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: manageDialog.userName
                        font.pixelSize: 13; font.bold: true; color: "#1565C0"
                    }
                    Rectangle {
                        width: 22; height: 22; radius: 3; color: "#E0E0E0"
                        Text { anchors.centerIn: parent; text: "▶"; font.pixelSize: 12; color: "#333333" }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var names = userSystem.userNames()
                                if (names.length === 0) return
                                var idx = names.indexOf(manageDialog.userName)
                                idx = (idx + 1) % names.length
                                manageDialog.userName = names[idx]
                                manageDialog.refreshUser()
                            }
                        }
                    }
                }

                // 新用户名
                TextInput {
                    id: mgrNameInput
                    width: parent.width
                    height: 24
                    color: "#333333"
                    font.pixelSize: 12
                    clip: true
                    text: manageDialog.newName
                    onTextChanged: manageDialog.newName = text
                    Rectangle { anchors.fill: parent; z: -1; color: "#F2F2F2"; radius: 3 }
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        text: "新用户名（留空=不改）"
                        color: "#AAAAAA"
                        font.pixelSize: 11
                        visible: parent.text === ""
                    }
                }
                // 新密码
                TextInput {
                    id: mgrPasswordInput
                    width: parent.width
                    height: 24
                    color: "#333333"
                    font.pixelSize: 12
                    clip: true
                    echoMode: TextInput.Password
                    text: manageDialog.newPassword
                    onTextChanged: manageDialog.newPassword = text
                    Rectangle { anchors.fill: parent; z: -1; color: "#F2F2F2"; radius: 3 }
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        text: "新密码（留空=不改）"
                        color: "#AAAAAA"
                        font.pixelSize: 11
                        visible: parent.text === ""
                    }
                }
                // 所属组
                Row {
                    spacing: 6
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "组:"
                        font.pixelSize: 11; color: "#666666"
                    }
                    ComboBox {
                        id: groupCombo
                        width: 120; height: 24
                        model: userSystem ? userSystem.groupNames() : []
                        currentIndex: Math.max(0, model.indexOf(manageDialog.newGroup))
                        // 审查 M1: 用户激活选择后 currentIndex 绑定销毁——onActivated 同步属性即可
                        onActivated: manageDialog.newGroup = currentText
                    }
                }

                Text {
                    width: parent.width
                    text: manageDialog.errText
                    color: "#D32F2F"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    visible: manageDialog.errText !== ""
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12
                    Rectangle {
                        width: 70; height: 26; radius: 4; color: "#1565C0"
                        Text { anchors.centerIn: parent; text: "保存"; color: "white"; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                // 审查 M1: 原样传 newGroup（空=不改, 对齐 C++「留空=不改」契约）
                                var err = userSystem.updateUser(manageDialog.userName,
                                    manageDialog.newName, manageDialog.newPassword,
                                    manageDialog.newGroup)
                                if (err === "") {
                                    manageDialog.closeDialog()
                                } else {
                                    manageDialog.errText = err
                                }
                            }
                        }
                    }
                    Rectangle {
                        width: 70; height: 26; radius: 4; color: "#BBBBBB"
                        Text { anchors.centerIn: parent; text: "取消"; color: "white"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: manageDialog.closeDialog() }
                    }
                }
            }
        }
    }
}
