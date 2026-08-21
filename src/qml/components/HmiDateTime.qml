// B-4: HmiDateTime——日期时间控件组件
import QtQuick 2.15

Text {
    id: root
    width: 160
    height: 30
    text: dtText !== "" ? dtText : (dtFormat !== "" ? formatNow() : "2026-01-01 00:00:00")
    color: textColor !== "" ? textColor : "#000000"
    font.pixelSize: fontSize > 0 ? fontSize : 14
    font.family: fontFamily !== "" ? fontFamily : "sans-serif"
    horizontalAlignment: hAlign === "Center" ? Text.AlignHCenter
                      : hAlign === "Right" ? Text.AlignRight
                      : Text.AlignLeft
    verticalAlignment: Text.AlignVCenter

    property string objectName: ""
     property string textDecoration: "None"
    property string boundTag: ""
    property string dtText: ""
    property string dtFormat: "yyyy-MM-dd HH:mm:ss"
    property string textColor: ""
    property double fontSize: 14
    property string fontFamily: ""
    property string hAlign: "Center"
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""
     property string content: ""  // 并集字段容忍(生成器统一输出)
     property string fontWeight: "Normal"  // 并集字段容忍(生成器统一输出)
     property string fontStyle: "Normal"  // 并集字段容忍(生成器统一输出)
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

    function formatNow() {
        var d = new Date()
        var pad = function(n) { return n < 10 ? "0" + n : "" + n }
        return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate()) +
               " " + pad(d.getHours()) + ":" + pad(d.getMinutes()) + ":" + pad(d.getSeconds())
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.dtFormat !== ""
        onTriggered: {
            root.text = root.dtText !== "" ? root.dtText : root.formatNow()
            if (vncMirror) vncMirror.markDirty(root.x, root.y, root.width, root.height)
        }
    }
}
