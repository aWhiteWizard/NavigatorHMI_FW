/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\devicemeta.h
 * @Description: 设备身份推导单点（K-9 审查：CommandService / HttpReceiver 共用）——
 *               型号/尺寸 = 设备自身硬件身份（物理屏默认分辨率查 /etc/navigatorhmi/device-profiles.json），
 *               与工程内容无关（2026-08-30 用户 Check 指正：设备身份是设备自身的属性，PC 需要时向设备要，
 *               不因加载的工程有无型号而改变）。
 *               kDefaultDeviceWidth/Height = 设备物理屏默认分辨率（身份推导依据，单点收敛）。
 *               防双处重复实现漂移（10/15 寸扩展时只改配置文件）。
 */
#pragma once

#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPair>

#include "runtime/projectmodel.h"

namespace navihmi {

/// 设备物理屏默认分辨率（设备自身硬件身份推导依据：查 device-profiles.json 得型号/尺寸；2026-08-30 用户 Check 指正——设备身份与工程无关）
inline constexpr int kDefaultDeviceWidth = 1024;
inline constexpr int kDefaultDeviceHeight = 600;

namespace detail {

/// 设备型号表（型号 → 宽/高/尺寸）；启动时从 /etc/navigatorhmi/device-profiles.json 读入（K-9 评论1/2：查表化，加型号只改配置文件）
inline const QJsonArray& deviceProfileTable()
{
    static const QJsonArray table = [] {
        QJsonArray arr;
        QFile f(QStringLiteral("/etc/navigatorhmi/device-profiles.json"));
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isArray()) {
                arr = doc.array();
            } else {
                qWarning().noquote() << "device-profiles.json 解析失败（非数组），型号表为空";
            }
        } else {
            // 配置文件不存在：Linux 真机 = 未部署（型号表空，身份推导回退空 → 调用方报错）；Windows 仿真 = 合法（无该文件）
            qInfo().noquote() << "device-profiles.json 未找到（/etc/navigatorhmi/），型号表为空";
        }
        return arr;
    }();
    return table;
}

/// 按分辨率查型号（遍历表匹配 width/height；无匹配返回空）
inline QString modelForResolution(int width, int height)
{
    if (width <= 0 || height <= 0) return QString();   // 分辨率无效 = 无法推导（防御：恒传物理屏默认分辨率，正常不可达）
    const QJsonArray& table = deviceProfileTable();
    for (const QJsonValue& v : table) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("width")).toInt() == width &&
            o.value(QStringLiteral("height")).toInt() == height)
            return o.value(QStringLiteral("model")).toString();
    }
    return QString();
}

/// 按型号查尺寸（"7寸"）；查不到返回空
inline QString sizeInchForModel(const QString& model)
{
    const QJsonArray& table = deviceProfileTable();
    for (const QJsonValue& v : table) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("model")).toString() == model)
            return o.value(QStringLiteral("sizeInch")).toString();
    }
    return QString();
}

} // namespace detail

/// 设备型号（设备自身硬件身份）：按设备物理屏默认分辨率查 device-profiles.json——与工程无关
/// （2026-08-30 用户 Check 指正：设备身份由设备端判断，PC 需要时向设备要；不因工程有无型号而变）
inline QString deviceModelFor()
{
    return detail::modelForResolution(kDefaultDeviceWidth, kDefaultDeviceHeight);
}

/// 设备尺寸（"7寸"/"4寸"，型号查表；与工程无关）
inline QString deviceSizeInchFor()
{
    const QString model = deviceModelFor();
    if (model.isEmpty())
        return QString();
    return detail::sizeInchForModel(model);
}

/// 设备物理屏分辨率（型号查表；与工程无关）——VNC 读帧区域/宣告尺寸用
/// （2026-08-30 N+24 用户裁决：VNC 尺寸应 = 连接的设备，而非工程 deviceWidth/Height——
///   工程 800×480 时 VNC 曾按 800×480 读帧，与 7 寸物理屏 1024×600 不符）
inline QPair<int, int> deviceResolutionFor()
{
    const QString model = deviceModelFor();
    if (!model.isEmpty()) {
        const QJsonArray& table = detail::deviceProfileTable();
        for (const QJsonValue& v : table) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("model")).toString() == model) {
                const int w = o.value(QStringLiteral("width")).toInt();
                const int h = o.value(QStringLiteral("height")).toInt();
                if (w > 0 && h > 0)
                    return { w, h };
            }
        }
    }
    // 型号表缺失/查不到 → 物理屏默认分辨率兜底（与 deviceModelFor 推导同源）
    return { kDefaultDeviceWidth, kDefaultDeviceHeight };
}

} // namespace navihmi
