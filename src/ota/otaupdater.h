/*
 * @FilePath: \NavigatorHMI_FW\src\ota\otaupdater.h
 * @Description: D1 OTA 固件安装器（2026-08-30 批 2；O-D D-3 2026-09 扩展 rootfs 文件级）。
 *               组件类型（用户 2026-08-30 分组）：app（/usr/bin/navigatorhmi-fw，常规小包主路径）/
 *               rootfs（**文件级更新集**——2026-09 板端勘察调整：backup 分区仅 32MB 装不下整 rootfs 镜像，
 *               boot 64MB>32MB 也无法分区写兜底；用户裁决 rootfs 走**文件级更新 + userdata 备份回滚**，
 *               payload = 连续文件段清单：路径+内容，安装 = 逐文件备份旧版到 userdata → 写新 → 校验 → 重启）/
 *               kernel（boot.img，因 backup 容量不足本轮不做——留专用烧录工具，同 uboot 策略）。
 *               策略（v1.1-design §5.2 D1 + 执行书 D1 + O-D D-3）：
 *                 app：备份当前二进制 → 写新 → 校验 → 重启生效（应用层替换，无分区写，安全主路径）；
 *                 rootfs（文件级）：每个文件备份旧版到 /mnt/user/userdata/ota-backup/<fwver>/ → 写新（原子 tmp+rename）
 *                   → 整体 sha 已由 httreceiver 校验；启动失败回滚 = start_runtime/守护从 userdata 备份恢复
 *                   （bootFailCount 接线，O-D D-3b）；
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

    /// O-D D-3b：启动失败计数递增（main() 启动失败路径调用——loadAndInject/engine 失败即新固件不能启动）
    /// 返回递增后的计数；>= kMaxBootFail 时应执行回滚（由调用方决定——main 失败路径无法自恢复，需脚本层）
    int recordBootFail();

    /// O-D D-3b：从 userdata ota-backup 恢复上一次 rootfs 文件级备份（启动失败 >=N 次后由 start_runtime/守护调用）。
    /// 返回空串=成功；非空=失败原因。latestOnly=true 只恢复最新一次备份。
    QString restoreFromUserdataBackup(bool latestOnly = true);

    /// O-D D-3b：rootfs 文件级安装后重启前调用——写期望 md5 等（对齐 app 的 expected-md5 语义不需要——
    /// 文件级已逐文件备份，恢复走 restoreFromUserdataBackup；本方法供扩展）
    void markRootfsInstalled(const QString& fwVersion);

    /// 检查连续失败次数（/mnt/user/userdata/.boot_fail 计数）——O-D D-3b 接线（main 启动失败路径递增；
    /// main.cpp 启动前置回滚判定需要 → public）
    int bootFailCount() const;

    /// 回滚阈值（连续失败达 N 次触发 userdata 备份恢复）——main 启动前置检查用（public）。
    static constexpr int maxBootFailForRestore() { return kMaxBootFail; }

signals:
    /// 安装进度（0~100 + 阶段）——目前未接线到 /api/progress（OtaUpdater 无法写 HttpReceiver 私有成员；
    /// 安装期间进度停留在 staging 写入的 100，可接受；如需细分后续经信号桥接）
    void progress(int percent, const QString& stage);

    /// 批 D 收尾（httreceiver「假成功」缺陷修复）：安装结束（成功=空错误串 / 失败=原因）——
    /// main.cpp 桥接到 HttpReceiver → 以**真实安装结果**写 .fw 传输响应（替代原「校验通过即报成功」）
    void installFinished(const QString& error);

private:
    /// 解析 .fw 组件表（返回 (name,type,target,size,sha,payloadOffset) 列表；失败返回空）
    struct Component { QString name; QString type; QString target; quint64 size; QString sha; qint64 offset; QString version; };
    QList<Component> parseComponents(const QByteArray& body, QString& error) const;

    /// app 组件安装（备份→写新→校验→重启）；返回空串成功
    QString installApp(const Component& comp, const QByteArray& payload, int& progressOut);
    /// O-D D-3b：rootfs **文件级**安装（payload = 连续文件段：4B LE 路径长+路径 UTF8 + 8B LE 内容长+内容，循环）；
    /// 逐文件备份旧版到 userdata/ota-backup → 写新（tmp+rename 原子）→ 校验；返回空串成功。
    /// 路径白名单：相对路径禁 `..` 穿越（防覆盖任意系统文件——payload 可能被 PC 端信任侧打包，仍防御）
    QString installRootfsFiles(const Component& comp, const QByteArray& payload, int& progressOut);
    /// rootfs/kernel 分区写（backup 分区兜底 + 主分区写 + 回滚标记）——O-D D-3 板端勘察：backup 32MB 装不下
    /// rootfs/boot，分区写路径本轮不开放（保留接口注释历史）
    QString installPartition(const Component& comp, const QByteArray& payload);

    static constexpr int kMaxBootFail = 3;      // 连续失败 N 次恢复备份（maxBootFailForRestore 引用）
    static constexpr const char* kBootOkPath = "/mnt/user/userdata/.boot_ok";
    static constexpr const char* kBootFailPath = "/mnt/user/userdata/.boot_fail";
    static constexpr const char* kOtaBackupRoot = "/mnt/user/userdata/ota-backup";   // O-D D-3：rootfs 文件级备份根
};

} // namespace navihmi
