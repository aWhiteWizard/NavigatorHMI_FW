/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.h
 * @Description: 数据采集管理（V-1 2026-09-06 驱动插件化重构——Manager 化）
 *               Tag.source 约定: modbus://{从站}/{寄存器}（未来 mqtt:// 等）
 *               Manager 职责: tag 解析分组 → 注册表建 Driver → 周期调度 poll →
 *                             deadband 判断 → DataManager 写入（跨协议统一）
 *               Driver 职责: 连接 + 协议读写 + 解码（drivers/ 目录，可插拔）
 *               对外 API 不变（setProject/setDataManager/handleValueWritten）——main.cpp 零改动
 *               工程重载 = 驱动 stop+delete 重建（断连重连——原实现保持连接；差异顺带修复
 *               「换 host 工程仍连旧设备」潜在缺陷，2026-09-06 复审记录）
 */
#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QTimer>
#include "runtime/projectmodel.h"

namespace navihmi {
class DataManager;
class IDriver;

class Acquisition : public QObject
{
    Q_OBJECT
public:
    explicit Acquisition(QObject* parent = nullptr);
    ~Acquisition() override;

    void setProject(const Project& proj);
    void setDataManager(DataManager* dm);
    /// 停止采集（V-4 F11 2026-09-06 主壳 Shutdown 显式调用：停调度 timer + 驱动断连；
    /// 析构仍兜底清理——stop 后可不再复用本实例）
    void stop();
    /// 写通道：DataManager 写采集来源变量 → 同步写设备（main.cpp 联动，接口不变）
    void handleValueWritten(const QString& tagName, const QVariant& value);

private:
    void tick();                    // 100ms 调度: driver.poll（到期读）
    void onDriverValueRead(const QString& tagName, const QVariant& value, double deadband);   // deadband + 写 DataManager
    /// 设备连接参数展开（deviceName → DeviceConfig.connectionInfo JSON 键值；空 = 无设备）
    QHash<QString, QString> connInfoForDevice(const QString& deviceName) const;

    Project m_project;
    DataManager* m_dataManager = nullptr;
    QTimer* m_timer = nullptr;
    IDriver* m_driver = nullptr;          // 当前驱动（V-1 单协议实例；多协议并存 V+1 列表化 + 按 tag 分发）
    QHash<QString, QVariant> m_lastVal;   // deadband 判断（跨协议统一语义——原 Acquisition per-tag lastVal 上移）
};

} // namespace navihmi
