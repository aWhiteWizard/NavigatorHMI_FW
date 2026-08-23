/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\touchcalibrator.cpp
 * @Description: 触摸校准引擎实现（FW 内集成, VNC 全程不断）
 *               求解逻辑移植自 src/calib/touch-calibrate.cpp（leastSquares/computeAndWritePointercal）,
 *               去掉 event3 直读(EVIOCGRAB)与 Widgets UI——坐标改由 QML MouseArea 采集(Qt 层),
 *               本地触摸(eglfs evdevtouch)与 VNC 注入(QWindowSystemInterface)统一到达。
 */
#include "runtime/touchcalibrator.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>
#include <cmath>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <string.h>
#include <errno.h>
#endif

namespace navihmi {

namespace {

// ── 3x3 高斯消元（移植自 touch-calibrate.cpp）──
bool solve3(double A[3][3], double b[3], double x[3])
{
    double m[3][4];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) m[i][j] = A[i][j];
    for (int i = 0; i < 3; ++i) m[i][3] = b[i];
    for (int col = 0; col < 3; ++col) {
        int piv = col;
        for (int r = col + 1; r < 3; ++r)
            if (std::fabs(m[r][col]) > std::fabs(m[piv][col])) piv = r;
        if (std::fabs(m[piv][col]) < 1e-12) return false;
        if (piv != col)
            for (int j = 0; j < 4; ++j) std::swap(m[col][j], m[piv][j]);
        for (int r = 0; r < 3; ++r) {
            if (r == col) continue;
            double f = m[r][col] / m[col][col];
            for (int j = col; j < 4; ++j) m[r][j] -= f * m[col][j];
        }
    }
    for (int i = 0; i < 3; ++i) x[i] = m[i][3] / m[i][i];
    return true;
}

bool leastSquares(const QList<CalPair>& ps, double& a, double& b, double& c, bool forY)
{
    if (ps.size() < 3) return false;
    double A[3][3] = {}, Bx[3] = {};
    for (const auto& p : ps) {
        A[0][0] += p.rx * p.rx; A[0][1] += p.rx * p.ry; A[0][2] += p.rx;
        A[1][1] += p.ry * p.ry; A[1][2] += p.ry;
        A[2][2] += 1.0;
        const double t = forY ? p.sy : p.sx;
        Bx[0] += p.rx * t; Bx[1] += p.ry * t; Bx[2] += t;
    }
    A[1][0] = A[0][1]; A[2][0] = A[0][2]; A[2][1] = A[1][2];
    double x[3];
    if (!solve3(A, Bx, x)) return false;
    a = x[0]; b = x[1]; c = x[2];
    return true;
}

} // namespace

TouchCalibrator::TouchCalibrator(QObject* parent)
    : QObject(parent)
{
}

QString TouchCalibrator::statusText() const
{
    if (m_done) return QStringLiteral("校准完成");
    if (!m_active) return QString();
    return QStringLiteral("请触摸十字中心（%1/%2）").arg(m_index + 1).arg(m_points.size());
}

void TouchCalibrator::startCalibration(int devW, int devH)
{
    const int w = devW > 0 ? devW : 1024;
    const int h = devH > 0 ? devH : 600;
    m_points = {
        { 80, 80 }, { w - 80, 80 }, { w / 2, h / 2 }, { 80, h - 80 }, { w - 80, h - 80 }
    };
    m_pairs.clear();
    m_index = 0;
    m_done = false;
    m_restartNeeded = false;
    m_resultText.clear();
    m_active = true;
    // 重校准检测（审查 7e0143b8）: 已存在非空 pointercal 说明 tslib 已启用, Qt 层坐标是
    // 变换后坐标, 再校准会双重变换——提示用户先删旧校准再校准
    const QFileInfo oldCal(QStringLiteral("/etc/pointercal"));
    if (oldCal.exists() && oldCal.size() > 0) {
        qWarning().noquote() << "触摸校准: 检测到旧校准文件 /etc/pointercal(" << oldCal.size()
                             << "B)——tslib 已启用, 本次采集的是变换后坐标, 结果可能不准确; "
                                "建议先删除校准文件并重启后再校准";
    }
    qInfo().noquote() << "触摸校准: 进入校准模式, 屏幕" << w << "x" << h
                      << " 5点:" << QStringLiteral("(%1,%2) (%3,%4) (%5,%6) (%7,%8) (%9,%10)")
                             .arg(m_points[0].x).arg(m_points[0].y)
                             .arg(m_points[1].x).arg(m_points[1].y)
                             .arg(m_points[2].x).arg(m_points[2].y)
                             .arg(m_points[3].x).arg(m_points[3].y)
                             .arg(m_points[4].x).arg(m_points[4].y);
    emit stateChanged();
}

void TouchCalibrator::captureAt(int qx, int qy)
{
    if (!m_active || m_done) return;
    if (m_index >= m_points.size()) return;
    const auto& cp = m_points[m_index];
    // Qt 层坐标 = 设备原始坐标（无 pointercal 时 evdevtouch 直读 1:1）; 目标 = 十字屏幕坐标
    m_pairs.append({ (double)qx, (double)qy, (double)cp.x, (double)cp.y });
    qInfo().noquote() << QStringLiteral("校准点 %1/%2: 目标(%3,%4) 采集(Qt层 %5,%6)")
                             .arg(m_index + 1).arg(m_points.size())
                             .arg(cp.x).arg(cp.y).arg(qx).arg(qy);
    ++m_index;
    emit stateChanged();
    if (m_index >= m_points.size()) {
        computeAndWrite();
    }
}

void TouchCalibrator::cancelCalibration()
{
    if (!m_active) return;
    qInfo() << "触摸校准: 用户取消";
    m_active = false;
    m_done = false;
    m_points.clear();
    m_pairs.clear();
    m_index = 0;
    emit stateChanged();
}

void TouchCalibrator::computeAndWrite()
{
    m_done = true;
    m_restartNeeded = false;
    m_resultText = QStringLiteral("校准失败（点数不足）");
    const auto pairs = m_pairs;
    if (pairs.size() < 5) {
        emit stateChanged();
        return;
    }
    double a, b, c, d, e, f_;
    if (!leastSquares(pairs, a, b, c, false) || !leastSquares(pairs, d, e, f_, true)) {
        m_resultText = QStringLiteral("校准失败（仿射求解异常）");
        emit stateChanged();
        return;
    }
    double err = 0;
    for (const auto& pp : pairs) {
        const double px_ = a * pp.rx + b * pp.ry + c;
        const double py_ = d * pp.rx + e * pp.ry + f_;
        err += std::hypot(px_ - pp.sx, py_ - pp.sy);
    }
    const double avgErr = err / pairs.size();
    qInfo().noquote() << QStringLiteral("校准矩阵: a=%1 b=%2 c=%3 d=%4 e=%5 f=%6 平均误差=%7px")
                             .arg(a, 0, 'f', 6).arg(b, 0, 'f', 6).arg(c, 0, 'f', 6)
                             .arg(d, 0, 'f', 6).arg(e, 0, 'f', 6).arg(f_, 0, 'f', 6)
                             .arg(avgErr, 0, 'f', 2);
    // 质量校验: 平均误差 > 20px 视为采集异常, 拒绝写入（防异常矩阵进 tslib 触摸全灭）
    if (avgErr > 20.0) {
        m_resultText = QStringLiteral("校准质量差（平均误差 %1px），未写入——请重新校准")
                           .arg(avgErr, 0, 'f', 1);
        qWarning().noquote() << m_resultText;
        emit stateChanged();
        return;
    }
    // 矩阵≈单位（直读已准, 如当前 ft5x06 1:1 屏）——不写 pointercal, 保持 evdevtouch 直读。
    // 复审 cd38dd9a: c/f 阈值收紧到 20px（与质量门 20px 一致）——均匀平移偏移 δ(20≤|δ|<40px)
    // 时最小二乘解 M=-δ 残差≈0 能过 20px 质量门, 若 identity 门 c/f 阈值 40px 会误判"已准确"
    // 不写校准, 触摸保持偏移 20-39px 却被告知准确。收紧后 |δ|<20 判"已准确"(与质量门容差一致),
    // |δ|≥20 写 M=-δ 纠正; 当前 1:1 面板 c,f≈0 不受影响。
    // 命中时若存在旧校准文件则删除之(回退直读)——否则旧矩阵继续生效,"已准确"是假话
    if (std::fabs(a - 1.0) < 0.15 && std::fabs(e - 1.0) < 0.15
        && std::fabs(b) < 0.15 && std::fabs(d) < 0.15
        && std::fabs(c) < 20 && std::fabs(f_) < 20) {
        if (QFile::exists(QStringLiteral("/etc/pointercal"))) {
            if (QFile::remove(QStringLiteral("/etc/pointercal")))
                qInfo() << "触摸校准: 已删除旧校准文件, 回退 evdevtouch 直读";
            else
                qWarning() << "触摸校准: 删除旧校准文件失败(需 root)";
        }
        m_resultText = QStringLiteral("触摸已准确（矩阵≈单位），无需校准文件——保持直读");
        qInfo().noquote() << "触摸校准: " << m_resultText;
        emit stateChanged();
        return;
    }
    // 原子写入（审查 7e0143b8）: 先写临时文件 + fsync + rename——直接 Truncate 写 /etc/pointercal
    // 在断电/崩溃时留下残缺文件(size>0 触发 tslib, linear 解析失败 → 触摸全灭, 本轮历史反复踩)
    const QString tmpPath = QStringLiteral("/etc/pointercal.tmp");
    QFile out(tmpPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_resultText = QStringLiteral("无法写入校准文件（需要 root 权限）");
        qWarning().noquote() << "触摸校准: " << m_resultText;
        emit stateChanged();
        return;
    }
    // tslib linear 用 sscanf("%d×7") 整数解析——写定点整数 s=65536
    QTextStream ts(&out);
    const qint64 S = 65536;
    ts << qint64(qRound64(a * S)) << ' ' << qint64(qRound64(b * S)) << ' '
       << qint64(qRound64(c * S)) << ' ' << qint64(qRound64(d * S)) << ' '
       << qint64(qRound64(e * S)) << ' ' << qint64(qRound64(f_ * S)) << ' ' << S << '\n';
    out.flush();
#ifdef Q_OS_UNIX
    if (::fsync(out.handle()) != 0)
        qWarning() << "触摸校准: fsync 失败" << strerror(errno);
#endif
    out.close();
    if (!QFile::rename(tmpPath, QStringLiteral("/etc/pointercal"))) {
        QFile::remove(tmpPath);
        m_resultText = QStringLiteral("写入校准文件失败（rename 失败）");
        qWarning().noquote() << "触摸校准: " << m_resultText;
        emit stateChanged();
        return;
    }
    m_restartNeeded = true;
    m_resultText = QStringLiteral("校准完成：已写入 /etc/pointercal（误差 %1px），重启 FW 生效")
                       .arg(avgErr, 0, 'f', 1);
    qInfo().noquote() << "触摸校准: " << m_resultText;
    emit stateChanged();
}

void TouchCalibrator::restartFw()
{
    // 重入防护（审查 7e0143b8）: 退出窗口内重复点击会调度多个子进程互相抢 DRM
    if (m_restarting) return;
    m_restarting = true;
    qInfo().noquote() << "触摸校准: 用户确认重启 FW 生效";
    // eglfs 双进程不能共享 DRM——不能 startDetached 立即启动(新进程会与当前进程抢 DRM 启动失败)。
    // 用延迟重启脚本: 本进程退出释放 DRM 后(sleep 2)再由脚本拉起新 FW。
    // 子进程继承本进程环境(QT_QPA_PLATFORM/插件路径/QML 路径/NAVIHMI_VNC 均在环境里),
    // 重启后 main.cpp 读到非空 /etc/pointercal → 自动启用 tslib 应用校准
    QString cmd = QStringLiteral("sleep 2; /usr/bin/navigatorhmi-fw");
    if (!m_projectPath.isEmpty())
        cmd += QStringLiteral(" --project '") + m_projectPath + QLatin1Char('\'');
    const bool ok = QProcess::startDetached(QStringLiteral("/bin/sh"),
                                            { QStringLiteral("-c"), cmd });
    if (ok) {
        qInfo().noquote() << "触摸校准: 延迟重启 FW(2s 后) 已调度";
        // 500ms 后让出 eglfs DRM（脚本 sleep 2 在其后拉起新进程, 无 DRM 竞争）
        QTimer::singleShot(500, qApp, &QCoreApplication::quit);
    } else {
        qWarning().noquote() << "触摸校准: 重启脚本启动失败——已写入校准文件, 重启设备后生效";
        m_restarting = false;
        // 保持结果页（不退出校准模式）, 提示用户手动重启设备
        m_resultText = QStringLiteral("校准已写入，但自动重启失败——请重启设备生效");
        emit stateChanged();
    }
}

} // namespace navihmi
