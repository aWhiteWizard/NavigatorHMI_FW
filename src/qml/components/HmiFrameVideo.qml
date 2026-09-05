// P-6 (2026-09-04): HmiFrameVideo——Frame 视频模式内部实现（QMediaPlayer 后端 + VideoOutput）。
// 独立文件单独 import QtMultimedia：普通 Frame（非视频）不依赖媒体模块——模块缺失只影响视频 Frame，
// 不拖垮非视频画面（HmiFrame 用 Loader 动态挂载本组件）。
// 视频源语义（qmlgenerator resolveVideoPath 已重定位）：本地文件 → 设备端绝对路径（<工程目录>/media/<rel>，
// 需补 file:// 前缀——裸 /mnt/... 被当 qrc 资源加载失败，同 HmiImage.toFileUrl 先例）；
// RTSP/http(s) 网络流 → 原样 URL 直连（RTSP 不入部署包，设备端直连流地址）。
// R-1/R-3/R-4 (2026-09-05 用户 Check)：循环播放（EndOfMedia 重播——Qt6.4 ffmpeg 后端不实现 loops 的兜底）+
// 点击切换播放/暂停（暂停显示半透明圆+暂停符号）+ 播放控制布尔变量（true=播放 false=暂停，点击翻转写回）。
// S-5 (2026-09-05 用户拍板)：视频源列表（videoListRef + videoListItems | 分隔 + videoIndexTag 整型索引变量）——
//   运行时按索引变量值取列表对应项切源播放；列表模式优先于单源 videoSource。
// S-6 (2026-09-05)：播放失败诊断——打印当前源路径 + 提示「该路径不可播放」（非静默）。
import QtQuick 2.15
import QtMultimedia

Item {
    id: vroot

    property string videoSource: ""
    property string playTag: ""   // R-4: 播放控制布尔变量（空=未绑定——仅点击直接控制；非空=变量驱动 true=播放 false=暂停）
    property string videoListRef: ""   // S-5: 视频源列表名（空=未选列表走单源 videoSource）
    property string videoIndexTag: ""  // S-5: 视频源选择变量（整型非负——变量值=列表项索引）
    property string videoListItems: "" // S-5: 视频源列表项（生成器按列表展开 | 分隔源地址串，含空占位项）

    // 本地绝对路径 → file:// URL；空/已带 file:///qrc:/rtsp://http(s):// 前缀原样返回（与 HmiImage.toFileUrl 先例对齐）
    function toFileUrl(p) {
        if (p === undefined || p === null || p === "") return p
        if (p.indexOf("file://") === 0 || p.indexOf("qrc:/") === 0 || p.indexOf("rtsp://") === 0
                || p.indexOf("http://") === 0 || p.indexOf("https://") === 0)
            return p
        if (p.charAt(0) === "/") return "file://" + p
        return p
    }

    // S-5: 列表项数组（videoListItems split——保留空占位项使索引与 PC 列表项对齐）
    property var sourceItems: videoListItems.length > 0 ? videoListItems.split("|") : []

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

    // S-5: 当前目标源（列表模式取 items[idx]，idx 越界/空项 → ""（走提示路径）；单源 → videoSource）
    function currentTargetSource() {
        if (vroot.videoListRef !== "" && vroot.sourceItems.length > 0) {
            var idx = 0
            if (vroot.videoIndexTag !== "" && dataManager) {
                var raw = dataManager.value(vroot.videoIndexTag)
                var n = Number(raw)
                idx = (!isNaN(n) && n >= 0) ? Math.floor(n) : 0
            }
            if (idx >= 0 && idx < vroot.sourceItems.length)
                return vroot.sourceItems[idx]
            return ""   // 越界 → 无源（S-6 提示）
        }
        return vroot.videoSource
    }
    // S-5: 应用当前目标源（不同 → 重设 source（onSourceChanged 触发播放）；相同 → 未播则补播）
    // 复审 🔴（2026-09-05 FW 审）：成功路径必须清屏——错误文本（S-6/空源提示）一旦显示永不消除，
    // 正常播放中会常显红色错误覆盖层；此处有效源即清屏（后续切源成功同样清除，错误只留在真正无效/失败时）。
    function applySource() {
        var target = vroot.currentTargetSource()
        if (target === undefined || target === null || target === "") {
            stateText.text = "视频源不可用（当前索引无有效源）"
            stateText.visible = true
            console.warn("[HmiFrameVideo] 无有效视频源 listRef=" + vroot.videoListRef + " videoSource=" + vroot.videoSource)
            return
        }
        var url = vroot.toFileUrl(target)
        if (player.source.toString() !== url) {   // 复审 🟡：player.source 读回为 QUrl 对象——严格 !== 不做类型转换，
                                                  // 同源重入会恒不等 → 幂等分支失效每次都重设 source 重载；toString 化显式比较
            player.source = url
        } else if (player.playbackState !== MediaPlayer.PlayingState
                   && player.mediaStatus !== MediaPlayer.EndOfMedia) {
            player.play()
        }
        stateText.visible = false   // 有效源 → 清除错误覆盖层（含切源成功后的旧 S-6/空源提示）
    }
    // 复审 🔴（2026-09-05 FW 审）：装载同步显式化——原 onVideoSourceChanged/onVideoListItemsChanged/
    // onVideoListRefChanged 隐式级联 + Component.onCompleted 无条件 applySource()：onCompleted 早于
    // HmiFrame.onLoaded 赋属性（此时全默认 ""）必误报一次；onLoaded 逐参赋值途中（listRef 已赋、
    // videoListItems 未赋）的中间态同样误报。现由 HmiFrame.onLoaded 末尾显式调本函数一次（五参齐备后），
    // 无中间态级联；运行时切源仍由下方 Connections(videoIndexTag) → applyIndexValue 驱动。
    function syncSources() {
        vroot.applySource()
    }
    // S-5: 索引变量值变化 → 重取列表项切源（列表模式）
    function applyIndexValue() {
        if (vroot.videoListRef !== "" && vroot.sourceItems.length > 0)
            vroot.applySource()
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
        // 注意：source 不在此绑定——S-5 起由 vroot.applySource() 统一决定（列表/单源），绑定会与运行时切源冲突
        // Qt 6.4 QML MediaPlayer **无 autoPlay 属性**（qtmultimedia-6.4.3 qmediaplayer.h 仅 loops 等；
        // autoPlay 是 Qt 6.5+ 才给 MediaPlayer 引入，6.4 仅 spatialaudio 类型有）——早期版本写过
        // `autoPlay: vroot.videoSource !== ""` 致「Cannot assign to non-existent property autoPlay」
        // → 组件创建失败 → HmiFrame 误报「QtMultimedia 未部署」。播放由 onSourceChanged 显式 play() 驱动。
        loops: MediaPlayer.Infinite
        // R-1（2026-09-05）：Qt6.4 ffmpeg 后端**不实现 loops**（qffmpegmediaplayer.cpp 无 setLoops 覆盖，
        // endOfStream 直接 Stopped）→ 显式 EndOfMedia 重播兜底（play() 内部 EndOfMedia+Stopped 态自动 seek 0）
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia && playbackState !== MediaPlayer.PlayingState)
                play()
        }
        // 审查 🟡（2026-09-04，复审驳回后修正）：显式 play() 幂等兜底——不依赖「source/autoPlay 同一变更内
        // 绑定求值顺序」。处理器挂 MediaPlayer 自身 onSourceChanged（source 重算后确定性触发）
        onSourceChanged: {
            if (playbackState !== MediaPlayer.PlayingState && player.source !== "")
                play()
            // 审查 🟡（2026-09-05）：onCompleted 早于 Loader.onLoaded 赋 playTag → 初始同步失效——
            // source 就绪（onLoaded 赋值后触发本 handler）按变量当前值纠正（绑定 false 的画面打开即暂停）
            if (vroot.playTag !== "" && dataManager)
                vroot.applyPlayTagValue(dataManager.value(vroot.playTag))
        }
        // S-6（2026-09-05）：播放失败诊断——打印当前源路径 + 明确「该路径不可播放」提示（非静默）
        onErrorOccurred: function (error, errorString) {
            var src = vroot.currentTargetSource()
            stateText.text = "该路径不可播放: " + (src === "" ? "(空/越界)" : src)
            stateText.visible = true
            console.warn("[HmiFrameVideo] 该路径不可播放 source=" + src
                         + " err=" + errorString + " errCode=" + error)
        }
    }

    // R-4/S-5: 变量监听（播放控制布尔 + 视频源选择索引）
    Connections {
        target: dataManager
        function onValueChanged(tagName, value) {
            if (tagName === vroot.playTag) vroot.applyPlayTagValue(value)
            if (tagName === vroot.videoIndexTag) vroot.applyIndexValue()   // S-5：索引变 → 切源
        }
    }
    // 装载同步统一由 HmiFrame.onLoaded 末尾 item.syncSources() 显式驱动（见上方 syncSources 注释——
    // 属性变化隐式级联与 Component.onCompleted 的启动/中间态误报已随复审 🔴 移除）；运行时钟频由
    // 上方 Connections（playTag 播放控制 + videoIndexTag 切源）驱动。

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
