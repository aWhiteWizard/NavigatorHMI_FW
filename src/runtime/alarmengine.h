/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\alarmengine.h
 * @Description: 报警引擎（G-1b）——活动报警列表（变量阈值驱动模拟源）
 *               用户 2026-08-23 定：报警用模拟测试数据 UI（不连真实采集/检测引擎）
 *               数据来源：AlarmRule 阈值 vs DataManager 变量值（高/低压等, iofield 改值触发）
 */
#pragma once

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QTimer>
#include "runtime/projectmodel.h"

namespace navihmi {

class DataManager;

class AlarmEngine : public QObject
{
    Q_OBJECT
    // QML 绑定用（activeAlarms 变化时 NOTIFY 触发重新读取）
    Q_PROPERTY(QVariantList activeAlarms READ activeAlarms NOTIFY alarmsChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY alarmsChanged)
public:
    explicit AlarmEngine(QObject* parent = nullptr);

    void setProject(const Project& proj);
    void setDataManager(DataManager* dm);

    /// 活动报警列表：[{id, time, level, message, acked, tag}]（时间倒序）
    Q_INVOKABLE QVariantList activeAlarms() const;
    Q_INVOKABLE int activeCount() const;

    /// 行确认（携带编号）
    Q_INVOKABLE void ackAlarm(const QString& id);
    /// 批量确认
    Q_INVOKABLE void ackAlarms(const QVariantList& ids);
    /// 全部确认
    Q_INVOKABLE void ackAll();

signals:
    /// 活动报警列表变化（QML 刷新）
    void alarmsChanged();
    /// 报警触发（G-2: DataLogger alarm_history 联动）
    void alarmTriggered(const QString& ruleName, const QString& tagName, int level, const QString& message);
    /// 报警确认（G-2: DataLogger alarm_history 联动；单条/批量确认均发）
    void alarmAcked(const QString& ruleName, const QString& tagName, int level, const QString& message);
    /// 报警恢复清除（G-2: DataLogger alarm_history CLEAR；变量回落阈值后）
    void alarmCleared(const QString& ruleName, const QString& tagName, int level, const QString& message);

private:
    void poll();   // 定时轮询变量值 → 触发/清除报警

    struct ActiveAlarm {
        QString id;         // 规则名（唯一）
        QString time;       // 触发时间 HH:mm:ss
        int level = 0;      // Severity 枚举（0 紧急/1 重要/2 警告/3 提示）
        QString message;
        bool acked = false; // 已确认（历史保留）
        QString tag;        // 关联变量名
    };

    Project m_project;
    DataManager* m_dataManager = nullptr;
    QList<ActiveAlarm> m_active;
    QHash<QString, bool> m_triggered;   // ruleName -> 是否处于触发态（防重复触发）
    QHash<QString, QString> m_triggeredTime;
    QTimer* m_timer = nullptr;
};

} // namespace navihmi
