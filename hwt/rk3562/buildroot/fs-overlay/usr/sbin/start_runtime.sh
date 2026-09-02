#!/bin/sh
# NavigatorHMI FW 统一启动入口（O 轮批 D D-1，2026-09）
# 职责链（用户 2026-08-30 N+29 定稿）：设全部环境变量（Qt 插件/QML 路径/触摸）→ 环境准备（残留清理/qmlcache/
# 单实例预检）→ 确认环境干净 → 运行 navigatorhmi-fw --project <默认工程>。
# 设计要点：
#   * S99qt-test 守护循环调用本脚本（绝对路径，不依赖 PATH）——环境集中一处，避免启动脚本与守护各设一遍漂移；
#   * 单实例预检（D-2）：已有 navigatorhmi-fw 存活 → 提示退出（防双实例 EGL/DRM 冲突 + HTTP 80 冲突，N+28 先例）；
#   * 工程选择：/mnt/user/userdata/app.navihmi 存在 → --project 打开（自动打开上次工程，用户 2026-08-30 期望）；
#     不存在 → 无参启动（空工程导航模式，进程不退出——B6-8 语义）；
#   * 期望固件校验（D-2 版本监控）：/etc/navigatorhmi/expected-md5（OTA app 安装成功后由 otaupdater 写入）——
#     本脚本启动前比对 /usr/bin/navigatorhmi-fw 实际 md5，不符 → **仅告警不回滚**（见下方 L29-33 修复说明；
#     版本比对用 md5 而非字符串：二进制内版本字符串不可靠——strings 会命中 Qt 内部 1.3.x 等）。

export QT_QPA_PLATFORM=eglfs
export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms
export QT_PLUGIN_PATH=/usr/plugins
export QML_IMPORT_PATH=/usr/qml
export QML2_IMPORT_PATH=/usr/qml
export QT_QPA_EGLFS_ALWAYS_SET_MODE=1

FW=/usr/bin/navigatorhmi-fw
PROJECT=/mnt/user/userdata/app.navihmi
EXPECTED_MD5=/etc/navigatorhmi/expected-md5
LOG=/tmp/navihmi.log

# ── 环境准备：qmlcache 残留清理（缓存与二进制版本强相关——OTA 换版本后旧缓存致 QML 行为错乱，K 循环先例）──
rm -rf /tmp/navihmi-gen/qmlcache 2>/dev/null

# ── 期望固件校验（D-2 版本监控）──
# 修复（2026-09 上板实测发现）：md5 不符**不回滚**——OTA 升级后首次启动新二进制 md5 必然 ≠ 旧 expected-md5
# （expected-md5 由 otaupdater 安装成功后更新为新版；升级前旧值仍在），若不符即回滚 .bak 会**误伤正常升级**
# （实测 v1.1.3 OTA 后新版被 start_runtime 回滚成旧版）。md5 不符仅告警记录；真正的失败回滚走
# bootFail 连续失败机制（main.cpp 前置检查 bootFailCount>=N → restoreFromUserdataBackup / .bak 恢复）。
if [ -f "${EXPECTED_MD5}" ]; then
    exp_md5=$(cat "${EXPECTED_MD5}" | tr -d '[:space:]')
    if [ -n "${exp_md5}" ] && [ -x "${FW}" ]; then
        cur_md5=$(md5sum "${FW}" | awk '{print $1}')
        if [ "${cur_md5}" != "${exp_md5}" ]; then
            echo "[$(date +%H:%M:%S)] start_runtime: FW md5 与期望不符 (${cur_md5} != ${exp_md5})——"
                 "可能是 OTA 升级后首次启动（otaupdater 将更新期望 md5）；不回滚，告警记录" >> "${LOG}"
        fi
    fi
fi

# ── 单实例预检（D-2：防双实例——N+28 .bak.off 双启动 / 手动启动残留 / 守护与独立实例并存）──
running=$(pgrep -x navigatorhmi-fw | wc -l)
if [ "${running}" -gt 0 ]; then
    echo "[$(date +%H:%M:%S)] start_runtime: navigatorhmi-fw 已有 ${running} 个实例运行，跳过启动" >> "${LOG}"
    exit 0
fi

echo "[$(date +%H:%M:%S)] start_runtime: 环境就绪，启动 FW（project=${PROJECT}）" >> "${LOG}"
# ⚠ 不用 exec：S99qt-test 守护循环前台调用本脚本（退出码返回 → 循环 sleep 5 再拉起崩溃的 FW）。
# exec 会把守护 shell 替换成 FW（循环消失 → 崩溃无法拉起 + 单实例收敛失效）——D-2 实测发现的缺陷。
if [ -f "${PROJECT}" ]; then
    "${FW}" --project "${PROJECT}" >> "${LOG}" 2>&1
else
    "${FW}" >> "${LOG}" 2>&1
fi
