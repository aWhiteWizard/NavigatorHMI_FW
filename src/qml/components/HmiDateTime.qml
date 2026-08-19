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
        onTriggered: root.text = root.dtText !== "" ? root.dtText : root.formatNow()
    }
}
