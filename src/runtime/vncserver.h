/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncserver.h
 * @Description: VNC 服务层（W-D F19 三层职责拆分——配置层 VncConfig / 管理层 VncManager / 服务层 VncServer）。
 *               单个 VNC 客户端会话：RFB 3.3 协议状态机（版本/ClientInit/消息解析）+ 编码推送
 *               （ServerInit/增量区域/全帧/心跳，Raw 32bpp）+ 输入注入（Pointer/Key → QWindowSystemInterface）。
 *               由 VncManager 在客户端接入时创建，生命周期归 Manager；本类只服务一条连接。
 *               ⚠️ 逻辑迁移自原 vncmirror.cpp（行为零改动，仅按职责归档）；线程约束同原实现：
 *               onReadyRead/发送在 GUI 线程；wantsUpdate 由渲染线程（Manager 帧消费）在锁内读取。
 */
#pragma once

#include <QObject>
#include <QByteArray>
#include <QRect>
#include <QVector>
#include <QAtomicInteger>

class QTcpSocket;
class QQuickWindow;

namespace navihmi {

class VncServer : public QObject
{
    Q_OBJECT
public:
    /// @param socket 已接入的客户端 socket（所有权归本会话，断开后由 Manager 清理）
    /// @param devW/devH 会话创建时刻的设备物理尺寸快照（N+24：尺寸恒定物理屏，重载工程不变化）
    VncServer(QTcpSocket* socket, QQuickWindow* window, int devW, int devH, QObject* parent = nullptr);
    ~VncServer() override;

    QTcpSocket* socket() const { return m_socket; }

    /// 客户端是否需要帧更新（ServerInit / FramebufferUpdateRequest 置位；原实现永不清除——
    /// 客户端在线即持续推帧，打破「服务器发得慢 → 客户端 FBU 请求慢 → 更慢」反馈环）
    bool wantsUpdate() const { return m_needsUpdate; }

    /// GUI 线程：发送矩形区域增量（rects[i] ↔ datas[i]，Manager 帧消费后逐个会话调用）
    void sendRegions(const QVector<QRect>& rects, const QVector<QByteArray>& datas);
    /// GUI 线程：发送全帧（首帧/退化）
    void sendFullFrame(const QByteArray& bgra);
    /// GUI 线程：发送心跳帧（0 矩形，维持客户端帧率计数）
    void sendHeartbeat();

signals:
    /// 客户端请求帧更新（FBU / ServerInit 后）→ VncManager 按节流触发 window->update()（GUI 线程）
    void framebufferRequested();

private slots:
    void onReadyRead();   // GUI 线程：socket 可读 → 协议状态机

private:
    enum class ClientState { WaitVersion, WaitClientInit, Ready };

    void processClient();
    void sendServerInit();
    void handleMessage(quint8 type, const QByteArray& payload);
    void injectPointer(int x, int y, quint8 mask);
    void injectKey(quint32 keysym, bool down);

    QTcpSocket* m_socket = nullptr;
    QQuickWindow* m_window = nullptr;
    int m_devW = 0;
    int m_devH = 0;
    ClientState m_state = ClientState::WaitVersion;
    QByteArray m_buf;
    bool m_needsUpdate = false;
    quint8 m_lastButtons = 0;
};

} // namespace navihmi
