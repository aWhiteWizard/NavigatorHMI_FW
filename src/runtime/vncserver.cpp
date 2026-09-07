/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncserver.cpp
 * @Description: VNC 服务层实现（W-D F19 三层职责拆分）——单会话 RFB 3.3 协议/编码/注入。
 *               逻辑迁移自原 vncmirror.cpp（VncMirror::Client 相关全部，行为零改动）。
 */
#if defined(HAVE_QT_QML)

#include "runtime/vncserver.h"

#include <QTcpSocket>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QDebug>
// SDK Qt 6.4.3 的 qpa 私有头在版本化路径下（<prefix>/include/QtGui/6.4.3/QtGui/qpa/）
#include <QtGui/6.4.3/QtGui/qpa/qwindowsysteminterface.h>

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

VncServer::VncServer(QTcpSocket* socket, QQuickWindow* window, int devW, int devH, QObject* parent)
    : QObject(parent), m_socket(socket), m_window(window), m_devW(devW), m_devH(devH)
{
    // 会话创建即开始握手：服务器先发版本（原 VncMirror::onNewConnection）
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &VncServer::onReadyRead);
    m_socket->write("RFB 003.003\n", 12);
}

VncServer::~VncServer()
{
    // socket 生命周期由 VncManager 断开清理（deleteLater）；析构兜底释放引用
}

void VncServer::onReadyRead()
{
    m_buf.append(m_socket->readAll());
    processClient();
}

void VncServer::processClient()
{
    for (;;) {
        if (m_state == ClientState::WaitVersion) {
            if (m_buf.size() < 12) return;
            m_buf.remove(0, 12);
            QByteArray sec; put32(sec, 1);
            m_socket->write(sec);
            m_state = ClientState::WaitClientInit;
        } else if (m_state == ClientState::WaitClientInit) {
            if (m_buf.size() < 1) return;
            m_buf.remove(0, 1);
            sendServerInit();
            m_state = ClientState::Ready;
        } else { // Ready
            if (m_buf.isEmpty()) return;
            quint8 type = quint8(m_buf.at(0));
            int need = 0;
            switch (type) {
            case 0: need = 20; break;
            case 2: need = 4; break;
            case 3: need = 10; break;
            case 4: need = 8; break;
            case 5: need = 6; break;
            case 6: need = 8; break;
            default:
                m_socket->disconnectFromHost();
                return;
            }
            if (m_buf.size() < need) return;
            int total = need;
            if (type == 2) total = 4 + be16(m_buf, 2) * 4;
            else if (type == 6) total = 8 + be32(m_buf, 4);
            if (m_buf.size() < total) return;
            QByteArray payload = m_buf.mid(1, total - 1);
            m_buf.remove(0, total);
            handleMessage(type, payload);
        }
    }
}

void VncServer::sendServerInit()
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
    m_socket->write(si);
    m_needsUpdate = true;
    emit framebufferRequested();   // 强制渲染一帧（静态画面也要能出帧）——Manager 按节流 update
}

void VncServer::handleMessage(quint8 type, const QByteArray& p)
{
    switch (type) {
    case 0:   // SetPixelFormat：忽略
        break;
    case 2:   // SetEncodings：忽略（只发 Raw）
        break;
    case 3: { // FramebufferUpdateRequest：标记待更新 + 通知 Manager 按节流强制渲染（≥20ms = 50fps 上限）
        (void)p;
        m_needsUpdate = true;
        emit framebufferRequested();
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
        m_socket->disconnectFromHost();
        break;
    }
}

// ── 发送（GUI 线程，Manager 帧消费后调用）──

void VncServer::sendRegions(const QVector<QRect>& rects, const QVector<QByteArray>& datas)
{
    if (rects.size() != datas.size() || rects.isEmpty())
        return;
    // N+23：原 m_lastFrame 同步已删除，帧尺寸不再需要（矩形已在上游钳制）
    QByteArray msg;
    msg.append(char(0)); msg.append(char(0));
    put16(msg, quint16(rects.size()));
    for (int i = 0; i < rects.size(); ++i) {
        const QRect& r = rects[i];
        put16(msg, quint16(r.x())); put16(msg, quint16(r.y()));
        put16(msg, quint16(r.width())); put16(msg, quint16(r.height()));
        put32(msg, 0);   // Raw
        msg.append(datas[i]);
        // N+23 修复（2026-08-30）：原「同步 m_lastFrame 对应区域」memcpy 已删除——
        // m_lastFrame 缓存从不被读取（死代码），且无边界钳制的 memcpy 遇出界脏矩形
        // （QML 组件坐标超出 VNC 设备尺寸）→ 堆越界写 → 后续 free 报 double free 崩溃。
        // 出界防护在 markDirty / 帧消费统一钳制。
    }
    m_socket->write(msg);
}

void VncServer::sendFullFrame(const QByteArray& bgra)
{
    QByteArray msg;
    msg.append(char(0)); msg.append(char(0));
    put16(msg, 1);
    put16(msg, 0); put16(msg, 0);
    put16(msg, quint16(m_devW)); put16(msg, quint16(m_devH));
    put32(msg, 0);
    msg.append(bgra);
    m_socket->write(msg);
}

void VncServer::sendHeartbeat()
{
    QByteArray msg;
    msg.append(char(0)); msg.append(char(0));
    put16(msg, 0);   // 0 矩形：无变化（维持客户端帧率计数，零带宽）
    m_socket->write(msg);
}

// ── 输入注入（GUI 线程：协议消息 → Qt 窗口事件）──

void VncServer::injectPointer(int x, int y, quint8 mask)
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

void VncServer::injectKey(quint32 keysym, bool down)
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
