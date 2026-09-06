/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\touchcalibrator.h
 * @Description: 触摸校准引擎（E 循环, 集成进 FW——校准 UI 渲染在 FW 主窗口内, VNC 全程不断）
 *               方案变更（2026-08-22 用户拍板）: 原"独立程序 touch-calibrate"作废——
 *               独立程序与 FW 双 eglfs 进程不能共享 DRM, 校准期间 FW 退出 → VNC 断连。
 *               改为 FW 内模式: 校准 overlay 显示在 FW 的 QQuickWindow 里 → VncMirror
 *               frameSwapped 抓帧天然覆盖校准画面 → VNC 全程可见不断连;
 *               坐标采集走 Qt 层(QML MouseArea 点击坐标) → 本地触摸与 VNC 注入的
 *               QWindowSystemInterface 鼠标事件统一到达 → 远程(经 VNC)也能操作校准。
 *               5 点(四角+中心) → 最小二乘仿射 → 质量校验(残差<=20px) → 写 /etc/pointercal
 *               （矩阵≈单位不写, 保持 evdevtouch 直读; 写入后需重启 FW 让 tslib 生效）。
 */
#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace navihmi {

// 校准配对: 原始(Qt层)坐标 → 目标屏幕坐标（leastSquares 自由函数共用, 故置于类外）
struct CalPair { double rx, ry, sx, sy; };

class TouchCalibrator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)          // 校准模式进行中（overlay 可见性）
    Q_PROPERTY(int pointIndex READ pointIndex NOTIFY stateChanged)   // 当前点 0..4
    Q_PROPERTY(int pointCount READ pointCount NOTIFY stateChanged)   // 总点数（未激活 0 / 激活 5）
    Q_PROPERTY(int pointX READ pointX NOTIFY stateChanged)           // 当前十字 X（overlay 坐标空间）
    Q_PROPERTY(int pointY READ pointY NOTIFY stateChanged)           // 当前十字 Y
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged) // 采集提示
    Q_PROPERTY(bool done READ done NOTIFY stateChanged)              // 5 点采集完成（显示结果页）
    Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged) // 结果文案（质量/写入/直读）
    Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY stateChanged) // 已写 pointercal, 需重启生效
public:
    explicit TouchCalibrator(QObject* parent = nullptr);

    bool active() const { return m_active; }
    int pointIndex() const { return m_index; }
    int pointCount() const { return m_points.size(); }
    int pointX() const { return m_points.value(m_index).x; }
    int pointY() const { return m_points.value(m_index).y; }
    QString statusText() const;
    bool done() const { return m_done; }
    QString resultText() const { return m_resultText; }
    bool restartNeeded() const { return m_restartNeeded; }

    /// 进入校准模式（QML 调用, 传入物理屏分辨率决定 5 点位置——F-2：校准是设备级功能, 与工程 deviceWidth 解耦）
    Q_INVOKABLE void startCalibration(int devW, int devH);
    /// QML MouseArea 点击采集（Qt 层坐标; 本地触摸 / VNC 注入统一到达）
    Q_INVOKABLE void captureAt(int qx, int qy);
    /// 取消校准（放弃本次采集, 回 HMI）
    Q_INVOKABLE void cancelCalibration();
    /// 校准完成(已写 pointercal)后重启 FW 使其生效（QML 结果页"重启"按钮）
    Q_INVOKABLE void restartFw();

    /// 工程路径（重启 FW 时 --project 参数; 由 main.cpp 注入原始路径）
    void setProjectPath(const QString& p) { m_projectPath = p; }

signals:
    void stateChanged();

private:
    void computeAndWrite();   // 最小二乘 + 质量门 + 写 pointercal（移植自 touch-calibrate）

    struct Point { int x, y; };

    QList<Point> m_points;
    QList<CalPair> m_pairs;
    int m_index = 0;
    bool m_active = false;
    bool m_done = false;
    bool m_restartNeeded = false;
    bool m_restarting = false;   // restartFw 重入防护
    QString m_resultText;
    QString m_projectPath;
};

} // namespace navihmi
