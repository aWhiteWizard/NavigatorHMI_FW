/*
 * @FilePath: \NavigatorHMI_FW\tools\navihmi-cli\main.cpp
 * @Description: navihmi-cli——SSH 登录后执行的 FW 命令客户端（I-1）
 *               用法: navihmi-cli screen list / tag read R01_status / help ...
 *               连接 FW 主程序 QLocalServer（/tmp/navihmi-cli.sock）发一行命令收结果
 */
#include <QCoreApplication>
#include <QLocalSocket>
#include <QTextStream>
#include "cli/cliprotocol.h"

static void printUsage()
{
    QTextStream out(stdout);
    out << "navihmi-cli - NavigatorHMI FW 命令客户端\n"
        << "用法: navihmi-cli <命令> [参数...]\n"
        << "示例: navihmi-cli screen list\n"
        << "      navihmi-cli tag read R01_status\n"
        << "      navihmi-cli help\n";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        printUsage();
        return 1;
    }
    const QString line = args.mid(1).join(QLatin1Char(' '));

    QLocalSocket sock;
    sock.connectToServer(navihmi::cliSocketPath());
    if (!sock.waitForConnected(2000)) {
        out << "ERROR: 无法连接 FW 命令服务（FW 未运行？）\n";
        return 2;
    }
    sock.write((line + QLatin1Char('\n')).toUtf8());
    sock.flush();
    if (!sock.waitForReadyRead(5000)) {
        out << "ERROR: 等待 FW 响应超时\n";
        return 3;
    }
    const QByteArray resp = sock.readAll();
    out << QString::fromUtf8(resp);
    if (!resp.endsWith('\n')) out << "\n";
    out.flush();
    return 0;
}
