/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\usersystem.h
 * @Description: 用户系统（G-1a）——登录/注销/权限判定/改密改名改组
 *               PC 端 UserModels.cs 契约一一对应（SHA256 大写 Hex / 权限 4 项 / 预置三组）
 *               设备端管理写本地存储（JSON, 不写回工程, 下次启动生效——DESIGN-WINDOWS L151）
 */
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QDateTime>
#include "runtime/projectmodel.h"

namespace navihmi {

class UserSystem : public QObject
{
    Q_OBJECT
    // G-1a 修复(2026-08-23): QML 绑定必须用 Q_PROPERTY(带 NOTIFY) 才有依赖跟踪——
    // 直接调用 isLoggedIn() 等方法无依赖, userChanged 后界面不刷新（登录成功但 UI 不动）
    Q_PROPERTY(bool loggedIn READ isLoggedIn NOTIFY userChanged)
    Q_PROPERTY(QString userName READ currentUserName NOTIFY userChanged)
    Q_PROPERTY(QString group READ currentGroup NOTIFY userChanged)
    Q_PROPERTY(bool canManage READ canManage NOTIFY userChanged)
    Q_PROPERTY(bool mustChangePassword READ mustChangePassword NOTIFY userChanged)
public:
    explicit UserSystem(QObject* parent = nullptr);

    /// 工程加载时注入用户/组/安全配置（main.cpp loadAndInject 调用；含初始管理员兜底）
    void setProject(const Project& proj);

    // ── 登录/注销 ──
    /// 登录：成功返回空串, 失败返回错误信息（用户名/密码错误 / 锁定中 / 密码策略未满足）
    Q_INVOKABLE QString login(const QString& userName, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool isLoggedIn() const;
    Q_INVOKABLE QString currentUserName() const;
    Q_INVOKABLE QString currentGroup() const;
    /// 当前登录用户组是否含权限（UserPermission 枚举值 0-3）
    Q_INVOKABLE bool hasPermission(int perm) const;

    // ── 用户管理（设备端 ⚙ 弹窗；需 UserManage 权限）──
    Q_INVOKABLE bool canManage() const;
    /// 当前登录用户是否初始兜底 admin（H-3: 默认 admin 禁改自己的组, 防把唯一管理员降权锁死）
    Q_INVOKABLE bool isDefaultAdmin() const;
    Q_INVOKABLE QStringList userNames() const;
    Q_INVOKABLE QStringList groupNames() const;
    /// 查询指定用户的所属组（管理弹窗预填用；不存在返回空串）
    Q_INVOKABLE QString groupOf(const QString& userName) const;
    /// 改用户名/密码/组（留空=不改）；成功返回空串
    Q_INVOKABLE QString updateUser(const QString& userName, const QString& newName,
                                   const QString& newPassword, const QString& newGroup);
    /// 当前用户是否须强制改密（初始管理员首次登录）
    Q_INVOKABLE bool mustChangePassword() const;

    // ── 本地持久化（设备端改密/改名/改组覆盖工程配置, 下次启动生效）──
    Q_INVOKABLE QString storagePath() const;

signals:
    /// 登录/注销后触发（UserView onUserChanged 事件）
    void userChanged(const QString& userName);

private:
    static QString sha256Hex(const QString& s);
    const UserAccount* findAccount(const QString& userName) const;
    QString passwordPolicyError(const QString& password) const;
    void loadPersisted();
    void persist();

    Project m_project;
    QString m_currentUser;
    QString m_defaultAdminName;    // H-3: 初始兜底 admin 用户名（改名时跟随）——禁改自己的组
    // 设备端本地持久化覆盖（工程 users 之上；key=用户当前名, oldName=改名前的名字）
    // 审查 B1(2026-08-23 G-1a): setProject 时合并回 m_project.users——否则"只写不读",
    // 重启后改密/改组/改名全部失效且旧密码仍可登录
    struct Override { QString hash; QString group; bool mustChange = false; QString oldName; };
    QHash<QString, Override> m_overrides;
    int m_failedAttempts = 0;        // 连续失败计数（security.failed_login_lockout 阈值）
    QDateTime m_lockUntil;           // 锁定截止（security.lock_minutes）
};

} // namespace navihmi
