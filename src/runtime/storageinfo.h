/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\storageinfo.h
 * @Description: 存储状态真实检测 + 工程文件扫描/替换（B6-7）
 *               SD: /dev/mmcblk1 /sys/block/mmcblk1/size；USB: /dev/sd*
 *               工程目录: 内存 /mnt/user/userdata、SD /mnt/sdcard、USB /mnt/udisk
 *               默认工程文件: /mnt/user/userdata/app.navihmi（加载=复制替换）
 */
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace navihmi {

class StorageInfo : public QObject
{
    Q_OBJECT
public:
    explicit StorageInfo(QObject* parent = nullptr);

    /// SD 卡状态文本（"已插入 (16GB)" / "未插入"）
    Q_INVOKABLE QString sdStatusText() const;
    /// USB 状态文本（同 SD）
    Q_INVOKABLE QString usbStatusText() const;
    /// 列出目录下 .navihmi 工程文件 → [{name, sizeText, path}]
    Q_INVOKABLE QVariantList listProjects(const QString& dir) const;
    /// 复制替换默认工程文件（/mnt/user/userdata/app.navihmi）——非 const（emit projectReplaced）
    Q_INVOKABLE bool replaceDefaultProject(const QString& srcPath);

    static QString defaultProjectPath();

signals:
    /// 默认工程文件已被替换（main.cpp 连接 → 重新加载工程注入 screenFiles）
    void projectReplaced();
};

} // namespace navihmi
