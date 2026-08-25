/*
 * @FilePath: \NavigatorHMI_FW\src\cli\cliprotocol.h
 * @Description: SSH CLI 协议常量——FW 主程序 CliServer 与独立 navihmi-cli 工具共享
 * I-1 (2026-08-25): 板端 SSH CLI——独立 CLI 工具 + QLocalServer 本地 socket IPC
 *                   命令与触屏共享同一服务实例（CommandService），无特权路径差异
 */
#pragma once

#include <QString>

namespace navihmi {

/// 本地 socket 路径：Linux /tmp（SSH 场景 root 可访问）；Windows 命名管道名
inline QString cliSocketPath()
{
#ifdef Q_OS_WIN
    return QStringLiteral("navihmi-cli");
#else
    return QStringLiteral("/tmp/navihmi-cli.sock");
#endif
}

} // namespace navihmi
