/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\vncconfig.h
 * @Description: VNC 配置层（W-D F19 三层职责拆分——配置层/管理层/服务层）。
 *               纯参数聚合：启停端口、设备尺寸、帧调度与推送阈值；
 *               由 VncManager 构造时填充，供管理/服务两层读取——参数单点，防魔法数字漂移。
 *               纯数据无 QObject/线程依赖，可独立测试。
 */
#pragma once

#include <cstdint>

namespace navihmi {

/// VNC 配置（帧推流调优参数 2026-08-26 魔法数字整改命名的集中化——原散在 vncmirror.cpp 常量）
struct VncConfig
{
    // ── 尺寸（VNC 屏幕 = 设备物理屏分辨率——N+24 用户裁决：按连接设备型号查表，
    //    devicemeta.deviceResolutionFor()，与工程 deviceWidth/Height 解耦；如 7 寸 1024x600）──
    int devW = 1024;   // 默认 7 寸（setDeviceSize 前兜底）
    int devH = 600;
    quint16 port = 0;  // 启动端口（= navihmi::vncPort()，由 VncManager::start 传入后填）

    // ── 帧调度 ──
    int minFrameIntervalMs = 20;        // 读回节流下限（50fps 上限）
    int fullFrameFallbackMs = 1500;     // 无脏区报告时全帧兜底周期（防漏报画面冻结）

    // ── 推送策略 ──
    int maxDirtyRects = 256;            // 脏矩形队列上限（防恶意/错误报告无限增长）
    int dirtyAreaFullFrameThresholdPercent = 40;   // 脏面积 >40% 走分条带渐进读回
    int fullFrameBandCount = 4;         // 大面积变化时分条带数（每帧 1/4 高度）
};

} // namespace navihmi
