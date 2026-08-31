/*
 * @FilePath: \NavigatorHMI_FW\src\ota\otaupdater.h
 * @Description: D1 OTA 固件安装器（2026-08-30 批 2）——消费 httreceiver 校验通过的 .fw staging 包。
 *               组件类型（用户 2026-08-30 分组）：app（/usr/bin/navigatorhmi-fw，常规小包主路径）/
 *               rootfs（rootfs.img，系统级大包，风险项）/ kernel（boot.img = 内核+dtb+resource）。
 *               策略（v1.1-design §5.2 D1 + 执行书 D1）：
 *                 app：备份当前二进制 → 写新 → 校验 → 重启生效（应用层替换，无分区写，安全主路径）；
 *                 rootfs/kernel：backup 分区兜底 + 软件回滚标记——**本轮默认整体拒绝（install() all-or-nothing
 *                   预检，任何写入前返回），分区操作需板端实测验证（rknetupdate-upgrade-hang 先例）后再放开**
 *                   （见 installPartition 预留接口注释）；
 *                 断电保护：先完整校验 staging（httreceiver 已做 sha）+ tmp 落盘校验后 ::rename 原子替换。
 *               版本检查前置由 PC 端 DeployFirmwareHandler 完成；本模块只做安装。
 */
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace navihmi {

class OtaUpdater : public QObject
{
    Q_OBJECT
public:
    explicit OtaUpdater(QObject* parent = nullptr);

    /// 安装 staging .fw 包（httreceiver 校验通过后触发）。成功返回空错误串；失败返回原因。
    /// 主线程调用（firmwarePackageReady 信号 AutoConnection 队列到主线程）。
    QString install(const QString& stagingFwPath);

    /// 启动成功标记（软件回滚依据）——由 main() 在新固件启动成功（engine.load + 工程加载 OK）后调用；
    /// 审查 🔴：不得在 install 内重启前调用（标记未验证的启动为成功使回滚失效）
    void markBootOk();

signals:
    /// 安装进度（0~100 + 阶段）——目前未接线到 /api/progress（OtaUpdater 无法写 HttpReceiver 私有成员；
    /// 安装期间进度停留在 staging 写入的 100，可接受；如需细分后续经信号桥接）
    void progress(int percent, const QString& stage);

private:
    /// 解析 .fw 组件表（返回 (name,type,target,size,sha,payloadOffset) 列表；失败返回空）
    struct Component { QString name; QString type; QString target; quint64 size; QString sha; qint64 offset; };
    QList<Component> parseComponents(const QByteArray& body, QString& error) const;

    /// app 组件安装（备份→写新→校验→重启）；返回空串成功
    QString installApp(const Component& comp, const QByteArray& payload, int& progressOut);
    /// rootfs/kernel 组件安装（backup 分区兜底 + 主分区写 + 回滚标记）——板端实测开关（当前由预检整体拒绝）
    QString installPartition(const Component& comp, const QByteArray& payload);
    /// 检查连续失败次数（/mnt/user/userdata/.boot_fail 计数）——⚠️ 本轮未接线（软件回滚恢复逻辑待
    /// 后续批：main() 启动失败路径递增 + bootFailCount>=N 从 backup 恢复；当前为预留接口）
    int bootFailCount() const;

    static constexpr int kMaxBootFail = 3;      // 连续失败 N 次恢复 backup
    static constexpr const char* kBootOkPath = "/mnt/user/userdata/.boot_ok";
    static constexpr const char* kBootFailPath = "/mnt/user/userdata/.boot_fail";
};

} // namespace navihmi
