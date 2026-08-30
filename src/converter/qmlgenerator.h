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
    /// 生成单画面 QML 文本；resourceRoot 非空时控件图片路径（imagePath/listItems）解析为设备端绝对落盘路径
    /// （D-B2：QML 相对路径按文档基址 /tmp/navihmi_gen/ 解析，不指向工程目录——必须注入绝对路径，
    ///  对齐 backgroundImage 先例 main.cpp loadAndInject）
    static QString generateScreen(const Project& proj, const Screen& screen, const QString& resourceRoot = QString());
    /// 生成世界地图 QML 文本（特殊画面）；tileBasePath 非空时注入瓦片根目录（R3: 工程自带瓦片）；
    /// backgroundImagePath 非空时注入锁定视角底图（N-1：PC 拼好的单张 PNG，有底图时瓦片层/模拟底图隐藏）
    static QString generateWorldMap(const Project& proj, const QString& tileBasePath = QString(),
                                    const QString& backgroundImagePath = QString());
    /// 生成全局画面叠加层 QML 文本；resourceRoot 同上（D-B2：全局画面控件图片路径解析）
    static QString generateOverlay(const Project& proj, const QString& resourceRoot = QString());
    /// 生成全部画面文件（返回 <文件名, 内容> 列表；主壳固定用 qrc:/qml/main.qml）；
    /// resourceRoot 非空时传入设备端资源根目录（工程目录，如 /mnt/user/userdata）
    static QList<QPair<QString, QString>> generateAll(const Project& proj, const QString& resourceRoot = QString());
};

} // namespace navihmi
