/*
 * @FilePath: \NavigatorHMI_FW\src\cli\commands.cpp
 * @Description: FW 命令服务实现（I-1 SSH CLI）——命令解析 + 执行，输出文本
 *               数据源：RuntimeBus::project()（screens/tags/alarms）+ DataManager/AlarmEngine/
 *                      DeviceInfo/DataLogger（同一服务实例，与触屏共享）
 */
#include "cli/commands.h"
#include "runtime/datamanager.h"
#include "runtime/runtimebus.h"
#include "runtime/alarmengine.h"
#include "runtime/deviceinfo.h"
#include "runtime/datalogger.h"
#include "runtime/vncmirror.h"   // K-9：vnc 启停命令
#include "runtime/devicemeta.h"  // K-9：设备身份推导单点
#include "runtime/fwconfig.h"    // K-9 评论3：VNC 端口配置单点

#include <QDateTime>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <cstdlib>
#endif

namespace navihmi {

CommandService::CommandService(QObject* parent)
    : QObject(parent)
{
}

void CommandService::setDataManager(DataManager* dm) { m_dm = dm; }
void CommandService::setRuntimeBus(RuntimeBus* bus) { m_bus = bus; }
void CommandService::setAlarmEngine(AlarmEngine* ae) { m_ae = ae; }
void CommandService::setDeviceInfo(DeviceInfo* di) { m_di = di; }
void CommandService::setDataLogger(DataLogger* dl) { m_dl = dl; }
void CommandService::setVncMirror(VncMirror* vm) { m_vm = vm; }   // K-9

bool CommandService::isAdmin(int clientUid)
{
#ifdef Q_OS_WIN
    Q_UNUSED(clientUid);
    return true;   // Windows 仿真环境恒管理员
#else
    // 权限基于客户端（SSH 登录用户）UID——由 CliServer 经 SO_PEERCRED 读取后传入；
    // -1 = 未知（不应出现），按非管理员处理
    return clientUid == 0;
#endif
}

QString CommandService::execute(const QString& line, int clientUid)
{
    m_clientUid = clientUid;
    const QStringList args = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (args.isEmpty())
        return QString();
    const QString cmd = args[0].toLower();
    const QStringList rest = args.mid(1);

    if (cmd == QLatin1String("screen")) return cmdScreen(rest);
    if (cmd == QLatin1String("tag"))    return cmdTag(rest);
    if (cmd == QLatin1String("alarm"))  return cmdAlarm(rest);
    if (cmd == QLatin1String("system")) return cmdSystem(rest);
    if (cmd == QLatin1String("config")) return cmdConfig(rest);
    if (cmd == QLatin1String("render")) return cmdRender(rest);
    if (cmd == QLatin1String("device")) return cmdDevice(rest);   // K-9：设备信息/状态（PC SSH 数据回传）
    if (cmd == QLatin1String("vnc"))    return cmdVnc(rest);      // K-9：VNC 运行时启停（设备面板对等）
    if (cmd == QLatin1String("help"))   return helpText();
    if (cmd == QLatin1String("exit") || cmd == QLatin1String("quit"))
        return QStringLiteral("Connection closed.");

    return QStringLiteral("ERROR: 未知命令 \"%1\"（输入 help 查看命令清单）").arg(args[0]);
}

// ── screen ────────────────────────────────────────────────
QString CommandService::cmdScreen(const QStringList& args)
{
    if (!m_bus) return QStringLiteral("ERROR: 运行时总线未初始化");
    const Project& proj = m_bus->project();
    if (args.isEmpty()) return QStringLiteral("用法: screen list | current | switch <name>");

    const QString sub = args[0].toLower();
    if (sub == QLatin1String("list")) {
        QString out = QStringLiteral("画面列表 (%1):").arg(proj.screens.size());
        const QString cur = m_bus->currentScreenName();
        for (const Screen& s : proj.screens) {
            const QString mark = (s.name == cur) ? QStringLiteral("  [当前]") : QString();
            out += QStringLiteral("\n  %1%2").arg(s.name, mark);
        }
        return out;
    }
    if (sub == QLatin1String("current")) {
        const QString cur = m_bus->currentScreenName();
        return cur.isEmpty() ? QStringLiteral("（未进入画面 / 导航模式）") : QStringLiteral("当前画面: %1").arg(cur);
    }
    if (sub == QLatin1String("switch")) {
        if (args.size() < 2) return QStringLiteral("用法: screen switch <name>");
        const QString name = args.mid(1).join(QLatin1Char(' '));
        bool found = false;
        for (const Screen& s : proj.screens) { if (s.name == name) { found = true; break; } }
        if (!found) return QStringLiteral("ERROR: 画面不存在: %1").arg(name);
        // 只调 onScreenSwitch（QML switchToName→switchTo 唯一入口同步 current/previous，
        // 保留切出画面卸载瞬间事件命中——预置 setCurrentScreenByName 会造成双重置丢失 previous）
        if (m_bus->onScreenSwitch) m_bus->onScreenSwitch(name);
        return QStringLiteral("✓ 已切换到画面 \"%1\"").arg(name);
    }
    return QStringLiteral("用法: screen list | current | switch <name>");
}

// ── tag ───────────────────────────────────────────────────
QString CommandService::cmdTag(const QStringList& args)
{
    if (!m_bus || !m_dm) return QStringLiteral("ERROR: 数据管理器未初始化");
    const Project& proj = m_bus->project();
    if (args.isEmpty()) return QStringLiteral("用法: tag list | read <name> | write <name> <value>");

    const QString sub = args[0].toLower();
    if (sub == QLatin1String("list")) {
        QString out = QStringLiteral("变量列表 (%1):").arg(proj.tags.size());
        for (const Tag& t : proj.tags) {
            const QVariant v = m_dm->value(t.name);
            out += QStringLiteral("\n  %1 = %2").arg(t.name, v.isValid() ? v.toString() : QStringLiteral("-"));
        }
        return out;
    }
    if (sub == QLatin1String("read")) {
        if (args.size() < 2) return QStringLiteral("用法: tag read <name>");
        const QString name = args[1];
        const QVariant v = m_dm->value(name);
        if (!v.isValid()) return QStringLiteral("ERROR: 变量不存在: %1").arg(name);
        return QStringLiteral("%1 = %2").arg(name, v.toString());
    }
    if (sub == QLatin1String("write")) {
        if (args.size() < 3) return QStringLiteral("用法: tag write <name> <value>");
        if (!isAdmin(m_clientUid)) return QStringLiteral("ERROR: 权限不足（写变量需 root）");
        const QString name = args[1];
        // 按变量声明类型转换（与 GUI TagWrite 路径一致：Bool→bool、数字→double、其余→字符串）
        const Tag* tag = nullptr;
        for (const Tag& t : proj.tags) { if (t.name == name) { tag = &t; break; } }
        if (!tag || !m_dm->hasTag(name)) return QStringLiteral("ERROR: 变量不存在: %1").arg(name);
        const QString value = args.mid(2).join(QLatin1Char(' '));
        QVariant out;
        if (tag->dataType == TagDataType::Bool) {
            const QString v = value.trimmed().toLower();
            if (v == QLatin1String("true") || v == QLatin1String("1")) out = QVariant(true);
            else if (v == QLatin1String("false") || v == QLatin1String("0")) out = QVariant(false);
            else return QStringLiteral("ERROR: BOOL 变量只能写 true/false（或 1/0）");
        } else if (tag->dataType == TagDataType::String) {
            out = QVariant(value);
        } else {
            bool ok = false;
            const double num = value.toDouble(&ok);
            if (!ok) return QStringLiteral("ERROR: 数值变量需要数值: %1").arg(value);
            out = QVariant(num);
        }
        m_dm->setValue(name, out);
        return QStringLiteral("✓ %1 = %2").arg(name, m_dm->value(name).toString());
    }
    return QStringLiteral("用法: tag list | read <name> | write <name> <value>");
}

// ── alarm ─────────────────────────────────────────────────
QString CommandService::cmdAlarm(const QStringList& args)
{
    if (!m_ae) return QStringLiteral("ERROR: 报警引擎未初始化");
    if (args.isEmpty()) return QStringLiteral("用法: alarm list | ack <id> | history");

    const QString sub = args[0].toLower();
    if (sub == QLatin1String("list")) {
        const QVariantList alarms = m_ae->activeAlarms();
        if (alarms.isEmpty()) return QStringLiteral("当前无活动报警");
        QString out = QStringLiteral("活动报警 (%1):").arg(alarms.size());
        for (const QVariant& a : alarms) {
            const QVariantMap m = a.toMap();
            out += QStringLiteral("\n  %1 [%2] %3 %4")
                       .arg(m.value(QStringLiteral("id")).toString(),
                            m.value(QStringLiteral("level")).toString(),
                            m.value(QStringLiteral("time")).toString(),
                            m.value(QStringLiteral("message")).toString());
        }
        return out;
    }
    if (sub == QLatin1String("ack")) {
        if (args.size() < 2) return QStringLiteral("用法: alarm ack <id>");
        if (!isAdmin(m_clientUid)) return QStringLiteral("ERROR: 权限不足（确认报警需 root）");
        // 校验 id 存在（ackAlarm 对不存在 id 静默返回——防「已确认」假成功）
        bool found = false;
        const QVariantList alarms = m_ae->activeAlarms();
        for (const QVariant& a : alarms) {
            if (a.toMap().value(QStringLiteral("id")).toString() == args[1]) { found = true; break; }
        }
        if (!found) return QStringLiteral("ERROR: 活动报警不存在: %1").arg(args[1]);
        m_ae->ackAlarm(args[1]);
        return QStringLiteral("✓ 报警 %1 已确认").arg(args[1]);
    }
    if (sub == QLatin1String("history")) {
        if (!m_dl) return QStringLiteral("ERROR: 数据记录未初始化");
        const QVariantList rows = m_dl->queryAlarmHistory(50);   // 最近 50 条（与历史页 nav.qml 一致，2026-08-26 注释对齐）
        if (rows.isEmpty()) return QStringLiteral("报警历史为空");
        QString out = QStringLiteral("报警历史 (最近 %1):").arg(rows.size());
        for (const QVariant& r : rows) {
            const QVariantMap m = r.toMap();
            out += QStringLiteral("\n  %1 %2 %3")
                       .arg(m.value(QStringLiteral("ts")).toString(),
                            m.value(QStringLiteral("tag_name")).toString(),
                            m.value(QStringLiteral("event")).toString());
        }
        return out;
    }
    return QStringLiteral("用法: alarm list | ack <id> | history");
}

// ── system ────────────────────────────────────────────────
QString CommandService::cmdSystem(const QStringList& args)
{
    if (args.isEmpty()) return QStringLiteral("用法: system info | reboot confirm");
    const QString sub = args[0].toLower();
    if (sub == QLatin1String("info")) {
        if (!m_di) return QStringLiteral("ERROR: 设备信息未初始化");
        return QStringLiteral("IP: %1\n内核: %2\n版本: %3\n运行: %4")
                   .arg(m_di->ipAddress(), m_di->kernelVersion(), m_di->appVersion(), m_di->uptimeText());
    }
    if (sub == QLatin1String("reboot")) {
        if (!isAdmin(m_clientUid)) return QStringLiteral("ERROR: 权限不足（重启需 root）");
        if (args.size() < 2 || args[1].toLower() != QLatin1String("confirm"))
            return QStringLiteral("危险操作：system reboot confirm 才会执行（确认重启设备）");
#ifdef Q_OS_UNIX
        // 先返回响应文本，再延迟执行（避免 system() 同步阻塞导致响应未写出就重启）
        QTimer::singleShot(200, this, []() { ::system("sync; reboot"); });
        return QStringLiteral("已发起重启…");
#else
        return QStringLiteral("（仿真环境不执行真实重启）");
#endif
    }
    return QStringLiteral("用法: system info | reboot confirm");
}

// ── config / render ───────────────────────────────────────
QString CommandService::cmdConfig(const QStringList& args)
{
    if (args.isEmpty()) return QStringLiteral("用法: config reload");
    if (args[0].toLower() != QLatin1String("reload")) return QStringLiteral("用法: config reload");
    // 首版：工程热重载需走主程序加载路径——仅提示（不调 resetScreens，避免当前画面事件匹配失效）
    return QStringLiteral("工程重载请重启 FW：killall navigatorhmi-fw && navigatorhmi-fw --project <默认工程>（--project 用 /mnt/user/userdata/app.navihmi）");
}

QString CommandService::cmdRender(const QStringList& args)
{
    if (args.isEmpty()) return QStringLiteral("用法: render refresh");
    if (args[0].toLower() != QLatin1String("refresh")) return QStringLiteral("用法: render refresh");
    // 真实重绘需 VNC markDirty 机制——首版不做破坏性画面重置（不调 resetScreens）
    return QStringLiteral("画面渲染为实时刷新（无需手动触发）；如需重载工程请用 config reload");
}

// ── device（K-9：设备信息/状态——PC SSH 数据回传基础）────────────────
QString CommandService::deviceModel() const
{
    // K-9 评论1：不写死——无工程时按设备默认分辨率查型号表（设备本身型号，非工程推导），工程加载后走工程字段/查表
    // 2026-08-30 用户评论：有工程但型号/分辨率皆空 = 错误工程 → 返回"未知"（调用方报错，不静默兜底）
    if (!m_bus) {
        Project stub;
        stub.deviceWidth = kDefaultDeviceWidth;
        stub.deviceHeight = kDefaultDeviceHeight;
        return deviceModelFor(stub);
    }
    const QString model = deviceModelFor(m_bus->project());
    return model.isEmpty() ? QStringLiteral("未知（错误工程：无型号且无有效分辨率）") : model;
}

QString CommandService::deviceSizeInch() const
{
    if (!m_bus) {
        Project stub;
        stub.deviceWidth = kDefaultDeviceWidth;
        stub.deviceHeight = kDefaultDeviceHeight;
        return deviceSizeInchFor(stub);
    }
    const QString inch = deviceSizeInchFor(m_bus->project());
    return inch.isEmpty() ? QStringLiteral("未知（错误工程）") : inch;
}

QString CommandService::cmdDevice(const QStringList& args)
{
    if (args.isEmpty()) return QStringLiteral("用法: device info");
    if (args[0].toLower() != QLatin1String("info")) return QStringLiteral("用法: device info");
    if (!m_di) return QStringLiteral("ERROR: 设备信息未初始化");
    const QString ip = m_di->ipAddress();
    const QString fw = m_di->appVersion();
    const bool json = args.size() > 1 && args[1] == QLatin1String("-j");   // K-9：-j JSON 输出（PC 解析用）
    if (json)
        return QStringLiteral("{\"model\":\"%1\",\"sizeInch\":\"%2\",\"id\":\"%3\",\"version\":\"%4\"}")
                   .arg(deviceModel(), deviceSizeInch(), ip, fw);
    return QStringLiteral("型号: %1\n尺寸: %2\nID: %3\n固件版本: %4\n")
               .arg(deviceModel(), deviceSizeInch(), ip, fw);
}

// ── vnc（K-9：VNC 运行时启停——设备面板对等；proto enable_vnc=21 启动默认值，运行时指令可覆盖）──
QString CommandService::cmdVnc(const QStringList& args)
{
    if (args.isEmpty()) return QStringLiteral("用法: vnc on | off");
    if (!isAdmin(m_clientUid)) return QStringLiteral("ERROR: 权限不足（VNC 启停需 root）");
    if (!m_vm) return QStringLiteral("ERROR: VNC 镜像未初始化");
    const QString op = args[0].toLower();
    if (op == QLatin1String("on")) {
        // K-9 评论3：端口走 fwconfig（配置/环境变量），不再写死 5900
        const int port = navihmi::vncPort();
        if (m_vm->start(quint16(port)))
            return QStringLiteral("✓ VNC 已启动（%1）").arg(port);
        return QStringLiteral("ERROR: VNC 启动失败（端口占用？）");
    }
    if (op == QLatin1String("off")) {
        m_vm->stop();
        return QStringLiteral("✓ VNC 已停止");
    }
    return QStringLiteral("用法: vnc on | off");
}

QString CommandService::helpText() const
{
    return QStringLiteral(
        "NavigatorHMI FW 命令（root 全权 / 非 root 只读）\n"
        "  screen list | current | switch <name>   画面\n"
        "  tag list | read <name> | write <name> <value>   变量\n"
        "  alarm list | ack <id> | history   报警\n"
        "  system info | reboot confirm   设备信息/重启\n"
        "  device info [-j]   设备信息（型号/尺寸/ID/固件版本，-j=JSON）\n"
        "  vnc on | off   VNC 运行时启停（端口见 /etc/navigatorhmi/fw-config.json）\n"
        "  config reload   工程重载提示（需重启 FW 生效）\n"
        "  render refresh   画面为实时刷新（无需手动触发）\n"
        "  help   本帮助\n"
        "  exit   退出");
}

} // namespace navihmi
