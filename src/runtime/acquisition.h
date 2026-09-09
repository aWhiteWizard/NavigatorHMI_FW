/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.h
 * @Description: 数据采集管理（V-1 2026-09-06 驱动插件化重构——Manager 化；
 *               Y-4 2026-09-10 多协议列表化：modbus_tcp + mqtt 并存，按 tag.source 前缀分发）
 *               Tag.source 约定: modbus://{从站}/{寄存器} / mqtt://（MQTT 采集来源标识）
 *               Manager 职责: tag 解析分组 → 注册表建 Driver（按协议）→ 周期调度 poll →
 *                             deadband 判断 → DataManager 写入（跨协议统一）
 *               Driver 职责: 连接 + 协议读写 + 解码（drivers/ 目录，可插拔）
 *               对外 API 不变（setProject/setDataManager/handleValueWritten）——main.cpp 零改动
 */
#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include <QSet>
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
    /// 设备连接参数展开（deviceName → DeviceConfig.connectionInfo JSON 键值；空 = 无设备——modbus 用）
    QHash<QString, QString> connInfoForDevice(const QString& deviceName) const;
    /// 按 source 前缀路由协议（modbus:// → modbus_tcp；mqtt:// → mqtt；空 → 未知）
    QString protocolForSource(const QString& source) const;
    /// 销毁全部驱动（工程重载/析构用）
    void destroyDrivers();

    Project m_project;
    DataManager* m_dataManager = nullptr;
    QTimer* m_timer = nullptr;
    QList<IDriver*> m_drivers;            // Y-4: 多协议驱动列表（modbus_tcp + mqtt×连接 并存）
    /// Z 循环：driver → 本连接配置映射（StatusTag 回写按连接找 status_tag；驱动销毁时同步清）
    QHash<const IDriver*, const MqttConnectionConfig*> m_driverConns;
    /// StatusTag 未声明告警去重（每 tag 首现 qWarning——reviewer Z-5 🟡6）
    QSet<QString> m_statusTagWarned;
    QHash<QString, QVariant> m_lastVal;   // deadband 判断（跨协议统一语义——原 Acquisition per-tag lastVal 上移）
};

} // namespace navihmi
