/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\rtsp_probe.cpp
 * @Description: 网络视频源可达性预探测（AC-2 2026-10-24）——实现见头文件背景说明。
 *   纯异步：QTcpSocket::connectToHost + QTimer 超时；无阻塞调用、无手写线程。
 */
#include "runtime/rtsp_probe.h"

#include <QDebug>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace navihmi {

RtspProbe::RtspProbe(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    // 超时：取消并报 timeout（socket 连接中的竞态由 m_url 守卫 + cleanup 统一处理）
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (m_url.isEmpty())
            return;
        finish(false, QStringLiteral("timeout"));
    });
}

RtspProbe::~RtspProbe()
{
    cancel();   // 复审 ⚪：析构走 cancel()，保证 m_url 一并清空（防止迟到信号被当作新 URL 的结果）
}

void RtspProbe::cancel()
{
    if (m_url.isEmpty() && !m_socket && !m_timer->isActive())
        return;
    m_url.clear();
    if (m_timer->isActive())
        m_timer->stop();
    cleanup();
}

void RtspProbe::cleanup()
{
    if (m_socket) {
        // 复审 🟡：先断信号再 abort/deleteLater——否则"旧 socket 的迟到结果"可能被当作新 URL 的结果
        disconnect(m_socket, nullptr, this, nullptr);
        m_socket->abort();          // 断开并作废未完成的连接
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_timer && m_timer->isActive())
        m_timer->stop();
}

bool RtspProbe::parseEndpoint(const QString& url, QString& host, quint16& port)
{
    const QUrl u(url);
    if (!u.isValid())
        return false;
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("rtsp") && scheme != QLatin1String("rtspu")
        && scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return false;
    host = u.host();
    if (host.isEmpty())
        return false;
    int p = u.port(-1);
    if (p <= 0) {
        if (scheme == QLatin1String("rtsp") || scheme == QLatin1String("rtspu"))
            p = 554;
        else if (scheme == QLatin1String("https"))
            p = 443;
        else
            p = 80;
    }
    if (p <= 0 || p > 65535)
        return false;
    port = static_cast<quint16>(p);
    return true;
}

void RtspProbe::probe(const QString& url, int timeoutMs)
{
    // 幂等：同一 URL 探测中 ⇒ 忽略（避免切换动画/重复触发打出多份 socket）
    if (!m_url.isEmpty() && m_url == url) {
        qInfo().noquote() << "RtspProbe: 重复探测忽略" << url;
        return;
    }
    // 换 URL ⇒ 取消上一次（旧请求不再回信号）
    cancel();

    QString host;
    quint16 port = 0;
    if (url.isEmpty() || !parseEndpoint(url, host, port)) {
        // 非网络 URL / 无法解析：不支持 = 本次探测失败（本地文件不走本类，调用方自行处理）
        qWarning().noquote() << "RtspProbe: 非法或非网络 URL" << url;
        emit probeFinished(url, false, QStringLiteral("unsupported"));
        return;
    }

    m_url = url;
    m_socket = new QTcpSocket(this);

    // 连接成功 ⇒ ok（TCP 可达；能否出流由 QML 看门狗判定）
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        finish(true, QStringLiteral("ok"));
    });
    // 连接失败 ⇒ 细分原因（拒绝/不可解析/其它）
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError err) {
        if (m_url.isEmpty())
            return;   // 已 cancel/已结束：忽略迟到错误
        QString reason = QStringLiteral("error");
        switch (err) {
        case QAbstractSocket::ConnectionRefusedError:
            reason = QStringLiteral("refused");     // 端口无服务（最常见：RTSP 服务未启动）
            break;
        case QAbstractSocket::HostNotFoundError:
            reason = QStringLiteral("unresolved");
            break;
        case QAbstractSocket::SocketTimeoutError:
            reason = QStringLiteral("timeout");
            break;
        default:
            reason = QStringLiteral("error");
            break;
        }
        finish(false, reason);
    });

    qInfo().noquote() << "RtspProbe: 开始探测" << host << port << "timeout=" << timeoutMs << "ms";
    m_timer->start(timeoutMs > 0 ? timeoutMs : 2000);
    m_socket->connectToHost(host, port);
}

void RtspProbe::finish(bool ok, const QString& reason)
{
    const QString url = m_url;
    m_url.clear();
    if (m_timer->isActive())
        m_timer->stop();
    cleanup();
    if (url.isEmpty())
        return;
    if (ok)
        qInfo().noquote() << "RtspProbe: 可达" << url;
    else
        qWarning().noquote() << "RtspProbe: 不可达" << url << "reason=" << reason;
    emit probeFinished(url, ok, reason);
}

} // namespace navihmi
