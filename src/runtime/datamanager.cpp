/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\datamanager.cpp
 * @Description: 数据管理器实现——变量值存储 + 变化通知
 */
#include "runtime/datamanager.h"

namespace navihmi {

DataManager::DataManager(QObject* parent)
    : QObject(parent)
{
}

void DataManager::setProject(const Project& proj)
{
    m_values.clear();
    m_sources.clear();
    m_types.clear();
    for (const auto& tag : proj.tags) {
        m_sources.insert(tag.name, tag.source);   // J-1: 变量数据源映射（内部/外部判定）
        m_types.insert(tag.name, int(tag.dataType));   // W-E: 存类型（tagType() 查询）
        // 初始值用 baseValue（设计态基准值），按类型解析
        QVariant v;
        switch (tag.dataType) {
        case TagDataType::Bool:
            v = (tag.baseValue == "1" || tag.baseValue == "true" ||
                 tag.baseValue == "TRUE" || tag.baseValue == "True");
            break;
        case TagDataType::Int16:
        case TagDataType::Uint16:
        case TagDataType::Int32:
            v = tag.baseValue.toInt();
            break;
        case TagDataType::Float:
            v = tag.baseValue.toDouble();
            break;
        case TagDataType::Gps:
        case TagDataType::DateTime:
        case TagDataType::String:
        default:
            v = tag.baseValue;
            break;
        }
        m_values.insert(tag.name, v);
    }
}

QString DataManager::tagType(const QString& tagName) const
{
    const auto it = m_types.constFind(tagName);
    if (it == m_types.constEnd())
        return QString();
    switch (TagDataType(it.value())) {
    case TagDataType::Bool:   return QStringLiteral("BOOL");
    case TagDataType::Int16:  return QStringLiteral("INT16");
    case TagDataType::Uint16: return QStringLiteral("UINT16");
    case TagDataType::Int32:  return QStringLiteral("INT32");
    case TagDataType::Float:  return QStringLiteral("FLOAT");
    case TagDataType::String: return QStringLiteral("STRING");
    case TagDataType::DateTime: return QStringLiteral("DATETIME");
    case TagDataType::Gps:    return QStringLiteral("GPS");
    }
    return QString();
}

QVariant DataManager::value(const QString& tagName) const
{
    return m_values.value(tagName);
}

void DataManager::setValue(const QString& tagName, const QVariant& value)
{
    if (!m_values.contains(tagName))
        return;
    if (m_values.value(tagName) == value)
        return;   // 值未变不发信号（防抖——F9 V-3 环路双保险①：源头同值不触发；
                  // QML onValueChanged 同值不触发 → OnValueChange 事件链自环天然断在源头；
                  // 增值类动作自激（每次值变）由 RuntimeBus 风暴熔断（保险②）兜底）
    m_values.insert(tagName, value);
    emit valueChanged(tagName, value);
}

bool DataManager::hasTag(const QString& tagName) const
{
    return m_values.contains(tagName);
}

QStringList DataManager::tagNames() const
{
    return m_values.keys();
}

QString DataManager::tagSource(const QString& tagName) const
{
    return m_sources.value(tagName);
}

} // namespace navihmi
