/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\datalogger.cpp
 * @Description: 数据记录实现——SQLite tag_history/alarm_history（WAL + 事务批量写 + 保留策略）
 */
#include "runtime/datalogger.h"
#include "runtime/datamanager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QStandardPaths>
#include <QVariant>

namespace navihmi {

DataLogger::DataLogger(QObject* parent)
    : QObject(parent)
{
}

DataLogger::~DataLogger()
{
    if (m_db) {
        m_db->close();
        delete m_db;
        m_db = nullptr;
    }
}

void DataLogger::setProject(const Project& proj)
{
    m_project = proj;
    if (!openDb())
        return;
    // 定时采样（审查 M10: 30s 间隔——500ms 全量采样 20 变量 → 36 万条/小时, 50 万条上限仅 1.4 小时;
    // 30s × 20 变量 ≈ 2400 条/小时 → 50 万条 ≈ 8.7 天, 匹配 7 天保留策略; 趋势图粒度足够）
    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(30 * 1000);
        connect(m_timer, &QTimer::timeout, this, &DataLogger::sample);
        m_timer->start();
    }
    // 保留策略清理（审查 M10: 6 小时一次——NOT IN 反连接较重, 不宜每小时在 GUI 线程跑）
    if (!m_cleanupTimer) {
        m_cleanupTimer = new QTimer(this);
        m_cleanupTimer->setInterval(6 * 60 * 60 * 1000);
        connect(m_cleanupTimer, &QTimer::timeout, this, &DataLogger::cleanup);
        m_cleanupTimer->start();
    }
}

void DataLogger::setDataManager(DataManager* dm)
{
    m_dataManager = dm;
}

QString DataLogger::dbPath() const
{
#if defined(Q_OS_WIN)
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/navihmi_history.db");
#else
    return QStringLiteral("/mnt/user/userdata/navihmi_history.db");
#endif
}

bool DataLogger::openDb()
{
    if (m_db)
        return m_db->isOpen();
    // 确保目录存在
    const QString path = dbPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_db = new QSqlDatabase(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("navihmi_history")));
    m_db->setDatabaseName(path);
    if (!m_db->open()) {
        qWarning().noquote() << "DataLogger: SQLite 打开失败" << m_db->lastError().text();
        delete m_db;
        m_db = nullptr;
        return false;
    }
    // WAL 模式防损坏（执行书 G-2 边界）
    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral("PRAGMA journal_mode=WAL")))
        qWarning().noquote() << "DataLogger: WAL 模式设置失败" << q.lastError().text();
    if (!q.exec(QStringLiteral("PRAGMA synchronous=NORMAL")))
        qWarning().noquote() << "DataLogger: synchronous=NORMAL 设置失败" << q.lastError().text();
    // 建表
    QSqlQuery createTag(*m_db);
    if (!createTag.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tag_history ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " ts TEXT NOT NULL,"
        " tag_name TEXT NOT NULL,"
        " value TEXT NOT NULL)")))
        qWarning().noquote() << "DataLogger: tag_history 建表失败" << createTag.lastError().text();
    if (!createTag.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tag_name ON tag_history(tag_name, ts)")))
        qWarning().noquote() << "DataLogger: idx_tag_name 建索引失败" << createTag.lastError().text();
    QSqlQuery createAlarm(*m_db);
    if (!createAlarm.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS alarm_history ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " ts TEXT NOT NULL,"
        " rule_name TEXT NOT NULL,"
        " tag_name TEXT,"
        " level INTEGER DEFAULT 0,"
        " message TEXT,"
        " event TEXT NOT NULL)")))   // TRIGGER/ACK/CLEAR
        qWarning().noquote() << "DataLogger: alarm_history 建表失败" << createAlarm.lastError().text();
    if (!createAlarm.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_ts ON alarm_history(ts)")))
        qWarning().noquote() << "DataLogger: idx_alarm_ts 建索引失败" << createAlarm.lastError().text();
    qInfo().noquote() << "DataLogger: SQLite 就绪" << path;
    return true;
}

void DataLogger::recordAlarmEvent(const QString& ruleName, const QString& tagName, int level,
                                  const QString& message, const QString& event)
{
    if (!openDb())
        return;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("INSERT INTO alarm_history (ts, rule_name, tag_name, level, message, event)"
                             " VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.addBindValue(ruleName);
    q.addBindValue(tagName);
    q.addBindValue(level);
    q.addBindValue(message);
    q.addBindValue(event);
    if (!q.exec())
        qWarning().noquote() << "DataLogger: alarm_history 写入失败" << q.lastError().text();
}

void DataLogger::sample()
{
    if (!m_dataManager || m_project.tags.isEmpty() || !openDb())
        return;
    // 事务批量写（审查 M10: 30s 一次, 全部变量采样——采样量 = 变量数×2条/分钟, 50 万条约 8.7 天, cleanup 兜底）
    QSqlQuery q(*m_db);
    if (!m_db->transaction()) {
        qWarning().noquote() << "DataLogger: 事务开始失败" << m_db->lastError().text();
        return;
    }
    q.prepare(QStringLiteral("INSERT INTO tag_history (ts, tag_name, value) VALUES (?, ?, ?)"));
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    for (const auto& tag : m_project.tags) {
        const QVariant v = m_dataManager->value(tag.name);
        if (!v.isValid() || v.isNull())
            continue;
        q.addBindValue(ts);
        q.addBindValue(tag.name);
        q.addBindValue(v.toString());
        if (!q.exec()) {
            qWarning().noquote() << "DataLogger: tag_history 写入失败" << q.lastError().text();
            m_db->rollback();
            return;
        }
    }
    if (!m_db->commit())
        qWarning().noquote() << "DataLogger: 事务提交失败" << m_db->lastError().text();
}

void DataLogger::cleanup()
{
    if (!openDb())
        return;
    // 保留策略: 7 天或 50 万条先到（执行书 G-2 边界）
    QSqlQuery delOld(*m_db);
    const QString cutoff = QDateTime::currentDateTime().addDays(-7)
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    delOld.prepare(QStringLiteral("DELETE FROM tag_history WHERE ts < ?"));
    delOld.addBindValue(cutoff);
    if (!delOld.exec())
        qWarning().noquote() << "DataLogger: tag_history 过期清理失败" << delOld.lastError().text();
    delOld.prepare(QStringLiteral("DELETE FROM alarm_history WHERE ts < ?"));
    delOld.addBindValue(cutoff);
    if (!delOld.exec())
        qWarning().noquote() << "DataLogger: alarm_history 过期清理失败" << delOld.lastError().text();
    QSqlQuery cap(*m_db);
    if (!cap.exec(QStringLiteral("DELETE FROM tag_history WHERE id NOT IN"
                            " (SELECT id FROM tag_history ORDER BY id DESC LIMIT 500000)")))
        qWarning().noquote() << "DataLogger: tag_history 条数上限清理失败" << cap.lastError().text();
    if (!cap.exec(QStringLiteral("DELETE FROM alarm_history WHERE id NOT IN"
                            " (SELECT id FROM alarm_history ORDER BY id DESC LIMIT 500000)")))
        qWarning().noquote() << "DataLogger: alarm_history 条数上限清理失败" << cap.lastError().text();
    qInfo().noquote() << "DataLogger: 保留策略清理完成（7 天 / 50 万条）";
}

QVariantList DataLogger::queryTagHistory(const QString& tagName, int limit)
{
    QVariantList list;
    if (!openDb())
        return list;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("SELECT ts, value FROM tag_history WHERE tag_name = ?"
                             " ORDER BY id DESC LIMIT ?"));
    q.addBindValue(tagName);
    q.addBindValue(limit);
    if (!q.exec())
        return list;
    while (q.next()) {
        QVariantMap m;
        m.insert("ts", q.value(0));
        m.insert("value", q.value(1));
        list.prepend(m);   // 时间正序
    }
    return list;
}

QVariantList DataLogger::queryAlarmHistory(int limit)
{
    QVariantList list;
    if (!openDb())
        return list;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("SELECT ts, rule_name, tag_name, level, message, event"
                             " FROM alarm_history ORDER BY id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return list;
    while (q.next()) {
        QVariantMap m;
        m.insert("ts", q.value(0));
        m.insert("rule_name", q.value(1));
        m.insert("tag_name", q.value(2));
        m.insert("level", q.value(3));
        m.insert("message", q.value(4));
        m.insert("event", q.value(5));
        list.append(m);
    }
    return list;
}

} // namespace navihmi
