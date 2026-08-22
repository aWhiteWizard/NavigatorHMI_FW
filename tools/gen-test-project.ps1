# ============================================================
# B-4 测试工程生成脚本 v3 (UTF-8 BOM; 2026-08-20 修订)
# 覆盖: 15 基础控件 + 3 窗口模板 + 19 事件 + 17 动作 + 世界地图(成都)
#       + 全局画面(Stop 唯一名 btn_stop) + 画面间双向跳转
# 修订: ① 全局 Stop 改名 btn_stop(防与画面A button_1 同名连动)
#       ② 返回地图按钮 y=40→90(避开全局 Stop 900,10~50 重叠)
#       ③ 画面A 新增「切到画面B」按钮(右下角, 显眼)
# ============================================================
$ErrorActionPreference = "Continue"

$CLI = "D:\workspace\code\navigator_hmi\src\NaviHmiCLI\bin\Debug\net8.0\navihmi.exe"
$DIR = "D:\workspace\test_project"
$PROJ = "B4_测试工程.hmiproj"
$P = "-p $PROJ"

# D+6: 生成新工程前彻底删除旧工程目录（不并存, 不做兼容）
if (Test-Path $DIR) {
    Remove-Item $DIR -Recurse -Force
}
New-Item -ItemType Directory -Path $DIR -Force | Out-Null
Set-Location $DIR

# 执行 CLI (参数数组展开调用)
function Exec([string[]]$argList) {
    & $CLI @argList 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { Write-Host "!! 失败: $($argList -join ' ')" -ForegroundColor Red }
}

# ---------- 1. 工程 ----------
Write-Host "=== 创建工程 (1024x600) ===" -ForegroundColor Cyan
Exec @("create-project", "--name", "B4_测试工程", "--path", ".", "--width", "1024", "--height", "600")

Write-Host "=== 创建画面A/B ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "create-screen", "--name", "画面A", "--type", "custom")
Exec @("-p", $PROJ, "create-screen", "--name", "画面B", "--type", "custom")

# ---------- 2. 变量 ----------
Write-Host "=== 变量 ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "create-tag", "--name", "温度", "--type", "FLOAT", "--base-value", "25.5")
Exec @("-p", $PROJ, "create-tag", "--name", "压力", "--type", "FLOAT", "--base-value", "1.2")
Exec @("-p", $PROJ, "create-tag", "--name", "开关1", "--type", "BOOL", "--base-value", "false")
Exec @("-p", $PROJ, "create-tag", "--name", "数值1", "--type", "INT16", "--base-value", "100")
Exec @("-p", $PROJ, "create-tag", "--name", "GPS_1", "--type", "GPS", "--base-value", "104.0657,30.6570")
Exec @("-p", $PROJ, "create-tag", "--name", "GPS_2", "--type", "GPS", "--base-value", "104.1423,30.6294")
Exec @("-p", $PROJ, "create-tag", "--name", "GPS_3", "--type", "GPS", "--base-value", "103.9471,30.5785")
Exec @("-p", $PROJ, "create-tag", "--name", "GPS_4", "--type", "GPS", "--base-value", "104.0727,30.7000")
Exec @("-p", $PROJ, "create-tag", "--name", "名称", "--type", "STRING", "--base-value", "设备A")

# ---------- 3. 报警 ----------
Write-Host "=== 报警 ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "create-alarm", "--name", "高温报警", "--tag", "温度", "--type", "High", "--threshold", "80", "--severity", "Important", "--message", "温度超限")
Exec @("-p", $PROJ, "create-alarm", "--name", "低压报警", "--tag", "压力", "--type", "Low", "--threshold", "0.5", "--severity", "Warning")

# ---------- 3.5 列表（D-3: 模式列表供 textlist 引用）----------
Write-Host "=== 列表 ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "create-list", "--name", "模式列表", "--type", "text", "--items", "模式1|模式2|模式3|模式4")

# ---------- 4. 画面A: 15 控件 + 3 窗口模板 ----------
Write-Host "=== 画面A 控件 ===" -ForegroundColor Cyan
# button_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "button", "--x", "40", "--y", "40", "--width", "120", "--height", "40")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "button_1", "--key", "text", "--value", "启动")
# text_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "text", "--x", "40", "--y", "100", "--width", "120", "--height", "30")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "text_2", "--key", "content", "--value", "文本控件")
# label_1 (绑温度)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "label", "--x", "40", "--y", "150", "--width", "120", "--height", "30")
# rectangle_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "rectangle", "--x", "40", "--y", "200", "--width", "120", "--height", "60")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "rectangle_4", "--key", "fillColor", "--value", "#3366CC")
# image_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "image", "--x", "40", "--y", "280", "--width", "120", "--height", "60")
# numeric_1 (绑温度)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "numeric", "--x", "200", "--y", "40", "--width", "120", "--height", "40", "--bound-tag", "温度")
# switch_1 (绑开关1)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "switch", "--x", "200", "--y", "100", "--width", "120", "--height", "40", "--bound-tag", "开关1")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "switch_7", "--key", "onText", "--value", "开")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "switch_7", "--key", "offText", "--value", "关")
# line_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "line", "--x", "200", "--y", "160", "--width", "120", "--height", "40")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "line_8", "--key", "x2", "--value", "120")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "line_8", "--key", "y2", "--value", "40")
# circle_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "circle", "--x", "200", "--y", "220", "--width", "60", "--height", "60")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "circle_9", "--key", "fillColor", "--value", "#FF6633")
# ellipse_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "ellipse", "--x", "200", "--y", "300", "--width", "90", "--height", "50")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "ellipse_10", "--key", "fillColor", "--value", "#66CC33")
# iofield_1 (绑数值1)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "iofield", "--x", "360", "--y", "40", "--width", "120", "--height", "40", "--bound-tag", "数值1")
# checkbox_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "checkbox", "--x", "360", "--y", "100", "--width", "120", "--height", "30")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "checkbox_12", "--key", "text", "--value", "启用")
# textlist_1 (D-3: 配 list 引用, 有下拉窗口; D+-4: 控件名=全局序数 textlist_13)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "textlist", "--x", "360", "--y", "150", "--width", "120", "--height", "60")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "textlist_13", "--key", "listRef", "--value", "模式列表")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "textlist_13", "--key", "defaultIndex", "--value", "0")
# iofield_2 (D-3: 绑开关1; D+-5: 控件名=iofield_14, 移到 (200,360) 避开 frame_15 覆盖; 2026-08-22: BOOL→双按钮)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "iofield", "--x", "200", "--y", "360", "--width", "120", "--height", "40", "--bound-tag", "开关1")
# 2026-08-22: 测浮点/坐标/字符串 iofield 键盘 (FLOAT→数字+小数点, GPS→坐标, STRING→全键盘)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "iofield", "--x", "40", "--y", "360", "--width", "120", "--height", "40", "--bound-tag", "温度")
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "iofield", "--x", "40", "--y", "420", "--width", "160", "--height", "40", "--bound-tag", "GPS_1")
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "iofield", "--x", "200", "--y", "420", "--width", "160", "--height", "40", "--bound-tag", "名称")
# frame_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "frame", "--x", "360", "--y", "230", "--width", "160", "--height", "100")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "frame_15", "--key", "title", "--value", "框架")
# progressbar_1
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "progressbar", "--x", "360", "--y", "350", "--width", "160", "--height", "30")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "progressbar_16", "--key", "value", "--value", "60")
# 3 窗口模板
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "window", "--x", "560", "--y", "40", "--width", "200", "--height", "120", "--window-type", "userview")
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "window", "--x", "560", "--y", "180", "--width", "200", "--height", "120", "--window-type", "alarmview")
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "window", "--x", "560", "--y", "320", "--width", "200", "--height", "120", "--window-type", "robotlist")
# 返回地图按钮 button_19 (y=90 避开全局 Stop 900,10~50)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "button", "--x", "900", "--y", "90", "--width", "100", "--height", "40")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "button_20", "--key", "text", "--value", "返回地图")
# 切到画面B 按钮 button_21 (右下角显眼)
Exec @("-p", $PROJ, "add-widget", "--screen", "画面A", "--type", "button", "--x", "700", "--y", "520", "--width", "140", "--height", "40")
Exec @("-p", $PROJ, "set-property", "--screen", "画面A", "--widget", "button_21", "--key", "text", "--value", "切到画面B")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "button_21", "--event", "onClick", "--action", "screen_switch", "--params", "target_screen=画面B")

# ---------- 5. 画面A: 事件绑定 (19 事件) ----------
Write-Host "=== 画面A 事件 ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "tag_write", "--params", "tag_name=开关1,value=true")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "button_1", "--event", "onPress", "--action", "run_command", "--params", "command=list")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "button_1", "--event", "onRelease", "--action", "set_property", "--params", "widget=button_1,key=text,value=已点")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "switch_7", "--event", "onValueChange", "--action", "tag_write", "--params", "tag_name=开关1")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "numeric_6", "--event", "onAlarmTrigger", "--action", "show_popup", "--params", "title=报警")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "numeric_6", "--event", "onAlarmAck", "--action", "acknowledge_alarm")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "numeric_6", "--event", "onAlarmClear", "--action", "send_notification", "--params", "topic=alarm,message=恢复")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "label_3", "--event", "onScreenLoad", "--action", "tag_add", "--params", "tag_name=数值1,value=1")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "label_3", "--event", "onScreenUnload", "--action", "tag_subtract", "--params", "tag_name=数值1,value=1")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "checkbox_12", "--event", "onTimer", "--action", "tag_toggle", "--params", "tag_name=开关1,interval=5000")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "button_1", "--event", "onSystemStart", "--action", "tag_write", "--params", "tag_name=数值1,value=0")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "iofield_11", "--event", "onInput", "--action", "tag_write", "--params", "tag_name=数值1")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "switch_7", "--event", "onOn", "--action", "set_bit", "--params", "tag_name=开关1")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "switch_7", "--event", "onOff", "--action", "reset_bit", "--params", "tag_name=开关1")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "progressbar_16", "--event", "onProgressComplete", "--action", "send_notification", "--params", "topic=progress,message=完成")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "circle_9", "--event", "onClick", "--action", "screen_switch", "--params", "target_screen=画面B")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "ellipse_10", "--event", "onClick", "--action", "show_popup", "--params", "title=提示,message=椭圆点击")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面A", "--widget", "button_20", "--event", "onClick", "--action", "screen_switch", "--params", "target_screen=世界地图")

# 17 动作全覆盖 (onClick 累积)
Write-Host "=== 画面A 动作全覆盖 ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "screen_next")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "screen_prev")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "tag_add", "--params", "tag_name=数值1,value=10")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "tag_subtract", "--params", "tag_name=数值1,value=5")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "tag_toggle", "--params", "tag_name=开关1")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "set_bit", "--params", "tag_name=开关1")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "reset_bit", "--params", "tag_name=开关1")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "set_datetime", "--params", "tag_name=时间")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "get_datetime", "--params", "tag_name=时间")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "set_system_time", "--params", "value=2026-01-01 00:00:00")
Exec @("-p", $PROJ, "add-event", "--screen", "画面A", "--widget", "button_1", "--event", "onClick", "--action", "acknowledge_alarm")

# ---------- 6. 画面B: 返回地图 + 系统关机事件 ----------
Write-Host "=== 画面B ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "add-widget", "--screen", "画面B", "--type", "text", "--x", "40", "--y", "40", "--width", "200", "--height", "30")
Exec @("-p", $PROJ, "add-widget", "--screen", "画面B", "--type", "button", "--x", "900", "--y", "90", "--width", "100", "--height", "40")
Exec @("-p", $PROJ, "set-property", "--screen", "画面B", "--widget", "button_2", "--key", "text", "--value", "返回地图")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面B", "--widget", "button_2", "--event", "onClick", "--action", "screen_switch", "--params", "target_screen=世界地图")
Exec @("-p", $PROJ, "bind-event", "--screen", "画面B", "--widget", "button_2", "--event", "onSystemShutdown", "--action", "send_notification", "--params", "topic=shutdown,message=关机")

# ---------- 7. 世界地图: 成都作业点/范围/配置/跳转 ----------
Write-Host "=== 世界地图 (成都) ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "add-work-point", "--screen", "世界地图", "--name", "天府广场", "--lng-lat", "104.0657,30.6570")
Exec @("-p", $PROJ, "add-work-point", "--screen", "世界地图", "--name", "成都东站", "--bound-tag", "GPS_2")
Exec @("-p", $PROJ, "add-work-point", "--screen", "世界地图", "--name", "双流机场", "--lng-lat", "103.9471,30.5785")
Exec @("-p", $PROJ, "add-work-point", "--screen", "世界地图", "--name", "成都北站", "--lng-lat", "104.0727,30.7000")
Exec @("-p", $PROJ, "add-work-range-point", "--screen", "世界地图", "--lng-lat", "104.05,30.68")
Exec @("-p", $PROJ, "add-work-range-point", "--screen", "世界地图", "--lng-lat", "104.10,30.68")
Exec @("-p", $PROJ, "add-work-range-point", "--screen", "世界地图", "--lng-lat", "104.10,30.60")
Exec @("-p", $PROJ, "add-work-range-point", "--screen", "世界地图", "--lng-lat", "104.05,30.60")
Exec @("-p", $PROJ, "update-world-map", "--screen", "世界地图", "--tile-source", "offline", "--zoom-level", "12", "--show-global-overlay", "true")
Exec @("-p", $PROJ, "add-event", "--screen", "世界地图", "--event", "onClick", "--action", "screen_switch", "--params", "target_screen=画面A")

# ---------- 8. 全局画面: Stop Runtime（唯一名 btn_stop）----------
Write-Host "=== 全局画面 Stop ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "add-widget", "--screen", "全局画面", "--type", "button", "--x", "900", "--y", "10", "--width", "110", "--height", "40")
Exec @("-p", $PROJ, "set-property", "--screen", "全局画面", "--widget", "button_1", "--key", "text", "--value", "Stop Runtime")
Exec @("-p", $PROJ, "set-property", "--screen", "全局画面", "--widget", "button_1", "--key", "objectName", "--value", "btn_stop")
Exec @("-p", $PROJ, "bind-event", "--screen", "全局画面", "--widget", "btn_stop", "--event", "onClick", "--action", "run_command", "--params", "command=stop_runtime")

# ---------- 9. 保存 + 编译 ----------
Write-Host "=== 保存 + 编译 ===" -ForegroundColor Cyan
Exec @("-p", $PROJ, "save-project")
Exec @("-p", $PROJ, "compile", "--output", "app.navihmi")

Set-Location "D:\workspace\code\navigator_hmi"
Write-Host "`n=== 完成: $DIR\$PROJ + app.navihmi ===" -ForegroundColor Green
