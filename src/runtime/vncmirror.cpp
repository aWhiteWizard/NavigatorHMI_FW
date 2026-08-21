/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncmirror.cpp
 * @Description: 内嵌 VNC 镜像服务实现（RFB 3.3 / None / Raw 32bpp）
 *               帧捕获：QQuickWindow::afterRendering（渲染线程）→ glReadPixels
 *               读默认帧缓冲（零额外渲染，不打扰 eglfs 实时显示）。
 */
#if defined(HAVE_QT_QML)

#include "runtime/vncmirror.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
// SDK Qt 6.4.3 的 qpa 私有头在版本化路径下（<prefix>/include/QtGui/6.4.3/QtGui/qpa/）
#include <QtGui/6.4.3/QtGui/qpa/qwindowsysteminterface.h>
#include <cstring>

namespace navihmi {

namespace {

// RFB 大端工具
inline quint16 be16(const QByteArray& b, int o)
{
    return quint16((quint8(b.at(o)) << 8) | quint8(b.at(o + 1)));
}
inline quint32 be32(const QByteArray& b, int o)
{
    return (quint32(quint8(b.at(o))) << 24) | (quint32(quint8(b.at(o + 1))) << 16)
         | (quint32(quint8(b.at(o + 2))) << 8) | quint32(quint8(b.at(o + 3)));
}
inline void put16(QByteArray& b, quint16 v) { b.append(char(v >> 8)); b.append(char(v & 0xff)); }
inline void put32(QByteArray& b, quint32 v)
{
    b.append(char(v >> 24)); b.append(char(v >> 16)); b.append(char(v >> 8)); b.append(char(v));
}

// X11 keysym → Qt::Key（常用子集）
Qt::Key keysymToQt(quint32 ks)
{
    if (ks >= 'a' && ks <= 'z') return Qt::Key(ks - 'a' + Qt::Key_A);
    if (ks >= 'A' && ks <= 'Z') return Qt::Key(ks - 'A' + Qt::Key_A);
    if (ks >= '0' && ks <= '9') return Qt::Key(ks - '0' + Qt::Key_0);
    switch (ks) {
    case 0x20:  return Qt::Key_Space;
    case 0xff0d: return Qt::Key_Return;
    case 0xff8d: return Qt::Key_Enter;
    case 0xff08: return Qt::Key_Backspace;
    case 0xff09: return Qt::Key_Tab;
    case 0xff1b: return Qt::Key_Escape;
    case 0xffff: return Qt::Key_Delete;
    case 0xff51: return Qt::Key_Left;
    case 0xff52: return Qt::Key_Up;
    case 0xff53: return Qt::Key_Right;
    case 0xff54: return Qt::Key_Down;
    case 0xff50: return Qt::Key_Home;
    case 0xff57: return Qt::Key_End;
    case 0xff55: return Qt::Key_PageUp;
    case 0xff56: return Qt::Key_PageDown;
    case 0xffe1: return Qt::Key_Shift;
    case 0xffe2: return Qt::Key_Shift;
    case 0xffe3: return Qt::Key_Control;
    case 0xffe4: return Qt::Key_Control;
    case 0xffe9: return Qt::Key_Alt;
    case 0xffea: return Qt::Key_Alt;
    default:
        if (ks >= 0xffbe && ks <= 0xffc9) return Qt::Key(ks - 0xffbe + Qt::Key_F1);
        return Qt::Key_unknown;
    }
}

} // namespace

VncMirror::VncMirror(QQuickWindow* window, QObject* parent)
    : QObject(parent), m_window(window)
{
    m_elapsed.start();
}

VncMirror::~VncMirror()
{
    stop();
}

void VncMirror::setDeviceSize(int w, int h)
{
    if (w > 0 && h > 0) { m_devW = w; m_devH = h; }
}

bool VncMirror::start(quint16 port)
{
    if (m_server)
        return true;
    if (!m_window) {
        qWarning().noquote() << "VncMirror: 无 QQuickWindow，无法启动";
        return false;
    }
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning().noquote() << "VncMirror: 监听失败" << port << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QTcpServer::newConnection, this, &VncMirror::onNewConnection);
    // 渲染线程读帧：frameSwapped（present 之后）读默认帧缓冲 = 当前实际显示内容
    connect(m_window, &QQuickWindow::frameSwapped, this, &VncMirror::onAfterRendering,
            Qt::DirectConnection);
    qInfo().noquote() << "VncMirror: VNC 服务已启动 :" << port
                      << "屏幕" << m_devW << "x" << m_devH;
    return true;
}

void VncMirror::stop()
{
    if (!m_server)
        return;
    disconnect(m_window, &QQuickWindow::frameSwapped, this, &VncMirror::onAfterRendering);
    m_server->close();
    delete m_server;
    m_server = nullptr;
    QMutexLocker lock(&m_clientsMutex);
    qDeleteAll(m_clients);
    m_clients.clear();
    qInfo().noquote() << "VncMirror: VNC 服务已停止";
}

void VncMirror::onNewConnection()
{
    while (auto* sock = m_server->nextPendingConnection()) {
        auto* c = new Client;
        c->socket = sock;
        sock->setParent(this);
        connect(sock, &QTcpSocket::readyRead, this, &VncMirror::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &VncMirror::onClientDisconnected);
        {
            QMutexLocker lock(&m_clientsMutex);
            bool wasEmpty = m_clients.isEmpty();
            m_clients.append(c);
            // 首个客户端连接 → 通知 QML 启动无限动画驱动持续渲染（30fps；动画驱动渲染循环，
            // 不同于 Timer 设属性——动画让渲染循环连续跑帧，frameSwapped 每帧读回）
            if (wasEmpty && m_window) {
                bool ok = m_window->setProperty("vncMirrorActive", true);
                qInfo().noquote() << "VncMirror: 首个客户端连接, setProperty vncMirrorActive ok=" << ok
                                  << "clients=" << m_clients.size();
            }
        }
        // 服务器先发版本
        sock->write("RFB 003.003\n", 12);
    }
}

VncMirror::Client* VncMirror::clientFor(QTcpSocket* s)
{
    QMutexLocker lock(&m_clientsMutex);
    for (auto* c : m_clients)
        if (c->socket == s) return c;
    return nullptr;
}

void VncMirror::onClientReadyRead()
{
    auto* c = clientFor(qobject_cast<QTcpSocket*>(sender()));
    if (!c) return;
    c->buf.append(c->socket->readAll());
    processClient(c);
}

void VncMirror::onClientDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    QMutexLocker lock(&m_clientsMutex);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if ((*it)->socket == sock) {
            auto* c = *it;
            m_clients.erase(it);
            sock->deleteLater();
            delete c;
            break;
        }
    }
    // 最后一个客户端断开 → 通知 QML 停止动画驱动（恢复零开销）
    if (m_clients.isEmpty() && m_window)
        m_window->setProperty("vncMirrorActive", false);
}

void VncMirror::processClient(Client* c)
{
    auto* s = c->socket;
    for (;;) {
        if (c->state == ClientState::WaitVersion) {
            if (c->buf.size() < 12) return;
            c->buf.remove(0, 12);
            QByteArray sec; put32(sec, 1);
            s->write(sec);
            c->state = ClientState::WaitClientInit;
        } else if (c->state == ClientState::WaitClientInit) {
            if (c->buf.size() < 1) return;
            c->buf.remove(0, 1);
            sendServerInit(c);
            c->state = ClientState::Ready;
        } else { // Ready
            if (c->buf.isEmpty()) return;
            quint8 type = quint8(c->buf.at(0));
            int need = 0;
            switch (type) {
            case 0: need = 20; break;
            case 2: need = 4; break;
            case 3: need = 10; break;
            case 4: need = 8; break;
            case 5: need = 6; break;
            case 6: need = 8; break;
            default:
                s->disconnectFromHost();
                return;
            }
            if (c->buf.size() < need) return;
            int total = need;
            if (type == 2) total = 4 + be16(c->buf, 2) * 4;
            else if (type == 6) total = 8 + be32(c->buf, 4);
            if (c->buf.size() < total) return;
            QByteArray payload = c->buf.mid(1, total - 1);
            c->buf.remove(0, total);
            handleMessage(c, type, payload);
        }
    }
}

void VncMirror::sendServerInit(Client* c)
{
    const QByteArray name = QByteArrayLiteral("NavigatorHMI VNC Mirror");
    QByteArray si;
    put16(si, quint16(m_devW));
    put16(si, quint16(m_devH));
    si.append(char(32));  si.append(char(24));
    si.append(char(0));   si.append(char(1));
    put16(si, 255); put16(si, 255); put16(si, 255);
    si.append(char(16)); si.append(char(8)); si.append(char(0));
    si.append(char(0)); si.append(char(0)); si.append(char(0));
    put32(si, quint32(name.size()));
    si.append(name);
    c->socket->write(si);
    c->needsUpdate = true;
    m_window->update();   // 强制渲染一帧（静态画面也要能出帧）
}

void VncMirror::handleMessage(Client* c, quint8 type, const QByteArray& p)
{
    switch (type) {
    case 0:   // SetPixelFormat：忽略
        break;
    case 2:   // SetEncodings：忽略（只发 Raw）
        break;
    case 3: { // FramebufferUpdateRequest：标记待更新 + 按需强制渲染（节流 ≥20ms = 50fps 上限）
        (void)p;
        c->needsUpdate = true;
        if (m_elapsed.elapsed() - m_lastCaptureMs >= 20)
            m_window->update();
        break;
    }
    case 4:   // KeyEvent
        injectKey(be32(p, 3), p.at(0) != 0);
        break;
    case 5: { // PointerEvent
        quint8 mask = quint8(p.at(0));
        injectPointer(int(be16(p, 1)), int(be16(p, 3)), mask);
        break;
    }
    case 6:   // ClientCutText：忽略
        break;
    default:
        c->socket->disconnectFromHost();
        break;
    }
}

// ── 渲染线程：读生产端报告的脏矩形区域全分辨率（绕开全帧 792ms 读回）──
void VncMirror::onAfterRendering()
{
    if (!m_window)
        return;
    bool need = false;
    {
        QMutexLocker lock(&m_clientsMutex);
        for (auto* c : m_clients)
            if (c->needsUpdate) { need = true; break; }
        if (!need)
            return;
        // 节流：≥20ms 读回一次（动画驱动渲染 ~45fps；局部读回不阻塞渲染循环）
        if (m_elapsed.elapsed() - m_lastCaptureMs < 20)
            return;
        // 注意：不清除 needsUpdate —— 客户端在线即持续推帧（心跳/增量），
        // 打破「服务器发得慢 → 客户端 FBU 请求慢 → 更慢」的反馈环。
    }

    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid())
        return;
    QOpenGLFunctions* f = ctx->functions();
    const int w = m_devW, h = m_devH;

    // 首帧：无缓存 → 全帧读回初始化
    if (m_lastFrame.size() != w * h * 4) {
        QByteArray rgba(w * h * 4, Qt::Uninitialized);
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                        reinterpret_cast<GLvoid*>(rgba.data()));
        QMetaObject::invokeMethod(this, [this, rgba, w, h] {
            sendFullFrame(rgbaToBgra(rgba, w, h), w, h);
        }, Qt::QueuedConnection);
        m_lastCaptureMs = m_elapsed.elapsed();
        return;
    }

    // 取生产端报告的脏矩形（QML 层 markDirty；无报告时周期性全帧兜底防漏）
    QVector<QRect> dirty;
    {
        QMutexLocker lock(&m_dirtyMutex);
        dirty = m_dirtyRects;
        m_dirtyRects.clear();
    }
    // 兜底：长时间无脏区报告时强制全帧（防止漏报导致画面冻结；1.5s 周期兼顾实时与开销）
    const qint64 now = m_elapsed.elapsed();
    static qint64 lastFullMs = 0;
    if (dirty.isEmpty() && (lastFullMs == 0 || now - lastFullMs > 1500)) {
        QByteArray rgba(w * h * 4, Qt::Uninitialized);
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                        reinterpret_cast<GLvoid*>(rgba.data()));
        QMetaObject::invokeMethod(this, [this, rgba, w, h] {
            sendFullFrame(rgbaToBgra(rgba, w, h), w, h);
        }, Qt::QueuedConnection);
        lastFullMs = now;
        m_lastCaptureMs = now;
        return;
    }

    if (dirty.isEmpty()) {
        // 无变化：心跳帧维持客户端帧率计数（零带宽）
        QMetaObject::invokeMethod(this, [this] { sendHeartbeat(); }, Qt::QueuedConnection);
        m_lastCaptureMs = now;
        return;
    }

    // 合并脏矩形（相邻合并减少读回次数）+ 面积判断
    std::sort(dirty.begin(), dirty.end(),
              [](const QRect& a, const QRect& b) { return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x(); });
    QVector<QRect> merged;
    for (const QRect& r : dirty) {
        if (!merged.isEmpty()) {
            QRect& last = merged.last();
            // 纵向相邻且 x 重叠 → 合并（拉高到覆盖两矩形）
            if (r.y() <= last.bottom() + 1 && r.x() <= last.right() && r.right() >= last.left()) {
                last = last.united(r);
                continue;
            }
        }
        merged.append(r);
    }
    qint64 dirtyArea = 0;
    for (const QRect& r : merged)
        dirtyArea += qint64(r.width()) * r.height();
    if (dirtyArea > qint64(w) * h * 40 / 100) {
        // 大面积变化（切页等）：分条带读回——每帧只读 1/4 高度条带（~200ms），
        // 4 帧渐进完成全屏，视觉 ~5fps 而非全帧 792ms 的 1fps
        const int bandH = h / 4;
        static int band = 0;
        const QRect bandRect(0, band * bandH, w, qMin(bandH, h - band * bandH));
        band = (band + 1) % 4;
        QByteArray bgra;
        if (readRegion(f, bandRect, w, h, bgra)) {
            QVector<QRect> br{ bandRect };
            QVector<QByteArray> bd{ bgra };
            QMetaObject::invokeMethod(this, [this, br, bd, w, h] {
                sendRegions(br, bd, w, h);
            }, Qt::QueuedConnection);
        }
        m_lastCaptureMs = now;
        return;
    }

    // 局部读回：只读脏矩形区域全分辨率（小块 ~几 ms，绕开全帧 792ms）
    QVector<QRect> outRects;
    QVector<QByteArray> outDatas;
    for (const QRect& r : merged) {
        QByteArray bgra;
        if (readRegion(f, r, w, h, bgra)) {
            outRects.append(r);
            outDatas.append(bgra);
        }
    }
    if (outRects.isEmpty()) {
        QMetaObject::invokeMethod(this, [this] { sendHeartbeat(); }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, [this, outRects, outDatas, w, h] {
            sendRegions(outRects, outDatas, w, h);
        }, Qt::QueuedConnection);
    }
    m_lastCaptureMs = now;
}

void VncMirror::markDirty(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    QMutexLocker lock(&m_dirtyMutex);
    m_dirtyRects.append(QRect(x, y, w, h));
    // 上限保护：防恶意/错误报告无限增长
    if (m_dirtyRects.size() > 256)
        m_dirtyRects.clear();
}

bool VncMirror::readRegion(QOpenGLFunctions* f, const QRect& r, int w, int h, QByteArray& bgra)
{
    if (r.isEmpty())
        return false;
    QByteArray rgba(r.width() * r.height() * 4, Qt::Uninitialized);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // 坐标翻转：GL 帧缓冲左下原点 (y 向上)，屏幕/VNC 左上原点 (y 向下)。
    // 屏幕区域 (x, y, w, h) → GL 区域 (x, h - y - h, w, h)，否则读到上下错位区域
    f->glReadPixels(r.x(), h - r.y() - r.height(), r.width(), r.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, reinterpret_cast<GLvoid*>(rgba.data()));
    bgra = rgbaToBgra(rgba, r.width(), r.height());
    return true;
}

// ── 发送（GUI 线程）──

void VncMirror::sendRegions(const QVector<QRect>& rects, const QVector<QByteArray>& datas, int w, int h)
{
    if (rects.size() != datas.size() || rects.isEmpty())
        return;
    QMutexLocker lock(&m_clientsMutex);
    QByteArray msg;
    msg.append(char(0)); msg.append(char(0));
    put16(msg, quint16(rects.size()));
    for (int i = 0; i < rects.size(); ++i) {
        const QRect& r = rects[i];
        put16(msg, quint16(r.x())); put16(msg, quint16(r.y()));
        put16(msg, quint16(r.width())); put16(msg, quint16(r.height()));
        put32(msg, 0);   // Raw
        msg.append(datas[i]);
        // 同步 m_lastFrame 对应区域（供后续退化全帧比较）
        if (m_lastFrame.size() == w * h * 4) {
            for (int row = 0; row < r.height(); ++row) {
                memcpy(m_lastFrame.data() + ((r.y() + row) * w + r.x()) * 4,
                       datas[i].constData() + row * r.width() * 4,
                       r.width() * 4);
            }
        }
    }
    for (auto* c : m_clients)
        c->socket->write(msg);
}

void VncMirror::sendFullFrame(const QByteArray& bgra, int w, int h)
{
    if (bgra.size() < w * h * 4)
        return;
    QMutexLocker lock(&m_clientsMutex);
    QByteArray msg;
    msg.append(char(0)); msg.append(char(0));
    put16(msg, 1);
    put16(msg, 0); put16(msg, 0);
    put16(msg, quint16(w)); put16(msg, quint16(h));
    put32(msg, 0);
    msg.append(bgra);
    for (auto* c : m_clients)
        c->socket->write(msg);
    m_lastFrame = bgra;
    m_lastFrameW = w;
    m_lastFrameH = h;
}

void VncMirror::sendHeartbeat()
{
    QMutexLocker lock(&m_clientsMutex);
    QByteArray msg;
    msg.append(char(0)); msg.append(char(0));
    put16(msg, 0);   // 0 矩形：无变化（维持客户端帧率计数，零带宽）
    for (auto* c : m_clients)
        c->socket->write(msg);
}

QByteArray VncMirror::rgbaToBgra(const QByteArray& rgba, int w, int h)
{
    QByteArray bgra(w * h * 4, Qt::Uninitialized);
    const int stride = w * 4;
    for (int y = 0; y < h; ++y) {
        const char* src = rgba.constData() + (h - 1 - y) * stride;   // 垂直翻转（GL 左下 → 帧格式左上）
        char* dst = bgra.data() + y * stride;
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = 0xff;
        }
    }
    return bgra;
}

void VncMirror::injectPointer(int x, int y, quint8 mask)
{
    if (!m_window)
        return;
    x = qBound(0, x, m_devW - 1);
    y = qBound(0, y, m_devH - 1);
    Qt::MouseButtons cur = Qt::NoButton;
    if (mask & 0x01) cur |= Qt::LeftButton;
    if (mask & 0x02) cur |= Qt::MiddleButton;
    if (mask & 0x04) cur |= Qt::RightButton;
    const QPointF pos(x, y);

    if (cur == m_lastButtons) {
        if (cur != Qt::NoButton)
            QWindowSystemInterface::handleMouseEvent(m_window, pos, pos, cur,
                                                     Qt::NoButton, QEvent::MouseMove);
    } else {
        static const struct { quint8 bit; Qt::MouseButton btn; } kTable[3] = {
            { 0x01, Qt::LeftButton }, { 0x02, Qt::MiddleButton }, { 0x04, Qt::RightButton } };
        for (const auto& t : kTable) {
            bool was = m_lastButtons & t.bit;
            bool now = cur & t.btn;
            if (was && !now)
                QWindowSystemInterface::handleMouseEvent(m_window, pos, pos, cur,
                                                         t.btn, QEvent::MouseButtonRelease);
            else if (!was && now)
                QWindowSystemInterface::handleMouseEvent(m_window, pos, pos, cur,
                                                         t.btn, QEvent::MouseButtonPress);
        }
    }
    m_lastButtons = cur;
}

void VncMirror::injectKey(quint32 keysym, bool down)
{
    if (!m_window)
        return;
    Qt::Key key = keysymToQt(keysym);
    if (key == Qt::Key_unknown)
        return;
    QString text;
    if (keysym < 0x100)
        text = QChar(keysym);
    QWindowSystemInterface::handleKeyEvent(m_window,
                                           down ? QEvent::KeyPress : QEvent::KeyRelease,
                                           key, Qt::NoModifier, text);
}

} // namespace navihmi

#endif // HAVE_QT_QML
