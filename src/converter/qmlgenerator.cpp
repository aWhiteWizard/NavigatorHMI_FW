/*
 * @FilePath: \NavigatorHMI_FW\src\converter\qmlgenerator.cpp
 * @Description: HMIProject 运行时模型 → 每画面 QML 文件
 */
#include "converter/qmlgenerator.h"

#include <QTextStream>
#include <QStringList>
#include <cmath>

namespace navihmi {

namespace {

// 控件类型 → QML 组件名（映射表，B4-5）
QString widgetQmlType(WidgetType t)
{
    switch (t) {
    case WidgetType::Button: return "HmiButton";
    case WidgetType::Text: return "HmiText";
    case WidgetType::Label: return "HmiLabel";
    case WidgetType::Rectangle: return "HmiRectangle";
    case WidgetType::Image: return "HmiImage";
    case WidgetType::NumericDisplay: return "HmiNumericDisplay";
    case WidgetType::Switch: return "HmiSwitch";
    case WidgetType::Line: return "HmiLine";
    case WidgetType::Circle: return "HmiCircle";
    case WidgetType::Ellipse: return "HmiEllipse";
    case WidgetType::IoField: return "HmiIoField";
    case WidgetType::CheckBox: return "HmiCheckBox";
    case WidgetType::TextList: return "HmiTextList";
    case WidgetType::Frame: return "HmiFrame";
    case WidgetType::ProgressBar: return "HmiProgressBar";
    case WidgetType::DateTime: return "HmiDateTime";
    case WidgetType::Window: return "HmiWindow";
    case WidgetType::Polygon: return "HmiPolygon";
    default: return "HmiRectangle";
    }
}

// 转义 QML 字符串
QString qmlEsc(const QString& s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    return r;
}

// 属性行（仅非空/非零值）
void appendProp(QTextStream& out, const QString& name, const QString& val)
{
    if (!val.isEmpty())
        out << "    " << name << ": \"" << qmlEsc(val) << "\"\n";
}
void appendProp(QTextStream& out, const QString& name, double val)
{
    if (val != 0)
        out << "    " << name << ": " << QString::number(val, 'f', 8) << "\n";
}
void appendProp(QTextStream& out, const QString& name, int val)
{
    if (val != 0)
        out << "    " << name << ": " << val << "\n";
}
void appendProp(QTextStream& out, const QString& name, bool val)
{
    if (val)
        out << "    " << name << ": true\n";
}

// 生成单控件 QML（事件信号占位）
void generateWidget(QTextStream& out, const Widget& w)
{
    const QString type = widgetQmlType(w.type);
    out << "    " << type << " {\n";
    out << "        objectName: \"" << qmlEsc(w.objectName) << "\"\n";
    appendProp(out, "x", w.x);
    appendProp(out, "y", w.y);
    appendProp(out, "width", w.width);
    appendProp(out, "height", w.height);
    appendProp(out, "boundTag", w.boundTag);
    appendProp(out, "text", w.text);
    appendProp(out, "content", w.content);
    appendProp(out, "hAlign", w.hAlign);
    appendProp(out, "fontFamily", w.fontFamily);
    appendProp(out, "fontSize", w.fontSize);
    appendProp(out, "fontWeight", w.fontWeight);
    appendProp(out, "fontStyle", w.fontStyle);
    appendProp(out, "textDecoration", w.textDecoration);
    appendProp(out, "textColor", w.textColor);
    appendProp(out, "fillColor", w.fillColor);
    appendProp(out, "strokeColor", w.strokeColor);
    appendProp(out, "strokeThickness", w.strokeThickness);
    appendProp(out, "imagePath", w.imagePath);
    appendProp(out, "stretchMode", w.stretchMode);
    appendProp(out, "listRef", w.listRef);
    appendProp(out, "defaultIndex", w.defaultIndex);
    appendProp(out, "value", w.value);
    appendProp(out, "min", w.min);
    appendProp(out, "max", w.max);
    appendProp(out, "fillStyle", w.fillStyle);
    appendProp(out, "isOn", w.isOn);
    appendProp(out, "onText", w.onText);
    appendProp(out, "offText", w.offText);
    appendProp(out, "isChecked", w.isChecked);
    appendProp(out, "isReadOnly", w.isReadOnly);
    appendProp(out, "x2", w.x2);
    appendProp(out, "y2", w.y2);
    appendProp(out, "title", w.title);
    appendProp(out, "dtText", w.dtText);
    appendProp(out, "dtFormat", w.dtFormat);
    // Window 专属属性（仅 W_WINDOW 类型输出, 避免其他组件收到无关属性）
    if (w.type == WidgetType::Window) {
        appendProp(out, "windowType", int(w.windowType));
        appendProp(out, "winTitle", w.winTitle);
        appendProp(out, "showTitleBar", w.showTitleBar);
        appendProp(out, "showHistory", w.showHistory);
        appendProp(out, "selectedTag", w.selectedTag);
        appendProp(out, "cardWidth", w.cardWidth);
        appendProp(out, "cardHeight", w.cardHeight);
        appendProp(out, "showUserName", w.showUserName);
        appendProp(out, "showRole", w.showRole);
        appendProp(out, "showMode", w.showMode);
        appendProp(out, "cardShowNumber", w.cardShowNumber);
        appendProp(out, "cardShowStatus", w.cardShowStatus);
        appendProp(out, "cardShowLocation", w.cardShowLocation);
        appendProp(out, "boundDevice", w.boundDevice);
    }
    // Polygon 顶点（仅 W_POLYGON 类型, QML 数组 [{x,y},...]）
    if (w.type == WidgetType::Polygon && !w.points.isEmpty()) {
        out << "        points: [";
        for (int i = 0; i < w.points.size(); ++i) {
            if (i) out << ", ";
            out << "{ x: " << QString::number(w.points[i].x(), 'f', 2)
                << ", y: " << QString::number(w.points[i].y(), 'f', 2) << " }";
        }
        out << "]\n";
    }

    // 事件占位：onClick 等 → 信号处理器（联动 ActionRunner 后续循环接入）
    for (const auto& ev : w.events) {
        QString signalName;
        switch (ev.type) {
        case EventType::OnClick: signalName = "onHmiClicked"; break;
        case EventType::OnPress: signalName = "onHmiPressed"; break;
        case EventType::OnRelease: signalName = "onHmiReleased"; break;
        case EventType::OnValueChange: signalName = "onHmiValueChanged"; break;
        case EventType::OnAlarmTrigger: signalName = "onHmiAlarmTrigger"; break;
        case EventType::OnAlarmAck: signalName = "onHmiAlarmAck"; break;
        case EventType::OnAlarmClear: signalName = "onHmiAlarmClear"; break;
        case EventType::OnScreenLoad: signalName = "onHmiScreenLoad"; break;
        case EventType::OnScreenUnload: signalName = "onHmiScreenUnload"; break;
        case EventType::OnTimer: signalName = "onHmiTimer"; break;
        case EventType::OnSystemStart: signalName = "onHmiSystemStart"; break;
        case EventType::OnSystemShutdown: signalName = "onHmiSystemShutdown"; break;
        case EventType::OnInput: signalName = "onHmiInput"; break;
        case EventType::OnOn: signalName = "onHmiOn"; break;
        case EventType::OnOff: signalName = "onHmiOff"; break;
        case EventType::OnProgressComplete: signalName = "onHmiProgressComplete"; break;
        case EventType::OnUserChanged: signalName = "onHmiUserChanged"; break;
        case EventType::OnAck: signalName = "onHmiAck"; break;
        case EventType::OnSelect: signalName = "onHmiSelect"; break;
        default: signalName = "onHmi" + QString::number(int(ev.type)); break;
        }
        // 占位：eventType 供后续 ActionRunner 路由（QML 只发事件，动作由 C++ 执行）
        out << "        " << signalName << ": function() { runtimeBus.emitEvent(\""
            << qmlEsc(w.objectName) << "\", " << int(ev.type) << "); }\n";
    }
    out << "    }\n";
}

} // anonymous namespace

QString QmlGenerator::generateScreen(const Project& proj, const Screen& screen)
{
    Q_UNUSED(proj)
    QString out;
    QTextStream ts(&out);
    ts << "import QtQuick 2.15\n";
    ts << "import QtQuick.Controls 2.15\n";
    ts << "import \"qrc:/qml/components\"\n\n";   // Hmi* 组件库 (qrc 内)
    ts << "// 画面: " << screen.name << " (由转换器生成, 勿手改)\n";
    ts << "Item {\n";
    ts << "    id: screenRoot\n";
    ts << "    width: " << screen.width << "\n";
    ts << "    height: " << screen.height << "\n";
    for (const auto& w : screen.widgets)
        generateWidget(ts, w);
    ts << "}\n";
    return out;
}

QString QmlGenerator::generateWorldMap(const Project& proj)
{
    QString out;
    QTextStream ts(&out);
    ts << "import QtQuick 2.15\n";
    ts << "import \"qrc:/qml/components\"\n\n";   // HmiWorldMap 组件 (qrc 内)
    ts << "// 世界地图: " << proj.worldMap.tileSource
       << " (由转换器生成)\n";
    ts.setRealNumberPrecision(8);   // GPS 坐标精度 (与 appendProp 'f',8 一致)
    ts << "HmiWorldMap {\n";
    ts << "    width: " << proj.deviceWidth << "\n";
    ts << "    height: " << proj.deviceHeight << "\n";
    // bounds 全 0 = 未配置, 兜底成都范围 (2026-08-18 用户定: 世界地图放成都市)
    double latMin = proj.worldMap.latMin, latMax = proj.worldMap.latMax;
    double lngMin = proj.worldMap.lngMin, lngMax = proj.worldMap.lngMax;
    if (latMin == 0 && latMax == 0 && lngMin == 0 && lngMax == 0) {
        latMin = 30.55; latMax = 30.72; lngMin = 103.90; lngMax = 104.15;
    }
    ts << "    latMin: " << latMin << "\n";
    ts << "    latMax: " << latMax << "\n";
    ts << "    lngMin: " << lngMin << "\n";
    ts << "    lngMax: " << lngMax << "\n";
    ts << "    zoomLevel: " << proj.worldMap.zoomLevel << "\n";
    ts << "    showGlobalOverlay: " << (proj.worldMap.showGlobalOverlay ? "true" : "false") << "\n";
    ts << "    viewLocked: " << (proj.worldMap.viewLocked ? "true" : "false") << "\n";
    // 作业点
    ts << "    workPoints: [\n";
    for (int i = 0; i < proj.worldMap.workPoints.size(); ++i) {
        const auto& wp = proj.worldMap.workPoints[i];
        ts << "        { name: \"" << qmlEsc(wp.name) << "\", lng: "
           << wp.fixedPoint.longitude << ", lat: " << wp.fixedPoint.latitude
           << ", boundTag: \"" << qmlEsc(wp.boundTag) << "\" }";
        ts << (i < proj.worldMap.workPoints.size() - 1 ? ",\n" : "\n");
    }
    ts << "    ]\n";
    // 范围点
    ts << "    workRange: [\n";
    for (int i = 0; i < proj.worldMap.workRangePoints.size(); ++i) {
        const auto& rp = proj.worldMap.workRangePoints[i];
        ts << "        { lng: " << rp.fixedPoint.longitude
           << ", lat: " << rp.fixedPoint.latitude
           << ", boundTag: \"" << qmlEsc(rp.boundTag) << "\" }";
        ts << (i < proj.worldMap.workRangePoints.size() - 1 ? ",\n" : "\n");
    }
    ts << "    ]\n";
    ts << "}\n";
    return out;
}

QString QmlGenerator::generateOverlay(const Project& proj)
{
    QString out;
    QTextStream ts(&out);
    ts << "import QtQuick 2.15\n";
    ts << "import \"qrc:/qml/components\"\n\n";   // Hmi* 组件库 (qrc 内)
    ts << "// 全局画面叠加层 (由转换器生成)\n";
    ts << "Item {\n";
    ts << "    id: overlayRoot\n";
    ts << "    width: " << proj.deviceWidth << "\n";
    ts << "    height: " << proj.deviceHeight << "\n";
    // 全局画面控件（含 Stop Runtime 按钮）
    for (const auto& sc : proj.screens) {
        if (sc.type == ScreenType::Template) {
            for (const auto& w : sc.widgets)
                generateWidget(ts, w);
        }
    }
    ts << "}\n";
    return out;
}

QList<QPair<QString, QString>> QmlGenerator::generateAll(const Project& proj)
{
    QList<QPair<QString, QString>> files;
    int idx = 0;
    for (const auto& sc : proj.screens) {
        if (sc.type == ScreenType::WorldMap) {
            files.append({QStringLiteral("screen_%1.qml").arg(idx), generateWorldMap(proj)});
        } else if (sc.type == ScreenType::Template) {
            // 全局画面 → overlay.qml（单独文件）
            files.append({QStringLiteral("overlay.qml"), generateOverlay(proj)});
        } else {
            files.append({QStringLiteral("screen_%1.qml").arg(idx), generateScreen(proj, sc)});
        }
        ++idx;
    }
    // 运行时主壳固定用 qrc:/qml/main.qml（带导航/Stop/startProject 逻辑），
    // 不生成 main.qml（避免与 qrc 版不一致造成误导）
    return files;
}

} // namespace navihmi
