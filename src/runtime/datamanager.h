/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\datamanager.h
 * @Description: 数据管理器（TagStore 雏形）——变量实时值中心存储
 *               QML 组件绑定变量名 → 显示实时值（值变化发信号刷新）
 * B-5: 首版——内部变量值存储 + 读写接口 + 变化通知；数据源（Modbus/MQTT）后续接
 */
#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariant>
#include "runtime/projectmodel.h"

namespace navihmi {

class DataManager : public QObject
{
    Q_OBJECT
public:
    explicit DataManager(QObject* parent = nullptr);

    /// 初始化变量表（从工程模型建初始值：用 baseValue）
    void setProject(const Project& proj);

    /// 读取变量值（不存在返回 invalid QVariant）
    Q_INVOKABLE QVariant value(const QString& tagName) const;
    /// 写变量值（触发 valueChanged 信号）
    Q_INVOKABLE void setValue(const QString& tagName, const QVariant& value);
    /// 是否存在
    Q_INVOKABLE bool hasTag(const QString& tagName) const;

signals:
    /// 变量值变化（QML 组件订阅刷新）
    void valueChanged(const QString& tagName, const QVariant& value);

private:
    QHash<QString, QVariant> m_values;
};

} // namespace navihmi
