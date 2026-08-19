/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\storageinfo.cpp
 * @Description: 存储状态实现（B6-7）
 */
#include "runtime/storageinfo.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

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

bool hasBlockDev(const QString& name)
{
    return QFile::exists(QStringLiteral("/sys/block/%1").arg(name));
}
} // namespace

StorageInfo::StorageInfo(QObject* parent)
    : QObject(parent)
{
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
    // SD 卡: mmcblk1 块设备存在 = 已插入
    if (hasBlockDev(QStringLiteral("mmcblk1")))
        return QStringLiteral("已插入 (%1)").arg(blockSizeText(QStringLiteral("mmcblk1")));
    return QStringLiteral("未插入");
#endif
}

QString StorageInfo::usbStatusText() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("未插入");
#else
    // USB: 任意 sd* 块设备存在 = 已插入
    QDir sysBlock(QStringLiteral("/sys/block"));
    const QStringList names = sysBlock.entryList({ QStringLiteral("sd*") });
    if (!names.isEmpty())
        return QStringLiteral("已插入 (%1)").arg(blockSizeText(names.first()));
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
    // 原子替换：先拷临时文件再 rename（避免 copy 失败删掉原默认工程）
    const QString tmp = target + QStringLiteral(".tmp");
    QFile::remove(tmp);   // 清陈旧 tmp（上次异常中断残留）
    if (!QFile::copy(srcPath, tmp))
        return false;
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
