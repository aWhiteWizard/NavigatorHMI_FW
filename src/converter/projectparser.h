/*
 * @FilePath: \NavigatorHMI_FW\src\converter\projectparser.h
 * @Description: 转换器——.navihmi (proto3 二进制) → HMIProject 运行时模型
 *               输入: app.navihmi（PC 组态软件 compile 产物, 契约 navihmi.proto）
 *               输出: navihmi::Project（QML/报警/渲染只依赖此模型）
 * B-4: 首版实现——proto 解析 + 字段映射全量（含世界地图/用户系统）
 */
#pragma once

#include <QString>
#include "runtime/projectmodel.h"

namespace navihmi {

class ProjectParser
{
public:
    /// 解析 .navihmi 二进制 → 运行时模型。成功返回 true。
    static bool parseFile(const QString& path, Project& out);
    /// 从内存字节解析（测试用）
    static bool parseBytes(const QByteArray& data, Project& out);
};

} // namespace navihmi
