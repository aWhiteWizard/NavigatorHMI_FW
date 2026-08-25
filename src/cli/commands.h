/*
 * @FilePath: \NavigatorHMI_FW\src\cli\commands.h
 * @Description: FW 命令服务（I-1 SSH CLI）——命令执行，与触屏操作共享同一服务实例
 *               命令集（cli-console.md 设计落地）：
 *                 screen list/current/switch <name>
 *                 tag list/read <name>/write <name> <value>
 *                 alarm list/ack <id>/history
 *                 system info/reboot confirm
 *                 config reload / render refresh / help
 *               权限分级：root（uid==0）全权；非 root 只读（写/控制命令拒绝）
 */
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace navihmi {

class DataManager;
class RuntimeBus;
class AlarmEngine;
class DeviceInfo;
class DataLogger;

class CommandService : public QObject
{
    Q_OBJECT
public:
    explicit CommandService(QObject* parent = nullptr);

    void setDataManager(DataManager* dm);
    void setRuntimeBus(RuntimeBus* bus);
    void setAlarmEngine(AlarmEngine* ae);
    void setDeviceInfo(DeviceInfo* di);
    void setDataLogger(DataLogger* dl);

    /// 执行一行命令，返回输出文本（错误以 "ERROR: " 前缀）
    /// clientUid：客户端进程 UID（SSH CLI 场景 = SSH 登录用户 UID；Linux SO_PEERCRED 由 CliServer 读取）
    QString execute(const QString& line, int clientUid);

    /// 权限：管理员（客户端 uid==0 root 全权）；Windows 仿真环境恒管理员
    static bool isAdmin(int clientUid);

private:
    QString cmdScreen(const QStringList& args);
    QString cmdTag(const QStringList& args);
    QString cmdAlarm(const QStringList& args);
    QString cmdSystem(const QStringList& args);
    QString cmdConfig(const QStringList& args);
    QString cmdRender(const QStringList& args);
    QString helpText() const;

    DataManager* m_dm = nullptr;
    RuntimeBus* m_bus = nullptr;
    AlarmEngine* m_ae = nullptr;
    DeviceInfo* m_di = nullptr;
    DataLogger* m_dl = nullptr;
    int m_clientUid = -1;   // 当前连接客户端 UID（-1=未知，按非管理员处理）
};

} // namespace navihmi
