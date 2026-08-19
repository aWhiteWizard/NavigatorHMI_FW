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
    for (const auto& tag : proj.tags) {
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

QVariant DataManager::value(const QString& tagName) const
{
    return m_values.value(tagName);
}

void DataManager::setValue(const QString& tagName, const QVariant& value)
{
    if (!m_values.contains(tagName))
        return;
    if (m_values.value(tagName) == value)
        return;   // 值未变不发信号（防抖）
    m_values.insert(tagName, value);
    emit valueChanged(tagName, value);
}

bool DataManager::hasTag(const QString& tagName) const
{
    return m_values.contains(tagName);
}

} // namespace navihmi
