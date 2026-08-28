/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\devicemeta.h
 * @Description: 设备身份推导单点（K-9 审查：CommandService / HttpReceiver 共用）——
 *               型号/尺寸按工程分辨率推导（1024×600→NavigatorHMI-7 / 720×720→NavigatorHMI-4）。
 *               防双处重复实现漂移（10/15 寸扩展时只改这一处）。
 */
#pragma once

#include <QString>

#include "runtime/projectmodel.h"

namespace navihmi {

/// 设备型号（按工程分辨率推导；无工程/未知分辨率默认 7 寸）
inline QString deviceModelFor(const Project& proj)
{
    if (proj.deviceWidth == 720 && proj.deviceHeight == 720)
        return QStringLiteral("NavigatorHMI-4");
    return QStringLiteral("NavigatorHMI-7");
}

/// 设备尺寸（"7寸"/"4寸"——与 device-profile sizeInch 口径一致）
inline QString deviceSizeInchFor(const Project& proj)
{
    return deviceModelFor(proj) == QLatin1String("NavigatorHMI-4") ? QStringLiteral("4寸") : QStringLiteral("7寸");
}

} // namespace navihmi
