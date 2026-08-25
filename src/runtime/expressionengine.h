/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\expressionengine.h
 * @Description: 条件表达式引擎（I-3）——事件 condition 字符串解析求值（首版最小集）
 *               语法（I-execution-plan.md 附录 A 定稿）：
 *                 值：数字/布尔/字符串("...")/变量名（DataManager 当前值，缺失=0+告警）/ value 关键字（触发事件对象绑定变量）
 *                 运算符（优先级从高到低）：!  →  * /  →  + -  →  比较(> >= < <= == !=)  →  && ||
 *                 函数：abs(x) round(x) min(a,b) max(a,b) now()（Unix 秒）
 *                 字符串仅支持 == / !=
 *               执行语义：非法表达式/变量缺失 → false + qWarning（fail-safe 不误动作）
 */
#pragma once

#include <QString>

namespace navihmi {

class DataManager;
class Widget;

class ExprEngine
{
public:
    /// 求值条件表达式（condition 空串 → true；解析/求值失败 → false + qWarning）
    /// widget：事件源控件（value 关键字取 widget->boundTag 的当前值；nullptr 时 value=0）
    static bool eval(const QString& expression, const DataManager* dm, const Widget* widget);
};

} // namespace navihmi
