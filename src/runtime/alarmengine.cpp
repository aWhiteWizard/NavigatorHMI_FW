/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\alarmengine.cpp
 * @Description: 报警引擎实现——定时轮询变量阈值触发/清除活动报警
 */
#include "runtime/alarmengine.h"
#include "runtime/datamanager.h"

#include <QTimer>
#include <QDateTime>
#include <QDebug>

namespace navihmi {

AlarmEngine::AlarmEngine(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(500);   // 500ms 轮询变量值（模拟采集）
    connect(m_timer, &QTimer::timeout, this, &AlarmEngine::poll);
    m_timer->start();
}

void AlarmEngine::setProject(const Project& proj)
{
    m_project = proj;
    m_active.clear();
    m_triggered.clear();
    m_triggeredTime.clear();
    emit alarmsChanged();
}

void AlarmEngine::setDataManager(DataManager* dm)
{
    m_dataManager = dm;
}

QVariantList AlarmEngine::activeAlarms() const
{
    QVariantList list;
    // 时间倒序（最新在前）
    for (int i = m_active.size() - 1; i >= 0; --i) {
        const auto& a = m_active[i];
        QVariantMap m;
        m.insert("id", a.id);
        m.insert("time", a.time);
        m.insert("level", a.level);
        m.insert("message", a.message);
        m.insert("acked", a.acked);
        m.insert("tag", a.tag);
        list.append(m);
    }
    return list;
}

int AlarmEngine::activeCount() const
{
    // 活动报警数 = 未确认数（历史已确认不计入标题）
    int n = 0;
    for (const auto& a : m_active)
        if (!a.acked) ++n;
    return n;
}

void AlarmEngine::ackAlarm(const QString& id)
{
    for (int i = 0; i < m_active.size(); ++i)
        if (m_active[i].id == id) {
            const ActiveAlarm a = m_active[i];
            m_active.removeAt(i);   // 审查 M2: 确认即从活动列表移除（历史在 G-2 DB）
            emit alarmsChanged();
            qInfo().noquote() << "AlarmEngine: 确认报警" << id;
            emit alarmAcked(a.id, a.tag, a.level, a.message);
            return;
        }
}

void AlarmEngine::ackAlarms(const QVariantList& ids)
{
    bool changed = false;
    for (const auto& v : ids) {
        const QString id = v.toString();
        for (int i = 0; i < m_active.size(); ++i)
            if (m_active[i].id == id) {
                const ActiveAlarm a = m_active[i];
                m_active.removeAt(i);
                changed = true;
                // 审查 M1: 批量确认逐条发 ACK（审计完整）
                emit alarmAcked(a.id, a.tag, a.level, a.message);
                break;
            }
    }
    if (changed)
        emit alarmsChanged();
}

void AlarmEngine::ackAll()
{
    bool changed = false;
    while (!m_active.isEmpty()) {
        const ActiveAlarm a = m_active.takeFirst();
        changed = true;
        // 审查 M1: 全部确认逐条发 ACK（审计完整）
        emit alarmAcked(a.id, a.tag, a.level, a.message);
    }
    if (changed)
        emit alarmsChanged();
}

void AlarmEngine::poll()
{
    if (!m_dataManager || m_project.alarms.isEmpty())
        return;
    bool changed = false;
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    for (const auto& rule : m_project.alarms) {
        if (rule.tagName.isEmpty())
            continue;
        const QVariant v = m_dataManager->value(rule.tagName);
        if (!v.isValid() || v.isNull())
            continue;
        const double value = v.toDouble();
        const bool inAlarm = rule.type == AlarmType::High ? value > rule.threshold
                           : rule.type == AlarmType::Low  ? value < rule.threshold
                           : false;   // RateChange/Deviation 模拟源不处理（首版范围）
        const bool wasTriggered = m_triggered.value(rule.name, false);

        if (inAlarm && !wasTriggered) {
            // 触发
            ActiveAlarm a;
            a.id = rule.name;
            a.time = now;
            a.level = int(rule.level);
            a.message = rule.message.isEmpty() ? rule.name : rule.message;
            a.tag = rule.tagName;
            m_active.append(a);
            m_triggered.insert(rule.name, true);
            m_triggeredTime.insert(rule.name, now);
            changed = true;
            qInfo().noquote() << "AlarmEngine: 触发报警" << rule.name
                              << "tag=" << rule.tagName << "值=" << value
                              << "阈值=" << rule.threshold;
            emit alarmTriggered(rule.name, rule.tagName, int(rule.level),
                                rule.message.isEmpty() ? rule.name : rule.message);
        } else if (!inAlarm && wasTriggered) {
            // 清除（审查 M2: 活动列表只含未确认报警——恢复即从列表移除, 历史由 G-2 alarm_history 承载;
            // 审查 M1: 已确认/未确认的恢复均发 CLEAR——完整生命周期 TRIGGER→ACK→CLEAR）
            m_triggered.insert(rule.name, false);
            QString msg = rule.message.isEmpty() ? rule.name : rule.message;
            int lv = int(rule.level);
            QString tg = rule.tagName;
            for (int i = 0; i < m_active.size(); ++i) {
                if (m_active[i].id == rule.name) {
                    msg = m_active[i].message;
                    lv = m_active[i].level;
                    tg = m_active[i].tag;
                    m_active.removeAt(i);
                    break;
                }
            }
            changed = true;
            qInfo().noquote() << "AlarmEngine: 报警恢复" << rule.name;
            emit alarmCleared(rule.name, tg, lv, msg);
        }
    }
    if (changed)
        emit alarmsChanged();
}

} // namespace navihmi
