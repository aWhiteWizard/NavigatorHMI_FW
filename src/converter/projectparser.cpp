/*
 * @FilePath: \NavigatorHMI_FW\src\converter\projectparser.cpp
 * @Description: .navihmi → HMIProject 运行时模型（protobuf 解析 + 字段映射）
 *
 * 数据流：PC 组态软件 compile → .navihmi（proto3 二进制，proto/navihmi.proto 契约）
 *   → parseFile/parseBytes 解析 → navihmi::Project 运行时模型
 *   → qmlgenerator 生成每画面 QML → QML 引擎加载运行
 *
 * 关键点：
 * - 契约版本校验（format_version=1）：版本不匹配拒绝加载，防旧/新产物静默错读
 * - 字段映射与 PC 端 NavihmiDto（protobuf-net）严格对齐（字段号一致）
 * - protoc 生成类命名空间 navihmi_pb（与运行时模型 navihmi 区分）
 */
#include "converter/projectparser.h"
#include "navihmi.pb.h"   // protoc 生成（命名空间 navihmi_pb）

#include <QFile>
#include <QDateTime>
#include <QHash>
#include <QDebug>

namespace navihmi {
namespace pb = ::navihmi_pb;

namespace {

QString s(const std::string& v) { return QString::fromUtf8(v.data(), int(v.size())); }

// P-2b（2026-09-02）：控件/事件/动作类型映射——default 不再静默回退（错读比跳过危险）。
// 未知类型（旧 FW 读新 PC 产物含 18/19 等）→ 返回 false + qWarning 告警，调用方跳过该控件/事件/动作，
// 工程其余部分正常加载（不因单坏控件拒整个工程；「拒绝整个工程」模式留 V1.1 降级策略可配化——v1.1-design §5.3 F1 P1）。
bool mapWidgetType(pb::WidgetType t, WidgetType& out)
{
    switch (t) {
    case pb::W_BUTTON: out = WidgetType::Button; return true;
    case pb::W_TEXT: out = WidgetType::Text; return true;
    case pb::W_LABEL: out = WidgetType::Label; return true;
    case pb::W_RECTANGLE: out = WidgetType::Rectangle; return true;
    case pb::W_IMAGE: out = WidgetType::Image; return true;
    case pb::W_NUMERIC_DISPLAY: out = WidgetType::NumericDisplay; return true;
    case pb::W_SWITCH: out = WidgetType::Switch; return true;
    case pb::W_LINE: out = WidgetType::Line; return true;
    case pb::W_CIRCLE: out = WidgetType::Circle; return true;
    case pb::W_ELLIPSE: out = WidgetType::Ellipse; return true;
    case pb::W_IO_FIELD: out = WidgetType::IoField; return true;
    case pb::W_CHECKBOX: out = WidgetType::CheckBox; return true;
    case pb::W_TEXT_LIST: out = WidgetType::TextList; return true;
    case pb::W_FRAME: out = WidgetType::Frame; return true;
    case pb::W_PROGRESS_BAR: out = WidgetType::ProgressBar; return true;
    case pb::W_DATETIME: out = WidgetType::DateTime; return true;
    case pb::W_WINDOW: out = WidgetType::Window; return true;
    case pb::W_POLYGON: out = WidgetType::Polygon; return true;
    case pb::W_TREND_VIEW: out = WidgetType::TrendView; return true;   // P-4
    case pb::W_HISTORY_VIEW: out = WidgetType::HistoryView; return true;   // P-5（枚举一次到位）
    default:
        qWarning("projectparser: 未知控件类型 %d —— 跳过该控件（旧 FW 读新工程产物？）", static_cast<int>(t));
        return false;
    }
}

bool mapEventType(pb::EventType t, EventType& out)
{
    switch (t) {
    case pb::EV_ON_CLICK: out = EventType::OnClick; return true;
    case pb::EV_ON_PRESS: out = EventType::OnPress; return true;
    case pb::EV_ON_RELEASE: out = EventType::OnRelease; return true;
    case pb::EV_ON_VALUE_CHANGE: out = EventType::OnValueChange; return true;
    case pb::EV_ON_ALARM_TRIGGER: out = EventType::OnAlarmTrigger; return true;
    case pb::EV_ON_ALARM_ACK: out = EventType::OnAlarmAck; return true;
    case pb::EV_ON_ALARM_CLEAR: out = EventType::OnAlarmClear; return true;
    case pb::EV_ON_SCREEN_LOAD: out = EventType::OnScreenLoad; return true;
    case pb::EV_ON_SCREEN_UNLOAD: out = EventType::OnScreenUnload; return true;
    case pb::EV_ON_TIMER: out = EventType::OnTimer; return true;
    case pb::EV_ON_SYSTEM_START: out = EventType::OnSystemStart; return true;
    case pb::EV_ON_SYSTEM_SHUTDOWN: out = EventType::OnSystemShutdown; return true;
    case pb::EV_ON_INPUT: out = EventType::OnInput; return true;
    case pb::EV_ON_ON: out = EventType::OnOn; return true;
    case pb::EV_ON_OFF: out = EventType::OnOff; return true;
    case pb::EV_ON_PROGRESS_COMPLETE: out = EventType::OnProgressComplete; return true;
    case pb::EV_ON_USER_CHANGED: out = EventType::OnUserChanged; return true;
    case pb::EV_ON_ACK: out = EventType::OnAck; return true;
    case pb::EV_ON_SELECT: out = EventType::OnSelect; return true;
    default:
        qWarning("projectparser: 未知事件类型 %d —— 跳过该事件", static_cast<int>(t));
        return false;
    }
}

bool mapActionType(pb::ActionType t, ActionType& out)
{
    switch (t) {
    case pb::ACT_TAG_WRITE: out = ActionType::TagWrite; return true;
    case pb::ACT_SCREEN_SWITCH: out = ActionType::ScreenSwitch; return true;
    case pb::ACT_SET_PROPERTY: out = ActionType::SetProperty; return true;
    case pb::ACT_RUN_COMMAND: out = ActionType::RunCommand; return true;
    case pb::ACT_SHOW_POPUP: out = ActionType::ShowPopup; return true;
    case pb::ACT_SEND_NOTIFICATION: out = ActionType::SendNotification; return true;
    case pb::ACT_SCREEN_PREV: out = ActionType::ScreenPrev; return true;
    case pb::ACT_SCREEN_NEXT: out = ActionType::ScreenNext; return true;
    case pb::ACT_TAG_ADD: out = ActionType::TagAdd; return true;
    case pb::ACT_TAG_SUBTRACT: out = ActionType::TagSubtract; return true;
    case pb::ACT_TAG_TOGGLE: out = ActionType::TagToggle; return true;
    case pb::ACT_SET_BIT: out = ActionType::SetBit; return true;
    case pb::ACT_RESET_BIT: out = ActionType::ResetBit; return true;
    case pb::ACT_SET_DATETIME: out = ActionType::SetDatetime; return true;
    case pb::ACT_GET_DATETIME: out = ActionType::GetDatetime; return true;
    case pb::ACT_ACKNOWLEDGE_ALARM: out = ActionType::AcknowledgeAlarm; return true;
    case pb::ACT_SET_SYSTEM_TIME: out = ActionType::SetSystemTime; return true;
    case pb::ACT_STOP_RUNTIME: out = ActionType::StopRuntime; return true;
    case pb::ACT_TAG_STEP: out = ActionType::TagStep; return true;   // S-7 变量循环步进
    default:
        qWarning("projectparser: 未知动作类型 %d —— 跳过该动作", static_cast<int>(t));
        return false;
    }
}

// P-2b：返回 bool 表示控件类型是否已知。已知类型 → 下方字段拷贝全量执行（各类型共用扁平字段，无类型专属分流）；
// 未知类型（false）→ 早退，整控件（含其事件/动作）跳过——Widget 无类型专属语义，跳过无副作用（审查 🟡 注释措辞澄清）。
bool mapWidget(const pb::Widget& p, Widget& w)
{
    if (!mapWidgetType(p.type(), w.type))
        return false;
    w.x = p.x(); w.y = p.y(); w.width = p.width(); w.height = p.height();
    w.objectName = s(p.object_name());
    w.boundTag = s(p.bound_tag());
    w.text = s(p.text());
    w.content = s(p.content());
    w.hAlign = s(p.h_align());
    w.fontFamily = s(p.font_family());
    w.fontSize = p.font_size();
    w.fontWeight = s(p.font_weight());
    w.fontStyle = s(p.font_style());
    w.textDecoration = s(p.text_decoration());
    w.textColor = s(p.text_color());
    w.fillColor = s(p.fill_color());
    w.strokeColor = s(p.stroke_color());
    w.strokeThickness = p.stroke_thickness();
    w.imagePath = s(p.image_path());
    w.stretchMode = s(p.stretch_mode());
    w.listRef = s(p.list_ref());
    w.defaultIndex = p.default_index();
    w.value = p.value(); w.min = p.min(); w.max = p.max();
    w.fillStyle = s(p.fill_style());
    w.isOn = p.is_on();
    w.labelOn = s(p.on_text());
    w.labelOff = s(p.off_text());
    w.isChecked = p.is_checked();
    w.isReadOnly = p.is_read_only();
    w.x2 = p.x2(); w.y2 = p.y2();
    w.title = s(p.title());
    w.dtText = s(p.dt_text());
    w.dtFormat = s(p.dt_format());
    w.windowType = static_cast<WindowType>(p.window_type());
    w.winTitle = s(p.win_title());
    w.showTitleBar = p.show_title_bar();
    w.showHistory = p.show_history();
    w.selectedTag = s(p.selected_tag());
    w.cardWidth = p.card_width();
    w.cardHeight = p.card_height();
    w.showUserName = p.show_user_name();
    w.showRole = p.show_role();
    w.showMode = p.show_mode();
    w.cardShowNumber = p.card_show_number();
    w.cardShowStatus = p.card_show_status();
    w.cardShowLocation = p.card_show_location();
    w.boundDevice = s(p.bound_device());
    for (const auto& rs : p.robot_slots()) {
        QHash<QString, QString> slot;
        slot["id"] = s(rs.id_tag());
        slot["status"] = s(rs.status_tag());
        slot["location"] = s(rs.location_tag());
        slot["detail"] = s(rs.detail_tag());
        slot["oper"] = s(rs.oper_tag());
        w.robotSlots.append(slot);
    }
    for (const auto& pt : p.points()) {
        w.points.append(QPointF(pt.x(), pt.y()));
    }
    // P-4 趋势图字段（65-72）
    w.trendMode = p.trend_mode();
    w.trendTagA = s(p.trend_tag_a());
    w.trendTagB = s(p.trend_tag_b());
    w.sampleIntervalMs = p.sample_interval_ms();
    w.timeWindowSeconds = p.time_window_seconds();
    w.lineColor = s(p.line_color());
    w.lineWidth = p.line_width();
    w.refreshRateMs = p.refresh_rate_ms();
    // P-5 历史记录字段（73-74）+ Window DisplayMode（54）
    for (const auto& tag : p.history_tags())
        w.historyTags.append(s(tag));
    for (const auto& t : p.history_tag_titles())   // Q-6: 列显示名（空=老工程，QML 回退变量名）
        w.historyTagTitles.append(s(t));
    w.historyDbPath = s(p.history_db_path());
    w.displayMode = p.display_mode();
    // P-6 Frame 视频字段（75-76）+ R-4 播放控制（78）+ S-5 视频源列表（79-80）
    w.showVideo = p.show_video();
    w.videoSource = s(p.video_source());
    w.playTag = s(p.play_tag());   // R-4: 播放控制变量（空=未绑定）
    w.videoListRef = s(p.video_list_ref());   // S-5: 视频源列表名（空=未选）
    w.videoIndexTag = s(p.video_index_tag());   // S-5: 视频源选择变量（空=未绑）
    // 事件（未知事件类型 → 跳过该事件；未知动作类型 → 跳过该动作——不静默回退默认）
    for (const auto& pe : p.events()) {
        WidgetEvent we;
        if (!mapEventType(pe.type(), we.type))
            continue;
        we.condition = s(pe.condition());
        we.priority = pe.priority();   // V-3c：事件优先级（proto 缺省 0=Normal）
        for (const auto& pa : pe.actions()) {
            EventAction ea;
            if (!mapActionType(pa.type(), ea.type))
                continue;
            for (const auto& kv : pa.parameters()) {
                ea.parameters.insert(s(kv.first), s(kv.second));
            }
            we.actions.append(ea);
        }
        w.events.append(we);
    }
    return true;
}

} // anonymous namespace

bool ProjectParser::parseBytes(const QByteArray& data, Project& out)
{
    pb::HMIProject pb;
    if (!pb.ParseFromArray(data.constData(), data.size()))
        return false;

    // 契约版本校验：format_version=1 是当前版本，不匹配拒绝加载（防旧/新产物静默错读）
    if (pb.format_version() != 1)
        return false;

    out.name = s(pb.name());
    out.createTime = QDateTime::fromSecsSinceEpoch(pb.create_time());
    out.lastModifiedTime = QDateTime::fromSecsSinceEpoch(pb.last_modified_time());
    out.version = s(pb.version());
    out.formatVersion = pb.format_version();
    out.deviceWidth = pb.device_width();
    out.deviceHeight = pb.device_height();
    out.deviceModel = s(pb.device_model());   // 工程目标设备型号（proto 22；设备身份由设备自身配置决定，与工程无关——2026-08-30 用户 Check 指正）
    out.showNavigationBar = pb.show_navigation_bar();
    out.enableVnc = pb.enable_vnc();
    out.navigationPosition = pb.navigation_position() == pb::NAV_TOP ? NavPosition::Top : NavPosition::Bottom;
    out.startScreen = s(pb.start_screen());

    for (const auto& ps : pb.screens()) {
        Screen sc;
        sc.name = s(ps.name());
        sc.width = ps.width();
        sc.height = ps.height();
        sc.type = ps.type() == pb::SCREEN_TEMPLATE ? ScreenType::Template
                 : ps.type() == pb::SCREEN_WORLD_MAP ? ScreenType::WorldMap
                 : ScreenType::Custom;
        sc.isGlobal = ps.is_global();
        sc.showInNav = ps.show_in_nav();
        sc.navOrder = ps.nav_order();
        for (const auto& pw : ps.widgets()) {
            Widget w;
            if (mapWidget(pw, w))   // P-2b：未知控件类型 → 跳过该控件（qWarning 已打），工程其余正常
                sc.widgets.append(w);
        }
        out.screens.append(sc);
    }

    for (const auto& pt : pb.tags()) {
        Tag t;
        t.name = s(pt.name());
        t.dataType = static_cast<TagDataType>(pt.data_type());
        t.unit = s(pt.unit());
        t.source = s(pt.source());
        t.scanIntervalMs = pt.scan_interval_ms();
        t.deadband = pt.deadband();
        t.description = s(pt.description());
        t.baseValue = s(pt.base_value());
        t.deviceName = s(pt.device_name());
        out.tags.append(t);
    }

    for (const auto& pa : pb.alarms()) {
        AlarmRule a;
        a.name = s(pa.name());
        a.tagName = s(pa.tag_name());
        a.type = static_cast<AlarmType>(pa.type());
        a.threshold = pa.threshold();
        a.deadband = pa.deadband();
        a.delayMs = pa.delay_ms();
        a.level = static_cast<Severity>(pa.level());
        a.message = s(pa.message());
        a.triggerMode = static_cast<AlarmTriggerMode>(pa.trigger_mode());
        a.category = static_cast<AlarmCategory>(pa.category());
        a.priority = pa.priority();
        a.ackRequired = pa.ack_required();
        a.ackGroup = s(pa.ack_group());
        a.colorOverride = s(pa.color_override());
        out.alarms.append(a);
    }

    for (const auto& pd : pb.devices()) {
        DeviceConfig dc;
        dc.name = s(pd.name());
        dc.protocol = static_cast<ProtocolType>(pd.protocol());
        dc.connectionInfo = s(pd.connection_info());
        out.devices.append(dc);
    }

    for (const auto& pl : pb.lists()) {
        ListDef ld;
        ld.name = s(pl.name());
        ld.type = static_cast<ListType>(pl.type());
        for (const auto& item : pl.items())
            ld.items.append(s(item));
        out.lists.append(ld);
    }

    // 世界地图
    if (pb.has_world_map()) {
        const auto& wm = pb.world_map();
        out.worldMap.latMin = wm.lat_min();
        out.worldMap.latMax = wm.lat_max();
        out.worldMap.lngMin = wm.lng_min();
        out.worldMap.lngMax = wm.lng_max();
        out.worldMap.tileSource = s(wm.tile_source());
        out.worldMap.zoomLevel = wm.zoom_level();
        out.worldMap.showGlobalOverlay = wm.show_global_overlay();
        out.worldMap.viewLocked = wm.view_locked();
        for (const auto& wp : wm.work_points()) {
            MapWorkPoint mwp;
            mwp.name = s(wp.name());
            mwp.boundTag = s(wp.bound_tag());
            if (wp.has_fixed_point()) {
                mwp.fixedPoint.longitude = wp.fixed_point().longitude();
                mwp.fixedPoint.latitude = wp.fixed_point().latitude();
            }
            out.worldMap.workPoints.append(mwp);
        }
        for (const auto& rp : wm.work_range_points()) {
            WorkRangePoint wrp;
            wrp.boundTag = s(rp.bound_tag());
            if (rp.has_fixed_point()) {
                wrp.fixedPoint.longitude = rp.fixed_point().longitude();
                wrp.fixedPoint.latitude = rp.fixed_point().latitude();
            }
            out.worldMap.workRangePoints.append(wrp);
        }
        for (const auto& pe : wm.events()) {
            WidgetEvent we;
            if (!mapEventType(pe.type(), we.type))
                continue;   // P-2b：未知事件类型 → 跳过该事件
            we.condition = s(pe.condition());
            we.priority = pe.priority();   // V-3c：事件优先级
            for (const auto& pa : pe.actions()) {
                EventAction ea;
                if (!mapActionType(pa.type(), ea.type))
                    continue;   // P-2b：未知动作类型 → 跳过该动作
                for (const auto& kv : pa.parameters())
                    ea.parameters.insert(s(kv.first), s(kv.second));
                we.actions.append(ea);
            }
            out.worldMap.events.append(we);
        }
    }

    // 用户系统
    for (const auto& pu : pb.users()) {
        UserAccount ua;
        ua.userName = s(pu.user_name());
        ua.passwordHash = s(pu.password_hash());
        ua.groupName = s(pu.group_name());
        ua.mustChangePassword = pu.must_change_password();
        out.users.append(ua);
    }
    for (const auto& pg : pb.groups()) {
        UserGroup ug;
        ug.name = s(pg.name());
        for (int perm : pg.permissions())
            ug.permissions.append(perm);
        out.groups.append(ug);
    }
    if (pb.has_security()) {
        const auto& sec = pb.security();
        out.security.minPasswordLength = sec.min_password_length();
        out.security.requireDigit = sec.require_digit();
        out.security.requireLetter = sec.require_letter();
        out.security.requireUpperLower = sec.require_upper_lower();
        out.security.requireSpecial = sec.require_special();
        out.security.passwordMaxAgeDays = sec.password_max_age_days();
        out.security.failedLoginLockout = sec.failed_login_lockout();
        out.security.lockMinutes = sec.lock_minutes();
    }

    return true;
}

bool ProjectParser::parseFile(const QString& path, Project& out)
{
    // 读取 .navihmi 文件 → parseBytes（纯二进制 proto；ZIP 工程包的解压由 main.cpp resolveProjectPackage 处理，
    // 此处只收解压后的 app.navihmi 或纯单文件）
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    f.close();
    return parseBytes(data, out);
}

} // namespace navihmi
