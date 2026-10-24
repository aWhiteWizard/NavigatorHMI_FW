/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\rtsp_probe.h
 * @Description: 网络视频源可达性预探测（AC-2 2026-10-24，修复「RTSP 源不可达 ⇒ 整机卡死」）
 *
 *   背景：HmiFrameVideo.qml 把 player.source 设成不可达的 rtsp:// 后，Qt/FFmpeg 后端
 *         avformat_open_input() 对 RTSP 解复用器**默认没有任何超时**（FW 全项目零处
 *         rw_timeout/stimeout）⇒ 永久等待 ⇒ MediaPlayer.onErrorOccurred 永不触发
 *         ⇒ 提示走不到、界面冻死（详见 4_bugs/rk3562/rtsp-source-unreachable-freezes-ui.md）。
 *
 *   对策（L1 层）：设源之前先做一次**异步 TCP 可达性探测**——不通就根本不碰 MediaPlayer，
 *         因此本类**不依赖"阻塞发生在哪个线程"**（哪怕阻塞在主线程，我们也进不到那一步）；
 *         纯 QTcpSocket 异步 + QTimer 超时，**绝不阻塞 GUI 线程**。
 *
 *   语义：ok=true 只代表 host:port 能 TCP 连上（不代表能出流）；出流失败仍由 QML 看门狗（L2）兜底。
 */
#pragma once

#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

namespace navihmi {

/// 网络源（rtsp/http/https）可达性预探测：QML 侧作为 context property `rtspProbe` 使用。
class RtspProbe : public QObject
{
    Q_OBJECT
public:
    explicit RtspProbe(QObject* parent = nullptr);
    ~RtspProbe() override;

    /// 探测 URL 的 host:port（rtsp 缺省 554 / http 80 / https 443）。
    /// **幂等**：同一 URL 正在探测时重复调用直接忽略；换 URL 会取消上一次（旧请求不再发信号）。
    /// 结果经 probeFinished(url, ok, reason) 回送；
    /// reason ∈ **ok / timeout / refused / unresolved / unsupported / error**（error = 其他 socket 错误，
    /// 如 RemoteHostClosed/Bearer 类；QML 侧 reasonText() 回落为「连接失败」）。
    /// 注：仅判定 **TCP 可达**，不含 RTSP 鉴权/会话协商——鉴权失败(401)/路径不存在(404) 由播放器 L2 兜底。
    Q_INVOKABLE void probe(const QString& url, int timeoutMs = 2000);

    /// 取消进行中的探测（切换源/页面销毁时调用；不发信号）
    Q_INVOKABLE void cancel();

signals:
    void probeFinished(const QString& url, bool ok, const QString& reason);

private:
    void finish(bool ok, const QString& reason);
    static bool parseEndpoint(const QString& url, QString& host, quint16& port);
    void cleanup();

    QTcpSocket* m_socket = nullptr;
    QTimer* m_timer = nullptr;
    QString m_url;   // 进行中的 URL（空 = 无探测）
};

} // namespace navihmi
