/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\acquisition.h
 * @Description: 数据采集引擎（H-8）——Modbus RTU/TCP 轮询读 + 写通道
 *               Tag.source 约定: modbus://{从站}/{寄存器}（01_architecture/DESIGN-WINDOWS）
 *               首版: Modbus TCP 轮询读 + 写通道; RTU/MQTT 二期（板端 Qt SerialBus 已装, MQTT 缺库）
 *               连接参数: Tag.deviceName → DeviceConfig.connectionInfo JSON（host/port/serial）
 */
#pragma once

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QTimer>
#include "runtime/projectmodel.h"

class QModbusTcpClient;

namespace navihmi {
class DataManager;

/// Modbus 请求超时（ms）——影响读/写成败判定（2026-08-26 魔法数字整改命名）。
constexpr int kModbusTimeoutMs = 500;
/// Modbus 从站地址上限（协议标准 1-247）。
constexpr int kModbusSlaveMax = 247;
/// Modbus 默认端口（IANA 标准）。
constexpr char kModbusDefaultPort[] = "502";

class Acquisition : public QObject
{
    Q_OBJECT
public:
    explicit Acquisition(QObject* parent = nullptr);
    ~Acquisition() override;

    void setProject(const Project& proj);
    void setDataManager(DataManager* dm);
    /// H-8 写通道：DataManager 写 modbus 来源变量 → 同步写设备（main.cpp 联动）
    void handleValueWritten(const QString& tagName, const QVariant& value);

private:
    // 采集任务项（按 Tag.source 解析）——须先于成员函数声明（签名引用嵌套类型）
    struct ModbusTag {
        QString tagName;
        int slave = 1;
        quint16 reg = 0;
        int dataType = 0;        // TagDataType
        int scanMs = 0;
        double deadband = 0;
        QHash<QString, QString> conn;   // host/port/serial/baud
        qint64 lastReadMs = 0;
        QVariant lastVal;
    };

    void tick();          // 100ms 调度: 到期变量轮询读
    void ensureConnected();
    void readTag(const ModbusTag* tag);
    void writeTag(const QString& tagName, const QVariant& value);   // 写通道（DataManager 联动）
    QVariant decodeValue(const ModbusTag& tag, quint16 raw) const;
    QHash<QString, QString> connInfoForDevice(const QString& deviceName) const;

    Project m_project;
    DataManager* m_dataManager = nullptr;
    QList<ModbusTag> m_tags;
    QTimer* m_timer = nullptr;
    QModbusTcpClient* m_client = nullptr;
    bool m_connecting = false;
    QString m_connectedDevice;   // 当前连接设备（连接参数变更时重连）
    QHash<QString, QString> m_conn;   // 当前连接参数
    qint64 m_lastConnectFailMs = 0;      // J-3: 最近一次连接失败时间戳（重连退避 5s）
    qint64 m_lastConnectWarnMs = 0;      // J-3: 最近一次连接失败告警时间戳（日志降频）
};

} // namespace navihmi
