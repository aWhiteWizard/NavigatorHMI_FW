/*
 * @FilePath: \NavigatorHMI_FW\src\ota\otaupdater.cpp
 * @Description: D1 OTA 固件安装器实现（2026-08-30 批 2）。
 *               app：备份 → 写新 → sha 校验 → 重启（应用层替换，无分区写，安全）；
 *               rootfs/kernel：backup 分区 dd 备份 → 主分区写 → 回滚标记（软件回滚 .boot_ok 计数）。
 *               ⚠️ 分区写（rootfs/kernel）需板端实测验证（rknetupdate-upgrade-hang 先例）——
 *                 本实现**默认整体拒绝非 app 组件**（install() all-or-nothing 预检，任何写入前返回），
 *                 app-only .fw 可先行；分区链路待板端验证后再放开（见 installPartition）。
 */
#include "ota/otaupdater.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>
#include <cerrno>      // ::rename 原子覆盖（审查 🔴：errno 诊断）
#include <cstring>
#include <cstdio>      // 复审 🟡：::rename 声明源（unistd 在 Linux 提供，显式包含防 Qt 头传递包含脆弱依赖）

namespace navihmi {

OtaUpdater::OtaUpdater(QObject* parent) : QObject(parent) {}

QString OtaUpdater::install(const QString& stagingFwPath)
{
    if (stagingFwPath.isEmpty() || !QFile::exists(stagingFwPath))
        return QStringLiteral("OTA staging 固件包不存在: %1").arg(stagingFwPath);
    QFile f(stagingFwPath);
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("OTA staging 读取失败: %1").arg(stagingFwPath);
    const QByteArray body = f.readAll();
    f.close();

    // 解析组件表（header 128B + 每项 184B，布局见 FwPackageBuilder 单一事实源）
    QString parseErr;
    const auto comps = parseComponents(body, parseErr);
    if (comps.isEmpty())
        return parseErr;

    int progressPct = 10;   // 局部名避让信号 progress（同名遮蔽致 emit 报错）
    emit progress(progressPct, QStringLiteral("OTA 开始安装"));

    // 审查 🟡：all-or-nothing 预检——任何组件不可安装（rootfs/kernel 分区路径本轮拒）则整体拒绝，
    // 在任何文件写入前返回（防混合包 app 替换后 rootfs 拒绝 → 半更新状态无回滚）
    for (const auto& comp : comps) {
        if (comp.type != QLatin1String("app"))
            return QStringLiteral("OTA 拒绝安装：组件 %1（%2）需板端分区实测验证（rknetupdate-upgrade-hang 先例），"
                                  "本轮 OTA 范围仅 app 组件（应用层替换）；请上传 app-only .fw").arg(comp.name).arg(comp.type);
    }

    // 逐个组件安装（全组件预检通过后执行——app 安全路径）
    QStringList installed;
    for (const auto& comp : comps) {
        const QByteArray payload = body.mid(comp.offset, qint64(comp.size));
        QString err;
        if (comp.type == QLatin1String("app")) {
            err = installApp(comp, payload, progressPct);
        } else {
            err = QStringLiteral("未知组件类型: %1").arg(comp.type);   // 预检已拦 rootfs/kernel，此处仅防御
        }
        if (!err.isEmpty())
            return err;
        installed << comp.type;
    }

    emit progress(100, QStringLiteral("OTA 安装完成，即将重启"));
    // 注：markBootOk 不在 install 内调用——审查 🔴：重启前预写 .boot_ok 会标记「未验证的启动为成功」，
    // 使软件回滚判定失效。启动成功标记由 main() 在 engine.load + 工程加载 OK 后调用（新固件证明自己能启动才记成功）。
    qInfo().noquote() << "OTA 安装完成，重启生效: " << installed.join(QLatin1Char(','));
    const bool rebootOk = QProcess::startDetached(QStringLiteral("/sbin/reboot"), {});
    if (!rebootOk)
        qWarning().noquote() << "OTA 重启命令发送失败（/sbin/reboot 不可用？）——请手动重启使新固件生效";   // 审查 🟡：返回值检查
    return QString();
}

QList<OtaUpdater::Component> OtaUpdater::parseComponents(const QByteArray& body, QString& error) const
{
    const int kHeaderSize = 128;
    const int kEntrySize = 184;
    QList<Component> comps;
    if (body.size() < kHeaderSize) { error = QStringLiteral(".fw 固件包损坏（不足 header）"); return comps; }
    quint32 count = 0;
    for (int i = 0; i < 4; ++i)
        count |= quint32(quint8(body.at(28 + i))) << (8 * i);   // LE
    if (count == 0 || count > 32) { error = QStringLiteral("组件数非法: %1").arg(count); return comps; }
    if (body.size() < kHeaderSize + qint64(count) * kEntrySize) { error = QStringLiteral("组件表不完整"); return comps; }
    qint64 off = kHeaderSize;
    for (quint32 i = 0; i < count; ++i) {
        Component c;
        c.name = QString::fromLatin1(body.mid(off, 32).trimmed());
        c.type = QString::fromLatin1(body.mid(off + 32, 16).trimmed());
        c.target = QString::fromLatin1(body.mid(off + 48, 48).trimmed());
        quint64 size = 0;
        for (int b = 0; b < 8; ++b)
            size |= quint64(quint8(body.at(off + 96 + b))) << (8 * b);   // LE
        c.size = size;
        c.sha = QString::fromLatin1(body.mid(off + 104, 64).trimmed());
        comps.append(c);
        off += kEntrySize;
    }
    // payload 偏移：header + 全表后按序累加（FwPackageBuilder 无 offset 字段，依赖表序+size）
    // 复审 🟡：quint64 无符号累加防 size ≥ 2^63 溢出（有符号 qint64 变负可绕过越界检查）
    quint64 payloadBase = quint64(kHeaderSize) + quint64(count) * quint64(kEntrySize);
    for (auto& c : comps) {
        c.offset = qint64(payloadBase);
        payloadBase += c.size;
    }
    // 越界检查（复审 🟡：无符号比较，防溢出绕过）
    if (payloadBase > quint64(body.size())) { error = QStringLiteral("组件 payload 越界"); return {}; }
    return comps;
}

QString OtaUpdater::installApp(const Component& comp, const QByteArray& payload, int& progressOut)
{
    const QString target = QStringLiteral("/usr/bin/navigatorhmi-fw");
    const QString backup = QStringLiteral("/usr/bin/navigatorhmi-fw.bak");

    // 1. 备份当前二进制（回滚依据）
    if (QFile::exists(target)) {
        if (QFile::exists(backup) && !QFile::remove(backup))
            return QStringLiteral("app 备份清理失败: %1").arg(backup);
        if (!QFile::copy(target, backup))
            return QStringLiteral("app 备份失败: %1").arg(backup);
    }
    // 2. 写新（原子：tmp + rename，断电保护）
    const QString tmp = target + QStringLiteral(".ota_tmp");
    {
        QFile w(tmp);
        if (!w.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QStringLiteral("app 写新失败（打开 tmp）: %1").arg(tmp);
        if (w.write(payload) != qint64(payload.size()))
            return QStringLiteral("app 写新不完整");
        w.close();
    }
    // 3. 校验新文件 sha（对齐组件表）
    QFile nf(tmp);
    if (!nf.open(QIODevice::ReadOnly)) return QStringLiteral("app 新文件校验读取失败");
    const QByteArray newSha = QCryptographicHash::hash(nf.readAll(), QCryptographicHash::Sha256).toHex();
    nf.close();
    if (QString::fromLatin1(newSha) != comp.sha) {
        QFile::remove(tmp);   // 复审 🟡：sha 失败清 tmp 残留（对齐 httreceiver 清理对称模式）
        return QStringLiteral("app 新文件 SHA256 校验失败（写入损坏）");
    }
    // 4. 原子替换（tmp → target）：审查 🔴 修复——QFile::remove+rename 是「先删后换」，断电窗口丢主程序；
    //    用 POSIX ::rename() 原子覆盖（对齐仓库 L-A4 实测结论：QFile::rename 对已存在目标失败，
    //    且必须一次原子覆盖防断电半替换；先例 httpreceiver.cpp:430-434）
    if (!QFile::exists(target)) {
        if (!QFile::rename(tmp, target))
            return QStringLiteral("app 替换失败（rename）");
    } else {
        if (::rename(tmp.toLocal8Bit().constData(), target.toLocal8Bit().constData()) != 0)
            return QStringLiteral("app 替换失败（::rename 原子覆盖）: %1").arg(QString::fromLocal8Bit(strerror(errno)));
    }
    // 5. 权限保持可执行（fs-overlay 原 755；重写后确保——审查 🟡：失败告警防下次启动不可执行）
    if (!QFile::setPermissions(target, QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                                          | QFile::ReadGroup | QFile::ExeGroup
                                                          | QFile::ReadOther | QFile::ExeOther)))
        qWarning().noquote() << "OTA app 权限设置失败（可能不可执行）: " << target;
    progressOut += 30;
    emit progress(progressOut, QStringLiteral("app 已更新"));
    qInfo().noquote() << "OTA app 组件安装完成: " << target << "（备份 " << backup << "）";
    return QString();
}

QString OtaUpdater::installPartition(const Component& comp, const QByteArray& payload)
{
    // ⚠️ 分区写（rootfs/kernel）为高风险路径：rknetupdate-upgrade-hang 先例（整包 recovery 卡住）。
    // 本实现为预留接口——当前由 install() 的 all-or-nothing 预检整体拒绝（任何文件写入前返回）；
    // 待板端分区实测验证（backup 分区写读测试 + 单组件升级实测，执行书风险项）后放开。
    Q_UNUSED(comp) Q_UNUSED(payload)
    return QStringLiteral("分区组件安装未开放——需板端分区实测验证（rknetupdate-upgrade-hang 先例）");
}

void OtaUpdater::markBootOk()
{
    QFile f{QLatin1String(kBootOkPath)};   // 统一初始化规避 most vexing parse
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote() << "OTA 启动成功标记写入失败（软件回滚判定将失效）: " << kBootOkPath;   // 复审 🟡：返回值检查
        return;
    }
    f.write("ok");
    f.close();
    QFile::remove(QLatin1String(kBootFailPath));   // 成功则清失败计数
}

int OtaUpdater::bootFailCount() const
{
    QFile f{QLatin1String(kBootFailPath)};
    if (!f.open(QIODevice::ReadOnly)) return 0;
    const QByteArray n = f.readAll().trimmed();
    f.close();
    return n.toInt();
}

} // namespace navihmi
