/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\objectmanager.cpp
 * @Description: 对象管理器实现——注册表 + 跨画面寻址 + 属性读写
 */
#include "runtime/objectmanager.h"

#include <QDebug>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QTextStream>

namespace navihmi {

// 审查 M-1(2026-08-23 G-0 复审): PC 端 set_property 属性键名（EventConfigViewModel.SetPropertyKeys）
// 与 FW Hmi* 组件属性名不一致——建映射表对齐（CONFLICT_SOFT: FW 侧映射, 不改 PC 端契约）
// 未映射且组件无此声明的键 → 不落动态属性（原 N3 兜底创建动态属性对渲染无效且难排查, 取消）
const QHash<QString, QString>& propertyKeyMap()
{
    static const QHash<QString, QString> m = {
        { "backColor",    "fillColor" },        // PC: 背景色 → FW: 填充色
        { "foreColor",    "textColor" },        // PC: 前景色 → FW: 文字色
        { "borderColor",  "strokeColor" },      // PC: 边框色 → FW: 描边色
        { "borderWidth",  "strokeThickness" },  // PC: 边框宽 → FW: 描边宽
        { "minValue",     "min" },              // PC: 最小值 → FW: min
        { "maxValue",     "max" },              // PC: 最大值 → FW: max
        // visible/enabled 为 QML Item 通用属性, 直接可用无需映射
        // vAlign/stepValue/unit/inputType/switchOnColor/switchOffColor/frameIndex/repeat/base_value
        // FW 组件无对应声明属性 → 映射后仍找不到则拒绝（告警由调用方 RuntimeBus 打）
    };
    return m;
}

ObjectManager::ObjectManager(QObject* parent)
    : QObject(parent)
{
}

// ── 系统对象 ──

void ObjectManager::registerSystemObject(const QString& name, QObject* obj)
{
    if (!obj || name.isEmpty())
        return;
    m_systemObjects.insert(name, obj);
    qInfo().noquote() << "ObjectManager: 系统对象注册" << name;
}

QObject* ObjectManager::systemObject(const QString& name) const
{
    return m_systemObjects.value(name, nullptr);
}

QStringList ObjectManager::systemObjectNames() const
{
    return m_systemObjects.keys();
}

// ── 控件对象注册 ──

void ObjectManager::registerObject(const QString& screenName, const QString& objectName, QObject* obj)
{
    if (!obj || screenName.isEmpty() || objectName.isEmpty())
        return;
    // 同路径重复注册（画面重载/切换往返）直接覆盖，不发冗余信号
    auto& screen = m_screenObjects[screenName];
    const QString path = screenName + "." + objectName;
    // 审查 N-1(复审): 存活期内替换不同实例 = 组态端重名寻址歧义——告警（同一实例重注册不告警）
    if (screen.contains(objectName) && screen.value(objectName) != obj)
        qWarning().noquote() << "ObjectManager: 同画面重名控件覆盖" << path
                             << "(旧实例被新实例替换, 寻址以新实例为准)";
    const bool isNew = !screen.contains(objectName) || screen.value(objectName) != obj;
    screen.insert(objectName, obj);
    if (isNew) {
        // TraceLog: 注册事件可查（审查 N6: 默认关——大工程数百控件启动刷屏; NAVIHMI_TRACE=1 开启）
        const bool trace = qEnvironmentVariableIntValue("NAVIHMI_TRACE") != 0;
        if (trace)
            qInfo().noquote() << "[TRACE] ObjectManager 注册" << path;
        emit objectRegistered(path);
    }
}

void ObjectManager::unregisterObject(const QString& screenName, const QString& objectName)
{
    auto it = m_screenObjects.find(screenName);
    if (it == m_screenObjects.end())
        return;
    if (it->remove(objectName) > 0)
        emit objectUnregistered(screenName + "." + objectName);
    if (it->isEmpty())
        m_screenObjects.erase(it);
}

void ObjectManager::unregisterScreen(const QString& screenName)
{
    auto it = m_screenObjects.find(screenName);
    if (it == m_screenObjects.end())
        return;
    const auto names = it->keys();
    m_screenObjects.erase(it);
    for (const auto& n : names)
        emit objectUnregistered(screenName + "." + n);
}

void ObjectManager::clearScreens()
{
    // 先收集全部路径发注销信号，再清空注册表
    QStringList paths;
    for (auto it = m_screenObjects.constBegin(); it != m_screenObjects.constEnd(); ++it) {
        const QString sc = it.key();
        for (const auto& n : it->keys())
            paths.append(sc + "." + n);
    }
    m_screenObjects.clear();
    for (const auto& path : paths)
        emit objectUnregistered(path);
    qInfo().noquote() << "ObjectManager: 画面控件注册表已清空" << paths.size() << "项";
}

// ── 跨画面寻址 ──

QObject* ObjectManager::findObject(const QString& screenName, const QString& objectName) const
{
    if (objectName.isEmpty())
        return nullptr;
    // screenName 空 = 当前画面
    const QString sc = screenName.isEmpty() ? m_currentScreen : screenName;
    const auto it = m_screenObjects.constFind(sc);
    if (it == m_screenObjects.constEnd())
        return nullptr;
    return it->value(objectName, nullptr);
}

QObject* ObjectManager::findObjectByPath(const QString& path) const
{
    if (path.isEmpty())
        return nullptr;
    // "screenName.objectName"；无点 = 当前画面内寻址
    const int dot = path.indexOf(QLatin1Char('.'));
    if (dot <= 0)
        return findObject(QString(), path);
    return findObject(path.left(dot), path.mid(dot + 1));
}

// ── 属性读写 ──

QVariant ObjectManager::getProperty(const QString& screenName, const QString& objectName, const QString& key) const
{
    QObject* obj = findObject(screenName, objectName);
    if (!obj || key.isEmpty())
        return QVariant();
    // QML 属性经 meta 系统可读（Q_PROPERTY / QML property 均可）
    const QMetaObject* mo = obj->metaObject();
    const int idx = mo->indexOfProperty(key.toUtf8().constData());
    if (idx >= 0) {
        const QMetaProperty mp = mo->property(idx);
        if (mp.isReadable())
            return mp.read(obj);
    }
    return obj->property(key.toUtf8().constData());
}

bool ObjectManager::setProperty(const QString& screenName, const QString& objectName, const QString& key, const QVariant& value)
{
    QObject* obj = findObject(screenName, objectName);
    if (!obj || key.isEmpty())
        return false;
    // 审查 M-1: PC 键名 → FW 属性名映射（visible/enabled 等通用属性直接可用）
    const QString fwKey = propertyKeyMap().value(key, key);
    const QByteArray keyBytes = fwKey.toUtf8();
    const QMetaObject* mo = obj->metaObject();
    const int idx = mo->indexOfProperty(keyBytes.constData());
    if (idx >= 0) {
        const QMetaProperty mp = mo->property(idx);
        if (mp.isWritable()) {
            // QVariant 按属性类型转换（字符串 "true"/数字串 → 目标类型）
            if (!mp.write(obj, value)) {
                // 转换失败时尝试字符串显式转换（如 bool 属性收 "true"/"false"）
                if (value.userType() == QMetaType::QString && mp.typeId() == QMetaType::Bool) {
                    const QString s = value.toString().toLower();
                    if (s == "true" || s == "1")
                        return mp.write(obj, true);
                    if (s == "false" || s == "0")
                        return mp.write(obj, false);
                }
                return false;   // 类型不匹配, 静默失败（调用方 RuntimeBus 统一告警）
            }
            return true;
        }
        return false;   // 只读属性拒绝写入（审查 N-2: 不落动态属性）
    }
    // 审查 N-3(复审)/M-1: 未知键/无对应属性——拒绝写入动态属性（原兜底创建动态属性
    // 对渲染无效果且难排查）。告警由调用方 RuntimeBus 统一打（带完整上下文）
    return false;
}

// ── 当前画面 ──

void ObjectManager::setCurrentScreen(const QString& name)
{
    m_currentScreen = name;
}

QString ObjectManager::currentScreen() const
{
    return m_currentScreen;
}

// ── 注册表快照 ──

QString ObjectManager::dump() const
{
    QString out;
    QTextStream ts(&out);
    ts << "ObjectManager 注册表: 系统对象 " << m_systemObjects.size()
       << " 画面 " << m_screenObjects.size()
       << " 当前画面=" << (m_currentScreen.isEmpty() ? "(无)" : m_currentScreen) << "\n";
    for (auto it = m_screenObjects.constBegin(); it != m_screenObjects.constEnd(); ++it) {
        ts << "  [" << it.key() << "] " << it->size() << " 控件:";
        for (const auto& n : it->keys())
            ts << " " << n;
        ts << "\n";
    }
    return out;
}

} // namespace navihmi
