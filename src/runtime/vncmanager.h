/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncmanager.h
 * @Description: VNC 管理层（W-D F19 三层职责拆分——配置层 VncConfig / 管理层 VncManager / 服务层 VncServer）。
 *               职责：监听（QTcpServer）+ 会话生命周期（客户端接入 → 建 VncServer；断开清理）+ 连接状态
 *               （首/末客户端 → QML vncMirrorActive 动画驱动开关）+ 帧消费调度（渲染线程 afterRendering
 *               读生产端脏矩形 → 节流读帧 → 分派各会话推送）。QML 入口（contextProperty "vncMirror"，
 *               名称保持不变——QML 侧 markDirty 调用零改动）。
 *               ⚠️ 逻辑迁移自原 vncmirror.cpp（行为零改动，仅按职责归档）。线程模型同原实现：
 *               markDirty/会话增删/发送在 GUI 线程；onAfterRendering 在渲染线程（frameSwapped
 *               DirectConnection），经 m_serversMutex/m_dirtyMutex 与 GUI 线程互斥。
 */
#pragma once

#include <QObject>
#include <QImage>
#include <QRect>
#include <QVector>
#include <QMutex>
#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QAtomicInteger>

#include "runtime/vncconfig.h"

class QTcpServer;
class QTcpSocket;
class QQuickWindow;
class QOpenGLVertexArrayObject;
namespace navihmi {

class VncServer;

class VncManager : public QObject
{
    Q_OBJECT
public:
    explicit VncManager(QQuickWindow* window, QObject* parent = nullptr);
    ~VncManager() override;

    /// 设备尺寸（VNC 屏幕尺寸 = 设备物理屏分辨率——N+24 用户裁决：按连接设备型号查表
    /// devicemeta.deviceResolutionFor()，与工程 deviceWidth/Height 解耦；如 7 寸 1024x600）
    void setDeviceSize(int w, int h);

    /// 启动监听（端口由调用方传入 = navihmi::vncPort()；监听失败返回 false 并记日志）
    bool start(quint16 port);
    void stop();
    bool isRunning() const { return m_server != nullptr; }

    /// QML 生产端报告画面变化区域（西门子 dirty-rect 模式：QML 层知道哪里变了，
    /// VNC 只读该区域全分辨率，绕开全帧 792ms 读回）。线程安全。
    Q_INVOKABLE void markDirty(int x, int y, int w, int h);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onFramebufferRequested();   // VncServer 请求帧更新 → 节流触发 window->update()

private:
    /// 渲染线程：读帧 + 分派推送（frameSwapped DirectConnection）
    void onAfterRendering();
    /// 渲染线程：读指定矩形区域全分辨率 → BGRA（垂直翻转）
    bool readRegion(QOpenGLFunctions* f, const QRect& r, int w, int h, QByteArray& bgra);
    /// RGBA(左下) → BGRA(左上, 垂直翻转)
    static QByteArray rgbaToBgra(const QByteArray& rgba, int w, int h);

    VncConfig m_cfg;
    QQuickWindow* m_window = nullptr;
    QTcpServer* m_server = nullptr;
    QVector<VncServer*> m_servers;
    QMutex m_serversMutex;
    QElapsedTimer m_elapsed;
    qint64 m_lastCaptureMs = -1000;   // 上次读帧时刻（节流用；初值保证 ServerInit 首帧必触发）

    // N+23 修复（2026-08-30）：首帧全帧已下发标志——渲染线程只读此原子标志判定首帧。
    // （原 m_lastFrame QByteArray 缓存已删除：死代码 + 无边界钳制 memcpy 越界写风险，
    //   详见 4_bugs/rk3562/vnc-client-connect-crash.md N+23）
    QAtomicInteger<bool> m_firstFrameSent { false };

    // 生产端脏矩形报告（QML 层 markDirty 累积，渲染线程消费）
    QVector<QRect> m_dirtyRects;
    QMutex m_dirtyMutex;
};

} // namespace navihmi
