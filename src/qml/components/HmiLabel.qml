// B-5: HmiLabel——标签控件组件（绑变量显示实时值, DataManager 驱动）
// 契约: text = 固定文本; boundTag 绑定变量时显示 DataManager 实时值
import QtQuick 2.15

Text {
    id: root
    width: 100
    height: 30
    // 绑定变量 → DataManager 实时值; 否则固定文本
    text: boundTag !== "" && dataManager !== undefined && dataManager !== null
          ? dataManager.value(boundTag) + ""
          : root.text
    color: textColor !== "" ? textColor : "#000000"
    font.pixelSize: fontSize > 0 ? fontSize : 14
    font.family: fontFamily !== "" ? fontFamily : "sans-serif"
    font.bold: fontWeight === "Bold"
    horizontalAlignment: hAlign === "Center" ? Text.AlignHCenter
                      : hAlign === "Right" ? Text.AlignRight
                      : Text.AlignLeft
    verticalAlignment: Text.AlignVCenter

    property string objectName: ""
    property string boundTag: ""
    property string text: "Label"
    property string fontFamily: ""
    property double fontSize: 14
    property string fontWeight: "Normal"
    property string textColor: ""
    property string hAlign: "Left"
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""

    signal hmiValueChanged()

    // DataManager 值变化 → 刷新显示（C++ 注入 context property）
    Connections {
        target: dataManager !== undefined && dataManager !== null ? dataManager : null
        onValueChanged: function(tagName, value) {
            if (tagName === root.boundTag) {
                root.text = value + ""
                root.hmiValueChanged()
            }
        }
    }
}
