/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\projectmodel.cpp
 * @Description: 运行时模型查询实现
 */
#include "projectmodel.h"

namespace navihmi {

const Screen* Project::ScreenByName(const QString& name) const
{
    for (const Screen& s : screens)
        if (s.name == name) return &s;
    return nullptr;
}

const Tag* Project::TagByName(const QString& name) const
{
    for (const Tag& t : tags)
        if (t.name == name) return &t;
    return nullptr;
}

const AlarmRule* Project::AlarmByName(const QString& name) const
{
    for (const AlarmRule& a : alarms)
        if (a.name == name) return &a;
    return nullptr;
}

const ListDef* Project::ListByName(const QString& name, ListType type) const
{
    // U-2（2026-09-06）：按类型过滤——Text/Image/Video 列表可跨类型同名，消费方（TextList/Image/视频）带类型查
    for (const ListDef& l : lists)
        if (l.type == type && l.name == name) return &l;
    return nullptr;
}

const Screen* Project::StartScreenModel() const
{
    const Screen* s = ScreenByName(startScreen);
    if (s) return s;
    for (const Screen& c : screens)
        if (c.type == ScreenType::Custom) return &c;
    return screens.isEmpty() ? nullptr : &screens.first();
}

} // namespace navihmi
