/*
 * @Author: aWhiteWizard www.123518341@qq.com
 * @FilePath: \NavigatorHMI_FW\src\main.cpp
 * @Description: NavigatorHMI FW 应用入口（跨平台）
 *               同一份代码：RK3128（Linux ARM, Qt 6.5.6）+ Windows 桌面（仿真器）
 *               输入：组态软件编译的 .navihmi（fw/proto/navihmi.proto 契约）
 *               用法：navihmi-fw --project xxx.navihmi
 */
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QDebug>

// FW 运行时模块（后续迭代逐个接入）
// #include "runtime/projectparser.h"
// #include "runtime/tagstore.h"
// #include "runtime/alarmengine.h"
// #include "ui/hmiview.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NavigatorHMI_FW"));
    app.setApplicationVersion(QStringLiteral("1.1.0"));

    // 平台后端：Linux 嵌入式按 FW_PLATFORM_BACKEND 设 QPA（linuxfb/eglfs，CMake -D 配置）
#if !defined(Q_OS_WIN)
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral(FW_PLATFORM_BACKEND));
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NavigatorHMI FW 应用（RK3128 / Windows 仿真器）"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringLiteral("project"), QStringLiteral(".navihmi 工程文件路径"), QStringLiteral("path")));
    parser.process(app);

    const QString projectPath = parser.value(QStringLiteral("project"));

    // ═══════ 工程解析（骨架：ProjectParser 在 runtime/ 迭代中实现）═══════
    // ProjectModel model;
    // ProjectParser projectParser;
    // if (!projectParser.load(projectPath, &model)) {
    //     qCritical().noquote() << "工程加载失败:" << projectPath;
    //     return 1;
    // }

    // ═══════ QML 引擎 ═══════
    QQmlApplicationEngine engine;
    // engine.rootContext()->setContextProperty("hmi", &hmiView);   // 控件/报警/设值接口
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 加载失败";
        return -1;
    }

    return app.exec();
}
