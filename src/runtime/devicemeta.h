/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\devicemeta.h
 * @Description: 设备身份推导单点（K-9 审查：CommandService / HttpReceiver 共用）——
 *               型号/尺寸：优先工程 device_model 字段（proto 22，PC 端 device-profile 写入），
 *               空（旧工程）按分辨率查表（/etc/navigatorhmi/device-profiles.json，与 PC 端同构）。
 *               合法工程必有型号或分辨率之一；两者皆空 = 错误工程 → 返回空（调用方显式报错），
 *               不静默兜底（2026-08-30 用户评论：错误工程不应伪装成 7 寸）。
 *               kDefaultDeviceWidth/Height 仅作设备物理屏默认（QML 初始化），不参与身份推导。
 *               防双处重复实现漂移（10/15 寸扩展时只改配置文件）。
 */
#pragma once

#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "runtime/projectmodel.h"

namespace navihmi {

/// 设备物理屏默认分辨率（QML 初始化/无工程时设备默认显示尺寸用；**非身份推导兜底**——错误工程不适用）
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
    if (width <= 0 || height <= 0) return QString();   // 分辨率无效 = 无法推导（错误工程）
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

/// 设备型号：优先工程型号字段；空则按分辨率查表；均无法确定 → 返回空（错误工程，调用方显式报错，不兜底）
inline QString deviceModelFor(const Project& proj)
{
    if (!proj.deviceModel.isEmpty())
        return proj.deviceModel;
    return detail::modelForResolution(proj.deviceWidth, proj.deviceHeight);
}

/// 设备尺寸：型号查表；型号为空/查不到 → 返回空（错误工程，调用方显式报错，不兜底）
inline QString deviceSizeInchFor(const Project& proj)
{
    const QString model = deviceModelFor(proj);
    if (model.isEmpty())
        return QString();
    return detail::sizeInchForModel(model);
}

} // namespace navihmi
