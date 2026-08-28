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

class QHttpServerRequest;

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

signals:
    /// 校验通过、容器已落盘 → 主程序重载工程（projectPath=app.navihmi 路径）
    void projectPackageReady(const QString& projectPath);

    /// K-9：设备闪烁请求（QML 覆盖层亮灭交替 ~1s；enable=false 停止恢复原画面）
    void blinkRequested(bool enable);

private:
    void setupRoutes();
    QHttpServerResponse handleDeviceInfo();
    QHttpServerResponse handleVersion();
    QHttpServerResponse handleTransfer(const QHttpServerRequest& request);
    QHttpServerResponse handleVnc(const QHttpServerRequest& request);    // K-9：POST /api/vnc {enable}
    QHttpServerResponse handleBlink(const QHttpServerRequest& request);  // K-9：POST /api/blink {enable}
    /// 校验 + 落盘；成功返回空错误串并输出 projectPath，失败返回原因
    QString receiveAndInstall(const QByteArray& body, QString& projectPathOut);
    /// JSON 响应构造（Content-Type application/json）
    QHttpServerResponse jsonResponse(const QJsonObject& obj, QHttpServerResponse::StatusCode status);
    /// 设备型号（按工程分辨率推导：1024x600→NavigatorHMI-7 / 720x720→NavigatorHMI-4）
    QString deviceModel() const;
    /// 设备尺寸（"7寸"/"4寸"）
    QString deviceSizeInch() const;

    RuntimeBus* m_bus = nullptr;
    DeviceInfo* m_deviceInfo = nullptr;
    VncMirror* m_vncMirror = nullptr;   // K-9
    QHttpServer m_server;
    QAtomicInteger<bool> m_transferActive { false };
};

} // namespace navihmi
