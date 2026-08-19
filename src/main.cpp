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
#endif
#include <QCommandLineParser>
#include <QDebug>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMetaObject>
#include <cstdio>

#include "converter/projectparser.h"
#include "converter/qmlgenerator.h"
#include "runtime/projectmodel.h"
#if defined(HAVE_QT_QML)
#include "runtime/runtimebus.h"
#include "runtime/datamanager.h"
#include "runtime/deviceinfo.h"
#include "runtime/storageinfo.h"
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
                          const QString& projectPath)
{
    navihmi::Project proj;
    if (!projectPath.isEmpty() && !navihmi::ProjectParser::parseFile(projectPath, proj)) {
        qCritical().noquote() << "工程加载失败:" << projectPath;
        return false;
    }
    runtimeBus.setProject(proj);      // 内部重置画面匹配状态（⑪候选A）
    dataManager.setProject(proj);

    // 生成画面 QML 到临时目录（每画面 + overlay + 主壳）
    QDir genDir(QDir::tempPath() + "/navihmi_gen");
    genDir.mkpath(".");
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
            content = navihmi::QmlGenerator::generateWorldMap(proj);
        } else if (sc.type == navihmi::ScreenType::Template) {
            fname = QStringLiteral("overlay.qml");
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
    int devW = proj.deviceWidth > 0 ? proj.deviceWidth : 1024;
    int devH = proj.deviceHeight > 0 ? proj.deviceHeight : 600;
    rootObj->setProperty("deviceWidth", devW);
    rootObj->setProperty("deviceHeight", devH);

    // overlay 生成（Stop Runtime 按钮所在）——无 Template 画面时写空 overlay（Truncate 覆盖,
    // 防 reload 后旧工程 overlay 残留导致旧全局控件事件误触发）
    bool hasTemplate = false;
    for (const auto& sc : proj.screens)
        if (sc.type == navihmi::ScreenType::Template) { hasTemplate = true; break; }
    QFile overlayFile(genDir.filePath("overlay.qml"));
    if (!hasTemplate) {
        overlayFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        overlayFile.write("import QtQuick 2.15\nItem { width: 1024; height: 600 }\n");
        overlayFile.close();
    }
    rootObj->setProperty("overlayFile", genDir.filePath("overlay.qml"));
    return true;
}
#endif

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

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NavigatorHMI_FW"));
    app.setApplicationVersion(QStringLiteral("1.1.0"));

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

    // ═══════ QML 引擎 ═══════
#if defined(HAVE_QT_QML)
    QQmlApplicationEngine engine;

    // 运行时事件总线（QML 只发事件，C++ 执行动作；工程注入见 loadAndInject）
    navihmi::RuntimeBus runtimeBus;

    // 数据管理器（TagStore 雏形：变量实时值中心, QML 组件绑定显示）
    navihmi::DataManager dataManager;
    runtimeBus.setDataManager(&dataManager);
    engine.rootContext()->setContextProperty("runtimeBus", &runtimeBus);
    engine.rootContext()->setContextProperty("dataManager", &dataManager);

    // 设备信息（B6-6: IP/MAC/版本/内核/运行时间真实读取, 导航页显示）
    navihmi::DeviceInfo deviceInfo;
    engine.rootContext()->setContextProperty("deviceInfo", &deviceInfo);

    // 存储信息（B6-7: SD/USB 真实检测 + 工程扫描/替换）
    navihmi::StorageInfo storageInfo;
    engine.rootContext()->setContextProperty("storageInfo", &storageInfo);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 加载失败";
        return -1;
    }
    QObject* rootObj = engine.rootObjects().first();

    // 加载并注入工程（B6-8: 抽函数——无 --project / 文件缺失 → 空工程导航模式, 进程不退出）
    if (!loadAndInject(rootObj, runtimeBus, dataManager, projectPath))
        return 1;

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
                     [rootObj, &runtimeBus, &dataManager]() {
        loadAndInject(rootObj, runtimeBus, dataManager,
                      navihmi::StorageInfo::defaultProjectPath());
    });

    return app.exec();
#else
    qWarning().noquote() << "当前构建无 Qt Qml/Quick（转换器模式可用 --convert）；QML 界面需 buildroot 补装 Qt6 QML 模块";
    return 0;
#endif
}
