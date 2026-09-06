/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\storageinfo.cpp
 * @Description: 存储状态实现（B6-7）
 */
#include "runtime/storageinfo.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#endif

namespace navihmi {

namespace {
// 读 /sys/block/<dev>/size（512B 扇区数）→ 容量文本
QString blockSizeText(const QString& dev)
{
    QFile f(QStringLiteral("/sys/block/%1/size").arg(dev));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const qint64 sectors = f.readLine().trimmed().toLongLong();
    f.close();
    const double gb = sectors * 512.0 / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0)
        return QStringLiteral("%1GB").arg(QString::number(gb, 'f', 0));
    return QStringLiteral("%1MB").arg(QString::number(gb * 1024.0, 'f', 0));
}

// W-C（F4）：statvfs 挂载点可用空间文本（"12.5GB"/"340MB"）；未挂载/失败返回空（调用方决定降级文案）
QString mountAvailText(const QString& mountPath)
{
#ifdef Q_OS_UNIX
    struct statvfs st;
    if (::statvfs(mountPath.toUtf8().constData(), &st) != 0)
        return QString();
    const double availBytes = double(st.f_bavail) * double(st.f_frsize);
    const double gb = availBytes / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0)
        return QStringLiteral("%1GB").arg(QString::number(gb, 'f', 1));
    // MB = GB × 1024（<1GiB 用 MB 显示；1GiB=1024MiB，MiB 数 = GiB 数 × 1024）
    return QStringLiteral("%1MB").arg(QString::number(gb * 1024.0, 'f', 0));
#else
    return QString();
#endif
}

bool hasBlockDev(const QString& name)
{
    return QFile::exists(QStringLiteral("/sys/block/%1").arg(name));
}
} // namespace

StorageInfo::StorageInfo(QObject* parent)
    : QObject(parent)
{
#ifndef Q_OS_WIN
    // W-C（F4）：热插拔事件驱动（替代轮询）——/sys/block 目录项增删 = SD/USB 块设备插拔
    m_blockWatcher.addPath(QStringLiteral("/sys/block"));
    connect(&m_blockWatcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString&) { emit storageChanged(); });
#endif
}

QString StorageInfo::defaultProjectPath()
{
    return QStringLiteral("/mnt/user/userdata/app.navihmi");
}

QString StorageInfo::sdStatusText() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("未插入");   // 仿真占位（真实无卡）
#else
    // SD 卡: mmcblk1 块设备存在 = 已插入；W-C（F4）：附 statvfs 可用空间（挂载点已挂载才显示）
    if (hasBlockDev(QStringLiteral("mmcblk1"))) {
        const QString cap = blockSizeText(QStringLiteral("mmcblk1"));
        const QString avail = mountAvailText(QStringLiteral("/mnt/sdcard"));
        return avail.isEmpty()
            ? QStringLiteral("已插入 (%1)").arg(cap)
            : QStringLiteral("已插入 (%1, 可用 %2)").arg(cap, avail);
    }
    return QStringLiteral("未插入");
#endif
}

QString StorageInfo::usbStatusText() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("未插入");
#else
    // USB: 任意 sd* 块设备存在 = 已插入；W-C（F4）：附 statvfs 可用空间
    QDir sysBlock(QStringLiteral("/sys/block"));
    const QStringList names = sysBlock.entryList({ QStringLiteral("sd*") });
    if (!names.isEmpty()) {
        const QString cap = blockSizeText(names.first());
        const QString avail = mountAvailText(QStringLiteral("/mnt/udisk"));
        return avail.isEmpty()
            ? QStringLiteral("已插入 (%1)").arg(cap)
            : QStringLiteral("已插入 (%1, 可用 %2)").arg(cap, avail);
    }
    return QStringLiteral("未插入");
#endif
}

QVariantList StorageInfo::listProjects(const QString& dir) const
{
    QVariantList out;
    QDir d(dir);
    if (!d.exists())
        return out;
    const QStringList files = d.entryList({ QStringLiteral("*.navihmi") }, QDir::Files, QDir::Name);
    for (const QString& f : files) {
        const QFileInfo fi(d.filePath(f));
        QVariantMap m;
        m["name"] = f;
        m["sizeText"] = QStringLiteral("%1KB").arg(fi.size() / 1024.0, 0, 'f', 1);
        m["path"] = fi.absoluteFilePath();
        out.append(m);
    }
    return out;
}

bool StorageInfo::replaceDefaultProject(const QString& srcPath)
{
    if (srcPath.isEmpty() || !QFile::exists(srcPath))
        return false;
    const QString target = defaultProjectPath();
    if (srcPath == target) {
        emit projectReplaced();   // 本身就是默认文件（用户对默认文件点加载）——仍通知刷新
        return true;
    }
    // 用户 2026-08-30 方案：内部内存只保留一个可显示工程——SD/USB 加载替换默认工程前，
    // 清旧瓦片目录（新工程若无瓦片，防单文件探测命中旧瓦片显示错配；有瓦片则落新后生效）
    // 原子替换：先拷临时文件再 rename（避免 copy 失败删掉原默认工程）
    const QString tmp = target + QStringLiteral(".tmp");
    QFile::remove(tmp);   // 清陈旧 tmp（上次异常中断残留）
    if (!QFile::copy(srcPath, tmp))
        return false;
    // 审查 🟡：瓦片清理放在 tmp copy 成功之后——copy 失败时工程与瓦片完全不动（保持「失败不影响当前工程」直觉）；
    // 检查 removeRecursively 返回值：失败 → qWarning + return false（QML 弹「替换失败」；防旧瓦片残留错配）
    const QString tilesDir = QFileInfo(target).absolutePath() + QStringLiteral("/tiles");
    if (QDir(tilesDir).exists()) {
        QDir oldTiles(tilesDir);
        if (!oldTiles.removeRecursively()) {
            qWarning().noquote() << "旧瓦片目录清理失败（SD/USB 替换）:" << tilesDir;
            QFile::remove(tmp);
            return false;
        }
        qInfo().noquote() << "已清理旧瓦片目录（SD/USB 替换，内部内存单工程原则）:" << tilesDir;
    }
    QFile::remove(target);
    if (!QFile::rename(tmp, target)) {
        // rename 失败兜底还原（target 已被 remove）
        if (!QFile::copy(tmp, target)) {
            QFile::remove(tmp);
            return false;
        }
        QFile::remove(tmp);
    }
    emit projectReplaced();
    return true;
}

} // namespace navihmi
