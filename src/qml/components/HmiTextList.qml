// D+: HmiTextList——文本列表控件（点击弹出下拉选择; content 逗号分隔项）
// 下拉自建（QtQuick.Controls Popup 在普通 Window 无 Overlay 时 popupItem parent=null 永不渲染,
// 源码实证: qquickpopup.cpp prepareEnterTransition popupItem->setParentItem(QQuickOverlay::overlay(window)));
// 方案: 下拉面板动态 reparent 到窗口 contentItem(尾部绘制在兄弟之上不被遮挡) + 全屏关闭层
// 审查修复(2b3d04bd): 补全事件信号集/并集属性、setIndex 钳制(不销毁绑定)、上弹钳制、空列表防护
import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: 120
    height: 60
    color: fillColor !== "" ? fillColor : "#FFFFFF"
    border.color: strokeColor !== "" ? strokeColor : "#CCCCCC"
    border.width: strokeThickness > 0 ? strokeThickness : 1

    property string objectName: ""
    property string textDecoration: "None"
    property string boundTag: ""
    property string content: ""
    property int defaultIndex: 0
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string textColor: "#000000"
    property double fontSize: 13
    property string text: ""  // 并集字段容忍(生成器统一输出)
    property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
    property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
    property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
    property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
    property string imagePath: ""  // 并集字段容忍(生成器统一输出)
    property string stretchMode: ""  // 并集字段容忍(生成器统一输出)
    property string listRef: ""  // 并集字段容忍(生成器统一输出)
    property double value: 0  // 并集字段容忍(生成器统一输出)
    property double min: 0  // 并集字段容忍(生成器统一输出)
    property double max: 0  // 并集字段容忍(生成器统一输出)
    property string fillStyle: "Solid"  // 并集字段容忍(生成器统一输出)
    property bool isOn: false  // 并集字段容忍(生成器统一输出)
    property bool isChecked: false  // 并集字段容忍(生成器统一输出)
    property bool isReadOnly: false  // 并集字段容忍(生成器统一输出)
    property string labelOn: ""  // 并集字段容忍(生成器统一输出)
    property string labelOff: ""  // 并集字段容忍(生成器统一输出)
    property double x2: 0  // 并集字段容忍(生成器统一输出)
    property double y2: 0  // 并集字段容忍(生成器统一输出)
    property string title: ""  // 并集字段容忍(生成器统一输出)
    property string dtText: ""  // 并集字段容忍(生成器统一输出)
    property string dtFormat: ""  // 并集字段容忍(生成器统一输出)
    property int windowType: 0  // 并集字段容忍(生成器统一输出)
    property string winTitle: ""  // 并集字段容忍(生成器统一输出)
    property bool showTitleBar: true  // 并集字段容忍(生成器统一输出)
    property bool showHistory: false  // 并集字段容忍(生成器统一输出)
    property string selectedTag: ""  // 并集字段容忍(生成器统一输出)
    property double cardWidth: 0  // 并集字段容忍(生成器统一输出)
    property double cardHeight: 0  // 并集字段容忍(生成器统一输出)
    property bool showUserName: false  // 并集字段容忍(生成器统一输出)
    property bool showRole: false  // 并集字段容忍(生成器统一输出)
    property bool showMode: false  // 并集字段容忍(生成器统一输出)
    property bool cardShowNumber: false  // 并集字段容忍(生成器统一输出)
    property bool cardShowStatus: false  // 并集字段容忍(生成器统一输出)
    property bool cardShowLocation: false  // 并集字段容忍(生成器统一输出)
    property string boundDevice: ""  // 并集字段容忍(生成器统一输出)

    // 事件信号集（生成器 18 种事件映射全声明, 防绑事件的 textlist Cannot assign——契约缺口修复）
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
    signal hmiUserChanged()
    signal hmiAck()
    signal hmiSelect()

    // 数据: 项数组 + 当前选中索引
    property var items: root.content.length > 0 ? root.content.split(",") : []
    property int currentIndex: Math.max(0, Math.min(root.defaultIndex, items.length - 1))
    property string currentText: (currentIndex >= 0 && currentIndex < items.length) ? items[currentIndex] : ""

    // 窗口内容层（声明处 QML 上下文求值——handler 里访问 attached 会 TypeError, 用属性缓存）
    // 初始 parent=root(挂组件下, 随画面销毁), 打开时动态 reparent 到窗口层
    property var contentRoot: root.Window ? root.Window.contentItem : null

    // 审查修复: 统一钳制入口（defaultIndex/items 静态时绑定即初始值; 交互/变量回写走本函数钳制,
    // 避免越界——对静态配置无实害, 未来 content 动态化时再改 onContentChanged 重钳制保留绑定）
    function setIndex(n) {
        if (items.length === 0) return
        currentIndex = Math.max(0, Math.min(n, items.length - 1))
    }

    // 显示当前选中项
    Text {
        id: displayText
        anchors.fill: parent
        anchors.margins: 4
        anchors.rightMargin: 22  // 给下拉箭头留位
        text: root.currentText
        color: root.textColor
        font.pixelSize: root.fontSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    // 下拉箭头（旋转方块三角, 防嵌入式字体缺字形）
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        width: 7; height: 7
        color: "transparent"
        Rectangle {
            width: 6; height: 6
            anchors.centerIn: parent
            color: "#666666"
            rotation: 45
        }
    }
    // 点击弹出下拉
    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.hmiClicked()  // 复审🟡4: 与 HmiButton 等一致, 绑 OnClick 事件的 TextList 可触发
            if (root.isReadOnly) return
            if (root.items.length === 0) {  // 审查修复: 空列表不弹空下拉
                console.warn("[TextList] 列表为空, 不弹出下拉: " + root.objectName)
                return
            }
            root.showDropdown()
        }
    }

    // ── 自建下拉面板（打开时挂窗口 contentItem, 尾部绘制不被兄弟遮挡）──
    Rectangle {
        id: dropdown
        width: root.width
        height: Math.min(root.items.length * 20 + 4, 240)
        visible: false
        z: 10000
        color: "#FFFFFF"
        border.color: "#999999"
        border.width: 1
        clip: true

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 2
            clip: true
            model: root.items
            delegate: Rectangle {
                width: list.width
                height: 20
                color: root.currentIndex === index ? "#E3F2FD" : "transparent"
                Text {
                    anchors.fill: parent
                    anchors.margins: 4
                    text: modelData
                    color: root.textColor
                    font.pixelSize: root.fontSize
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.setIndex(index)
                        root.hideDropdown()
                        // R1: 交互写变量（状态持久化）
                        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag))
                            dataManager.setValue(root.boundTag, index)
                        root.hmiValueChanged()
                    }
                }
            }
        }
    }

    // ── 全屏关闭层（面板打开时挂 contentItem, 点击面板外任意处关闭）──
    MouseArea {
        id: externalDismiss
        anchors.fill: parent  // 初始挂 root(尺寸=控件), 打开时 reparent 全屏
        z: 9999
        visible: false
        onClicked: root.hideDropdown()
    }

    function showDropdown() {
        if (!contentRoot) return
        // 已打开 → 关闭（toggle）
        if (dropdown.visible) { hideDropdown(); return }
        // 动态 reparent 到窗口内容层
        dropdown.parent = contentRoot
        externalDismiss.parent = contentRoot
        externalDismiss.anchors.fill = null
        externalDismiss.x = 0; externalDismiss.y = 0
        externalDismiss.width = contentRoot.width
        externalDismiss.height = contentRoot.height
        var p = root.mapToItem(null, 0, 0)  // 场景坐标 = contentItem 坐标（全屏同原点）
        dropdown.x = p.x
        dropdown.y = p.y + root.height
        // 复审🟡1: 用 contentRoot 实际尺寸钳制（不硬编码 600/1024, 兼容 4 寸 720×720）
        if (dropdown.y + dropdown.height > contentRoot.height)
            dropdown.y = Math.max(0, p.y - dropdown.height)
        dropdown.x = Math.max(0, Math.min(dropdown.x, contentRoot.width - dropdown.width))
        dropdown.visible = true
        externalDismiss.visible = true
    }
    function hideDropdown() {
        dropdown.visible = false
        externalDismiss.visible = false
    }
    // 复审🟡3: 面板已 reparent 到窗口层时, 组件销毁前恢复 parent 防孤儿对象累积
    Component.onDestruction: {
        hideDropdown()
        if (dropdown.parent !== root) dropdown.parent = root
        if (externalDismiss.parent !== root) externalDismiss.parent = root
    }

    // R1: 状态持久化——初始化 + 外部跟随
    Component.onCompleted: {
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) {
                var n = parseInt(String(v))
                if (!isNaN(n)) root.setIndex(n)
            }
        }
    }
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag) {
                var n = parseInt(String(value))
                if (!isNaN(n)) root.setIndex(n)
            }
        }
    }
}
