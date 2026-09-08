/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\alarmengine.cpp
 * @Description: 报警引擎实现——定时轮询变量阈值触发/清除活动报警
 */
#include "runtime/alarmengine.h"
#include "runtime/datamanager.h"

#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <cmath>

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
    m_geoAlerted.clear();        // X-6: 工程重载清几何告警状态（新工程范围/作业点重新判定）
    m_geoMessage.clear();
    m_geoLevel.clear();
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
    if (!m_dataManager)
        return;
    geoCheck();   // X-6: 几何范围检测独立于阈值报警（无 alarm 规则也执行——作业点出范围告警）
    if (m_project.alarms.isEmpty())
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

// ── X-6（2026-09-08 用户规格）：作业点出作业范围几何告警（走报警体系 TRIGGER/CLEAR；含固定位置作业点）──

void AlarmEngine::raiseGeoAlarm(const QString& key, int level, const QString& message)
{
    if (m_geoAlerted.contains(key))
        return;   // 已触发（出范围期间不重复刷——回范围 clearGeoAlarm 后再次出范围再触发）
    m_geoAlerted.insert(key);
    m_geoMessage.insert(key, message);
    m_geoLevel.insert(key, level);
    ActiveAlarm a;
    a.id = QStringLiteral("geo-") + key;
    a.time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    a.level = level;
    a.message = message;
    a.tag = QString();
    a.priority = 0;
    a.acked = false;
    m_active.append(a);
    m_triggered.insert(a.id, true);
    emit alarmsChanged();
    emit alarmTriggered(a.id, QString(), level, message);   // DataLogger TRIGGER 联动
    qInfo().noquote() << "AlarmEngine: 作业点出范围" << message;
}

void AlarmEngine::clearGeoAlarm(const QString& key)
{
    if (!m_geoAlerted.contains(key))
        return;
    m_geoAlerted.remove(key);
    const QString msg = m_geoMessage.take(key);
    const int lv = m_geoLevel.take(key);
    const QString id = QStringLiteral("geo-") + key;
    for (int i = 0; i < m_active.size(); ++i)
        if (m_active[i].id == id) {
            m_active.removeAt(i);
            break;
        }
    m_triggered.remove(id);
    emit alarmsChanged();
    emit alarmCleared(id, QString(), lv, msg);   // DataLogger CLEAR 联动
    qInfo().noquote() << "AlarmEngine: 作业点回范围" << msg;
}

void AlarmEngine::geoCheck()
{
    const auto& wm = m_project.worldMap;
    if (!m_dataManager || wm.workRangePoints.size() < 3 || wm.workPoints.isEmpty())
        return;
    // 范围多边形顶点（范围点绑 GPS 变量 → 动态值；未绑 → fixedPoint）；任一解析失败/未配置 (0,0) → 本 tick 不检测
    QList<GeoPoint> poly;
    for (const auto& rp : wm.workRangePoints) {
        GeoPoint g = rp.fixedPoint;
        if (!rp.boundTag.isEmpty() && m_dataManager->hasTag(rp.boundTag)) {
            const QString val = m_dataManager->value(rp.boundTag).toString().trimmed();
            QString clean = val;
            if (clean.startsWith(QLatin1Char('('))) clean = clean.mid(1);
            if (clean.endsWith(QLatin1Char(')'))) clean.chop(1);
            const QStringList parts = clean.split(QLatin1Char(','));
            if (parts.size() >= 2) {
                const double lng = coordToDec(parts.at(0));
                const double lat = coordToDec(parts.at(1));
                if (!std::isnan(lng) && !std::isnan(lat)) { g.longitude = lng; g.latitude = lat; }
            }
        }
        if (g.longitude == 0 && g.latitude == 0)
            return;   // 未配置坐标（对齐 HmiWorldMap (0,0)/NaN 守卫）——本 tick 不检测
        poly.append(g);
    }
    for (const auto& wp : wm.workPoints) {
        GeoPoint g = wp.fixedPoint;
        if (!wp.boundTag.isEmpty() && m_dataManager->hasTag(wp.boundTag)) {
            const QString val = m_dataManager->value(wp.boundTag).toString().trimmed();
            QString clean = val;
            if (clean.startsWith(QLatin1Char('('))) clean = clean.mid(1);
            if (clean.endsWith(QLatin1Char(')'))) clean.chop(1);
            const QStringList parts = clean.split(QLatin1Char(','));
            if (parts.size() >= 2) {
                const double lng = coordToDec(parts.at(0));
                const double lat = coordToDec(parts.at(1));
                if (!std::isnan(lng) && !std::isnan(lat)) { g.longitude = lng; g.latitude = lat; }
            }
        }
        if (g.longitude == 0 && g.latitude == 0)
            continue;   // 未配置坐标（固定点未设）跳过
        const bool in = pointInPolygon(g.longitude, g.latitude, poly);
        const QString key = wp.name.isEmpty() ? QStringLiteral("作业点") : wp.name;
        if (!in && !m_geoAlerted.contains(key))
            raiseGeoAlarm(key, 1 /* Severity 重要 */,
                          QStringLiteral("作业点 %1 超出作业范围").arg(key));
        else if (in && m_geoAlerted.contains(key))
            clearGeoAlarm(key);
        // raiseGeoAlarm/clearGeoAlarm 内部已 emit alarmsChanged（🟡 reviewer：此处不再重复 emit）
    }
}

// 点在多边形内（射线法）
bool AlarmEngine::pointInPolygon(double lng, double lat, const QList<GeoPoint>& poly)
{
    const int n = poly.size();
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const GeoPoint& a = poly.at(i);
        const GeoPoint& b = poly.at(j);
        if ((a.latitude > lat) != (b.latitude > lat)
            && lng < (b.longitude - a.longitude) * (lat - a.latitude) / (b.latitude - a.latitude) + a.longitude)
            inside = !inside;
    }
    return inside;
}

// DMS/十进制坐标解析（对齐 QML HmiWorldMap.dmsToDec：DMS "E104°3'30\"" / 十进制 "104.1423"；W/S 为负）
double AlarmEngine::coordToDec(const QString& raw)
{
    QString s = raw.trimmed();
    const bool neg = s.contains(QLatin1Char('W')) || s.contains(QLatin1Char('S'));
    QRegularExpression re(QStringLiteral("([0-9.]+)°([0-9.]+)'([0-9.]+)\""));
    const auto m = re.match(s);
    double v;
    if (m.hasMatch())
        v = m.captured(1).toDouble() + m.captured(2).toDouble() / 60.0 + m.captured(3).toDouble() / 3600.0;
    else {
        bool ok = false;
        v = s.toDouble(&ok);
        if (!ok)
            return qQNaN();
    }
    return neg ? -v : v;
}

} // namespace navihmi
