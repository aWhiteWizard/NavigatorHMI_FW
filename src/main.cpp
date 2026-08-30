/*
 * @FilePath: \NavigatorHMI_FW\src\main.cpp
 * @Description: NavigatorHMI FW 应用入口（跨平台）
 *               同一份代码：RK3562（Linux ARM, Qt 6.4.3）+ Windows 桌面（仿真器）
 *               输入：组态软件编译的 .navihmi（proto/navihmi.proto 契约）
 *               用法：navihmi-fw --project xxx.navihmi        （正常启动）
 *                     navihmi-fw --convert xxx.navihmi        （转换器测试：解析并打印模型摘要）
 */
#include <QGuiApplication>
#if defined(HAVE_QT_QML)
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#endif
#include <QCommandLineParser>
#include <QDebug>
#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMetaObject>
#include <cstdio>
#if defined(HAVE_QT_QML)
#include <QtGui/private/qzipreader_p.h>   // R3: 工程 ZIP 包解压（Qt private API, sysroot 已含）
#endif

#include "converter/projectparser.h"
#include "converter/qmlgenerator.h"
#include "runtime/projectmodel.h"
#if defined(HAVE_QT_QML)
#include "runtime/runtimebus.h"
#include "runtime/datamanager.h"
#include "runtime/objectmanager.h"
#include "runtime/usersystem.h"
#include "runtime/alarmengine.h"
#include "runtime/datalogger.h"
#include "runtime/acquisition.h"
#include "runtime/deviceinfo.h"
#include "runtime/storageinfo.h"
#include "runtime/vncmirror.h"
#include "runtime/fwconfig.h"   // K-9 评论3：VNC 端口配置单点
#include "runtime/touchcalibrator.h"
#include "runtime/devicemeta.h"   // kDefaultDeviceWidth/Height 单点（K-9 评论1：默认分辨率收敛，替代本地 kDefaultDevW/H）
#include "cli/commands.h"
#include "cli/cliserver.h"
#if defined(HAVE_QT_HTTPSERVER)
#include "httpreceiver/httpreceiver.h"   // K-8b: HTTP 接收端（工程部署容器；QtHttpServer 缺失时降级排除）
#endif
#endif

namespace {
// 默认设备尺寸（7 寸 1024×600；与 touchcalibrator/vncmirror 兜底一致，2026-08-26 魔法数字整改命名）
// K-9 评论1：值已收敛到 devicemeta.h kDefaultDeviceWidth/Height 单点（此处保留别名引用，防匿名命名空间内原调用点改动面扩大）
constexpr int kDefaultDevW = navihmi::kDefaultDeviceWidth;
constexpr int kDefaultDevH = navihmi::kDefaultDeviceHeight;
} // anonymous namespace

// ═══════ 工程包解析（R3: 工程=单个 ZIP, 内含工程信息 + 瓦片地图）═══════
// 工程文件 = 一个 ZIP 压缩包（2026-08-21 用户定）：
//   <工程>.navihmi (ZIP)
//     ├── app.navihmi   ← 工程二进制（实际加载）
//     └── tiles/        ← 地图瓦片目录 z/x/y.png（解压后供 HmiWorldMap 加载）
// 下载到 HMI 就是这个 ZIP；设备端收到后自行解压到对应目录。
// 兼容：纯二进制 .navihmi（非 ZIP）原样返回, 无瓦片。
#if defined(HAVE_QT_QML)
// 整包解压：把 ZIP 全部内容解压到 outDir（返回解压文件数；失败 -1）
static int extractZipAll(const QString& zipPath, const QString& outDir)
{
    QDir dir(outDir);
    dir.mkpath(".");
    QZipReader reader(zipPath);
    if (!reader.exists()) {
        qWarning().noquote() << "工程 ZIP 打不开:" << zipPath;
        return -1;
    }
    const auto entries = reader.fileInfoList();
    int extracted = 0;
    for (const auto& entry : entries) {
        if (entry.isDir) continue;
        const QString rel = entry.filePath;
        if (rel.contains("..")) continue;   // 防路径穿越
        const QString target = outDir + "/" + rel;
        QFileInfo fi(target);
        QDir().mkpath(fi.absolutePath());
        QFile f(target);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(reader.fileData(rel));
            f.close();
            ++extracted;
        }
    }
    reader.close();
    qInfo().noquote() << "工程 ZIP 解压完成:" << extracted << "files ->" << outDir;
    return extracted;
}

// 解析 --project 参数：ZIP 工程包（PK 魔数）→ 解压到 /tmp/navihmi_pkg/ → 返回内部 app.navihmi
// tileBasePath 出参为瓦片根目录（无则空）；普通文件 → 原样返回（向后兼容纯二进制）
static QString resolveProjectPackage(const QString& projectPath, QString& tileBasePath)
{
    QFileInfo info(projectPath);
    if (!info.exists() || !info.isFile()) {
        tileBasePath = QString();
        return projectPath;
    }
    // 读魔数判断 ZIP
    QFile f(projectPath);
    if (!f.open(QIODevice::ReadOnly)) {
        tileBasePath = QString();
        return projectPath;
    }
    QByteArray magic = f.read(4);
    f.close();
    if (magic.size() < 4 || magic[0] != 'P' || magic[1] != 'K') {
        // M-3 ①: 单文件工程（HTTP 下载链路落盘 app.navihmi + 同目录 tiles/ 瓦片——
        // httreceiver 按 manifest target 落盘，瓦片保留根级 tiles/ 前缀）→ 探测同目录 tiles/（与 ZIP 直启同约定）
        const QString appDir = QFileInfo(projectPath).absolutePath();
        const QString tilesDir = appDir + QStringLiteral("/tiles");
        int tilePngCount = 0;
        if (QFileInfo::exists(tilesDir)) {
            QDirIterator it(tilesDir, QStringList() << "*.png", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) { it.next(); ++tilePngCount; }
        }
        tileBasePath = (tilePngCount > 0) ? tilesDir : QString();
        if (tileBasePath.isEmpty())
            qWarning().noquote() << "单文件工程缺瓦片数据（同目录 tiles/ 目录缺失或 0 张 PNG）:"
                                 << QFileInfo(projectPath).fileName();
        return projectPath;          // 普通单文件工程（向后兼容纯二进制）
    }
    // ZIP 工程包：整包解压到固定临时目录
    const QString pkgDir = QDir::tempPath() + "/navihmi_pkg";
    QDir old(pkgDir);
    if (old.exists()) {
        // J-2 审查修复: 清空旧包用递归删除（QFile::remove 对目录无效——原实现 tiles/ 子树残留,
        // reload「有瓦片→无瓦片」时残留瓦片导致 J-2 校验假阴性 + 显示旧工程瓦片）
        old.removeRecursively();
    }
    if (extractZipAll(projectPath, pkgDir) < 0) {
        tileBasePath = QString();
        return projectPath;
    }
    // 内部工程二进制
    const QString inner = pkgDir + "/app.navihmi";
    // 瓦片目录（存在且含 PNG 才注入）
    const QString tilesDir = pkgDir + "/tiles";
    int tilePngCount = 0;
    if (QFileInfo::exists(tilesDir)) {
        QDirIterator it(tilesDir, QStringList() << "*.png", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); ++tilePngCount; }
    }
    tileBasePath = (tilePngCount > 0) ? tilesDir : QString();
    // J-2: ZIP 工程包缺瓦片（目录缺失或 0 PNG）→ 明确警告（R3 工程包应含 tiles/）
    if (tileBasePath.isEmpty())
        qWarning().noquote() << "工程 ZIP 缺瓦片数据（tiles/ 目录缺失或 0 张 PNG）:"
                             << QFileInfo(projectPath).fileName();
    qInfo().noquote() << "工程包解析: inner=" << inner
                      << " tiles=" << (tileBasePath.isEmpty() ? "(无)" : QStringLiteral("%1 (%2 PNG)").arg(tileBasePath).arg(tilePngCount));
    return inner;
}
#endif

// ═══════ 转换器测试模式：解析 .navihmi → 打印模型摘要 ═══════
static int runConvert(const QString& path)
{
    navihmi::Project proj;
    if (!navihmi::ProjectParser::parseFile(path, proj)) {
        qCritical().noquote() << "解析失败:" << path;
        return 1;
    }

    QTextStream out(stdout);
    out << "=== .navihmi 解析成功 ===" << "\n";
    out << "工程: " << proj.name << "  v" << proj.version
        << "  format=" << proj.formatVersion << "\n";
    out << "设备: " << proj.deviceWidth << "x" << proj.deviceHeight << "\n";
    out << "画面数: " << proj.screens.size() << "\n";

    int totalWidgets = 0, totalEvents = 0, totalActions = 0;
    for (const auto& sc : proj.screens) {
        QString typeName = sc.type == navihmi::ScreenType::Template ? "全局"
                         : sc.type == navihmi::ScreenType::WorldMap ? "世界地图"
                         : "自定义";
        out << "  [" << typeName << "] " << sc.name
            << " (" << sc.width << "x" << sc.height << ")"
            << " 控件" << sc.widgets.size() << "\n";
        for (const auto& w : sc.widgets) {
            totalWidgets++;
            out << "    " << w.objectName << " type=" << int(w.type)
                << " x=" << w.x << " y=" << w.y
                << " w=" << w.width << " h=" << w.height;
            if (!w.boundTag.isEmpty()) out << " tag=" << w.boundTag;
            if (!w.events.isEmpty()) {
                out << " events=" << w.events.size();
                for (const auto& ev : w.events) {
                    totalEvents++;
                    totalActions += ev.actions.size();
                    out << " [" << int(ev.type) << ":";
                    for (const auto& ac : ev.actions)
                        out << int(ac.type) << ",";
                    out << "]";
                }
            }
            out << "\n";
        }
    }

    out << "变量: " << proj.tags.size() << " 报警: " << proj.alarms.size()
        << " 设备: " << proj.devices.size() << " 列表: " << proj.lists.size() << "\n";
    for (const auto& t : proj.tags)
        out << "  tag " << t.name << " type=" << int(t.dataType)
            << " base=" << t.baseValue << "\n";

    out << "世界地图: 作业点" << proj.worldMap.workPoints.size()
        << " 范围点" << proj.worldMap.workRangePoints.size()
        << " 事件" << proj.worldMap.events.size()
        << " 范围[" << proj.worldMap.latMin << "," << proj.worldMap.latMax
        << "]x[" << proj.worldMap.lngMin << "," << proj.worldMap.lngMax
        << "] overlay=" << proj.worldMap.showGlobalOverlay << "\n";
    for (const auto& wp : proj.worldMap.workPoints)
        out << "  workpoint " << wp.name << " (" << wp.fixedPoint.longitude
            << "," << wp.fixedPoint.latitude << ") tag=" << wp.boundTag << "\n";

    out << "用户: " << proj.users.size() << " 组: " << proj.groups.size() << "\n";
    out << "=== 统计: 控件" << totalWidgets << " 事件" << totalEvents
        << " 动作" << totalActions << " ===" << "\n";
    return 0;
}

// ═══════ QML 生成测试模式：解析 .navihmi → 生成 QML 文件到目录 ═══════
static int runGenQml(const QString& path, const QString& outDir)
{
    navihmi::Project proj;
    if (!navihmi::ProjectParser::parseFile(path, proj)) {
        qCritical().noquote() << "解析失败:" << path;
        return 1;
    }
    QDir dir(outDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qCritical().noquote() << "创建目录失败:" << outDir;
        return 1;
    }
    const auto files = navihmi::QmlGenerator::generateAll(proj);
    QTextStream out(stdout);
    out << "=== QML 生成 ===" << "\n";
    for (const auto& f : files) {
        const QString fpath = outDir + "/" + f.first;
        QFile file(fpath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qCritical().noquote() << "写入失败:" << fpath;
            return 1;
        }
        file.write(f.second.toUtf8());
        file.close();
        out << "  " << f.first << " (" << f.second.size() << " bytes)" << "\n";
    }
    out << "=== 共 " << files.size() << " 个 QML 文件 ===" << "\n";
    return 0;
}

// ═══════ QML 工程加载 + 注入（B6-8: 抽函数——启动与"存储替换默认工程后 reload"共用）═══════
#if defined(HAVE_QT_QML)
static bool loadAndInject(QObject* rootObj,
                          navihmi::RuntimeBus& runtimeBus, navihmi::DataManager& dataManager,
                          const QString& projectPath,
                          navihmi::VncMirror* vncMirror = nullptr,
                          const QString& tileBasePath = QString(),
                          navihmi::UserSystem* userSystem = nullptr,
                          navihmi::AlarmEngine* alarmEngine = nullptr,
                          navihmi::DataLogger* dataLogger = nullptr,
                          navihmi::Acquisition* acquisition = nullptr)
{
    navihmi::Project proj;
    if (!projectPath.isEmpty() && !navihmi::ProjectParser::parseFile(projectPath, proj)) {
        qCritical().noquote() << "工程加载失败:" << projectPath;
        return false;
    }
    runtimeBus.setProject(proj);      // 内部重置画面匹配状态（⑪候选A）
    dataManager.setProject(proj);
    // G-1a: 用户系统注入工程用户/组/安全配置（含初始管理员兜底）
    if (userSystem)
        userSystem->setProject(proj);
    // G-1b: 报警引擎注入报警规则（变量阈值驱动）
    if (alarmEngine)
        alarmEngine->setProject(proj);
    // G-2: 数据记录建表 + 定时采样（工程变量）
    if (dataLogger)
        dataLogger->setProject(proj);
    // H-8: 数据采集注入 modbus 源（Tag.source = modbus://slave/reg）
    if (acquisition)
        acquisition->setProject(proj);

    // 生成画面 QML 到临时目录（每画面 + overlay + 主壳）
    QDir genDir(QDir::tempPath() + "/navihmi_gen");
    genDir.mkpath(".");
    // 审查 M3(2026-08-23 G-0): overlay 文件名带递增序号——工程重载时同路径 source 相等
    // Loader 不重载, 旧 Template 控件残留注册; 序号保证每次路径不同强制重载
    // 审查 N-7(复审): 双文件轮换(a/b)封顶 2 文件, 防进程内多次替换累积
    static int s_overlaySeq = 0;
    const QString overlayName = QStringLiteral("overlay_%1.qml")
        .arg((++s_overlaySeq % 2) ? QStringLiteral("a") : QStringLiteral("b"));
    // 2026-08-30 用户 Check 修复：VNC 按工程 enable_vnc 启停仅首次加载执行（此后重载保持运行时状态）
    static bool s_initialVncApplied = false;
    QStringList screenFiles;
    QStringList screenNames;
    int genIdx = 0;
    QString startScreen = proj.startScreen;
    if (startScreen.isEmpty()) {
        // 默认世界地图（设计文档⑧: start_screen 确认后进入, 默认世界地图）
        for (const auto& sc : proj.screens)
            if (sc.type == navihmi::ScreenType::WorldMap) { startScreen = sc.name; break; }
        if (startScreen.isEmpty()) {
            for (const auto& sc : proj.screens)
                if (sc.type == navihmi::ScreenType::Custom) { startScreen = sc.name; break; }
        }
    }
    for (const auto& sc : proj.screens) {
        QString fname;
        QString content;
        if (sc.type == navihmi::ScreenType::WorldMap) {
            fname = QStringLiteral("screen_%1.qml").arg(genIdx);
            // J-2: 工程级瓦片校验——WorldMap 画面无瓦片 → 明确警告（任何来源：ZIP 缺 tiles/ 或普通文件工程）
            if (tileBasePath.isEmpty())
                qWarning().noquote() << "世界地图画面无瓦片数据，使用模拟底图:"
                                     << sc.name;
            content = navihmi::QmlGenerator::generateWorldMap(proj, tileBasePath);   // R3: 工程自带瓦片
        } else if (sc.type == navihmi::ScreenType::Template) {
            fname = overlayName;
            content = navihmi::QmlGenerator::generateOverlay(proj);
        } else {
            fname = QStringLiteral("screen_%1.qml").arg(genIdx);
            content = navihmi::QmlGenerator::generateScreen(proj, sc);
        }
        QFile f(genDir.filePath(fname));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(content.toUtf8());
            f.close();
        }
        if (sc.type != navihmi::ScreenType::Template) {
            screenFiles.append(genDir.filePath(fname));
            screenNames.append(sc.name);
        }
        ++genIdx;
    }

    // 注入画面清单（对象数组 [{name, file}]，QML switchToName 用 .name/.file）
    QVariantList filesList;
    for (int i = 0; i < screenFiles.size(); ++i) {
        QVariantMap m;
        m["name"] = screenNames[i];
        m["file"] = screenFiles[i];
        filesList.append(m);
    }
    rootObj->setProperty("screenFiles", filesList);
    rootObj->setProperty("startScreen", startScreen);
    rootObj->setProperty("hasProject", !proj.screens.isEmpty());
    // 设备尺寸（主壳自适应：7 寸 1024×600 / 4 寸 720×720 等比缩放）
    int devW = proj.deviceWidth > 0 ? proj.deviceWidth : kDefaultDevW;
    int devH = proj.deviceHeight > 0 ? proj.deviceHeight : kDefaultDevH;
    rootObj->setProperty("deviceWidth", devW);
    rootObj->setProperty("deviceHeight", devH);

    // VNC 镜像：按工程 enable_vnc 启停（eglfs 物理屏照常；NAVIHMI_VNC=0 强制关兜底 / =2 强制开调试）
    // 2026-08-30 用户 Check 修复：只在**首次加载**按工程 enable_vnc 决策——
    // 之后下载新工程/SD/USB 替换重载时保持当前 VNC 运行时状态（K-9 语义：enable_vnc 是启动默认值，
    // 运行时指令 vnc on/off 覆盖；否则下载 enable_vnc=false 的工程会把用户手动开着的 VNC 停掉）
    // 审查 🟡 边缘：冷启动无工程（空导航）时首载在空工程上决策（enableVnc=false → stop），
    // 之后下载 enable_vnc=true 工程重载不自动启动——需手动 `vnc on`（与 K-9"启动默认值"语义一致，接受）
    // 审查 🟡：setDeviceSize **不门控**——每次 loadAndInject 同步设备尺寸（VNC 读帧区域/握手帧尺寸用
    // m_devW/H，重载不同尺寸工程后必须更新，否则 glReadPixels 区域/宣告尺寸陈旧）；仅启停决策走 s_initialVncApplied
    if (vncMirror)
        vncMirror->setDeviceSize(devW, devH);
    if (vncMirror && !s_initialVncApplied) {
        s_initialVncApplied = true;
        int force = 1;
        bool okForce = false;
        int fv = qEnvironmentVariableIntValue("NAVIHMI_VNC", &okForce);
        if (okForce) force = fv;
        bool want = (force == 2) || (proj.enableVnc && force != 0);
        if (want) {
            // K-9 评论3：端口收敛到 fwconfig（/etc/navigatorhmi/fw-config.json + NAVIHMI_VNC_PORT 覆盖），不再写死 5900
            vncMirror->start(quint16(navihmi::vncPort()));
        } else {
            vncMirror->stop();
        }
    }

    // overlay 生成（Stop Runtime 按钮所在）——无 Template 画面时写空 overlay（Truncate 覆盖,
    // 防 reload 后旧工程 overlay 残留导致旧全局控件事件误触发）
    bool hasTemplate = false;
    for (const auto& sc : proj.screens)
        if (sc.type == navihmi::ScreenType::Template) { hasTemplate = true; break; }
    QFile overlayFile(genDir.filePath(overlayName));
    if (!hasTemplate) {
        overlayFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        overlayFile.write(QStringLiteral("import QtQuick 2.15\nItem { width: %1; height: %2 }\n")
                              .arg(kDefaultDevW).arg(kDefaultDevH).toUtf8());
        overlayFile.close();
    }
    rootObj->setProperty("overlayFile", genDir.filePath(overlayName));
    return true;
}
#endif

// ═══════ 启动诊断（B6-8: 无工程模式进程退出排查——信号/崩溃打印）═══════
#include <csignal>
#include <cstdlib>
namespace {
void onSignal(int sig)
{
    std::fprintf(stderr, "!!! navigatorhmi-fw 收到信号 %d\n", sig);
    std::fflush(stderr);
    std::_Exit(128 + sig);
}
} // namespace

int main(int argc, char *argv[])
{
    // ── 转换器模式（纯命令行，无需 GUI/Qt 平台插件）──
    // 在 QGuiApplication 之前处理 --convert/--genqml：避免无 QPA 插件环境下启动失败
    {
        bool convertMode = false;
        QString convertPath;
        bool genQmlMode = false;
        QString genQmlPath, genQmlDir;
        for (int i = 1; i < argc; ++i) {
            if (qstrcmp(argv[i], "--convert") == 0) {
                convertMode = true;
                if (i + 1 < argc) convertPath = QString::fromLocal8Bit(argv[i + 1]);
            } else if (qstrcmp(argv[i], "--genqml") == 0) {
                genQmlMode = true;
                if (i + 1 < argc) genQmlPath = QString::fromLocal8Bit(argv[i + 1]);
                if (i + 2 < argc) genQmlDir = QString::fromLocal8Bit(argv[i + 2]);
            }
        }
        if (convertMode) {
            if (convertPath.isEmpty()) {
                qCritical() << "用法: NavigatorHMI_FW --convert <xxx.navihmi>";
                return 1;
            }
            return runConvert(convertPath);
        }
        if (genQmlMode) {
            if (genQmlPath.isEmpty() || genQmlDir.isEmpty()) {
                qCritical() << "用法: NavigatorHMI_FW --genqml <xxx.navihmi> <outdir>";
                return 1;
            }
            return runGenQml(genQmlPath, genQmlDir);
        }
    }

    // R4: 启用 Qt VirtualKeyboard 屏上键盘输入法（数字→数字键盘 / 文字→全键盘含中英拼音）
    // 必须在 QGuiApplication 创建前设置 QT_IM_MODULE——输入法插件在 app 初始化时加载，晚了不生效（D+ 修复）
    // platforminputcontexts/libqtvirtualkeyboardplugin.so 由 fs-overlay 部署
#if !defined(Q_OS_WIN)
    qputenv("QT_IM_MODULE", QByteArrayLiteral("qtvirtualkeyboard"));
    // E 循环（2026-08-22）: 触摸校准生效链——eglfs 用 tslib 输入插件(ts_read 自动应用
    // /etc/pointercal)；必须在 app 构造前设置（qeglfsintegration 启动时选插件）。
    // 复审 42933353: 仅当 pointercal 已存在才启用 tslib——无 pointercal 时 tslib linear
    // 模块初始化失败会导致触摸全灭(无法长按进校准的死锁), 回退 evdevtouch 保持触摸可用
    // 2026-08-22 修复: 空 pointercal(0 字节)会误触 tslib 分支导致触摸全灭——须存在且非空
    const bool havePointercal = QFileInfo(QStringLiteral("/etc/pointercal")).size() > 0;
    if (havePointercal) {
        qputenv("QT_QPA_EGLFS_TSLIB", QByteArrayLiteral("1"));
        // 2026-08-22: tslib 明确指向触摸设备(防 ts_setup 自动探测失败导致触摸全灭)——
        // 探测 sysfs input name 含 touch/ft5x06, 兜底 event3
        QString tsDev = QStringLiteral("/dev/input/event3");
        QDir inputDir(QStringLiteral("/sys/class/input"));
        const auto events = inputDir.entryList(QStringList() << QStringLiteral("event*"), QDir::Dirs);
        for (const auto& ev : events) {
            QFile nameFile(inputDir.filePath(ev + QStringLiteral("/device/name")));
            if (nameFile.open(QIODevice::ReadOnly)) {
                const QString name = QString::fromUtf8(nameFile.readAll()).trimmed().toLower();
                if (name.contains(QStringLiteral("touch")) || name.contains(QStringLiteral("ft5x06"))) {
                    tsDev = QStringLiteral("/dev/input/") + ev;
                    break;
                }
            }
        }
        qputenv("TSLIB_TSDEVICE", tsDev.toUtf8());
        qInfo().noquote() << "触摸校准: tslib 启用, 设备=" << tsDev;
    } else {
        // 2026-08-22 修复: 覆盖外部环境残留的 QT_QPA_EGLFS_TSLIB=1(S99 曾 export)——
        // 无 pointercal 时必须回退 evdevtouch, 否则 eglfs 用 tslib(无配置)触摸全灭
        qputenv("QT_QPA_EGLFS_TSLIB", QByteArrayLiteral("0"));
    }
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NavigatorHMI_FW"));
    app.setApplicationVersion(QStringLiteral("1.1.0"));

    // 启动诊断（B6-8）: 信号处理器——退出原因定位
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGABRT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGHUP, onSignal);
    std::signal(SIGINT, onSignal);

    // 平台后端：Linux 嵌入式按 FW_PLATFORM_BACKEND 设 QPA（linuxfb/eglfs，CMake -D 配置）
#if !defined(Q_OS_WIN)
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral(FW_PLATFORM_BACKEND));
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NavigatorHMI FW 应用（RK3562 / Windows 仿真器）"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringLiteral("project"), QStringLiteral(".navihmi 工程文件路径"), QStringLiteral("path")));
    parser.process(app);

    const QString projectPath = parser.value(QStringLiteral("project"));

    // E 循环: 工程路径进进程环境——校准完成写 pointercal 后 FW 自重启(restartFw)时子进程继承,
    // 重启后 main 读到非空 /etc/pointercal → 自动启用 tslib 应用校准
    qputenv("NAVIHMI_PROJECT", projectPath.toUtf8());

    // ═══════ QML 引擎 ═══════
#if defined(HAVE_QT_QML)
    qInfo().noquote() << "navigatorhmi-fw: 启动 projectPath=" << projectPath;   // 诊断(B6-8)
    // 审查 M1(2026-08-23 G-0): 全部 setContextProperty 服务对象必须在 engine 之前构造——
    // engine 析构时销毁 QML 对象, 生成控件的 onDestruction 处理器（unregisterObject/emitEvent）
    // 会打到服务对象; 栈上对象逆序销毁 → engine 最后构造最先销毁, 服务对象存活到 engine 之后
    // 运行时事件总线（QML 只发事件，C++ 执行动作；工程注入见 loadAndInject）
    navihmi::RuntimeBus runtimeBus;

    // 数据管理器（TagStore 雏形：变量实时值中心, QML 组件绑定显示）
    navihmi::DataManager dataManager;

    // G-0: 对象管理器（架构三件套之一）——全局对象注册表 + 跨画面寻址
    // 系统对象注册：dataManager/runtimeBus（三件套数据访问统一入口, 西门子 Proxy 模式）
    navihmi::ObjectManager objectManager;

    // G-1a: 用户系统（登录/注销/权限/管理；三件套 UserView 数据源）
    navihmi::UserSystem userSystem;

    // G-1b: 报警引擎（模拟报警源——变量阈值驱动；三件套 AlarmView 数据源）
    navihmi::AlarmEngine alarmEngine;
    alarmEngine.setDataManager(&dataManager);

    // G-2: 数据记录（SQLite tag_history + alarm_history）
    navihmi::DataLogger dataLogger;
    dataLogger.setDataManager(&dataManager);

    // H-8: 数据采集引擎（Modbus TCP 轮询读 + 写通道; 无 modbus 变量时零开销）
    navihmi::Acquisition acquisition;
    acquisition.setDataManager(&dataManager);

    // 设备信息（B6-6: IP/MAC/版本/内核/运行时间真实读取, 导航页显示）
    navihmi::DeviceInfo deviceInfo;

    // 存储信息（B6-7: SD/USB 真实检测 + 工程扫描/替换）
    navihmi::StorageInfo storageInfo;

    // I-1: SSH CLI 命令服务（本地 socket + navihmi-cli 工具；命令与触屏共享服务实例）
    navihmi::CommandService commandService;
    navihmi::CliServer cliServer(&commandService);

#if defined(HAVE_QT_HTTPSERVER)
    // K-8b: HTTP 接收端（工程部署容器传输——qthttpserver；单客户端串行）
    navihmi::HttpReceiver httpReceiver(&runtimeBus, &deviceInfo);
#endif

    // 触摸校准引擎（E 循环集成进 FW: 校准 overlay 在 FW 主窗口内渲染 → VNC 全程不断;
    // 坐标走 Qt 层(QML MouseArea)采集 → VNC 注入鼠标事件也能操作校准）
    // 注: 必须在 engine.load 之前注入——main.qml 顶层绑定引用 touchCalibrator
    navihmi::TouchCalibrator touchCalibrator;

    QQmlApplicationEngine engine;

    // ── 服务初始化 + context property 注入（engine.load 之前）──
    runtimeBus.setDataManager(&dataManager);
    runtimeBus.setObjectManager(&objectManager);
    objectManager.registerSystemObject("dataManager", &dataManager);
    objectManager.registerSystemObject("runtimeBus", &runtimeBus);
    objectManager.registerSystemObject("userSystem", &userSystem);
    objectManager.registerSystemObject("alarmEngine", &alarmEngine);
    objectManager.registerSystemObject("dataLogger", &dataLogger);
    engine.rootContext()->setContextProperty("runtimeBus", &runtimeBus);
    engine.rootContext()->setContextProperty("dataManager", &dataManager);
    engine.rootContext()->setContextProperty("objectManager", &objectManager);
    engine.rootContext()->setContextProperty("userSystem", &userSystem);
    engine.rootContext()->setContextProperty("alarmEngine", &alarmEngine);
    engine.rootContext()->setContextProperty("dataLogger", &dataLogger);
    engine.rootContext()->setContextProperty("deviceInfo", &deviceInfo);
    engine.rootContext()->setContextProperty("storageInfo", &storageInfo);
    touchCalibrator.setProjectPath(projectPath);   // FW 自重启(--project)用原始工程路径
    engine.rootContext()->setContextProperty("touchCalibrator", &touchCalibrator);

    // I-1: CLI 命令服务注入（工程数据经 runtimeBus.project() 读取——loadAndInject 后生效）
    commandService.setDataManager(&dataManager);
    commandService.setRuntimeBus(&runtimeBus);
    commandService.setAlarmEngine(&alarmEngine);
    commandService.setDeviceInfo(&deviceInfo);
    commandService.setDataLogger(&dataLogger);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 加载失败";
        return -1;
    }
    qInfo().noquote() << "navigatorhmi-fw: engine.load 完成 rootObjects="
                      << engine.rootObjects().size();   // 诊断(B6-8)
    QObject* rootObj = engine.rootObjects().first();

    // VNC 镜像（eglfs 物理屏照常，额外远程通道，端口默认 5900 见 fw-config.json；按工程 enable_vnc 启停）
    navihmi::VncMirror vncMirror(qobject_cast<QQuickWindow*>(rootObj));
    // QML 生产端脏矩形报告（西门子 dirty-rect 模式：画面变化点调 vncMirror.markDirty）
    engine.rootContext()->setContextProperty("vncMirror", &vncMirror);
    // K-9: VNC 运行时启停注入——SSH CLI 命令（无条件，不依赖 HTTP）；proto enable_vnc=21 启动默认值，运行时指令覆盖
    commandService.setVncMirror(&vncMirror);
#if defined(HAVE_QT_HTTPSERVER)
    // K-9: HTTP 端点注入 + 设备闪烁请求 → QML 覆盖层（亮灭交替 ~1s；main.qml setBlink）
    httpReceiver.setVncMirror(&vncMirror);
    QObject::connect(&httpReceiver, &navihmi::HttpReceiver::blinkRequested, rootObj,
                     [rootObj](bool enable) {
        QMetaObject::invokeMethod(rootObj, "setBlink", Q_ARG(QVariant, QVariant(enable)));
    });
    // M-3 ④：下载/安装进度 → QML 屏幕进度条（退导航→进度→满停 1~2s→自动打开；percent<0 = 失败恢复隐藏）
    QObject::connect(&httpReceiver, &navihmi::HttpReceiver::transferProgress, rootObj,
                     [rootObj](int percent, const QString& stage) {
        QMetaObject::invokeMethod(rootObj, "showTransferProgress",
                                  Q_ARG(QVariant, QVariant(percent)), Q_ARG(QVariant, QVariant(stage)));
    });
#endif

    // 加载并注入工程（B6-8: 抽函数——无 --project / 文件缺失 → 空工程导航模式, 进程不退出）
    // R3: --project 是 ZIP 工程包时整包解压 → 内部 app.navihmi + tiles/ 瓦片
    QString tileBasePath;
    const QString resolvedProject = resolveProjectPackage(projectPath, tileBasePath);
    if (!loadAndInject(rootObj, runtimeBus, dataManager, resolvedProject, &vncMirror, tileBasePath, &userSystem, &alarmEngine, &dataLogger, &acquisition))
        return 1;
    qInfo().noquote() << "navigatorhmi-fw: loadAndInject 完成";   // 诊断(B6-8)

    // I-1: CLI 服务启动（工程加载后——命令数据源就绪）
    if (!cliServer.start())
        qWarning().noquote() << "SSH CLI 服务不可用——navihmi-cli 将无法连接（FW 继续正常运行）";

#if defined(HAVE_QT_HTTPSERVER)
    // K-8b: HTTP 接收端启动（工程加载后）+ 容器就绪 → 工程重载（下载事务性：校验已在接收端完成，此处重载）
    if (!httpReceiver.start())
        qWarning().noquote() << "HTTP 接收端不可用（FW 继续正常运行，无法接收工程部署）";
    QObject::connect(&httpReceiver, &navihmi::HttpReceiver::projectPackageReady, rootObj,
                     [rootObj, &runtimeBus, &dataManager, &vncMirror, &touchCalibrator, &objectManager, &userSystem, &alarmEngine, &dataLogger, &acquisition](const QString& projectPath) {
        QString tileBasePath;
        const QString resolved = resolveProjectPackage(projectPath, tileBasePath);
        // 同步校准重启路径（对齐 storageInfo.projectReplaced 链——校准后自重启拉起新工程）
        touchCalibrator.setProjectPath(projectPath);
        qputenv("NAVIHMI_PROJECT", projectPath.toUtf8());
        // 工程重载前清理 ObjectManager 画面上下文与注册表（防旧工程控件残留寻址幽灵）
        objectManager.setCurrentScreen(QString());
        objectManager.clearScreens();
        loadAndInject(rootObj, runtimeBus, dataManager, resolved, &vncMirror, tileBasePath, &userSystem, &alarmEngine, &dataLogger, &acquisition);
    });
#endif

    // H-8: 写通道联动——DataManager 写 modbus 来源变量 → 同步写设备
    QObject::connect(&dataManager, &navihmi::DataManager::valueChanged, &acquisition,
                     [&acquisition](const QString& tagName, const QVariant& value) {
        acquisition.handleValueWritten(tagName, value);
    });

    // G-2: 报警事件 → alarm_history（AlarmEngine 触发/确认联动 DataLogger）
    QObject::connect(&alarmEngine, &navihmi::AlarmEngine::alarmTriggered,
                     [&dataLogger](const QString& rule, const QString& tag, int level, const QString& msg) {
        dataLogger.recordAlarmEvent(rule, tag, level, msg, QStringLiteral("TRIGGER"));
    });
    QObject::connect(&alarmEngine, &navihmi::AlarmEngine::alarmAcked,
                     [&dataLogger](const QString& rule, const QString& tag, int level, const QString& msg) {
        dataLogger.recordAlarmEvent(rule, tag, level, msg, QStringLiteral("ACK"));
    });
    QObject::connect(&alarmEngine, &navihmi::AlarmEngine::alarmCleared,
                     [&dataLogger](const QString& rule, const QString& tag, int level, const QString& msg) {
        dataLogger.recordAlarmEvent(rule, tag, level, msg, QStringLiteral("CLEAR"));
    });

    // 画面切换（主壳 switchToName 调用）——⑪候选A: 当前画面同步在 QML switchTo 内完成（单一入口,
    // 覆盖 startProject/switchToName/switchTo 全路径; 此处不再重复同步, 避免覆盖 previous）
    runtimeBus.onScreenSwitch = [rootObj](const QString& name) {
        QMetaObject::invokeMethod(rootObj, "switchToName", Q_ARG(QVariant, QVariant(name)));
    };
    // Stop Runtime → 返回导航
    runtimeBus.onStopRuntime = [rootObj]() {
        QMetaObject::invokeMethod(rootObj, "stopRuntime");
    };
    // 存储管理替换默认工程后 → 重新加载注入（B6-8: 替换即时生效, 开始工程打开新工程）
    QObject::connect(&storageInfo, &navihmi::StorageInfo::projectReplaced, rootObj,
                     [rootObj, &runtimeBus, &dataManager, &vncMirror, &touchCalibrator, &objectManager, &userSystem, &alarmEngine, &dataLogger, &acquisition]() {
        QString tileBasePath;
        const QString defaultPath = navihmi::StorageInfo::defaultProjectPath();
        const QString resolved = resolveProjectPackage(defaultPath, tileBasePath);
        // 审查 7e0143b8: 替换工程后同步校准重启路径, 否则校准写 pointercal 后"重启生效"
        // 拉起的是替换前的旧工程
        touchCalibrator.setProjectPath(defaultPath);
        qputenv("NAVIHMI_PROJECT", defaultPath.toUtf8());
        // 审查 M3(2026-08-23 G-0): 工程重载前清理 ObjectManager 画面上下文与注册表——
        // 防旧工程画面控件残留注册, 新工程 set_property 按旧画面名寻址到幽灵控件
        objectManager.setCurrentScreen(QString());
        objectManager.clearScreens();
        loadAndInject(rootObj, runtimeBus, dataManager, resolved, &vncMirror, tileBasePath, &userSystem, &alarmEngine, &dataLogger, &acquisition);
    });

    qInfo().noquote() << "navigatorhmi-fw: 进入事件循环";   // 诊断(B6-8)
    const int execRc = app.exec();
    qInfo().noquote() << "navigatorhmi-fw: app.exec() 返回 rc=" << execRc;   // 诊断(B6-8)
    return execRc;
#else
    qWarning().noquote() << "当前构建无 Qt Qml/Quick（转换器模式可用 --convert）；QML 界面需 buildroot 补装 Qt6 QML 模块";
    return 0;
#endif
}
