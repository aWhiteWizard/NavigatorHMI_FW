/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\storageinfo.h
 * @Description: 存储状态真实检测 + 工程文件扫描/替换（B6-7）
 *               SD: /dev/mmcblk1 /sys/block/mmcblk1/size；USB: /dev/sd*
 *               工程目录: 内存 /mnt/user/userdata、SD /mnt/sdcard、USB /mnt/udisk
 *               默认工程文件: /mnt/user/userdata/app.navihmi（加载=复制替换）
 */
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>

namespace navihmi {

class StorageInfo : public QObject
{
    Q_OBJECT
public:
    explicit StorageInfo(QObject* parent = nullptr);

    /// SD 卡状态文本（"已插入 (16GB, 可用 X)" / "未插入"）——W-C 附 statvfs 可用空间
    Q_INVOKABLE QString sdStatusText() const;
    /// USB 状态文本（同 SD）——W-C 附 statvfs 可用空间
    Q_INVOKABLE QString usbStatusText() const;
    /// 列出目录下 .navihmi 工程文件 → [{name, sizeText, path}]
    Q_INVOKABLE QVariantList listProjects(const QString& dir) const;
    /// 复制替换默认工程文件（/mnt/user/userdata/app.navihmi）——非 const（emit projectReplaced）
    Q_INVOKABLE bool replaceDefaultProject(const QString& srcPath);

    static QString defaultProjectPath();

signals:
    /// 默认工程文件已被替换（main.cpp 连接 → 重新加载工程注入 screenFiles）
    void projectReplaced();
    /// W-C（F4）：SD/USB 块设备插拔变化 → QML 存储页即时刷新状态
    void storageChanged();

private:
    /// W-C 修复（2026-09-07 用户实测：/sys/block inotify directoryChanged 不派发——kernfs/sysfs 目录
    /// 监控不可靠）→ 2s 低频快照比对轮询兜底（保留 watcher 作为部分内核即时通道，轮询保证兜底）
    void pollBlockDevices();
    QSet<QString> blockDeviceSnapshot() const;

    QFileSystemWatcher m_blockWatcher;
    QTimer m_pollTimer;              // 2s 轮询
    QSet<QString> m_lastSnapshot;    // 上次轮询 /sys/block 块设备集合（mmcblk1/sd*）
};

} // namespace navihmi
