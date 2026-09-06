/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\fwconfig.h
 * @Description: FW 运行时配置单点服务（K-9 评论3/4 起始 + V-2 2026-09-06 F18 配置分层扩展）
 *               配置文件: /etc/navigatorhmi/fw-config.json（对象结构，点分路径寻址）
 *               能力:
 *                 - 键路径常量集中（防魔法字符串散落漂移）
 *                 - fwConfigGetInt/GetString(path, default) 通用读（懒加载 + 解析校验）
 *                 - fwConfigSet(path, value) 通用写（**原子写 tmp + ::rename**；写盘成功才 reload 内存——
 *                   失败不动内存（事务性纪律，F18）；V+1 首个写者（MQTT 配置/运行参数）落地时消费）
 *                 - fwConfigReload() 重载（V+1 watcher/热更新消费者接入；启动读项改后重启生效）
 *               环境变量覆盖机制（NAVIHMI_VNC_PORT 等）保留——env 优先于配置文件
 *               日志纪律：凭据类值**禁止**经 qInfo/qWarning 输出（脱敏见 logsanitize.h——V+1 MQTT 凭据接入点）
 */
#pragma once

#include <cstdio>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include "runtime/logsanitize.h"   // 配置域敏感值日志脱敏（随 fwconfig 一起编译——审查 🟡 覆盖）

#if defined(Q_OS_UNIX)
#include <unistd.h>   // fsync（fwConfigSet 原子写刷盘）
#endif

namespace navihmi {

/// VNC 默认端口（K-9 评论3：命名常量防魔法数字；实际端口经 vncPort() 读配置/环境变量）
inline constexpr quint16 kDefaultVncPort = 5900;

/// 配置文件路径（单点常量——防字符串散落漂移）
inline constexpr char kFwConfigPath[] = "/etc/navigatorhmi/fw-config.json";
/// 配置目录（原子写/otaupdater 共用）
inline constexpr char kFwConfigDir[] = "/etc/navigatorhmi";
/// 键路径常量（点分路径，fwConfigGet/Set 寻址）
inline constexpr char kCfgVncPort[] = "vnc/port";

namespace detail {

/// 加载并解析配置文件（文件缺失/解析失败 → 空对象，调用方按默认值兜底）
inline QJsonObject loadFwConfig()
{
    QFile f(QString::fromLatin1(kFwConfigPath));
    if (!f.open(QIODevice::ReadOnly)) {
        qInfo().noquote() << "fw-config.json 未找到（" << kFwConfigPath
                          << "），使用默认（可用 NAVIHMI_VNC_PORT 等环境变量覆盖）";
        return QJsonObject();
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        qWarning().noquote() << "fw-config.json 解析失败（非对象），使用默认";
        return QJsonObject();
    }
    return doc.object();
}

// C++17 inline 变量：header 多 TU 共享单份缓存（懒加载 + reload 可刷新）
inline QJsonObject g_fwConfig;
inline bool g_fwConfigLoaded = false;

/// 当前配置对象（懒加载）
inline const QJsonObject& fwConfigObject()
{
    if (!g_fwConfigLoaded) {
        g_fwConfig = loadFwConfig();
        g_fwConfigLoaded = true;
    }
    return g_fwConfig;
}

/// 重载配置（Set 写盘成功后调用同步内存；V+1 watcher 热更新调用点）
inline void fwConfigReload()
{
    g_fwConfig = loadFwConfig();
    g_fwConfigLoaded = true;
}

/// 点分路径取值（"vnc/port" → obj["vnc"]["port"]；路径任一段缺失/非对象返回 invalid）
inline QJsonValue fwConfigLookup(const QJsonObject& root, const QString& dottedPath)
{
    const QStringList segs = dottedPath.split(QLatin1Char('/'));
    QJsonObject cur = root;   // 值拷贝遍历（避免取临时 toObject() 地址——编译修正 2026-09-06）
    for (int i = 0; i < segs.size(); ++i) {
        if (!cur.contains(segs.at(i)))
            return QJsonValue();
        const QJsonValue v = cur.value(segs.at(i));
        if (i == segs.size() - 1)
            return v;
        if (!v.isObject())
            return QJsonValue();
        cur = v.toObject();
    }
    return QJsonValue();
}

/// 点分路径递归写入（segs[0..idx] 已处理；末段赋值）
inline void fwConfigSetNested(QJsonObject& obj, const QStringList& segs, int idx, const QJsonValue& v)
{
    if (idx == segs.size() - 1) {
        obj.insert(segs.at(idx), v);
        return;
    }
    QJsonObject child = obj.value(segs.at(idx)).toObject();
    fwConfigSetNested(child, segs, idx + 1, v);
    obj.insert(segs.at(idx), child);
}

} // namespace detail

/// 通用配置读：点分路径 + 默认值（缺失/非法 → 默认）
inline int fwConfigGetInt(const char* path, int def)
{
    const QJsonValue v = detail::fwConfigLookup(detail::fwConfigObject(), QString::fromLatin1(path));
    return v.isDouble() ? v.toInt(def) : def;
}

inline QString fwConfigGetString(const char* path, const QString& def)
{
    const QJsonValue v = detail::fwConfigLookup(detail::fwConfigObject(), QString::fromLatin1(path));
    return v.isString() ? v.toString() : def;
}

/// 通用配置写（V-2 F18 2026-09-06）：点分路径赋值 → **原子写（tmp + ::rename 覆盖）**——
/// 写盘失败不动内存（root 拷贝未提交 + 不 reload）；写盘成功 fwConfigReload 同步内存缓存。
/// 消费者：V+1 MQTT 配置/运行参数写（本版无业务写者——机制就绪；启动读项（如 vnc/port）改后重启生效）。
inline bool fwConfigSet(const char* path, const QVariant& value)
{
    const QString dotted = QString::fromLatin1(path);
    const QStringList segs = dotted.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segs.isEmpty() || value.isNull())
        return false;
    QJsonObject root = detail::fwConfigObject();   // 拷贝现有配置（空对象 = 全新写）
    detail::fwConfigSetNested(root, segs, 0, QJsonValue::fromVariant(value));

    QDir().mkpath(QString::fromLatin1(kFwConfigDir));
    const QString dest = QString::fromLatin1(kFwConfigPath);
    const QString tmp = dest + QStringLiteral(".tmp");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote() << "fw-config 写临时文件失败:" << f.errorString();
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    // 复审 🟡（2026-09-06）：write/flush 返回值检查 + rename 前 fsync（对齐 touchcalibrator 7e0143b8 先例——
    // ENOSPC 部分写入后 rename 成功 → 盘上残缺 JSON → reload 解析失败内存置空，「失败不动内存」不成立）
    if (f.write(payload) != payload.size() || !f.flush()) {
        f.close();
        QFile::remove(tmp);
        qWarning().noquote() << "fw-config 写盘失败（空间不足?）——保留内存旧值";
        return false;
    }
#if defined(Q_OS_UNIX)
    if (::fsync(f.handle()) != 0) {
        f.close();
        QFile::remove(tmp);
        qWarning().noquote() << "fw-config fsync 失败——保留内存旧值";
        return false;
    }
#endif
    f.close();
    if (::rename(tmp.toUtf8().constData(), dest.toUtf8().constData()) != 0) {
        QFile::remove(tmp);
        qWarning().noquote() << "fw-config 原子替换失败（保留内存旧值）";
        return false;
    }
    detail::fwConfigReload();   // 写盘成功才同步内存（事务性：失败路径内存未动）
    return true;
}

/// VNC 端口（配置/环境变量/默认 5900 三阶；进程启动读——所有 VNC 启动点统一走这里防魔法数字漂移；
/// 注意：static 缓存进程固定——fwConfigSet 改 vnc/port 后**重启生效**（启动读配置语义）；
/// 复审 🔴（2026-09-06）：配置文件值补 1..65535 值域校验（原 V-1 语义——70000 直通 quint16 截断为
/// 意外端口 4464 静默误动作））
inline int vncPort()
{
    static const int port = [] {
        int p = fwConfigGetInt(kCfgVncPort, kDefaultVncPort);
        if (p < 1 || p > 65535) {
            qWarning().noquote() << "fw-config.json vnc.port 非法（应为 1-65535），使用默认 5900";
            p = kDefaultVncPort;
        }
        bool okEnv = false;
        const int pv = qEnvironmentVariableIntValue("NAVIHMI_VNC_PORT", &okEnv);
        if (okEnv && pv > 0 && pv < 65536)
            p = pv;
        return p;
    }();
    return port;
}

} // namespace navihmi
