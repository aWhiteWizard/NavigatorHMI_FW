/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\modbus_driver_base.h
 * @Description: Modbus 双传输公共基类（AB-2 2026-09-11）
 *              把 V-1 起落在 modbus_tcp_driver 内、与传输无关的逻辑上提共用：
 *                tag 解析（source → slave/reg/类型/周期）、寄存器与值范围双闸、
 *                Float /100 编解码约定、写回复错误检查、轮询调度、
 *                连接失败 5s 退避 + 告警降频。
 *              TCP/RTU 子类只实现传输层三件事：建 client、落连接参数、日志目标描述。
 *              行为回归锚：TCP 侧语义与 V-1/Y-7 完全一致（本批为纯代码搬家 + RTU 复用）。
 *              设计源: v1.1-design.md / AB-execution-plan.md §AB-2。
 */
#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include "drivers/driver_iface.h"

class QModbusClient;

namespace navihmi {

/// Modbus 请求超时（ms）——影响读/写成败判定（2026-08-26 魔法数字整改命名）。
constexpr int kModbusTimeoutMs = 500;
/// Modbus 从站地址上限（协议标准 1-247）。
constexpr int kModbusSlaveMax = 247;
/// Modbus 默认端口（IANA 标准）。
constexpr char kModbusDefaultPort[] = "502";
/// Modbus RTU 默认波特率（PT100 模块出厂默认 9600 8N1）。
constexpr int kModbusRtuDefaultBaud = 9600;

/// Modbus 采集任务项（configure 时按 source 细解析——原 ModbusTcpDriver::ModbusTag）
struct ModbusTag {
    QString tagName;
    int slave = 1;
    quint16 reg = 0;
    int dataType = 0;        // TagDataType
    int scanMs = 0;
    double deadband = 0;     // 回传 Manager 判断（保留字段供 configure 聚合）
    qint64 lastReadMs = 0;
    /// 🟡-4（AB-2 审查 2026-09-11）：在途请求保护——scanMs（PC 默认 100ms）< 超时 500ms 时，
    /// 无响应链路会持续入队（Qt RTU 无背压），恢复后回放陈旧值；发请求置位、reply finished 清除
    bool inFlight = false;
    /// 🔴-1（复审 2026-09-11）：在途起始时刻——Qt RTU 重试路径可能丢失 finished（写失败不再武装响应
    /// 定时器 + 端口级错误不改 state），必须由 poll 看门狗兜底复位，否则该变量永久静默停采
    qint64 inFlightSinceMs = 0;
};

/// Modbus 采集驱动公共基类（Manager 只认 IDriver；本类实现 configure/poll/writeValue 公共面）
class ModbusDriverBase : public IDriver
{
    Q_OBJECT
public:
    explicit ModbusDriverBase(QObject* parent = nullptr);
    ~ModbusDriverBase() override;

    bool configure(const QList<DriverTagInfo>& tags) override;
    void start() override;
    void stop() override;
    void poll(qint64 nowMs) override;
    bool writeValue(const QString& tagName, const QVariant& value) override;

protected:
    /// 传输层：建 client（父对象须为 this）；返回 nullptr = 传输不可用
    virtual QModbusClient* createClient() = 0;
    /// 传输层：connectDevice 之前把 m_conn 落到 client 连接参数
    virtual void applyConnectionParameters(QModbusClient* client) = 0;
    /// 日志用连接目标描述（如 "192.168.1.50:502" / "/dev/ttyUSB0@9600"）
    virtual QString targetDescription() const = 0;
    /// 连接参数兜底/校验：返回 false = 连接参数不可用（configure 失败，不建驱动）
    virtual bool ensureConnectionParams() = 0;
    /// 日志前缀（"ModbusTcpDriver"/"ModbusRtuDriver"）
    virtual QString logTag() const = 0;

    QHash<QString, QString> m_conn;      // 当前连接参数（本实例所属设备的 connection_info）
    QList<ModbusTag> m_tags;
    QModbusClient* m_client = nullptr;
    bool m_connecting = false;
    bool m_clientCreateFailed = false;   // 🟡-1（AB-2 审查）：createClient 失败只告警一次（防 100ms 刷屏）
    qint64 m_lastReadFailMs = 0;         // 🟡-3：最近读失败时间戳（错误日志 5s 降频）
    qint64 m_lastInFlightWarnMs = 0;     // 🔴-1：在途看门狗告警节流槽（独立于读失败槽，防互相压制）
    int m_readFailCount = 0;             // 🟡-3：累计读失败数（恢复时汇总一条）
    qint64 m_lastPortResetMs = 0;        // 🔴-1（复审）：端口级错误断开重连的节流时间戳
    qint64 m_lastConnectFailMs = 0;      // J-3: 最近连接失败时间戳（重连退避 5s）
    qint64 m_lastConnectWarnMs = 0;      // J-3: 最近连接失败告警时间戳（日志降频）

private:
    void ensureConnected();
    void readTag(const ModbusTag* tag);
    QVariant decodeValue(const ModbusTag& tag, quint16 raw) const;
    /// 🟡-4：清除在途标记（reply finished / 同步失败路径）
    void clearInFlight(const QString& tagName);
    /// 🔴-1：清除全部在途标记（断连/停止路径兜底）
    void clearAllInFlight();
    /// 🟡-3：读失败记录（5s 降频告警 + 累计计数）
    void reportReadFailure(const QString& tagName, const QString& reason);
    /// 🟡-3：读成功恢复（有失败累计时汇总一条）
    void reportReadSuccess();
};

} // namespace navihmi
