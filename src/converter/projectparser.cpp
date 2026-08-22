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

namespace navihmi {
namespace pb = ::navihmi_pb;

namespace {

QString s(const std::string& v) { return QString::fromUtf8(v.data(), int(v.size())); }

WidgetType mapWidgetType(pb::WidgetType t)
{
    switch (t) {
    case pb::W_BUTTON: return WidgetType::Button;
    case pb::W_TEXT: return WidgetType::Text;
    case pb::W_LABEL: return WidgetType::Label;
    case pb::W_RECTANGLE: return WidgetType::Rectangle;
    case pb::W_IMAGE: return WidgetType::Image;
    case pb::W_NUMERIC_DISPLAY: return WidgetType::NumericDisplay;
    case pb::W_SWITCH: return WidgetType::Switch;
    case pb::W_LINE: return WidgetType::Line;
    case pb::W_CIRCLE: return WidgetType::Circle;
    case pb::W_ELLIPSE: return WidgetType::Ellipse;
    case pb::W_IO_FIELD: return WidgetType::IoField;
    case pb::W_CHECKBOX: return WidgetType::CheckBox;
    case pb::W_TEXT_LIST: return WidgetType::TextList;
    case pb::W_FRAME: return WidgetType::Frame;
    case pb::W_PROGRESS_BAR: return WidgetType::ProgressBar;
    case pb::W_DATETIME: return WidgetType::DateTime;
    case pb::W_WINDOW: return WidgetType::Window;
    case pb::W_POLYGON: return WidgetType::Polygon;
    default: return WidgetType::Rectangle;
    }
}

EventType mapEventType(pb::EventType t)
{
    switch (t) {
    case pb::EV_ON_CLICK: return EventType::OnClick;
    case pb::EV_ON_PRESS: return EventType::OnPress;
    case pb::EV_ON_RELEASE: return EventType::OnRelease;
    case pb::EV_ON_VALUE_CHANGE: return EventType::OnValueChange;
    case pb::EV_ON_ALARM_TRIGGER: return EventType::OnAlarmTrigger;
    case pb::EV_ON_ALARM_ACK: return EventType::OnAlarmAck;
    case pb::EV_ON_ALARM_CLEAR: return EventType::OnAlarmClear;
    case pb::EV_ON_SCREEN_LOAD: return EventType::OnScreenLoad;
    case pb::EV_ON_SCREEN_UNLOAD: return EventType::OnScreenUnload;
    case pb::EV_ON_TIMER: return EventType::OnTimer;
    case pb::EV_ON_SYSTEM_START: return EventType::OnSystemStart;
    case pb::EV_ON_SYSTEM_SHUTDOWN: return EventType::OnSystemShutdown;
    case pb::EV_ON_INPUT: return EventType::OnInput;
    case pb::EV_ON_ON: return EventType::OnOn;
    case pb::EV_ON_OFF: return EventType::OnOff;
    case pb::EV_ON_PROGRESS_COMPLETE: return EventType::OnProgressComplete;
    case pb::EV_ON_USER_CHANGED: return EventType::OnUserChanged;
    case pb::EV_ON_ACK: return EventType::OnAck;
    case pb::EV_ON_SELECT: return EventType::OnSelect;
    default: return EventType::OnClick;
    }
}

ActionType mapActionType(pb::ActionType t)
{
    switch (t) {
    case pb::ACT_TAG_WRITE: return ActionType::TagWrite;
    case pb::ACT_SCREEN_SWITCH: return ActionType::ScreenSwitch;
    case pb::ACT_SET_PROPERTY: return ActionType::SetProperty;
    case pb::ACT_RUN_COMMAND: return ActionType::RunCommand;
    case pb::ACT_SHOW_POPUP: return ActionType::ShowPopup;
    case pb::ACT_SEND_NOTIFICATION: return ActionType::SendNotification;
    case pb::ACT_SCREEN_PREV: return ActionType::ScreenPrev;
    case pb::ACT_SCREEN_NEXT: return ActionType::ScreenNext;
    case pb::ACT_TAG_ADD: return ActionType::TagAdd;
    case pb::ACT_TAG_SUBTRACT: return ActionType::TagSubtract;
    case pb::ACT_TAG_TOGGLE: return ActionType::TagToggle;
    case pb::ACT_SET_BIT: return ActionType::SetBit;
    case pb::ACT_RESET_BIT: return ActionType::ResetBit;
    case pb::ACT_SET_DATETIME: return ActionType::SetDatetime;
    case pb::ACT_GET_DATETIME: return ActionType::GetDatetime;
    case pb::ACT_ACKNOWLEDGE_ALARM: return ActionType::AcknowledgeAlarm;
    case pb::ACT_SET_SYSTEM_TIME: return ActionType::SetSystemTime;
    default: return ActionType::TagWrite;
    }
}

void mapWidget(const pb::Widget& p, Widget& w)
{
    w.x = p.x(); w.y = p.y(); w.width = p.width(); w.height = p.height();
    w.objectName = s(p.object_name());
    w.boundTag = s(p.bound_tag());
    w.type = mapWidgetType(p.type());
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
    // 事件
    for (const auto& pe : p.events()) {
        WidgetEvent we;
        we.type = mapEventType(pe.type());
        we.condition = s(pe.condition());
        for (const auto& pa : pe.actions()) {
            EventAction ea;
            ea.type = mapActionType(pa.type());
            for (const auto& kv : pa.parameters()) {
                ea.parameters.insert(s(kv.first), s(kv.second));
            }
            we.actions.append(ea);
        }
        w.events.append(we);
    }
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
            mapWidget(pw, w);
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
            we.type = mapEventType(pe.type());
            we.condition = s(pe.condition());
            for (const auto& pa : pe.actions()) {
                EventAction ea;
                ea.type = mapActionType(pa.type());
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
