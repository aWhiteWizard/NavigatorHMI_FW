/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncmirror.h
 * @Description: 内嵌 VNC 镜像服务（RFB 3.3）
 *               应用照常跑 eglfs 物理屏（触摸/显示正常），本服务在渲染线程
 *               afterRendering 时 glReadPixels 读已渲染帧推给 VNC 客户端（5900），
 *               并把 VNC 鼠标/键盘事件回灌进应用。
 *               无人车场景默认关闭（proj.enable_vnc=false），零内存/CPU 开销。
 *               2026-08-20 修正：不再用 grabWindow 定时抓帧（eglfs 上高频
 *               grabWindow 会打断渲染导致显示空白），改 afterRendering 读帧。
 */
#pragma once

#include <QObject>
#include <QImage>
#include <QRect>
#include <QVector>
#include <QMutex>
#include <QElapsedTimer>
#include <QOpenGLFunctions>   // GLuint / QOpenGLFunctions*

class QTcpServer;
class QTcpSocket;
class QQuickWindow;
class QTimer;
class QOpenGLVertexArrayObject;
namespace navihmi {

class VncMirror : public QObject
{
    Q_OBJECT
public:
    explicit VncMirror(QQuickWindow* window, QObject* parent = nullptr);
    ~VncMirror() override;

    /// 设备尺寸（VNC 屏幕尺寸 = 工程 deviceWidth/Height，如 1024x600）
    void setDeviceSize(int w, int h);

    /// 启动监听（默认 5900；监听失败返回 false 并记日志）
    bool start(quint16 port = 5900);
    void stop();
    bool isRunning() const { return m_server != nullptr; }

    /// QML 生产端报告画面变化区域（西门子 dirty-rect 模式：QML 层知道哪里变了，
    /// VNC 只读该区域全分辨率，绕开全帧 792ms 读回）。线程安全。
    Q_INVOKABLE void markDirty(int x, int y, int w, int h);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    enum class ClientState { WaitVersion, WaitClientInit, Ready };

    struct Client {
        QTcpSocket* socket = nullptr;
        ClientState state = ClientState::WaitVersion;
        QByteArray buf;
        bool needsUpdate = false;
    };

    Client* clientFor(QTcpSocket* s);
    void processClient(Client* c);
    void sendServerInit(Client* c);
    void handleMessage(Client* c, quint8 type, const QByteArray& payload);
    void sendRawRect(Client* c, const QByteArray& bgra, int w, int h);
    void onAfterRendering();   // 渲染线程：读帧
    void injectPointer(int x, int y, quint8 mask);
    void injectKey(quint32 keysym, bool down);

    /// 渲染线程：读指定矩形区域全分辨率 → BGRA（垂直翻转）
    bool readRegion(QOpenGLFunctions* f, const QRect& r, int w, int h, QByteArray& bgra);
    /// GUI 线程：发送矩形区域增量（rects[i] ↔ datas[i]）
    void sendRegions(const QVector<QRect>& rects, const QVector<QByteArray>& datas, int w, int h);
    /// 发送全帧（首帧/大变化退化）
    void sendFullFrame(const QByteArray& bgra, int w, int h);
    /// 发送心跳帧（0 矩形，维持客户端帧率计数）
    void sendHeartbeat();
    /// RGBA(左下) → BGRA(左上, 垂直翻转)
    static QByteArray rgbaToBgra(const QByteArray& rgba, int w, int h);

    QQuickWindow* m_window = nullptr;
    QTcpServer* m_server = nullptr;
    QVector<Client*> m_clients;
    QMutex m_clientsMutex;
    int m_devW = 1024;
    int m_devH = 600;
    QElapsedTimer m_elapsed;
    qint64 m_lastCaptureMs = -1000;   // 上次读帧时刻（节流用）
    QByteArray m_lastFrame;           // 最近捕获帧缓存（连接时立即下发，静态画面可见）
    int m_lastFrameW = 0, m_lastFrameH = 0;

    // 生产端脏矩形报告（QML 层 markDirty 累积，渲染线程消费）
    QVector<QRect> m_dirtyRects;
    QMutex m_dirtyMutex;

    // 输入状态
    quint8 m_lastButtons = 0;
};

} // namespace navihmi
