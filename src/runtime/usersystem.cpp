/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\usersystem.cpp
 * @Description: 用户系统实现——登录校验/权限/管理/本地持久化
 */
#include "runtime/usersystem.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>

namespace navihmi {

UserSystem::UserSystem(QObject* parent)
    : QObject(parent)
{
    loadPersisted();
}

void UserSystem::setProject(const Project& proj)
{
    m_project = proj;
    m_currentUser.clear();
    // 审查 M2(2026-08-23 G-1a): 先补组（工程无组时补预置三组, 与 PC 端一致），后兜底 admin——
    // 原实现"用户空且组空"时 admin 永不创建 → 全系统无可登录账户
    if (m_project.groups.isEmpty()) {
        UserGroup g;
        g.name = QStringLiteral("管理员");
        g.permissions = { 0, 1, 2, 3 };   // ScreenEdit/AlarmAck/UserManage/SystemSettings
        m_project.groups.append(g);
        g.name = QStringLiteral("操作员");
        g.permissions = { 0, 1 };         // ScreenEdit/AlarmAck
        m_project.groups.append(g);
        g.name = QStringLiteral("访客");
        g.permissions = {};
        m_project.groups.append(g);
        qInfo().noquote() << "UserSystem: 工程无组, 补预置三组（管理员/操作员/访客）";
    }
    // 初始管理员兜底（仅用户列表为空时）：admin + 初始密码 admin + 首次强制改密
    if (m_project.users.isEmpty()) {
        // admin 必须落在含 UserManage(2) 的组, 否则无法改密解除 mustChange（审查 M2）
        QString mgmtGroup;
        for (const auto& g : m_project.groups)
            if (g.permissions.contains(2)) { mgmtGroup = g.name; break; }
        if (mgmtGroup.isEmpty()) {
            UserGroup g;
            g.name = QStringLiteral("管理员");
            g.permissions = { 0, 1, 2, 3 };
            m_project.groups.prepend(g);
            mgmtGroup = g.name;
            qInfo().noquote() << "UserSystem: 补管理员组（含 UserManage 权限）";
        }
        UserAccount admin;
        admin.userName = QStringLiteral("admin");
        admin.passwordHash = sha256Hex(QStringLiteral("admin"));
        admin.groupName = mgmtGroup;
        admin.mustChangePassword = true;
        m_project.users.append(admin);
        qInfo().noquote() << "UserSystem: 初始管理员兜底 admin（首次登录强制改密）";
    }
    // 审查 B1(2026-08-23 G-1a): 本地持久化覆盖合并回内存模型——否则"只写不读",
    // 重启后改密/改组/改名全部失效、旧密码仍可登录（DESIGN-WINDOWS L151「下次启动生效」）
    if (!m_overrides.isEmpty()) {
        // 第一遍：按 key（当前名）合并 + 裁剪工程中已不存在的覆盖
        QStringList staleKeys;
        for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it) {
            bool found = false;
            for (auto& u : m_project.users) {
                if (u.userName == it.key()) {
                    if (!it.value().hash.isEmpty()) u.passwordHash = it.value().hash;
                    if (!it.value().group.isEmpty()) u.groupName = it.value().group;
                    u.mustChangePassword = it.value().mustChange;
                    found = true;
                    break;
                }
            }
            if (!found && it.value().oldName.isEmpty())
                staleKeys.append(it.key());   // 无 oldName 且工程无此用户 → 陈旧覆盖, 清理
        }
        for (const auto& k : staleKeys)
            m_overrides.remove(k);
        // 第二遍：改名覆盖（oldName 命中工程基址用户 → 改名 + 应用覆盖）
        for (auto it = m_overrides.begin(); it != m_overrides.end();) {
            if (!it.value().oldName.isEmpty()) {
                bool renamed = false;
                for (auto& u : m_project.users) {
                    if (u.userName == it.value().oldName) {
                        u.userName = it.key();   // 应用新名
                        if (!it.value().hash.isEmpty()) u.passwordHash = it.value().hash;
                        if (!it.value().group.isEmpty()) u.groupName = it.value().group;
                        u.mustChangePassword = it.value().mustChange;
                        it.value().oldName.clear();   // 已应用
                        renamed = true;
                        break;
                    }
                }
                if (!renamed) { it = m_overrides.erase(it); continue; }
            }
            ++it;
        }
        qInfo().noquote() << "UserSystem: 本地覆盖已合并" << m_overrides.size() << "项";
    }
    emit userChanged(QString());
}

// ── 登录/注销 ──

QString UserSystem::login(const QString& userName, const QString& password)
{
    // 审查 MINOR(2026-08-23 G-1a): 用户名 trim（对齐 PC CLI）+ 锁定剩余分钟计算方向修复
    const QString name = userName.trimmed();
    // 失败锁定检查
    if (!m_lockUntil.isNull() && m_lockUntil > QDateTime::currentDateTime()) {
        const int leftMin = qMax(1, int(QDateTime::currentDateTime().secsTo(m_lockUntil) / 60.0));
        return QStringLiteral("账户已锁定，请 %1 分钟后再试").arg(leftMin);
    }
    const UserAccount* acc = findAccount(name);
    if (!acc) {
        // 审查 MINOR: 用户名不存在也计失败（防枚举绕过锁定）
        ++m_failedAttempts;
        if (m_project.security.failedLoginLockout > 0
            && m_failedAttempts >= m_project.security.failedLoginLockout) {
            m_lockUntil = QDateTime::currentDateTime().addSecs(m_project.security.lockMinutes * 60);
            m_failedAttempts = 0;
            return QStringLiteral("连续失败 %1 次，账户锁定 %2 分钟")
                .arg(m_project.security.failedLoginLockout).arg(m_project.security.lockMinutes);
        }
        return QStringLiteral("用户名或密码错误");
    }
    // SHA256 大写 Hex 比对（PC 端 Convert.ToHexString 大写契约）
    if (acc->passwordHash.compare(sha256Hex(password), Qt::CaseInsensitive) != 0) {
        ++m_failedAttempts;
        if (m_project.security.failedLoginLockout > 0
            && m_failedAttempts >= m_project.security.failedLoginLockout) {
            m_lockUntil = QDateTime::currentDateTime().addSecs(m_project.security.lockMinutes * 60);
            m_failedAttempts = 0;
            return QStringLiteral("连续失败 %1 次，账户锁定 %2 分钟")
                .arg(m_project.security.failedLoginLockout).arg(m_project.security.lockMinutes);
        }
        return QStringLiteral("用户名或密码错误");
    }
    m_failedAttempts = 0;
    m_lockUntil = QDateTime();
    m_currentUser = name;
    qInfo().noquote() << "UserSystem: 登录成功" << name;
    emit userChanged(name);
    return QString();
}

void UserSystem::logout()
{
    if (m_currentUser.isEmpty())
        return;
    const QString name = m_currentUser;
    m_currentUser.clear();
    qInfo().noquote() << "UserSystem: 注销" << name;
    emit userChanged(QString());
}

bool UserSystem::isLoggedIn() const
{
    return !m_currentUser.isEmpty();
}

QString UserSystem::currentUserName() const
{
    return m_currentUser;
}

QString UserSystem::currentGroup() const
{
    const UserAccount* acc = findAccount(m_currentUser);
    return acc ? acc->groupName : QString();
}

bool UserSystem::hasPermission(int perm) const
{
    const UserAccount* acc = findAccount(m_currentUser);
    if (!acc)
        return false;
    for (const auto& g : m_project.groups)
        if (g.name == acc->groupName)
            return g.permissions.contains(perm);
    return false;
}

// ── 用户管理 ──

bool UserSystem::canManage() const
{
    return hasPermission(2);   // UserManage
}

QStringList UserSystem::userNames() const
{
    QStringList names;
    for (const auto& u : m_project.users)
        names.append(u.userName);
    return names;
}

QStringList UserSystem::groupNames() const
{
    QStringList names;
    for (const auto& g : m_project.groups)
        names.append(g.name);
    return names;
}

QString UserSystem::groupOf(const QString& userName) const
{
    const UserAccount* acc = findAccount(userName);
    return acc ? acc->groupName : QString();
}

QString UserSystem::updateUser(const QString& userName, const QString& newName,
                               const QString& newPassword, const QString& newGroup)
{
    if (!canManage())
        return QStringLiteral("无用户管理权限");
    UserAccount* acc = nullptr;
    for (auto& u : m_project.users)
        if (u.userName == userName) { acc = &u; break; }
    if (!acc)
        return QStringLiteral("用户不存在: %1").arg(userName);

    // 新用户名（防重名）
    if (!newName.isEmpty() && newName != userName) {
        for (const auto& u : m_project.users)
            if (u.userName == newName)
                return QStringLiteral("用户名已存在: %1").arg(newName);
    }
    // 新密码（策略校验 + SHA256 重哈希）
    if (!newPassword.isEmpty()) {
        const QString err = passwordPolicyError(newPassword);
        if (!err.isEmpty())
            return err;
    }
    // 新组（存在性校验）
    if (!newGroup.isEmpty()) {
        bool found = false;
        for (const auto& g : m_project.groups)
            if (g.name == newGroup) { found = true; break; }
        if (!found)
            return QStringLiteral("用户组不存在: %1").arg(newGroup);
    }

    const bool wasCurrent = (userName == m_currentUser);
    const QString oldName = acc->userName;
    if (!newName.isEmpty())
        acc->userName = newName;
    if (!newPassword.isEmpty()) {
        acc->passwordHash = sha256Hex(newPassword);
        acc->mustChangePassword = false;   // 改密后解除强制改密
    }
    if (!newGroup.isEmpty())
        acc->groupName = newGroup;

    // 设备端持久化（覆盖工程配置, 下次启动生效——审查 B1: 覆盖在 setProject 时合并回内存模型）
    Override ov;
    ov.hash = acc->passwordHash;
    ov.group = acc->groupName;
    ov.mustChange = acc->mustChangePassword;
    if (!newName.isEmpty() && newName != oldName)
        ov.oldName = oldName;   // 改名跨重启对应基址用户
    m_overrides.remove(oldName);
    m_overrides.insert(acc->userName, ov);
    persist();

    if (wasCurrent) {
        m_currentUser = acc->userName;
        emit userChanged(m_currentUser);
    }
    qInfo().noquote() << "UserSystem: 更新用户" << oldName << "→" << acc->userName;
    return QString();
}

bool UserSystem::mustChangePassword() const
{
    const UserAccount* acc = findAccount(m_currentUser);
    if (!acc)
        return false;
    // 本地覆盖优先
    const auto it = m_overrides.constFind(acc->userName);
    if (it != m_overrides.constEnd())
        return it->mustChange;
    return acc->mustChangePassword;
}

// ── 本地持久化 ──

QString UserSystem::storagePath() const
{
#if defined(Q_OS_WIN)
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/navihmi_users.json");
#else
    return QStringLiteral("/mnt/user/userdata/navihmi_users.json");
#endif
}

void UserSystem::loadPersisted()
{
    QFile f(storagePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;
    const QJsonArray arr = doc.object().value(QStringLiteral("users")).toArray();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        Override ov;
        ov.hash = o.value(QStringLiteral("hash")).toString();
        ov.group = o.value(QStringLiteral("group")).toString();
        ov.mustChange = o.value(QStringLiteral("must_change")).toBool();
        ov.oldName = o.value(QStringLiteral("old_name")).toString();
        m_overrides.insert(o.value(QStringLiteral("name")).toString(), ov);
    }
}

void UserSystem::persist()
{
    // 审查 MINOR(2026-08-23 G-1a): 原子写（tmp+rename 防断电损坏）+ 失败日志
    const QString path = storagePath();
    QJsonArray arr;
    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), it.key());
        o.insert(QStringLiteral("hash"), it.value().hash);
        o.insert(QStringLiteral("group"), it.value().group);
        o.insert(QStringLiteral("must_change"), it.value().mustChange);
        if (!it.value().oldName.isEmpty())
            o.insert(QStringLiteral("old_name"), it.value().oldName);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("users"), arr);
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);

    QFile tmp(path + QStringLiteral(".tmp"));
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote() << "UserSystem: 持久化写失败（无法打开临时文件）" << path;
        return;
    }
    tmp.write(data);
    tmp.flush();
    tmp.close();
#if !defined(Q_OS_WIN)
    QFile::setPermissions(path + QStringLiteral(".tmp"), QFileDevice::ReadOwner | QFileDevice::WriteOwner);   // 0600 密码哈希文件
#endif
    // 原子替换：先删旧目标（Windows rename 不覆盖已存在文件）, 再 rename（Linux rename 原子）
    QFile::remove(path);
    if (!QFile::rename(path + QStringLiteral(".tmp"), path))
        qWarning().noquote() << "UserSystem: 持久化原子写失败" << path;
}

// ── 内部 ──

const UserAccount* UserSystem::findAccount(const QString& userName) const
{
    if (userName.isEmpty())
        return nullptr;
    for (const auto& u : m_project.users)
        if (u.userName == userName)
            return &u;
    return nullptr;
}

QString UserSystem::sha256Hex(const QString& s)
{
    // PC 端契约: SHA256(UTF8) 大写 Hex（.NET Convert.ToHexString）——QCryptographicHash 出小写, 转大写对齐
    return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256).toHex()).toUpper();
}

QString UserSystem::passwordPolicyError(const QString& password) const
{
    const auto& sec = m_project.security;
    if (sec.minPasswordLength > 0 && password.size() < sec.minPasswordLength)
        return QStringLiteral("密码长度至少 %1 位").arg(sec.minPasswordLength);
    bool hasDigit = false, hasLetter = false, hasUpper = false, hasLower = false, hasSpecial = false;
    for (const QChar& c : password) {
        if (c.isDigit()) hasDigit = true;
        else if (c.isLetter()) { hasLetter = true; if (c.isUpper()) hasUpper = true; if (c.isLower()) hasLower = true; }
        else hasSpecial = true;
    }
    if (sec.requireDigit && !hasDigit)
        return QStringLiteral("密码需包含数字");
    if (sec.requireLetter && !hasLetter)
        return QStringLiteral("密码需包含字母");
    if (sec.requireUpperLower && (!hasUpper || !hasLower))
        return QStringLiteral("密码需大小写混合");
    if (sec.requireSpecial && !hasSpecial)
        return QStringLiteral("密码需包含特殊字符");
    return QString();
}

} // namespace navihmi
