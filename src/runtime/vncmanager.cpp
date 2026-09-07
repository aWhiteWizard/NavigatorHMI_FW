/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncmanager.cpp
 * @Description: VNC 管理层实现（W-D F19 三层职责拆分）——监听/会话生命周期/连接状态/帧消费调度。
 *               逻辑迁移自原 vncmirror.cpp（VncMirror 外层全部，行为零改动）。
 *               帧捕获：QQuickWindow::afterRendering（渲染线程）→ glReadPixels
 *               读默认帧缓冲（零额外渲染，不打扰 eglfs 实时显示）。
 */
#if defined(HAVE_QT_QML)

#include "runtime/vncmanager.h"
#include "runtime/vncserver.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace navihmi {

VncManager::VncManager(QQuickWindow* window, QObject* parent)
    : QObject(parent), m_window(window)
{
    m_elapsed.start();
}

VncManager::~VncManager()
{
    stop();
}

void VncManager::setDeviceSize(int w, int h)
{
    if (w > 0 && h > 0) { m_cfg.devW = w; m_cfg.devH = h; }
}

bool VncManager::start(quint16 port)
{
    if (m_server)
        return true;
    if (!m_window) {
        qWarning().noquote() << "VncManager: 无 QQuickWindow，无法启动";
        return false;
    }
    m_cfg.port = port;
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning().noquote() << "VncManager: 监听失败" << port << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QTcpServer::newConnection, this, &VncManager::onNewConnection);
    // 渲染线程读帧：frameSwapped（present 之后）读默认帧缓冲 = 当前实际显示内容
    connect(m_window, &QQuickWindow::frameSwapped, this, &VncManager::onAfterRendering,
            Qt::DirectConnection);
    qInfo().noquote() << "VncManager: VNC 服务已启动 :" << port
                      << "屏幕" << m_cfg.devW << "x" << m_cfg.devH;
    return true;
}

void VncManager::stop()
{
    if (!m_server)
        return;
    disconnect(m_window, &QQuickWindow::frameSwapped, this, &VncManager::onAfterRendering);
    m_server->close();
    delete m_server;
    m_server = nullptr;
    {
        QMutexLocker lock(&m_serversMutex);
        qDeleteAll(m_servers);   // VncServer 析构连带子 socket 释放
        m_servers.clear();
    }
    qInfo().noquote() << "VncManager: VNC 服务已停止";
}

void VncManager::onNewConnection()
{
    while (auto* sock = m_server->nextPendingConnection()) {
        // 服务层：会话创建即发版本握手（尺寸快照 = 当前设备物理尺寸——N+24 恒定）
        auto* server = new VncServer(sock, m_window, m_cfg.devW, m_cfg.devH, this);
        connect(server, &VncServer::framebufferRequested, this, &VncManager::onFramebufferRequested);
        connect(sock, &QTcpSocket::disconnected, this, &VncManager::onClientDisconnected);
        {
            QMutexLocker lock(&m_serversMutex);
            bool wasEmpty = m_servers.isEmpty();
            m_servers.append(server);
            // 首个客户端连接 → 通知 QML 启动无限动画驱动持续渲染（30fps；动画驱动渲染循环，
            // 不同于 Timer 设属性——动画让渲染循环连续跑帧，frameSwapped 每帧读回）
            if (wasEmpty && m_window) {
                bool ok = m_window->setProperty("vncMirrorActive", true);
                qInfo().noquote() << "VncManager: 首个客户端连接, setProperty vncMirrorActive ok=" << ok
                                  << "clients=" << m_servers.size();
            }
        }
    }
}

void VncManager::onClientDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    VncServer* removed = nullptr;
    {
        QMutexLocker lock(&m_serversMutex);
        for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
            if ((*it)->socket() == sock) {
                removed = *it;
                m_servers.erase(it);
                break;
            }
        }
        // 最后一个客户端断开 → 通知 QML 停止动画驱动（恢复零开销）
        if (m_servers.isEmpty() && m_window)
            m_window->setProperty("vncMirrorActive", false);
    }
    if (removed)
        removed->deleteLater();   // 延迟删（连带子 socket）——disconnected 槽返回后安全
}

void VncManager::onFramebufferRequested()
{
    if (!m_window)
        return;
    // 客户端 FBU / ServerInit 请求 → 按节流强制渲染（≥20ms = 50fps 上限；ServerInit 首帧
    // m_lastCaptureMs 初值 -1000 保证必然触发）
    if (m_elapsed.elapsed() - m_lastCaptureMs >= 20)
        m_window->update();
}

// ── 渲染线程：读生产端报告的脏矩形区域全分辨率（绕开全帧 792ms 读回）──
void VncManager::onAfterRendering()
{
    if (!m_window)
        return;
    bool need = false;
    {
        QMutexLocker lock(&m_serversMutex);
        for (auto* s : m_servers)
            if (s->wantsUpdate()) { need = true; break; }
        if (!need)
            return;
        // 节流：≥20ms 读回一次（动画驱动渲染 ~45fps；局部读回不阻塞渲染循环）
        if (m_elapsed.elapsed() - m_lastCaptureMs < 20)
            return;
        // 注意：不清除 wantsUpdate —— 客户端在线即持续推帧（心跳/增量），
        // 打破「服务器发得慢 → 客户端 FBU 请求慢 → 更慢」的反馈环。
    }

    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid())
        return;
    QOpenGLFunctions* f = ctx->functions();
    const int w = m_cfg.devW, h = m_cfg.devH;

    // 首帧：无缓存 → 全帧读回初始化
    // N+22 修复（2026-08-30）：用原子标志判定（不再读 QByteArray size()——渲染线程读
    // QByteArray 与 GUI 线程赋值并发 → 隐式共享引用计数 double free）
    if (!m_firstFrameSent.loadRelaxed()) {
        QByteArray rgba(w * h * 4, Qt::Uninitialized);
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                        reinterpret_cast<GLvoid*>(rgba.data()));
        const QByteArray bgra = rgbaToBgra(rgba, w, h);
        QMetaObject::invokeMethod(this, [this, bgra] {
            QMutexLocker lock(&m_serversMutex);
            for (auto* s : m_servers)
                s->sendFullFrame(bgra);
            m_firstFrameSent.storeRelaxed(true);
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
    // N+23 双保险：消费端再钳制一次（setDeviceSize 换工程尺寸后，队列里可能有旧尺寸
    // 残留矩形；钳制保证后续 glReadPixels 读回区域 / 面积判断 / 增量发送全部落在帧内）
    const QRect frame(0, 0, w, h);
    for (auto it = dirty.begin(); it != dirty.end();) {
        const QRect c = it->intersected(frame);
        if (c.isEmpty()) it = dirty.erase(it);
        else { *it = c; ++it; }
    }
    // 兜底：长时间无脏区报告时强制全帧（防止漏报导致画面冻结；1.5s 周期兼顾实时与开销）
    const qint64 now = m_elapsed.elapsed();
    static qint64 lastFullMs = 0;
    if (dirty.isEmpty() && (lastFullMs == 0 || now - lastFullMs > m_cfg.fullFrameFallbackMs)) {
        QByteArray rgba(w * h * 4, Qt::Uninitialized);
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                        reinterpret_cast<GLvoid*>(rgba.data()));
        const QByteArray bgra = rgbaToBgra(rgba, w, h);
        QMetaObject::invokeMethod(this, [this, bgra] {
            QMutexLocker lock(&m_serversMutex);
            for (auto* s : m_servers)
                s->sendFullFrame(bgra);
        }, Qt::QueuedConnection);
        lastFullMs = now;
        m_lastCaptureMs = now;
        return;
    }

    if (dirty.isEmpty()) {
        // 无变化：心跳帧维持客户端帧率计数（零带宽）
        QMetaObject::invokeMethod(this, [this] {
            QMutexLocker lock(&m_serversMutex);
            for (auto* s : m_servers)
                s->sendHeartbeat();
        }, Qt::QueuedConnection);
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
    if (dirtyArea > qint64(w) * h * m_cfg.dirtyAreaFullFrameThresholdPercent / 100) {
        // 大面积变化（切页等）：分条带读回——每帧只读 1/4 高度条带（~200ms），
        // fullFrameBandCount 帧渐进完成全屏，视觉 ~5fps 而非全帧 792ms 的 1fps
        const int bandH = h / m_cfg.fullFrameBandCount;
        static int band = 0;
        const QRect bandRect(0, band * bandH, w, qMin(bandH, h - band * bandH));
        band = (band + 1) % m_cfg.fullFrameBandCount;
        QByteArray bgra;
        if (readRegion(f, bandRect, w, h, bgra)) {
            QVector<QRect> br{ bandRect };
            QVector<QByteArray> bd{ bgra };
            QMetaObject::invokeMethod(this, [this, br, bd] {
                QMutexLocker lock(&m_serversMutex);
                for (auto* s : m_servers)
                    s->sendRegions(br, bd);
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
        QMetaObject::invokeMethod(this, [this] {
            QMutexLocker lock(&m_serversMutex);
            for (auto* s : m_servers)
                s->sendHeartbeat();
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, [this, outRects, outDatas] {
            QMutexLocker lock(&m_serversMutex);
            for (auto* s : m_servers)
                s->sendRegions(outRects, outDatas);
        }, Qt::QueuedConnection);
    }
    m_lastCaptureMs = now;
}

void VncManager::markDirty(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    // N+23 修复（2026-08-30）：越界钳制——QML 组件坐标可能超出 VNC 设备尺寸
    // （工程 deviceWidth/Height 与主壳场景不一致、滚动内容坐标等），原无钳制直接消费，
    // 越界矩形经 sendRegions 旧 memcpy 越界写 → 堆损坏 → double free 崩溃。
    // 钳到 [0, devW]×[0, devH]，空矩形丢弃（出界组件的可见部分仍正确刷新）。
    const QRect clipped = QRect(x, y, w, h).intersected(QRect(0, 0, m_cfg.devW, m_cfg.devH));
    if (clipped.isEmpty())
        return;
    QMutexLocker lock(&m_dirtyMutex);
    m_dirtyRects.append(clipped);
    // 上限保护：防恶意/错误报告无限增长
    if (m_dirtyRects.size() > m_cfg.maxDirtyRects)
        m_dirtyRects.clear();
}

bool VncManager::readRegion(QOpenGLFunctions* f, const QRect& r, int w, int h, QByteArray& bgra)
{
    if (r.isEmpty())
        return false;
    (void)w;   // 坐标翻转只需 h（读回宽度由 r 携带）——签名保留 w 与调用点一致
    QByteArray rgba(r.width() * r.height() * 4, Qt::Uninitialized);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // 坐标翻转：GL 帧缓冲左下原点 (y 向上)，屏幕/VNC 左上原点 (y 向下)。
    // 屏幕区域 (x, y, w, h) → GL 区域 (x, h - y - h, w, h)，否则读到上下错位区域
    f->glReadPixels(r.x(), h - r.y() - r.height(), r.width(), r.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, reinterpret_cast<GLvoid*>(rgba.data()));
    bgra = rgbaToBgra(rgba, r.width(), r.height());
    return true;
}

QByteArray VncManager::rgbaToBgra(const QByteArray& rgba, int w, int h)
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

} // namespace navihmi

#endif // HAVE_QT_QML
