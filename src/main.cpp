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
#include <cstdio>

#include "converter/projectparser.h"
#include "runtime/projectmodel.h"

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

int main(int argc, char *argv[])
{
    // ── 转换器模式（纯命令行，无需 GUI/Qt 平台插件）──
    // 在 QGuiApplication 之前处理 --convert：避免无 QPA 插件环境下启动失败
    {
        bool convertMode = false;
        QString convertPath;
        for (int i = 1; i < argc; ++i) {
            if (qstrcmp(argv[i], "--convert") == 0) {
                convertMode = true;
                if (i + 1 < argc) convertPath = QString::fromLocal8Bit(argv[i + 1]);
            }
        }
        if (convertMode) {
            if (convertPath.isEmpty()) {
                qCritical() << "用法: NavigatorHMI_FW --convert <xxx.navihmi>";
                return 1;
            }
            return runConvert(convertPath);
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

    // ═══════ 工程解析 ═══════
    navihmi::Project proj;
    if (!projectPath.isEmpty()) {
        if (!navihmi::ProjectParser::parseFile(projectPath, proj)) {
            qCritical().noquote() << "工程加载失败:" << projectPath;
            return 1;
        }
    }

    // ═══════ QML 引擎 ═══════
#if defined(HAVE_QT_QML)
    QQmlApplicationEngine engine;
    // engine.rootContext()->setContextProperty("hmi", &hmiView);   // 控件/报警/设值接口
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 加载失败";
        return -1;
    }
    return app.exec();
#else
    qWarning().noquote() << "当前构建无 Qt Qml/Quick（转换器模式可用 --convert）；QML 界面需 buildroot 补装 Qt6 QML 模块";
    return 0;
#endif
}
