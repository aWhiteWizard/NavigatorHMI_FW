/*
 * @FilePath: \NavigatorHMI_FW\src\converter\qmlgenerator.cpp
 * @Description: HMIProject 运行时模型 → 每画面 QML 文件
 */
#include "converter/qmlgenerator.h"

#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <cmath>

namespace navihmi {

namespace {

// P-2c（2026-09-02）：控件类型 → QML 组件名——default 不再静默回退 HmiRectangle（错读比跳过危险）。
// 正常管线经 P-2b 解析过滤（未知控件不进模型），此处 default 为防御残留（防未来绕过 parser 直构模型）；
// 未知 → 返回 false + qWarning 告警，调用方跳过该控件（不生成其 QML）。与 F1 解析端策略一致（v1.1-design §5.3 F8）。
bool widgetQmlType(WidgetType t, QString& out)
{
    switch (t) {
    case WidgetType::Button: out = "HmiButton"; return true;
    case WidgetType::Text: out = "HmiText"; return true;
    case WidgetType::Label: out = "HmiLabel"; return true;
    case WidgetType::Rectangle: out = "HmiRectangle"; return true;
    case WidgetType::Image: out = "HmiImage"; return true;
    case WidgetType::NumericDisplay: out = "HmiNumericDisplay"; return true;
    case WidgetType::Switch: out = "HmiSwitch"; return true;
    case WidgetType::Line: out = "HmiLine"; return true;
    case WidgetType::Circle: out = "HmiCircle"; return true;
    case WidgetType::Ellipse: out = "HmiEllipse"; return true;
    case WidgetType::IoField: out = "HmiIoField"; return true;
    case WidgetType::CheckBox: out = "HmiCheckBox"; return true;
    case WidgetType::TextList: out = "HmiTextList"; return true;
    case WidgetType::Frame: out = "HmiFrame"; return true;
    case WidgetType::ProgressBar: out = "HmiProgressBar"; return true;
    case WidgetType::DateTime: out = "HmiDateTime"; return true;
    case WidgetType::Window: out = "HmiWindow"; return true;
    case WidgetType::Polygon: out = "HmiPolygon"; return true;
    case WidgetType::TrendView: out = "HmiTrendView"; return true;   // P-4
    case WidgetType::HistoryView: out = "HmiHistoryView"; return true;   // P-5（组件已实现 2026-09-02）
    default:
        qWarning("qmlgenerator: 未知控件类型 %d —— 跳过该控件 QML 生成", static_cast<int>(t));
        return false;
    }
}

// 转义 QML 字符串
QString qmlEsc(const QString& s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    return r;
}

// D-B2: 控件图片路径 → 设备端绝对落盘路径（resourceRoot/res/<rel>）
// PC 打包 target 保留工程内相对路径（DeploymentPackageBuilder），FW 接收端落盘 工程目录/res/<target>
// （httreceiver res 按 target 落盘）→ 图片文件实际在 <resourceRoot>/res/<rel>。
// resourceRoot 空（纯转换器模式）或已是绝对路径 → 原样返回；相对路径 → 拼 resourceRoot/res/ 前缀。
// 正斜杠统一（QML file 路径 Windows 反斜杠无效；设备端是 Linux 无歧义，统一防双平台不一致）。
QString resolveResPath(const QString& raw, const QString& resourceRoot)
{
    QString p = raw.trimmed();
    // T-4b（2026-09-05 T 循环）：剥首尾成对引号（对齐 resolveVideoPath 0e2d732 先例——图片源带引号复制
    // 粘贴场景；引号非路径合法字符，不剥则 QFileInfo(p).isAbsolute() 误判/拼 res/ 前缀错乱）
    if (p.size() >= 2 && (p.front() == QLatin1Char('"') || p.front() == QLatin1Char('\'')) && p.back() == p.front())
        p = p.mid(1, p.size() - 2).trimmed();
    if (p.isEmpty() || resourceRoot.isEmpty())
        return p;
    if (QFileInfo(p).isAbsolute())
        return p;
    QString root = resourceRoot;
    if (!root.endsWith(QLatin1Char('/')))
        root += QLatin1Char('/');
    return root + QStringLiteral("res/") + p;
}

// P-6: Frame 视频源 → 设备端可加载路径（与 resolveResPath 同构，前缀 media/——PC DeploymentPackageBuilder
// 视频收集入包 media/ 目录，FW 落盘 <工程目录>/media/<rel>，.navihmi 引用改写为 rel（无前缀））
// 网络流 URL（rtsp/http/https）→ 原样（RTSP 不入包，设备端直连）；绝对路径 → 原样（用户自备设备端文件）；
// 相对路径 → 拼 resourceRoot/media/ 前缀；resourceRoot 空（纯转换器模式）→ 原样。
QString resolveVideoPath(const QString& raw, const QString& resourceRoot)
{
    QString p = raw.trimmed();
    // Check ④（2026-09-05 用户）：视频链接带双引号（复制粘贴常见 `"rtsp://..."`）——剥首尾成对引号后再判
    // 协议/路径（引号非 URL/路径合法字符，不剥则 rtsp:// 前缀判断失配、被误拼 media/ 前缀；PC Video 列表
    // create/update 已清洗去引号，此处兜底单源 videoSource / 文本型列表项等其余路径）
    if (p.size() >= 2 && (p.front() == QLatin1Char('"') || p.front() == QLatin1Char('\'')) && p.back() == p.front())
        p = p.mid(1, p.size() - 2).trimmed();
    if (p.isEmpty() || resourceRoot.isEmpty())
        return p;
    if (p.startsWith(QLatin1String("rtsp://"), Qt::CaseInsensitive)
        || p.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        || p.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
        return p;
    if (QFileInfo(p).isAbsolute())
        return p;
    QString root = resourceRoot;
    if (!root.endsWith(QLatin1Char('/')))
        root += QLatin1Char('/');
    return root + QStringLiteral("media/") + p;
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

// 生成单控件 QML（事件信号占位）；proj 用于 TextList 的 listRef → content 展开（列表项拼入 content,
// HmiTextList 用 content.split(",") 渲染——列表数据源在工程模型 ListDef 里）
// screenName: 所属画面名（G-0: 控件加载/销毁时向 ObjectManager 注册/注销, 跨画面寻址依据）
// resourceRoot: 设备端资源根目录（工程目录绝对路径, 如 /mnt/user/userdata）——非空时控件图片路径
//               解析为绝对落盘路径（D-B2: QML 相对路径按文档基址 /tmp/navihmi_gen/ 解析不指向工程目录）
void generateWidget(QTextStream& out, const Widget& w, const Project& proj, const QString& screenName,
                    const QString& resourceRoot = QString())
{
    QString type;
    if (!widgetQmlType(w.type, type))
        return;   // P-2c：未知控件类型 → 跳过该控件（不生成 QML；qWarning 已打——与 F1 解析端一致）
    out << "    " << type << " {\n";
    out << "        objectName: \"" << qmlEsc(w.objectName) << "\"\n";
    appendProp(out, "x", w.x);
    appendProp(out, "y", w.y);
    appendProp(out, "width", w.width);
    appendProp(out, "height", w.height);
    appendProp(out, "boundTag", w.boundTag);
    // D+ 审查修复(2b3d04bd)+用户 2026-08-22: IOField 键盘按绑定变量类型区分——
    // 整型→纯数字(ImhDigitsOnly)、浮点/坐标→数字+小数点(ImhFormattedNumbersOnly)、
    // 布尔→双按钮选择(isBoolean, 非文本输入)、字符串/其它→全键盘(默认)
    if (w.type == WidgetType::IoField && !w.boundTag.isEmpty()) {
        const Tag* tg = proj.TagByName(w.boundTag);
        if (tg) {
            switch (tg->dataType) {
            case TagDataType::Int16: case TagDataType::Uint16:
            case TagDataType::Int32:
                out << "    inputMethodHints: Qt.ImhDigitsOnly\n";
                break;
            case TagDataType::Float:
                out << "    inputMethodHints: Qt.ImhFormattedNumbersOnly\n";
                break;
            case TagDataType::Gps:
                // F 循环(2026-08-23 用户): GPS 坐标 iofield——isGps 标记启用度分秒显示/解析
                // (对齐 PC 端 GeoPoint 契约: 坐标对 经度,纬度, DMS 前缀式 "E104°3'30\", N30°40'20\"";
                // 显示态转 DMS, 编辑态回小数, 提交双格式解析写回括号基准值); 键盘仍数字+小数点
                out << "    inputMethodHints: Qt.ImhFormattedNumbersOnly\n";
                out << "    isGps: true\n";
                // X-2（2026-09-08 用户规格——GPS CoordinatePicker）：注入工程世界地图底图 + 显示范围 bounds
                // （CoordinatePicker 地图选点用——与画面 HmiWorldMap 同底图同坐标系）；底图缺失不注入
                // → IO Field 回退文本编辑（正常工程被 PC 编译校验拦截，此处防手工部署绕过）
                // X 修复（2026-09-10 Check）：bounds 全 0（工程未手动配显示区域——FW 按点包围盒自适应）→
                //   用作业点+范围点 fixedPoint（非 0）包围盒兜底注入（CoordinatePicker 底图范围 ≈ 画面自适应范围）
                {
                    QString gpsBg;
                    if (!resourceRoot.isEmpty()) {
                        if (QFile::exists(resourceRoot + QStringLiteral("/res/worldmap_bg.png")))
                            gpsBg = resourceRoot + QStringLiteral("/res/worldmap_bg.png");
                        else if (QFile::exists(resourceRoot + QStringLiteral("/worldmap_bg.png")))
                            gpsBg = resourceRoot + QStringLiteral("/worldmap_bg.png");
                    }
                    const auto& wmc = proj.worldMap;
                    double mLngMin = wmc.lngMin, mLngMax = wmc.lngMax;
                    double mLatMin = wmc.latMin, mLatMax = wmc.latMax;
                    if (!(mLngMax > mLngMin && mLatMax > mLatMin)) {
                        // 兜底：作业点+范围点 fixedPoint 包围盒（绑变量点运行时值取不到——fixedPoint 覆盖围栏/固定点场景）
                        bool any = false;
                        auto fold = [&](double lng, double lat) {
                            if (lng == 0 && lat == 0) return;
                            if (!any) { mLngMin = mLngMax = lng; mLatMin = mLatMax = lat; any = true; }
                            else {
                                if (lng < mLngMin) mLngMin = lng;
                                if (lng > mLngMax) mLngMax = lng;
                                if (lat < mLatMin) mLatMin = lat;
                                if (lat > mLatMax) mLatMax = lat;
                            }
                        };
                        for (const auto& wp : wmc.workPoints)
                            fold(wp.fixedPoint.longitude, wp.fixedPoint.latitude);
                        for (const auto& rp : wmc.workRangePoints)
                            fold(rp.fixedPoint.longitude, rp.fixedPoint.latitude);
                    }
                    if (!gpsBg.isEmpty() && mLngMax > mLngMin && mLatMax > mLatMin) {
                        out << "    gpsMapBg: \"" << qmlEsc(gpsBg) << "\"\n";
                        // 🟡 reviewer：'f',8 精度（对齐 generateWorldMap setRealNumberPrecision(8)——6 位默认会致 ~百 m 坐标误差）
                        out << "    gpsMapLngMin: " << QString::number(mLngMin, 'f', 8) << "\n";
                        out << "    gpsMapLngMax: " << QString::number(mLngMax, 'f', 8) << "\n";
                        out << "    gpsMapLatMin: " << QString::number(mLatMin, 'f', 8) << "\n";
                        out << "    gpsMapLatMax: " << QString::number(mLatMax, 'f', 8) << "\n";
                    }
                }
                break;
            case TagDataType::Bool:
                out << "    isBoolean: true\n";
                break;
            default: break;  // String/DateTime → 全键盘(默认)
            }
        }
    }
    appendProp(out, "text", w.text);
    // D+: TextList 且未显式设 content 时, 按 listRef 从工程列表展开为逗号分隔项（供 HmiTextList 渲染）
    QString content = w.content;
    if (w.type == WidgetType::TextList && content.isEmpty() && !w.listRef.isEmpty()) {
        const ListDef* ld = proj.ListByName(w.listRef, ListType::Text);   // U-2：TextList 消费 Text 型列表（同名跨类型不串）
        if (ld) content = ld->items.join(QLatin1Char(','));
    }
    appendProp(out, "content", content);
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
    // D-B2: Image 控件图片路径统一解析为设备端绝对落盘路径（resourceRoot/res/<rel>）——
    // QML 相对路径按文档基址 /tmp/navihmi_gen/ 解析不指向工程目录（对齐 backgroundImage 先例）;
    // resourceRoot 空（纯转换器模式）保持原样。
    // ① 静态图 imagePath：非空时输出解析后绝对路径（替代裸相对）
    // ② 列表 listItems：listRef 非空时按工程列表展开为 | 分隔绝对路径串（对齐 TextList content 展开先例;
    //    路径含 | 字符的项会被拆错, 与 TextList 逗号分隔同局限, ListDef 项含分隔符需规避）
    if (w.type == WidgetType::Image) {
        if (!resourceRoot.isEmpty() && !w.imagePath.isEmpty())
            appendProp(out, "imagePath", resolveResPath(w.imagePath, resourceRoot));
        else
            appendProp(out, "imagePath", w.imagePath);
        if (!w.listRef.isEmpty()) {
            const ListDef* imgList = proj.ListByName(w.listRef, ListType::Image);   // U-2：Image 消费 Image 型列表
            if (imgList) {
                QStringList absItems;
                for (const auto& item : imgList->items)
                    absItems << resolveResPath(item, resourceRoot);
                appendProp(out, "listItems", absItems.join(QLatin1Char('|')));
            }
        }
    } else {
        appendProp(out, "imagePath", w.imagePath);
    }
    appendProp(out, "stretchMode", w.stretchMode);
    appendProp(out, "listRef", w.listRef);
    appendProp(out, "defaultIndex", w.defaultIndex);
    appendProp(out, "value", w.value);
    appendProp(out, "min", w.min);
    appendProp(out, "max", w.max);
    appendProp(out, "fillStyle", w.fillStyle);
    appendProp(out, "isOn", w.isOn);
    appendProp(out, "labelOn", w.labelOn);
    appendProp(out, "labelOff", w.labelOff);
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
        appendProp(out, "displayMode", w.displayMode);   // P-5：AlarmView 显示模式（0=当前 1=缓冲）
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
        // G-1c: robotSlots 逐组变量绑定（每组: id/status/location/detail/oper 变量名）
        if (!w.robotSlots.isEmpty()) {
            out << "    robotSlots: [\n";
            for (const auto& slot : w.robotSlots) {
                out << "        { id: \"" << qmlEsc(slot.value("id")) << "\""
                    << ", status: \"" << qmlEsc(slot.value("status")) << "\""
                    << ", location: \"" << qmlEsc(slot.value("location")) << "\""
                    << ", detail: \"" << qmlEsc(slot.value("detail")) << "\""
                    << ", oper: \"" << qmlEsc(slot.value("oper")) << "\" }";
                out << (slot == w.robotSlots.last() ? "\n" : ",\n");
            }
            out << "    ]\n";
        }
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
    // P-4 趋势图属性（仅 W_TREND_VIEW 类型输出）
    if (w.type == WidgetType::TrendView) {
        appendProp(out, "trendMode", w.trendMode);
        appendProp(out, "trendTagA", w.trendTagA);
        appendProp(out, "trendTagB", w.trendTagB);
        if (w.sampleIntervalMs > 0) appendProp(out, "sampleIntervalMs", w.sampleIntervalMs);
        if (w.timeWindowSeconds > 0) appendProp(out, "timeWindowSeconds", w.timeWindowSeconds);
        appendProp(out, "lineColor", w.lineColor);
        if (w.lineWidth > 0) appendProp(out, "lineWidth", w.lineWidth);
        if (w.refreshRateMs > 0) appendProp(out, "refreshRateMs", w.refreshRateMs);
    }
    // P-5 历史记录属性（仅 W_HISTORY_VIEW 类型输出）
    if (w.type == WidgetType::HistoryView) {
        if (!w.historyTags.isEmpty()) {
            out << "    historyTags: [";
            for (int i = 0; i < w.historyTags.size(); ++i) {
                if (i) out << ", ";
                out << "\"" << qmlEsc(w.historyTags[i]) << "\"";
            }
            out << "]\n";
        }
        // Q-6(2026-09-04): 列显示名平行输出（HmiHistoryView 列头用；空=显示变量名——老工程兼容）
        if (!w.historyTagTitles.isEmpty() && w.historyTagTitles.size() == w.historyTags.size()) {
            out << "    historyTitles: [";
            for (int i = 0; i < w.historyTagTitles.size(); ++i) {
                if (i) out << ", ";
                out << "\"" << qmlEsc(w.historyTagTitles[i]) << "\"";
            }
            out << "]\n";
        }
        appendProp(out, "historyDbPath", w.historyDbPath);
    }
    // P-6 Frame 视频属性（仅 W_FRAME 类型且视频模式输出——HmiFrame 默认普通模式；
    // 本地视频源 resolveVideoPath 重定位（打包 rel → 设备端 <resourceRoot>/media/<rel>），RTSP/绝对路径原样）
    if (w.type == WidgetType::Frame && w.showVideo) {
        appendProp(out, "showVideo", true);
        appendProp(out, "videoSource", resolveVideoPath(w.videoSource, resourceRoot));
        appendProp(out, "playTag", w.playTag);   // R-4: 播放控制变量（空=未绑定——HmiFrameVideo 点击直接控制）
        // S-5: 视频源列表（videoListRef 非空 → 展开 Video 列表项为 | 分隔源地址串——本地项 resolveVideoPath 重定位、RTSP 原样；
        //    项含 | 字符会被拆错——同 Image listItems 先例（上方 187-188 行注释）局限，Video 列表项（源地址）需规避含 | 的地址）
        appendProp(out, "videoListRef", w.videoListRef);
        appendProp(out, "videoIndexTag", w.videoIndexTag);
        if (!w.videoListRef.isEmpty()) {
            const ListDef* vlist = proj.ListByName(w.videoListRef, ListType::Video);   // U-2：Frame 视频消费 Video 型列表（同名跨类型不串）
            if (vlist) {
                QStringList srcs;
                for (const auto& item : vlist->items)
                    srcs << resolveVideoPath(item, resourceRoot);
                appendProp(out, "videoListItems", srcs.join(QLatin1Char('|')));
            }
        }
    }

    // 事件占位：onClick 等 → 信号处理器（联动 ActionRunner 后续循环接入）
    // E 循环: 组件已统一声明全 19 事件信号(审查修复)——任意事件输出不会 Cannot assign
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
        // H-7(M8): emitEvent 第三参 payload——窗口控件 onAck/onSelect 携带编号（HmiWindow.ackPayload/selectPayload）；
        // 其余事件 payload 空（兼容旧行为）
        QString payloadExpr = QStringLiteral("\"\"");
        if (w.type == WidgetType::Window) {
            if (ev.type == EventType::OnAck)
                payloadExpr = QStringLiteral("ackPayload");
            else if (ev.type == EventType::OnSelect)
                payloadExpr = QStringLiteral("selectPayload");
        }
        // 2026-08-30 用户 Check 修复：emitEvent 第四参 sourceScreen = 控件所在画面名——
        // runtimebus 来源画面优先精确匹配，同名控件不再连动（画面一按钮1=返回地图 不再连带触发 全局画面按钮1=StopRuntime）
        out << "        " << signalName << ": function() { if (runtimeBus) runtimeBus.emitEvent(\""
            << qmlEsc(w.objectName) << "\", " << int(ev.type) << ", " << payloadExpr
            << ", \"" << qmlEsc(screenName) << "\"); }\n";
    }
    // G-0: 控件注册/注销（ObjectManager 跨画面寻址依据；加载完成注册, 销毁注销）
    // 全局画面(overlay)控件 screenName 用所属 Template 画面名, 与 RuntimeBus 事件匹配口径一致
    out << "        Component.onCompleted: { if (objectManager) objectManager.registerObject(\""
        << qmlEsc(screenName) << "\", \"" << qmlEsc(w.objectName) << "\", this) }\n";
    out << "        Component.onDestruction: { if (objectManager) objectManager.unregisterObject(\""
        << qmlEsc(screenName) << "\", \"" << qmlEsc(w.objectName) << "\") }\n";
    out << "    }\n";
}

} // anonymous namespace

QString QmlGenerator::generateScreen(const Project& proj, const Screen& screen, const QString& resourceRoot)
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
    // R2: 画面空白背景浅灰（否则透出主壳深蓝 #0F5278；z:-1 在控件之下；世界地图特殊画面不含此背景）
    ts << "    Rectangle { anchors.fill: parent; color: \"#E8E8E8\"; z: -1 }\n";
    for (const auto& w : screen.widgets)
        generateWidget(ts, w, proj, screen.name, resourceRoot);
    ts << "}\n";
    return out;
}

QString QmlGenerator::generateWorldMap(const Project& proj, const QString& tileBasePath, const QString& backgroundImagePath)
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
    // bounds 全 0 = 未配置 → 2026-08-30 用户 Check 修复：优先用作业点/范围点包围盒（含 padding），
    // 保证地图显示到作业范围区域（用户实测：工程未配置范围时兜底成都，作业点在另一区域 → 视口错位、作业范围看不到）；
    // 无任何点才兜底成都范围（2026-08-18 用户定: 世界地图放成都市）
    double latMin = proj.worldMap.latMin, latMax = proj.worldMap.latMax;
    double lngMin = proj.worldMap.lngMin, lngMax = proj.worldMap.lngMax;
    if (!(latMin == 0 && latMax == 0 && lngMin == 0 && lngMax == 0)) {
        ts << "    latMin: " << latMin << "\n";
        ts << "    latMax: " << latMax << "\n";
        ts << "    lngMin: " << lngMin << "\n";
        ts << "    lngMax: " << lngMax << "\n";
    } else {
        ts << "    // bounds 未配置——视口由下方作业点/范围点包围盒计算（2026-08-30 修复）\n";
        ts << "    latMin: 0\n    latMax: 0\n    lngMin: 0\n    lngMax: 0\n";   // 占位，HmiWorldMap 内自适应
    }
    ts << "    zoomLevel: " << proj.worldMap.zoomLevel << "\n";
    // R3: 工程自带瓦片根目录（ZIP 工程包解压出的 tiles/）；空则组件用模拟底图
    if (!tileBasePath.isEmpty())
        ts << "    tileBasePath: \"" << qmlEsc(tileBasePath) << "\"\n";
    // N-1: 锁定视角底图（PC 拼好的单张 PNG，随工程包下发 worldmap_bg.png）；有底图时瓦片层/模拟底图隐藏
    if (!backgroundImagePath.isEmpty())
        ts << "    backgroundImage: \"" << qmlEsc(backgroundImagePath) << "\"\n";
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
    // 范围点：输出**用户实际配置的范围点**（原样坐标与顺序，不再扩展成 min/max 矩形——
    // 2026-08-30 用户 Check 修复：作业范围显示不对根因=生成器把不规则四边形扩展成包围盒矩形；
    // HmiWorldMap 按 workRange 顶点顺序画多边形（实际形状），视口自适应（computeBounds）已覆盖
    // workPoints + workRange 全部点，无需此处扩展；boundTag 透传（N-5：HmiWorldMap 范围点与作业点同路径解析 boundTag）
    if (!proj.worldMap.workRangePoints.isEmpty()) {
        ts << "    workRange: [\n";
        for (int i = 0; i < proj.worldMap.workRangePoints.size(); ++i) {
            const auto& rp = proj.worldMap.workRangePoints[i];
            ts << "        { lng: " << rp.fixedPoint.longitude << ", lat: " << rp.fixedPoint.latitude
               << ", boundTag: \"" << qmlEsc(rp.boundTag) << "\" }";
            ts << (i < proj.worldMap.workRangePoints.size() - 1 ? ",\n" : "\n");
        }
        ts << "    ]\n";
    } else {
        ts << "    workRange: []\n";
    }
    ts << "}\n";
    return out;
}

QString QmlGenerator::generateOverlay(const Project& proj, const QString& resourceRoot)
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
                generateWidget(ts, w, proj, sc.name, resourceRoot);
        }
    }
    ts << "}\n";
    return out;
}

QList<QPair<QString, QString>> QmlGenerator::generateAll(const Project& proj, const QString& resourceRoot)
{
    QList<QPair<QString, QString>> files;
    int idx = 0;
    for (const auto& sc : proj.screens) {
        if (sc.type == ScreenType::WorldMap) {
            files.append({QStringLiteral("screen_%1.qml").arg(idx), generateWorldMap(proj)});
        } else if (sc.type == ScreenType::Template) {
            // 全局画面 → overlay.qml（单独文件）
            files.append({QStringLiteral("overlay.qml"), generateOverlay(proj, resourceRoot)});
        } else {
            files.append({QStringLiteral("screen_%1.qml").arg(idx), generateScreen(proj, sc, resourceRoot)});
        }
        ++idx;
    }
    // 运行时主壳固定用 qrc:/qml/main.qml（带导航/Stop/startProject 逻辑），
    // 不生成 main.qml（避免与 qrc 版不一致造成误导）
    return files;
}

} // namespace navihmi
