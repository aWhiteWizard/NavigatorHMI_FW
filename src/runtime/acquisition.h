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
    /// 设备协议查询（未找到设备/无设备名 → ModbusTcp——兼容 Z/Y 时代「modbus:// 一律走 TCP」行为）
    ProtocolType protocolOfDevice(const QString& deviceName) const;
    /// 按 tag 路由协议（AB-2 2026-09-11）：mqtt:// → mqtt；modbus:// → 依所属设备协议取
    /// modbus_rtu / modbus_tcp（设备未知 → modbus_tcp 兼容旧行为）；其余/空来源 → 空串（不采集）
    QString protocolForTag(const Tag& tag) const;
    /// 销毁全部驱动（工程重载/析构用）
    void destroyDrivers();

    Project m_project;
    DataManager* m_dataManager = nullptr;
    QTimer* m_timer = nullptr;
    QList<IDriver*> m_drivers;            // Y-4: 多协议驱动列表（modbus_rtu/tcp×设备 + mqtt×连接 并存）
    /// Z 循环：driver → 本连接配置映射（StatusTag 回写按连接找 status_tag；驱动销毁时同步清）
    QHash<const IDriver*, const MqttConnectionConfig*> m_driverConns;
    /// AB-3（2026-09-11）：driver → 所属设备名（modbus 每（协议,设备）一实例——写值/日志按设备定位；
    /// 空串 = 无设备（裸 modbus:// 变量走默认连接））
    QHash<const IDriver*, QString> m_driverDevices;
    /// StatusTag 未声明告警去重（每 tag 首现 qWarning——reviewer Z-5 🟡6）
    QSet<QString> m_statusTagWarned;
    /// AB-2：无归属设备的 modbus 变量告警计数（最多 3 条，防日志刷屏）
    int m_modbusNoDeviceWarned = 0;
    QHash<QString, QVariant> m_lastVal;   // deadband 判断（跨协议统一语义——原 Acquisition per-tag lastVal 上移）
    /// AB-5（2026-09-11）：读回环一次性标记（采集 setValue 前挂 → handleValueWritten 首次消费；
    /// 值与采集值相同才抑制设备写回——防只读寄存器被回写，同时不吞用户写回旧值）
    QHash<QString, QVariant> m_ringBack;
};

} // namespace navihmi
