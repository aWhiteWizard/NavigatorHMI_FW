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

    // ── 查询接口（趋势图预留 / AlarmView 缓冲 / HistoryView）──
    Q_INVOKABLE QVariantList queryTagHistory(const QString& tagName, int limit = 100);   // 默认 100（HistoryView 传 200；CLI tag history 传 50）
    Q_INVOKABLE QVariantList queryAlarmHistory(int limit = 100);   // 默认 100（AlarmView 缓冲传 200；CLI alarm history 传 50）

    /// P-5（2026-09-02）：清空报警历史（未确认的当前活动报警不清除——活动集由 AlarmEngine 内存管理，清表不影响）。
    /// 返回 true=清空成功；false=数据库未就绪/删除失败（调用方不得无条件报成功——防假成功，审查 🟡）。
    Q_INVOKABLE bool clearAlarmHistory();

    /// P-5（2026-09-02）：设置数据库路径（空/默认=保持默认库——FW 单库架构恒用默认 navihmi_history.db；非默认仅告警不切库，审查 🔵-3）
    Q_INVOKABLE void setDbPath(const QString& path);

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
