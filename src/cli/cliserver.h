/*
 * @FilePath: \NavigatorHMI_FW\src\cli\cliserver.h
 * @Description: CLI 本地 socket 服务（I-1 SSH CLI）——QLocalServer 监听 /tmp/navihmi-cli.sock
 *               SSH 登录后运行 navihmi-cli 工具连接执行命令；命令在主线程执行（与 GUI 同线程，无竞态）
 */
#pragma once

#include <QObject>
#include <QLocalServer>
#include <QString>

namespace navihmi {

class CommandService;

class CliServer : public QObject
{
    Q_OBJECT
public:
    explicit CliServer(CommandService* svc, QObject* parent = nullptr);

    /// 开始监听；返回是否成功（端口占用时先清理旧 socket 文件）
    bool start();
    /// 监听路径（cliprotocol.h）
    QString socketPath() const;

private slots:
    void onNewConnection();

private:
    CommandService* m_svc = nullptr;
    QLocalServer m_server;
};

} // namespace navihmi
