/*
 * @FilePath: \NavigatorHMI_FW\src\converter\qmlgenerator.h
 * @Description: 转换器——HMIProject 运行时模型 → 每画面 QML 文件
 *               输入: navihmi::Project（ProjectParser 解析产物）
 *               输出: 每画面 .qml（控件声明 + 事件信号占位）
 * B-4: 首版——15 基础控件 + 窗口模板 + 世界地图 + 全局叠加层
 */
#pragma once

#include <QString>
#include <QList>
#include "runtime/projectmodel.h"

namespace navihmi {

class QmlGenerator
{
public:
    /// 生成单画面 QML 文本
    static QString generateScreen(const Project& proj, const Screen& screen);
    /// 生成世界地图 QML 文本（特殊画面）
    static QString generateWorldMap(const Project& proj);
    /// 生成全局画面叠加层 QML 文本
    static QString generateOverlay(const Project& proj);
    /// 生成全部画面文件（返回 <文件名, 内容> 列表；主壳固定用 qrc:/qml/main.qml）
    static QList<QPair<QString, QString>> generateAll(const Project& proj);
};

} // namespace navihmi
