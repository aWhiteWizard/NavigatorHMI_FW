// B-4: HmiImage——图片控件组件
import QtQuick 2.15

Image {
    id: root
    width: 100
    height: 60
    source: imagePath !== "" ? imagePath : ""
    fillMode: stretchMode === "Fill" ? Image.Stretch
            : stretchMode === "Uniform" ? Image.PreserveAspectFit
            : stretchMode === "UniformToFill" ? Image.PreserveAspectCrop
            : Image.PreserveAspectFit

    property string objectName: ""
    property string boundTag: ""
    property string imagePath: ""
    property string stretchMode: "Uniform"
    property string listRef: ""
    property int defaultIndex: 0
    // 通用字段（生成器并集输出）
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string title: ""
}
