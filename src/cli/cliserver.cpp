/*
 * @FilePath: \NavigatorHMI_FW\src\cli\cliserver.cpp
 * @Description: CLI 本地 socket 服务实现——收到一行命令 → CommandService::execute → 回写结果
 *               权限：连接时经 SO_PEERCRED 读对端（SSH 客户端进程）UID 传给命令服务（root 全权/非 root 只读）
 *               按 '\n' 边界读行（支持单连接多行/分段到达）
 */
#include "cli/cliserver.h"
#include "cli/commands.h"
#include "cli/cliprotocol.h"

#include <QLocalSocket>
#include <QDebug>

#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace navihmi {

CliServer::CliServer(CommandService* svc, QObject* parent)
    : QObject(parent)
    , m_svc(svc)
{
    QObject::connect(&m_server, &QLocalServer::newConnection, this, &CliServer::onNewConnection);
}

QString CliServer::socketPath() const
{
    return navihmi::cliSocketPath();
}

bool CliServer::start()
{
#ifndef Q_OS_WIN
    // Linux: 清理残留 socket 文件（FW 异常退出后可能遗留）
    QLocalServer::removeServer(socketPath());
#endif
    // 允许所有本地用户连接 socket（权限分级在命令服务按对端 UID 判定——root 全权/非 root 只读；
    // 否则 umask 077 下 socket 0600 只有 root 能连，非 root 连不上）
    m_server.setSocketOptions(QLocalServer::WorldAccessOption);
    if (!m_server.listen(socketPath())) {
        qWarning().noquote() << "CLI 服务: 监听失败" << m_server.errorString() << socketPath();
        return false;
    }
    qInfo().noquote() << "CLI 服务: 监听" << socketPath();
    return true;
}

/// 对端进程 UID（Linux SO_PEERCRED；Windows 仿真恒管理员=0；未知 -1）
static int peerUidOf(QLocalSocket* sock)
{
#ifdef Q_OS_LINUX
    struct ucred cr;
    socklen_t len = sizeof(cr);
    if (getsockopt(sock->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &cr, &len) == 0)
        return static_cast<int>(cr.uid);
    return -1;
#else
    Q_UNUSED(sock);
    return 0;
#endif
}

void CliServer::onNewConnection()
{
    while (QLocalSocket* sock = m_server.nextPendingConnection()) {
        QByteArray* buf = new QByteArray;   // 行缓冲（按 '\n' 切行，支持分段/多行）
        QObject::connect(sock, &QLocalSocket::readyRead, this, [this, sock, buf]() {
            buf->append(sock->readAll());
            int nl;
            while ((nl = buf->indexOf('\n')) >= 0) {
                const QString line = QString::fromUtf8(buf->left(nl)).trimmed();
                buf->remove(0, nl + 1);
                if (line.isEmpty()) continue;
                const int uid = peerUidOf(sock);
                const QString result = m_svc ? m_svc->execute(line, uid)
                                             : QStringLiteral("ERROR: 命令服务未就绪");
                sock->write((result + QLatin1Char('\n')).toUtf8());
                sock->flush();
                sock->disconnectFromServer();
                break;   // 单连接执行单条命令后断开
            }
        });
        QObject::connect(sock, &QLocalSocket::disconnected, sock, [sock, buf]() {
            delete buf;
            sock->deleteLater();
        });
    }
}

} // namespace navihmi
