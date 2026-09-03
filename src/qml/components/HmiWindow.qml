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
    property int displayMode: 0   // P-5：AlarmView 显示模式（0=当前报警 1=报警缓冲区；DisplayMode 权威）
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
    // H-7(M8): 事件 payload——确认报警时写入报警编号 / 点卡片时写入机器人编号
    // （生成器 onHmiAck/onHmiSelect 处理器读取此属性传给 runtimeBus.emitEvent）
    property string ackPayload: ""
    property string selectPayload: ""
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

            // 操作按钮（未登录→[登录]；登录态→[注销][改密]——H-3 自助改密入口, 仅登录用户可见）
            Row {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6
                Rectangle {
                    id: loginBtn
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
                // H-3: 自助改密（普通用户/管理员均可改自己的用户名+密码; 组下拉仅管理员且非默认 admin 显示）
                Rectangle {
                    width: 46; height: 22
                    radius: 4
                    color: "#1565C0"
                    visible: userSystem && userSystem.loggedIn
                    Text {
                        anchors.centerIn: parent
                        text: "改密"
                        color: "white"
                        font.pixelSize: 10
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            selfEditDialog.userName = userSystem.currentUserName()
                            selfEditDialog.openDialog()
                        }
                    }
                }
            }
        }

        // ══════════ AlarmView (1): 活动报警列表（G-1b 重写——变量阈值驱动模拟源）
        // P-5 双模式（2026-09-02, DisplayMode 权威）：displayMode=0 当前报警（下方 header/list/ackBar）；
        // displayMode=1 报警缓冲区（历史模式容器——queryAlarmHistory 滚动 + 清除，未确认活动不清除）
        Item {
            visible: root.windowType === 1 && root.displayMode === 0   // P-5：当前报警模式
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
                                root.ackPayload = alarmData.id   // H-7(M8): 确认报警携带编号
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
                            root.ackPayload = ""   // 审查 MINOR-1: 批量确认不携带单行编号
                            root.hmiAck()
                        }
                    }
                }
            }
        }

        // P-5 报警缓冲区模式（displayMode=1）：queryAlarmHistory 全部历史滚动 + 清除按钮（未确认活动不清除）
        Item {
            id: alarmHistRoot   // P-5 复审 🟡：显式 id 供后代 handler 调用 refreshHistory（仓库 robotListRoot.refreshCards 先例）
            visible: root.windowType === 1 && root.displayMode === 1
            anchors.fill: parent
            clip: true

            // 表头（时间/级别/内容 + 清除按钮）
            Rectangle {
                id: histHeader
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 20
                color: "#ECEFF1"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "报警历史（缓冲区）"
                    font.pixelSize: 10
                    color: "#666666"
                    font.bold: true
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 44; height: 15
                    radius: 2
                    color: "#C62828"
                    // P-5 审查 🟡-2：GUI 清除门控与 CLI root 对等（fail-closed——userSystem && canManage，L138 先例逐字一致）
                    visible: userSystem && userSystem.canManage
                    Text {
                        anchors.centerIn: parent
                        text: "清除"
                        color: "white"
                        font.pixelSize: 8
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            // 审查 🟡：clearAlarmHistory 返回 bool——失败不置空 model（防假成功）；不发 hmiAck（
                            // OnAck 语义=确认报警，清除是本地 DB 操作，复用会误触发已配置确认类动作且带残留 payload）
                            if (dataLogger && dataLogger.clearAlarmHistory())
                                alarmHistRoot.refreshHistory()
                        }
                    }
                }
            }

            // 历史列表刷新（审查 🟡-1 修正：子项 onVisibleChanged 不随父容器显隐触发——接受「进入即查 + 清除后重查」
            // 语义，由 Component.onCompleted 与清除按钮调用；画面每次进入经 Loader 重建 → onCompleted 即刷新）
            function refreshHistory() {
                if (dataLogger) histList.model = dataLogger.queryAlarmHistory(200)
            }

            // 历史滚动列表
            ListView {
                id: histList
                anchors.top: histHeader.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true
                delegate: Rectangle {
                    width: histList.width
                    height: 17
                    color: index % 2 === 0 ? "#FFFFFF" : "#F5F5F5"
                    border.color: "#DDDDDD"
                    border.width: 1
                    property var histData: modelData
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 3
                        anchors.verticalCenter: parent.verticalCenter
                        text: histData.ts
                        font.pixelSize: 9
                        color: "#444444"
                        width: 96
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 104
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 10
                        radius: 2
                        color: Number(histData.level) === 0 ? "#D32F2F"
                             : Number(histData.level) === 1 ? "#F57C00"
                             : Number(histData.level) === 2 ? "#F9A825"
                             : "#1976D2"
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 128
                        anchors.verticalCenter: parent.verticalCenter
                        text: histData.message
                        font.pixelSize: 9
                        color: "#333333"
                        elide: Text.ElideRight
                        width: parent.width - 140
                    }
                }
                // 空态（审查 🔵 补）
                Text {
                    anchors.centerIn: parent
                    text: "暂无报警历史"
                    font.pixelSize: 11
                    color: "#999999"
                    visible: histList.model === undefined || histList.model.length === 0
                }
                // 构造加载一次（db 此时已由 DataLogger.setProject 打开）
                Component.onCompleted: alarmHistRoot.refreshHistory()
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
                    // H-6(M6): 详情 JSON 解析 category → 图标分类（解析失败默认图标兜底）
                    var category = ""
                    if (s.detail !== "") {
                        var raw = dataManager ? dataManager.value(s.detail) : ""
                        if (raw !== "" && raw !== null && raw !== undefined) {
                            try {
                                var obj = JSON.parse("" + raw)
                                if (obj && obj.category) category = "" + obj.category
                            } catch (e) { category = "" }   // JSON 失败 → 默认图标
                        }
                    }
                    arr.push({
                        idVar: s.id || "", statusVar: s.status || "", locVar: s.location || "",
                        detailVar: s.detail || "", operVar: s.oper || "",
                        idVal: idVal, statusVal: statusVal, locVal: locVal,
                        category: category
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
                    // 审查 MINOR-10: detail 变化也刷新（category → 图标随数据更新）
                    for (var i = 0; i < root.robotSlots.length; i++) {
                        var s = root.robotSlots[i]
                        if (s.id === tagName || s.status === tagName || s.location === tagName || s.detail === tagName) {
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
                            if (!isNaN(n) && n === -1) { statusText = "已删除"; statusColor = "#9E9E9E" }   // J-1: 删除模拟值（唯一特殊值，其他负值走异常红框）
                            else if (!isNaN(n) && n === 0) { statusText = "空闲"; statusColor = "#90A4AE" }
                            else if (!isNaN(n) && n === 1) { statusText = "运行"; statusColor = "#4CAF50" }
                            else if (!isNaN(n) && n === 2) { statusText = "故障"; statusColor = "#D32F2F" }
                            else { statusText = "异常:" + v; statusColor = "#D32F2F" }
                        }
                        // 审查 M5(DESIGN L103): 异常标注三源——状态越界/非数值/位置无值（离线态=状态无值不标红, 置灰即常态）
                        var stOk = (v !== "" && v !== null && v !== undefined)
                        var stNum = stOk ? Number(v) : NaN
                        // J-1 审查修复: -1 已删除为终态灰——短路全部异常标注源（含位置无值维度，防「已删除」+ 空位置叠红框）
                        var isDeleted = (!isNaN(stNum) && stNum === -1)
                        statusAbnormal = stOk && !isDeleted
                                         && (isNaN(stNum) || stNum > 2 || stNum < 0)
                                         || (stOk && (cardData.locVal === "" || cardData.locVal === null || cardData.locVal === undefined))
                    }
                    Component.onCompleted: computeStatus()
                    // H-6: cardData 变化（delegate 创建后赋值）时重算状态 + 重绘图标（Canvas onPaint 只初始执行）
                    onCardDataChanged: {
                        computeStatus()
                        if (catIcon) catIcon.requestPaint()
                    }

                    color: cardData.statusVal === "" || cardData.statusVal === null || cardData.statusVal === undefined
                           ? "#ECEFF1" : "#E3F2FD"
                    border.color: statusAbnormal ? "#D32F2F" : (cardData.statusVal === "" ? "#B0BEC5" : "#90CAF9")
                    border.width: statusAbnormal ? 2 : 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 2
                        spacing: 1
                        // H-6(M6): category → 图标（Canvas 简化绘制: 无人机/无人船/潜航器/四足/轮式/人形 + 默认）
                        Canvas {
                            id: catIcon
                            width: 20; height: 14
                            visible: cardData.idVal !== "" && cardData.idVal !== null && cardData.idVal !== undefined
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.clearRect(0, 0, width, height)
                                ctx.fillStyle = "#1565C0"
                                var cat = (cardData && cardData.category) ? cardData.category : ""
                                if (cat === "drone" || cat === "uav") {
                                    // 无人机: 四旋翼 × 形 + 中心机身
                                    ctx.strokeStyle = "#1565C0"; ctx.lineWidth = 1.5
                                    ctx.beginPath()
                                    ctx.moveTo(3, 2); ctx.lineTo(17, 12)
                                    ctx.moveTo(17, 2); ctx.lineTo(3, 12)
                                    ctx.stroke()
                                    ctx.beginPath(); ctx.arc(10, 7, 2.2, 0, Math.PI * 2); ctx.fill()
                                } else if (cat === "ship" || cat === "boat") {
                                    // 无人船: 船体弧 + 桅杆
                                    ctx.beginPath()
                                    ctx.moveTo(2, 11); ctx.quadraticCurveTo(10, 14, 18, 11)
                                    ctx.lineTo(18, 12.5); ctx.quadraticCurveTo(10, 15.5, 2, 12.5)
                                    ctx.closePath(); ctx.fill()
                                    ctx.fillRect(9.2, 3, 1.6, 7)
                                } else if (cat === "uuv" || cat === "submarine" || cat === "auv") {
                                    // 潜航器: 椭圆鱼形 + 尾鳍
                                    ctx.beginPath(); ctx.ellipse(8, 7, 6, 3.2, 0, 0, Math.PI * 2); ctx.fill()
                                    ctx.beginPath()
                                    ctx.moveTo(14, 7); ctx.lineTo(18, 4); ctx.lineTo(17.5, 7); ctx.lineTo(18, 10)
                                    ctx.closePath(); ctx.fill()
                                } else if (cat === "quadruped" || cat === "dog" || cat === "robot-dog") {
                                    // 四足: 身 + 四腿
                                    ctx.fillRect(4, 4, 12, 3)
                                    ctx.fillRect(5, 7, 1.5, 5); ctx.fillRect(13.5, 7, 1.5, 5)
                                    ctx.fillRect(7.5, 7, 1.2, 3.5); ctx.fillRect(11.3, 7, 1.2, 3.5)
                                    ctx.beginPath(); ctx.arc(4, 3.4, 1.8, 0, Math.PI * 2); ctx.fill()
                                } else if (cat === "wheel" || cat === "track" || cat === "vehicle") {
                                    // 轮式/履带: 车身 + 两轮
                                    ctx.fillRect(2, 4, 16, 5)
                                    ctx.beginPath(); ctx.arc(5.5, 10.5, 2.2, 0, Math.PI * 2); ctx.fill()
                                    ctx.beginPath(); ctx.arc(14.5, 10.5, 2.2, 0, Math.PI * 2); ctx.fill()
                                } else if (cat === "humanoid" || cat === "human") {
                                    // 人形: 头 + 身 + 腿
                                    ctx.beginPath(); ctx.arc(10, 3, 2, 0, Math.PI * 2); ctx.fill()
                                    ctx.fillRect(7, 6, 6, 3.5)
                                    ctx.fillRect(8, 9.5, 1.5, 3.5); ctx.fillRect(10.5, 9.5, 1.5, 3.5)
                                } else {
                                    // 默认机器人图案: 头 + 方身 + 天线
                                    ctx.beginPath(); ctx.arc(10, 3.5, 2.2, 0, Math.PI * 2); ctx.fill()
                                    ctx.fillRect(6.5, 6, 7, 5)
                                    ctx.fillRect(10, 0.5, 1, 1.8)
                                }
                            }
                        }
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
                            root.selectPayload = "R" + cardData.idVal   // H-7(M8): 选中机器人携带编号
                            root.hmiSelect()
                            if (root.selectedTag !== "" && dataManager)
                                dataManager.setValue(root.selectedTag, "" + cardData.idVal)
                        }
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
        // 审查 MAJOR-5: Window attached property 在 JS handler 内访问抛 TypeError——声明处缓存
        property var contentRoot: root.Window ? root.Window.contentItem : null

        function openDialog() {
            // I-4 审查修复：重开清残留反馈（防上一会话「已下发」短暂闪现）
            operFeedback = ""
            pressedOper = -1
            if (feedbackTimer && feedbackTimer.running) feedbackTimer.stop()
            visible = true
        }
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
        // 操作指令写 OperTag（H-2 修复: function 放顶层——原在 Row 内声明序引用
        // 报 ReferenceError: sendOper is not defined, 下线/上线/删除按钮实际失效）
        // I-4 反馈（Check 续N+5 问题1）：写入成功无 UI 反馈 → 按钮高亮 + 提示「已下发」/「未绑定操作变量」
        property string operFeedback: ""
        property string operFeedbackColor: "#2E7D32"   // 与成功分支一致（I-4 复审残留清理）
        property int pressedOper: -1    // 当前按下的操作类型（0 下线/1 上线/2 删除）
        // J-1: 模拟控制器/外部等待参数（Timer onTriggered 读取；J-1 审查修复：schedule 时存值防触发时 cardData 变化张冠李戴）
        property string simStatusVar: ""
        property var simStatusVal: 0
        property string simDesc: ""
        property string simRobotId: ""
        property string waitStatusVar: ""
        property var waitBefore: null
        property string waitDesc: ""
        property string waitRobotId: ""
        function sendOper(type, idx) {
            // I-4 审查修复：任何点击必有反馈——cardData/dataManager 缺失 → 红「下发失败」
            if (!robotDetail.cardData || !dataManager) {
                robotDetail.operFeedback = "下发失败"
                robotDetail.operFeedbackColor = "#E65100"
            } else if (robotDetail.cardData.operVar === ""
                       || !dataManager.hasTag(robotDetail.cardData.operVar)) {
                // I-4 审查修复：hasTag 预检防假成功（setValue 对不存在变量静默丢弃——组态笔误/标签删除）
                robotDetail.operFeedback = "未绑定操作变量"
                robotDetail.operFeedbackColor = "#E65100"
            } else {
                dataManager.setValue(robotDetail.cardData.operVar,
                    type + ":R" + robotDetail.cardData.idVal)
                robotDetail.operFeedback = "已下发: " + type + ":R" + robotDetail.cardData.idVal
                robotDetail.operFeedbackColor = "#2E7D32"
                // J-1: 内部变量 → 模拟控制器（全链路联动）；外部变量 → 等待真实状态（超时报警）
                robotDetail.simulateOrWait(type)
            }
            robotDetail.pressedOper = idx
            feedbackTimer.restart()
        }
        // J-1: 按 status 变量数据源分流——内部（Tag.source 空）模拟改状态；外部（modbus/mqtt）等真实返回
        function simulateOrWait(type) {
            var statusVar = robotDetail.cardData.statusVar || ""
            var src = (dataManager && statusVar !== "") ? dataManager.tagSource(statusVar) : ""
            var isInternal = (statusVar === "" || src === "" || src === null || src === undefined)
            if (isInternal) {
                if (statusVar === "" || !dataManager.hasTag(statusVar)) return   // 无状态变量不模拟
                robotDetail.simStatusVar = statusVar
                robotDetail.simStatusVal = (type === "下线") ? 0 : (type === "上线") ? 1 : -1
                robotDetail.simDesc = type   // J-1 审查修复：报警文案只含操作名（定稿口径）
                robotDetail.simRobotId = robotDetail.cardData.idVal   // schedule 时存值防触发时错位
                simTimer.restart()
            } else {
                robotDetail.waitStatusVar = statusVar
                robotDetail.waitBefore = dataManager ? dataManager.value(statusVar) : null
                robotDetail.waitDesc = type
                robotDetail.waitRobotId = robotDetail.cardData.idVal
                waitTimer.restart()
            }
        }
        Timer {
            id: simTimer
            interval: 500   // 模拟状态写回延迟（J-1：内部变量场景，模拟控制器 500ms 后置状态）
            repeat: false
            onTriggered: {
                if (robotDetail.simStatusVar === "" || !dataManager
                    || !dataManager.hasTag(robotDetail.simStatusVar)) return
                dataManager.setValue(robotDetail.simStatusVar, robotDetail.simStatusVal)
                // 写后校验：模拟状态未生效 → 手动报警（用 schedule 时存值）
                if (dataManager.value(robotDetail.simStatusVar) !== robotDetail.simStatusVal && alarmEngine) {
                    alarmEngine.raiseManualAlarm(2, "机器人 R" + robotDetail.simRobotId
                        + " 操作「" + robotDetail.simDesc + "」未生效")
                }
            }
        }
        Timer {
            id: waitTimer
            interval: 10000   // 外部等待超时（用户定 5~10s）
            repeat: false
            onTriggered: {
                if (!dataManager || !alarmEngine) return
                var nowVal = dataManager.value(robotDetail.waitStatusVar)
                if (nowVal !== robotDetail.waitBefore) return   // 状态已变（真实控制器响应）→ 正常
                alarmEngine.raiseManualAlarm(2, "机器人 R" + robotDetail.waitRobotId
                    + " 操作「" + robotDetail.waitDesc + "」未生效（外部状态未返回）")
            }
        }
        Timer {
            id: feedbackTimer
            interval: 1500   // 操作反馈提示清除延时（按钮变色恢复 + 提示消失）
            repeat: false
            onTriggered: {
                robotDetail.operFeedback = ""
                robotDetail.pressedOper = -1
            }
        }

        Rectangle {
            // H-2: 最小显示尺寸（宽 240 / 高 max(内容,150)）——内容完整显示;
            // 窗口小于最小尺寸时弹窗可超出窗口（HmiWindow root 无 clip）, 经屏幕边界钳制保证可操作
            // 审查 MINOR-7: 原 max(min(w-8,240),240) 恒 240, 直接写明确值
            width: 240
            height: Math.max(Math.min(detailColumn.implicitHeight + 16, parent.height - 6), 150)
            anchors.centerIn: parent
            radius: 6
            color: "white"
            // H-2: 屏幕边界钳制（弹窗超出窗口时 x/y 限制在屏幕可视区, 防按钮落出屏幕）
            // 审查 MAJOR-5: 用声明处缓存 contentRoot（JS 内访问 root.Window 抛 TypeError）
            onXChanged: clampToScreen()
            onYChanged: clampToScreen()
            function clampToScreen() {
                var win = robotDetail.contentRoot
                if (!win) return
                if (x < 0) x = 0
                if (y < 0) y = 0
                if (x + width > win.width) x = Math.max(0, win.width - width)
                if (y + height > win.height) y = Math.max(0, win.height - height)
            }
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
                        width: 48; height: 20; radius: 3
                        color: robotDetail.pressedOper === 0 ? "#B71C1C" : "#E53935"   // I-4: 按下高亮
                        Text { anchors.centerIn: parent; text: "下线"; color: "white"; font.pixelSize: 9 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: robotDetail.sendOper("下线", 0)
                        }
                    }
                    Rectangle {
                        width: 48; height: 20; radius: 3
                        color: robotDetail.pressedOper === 1 ? "#2E7D32" : "#4CAF50"
                        Text { anchors.centerIn: parent; text: "上线"; color: "white"; font.pixelSize: 9 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: robotDetail.sendOper("上线", 1)
                        }
                    }
                    Rectangle {
                        width: 48; height: 20; radius: 3
                        color: robotDetail.pressedOper === 2 ? "#E65100" : "#F57C00"
                        Text { anchors.centerIn: parent; text: "删除"; color: "white"; font.pixelSize: 9 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: robotDetail.sendOper("删除", 2)
                        }
                    }
                }
                // I-4: 操作反馈提示（成功绿「已下发」/ 失败橙提示；1.5s 后自动消失）
                Text {
                    width: parent.width
                    text: robotDetail.operFeedback
                    color: robotDetail.operFeedbackColor
                    font.pixelSize: 9
                    font.bold: true   // I-4 审查修复：对比度加深（#2E7D32/#E65100 + 加粗）
                    horizontalAlignment: Text.AlignHCenter
                    visible: robotDetail.operFeedback !== ""
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
            updateGroupComboState()
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
            updateGroupComboState()
        }
        // H-3: 默认 admin 改自己时组下拉禁用（防把唯一管理员降权锁死）
        function updateGroupComboState() {
            if (groupCombo)
                groupCombo.enabled = !(userSystem && userSystem.isDefaultAdmin()
                                       && userName === userSystem.currentUserName())
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
                                // 审查 MAJOR-2: 默认 admin 改自己时传空组（禁改组; 否则组预填非空被服务端误拒）
                                var grp = manageDialog.newGroup
                                if (userSystem && userSystem.isDefaultAdmin()
                                    && manageDialog.userName === userSystem.currentUserName())
                                    grp = ""
                                var err = userSystem.updateUser(manageDialog.userName,
                                    manageDialog.newName, manageDialog.newPassword, grp)
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

    // ══════════ 自助改密弹窗（H-3：普通用户/管理员改自己的用户名+密码；组仅管理员且非默认 admin 可改）══════════
    Rectangle {
        id: selfEditDialog
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
            newName = ""; newPassword = ""; errText = ""
            // 组预填 = 当前用户所属组（仅管理员且非默认 admin 显示组区; 普通用户组区隐藏）
            newGroup = userSystem ? userSystem.groupOf(userName) : ""
            selNameInput.text = ""
            selPasswordInput.text = ""
            visible = true
        }
        function closeDialog() { visible = false; Qt.inputMethod.hide() }

        Rectangle {
            width: Math.min(parent.width - 8, 240)
            height: selColumn.implicitHeight + 20
            anchors.centerIn: parent
            radius: 6
            color: "white"
            Column {
                id: selColumn
                width: parent.width
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                spacing: 7
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "修改我的账户"
                    font.pixelSize: 13; font.bold: true; color: "#333333"
                }
                // 当前用户名（锁定, 不可改此处——自助改密只作用于自己）
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: selfEditDialog.userName
                    font.pixelSize: 12; font.bold: true; color: "#1565C0"
                }
                // 新用户名
                TextInput {
                    id: selNameInput
                    width: parent.width
                    height: 24
                    color: "#333333"
                    font.pixelSize: 12
                    clip: true
                    text: selfEditDialog.newName
                    onTextChanged: selfEditDialog.newName = text
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
                    id: selPasswordInput
                    width: parent.width
                    height: 24
                    color: "#333333"
                    font.pixelSize: 12
                    clip: true
                    echoMode: TextInput.Password
                    text: selfEditDialog.newPassword
                    onTextChanged: selfEditDialog.newPassword = text
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
                // 所属组（H-3: 仅管理员且非默认 admin 显示/可改——普通用户禁改组, 默认 admin 禁改自己组）
                Row {
                    spacing: 6
                    visible: userSystem && userSystem.canManage && !userSystem.isDefaultAdmin()
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "组:"
                        font.pixelSize: 11; color: "#666666"
                    }
                    ComboBox {
                        id: selGroupCombo
                        width: 120; height: 24
                        model: userSystem ? userSystem.groupNames() : []
                        currentIndex: Math.max(0, model.indexOf(selfEditDialog.newGroup))
                        onActivated: selfEditDialog.newGroup = currentText
                    }
                }
                Text {
                    width: parent.width
                    text: selfEditDialog.errText
                    color: "#D32F2F"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    visible: selfEditDialog.errText !== ""
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
                                // 组仅管理员且非默认 admin 传值（普通用户组区隐藏 newGroup 空=不改）
                                var grp = (userSystem && userSystem.canManage && !userSystem.isDefaultAdmin())
                                          ? selfEditDialog.newGroup : ""
                                var err = userSystem.updateUser(selfEditDialog.userName,
                                    selfEditDialog.newName, selfEditDialog.newPassword, grp)
                                if (err === "") {
                                    selfEditDialog.closeDialog()
                                } else {
                                    selfEditDialog.errText = err
                                }
                            }
                        }
                    }
                    Rectangle {
                        width: 70; height: 26; radius: 4; color: "#BBBBBB"
                        Text { anchors.centerIn: parent; text: "取消"; color: "white"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: selfEditDialog.closeDialog() }
                    }
                }
            }
        }
    }
}
