// B-4: HmiImage——图片控件组件
// D-B2 (2026-08-30): 实现 listRef 消费——绑图片列表(listRef)时按 boundTag 值/defaultIndex 索引
//                    显示列表第 N 项图片(对齐 HmiTextList 先例: items 数组 + setIndex 钳制 +
//                    Connections onValueChanged 联动); 未绑列表保持 imagePath 静态路径。
import QtQuick 2.15

Image {
    id: root
    width: 100
    height: 60
    source: root.currentImagePath
    fillMode: stretchMode === "Fill" ? Image.Stretch
            : stretchMode === "Uniform" ? Image.PreserveAspectFit
            : stretchMode === "UniformToFill" ? Image.PreserveAspectCrop
            : Image.PreserveAspectFit

    property string objectName: ""
    property string textDecoration: "None"
    property string boundTag: ""
    property string imagePath: ""
    property string stretchMode: "Uniform"
    property string listRef: ""
    property int defaultIndex: 0

    // D-B2: 列表数据——listRef 非空时按工程图片列表项展开（qmlgenerator 输出为 | 分隔路径串）
    property string listItems: ""          // 生成器展开: "path1|path2|..."（listRef 非空时）
    property var items: root.listItems.length > 0 ? root.listItems.split("|") : []

    // D-B2: 当前索引——默认 defaultIndex，绑变量时随变量值（钳制防越界，对齐 HmiTextList）
    property int currentIndex: 0

    // D-B2: 当前实际显示的图片路径——绑列表 → items[索引]；否则 imagePath（静态）
    property string currentImagePath: {
        if (root.items.length > 0) {
            var i = Math.max(0, Math.min(root.currentIndex, root.items.length - 1))
            return root.items[i]
        }
        return root.imagePath
    }

    // D-B2: 索引钳制入口（变量回写/初始化共用，对齐 HmiTextList.setIndex）
    function setIndex(n) {
        if (root.items.length === 0) return
        root.currentIndex = Math.max(0, Math.min(n, root.items.length - 1))
    }

    // D-B2: 初始化——defaultIndex 生效 + 绑变量读初值
    Component.onCompleted: {
        if (root.items.length > 0)
            root.setIndex(root.defaultIndex)
        if (root.boundTag !== "" && dataManager && dataManager.hasTag(root.boundTag)) {
            var v = dataManager.value(root.boundTag)
            if (v !== undefined && v !== null) {
                var n = parseInt(String(v))
                if (!isNaN(n)) root.setIndex(n)
            }
        }
    }
    // D-B2: 绑变量值变化 → 联动切换图片（对齐 HmiTextList Connections）
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (root.boundTag !== "" && tagName === root.boundTag) {
                var n = parseInt(String(value))
                if (!isNaN(n)) root.setIndex(n)
            }
        }
    }

    // 通用字段（生成器并集输出）
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
}
