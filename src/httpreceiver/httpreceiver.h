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
    /// M-3 ④（R1 修复）：transfer 改为 responder 异步——qthttpserver 6.4 处理器跑在服务器对象线程（=GUI 主线程），
    /// receiveAndInstall 解压/校验/落盘为秒级耗时，同步执行会冻结事件循环 → 进度条无法重绘、触摸/VNC 无响应；
    /// 后台线程执行安装，完成后经 finishTransfer 回主线程写响应（QTcpSocket 非线程安全）
    void handleTransfer(const QHttpServerRequest& request, QHttpServerResponder&& responder);
    /// 主线程收尾：写 HTTP 响应 + 释放并发锁 + 信号（成功→projectPackageReady；失败→progress(-1)）
    /// D1：isFirmware=true 时成功不发 projectPackageReady（OTA 走 firmwarePackageReady 安装重启）
    void finishTransfer(const QString& error, const QString& projectPath, bool isFirmware = false);
    QHttpServerResponse handleVnc(const QHttpServerRequest& request);    // K-9：POST /api/vnc {enable}
    QHttpServerResponse handleBlink(const QHttpServerRequest& request);  // K-9：POST /api/blink {enable}
    /// 校验 + 落盘；成功返回空错误串并输出 projectPath，失败返回原因（后台线程执行——只碰局部/线程安全成员）
    /// D1 审查 🔴：isFirmware 出参标识本次为 .fw OTA 传输——finishTransfer 据此跳过 projectPackageReady（工程重载）
    QString receiveAndInstall(const QByteArray& body, QString& projectPathOut, bool* isFirmware = nullptr);
    /// D1：.fw 固件包处理（NHFW 魔数嗅探分支）——校验 header sha + 组件表 → 写 staging → 触发 firmwarePackageReady；
    /// 成功返回空错误串并输出 staging 路径，失败返回原因（后台线程执行）
    QString handleFirmwarePackage(const QByteArray& body, QString& stagingPathOut);
    /// JSON 响应构造（Content-Type application/json）
    QHttpServerResponse jsonResponse(const QJsonObject& obj, QHttpServerResponse::StatusCode status);
    /// D-B4：进度值 → 阶段描述（派生，避免跨线程共享 QString）
    static QString stageForProgress(int pct);
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
};

} // namespace navihmi
