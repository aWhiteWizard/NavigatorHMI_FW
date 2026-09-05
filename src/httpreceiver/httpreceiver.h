/*
 * @FilePath: \NavigatorHMI_FW\src\httpreceiver\httpreceiver.h
 * @Description: K-8b FW 接收端（QtHttpServer）——POST /api/transfer 接收工程部署容器（zip, manifest+app+res）
 *               → SHA256 校验 → 原子落盘 → 信号触发工程重载（复用 storageInfo.projectReplaced 链）
 *               端点：GET /api/device/info（型号/尺寸/ID/固件版本）· GET /api/version（工程版本）· POST /api/transfer
 *               约束：单客户端串行（并发上限 1）；校验失败不动当前工程（下载事务性）
 */
#pragma once

#include <QObject>
#include <QHttpServer>
#include <QString>
#include <QAtomicInteger>
#include <QTimer>   // T-1a：分块会话空闲 TTL（审查 🔴 1-2——PC 断连后自动清理，防永久拒收）
#include <memory>   // M-3 ④：std::unique_ptr<QHttpServerResponder>（异步传输响应器）

class QHttpServerRequest;
class QHttpServerResponder;

namespace navihmi {

class RuntimeBus;
class DeviceInfo;
class VncMirror;

class HttpReceiver : public QObject
{
    Q_OBJECT
public:
    explicit HttpReceiver(RuntimeBus* bus, DeviceInfo* devInfo, QObject* parent = nullptr);
    ~HttpReceiver() override;

    /// 启动 HTTP 监听（默认 80 端口）；失败返回 false（FW 继续正常运行）
    bool start();

    /// 是否正在传输（并发上限 1 判定）
    bool isTransferActive() const { return m_transferActive.loadRelaxed(); }

    /// K-9：注入 VNC 镜像（VncMirror 构造晚于 HttpReceiver——engine.load 后调用）
    void setVncMirror(VncMirror* vm);

    /// D-B4：最近一次传输进度（0~100；-1=无/失败；原子存储供 GET /api/progress 跨线程读取）
    int lastProgress() const { return m_lastProgress.loadRelaxed(); }
    /// D-B4：最近一次传输阶段描述——由 progress 值派生（后台线程只写原子进度，stage 无跨线程共享，防 QString 数据竞争）
    QString lastProgressStage() const { return stageForProgress(m_lastProgress.loadRelaxed()); }
    /// D-B4：是否传输中（GET /api/progress 的 active 字段——并发锁状态）
    bool transferActive() const { return m_transferActive.loadRelaxed(); }

    /// 批 D 收尾（httreceiver「假成功」缺陷修复）：.fw OTA 安装结果回调（main.cpp 桥接 OtaUpdater.installFinished →
    /// 本方法，普通方法非信号）——以**真实安装结果**写 .fw 传输响应（原实现校验通过即报成功，install 失败静默误判成功）
    void completeFirmwareInstall(const QString& error);

signals:
    /// 校验通过、容器已落盘 → 主程序重载工程（projectPath=app.navihmi 路径）
    void projectPackageReady(const QString& projectPath);

    /// D1：.fw 固件包校验通过、staging 就绪 → 主程序触发 OTA 安装（otaupdater；stagingPath 指向 staging .fw）
    void firmwarePackageReady(const QString& stagingPath);

    /// K-9：设备闪烁请求（QML 覆盖层亮灭交替 ~1s；enable=false 停止恢复原画面）
    void blinkRequested(bool enable);

    /// M-3 ④：下载/安装进度回报（0~100；percent<0 = 失败恢复）——QML 屏幕进度条（退导航→进度→满停 1~2s→自动打开）
    void transferProgress(int percent, const QString& stage);

private:
    void setupRoutes();
    QHttpServerResponse handleDeviceInfo();
    QHttpServerResponse handleVersion();
    QHttpServerResponse handleProgress();   // D-B4：GET /api/progress——PC 轮询设备端进度（真同步）
    QHttpServerResponse handleLog();        // Q-1（2026-09-04）：GET /api/log——设备运行日志尾部（/tmp/navihmi.log 32KB，诊断/视频排障用）
    /// M-3 ④（R1 修复）：transfer 改为 responder 异步——qthttpserver 6.4 处理器跑在服务器对象线程（=GUI 主线程），
    /// receiveAndInstall 解压/校验/落盘为秒级耗时，同步执行会冻结事件循环 → 进度条无法重绘、触摸/VNC 无响应；
    /// 后台线程执行安装，完成后经 finishTransfer 回主线程写响应（QTcpSocket 非线程安全）
    void handleTransfer(const QHttpServerRequest& request, QHttpServerResponder&& responder);
    /// T-1a（2026-09-05 T 循环）：分块上传会话处理（请求头 X-Tf-Id/X-Tf-Index/X-Tf-Total/X-Tf-Size）——
    /// qthttpserver 6.4 请求体整读无流式 API → PC 分 N 块循环 POST（每块 4MB 级）：
    /// 首块（index=0）建会话（/mnt/user/userdata/.transfer/<id>.zip）+ cap=disk_free×2/3 预检；
    /// 续块 append 落盘（单块内存小，大包不受内存限制）+ 块进度 m_lastProgress=(idx+1)/total×70；
    /// 末块（index=total-1）转后台 receiveAndInstallFromZip 安装链（70→100 刻度）；失败/新会话清理。
    /// 整包旧路径（无分块头）保持 handleTransfer 原流程（≤256MB 内存兜底校验）。
    void handleChunkedTransfer(const QHttpServerRequest& request, QHttpServerResponder&& responder);
    /// T-1a：分块会话异常清理（删临时 zip + 复位会话状态；主线程）
    void abortChunkSession();
    /// T-1a：分块会话空闲超时（审查 🔴 1-2——块间 TTL 30s：PC 中途断连/崩溃后自动清理 + 复位并发锁 + 进度 -1，
    /// 设备不永久拒收；末块交棒后 stop）
    void onChunkTimeout();
    /// 主线程收尾：写 HTTP 响应 + 释放并发锁 + 信号（成功→projectPackageReady；失败→progress(-1)）
    /// D1：isFirmware=true 时成功不发 projectPackageReady（OTA 走 firmwarePackageReady 安装重启）
    void finishTransfer(const QString& error, const QString& projectPath, bool isFirmware = false);
    QHttpServerResponse handleVnc(const QHttpServerRequest& request);    // K-9：POST /api/vnc {enable}
    QHttpServerResponse handleBlink(const QHttpServerRequest& request);  // K-9：POST /api/blink {enable}
    /// 校验 + 落盘；成功返回空错误串并输出 projectPath，失败返回原因（后台线程执行——只碰局部/线程安全成员）
    /// D1 审查 🔴：isFirmware 出参标识本次为 .fw OTA 传输——finishTransfer 据此跳过 projectPackageReady（工程重载）
    QString receiveAndInstall(const QByteArray& body, QString& projectPathOut, bool* isFirmware = nullptr);
    /// T-1a：从已落盘 zip 执行安装链（分块路径末块入口 + 整包路径写临时后共用——414 行后段复用）；
    /// 安装段进度刻度 70~100（解压 70→85、写入 85→99、完成 100）；仅工程容器；后台线程执行
    QString receiveAndInstallFromZip(const QString& zipPath, QString& projectPathOut);
    /// D1：.fw 固件包处理（NHFW 魔数嗅探分支）——校验 header sha + 组件表 → 写 staging → 触发 firmwarePackageReady；
    /// 成功返回空错误串并输出 staging 路径，失败返回原因（后台线程执行）
    QString handleFirmwarePackage(const QByteArray& body, QString& stagingPathOut);
    /// JSON 响应构造（Content-Type application/json）
    QHttpServerResponse jsonResponse(const QJsonObject& obj, QHttpServerResponse::StatusCode status);
    /// D-B4：进度值 → 阶段描述（派生，避免跨线程共享 QString）
    static QString stageForProgress(int pct);
    /// 批 D 收尾：写 .fw OTA 传输最终响应（completeFirmwareInstall 内部）——install 结果成功/失败分支
    void writeFirmwareTransferResult(const QString& error);
    /// 设备型号（设备自身硬件身份：物理屏默认分辨率查 device-profiles.json，与工程无关；同 devicemeta 单点）
    QString deviceModel() const;
    /// 设备尺寸（"7寸"/"4寸"，型号查表；同 devicemeta 单点）
    QString deviceSizeInch() const;

    RuntimeBus* m_bus = nullptr;
    DeviceInfo* m_deviceInfo = nullptr;
    VncMirror* m_vncMirror = nullptr;   // K-9
    QHttpServer m_server;
    QAtomicInteger<bool> m_transferActive { false };
    QAtomicInteger<int> m_lastProgress { -1 };   // D-B4：最近传输进度（-1=无/失败；原子跨线程读，后台线程 storeRelaxed）
    std::unique_ptr<QHttpServerResponder> m_responder;   // M-3 ④：活动传输的异步响应器（单客户端串行，唯一持有者）
    bool m_pendingFirmwareInstall = false;   // 批 D 收尾：.fw OTA 校验通过已触发安装，等待 installFinished 回传真实结果
    // ── T-1a（2026-09-05 T 循环）：分块上传会话状态（m_transferActive 统一并发锁；主线程读写）──
    QString m_chunkId;                   // 分块会话 id（X-Tf-Id；空=无活动分块会话）
    QString m_chunkZipPath;              // 分块临时 zip（deployDir/.transfer/<id>.zip——/tmp 为 tmpfs 980M 放不下大包）
    qint64 m_chunkTotalSize = 0;         // 总字节（首块 X-Tf-Size；cap 预检）
    qint64 m_chunkReceivedSize = 0;      // 已收字节（cap 累计校验双保险）
    int m_chunkNextIndex = 0;            // 期望下一块序号（顺序校验：块丢失/乱序拒绝）
    int m_chunkTotal = 0;                // 总块数（X-Tf-Total——首块记录，续块头须一致；末块判定以本值而非请求头）
    QTimer m_chunkTimer;                 // 分块会话空闲 TTL（单次 30s；每块到达 restart；末块交棒 stop）
};

} // namespace navihmi
