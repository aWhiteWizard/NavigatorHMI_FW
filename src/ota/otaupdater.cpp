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

#include "runtime/deviceinfo.h"   // 2026-09-04：kOtaInstalledTsFile 共享常量（ts 标记 writer/reader 单点防漂移）

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>   // O-D D-3b：std::sort（备份目录按 mtime 排序）
#include <cerrno>      // ::rename 原子覆盖（审查 🔴：errno 诊断）
#include <cstring>
#include <cstdio>      // 复审 🟡：::rename 声明源（unistd 在 Linux 提供，显式包含防 Qt 头传递包含脆弱依赖）
#include <functional>  // O-D D-3b：std::function（回滚递归遍历）

namespace navihmi {

OtaUpdater::OtaUpdater(QObject* parent) : QObject(parent) {}

QString OtaUpdater::install(const QString& stagingFwPath)
{
    // 批 D 收尾（httreceiver「假成功」缺陷修复）：统一出口 emit installFinished(result)——
    // 让传输响应反映真实安装结果（原实现校验通过即报成功，install 异步失败静默——PC 误判升级成功）
    QString result;
    if (stagingFwPath.isEmpty() || !QFile::exists(stagingFwPath)) {
        result = QStringLiteral("OTA staging 固件包不存在: %1").arg(stagingFwPath);
        goto done;
    }
    {
    QFile f(stagingFwPath);
    if (!f.open(QIODevice::ReadOnly)) {
        result = QStringLiteral("OTA staging 读取失败: %1").arg(stagingFwPath);
        goto done;
    }
    const QByteArray body = f.readAll();
    f.close();

    // 解析组件表（header 128B + 每项 184B，布局见 FwPackageBuilder 单一事实源）
    QString parseErr;
    const auto comps = parseComponents(body, parseErr);
    if (comps.isEmpty()) {
        result = parseErr;
        goto done;
    }

    int progressPct = 10;   // 局部名避让信号 progress（同名遮蔽致 emit 报错）
    emit progress(progressPct, QStringLiteral("OTA 开始安装"));

    // O-D D-3b：组件可安装性预检——app 恒可装；rootfs（**文件级**，O-D D-3 用户裁决）可装；kernel/其它拒绝
    // （kernel 分区写需 backup 兜底，板端实测 backup 32MB 装不下 boot 64MB → 本轮不开放，留专用工具）
    for (const auto& comp : comps) {
        if (comp.type != QLatin1String("app") && comp.type != QLatin1String("rootfs")) {
            result = QStringLiteral("OTA 拒绝安装：组件 %1（%2）不受支持（本轮范围 app + rootfs 文件级；"
                                    "kernel/分区写需 backup 分区兜底，板端容量不足留专用工具）").arg(comp.name).arg(comp.type);
            goto done;
        }
    }

    // 逐个组件安装（全组件预检通过后执行——app 安全路径 / rootfs 文件级）
    QStringList installed;
    for (const auto& comp : comps) {
        const QByteArray payload = body.mid(comp.offset, qint64(comp.size));
        QString err;
        if (comp.type == QLatin1String("app")) {
            err = installApp(comp, payload, progressPct);
        } else if (comp.type == QLatin1String("rootfs")) {
            err = installRootfsFiles(comp, payload, progressPct);   // O-D D-3b：文件级 + userdata 备份
        } else {
            err = QStringLiteral("未知组件类型: %1").arg(comp.type);   // 预检已拦 kernel，此处仅防御
        }
        if (!err.isEmpty()) {
            result = err;
            goto done;
        }
        installed << comp.type;
    }

    emit progress(100, QStringLiteral("OTA 安装完成，即将重启"));
    // 2026-09-04 调试 OTA：全部组件安装成功 → 记录 .fw header 打包时刻（PC 端同版 v1.1.0 覆盖判断依据——
    // 调试期版本恒 v1.1.0，靠打包时刻先后区分每次调试固件；用户 2026-09-04 裁决「按打包时间」）
    // header 布局（FwPackageBuilder 单一事实源）：magic4 + version16 + timestamp8(LE, 偏移 20) + count4 + sha64
    {
        quint64 packTs = 0;
        for (int b = 0; b < 8; ++b)
            packTs |= quint64(quint8(body.at(20 + b))) << (8 * b);
        markOtaInstalled(packTs);
    }
    // 注：markBootOk 不在 install 内调用——审查 🔴：重启前预写 .boot_ok 会标记「未验证的启动为成功」，
    // 使软件回滚判定失效。启动成功标记由 main() 在 engine.load + 工程加载 OK 后调用（新固件证明自己能启动才记成功）。
    qInfo().noquote() << "OTA 安装完成，重启生效: " << installed.join(QLatin1Char(','));
    const bool rebootOk = QProcess::startDetached(QStringLiteral("/sbin/reboot"), {});
    if (!rebootOk)
        qWarning().noquote() << "OTA 重启命令发送失败（/sbin/reboot 不可用？）——请手动重启使新固件生效";   // 审查 🟡：返回值检查
    }   // 作用域收尾（result 默认空 = 成功）
done:
    emit installFinished(result);
    return result;
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
        c.version = QString::fromLatin1(body.mid(off + 168, 16).trimmed());   // O-D D-3b：组件 version（备份目录命名用）
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
    // 6. O-D D-2：期望固件 md5 落盘（/etc/navigatorhmi/expected-md5）——start_runtime.sh 启动前比对，
    //    不符则从 .bak 回退（软件回滚第一道；版本比对用 md5 而非字符串——二进制内版本字符串不可靠）
    {
        QFile nf(target);
        if (nf.open(QIODevice::ReadOnly)) {
            const QByteArray installedSha = QCryptographicHash::hash(nf.readAll(), QCryptographicHash::Sha256).toHex();
            nf.close();
            QDir().mkpath(QStringLiteral("/etc/navigatorhmi"));
            QFile ef(QStringLiteral("/etc/navigatorhmi/expected-md5"));
            if (ef.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                ef.write(installedSha);
                ef.close();
                qInfo().noquote() << "OTA 期望固件 md5 已写入 /etc/navigatorhmi/expected-md5";
            } else {
                qWarning().noquote() << "OTA 期望固件 md5 写入失败（start_runtime 版本校验将失效）";
            }
        }
    }
    progressOut += 30;
    emit progress(progressOut, QStringLiteral("app 已更新"));
    qInfo().noquote() << "OTA app 组件安装完成: " << target << "（备份 " << backup << "）";
    return QString();
}

QString OtaUpdater::installRootfsFiles(const Component& comp, const QByteArray& payload, int& progressOut)
{
    // O-D D-3b（2026-09，用户裁决：rootfs 文件级更新 + userdata 备份回滚——backup 分区 32MB 装不下整镜像）。
    // payload 格式（pack_fw.py 打包端对齐——单一事实源注释见 FwPackageBuilder/工具）：
    //   循环到 payload 尾：
    //     4B LE 路径长度 + 路径 UTF8（相对 / 的安装路径，如 usr/sbin/start_runtime.sh）
    //     8B LE 内容长度 + 内容
    // 安装 = 逐文件：备份旧版（存在则拷到 userdata/ota-backup/<fwver>/<路径>）→ 写新（tmp + ::rename 原子）→
    // 完成后 markRootfsInstalled（写版本标记）；重启生效。启动失败回滚见 restoreFromUserdataBackup。
    const QString fwVer = comp.version.isEmpty() ? QStringLiteral("unknown") : comp.version.trimmed();

    // 1. 路径安全白名单预检（防 payload 覆盖任意系统文件——`..`/绝对路径拒绝；只允许常规文件路径）
    struct Entry { QString relPath; QByteArray content; };
    QList<Entry> entries;
    {
        quint64 pos = 0;
        auto readLe32 = [&payload, &pos](quint32& out) -> bool {
            if (pos + 4 > quint64(payload.size())) return false;
            out = quint32(quint8(payload.at(int(pos)))) |
                  (quint32(quint8(payload.at(int(pos) + 1))) << 8) |
                  (quint32(quint8(payload.at(int(pos) + 2))) << 16) |
                  (quint32(quint8(payload.at(int(pos) + 3))) << 24);
            pos += 4;
            return true;
        };
        auto readLe64 = [&payload, &pos](quint64& out) -> bool {
            if (pos + 8 > quint64(payload.size())) return false;
            out = 0;
            for (int b = 0; b < 8; ++b)
                out |= quint64(quint8(payload.at(int(pos) + b))) << (8 * b);
            pos += 8;
            return true;
        };
        while (pos < quint64(payload.size())) {
            quint32 pathLen = 0;
            if (!readLe32(pathLen) || pathLen == 0 || pathLen > 512)
                return QStringLiteral("rootfs 文件级包路径长度非法（pos=%1）").arg(pos);
            if (pos + pathLen > quint64(payload.size()))
                return QStringLiteral("rootfs 文件级包路径越界");
            const QString rel = QString::fromUtf8(payload.mid(int(pos), int(pathLen)));
            pos += pathLen;
            quint64 contentLen = 0;
            if (!readLe64(contentLen) || contentLen > quint64(payload.size()) - pos)
                return QStringLiteral("rootfs 文件级包内容长度非法/越界（%1）").arg(rel);
            // 路径安全：绝对路径 / `..` / 空段拒绝（防穿越覆盖 /etc/passwd 等）
            if (rel.startsWith(QLatin1Char('/')) || rel.contains(QStringLiteral("..")))
                return QStringLiteral("rootfs 文件级包路径非法（禁止绝对路径或 ..）: %1").arg(rel);
            entries.append({ rel, payload.mid(int(pos), int(contentLen)) });
            pos += contentLen;
        }
    }
    if (entries.isEmpty())
        return QStringLiteral("rootfs 文件级包为空（无文件条目）");

    // 2. 全条目预检（防半装）：目标父目录可创建性 + 备份空间假设——先收集目标路径
    QStringList targets;
    for (const auto& e : entries)
        targets << QStringLiteral("/") + e.relPath;

    // 3. 逐文件：备份旧版到 userdata ota-backup → 写新（tmp + rename 原子，断电保护）
    // 备份目录：/mnt/user/userdata/ota-backup/<fwver>/<relPath>（保留相对路径树）
    const QString backupRoot = QString::fromLatin1(kOtaBackupRoot) + QLatin1Char('/') + fwVer;
    int done = 0;
    for (int i = 0; i < entries.size(); ++i) {
        const QString target = QStringLiteral("/") + entries[i].relPath;
        const QString backupPath = backupRoot + QLatin1Char('/') + entries[i].relPath;
        // 3a. 备份旧版（若目标存在且非符号链接——文件级覆盖常规文件）
        if (QFile::exists(target)) {
            QFileInfo ti(target);
            if (ti.isSymLink())
                return QStringLiteral("目标为符号链接，拒绝覆盖（安全）: %1").arg(target);
            if (!QDir().mkpath(QFileInfo(backupPath).absolutePath()))
                return QStringLiteral("rootfs 备份目录创建失败: %1").arg(QFileInfo(backupPath).absolutePath());
            if (QFile::exists(backupPath) && !QFile::remove(backupPath))
                return QStringLiteral("rootfs 备份清理失败: %1").arg(backupPath);
            if (!QFile::copy(target, backupPath))
                return QStringLiteral("rootfs 备份失败: %1 → %2").arg(target).arg(backupPath);
        } else {
            // 目标不存在：记占位（恢复时删除新文件）
            if (!QDir().mkpath(QFileInfo(backupPath).absolutePath()))
                return QStringLiteral("rootfs 备份目录创建失败: %1").arg(QFileInfo(backupPath).absolutePath());
            QFile marker(backupPath + QStringLiteral(".new"));
            if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) { marker.write("x"); marker.close(); }
        }
        // 3b. 写新（tmp + ::rename 原子）
        if (!QDir().mkpath(QFileInfo(target).absolutePath()))
            return QStringLiteral("rootfs 目标目录创建失败: %1").arg(QFileInfo(target).absolutePath());
        const QString tmp = target + QStringLiteral(".ota_tmp");
        {
            QFile w(tmp);
            if (!w.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return QStringLiteral("rootfs 写新失败（打开 tmp）: %1").arg(tmp);
            if (w.write(entries[i].content) != qint64(entries[i].content.size())) {
                w.close();
                QFile::remove(tmp);
                return QStringLiteral("rootfs 写新不完整: %1").arg(target);
            }
            w.close();
        }
        if (QFile::exists(target)) {
            if (::rename(tmp.toLocal8Bit().constData(), target.toLocal8Bit().constData()) != 0) {
                QFile::remove(tmp);
                return QStringLiteral("rootfs 替换失败（::rename 原子覆盖）: %1 (%2)")
                    .arg(target).arg(QString::fromLocal8Bit(strerror(errno)));
            }
            // 3c. 恢复原权限（上板实测缺陷修复：覆盖 /etc/init.d/S99qt-test 等脚本后新文件 0644
            //     失去执行位 → 守护/rcS 无法执行 → FW 起不来）。rename 后权限 = tmp 创建默认（0644），
            //     需从备份（=原文件权限，QFile::copy 保留）或目标原权限恢复
            QFileInfo ti(target);
            const QFile::Permissions origPerm = ti.permissions();
            QFileInfo backupFi(backupPath);
            const QFile::Permissions perm = backupFi.exists() ? backupFi.permissions() : origPerm;
            if (!QFile::setPermissions(target, perm))
                qWarning().noquote() << "rootfs 权限恢复失败: " << target;
        } else {
            if (!QFile::rename(tmp, target)) {
                QFile::remove(tmp);
                return QStringLiteral("rootfs 替换失败（rename）: %1").arg(target);
            }
            // 3c. 新文件默认 0644——若路径惯例为可执行（sbin/ 或 init.d/ 下）补执行位
            //     （pack_fw.py 源文件在 Windows 检出无 Unix 执行位 → 打包内容权限不可靠，此处按路径惯例兜底）
            if (entries[i].relPath.contains(QLatin1String("sbin/"))
                || entries[i].relPath.contains(QLatin1String("init.d/"))) {
                const QFile::Permissions x =
                    QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                    | QFile::ReadGroup | QFile::ExeGroup
                    | QFile::ReadOther | QFile::ExeOther;
                if (!QFile::setPermissions(target, x))
                    qWarning().noquote() << "rootfs 新脚本执行位设置失败: " << target;
            }
        }
        ++done;
        progressOut += int(30.0 * done / entries.size()) ;
        emit progress(progressOut, QStringLiteral("rootfs 文件 %1/%2 已更新").arg(done).arg(entries.size()));
    }

    markRootfsInstalled(fwVer);
    progressOut += 20;
    qInfo().noquote() << "OTA rootfs 文件级安装完成: " << done << " 文件（备份 " << backupRoot << "）";
    return QString();
}

void OtaUpdater::markRootfsInstalled(const QString& fwVersion)
{
    // 写本次 rootfs 文件级安装版本标记（供回滚/诊断识别当前生效的文件集版本）
    QDir().mkpath(QStringLiteral("/etc/navigatorhmi"));
    QFile f(QStringLiteral("/etc/navigatorhmi/rootfs-files-version"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(fwVersion.toUtf8());
        f.close();
    }
}

void OtaUpdater::markOtaInstalled(quint64 packTimestamp)
{
    // 写当前生效固件的 OTA 打包时刻（调试同版覆盖判断；deviceinfo::otaTimestamp 读取上报）
    QDir().mkpath(QStringLiteral("/etc/navigatorhmi"));
    QFile f(QString::fromLatin1(kOtaInstalledTsFile));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QByteArray::number(packTimestamp));
        f.close();
    } else {
        qWarning().noquote() << "OTA 打包时刻标记写入失败（" << kOtaInstalledTsFile
                             << "）——PC 端同版覆盖判断将退化";
    }
}

int OtaUpdater::recordBootFail()
{
    int count = bootFailCount() + 1;
    QFile f{QLatin1String(kBootFailPath)};
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QByteArray::number(count));
        f.close();
    }
    qWarning().noquote() << "OTA 启动失败计数: " << count << "/" << kMaxBootFail;
    return count;
}

QString OtaUpdater::restoreFromUserdataBackup(bool latestOnly)
{
    Q_UNUSED(latestOnly)   // 当前实现即「恢复最新一次备份」（按 mtime 取最近目录）——latestOnly=false（全量恢复）语义待扩展
    // 从 userdata/ota-backup/<fwver>/ 恢复最近一次 rootfs 文件级备份：
    //   备份树中 .new 后缀标记 = 原目标不存在（本次新装）→ 恢复 = 删除新文件；
    //   普通文件 = 原目标旧版 → 恢复 = 拷回目标。
    // 选择：latestOnly → 目录名按字典序取最大（版本号字典序在 x.y.z 下基本单调，数值差距大时不准——
    // 稳妥用 mtime 最新目录）。为简单可靠：遍历全部备份目录按目录 mtime 降序取第一个。
    QDir root(QString::fromLatin1(kOtaBackupRoot));
    if (!root.exists())
        return QStringLiteral("无 rootfs 备份可恢复（%1 不存在）").arg(QString::fromLatin1(kOtaBackupRoot));
    QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (dirs.isEmpty())
        return QStringLiteral("无 rootfs 备份可恢复（空备份目录）");
    // 按目录 mtime 降序（最近备份优先）
    std::sort(dirs.begin(), dirs.end(), [&root](const QString& a, const QString& b) {
        return QFileInfo(root.filePath(a)).lastModified() > QFileInfo(root.filePath(b)).lastModified();
    });
    const QString picked = dirs.first();
    int restored = 0, removed = 0;
    const QDir backupDir(root.filePath(picked));
    // 递归收集（备份保留相对路径树——entryList 仅顶层；用递归）。
    // 目标相对路径以 backupDir 为基准（剥掉 <fwver>/ 前缀）——.new 标记剥后缀后即原目标相对路径。
    std::function<void(const QDir&)> walk = [&](const QDir& dir) {
        const auto all = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& fi : all) {
            if (fi.isDir()) { walk(QDir(fi.absoluteFilePath())); continue; }
            QString rel = backupDir.relativeFilePath(fi.absoluteFilePath());
            const bool isNewMarker = rel.endsWith(QStringLiteral(".new"));
            if (isNewMarker)
                rel = rel.left(rel.size() - 4);          // 去掉 .new
            const QString target = QStringLiteral("/") + rel;
            if (isNewMarker) {
                // 原目标不存在 → 删除本次安装的新文件
                if (QFile::exists(target) && !QFile::remove(target))
                    qWarning().noquote() << "回滚删除失败: " << target;
                else if (QFile::exists(target))
                    ++removed;
            } else {
                if (!QDir().mkpath(QFileInfo(target).absolutePath()))
                    continue;
                if (QFile::exists(target) && !QFile::remove(target))
                    continue;
                if (QFile::copy(fi.absoluteFilePath(), target))
                    ++restored;
            }
        }
    };
    walk(backupDir);
    // 回滚成功标记：清失败计数（恢复后进入「再试启动」状态）
    QFile::remove(QLatin1String(kBootFailPath));
    // 2026-09-04 审查 🟡：回滚后清除 ota-installed-ts（置空 = 视作无记录 → PC 端归 0 → 任意调试包可重装）——
    // 否则 ts 标记仍是「失败包时刻」，与「firmware_ts=当前生效固件打包时刻」语义背离（PC 会误拒最后一次好包）
    QFile::remove(QString::fromLatin1(kOtaInstalledTsFile));
    qInfo().noquote() << "OTA rootfs 回滚完成（来源 " << picked << "）：恢复 " << restored
                      << " 文件，删除 " << removed << " 新文件（打包时刻标记已清）";
    return QString();
}

QString OtaUpdater::installPartition(const Component& comp, const QByteArray& payload)
{
    // ⚠️ 分区写（rootfs/kernel）为高风险路径：rknetupdate-upgrade-hang 先例。
    // O-D D-3（2026-09）板端勘察：backup 分区仅 32MB（rootfs 14.3GB / boot 64MB 均装不下）——
    // 分区写整包路径物理不可行；rootfs 已改文件级（installRootfsFiles）；本接口保留但持续拒绝（防误用）。
    Q_UNUSED(comp) Q_UNUSED(payload)
    return QStringLiteral("分区组件安装未开放——backup 分区容量不足（32MB），rootfs 走文件级更新（O-D D-3）");
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
