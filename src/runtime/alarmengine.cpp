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
    m_overThresholdMs.clear();   // 审查 MINOR-2: 工程重载清 delayMs 计时（防同名规则残留旧时间戳）
    m_manualLastMs.clear();      // J-1 审查修复: 工程重载清手动报警去重（防旧消息抑制新报警）
    emit alarmsChanged();
}

void AlarmEngine::setDataManager(DataManager* dm)
{
    m_dataManager = dm;
}

QVariantList AlarmEngine::activeAlarms() const
{
    QVariantList list;
    // H-5(M9): 时间倒序为主, 同时间按 priority 降序（DESIGN: 报警排序=时间倒序, 同时间按优先级）
    QList<ActiveAlarm> sorted = m_active;
    std::stable_sort(sorted.begin(), sorted.end(), [](const ActiveAlarm& a, const ActiveAlarm& b) {
        if (a.time == b.time) return a.priority > b.priority;
        return a.time > b.time;   // HH:mm:ss 字符串倒序即时间倒序
    });
    for (const auto& a : sorted) {
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
        // H-5(M9): deadband 回差——触发后恢复阈值 = 触发阈值 ∓ deadband（防抖, DESIGN 卖点）
        const bool inRecover = rule.type == AlarmType::High
                               ? value < rule.threshold - rule.deadband
                               : rule.type == AlarmType::Low
                                 ? value > rule.threshold + rule.deadband
                                 : false;

        if (inAlarm && !wasTriggered) {
            // H-5(M9): delayMs 延迟触发——触发条件持续 delayMs 才触发（记录首次超时时间）
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 firstMs = m_overThresholdMs.value(rule.name, nowMs);
            m_overThresholdMs.insert(rule.name, firstMs);
            if (rule.delayMs > 0 && nowMs - firstMs < rule.delayMs)
                continue;   // 未到延迟, 等待下一轮
            // 触发
            ActiveAlarm a;
            a.id = rule.name;
            a.time = now;
            a.level = int(rule.level);
            a.message = rule.message.isEmpty() ? rule.name : rule.message;
            a.tag = rule.tagName;
            a.priority = rule.priority;
            // 复审 MAJOR-3 修复(2026-08-24): 免确认置位必须在 append **之前**——
            // m_active.append(a) 是值拷贝, append 后再改 a.acked 只改局部副本, 列表项仍未确认（伪修复）
            const bool autoAck = !rule.ackRequired || rule.category == AlarmCategory::System;
            a.acked = autoAck;
            m_active.append(a);
            m_triggered.insert(rule.name, true);
            m_triggeredTime.insert(rule.name, now);
            m_overThresholdMs.remove(rule.name);
            changed = true;
            qInfo().noquote() << "AlarmEngine: 触发报警" << rule.name
                              << "tag=" << rule.tagName << "值=" << value
                              << "阈值=" << rule.threshold;
            emit alarmTriggered(rule.name, rule.tagName, int(rule.level),
                                rule.message.isEmpty() ? rule.name : rule.message);
            // H-5(M9): ackRequired=false 或 System 类别 → 免确认（触发即确认, 审计记录）
            if (autoAck)
                emit alarmAcked(a.id, a.tag, a.level, a.message);
        } else if (!inAlarm && wasTriggered) {
            // H-5(M9): deadband 恢复判定——deadband>0 时值须回到恢复阈值才清除,
            // 否则 deadband 内波动保持触发（防抖）
            if (rule.deadband > 0) {
                if (inRecover) {
                    changed = doClear(rule, value) || changed;
                }
            } else {
                changed = doClear(rule, value) || changed;
            }
        } else if (!inAlarm && !wasTriggered) {
            m_overThresholdMs.remove(rule.name);   // 未超阈值, 清延迟计时
        }
    }
    if (changed)
        emit alarmsChanged();
}

// H-5: 报警恢复统一处理（deadband 达标后: 移出活动列表 + CLEAR 审计）；返回是否发生变化
bool AlarmEngine::doClear(const AlarmRule& rule, double value)
{
    if (!m_triggered.value(rule.name, false))
        return false;
    m_triggered.insert(rule.name, false);
    m_overThresholdMs.remove(rule.name);
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
    qInfo().noquote() << "AlarmEngine: 报警恢复" << rule.name
                      << "值=" << value;
    emit alarmCleared(rule.name, tg, lv, msg);
    return true;
}

void AlarmEngine::raiseManualAlarm(int level, const QString& message)
{
    // 去重：同消息 10s 内不重复（操作连点/同因未生效不刷报警）
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_manualLastMs.value(message, 0) + 10000 > nowMs)
        return;
    m_manualLastMs.insert(message, nowMs);

    ActiveAlarm a;
    // J-1 审查修复: id 加静态自增序号（防同毫秒两条不同消息撞 id——ackAlarm 只删首条）
    static qint64 s_manualSeq = 0;
    a.id = QStringLiteral("manual-%1-%2").arg(nowMs).arg(++s_manualSeq);
    a.time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    a.level = level;
    a.message = message;
    a.tag = QString();   // 手动报警无绑定变量
    a.priority = 0;
    a.acked = false;
    m_active.append(a);
    m_triggered.insert(a.id, true);   // 防 poll 干扰（id 不与规则名冲突）
    emit alarmsChanged();
    emit alarmTriggered(a.id, QString(), level, message);   // DataLogger TRIGGER 联动
    qInfo().noquote() << "AlarmEngine: 手动报警" << message;
}

} // namespace navihmi
