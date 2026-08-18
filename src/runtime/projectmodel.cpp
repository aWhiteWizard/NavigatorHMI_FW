/*
 * @Author: aWhiteWizard www.123518341@qq.com
 * @FilePath: \NavigatorHMI_FW\src\runtime\projectmodel.cpp
 * @Description: 运行时模型查询实现
 */
#include "projectmodel.h"

namespace navihmi {

const Screen* Project::screenByName(const QString& name) const
{
    for (const Screen& s : screens)
        if (s.name == name) return &s;
    return nullptr;
}

const Tag* Project::tagByName(const QString& name) const
{
    for (const Tag& t : tags)
        if (t.name == name) return &t;
    return nullptr;
}

const AlarmRule* Project::alarmByName(const QString& name) const
{
    for (const AlarmRule& a : alarms)
        if (a.name == name) return &a;
    return nullptr;
}

const ListDef* Project::listByName(const QString& name) const
{
    for (const ListDef& l : lists)
        if (l.name == name) return &l;
    return nullptr;
}

const Screen* Project::startScreenModel() const
{
    const Screen* s = screenByName(startScreen);
    if (s) return s;
    for (const Screen& c : screens)
        if (c.type == ScreenType::Custom) return &c;
    return screens.isEmpty() ? nullptr : &screens.first();
}

} // namespace navihmi
