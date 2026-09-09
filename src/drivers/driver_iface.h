/*
 * @FilePath: \NavigatorHMI_FW\src\drivers\driver_iface.h
 * @Description: 采集驱动接口（V-1 2026-09-06 驱动插件化——协议实现可插拔）
 *               Modbus TCP 首迁（行为不变回归锚）；MQTT/Modbus RTU V+1 挂入同一框架
 *               设计源: v1.1-design.md L244-249（方向对齐：注册/裁剪/信号回传；接口为 V-1 最小集，
 *               三列表 Cycle/Request/Update 与批量读合并 V+1 落地——勿按 L245 终态契约误判本接口已含）
 *               分层: Manager(Acquisition) = tag 解析分组/周期调度/deadband/DataManager 写入
 *                     Driver = 连接 + 协议读写 + 解码（纯协议层，回 valueRead 信号）
 */
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QHash>
#include <QList>

namespace navihmi {

struct MqttConnectionConfig;   // 前向声明（projectmodel.h——Acquisition 按连接注入，driver 配置用；Z 循环多连接）

/// 采集项（Manager 解析工程 Tag 后统一传给 Driver；Driver 自行解析 source/类型/连接参数）
struct DriverTagInfo {
    QString name;
    QString source;         // "modbus://{slave}/{reg}" / "mqtt://..."；空 = 内部变量不采集
    QString deviceName;     // 关联设备名（连接参数来源——modbus；Z 循环 MQTT 连接参数内联不用）
    int dataType = 0;       // TagDataType 数值（Bool/Int16/Uint16/Int32/Float/String/DateTime/Gps）
    int scanMs = 0;         // 采集周期 ms（<=0 用驱动默认）
    double deadband = 0;    // 死区（Manager 写入 DataManager 前判断）
    QHash<QString, QString> conn;   // 连接参数（deviceName → connectionInfo JSON 键值展开；空=驱动默认——modbus）
    const MqttConnectionConfig* mqttConn = nullptr;   // Z 循环（2026-09-11）：所属 MQTT 连接（每连接一驱动，
                                                      // Acquisition 按连接注入；弃 Y 时代整份 MqttSettings 单连接语义）
};

/// 采集驱动接口。生命周期: configure → start → poll×N（Manager 100ms 调度）→ stop。
/// 值回传统一 valueRead(tagName, 解码值, deadband)——Manager 做死区 + DataManager.setValue（跨协议统一语义）。
class IDriver : public QObject
{
    Q_OBJECT
public:
    explicit IDriver(QObject* parent = nullptr) : QObject(parent) {}
    ~IDriver() override = default;

    /// 协议标识（"modbus_tcp"/"modbus_rtu"/"mqtt"...）——注册表路由
    virtual QString protocol() const = 0;
    /// 配置采集项（driver 内部按 source 细解析；返回 true = 有本协议有效项可启动）
    virtual bool configure(const QList<DriverTagInfo>& tags) = 0;
    /// 启动连接（driver 内部实现断线自动重连/退避）
    virtual void start() = 0;
    /// 停止并断开
    virtual void stop() = 0;
    /// 周期轮询（Manager 定时调用；到期项发读请求，异步收包完成后 emit valueRead）
    virtual void poll(qint64 nowMs) = 0;
    /// 写通道（DataManager 值变化 → 设备写）；返回 false = 未连接/无此 tag
    virtual bool writeValue(const QString& tagName, const QVariant& value) = 0;

signals:
    /// 读值完成（Manager 做 deadband + DataManager.setValue）
    void valueRead(const QString& tagName, const QVariant& value, double deadband);
    /// 连接状态变化（true=已连接）
    void stateChanged(bool connected);
    /// 连接失败/错误（Manager 上报日志/状态 tag——V+1 状态诊断用）
    void connectionError(const QString& message);
    /// MQTT 连接状态 4 态（Z 循环 2026-09-11 StatusTag 回写；值 = MqttConnState 0-3——
    /// Disconnected/Connecting/Connected/Error；仅 MQTT 驱动 emit——modbus 用 stateChanged(bool)）
    void connectionStateChanged(int state);
};

/// 驱动工厂（注册表登记；extern C init_driver_xxx() 返回实例——宏裁剪时可不注册）
class IDriverFactory
{
public:
    virtual ~IDriverFactory() = default;
    virtual QString protocol() const = 0;
    virtual IDriver* create(QObject* parent) = 0;
};

} // namespace navihmi
