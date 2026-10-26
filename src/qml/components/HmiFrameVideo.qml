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
//
// AC-4 (2026-10-24 用户报缺陷「RTSP 源未启动时切过去整机卡死」)：三层防线——
//   L1 预探测：网络源先经 C++ `rtspProbe`（异步 TCP 2s，**不碰 MediaPlayer**）⇒ 不通直接提示，
//      从源头避免 avformat_open_input() 永久等待（FFmpeg RTSP 解复用器默认无超时的根因）；
//   L2 看门狗：探测通过但 5s 内没进 Loaded/Buffered/Playing ⇒ stop+清源+提示（覆盖"端口通但出不了流"）；
//   L3 断流恢复：StalledMedia/心跳停走 ⇒ 提示"视频流中断，正在重连…" + 退避重试（10s→20s→40s 封顶）；
//   **AC-4-fix（V3，2026-10-24）**：网络源 EndOfMedia 不再 play() 重播（直播断流时那是主线程忙循环＝整机卡死），
//      改走 onStreamBroken()（停播+清源+提示+退避重试）；本地文件仍保留重播循环。
//   三者统一走 showHint()，成功路径仍由 applyPlayerSource()/onErrorOccurred 清屏或提示。
import QtQuick 2.15
import QtMultimedia

Item {
    id: vroot

    property string videoSource: ""
    property string playTag: ""   // R-4: 播放控制布尔变量（空=未绑定——仅点击直接控制；非空=变量驱动 true=播放 false=暂停）
    property string videoListRef: ""   // S-5: 视频源列表名（空=未选列表走单源 videoSource）
    property string videoIndexTag: ""  // S-5: 视频源选择变量（整型非负——变量值=列表项索引）
    property string videoListItems: "" // S-5: 视频源列表项（生成器按列表展开 | 分隔源地址串，含空占位项）
    property string pendingUrl: ""     // AC-4: 正在预探测的网络源（空=无探测中）
    property int retryCount: 0         // AC-4-fix（V3）: 连续重试次数（出帧即归零，间隔 10s→20s→40s 封顶）
    property real lastPosition: 0      // AC-4-fix（V3）: 心跳抽样基准（position 推进位）
    property int stillCount: 0         // AC-4-fix（V3）: 连续未推进次数（≥6 判中断）
    property string lastTargetUrl: ""  // AC-4-fix（V3）: 上次目标源（换源 ⇒ 退避计数归零）
    property string brokenWhy: ""      // AC-4-fix2（V3）: 待处置的断流原因
    property string brokenHint: ""     // AC-4-fix2（V3）: 待显示的提示文案（空=用默认"视频流中断，正在重连…"）
    property bool clearingSource: false // AC-4-fix3（V3）: 程序性清源中（屏蔽 onSourceChanged 的变量回灌）

    // 本地绝对路径 → file:// URL；空/已带 file:///qrc:/rtsp://http(s):// 前缀原样返回（与 HmiImage.toFileUrl 先例对齐）
    function toFileUrl(p) {
        if (p === undefined || p === null || p === "") return p
        if (p.indexOf("file://") === 0 || p.indexOf("qrc:/") === 0 || p.indexOf("rtsp://") === 0
                || p.indexOf("http://") === 0 || p.indexOf("https://") === 0)
            return p
        if (p.charAt(0) === "/") return "file://" + p
        return p
    }

    // AC-4: 网络源判定（本地文件/工程内资源不走预探测）
    function isNetUrl(u) {
        var s = ("" + u).toLowerCase()
        return s.indexOf("rtsp://") === 0 || s.indexOf("rtspu://") === 0
            || s.indexOf("http://") === 0 || s.indexOf("https://") === 0
    }    // AC-4: 探测失败原因 → 用户可读文案
    function reasonText(r) {
        if (r === "timeout") return "连接超时"
        if (r === "refused") return "服务未启动/拒绝连接"
        if (r === "unresolved") return "主机名无法解析"
        if (r === "unsupported") return "源格式不支持"
        return "连接失败"
    }
    // AC-4: 统一提示（info=true 用白字，失败用红字）
    function showHint(t, info) {
        stateText.text = t
        stateText.color = info ? "#FFFFFF" : "#FF5252"
        stateText.visible = true
    }
    function clearHint() {
        stateText.visible = false
    }
    function stopTimers() {
        watchdog.stop()
        stallTimer.stop()
        retryTimer.stop()
        probeGuard.stop()
        frameGuard.stop()
        brokenTimer.stop()
        vroot.brokenWhy = ""
        vroot.brokenHint = ""
        vroot.pendingUrl = ""   // AC-4 复审：换源/停表时清探测挂起态（防旧探测迟到改写状态）
        vroot.stillCount = 0
    }
    // AC-4-fix（V3，2026-10-24）：退避重试间隔（10s→20s→40s 封顶），防重试风暴
    function retryInterval() {
        var step = Math.min(vroot.retryCount, 2)
        return Math.min(40000, 10000 * Math.pow(2, step))
    }
    // AC-4-fix2（V3，2026-10-24）：断流信号处理 —— **不得在多媒体信号回调内直接销毁播放器**。
    // 实测（板端 SIGSEGV，3 次错误循环崩 1 次 = 竞态）：onErrorOccurred/onMediaStatusChanged 回调栈内
    // 同步调用 player.stop()+player.source="" 会与 Qt6.4.3 ffmpeg 后端 demuxer/decoder 线程的收尾相撞
    // ⇒ 段错误（守护重启，日志表现为「该路径不可播放…」紧接着「收到信号 11」）。⇒ 一律延迟一拍（150ms）
    // 再 stop/清源，让后端先把错误处理走完。
    function scheduleBroken(why, hintText) {
        var h = (hintText === undefined || hintText === null) ? "" : hintText
        if (brokenTimer.running) {
            // 复审 🟡H：已在处置窗口内不重启计时器（防同一错误多次排程），但**更具体的文案要能覆盖**
            //   （否则"EndOfMedia 通用文案"会盖掉随后到达的"视频源无法播放: <具体源>"）
            if (h !== "" && vroot.brokenHint === "") {
                vroot.brokenHint = h
                vroot.showHint(h)
            }
            return
        }
        vroot.brokenWhy = why
        vroot.brokenHint = h
        if (h !== "") vroot.showHint(h)   // 文案可以立刻显示（不碰播放器）
        console.warn("[HmiFrameVideo] 断流处置已排程(" + why + ") 150ms 后停播清源")
        brokenTimer.restart()
    }
    // 断流统一处置 —— **只在计时器回调里调用**（离开多媒体信号栈）；清源是必须的（否则恢复推流后不会重开）
    function onStreamBroken(why, hintText) {
        var src = player.source.toString()
        if (src === "" && retryTimer.running) return   // 已在重连窗口内（防重复触发刷日志/重置退避）
        console.info("[HmiFrameVideo] 断流处置开始(" + why + ") source=" + src)
        player.stop()
        vroot.clearingSource = true   // AC-4-fix3：屏蔽清源引起的 onSourceChanged 变量回灌（否则死循环，实测 6 次/秒）
        player.source = ""
        vroot.clearingSource = false
        vroot.stillCount = 0
        vroot.lastPosition = 0
        // AC-4-fix5（复审 🔴E）：本地源没有"重连"这条路 ⇒ 提示必须可操作（否则红字永挂、无自愈路径）
        var netTarget = vroot.isNetUrl(vroot.toFileUrl(vroot.currentTargetSource()))
        var shown = (hintText === undefined || hintText === null || hintText === "")
                     ? (netTarget ? "视频流中断，正在重连…" : "视频源无法播放（请检查文件格式或路径）")
                     : hintText
        vroot.showHint(shown)
        console.warn("[HmiFrameVideo] 视频流中断(" + why + ") source=" + src
                     + " netTarget=" + netTarget + " retryIn=" + vroot.retryInterval() + "ms")
        vroot.scheduleRetry()
        // AC-4-fix5（复审 🔴A：上轮此处的无条件 watchdog.restart() 是缺陷）：看门狗的首行守卫是
        //   `source === "" ⇒ return`，清源后起表**恒为死表**——只会让失败周期多等 5s（实测 10s→15s 漂移）
        //   且打误导日志。清源后的兜底由 retryTimer 自身负责，故仅在"仍有源"时起表（本地源不重试，靠提示）。
        if (player.source.toString() !== "") watchdog.restart()
    }
    function scheduleRetry() {
        if (vroot.isNetUrl(vroot.toFileUrl(vroot.currentTargetSource()))) {
            vroot.retryCount = vroot.retryCount + 1
            retryTimer.interval = vroot.retryInterval()
            retryTimer.restart()
        }
    }

    // S-5: 列表项数组（videoListItems split——保留空占位项使索引与 PC 列表项对齐）
    property var sourceItems: videoListItems.length > 0 ? videoListItems.split("|") : []

    // R-4: 布尔值 → 播放/暂停（"1"/"true" → 播；"0"/""/"false" → 暂停；playTag 空=未绑定不响应）
    function applyPlayTagValue(v) {
        if (vroot.playTag === "") return
        var s = (v === undefined || v === null) ? "" : ("" + v).trim().toLowerCase()
        if (s === "1" || s === "true") {
            if (player.source.toString() === "") {
                vroot.applySource("applyPlayTagValue")   // AC-4 复审：空源不 play（走重探测）
                return
            }
            if (player.playbackState !== MediaPlayer.PlayingState) player.play()
        } else {
            if (player.playbackState === MediaPlayer.PlayingState) player.pause()
        }
    }
    // R-3/R-4: 点击切换播放/暂停 + 写回控制变量（变量驱动与点击两入口一致）
    // AC-4 复审 🔴：**无有效源时不得对空源 play()**（会把 InvalidMedia/errorOccurred 打出来，覆盖掉"服务未启动"这类具体原因）
    //   ⇒ 空源改走 applySource()（重新探测/提示），与变量入口行为一致
    function togglePlay() {
        if (player.source.toString() === "") {
            vroot.applySource("togglePlay")
            return
        }
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

    // AC-4: 真正把源交给 MediaPlayer（仅"探测通过"或"本地源"会走到这里）
    function applyPlayerSource(url) {
        if (player.source.toString() !== url)   // 复审 🟡：player.source 读回为 QUrl 对象——toString 化显式比较（同源重入幂等）
            player.source = url
        else if (player.playbackState !== MediaPlayer.PlayingState
                   && player.mediaStatus !== MediaPlayer.EndOfMedia)
            player.play()
        // AC-4-fix4（复审 🔴③）：换源即重置心跳基准——否则上一轮残留 stillCount 会让新流在 6s 内被误判断流
        vroot.stillCount = 0
        vroot.lastPosition = player.position
        clearHint()
        watchdog.restart()   // L2：设源后开始计时
    }

    // S-5: 应用当前目标源（不同 → 重设 source（onSourceChanged 触发播放）；相同 → 未播则补播）
    // 复审 🔴（2026-09-05 FW 审）：成功路径必须清屏——错误文本（S-6/空源提示）一旦显示永不消除，
    // 正常播放中会常显红色错误覆盖层；此处有效源即清屏（后续切源成功同样清除，错误只留在真正无效/失败时）。
    // AC-4：网络源先探测（L1）——**探测不通绝不设 player.source**，这是"不卡死"的关键。
    function applySource(why) {
        var target = vroot.currentTargetSource()
        console.info("[HmiFrameVideo] applySource 来源=" + (why === undefined ? "?" : why)
                     + " target=" + target + " retryCount=" + vroot.retryCount)
        var prevTarget = vroot.lastTargetUrl
        // AC-4-fix（V3）：换了目标源 ⇒ 退避计数归零（人工切源重新给足重试预算；重连同一源则继续退避）
        if (target !== prevTarget) {
            vroot.lastTargetUrl = target
            vroot.retryCount = 0
        } else if (retryTimer.running) {
            // AC-4-fix3（V3）：**同一目标**已在重连退避期内 ⇒ 忽略重复触发（防「清源→变量回灌→再探测」自激循环）。
            //   判定必须用 prevTarget（换源场景 lastTargetUrl 已更新，用新值比较会把人工切源也拦下——实测切本地源被误拦）
            console.info("[HmiFrameVideo] applySource 忽略（同目标退避中, 来源=" + why + "）")
            return
        }
        stopTimers()
        if (target === undefined || target === null || target === "") {
            // U-5（2026-09-06 用户拍板「切了坏画面，应该立即停播，并且显示文字」）：空源/越界/坏项 → **停播清画面**
            //（原 S-5「不清旧源继续播」语义反转——避免旧视频继续播误导当前源状态）
            // 审查 🟡①（2026-09-06 FW 审）：同时清 source——防 stop 后旧源 URL 残留被点击/playTag 复活复播
            //（红字与旧视频并存的矛盾态）；source 清空触发 onSourceChanged 但守卫（source !== ""）不补播
            player.stop()
            vroot.clearingSource = true
            player.source = ""
            vroot.clearingSource = false
            vroot.pendingUrl = ""
            // AD（2026-10-24 用户裁决 ①）：三种空源语义分开提示——它们对用户意味着完全不同的动作：
            //   ① 根本没配视频列表 → "未选择视频列表"（去属性面板选列表）
            //   ② 配了列表但列表没有有效项 → "视频列表无有效项"（去列表管理补项）
            //   ③ 配了列表且列表有项，但索引越界/指向空项 → "当前索引无有效源"（查视频源选择变量）
            if (vroot.videoListRef === "" && vroot.videoSource === "")
                showHint("视频源不可用（未选择视频列表）")
            else if (vroot.videoListRef !== "" && vroot.sourceItems.length === 0)
                showHint("视频源不可用（视频列表无有效项）")
            else
                showHint("视频源不可用（当前索引无有效源）")
            console.warn("[HmiFrameVideo] 无有效视频源 listRef=" + vroot.videoListRef + " videoSource=" + vroot.videoSource)
            return
        }
        var url = vroot.toFileUrl(target)
        if (vroot.isNetUrl(url)) {
            // AC-4-fix5（复审 🔴C）：切到新网络源时**先把旧流停掉并清源**——否则 2.8s 探测窗口内旧视频照常播放，
            //   与"正在连接视频源…"提示叠加（同类矛盾态，违 U-5「切了坏画面立即停播」）；也避免心跳采到上一路
            //   的 position 恒值被误判 6s 无帧（复审 🔴B 的根因之一）。
            if (player.source.toString() !== "") {
                player.stop()
                vroot.clearingSource = true
                player.source = ""
                vroot.clearingSource = false
            }
            vroot.stillCount = 0
            vroot.lastPosition = 0
            vroot.pendingUrl = url
            showHint("正在连接视频源…", true)
            if (typeof rtspProbe !== "undefined" && rtspProbe !== null) {
                rtspProbe.probe(url, 2000)   // L1：异步 2s；结果走下方 Connections
                probeGuard.restart()         // 复审 🔴：每实例兜底表——探测服务是全局单例，多视频帧时兄弟帧的 probe() 会
                                             //  cancel 本帧的探测 ⇒ 本帧永远收不到 probeFinished ⇒ 必须自己超时收口
            } else {
                // 复审 🔴 g)：探测服务不可用 ⇒ **fail-safe：绝不设源**（旧镜像/降级场景下若把未探测 URL 交给
                //  MediaPlayer，仍会走 avformat_open_input() 永久等待 ⇒ 原样复现卡死，看门狗也在同一事件循环救不了）
                console.warn("[HmiFrameVideo] rtspProbe 未注册，拒绝直接设网络源（fail-safe）")
                player.stop()
                vroot.clearingSource = true
                player.source = ""
                vroot.clearingSource = false
                showHint("视频源无法播放（探测服务不可用）")
                scheduleRetry()
            }
            return
        }
        vroot.pendingUrl = ""
        applyPlayerSource(url)
    }
    // 复审 🔴（2026-09-05 FW 审）：装载同步显式化——原 onVideoSourceChanged/onVideoListItemsChanged/
    // onVideoListRefChanged 隐式级联 + Component.onCompleted 无条件 applySource()：onCompleted 早于
    // HmiFrame.onLoaded 赋属性（此时全默认 ""）必误报一次；onLoaded 逐参赋值途中（listRef 已赋、
    // videoListItems 未赋）的中间态同样误报。现由 HmiFrame.onLoaded 末尾显式调本函数一次（五参齐备后），
    // 无中间态级联；运行时切源仍由下方 Connections(videoIndexTag) → applyIndexValue 驱动。
    function syncSources() {
        vroot.applySource("syncSources")
    }
    // S-5: 索引变量值变化 → 重取列表项切源（列表模式）
    function applyIndexValue() {
        if (vroot.videoListRef !== "" && vroot.sourceItems.length > 0)
            vroot.applySource("applyIndexValue")
    }

    // AC-4: L1 预探测结果（context property `rtspProbe` 由 main.cpp 注入）
    Connections {
        target: (typeof rtspProbe !== "undefined" && rtspProbe !== null) ? rtspProbe : null
        function onProbeFinished(url, ok, reason) {
            if (url !== vroot.pendingUrl)
                return   // 迟到/已被新源取代的探测结果
            vroot.pendingUrl = ""
            probeGuard.stop()
            if (ok) {
                if (brokenTimer.running) return   // 复审 🟡：已排程处置中，不再起播放器（否则在将被清的源上白起一次）
                vroot.applyPlayerSource(url)   // TCP 可达 ⇒ 交给播放器，L2 看门狗接管
            } else {
                vroot.scheduleBroken("unreachable", "视频源无法播放（" + vroot.reasonText(reason) + "）")
                console.warn("[HmiFrameVideo] 视频源不可达 source=" + url + " reason=" + reason)
            }
        }
    }

    // AC-4 复审 🔴：每实例探测兜底（2.8s：探测自身 2s + brokenTimer 排程 150ms + connectToHost/事件循环抖动，
    //   并留出余量低于 V1 的"≤3s 出提示"判据——原 3000ms 与判据同秒边界）。
    // AC-4-fix4（2026-10-24 复审 🔴①）：本计时器**不再自行 stop+清源**（第二份清源实现会与 brokenTimer 竞态、
    //   把统一处置连同退避重试整段吃掉）——统一走 scheduleBroken → onStreamBroken。
    Timer {
        id: probeGuard
        interval: 2800
        repeat: false
        onTriggered: {
            if (vroot.pendingUrl === "") return
            if (brokenTimer.running) {
                // 复审 🔴D：已在统一处置窗口内——**也要清 pendingUrl**（本计时器 repeat:false 不会再来，
                //   留着会让 probeGuard 状态永久"在探测"）
                vroot.pendingUrl = ""
                return
            }
            console.warn("[HmiFrameVideo] 探测无结果（2.8s），按超时处理 source=" + vroot.pendingUrl)
            vroot.pendingUrl = ""
            vroot.scheduleBroken("probe-timeout", "视频源无法播放（连接超时）")
        }
    }

    // AC-4 · L2：设源后 5s 看门狗（未进就绪态 ⇒ 判定不可播放）
    Timer {
        id: watchdog
        interval: 5000
        repeat: false
        onTriggered: {
            if (player.source.toString() === "") return
            if (player.playbackState === MediaPlayer.PlayingState) return
            if (player.mediaStatus === MediaPlayer.LoadedMedia || player.mediaStatus === MediaPlayer.BufferedMedia) return
            var stalledSrc = player.source.toString()
            console.warn("[HmiFrameVideo] 看门狗超时（5s 未就绪）source=" + stalledSrc)
            vroot.onStreamBroken("watchdog 5s", "视频源无法播放（连接超时）")   // 计时器回调内 ⇒ 可直接处置
        }
    }

    // AC-4 · L3：断流判定（StalledMedia 持续 5s）
    Timer {
        id: stallTimer
        interval: 5000
        repeat: false
        onTriggered: {
            if (player.mediaStatus !== MediaPlayer.StalledMedia) return
            vroot.onStreamBroken("Stalled 5s")   // 与心跳判中断共用同一处置（停播+清源+提示+退避重试）
        }
    }

    // AC-4：退避重试（仅网络源；切源/索引变化会重置）
    Timer {
        id: retryTimer
        interval: 10000
        repeat: false
        onTriggered: vroot.applySource("retryTimer")
    }

    // AC-4-fix（V3，2026-10-24）：L3 心跳——**网络流在播但 6s 无帧推进 ⇒ 判中断**。
    // 为什么必须有：publisher 停掉时 FFmpeg 可能既不报 error 也不进 StalledMedia（实测 CPU 打满、无日志），
    // 只有 position 停走这一个可观测信号；本地文件不快进、暂停时不起表。
    Timer {
        id: frameGuard
        interval: 1000
        repeat: true
        running: !vroot.clearingSource && !brokenTimer.running
                 && vroot.pendingUrl === ""          // 复审 🔴B：探测期不采样（上一路 position 恒值会误判 6s 无帧）
                 && vroot.isNetUrl(player.source.toString())
                 && player.playbackState === MediaPlayer.PlayingState
        onTriggered: {
            if (player.position !== vroot.lastPosition) {
                vroot.lastPosition = player.position
                vroot.stillCount = 0
            } else if (vroot.stillCount + 1 >= 6) {
                vroot.stillCount = 0
                vroot.onStreamBroken("6s 无帧推进")
            } else {
                vroot.stillCount = vroot.stillCount + 1
            }
        }
    }

    // AC-4-fix2（V3）：断流处置延迟一拍（离开多媒体信号栈后再销毁播放器——防 SIGSEGV 竞态）
    Timer {
        id: brokenTimer
        interval: 150
        repeat: false
        onTriggered: vroot.onStreamBroken(vroot.brokenWhy, vroot.brokenHint)
    }

    // 页面/控件销毁时收尾（停表 + 取消在途探测，避免迟到的 probeFinished 打到已销毁对象）
    Component.onDestruction: {
        stopTimers()
        vroot.clearingSource = false   // 复审 ⚪：销毁路径若在 onStreamBroken 内部被打断，防标志残留
        if (typeof rtspProbe !== "undefined" && rtspProbe !== null)
            rtspProbe.cancel()
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
        // AC-4：另接入 L2 就绪判定与 L3 断流判定
        onMediaStatusChanged: {
            var srcNow = player.source.toString()
            var netNow = vroot.isNetUrl(srcNow)
            // AC-4-fix（V3，2026-10-24 用户报"RTSP 断流整机卡死"）：EndOfMedia 分支**必须按源类型区分**——
            //   本地文件 ⇒ 重播（R-1 循环播放兜底）；**网络流 ⇒ 绝不能再 play()**。
            //   实测根因：直播断流时 play() 在 Stopped+EndOfMedia 态立刻又触发 EndOfMedia，形成主线程忙循环
            //   （主线程 128~138% CPU、无任何日志、navihmi-cli 无响应 = 用户看到的"整机卡死"）。
            if (mediaStatus === MediaPlayer.EndOfMedia) {
                if (netNow) {
                    stallTimer.stop()                      // 复审 🟡G：终态即停表（防 stallTimer 残留到下一路）
                    vroot.scheduleBroken("EndOfMedia")     // 延迟处置：不在本回调栈内销毁播放器
                } else if (playbackState !== MediaPlayer.PlayingState) {
                    play()
                }
            } else if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia) {
                // 复审 🟡（改 else-if）：就绪分支不与其他终态同拍执行，避免异常路径「先 InvalidMedia 再 Loaded」
                //   把刚显示的断流红字清掉（提示气泡）
                stallTimer.stop()
                watchdog.stop()        // L2：进就绪态即停表（本地大文件 Loading 较久也不会被误判）
                vroot.retryCount = 0   // 真出帧了 ⇒ 退避计数归零
                vroot.stillCount = 0
                if (!brokenTimer.running) clearHint()   // 复审 ⚪：恢复播放也要清旧提示，但正在处置断流时别清
            } else if (mediaStatus === MediaPlayer.InvalidMedia) {
                // 后端明确报"媒体无效"：立即提示（文案不碰播放器）+ 延迟一拍停播清源
                if (netNow) {
                    vroot.scheduleBroken("InvalidMedia", "视频流中断，正在重连…")
                } else {
                    vroot.scheduleBroken("InvalidMedia(local)", "视频源无法播放（格式不受支持或流已结束）")
                }
            } else if (mediaStatus === MediaPlayer.StalledMedia) {
                stallTimer.restart()   // L3 起表；恢复后停表
            } else {
                stallTimer.stop()
            }
        }
        // 审查 🟡（2026-09-04，复审驳回后修正）：显式 play() 幂等兜底——不依赖「source/autoPlay 同一变更内
        // 绑定求值顺序」。处理器挂 MediaPlayer 自身 onSourceChanged（source 重算后确定性触发）
        onSourceChanged: {
            // AC-4 复审 🔴：QUrl 属性 vs 字符串严格比较恒真 ⇒ 必须 toString()（否则清源后仍会 play() 空源）
            if (playbackState !== MediaPlayer.PlayingState && player.source.toString() !== "")
                play()
            // 审查 🟡（2026-09-05）：onCompleted 早于 Loader.onLoaded 赋 playTag → 初始同步失效——
            // source 就绪（onLoaded 赋值后触发本 handler）按变量当前值纠正（绑定 false 的画面打开即暂停）
            // AC-4：守卫已改为 toString()（QUrl 对象与字符串严格比较恒真，等于没判空）
            // AC-4-fix3（V3，2026-10-24 实测死循环根因）：**清源后不得把播放变量回灌**——空源会让
            //   applyPlayTagValue 去调 applySource() 重探测，探测→报错→清源→再回灌，形成 6 次/秒的自激循环
            //  （板端实测 1221 次排程/10s + cpu 27%）。⇒ 只在「有真实源」且非程序性清源时才做变量纠正。
            if (!vroot.clearingSource && player.source.toString() !== ""
                    && vroot.playTag !== "" && dataManager)
                vroot.applyPlayTagValue(dataManager.value(vroot.playTag))
        }
        // S-6（2026-09-05）：播放失败诊断——打印当前源路径 + 明确「该路径不可播放」提示（非静默）
        // U-5（2026-09-06）：失败即停播（player.stop 清 error 态/残留画面——用户拍板切坏源立即停播）
        // AC-4：失败后停表并按退避重试（网络源）
        onErrorOccurred: function (error, errorString) {
            // AC-4-fix2（V3，2026-10-24）：**本回调栈内不得动播放器**（板端实测 SIGSEGV 竞态）——
            //   先显示具体原因（只改文本），再延迟 150ms 走统一断流处置（停播+清源+退避重试）。
            watchdog.stop()
            var src = vroot.currentTargetSource()
            console.warn("[HmiFrameVideo] 该路径不可播放 source=" + src
                         + " err=" + errorString + " errCode=" + error)
            vroot.scheduleBroken("errorOccurred",
                                 "视频源无法播放: " + (src === "" ? "(空/越界)" : src))
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
