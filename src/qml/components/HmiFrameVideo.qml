// P-6 (2026-09-04): HmiFrameVideo——Frame 视频模式内部实现（QMediaPlayer 后端 + VideoOutput）。
// 独立文件单独 import QtMultimedia：普通 Frame（非视频）不依赖媒体模块——模块缺失只影响视频 Frame，
// 不拖垮非视频画面（HmiFrame 用 Loader 动态挂载本组件）。
// 视频源语义（qmlgenerator resolveVideoPath 已重定位）：本地文件 → 设备端绝对路径（<工程目录>/media/<rel>，
// 需补 file:// 前缀——裸 /mnt/... 被当 qrc 资源加载失败，同 HmiImage.toFileUrl 先例）；
// RTSP/http(s) 网络流 → 原样 URL 直连（RTSP 不入部署包，设备端直连流地址）。
import QtQuick 2.15
import QtMultimedia

Item {
    id: vroot

    property string videoSource: ""

    // 本地绝对路径 → file:// URL；空/已带 file:///qrc:/rtsp://http(s):// 前缀原样返回（与 HmiImage.toFileUrl 先例对齐）
    function toFileUrl(p) {
        if (p === undefined || p === null || p === "") return p
        if (p.indexOf("file://") === 0 || p.indexOf("qrc:/") === 0 || p.indexOf("rtsp://") === 0
                || p.indexOf("http://") === 0 || p.indexOf("https://") === 0)
            return p
        if (p.charAt(0) === "/") return "file://" + p
        return p
    }

    // 黑色衬底（视频 letterbox 区域/未就绪时为黑，画面完整）
    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    // VideoOutput 声明在 MediaPlayer **之前**：MediaPlayer.videoOutput 是对象绑定，首次求值时 videoOut
    // 必须已创建（id 后向引用无变化通知——先 MediaPlayer 后 VideoOutput 会恒 null 无画面）
    VideoOutput {
        id: videoOut
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit   // 不变形（黑边补齐）
    }

    MediaPlayer {
        id: player
        videoOutput: videoOut   // Qt 6 关联方式：MediaPlayer.videoOutput 挂 VideoOutput（6.4 VideoOutput **无 source 属性**——Q_PROPERTY 仅 fillMode/orientation/sourceRect/contentRect/videoSink；早期版本写 VideoOutput.source: player 致「Cannot assign to non-existent property source」→ 组件创建失败第二层根因。官方示例 declarative-camera/VideoPreview 均此写法）
        source: vroot.toFileUrl(vroot.videoSource)
        // 注意：Qt 6.4 QML MediaPlayer **无 autoPlay 属性**（qtmultimedia-6.4.3 qmediaplayer.h 仅 loops 等；
        // autoPlay 是 Qt 6.5+ 才给 MediaPlayer 引入，6.4 仅 spatialaudio 类型有）——早期版本写过
        // `autoPlay: vroot.videoSource !== ""` 致「Cannot assign to non-existent property autoPlay」
        // → 组件创建失败 → HmiFrame 误报「QtMultimedia 未部署」。播放由下方 onSourceChanged 显式 play() 驱动。
        // Frame 视频作画面展示：循环播放（播完不黑屏；用户 Check 确认是否需单次播放）
        loops: MediaPlayer.Infinite
        // 审查 🟡（2026-09-04，复审驳回后修正）：显式 play() 幂等兜底——不依赖「source/autoPlay 同一变更内
        // 绑定求值顺序」（source 先于 autoPlay 求值时 Qt 可能不自动播放）。处理器挂 MediaPlayer 自身
        // onSourceChanged（source 绑定重算后确定性触发）；**不可**挂 onVideoSourceChanged——
        // MediaPlayer 无 videoSource 属性：QML 按未知属性赋值告警且永不触发（首轮复审死代码根因）
        onSourceChanged: {
            if (vroot.videoSource !== "" && playbackState !== MediaPlayer.PlayingState)
                play()
        }
        onErrorOccurred: function (error, errorString) {
            stateText.text = "视频错误: " + errorString
            stateText.visible = true
            console.warn("[HmiFrameVideo] source=" + vroot.videoSource + " err=" + errorString)
        }
    }

    Text {
        id: stateText
        anchors.centerIn: parent
        visible: false
        color: "#FF5252"
        font.pixelSize: 14
        width: parent.width - 20
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
}
