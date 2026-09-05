// P-6 (2026-09-04): HmiFrameVideo——Frame 视频模式内部实现（QMediaPlayer 后端 + VideoOutput）。
// 独立文件单独 import QtMultimedia：普通 Frame（非视频）不依赖媒体模块——模块缺失只影响视频 Frame，
// 不拖垮非视频画面（HmiFrame 用 Loader 动态挂载本组件）。
// 视频源语义（qmlgenerator resolveVideoPath 已重定位）：本地文件 → 设备端绝对路径（<工程目录>/media/<rel>，
// 需补 file:// 前缀——裸 /mnt/... 被当 qrc 资源加载失败，同 HmiImage.toFileUrl 先例）；
// RTSP/http(s) 网络流 → 原样 URL 直连（RTSP 不入部署包，设备端直连流地址）。
// R-1/R-3/R-4 (2026-09-05 用户 Check)：循环播放（EndOfMedia 重播——Qt6.4 ffmpeg 后端不实现 loops 的兜底）+
// 点击切换播放/暂停（暂停显示半透明圆+暂停符号）+ 播放控制布尔变量（true=播放 false=暂停，点击翻转写回）。
import QtQuick 2.15
import QtMultimedia

Item {
    id: vroot

    property string videoSource: ""
    property string playTag: ""   // R-4: 播放控制布尔变量（空=未绑定——仅点击直接控制；非空=变量驱动 true=播放 false=暂停）

    // 本地绝对路径 → file:// URL；空/已带 file:///qrc:/rtsp://http(s):// 前缀原样返回（与 HmiImage.toFileUrl 先例对齐）
    function toFileUrl(p) {
        if (p === undefined || p === null || p === "") return p
        if (p.indexOf("file://") === 0 || p.indexOf("qrc:/") === 0 || p.indexOf("rtsp://") === 0
                || p.indexOf("http://") === 0 || p.indexOf("https://") === 0)
            return p
        if (p.charAt(0) === "/") return "file://" + p
        return p
    }

    // R-4: 布尔值 → 播放/暂停（"1"/"true" → 播；"0"/""/"false" → 暂停；playTag 空=未绑定不响应）
    function applyPlayTagValue(v) {
        if (vroot.playTag === "") return
        var s = (v === undefined || v === null) ? "" : ("" + v).trim().toLowerCase()
        if (s === "1" || s === "true") {
            if (player.playbackState !== MediaPlayer.PlayingState) player.play()
        } else {
            if (player.playbackState === MediaPlayer.PlayingState) player.pause()
        }
    }
    // R-3/R-4: 点击切换播放/暂停 + 写回控制变量（变量驱动与点击两入口一致）
    function togglePlay() {
        if (player.playbackState === MediaPlayer.PlayingState) player.pause()
        else player.play()
        if (vroot.playTag !== "" && dataManager)
            dataManager.setValue(vroot.playTag, player.playbackState === MediaPlayer.PlayingState)   // 审查 🔵：写 JS bool（非 "1"/"0" 字符串——防 Bool 变量 bool/string 类型抖动，对齐 HmiSwitch/HmiCheckBox 先例）
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
        fillMode: VideoOutput.PreserveAspectFit   // R-1: 等比完整显示（长边 fit 留黑边——用户确认）
    }

    MediaPlayer {
        id: player
        videoOutput: videoOut   // Qt 6 关联方式：MediaPlayer.videoOutput 挂 VideoOutput（6.4 VideoOutput **无 source 属性**——Q_PROPERTY 仅 fillMode/orientation/sourceRect/contentRect/videoSink；早期版本写 VideoOutput.source: player 致「Cannot assign to non-existent property source」→ 组件创建失败第二层根因。官方示例 declarative-camera/VideoPreview 均此写法）
        source: vroot.toFileUrl(vroot.videoSource)
        // 注意：Qt 6.4 QML MediaPlayer **无 autoPlay 属性**（qtmultimedia-6.4.3 qmediaplayer.h 仅 loops 等；
        // autoPlay 是 Qt 6.5+ 才给 MediaPlayer 引入，6.4 仅 spatialaudio 类型有）——早期版本写过
        // `autoPlay: vroot.videoSource !== ""` 致「Cannot assign to non-existent property autoPlay」
        // → 组件创建失败 → HmiFrame 误报「QtMultimedia 未部署」。播放由下方 onSourceChanged 显式 play() 驱动。
        loops: MediaPlayer.Infinite
        // R-1（2026-09-05）：Qt6.4 ffmpeg 后端**不实现 loops**（qffmpegmediaplayer.cpp 无 setLoops 覆盖，
        // endOfStream 直接 Stopped）→ 显式 EndOfMedia 重播兜底（play() 内部 EndOfMedia+Stopped 态自动 seek 0）
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia && playbackState !== MediaPlayer.PlayingState)
                play()
        }
        // 审查 🟡（2026-09-04，复审驳回后修正）：显式 play() 幂等兜底——不依赖「source/autoPlay 同一变更内
        // 绑定求值顺序」（source 先于 autoPlay 求值时 Qt 可能不自动播放）。处理器挂 MediaPlayer 自身
        // onSourceChanged（source 绑定重算后确定性触发）；**不可**挂 onVideoSourceChanged——
        // MediaPlayer 无 videoSource 属性：QML 按未知属性赋值告警且永不触发（首轮复审死代码根因）
        onSourceChanged: {
            if (vroot.videoSource !== "" && playbackState !== MediaPlayer.PlayingState)
                play()
            // 审查 🟡（2026-09-05）：onCompleted 早于 Loader.onLoaded 赋 playTag → 初始同步失效——
            // source 就绪（onLoaded 赋值后触发本 handler）按变量当前值纠正（绑定 false 的画面打开即暂停）
            if (vroot.playTag !== "" && dataManager)
                vroot.applyPlayTagValue(dataManager.value(vroot.playTag))
        }
        onErrorOccurred: function (error, errorString) {
            stateText.text = "视频错误: " + errorString
            stateText.visible = true
            console.warn("[HmiFrameVideo] source=" + vroot.videoSource + " err=" + errorString)
        }
    }

    // R-4: 播放控制变量监听（值变驱动播放/暂停）
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (tagName === vroot.playTag) vroot.applyPlayTagValue(value)
        }
    }
    // R-4: 组件装载后按变量当前值同步一次（未绑定/变量无值 → 维持播放默认）
    Component.onCompleted: {
        if (vroot.playTag !== "" && dataManager)
            vroot.applyPlayTagValue(dataManager.value(vroot.playTag))
    }

    // R-3: 点击控件切换播放/暂停（覆盖全控件区）
    MouseArea {
        anchors.fill: parent
        onClicked: vroot.togglePlay()
    }

    // R-3: 暂停指示——半透明圆 + 白色暂停符号（两竖条）；仅暂停态显示（播放/循环中隐藏）
    Rectangle {
        id: pauseBadge
        anchors.centerIn: parent
        width: 84
        height: 84
        radius: width / 2
        color: "#66000000"   // 半透明黑圆
        visible: player.playbackState === MediaPlayer.PausedState
        Rectangle {   // 左竖条
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 34
            radius: 2
            color: "white"
        }
        Rectangle {   // 右竖条
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 34
            radius: 2
            color: "white"
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
        z: 10   // 错误文本在暂停指示之上（同显不遮挡）
    }
}
