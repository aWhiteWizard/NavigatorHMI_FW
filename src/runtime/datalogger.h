/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\datalogger.h
 * @Description: 数据记录模块（G-2）——SQLite 存储 tag_history（变量采样）+ alarm_history（报警事件）
 *               路径: /mnt/user/userdata/navihmi_history.db（Windows 仿真: AppData）
 *               用户 2026-08-23 定: tag_history + alarm_history 一并, 不做 CSV
 */
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QTimer>
#include "runtime/projectmodel.h"

class QSqlDatabase;

namespace navihmi {

class DataManager;

class DataLogger : public QObject
{
    Q_OBJECT
public:
    explicit DataLogger(QObject* parent = nullptr);
    ~DataLogger() override;

    void setProject(const Project& proj);
    void setDataManager(DataManager* dm);

    /// 报警事件记录（AlarmEngine 触发/确认/清除时调用; event: TRIGGER/ACK/CLEAR）
    void recordAlarmEvent(const QString& ruleName, const QString& tagName, int level,
                          const QString& message, const QString& event);

    // ── 查询接口（趋势图预留 / AlarmView 历史）──
    Q_INVOKABLE QVariantList queryTagHistory(const QString& tagName, int limit = 100);   // 默认 100 条（历史页/CLI 传 50 覆盖）
    Q_INVOKABLE QVariantList queryAlarmHistory(int limit = 100);   // 默认 100 条（同上）

    /// 数据库路径（调试/日志）
    Q_INVOKABLE QString dbPath() const;

private:
    bool openDb();
    void sample();          // 定时采样变量 → tag_history 批量事务写
    void cleanup();         // 保留策略: 7 天或 50 万条先到

    Project m_project;
    DataManager* m_dataManager = nullptr;
    QSqlDatabase* m_db = nullptr;
    QTimer* m_timer = nullptr;
    QTimer* m_cleanupTimer = nullptr;
};

} // namespace navihmi
