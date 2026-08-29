/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\fwconfig.h
 * @Description: FW 运行时配置单点（K-9 评论3/4：5900 等魔法数字收敛）——
 *               读 /etc/navigatorhmi/fw-config.json（{ "vnc": { "port": 5900 } }），
 *               保留环境变量 NAVIHMI_VNC_PORT 覆盖（既有机制，main.cpp 已用）。
 *               改端口只动配置文件，无需改代码。
 */
#pragma once

#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace navihmi {

/// VNC 默认端口（K-9 评论3：命名常量防魔法数字；实际端口经 vncPort() 读配置/环境变量）
inline constexpr quint16 kDefaultVncPort = 5900;

namespace detail {

/// VNC 端口配置（读 fw-config.json；缺失/非法 → kDefaultVncPort；NAVIHMI_VNC_PORT 环境变量优先覆盖）
inline int vncPortConfigured()
{
    int port = kDefaultVncPort;
    QFile f(QStringLiteral("/etc/navigatorhmi/fw-config.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const QJsonObject vnc = doc.object().value(QStringLiteral("vnc")).toObject();
            const int p = vnc.value(QStringLiteral("port")).toInt(0);
            if (p > 0 && p < 65536)
                port = p;
            else
                qWarning().noquote() << "fw-config.json vnc.port 非法（应为 1-65535），使用默认 5900";
        } else {
            qWarning().noquote() << "fw-config.json 解析失败（非对象），使用默认 5900";
        }
    } else {
        qInfo().noquote() << "fw-config.json 未找到（/etc/navigatorhmi/），使用默认 5900（可用 NAVIHMI_VNC_PORT 覆盖）";
    }
    bool okEnv = false;
    const int pv = qEnvironmentVariableIntValue("NAVIHMI_VNC_PORT", &okEnv);
    if (okEnv && pv > 0 && pv < 65536)
        port = pv;
    return port;
}

} // namespace detail

/// VNC 端口（配置/环境变量/默认 5900 三阶；所有 VNC 启动点统一走这里——防魔法数字漂移）
inline int vncPort()
{
    static const int port = detail::vncPortConfigured();
    return port;
}

} // namespace navihmi
