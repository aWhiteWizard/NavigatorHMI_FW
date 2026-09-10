/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\objectmanager.h
 * @Description: 对象管理器（架构三件套之一，G-0 新增）——全局对象注册表（画面/控件实例 + 系统对象）
 *               跨画面寻址 screenName.objectName；暴露可读属性（映射 QML 属性）
 *               分层对照（om/objectmanager 层）：数据中枢，UI/脚本/报警/通信统一经此访问
 */
#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariant>

namespace navihmi {

class ObjectManager : public QObject
{
    Q_OBJECT
public:
    explicit ObjectManager(QObject* parent = nullptr);

    // ── 系统对象注册（全局服务：dataManager/runtimeBus/后续 usersystem/alarmengine/datalogger）──
    void registerSystemObject(const QString& name, QObject* obj);
    QObject* systemObject(const QString& name) const;
    Q_INVOKABLE QStringList systemObjectNames() const;

    // ── 控件对象注册（QML 画面加载/销毁时调用；screenName.objectName 跨画面寻址）──
    Q_INVOKABLE void registerObject(const QString& screenName, const QString& objectName, QObject* obj);
    Q_INVOKABLE void unregisterObject(const QString& screenName, const QString& objectName);
    /// 整画面注销（Loader 卸载/Stop Runtime 时清理，防幽灵寻址）
    Q_INVOKABLE void unregisterScreen(const QString& screenName);
    /// 清空全部画面控件注册（工程重载 projectReplaced / Stop Runtime 兜底，防旧工程幽灵寻址）
    Q_INVOKABLE void clearScreens();

    // ── 跨画面寻址 ──
    /// 按画面+控件名寻址（screenName 空 = 当前画面）
    Q_INVOKABLE QObject* findObject(const QString& screenName, const QString& objectName) const;
    /// 按完整路径寻址 "screenName.objectName"（无点 = 当前画面内）
    Q_INVOKABLE QObject* findObjectByPath(const QString& path) const;

    // ── 属性读写（set_property 动作落点；暴露可读属性映射 QML 属性）──
    Q_INVOKABLE QVariant getProperty(const QString& screenName, const QString& objectName, const QString& key) const;
    Q_INVOKABLE bool setProperty(const QString& screenName, const QString& objectName, const QString& key, const QVariant& value);

    // ── 当前画面（main.qml 切换时同步；空 screenName 寻址的默认上下文）──
    Q_INVOKABLE void setCurrentScreen(const QString& name);
    Q_INVOKABLE QString currentScreen() const;

    /// 注册表快照（日志/CLI 验证跨画面寻址）
    Q_INVOKABLE QString dump() const;

signals:
    void objectRegistered(const QString& path);
    void objectUnregistered(const QString& path);

private:
    QHash<QString, QObject*> m_systemObjects;                  // 系统对象注册表
    QHash<QString, QHash<QString, QObject*>> m_screenObjects;  // screenName -> {objectName -> obj}
    QString m_currentScreen;
};

} // namespace navihmi
