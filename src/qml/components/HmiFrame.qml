// B-4: HmiFrame——框架控件组件（标题 + 边框）
import QtQuick 2.15

Rectangle {
    id: root
    width: 160
    height: 100
    color: fillColor !== "" ? fillColor : "transparent"
    border.color: strokeColor !== "" ? strokeColor : "#999999"
    border.width: strokeThickness > 0 ? strokeThickness : 1

    property string objectName: ""
    property string textDecoration: "None"
    property string boundTag: ""
    property string title: ""
    property string fillColor: ""
    property string strokeColor: ""
    property double strokeThickness: 0
    property string titleColor: "#000000"
    property double fontSize: 13
    property bool showVideo: false  // P-6: 视频模式（生成器仅视频 Frame 输出 true）——Loader 动态挂 HmiFrameVideo
    property string videoSource: ""  // P-6: 视频源（生成器 resolveVideoPath 重定位：本地绝对路径 / RTSP/网络 URL 原样）
    property string text: ""  // 并集字段容忍(生成器统一输出)
    property string content: ""  // 并集字段容忍(生成器统一输出)
    property string hAlign: "Left"  // 并集字段容忍(生成器统一输出)
    property string fontFamily: ""  // 并集字段容忍(生成器统一输出)
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

    // P-6: 视频模式——Loader 动态加载 HmiFrameVideo.qml（独立 import QtMultimedia：
    // 非视频 Frame 不依赖媒体模块，模块缺失不拖垮普通 Frame）；视频层铺满控件，
    // titleBar 在其上（title 非空时标题悬浮视频顶部）；视频源为空或加载失败由组件内提示
    Loader {
        id: videoLayer
        anchors.fill: parent
        visible: false
        active: root.showVideo && root.videoSource !== ""
        source: active ? "HmiFrameVideo.qml" : ""
        onLoaded: {
            item.videoSource = root.videoSource
            visible = true
        }
        // 审查 🟡（2026-09-04）：模块缺失/import 失败不得静默（Loader 永不 onLoaded、无任何反馈）——
        // Error 状态显式提示 + 日志（对齐「不静默失败」纪律）
        // 2026-09-05 根因实录：曾误报「QtMultimedia 未部署？」——实际是 HmiFrameVideo.qml 给 Qt 6.4
        // MediaPlayer 赋了不存在的 autoPlay 属性（QML 报错行已由 Qt 打印到 stderr/日志）。提示不再猜测原因，
        // 指引看设备日志定位（Loader 失败=组件创建错误，具体行号在日志）。
        onStatusChanged: {
            if (status === Loader.Error) {
                videoErrorText.visible = true
                console.warn("[HmiFrame] 视频组件加载失败 source=" + root.videoSource
                             + "（详见设备日志 QML 报错行——Loader 失败=组件创建错误，非必然模块缺失）")
            }
        }
    }

    // 视频组件加载失败提示（import 缺失/模块不可用；正常情况恒不可见）
    Text {
        id: videoErrorText
        anchors.centerIn: parent
        visible: false
        text: "视频模块不可用"
        color: "#FF5252"
        font.pixelSize: 13
    }

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 22
        color: "#E0E0E0"
        visible: root.title !== ""

        Text {
            anchors.fill: parent
            anchors.margins: 4
            text: root.title
            color: root.titleColor
            font.pixelSize: root.fontSize
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
