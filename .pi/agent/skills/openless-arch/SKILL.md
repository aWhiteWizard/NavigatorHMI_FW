---
name: openless-arch
description: OpenLess 项目代码架构全景——基于 Tauri v2 的本地语音输入桌面应用。加载此 skill 后快速理解 Rust 后端/React 前端模块职责、分层调用关系、平台适配模式、关键依赖版本，减少后续阅读时的探索成本。
---

# OpenLess 代码架构

**版本**：v1.3.15-Beta.2 · **系统**：macOS / Windows / Linux / Android
**后端**：Rust 2021 · **前端**：React 18 + TypeScript + Vite 6 + Tailwind CSS 4
**核心框架**：Tauri ~2.11 + tokio (full) + reqwest 0.12 + cpal 0.15

---

## 一、项目根目录

```
openless/
├── openless-all/app/            ← 主应用代码（前端 + 后端 + 移动端配置）
│   ├── src/                     ← 前端 React + TypeScript
│   ├── src-tauri/               ← 后端 Rust
│   ├── android/                 ← Android 平台配置
│   ├── windows-ime/             ← Windows 输入法集成
│   ├── scripts/                 ← 工程脚本
│   └── package.json
├── assets/                      ← 静态资源（图标、图片）
├── Casks/                       ← macOS Homebrew Cask 配置
├── docs/                        ← 项目文档
├── Examples/                    ← 使用示例
├── scripts/                     ← 顶层构建/发布脚本
├── README.md / README.zh.md
└── USAGE.md
```

---

## 二、后端 Rust 架构（src-tauri/src/）

### 2.1 入口与模块根

| 文件 | 职责 |
|---|---|
| `main.rs` → `lib.rs` | Tauri `Builder` 构造入口，注册命令、插件、状态 |
| `lib.rs` | 模块根，`#[cfg()]` 条件编译切分桌面/移动端；mobile 分支 `#[path="mobile_stubs/xxx.rs"]` 指向桩模块 |

### 2.2 模块全景

```
commands/                       ← IPC 命令层——前端直接调用的 Tauri 命令
├── mod.rs                      ← 公共导入 + 统一重导出
├── dictation.rs                ← 听写（开始/停止/状态查询）
├── qa.rs                       ← AI 问答
├── settings.rs                 ← 设置读写
├── hotkeys.rs                  ← 快捷键注册/查询
├── history.rs                  ← 历史记录
├── providers.rs                ← 云服务商配置（API key/endpoint）
├── marketplace.rs              ← 插件/风格市场
├── style_packs.rs              ← 风格包（改写风格）
├── local_asr.rs                ← 本地语音识别
├── sherpa_asr.rs               ← Sherpa ASR（Windows 本地）
├── foundry_asr.rs              ← Foundry ASR（Windows 本地）
├── remote_input.rs             ← 远程输入
├── github_oauth.rs             ← GitHub OAuth 登录
├── credentials.rs              ← 凭据管理
├── dictionary.rs               ← 自定义词典
└── permissions_cmds.rs         ← 权限请求（麦克风等）

coordinator/                    ← 核心业务编排层——commands 调用 coordinator，coordinator 调用服务
├── dictation.rs                ← 听写状态机（Idle / Starting / Listening / Processing / Inserting + 30s 全局超时）
├── qa.rs                       ← 问答流程编排
├── qa_session.rs               ← 问答会话状态管理
├── hotkey_loops.rs             ← 快捷键触发的主循环
├── capsule_focus.rs            ← Capsule 窗口焦点管理
├── asr_wiring.rs               ← ASR 引擎初始化与切换（本地/云端）
├── polish_flow.rs              ← 文本润色流程
└── resources.rs                ← 资源加载（模型/配置）

asr/                            ← 语音识别引擎 —— 本地 + 云端共约 10 种
├── mod.rs / frame.rs / pcm.rs / wav.rs  ← 基础设施（音频帧/PCM/WAV）
├── volcengine.rs               ← 火山引擎
├── bailian.rs                  ← 阿里云百炼
├── dashscope_multimodal.rs     ← 阿里云 DashScope 多模态
├── elevenlabs.rs               ← ElevenLabs
├── whisper.rs                  ← OpenAI Whisper
├── qwen_realtime.rs            ← Qwen Realtime（通义千问实时）
├── stepfun_realtime.rs         ← 阶跃星辰实时
├── mimo.rs                     ← MIMO
└── local/                      ← 本地 ASR
    ├── sherpa.rs / sherpa_runtime.rs / sherpa_provider.rs / sherpa_download.rs  ← sherpa-onnx 1.13.2
    ├── foundry.rs / foundry_runtime.rs / foundry_provider.rs / foundry_native.rs  ← foundry-local-sdk 1.2.1
    ├── qwen_engine.rs / qwen_ffi.rs ← Qwen C 引擎（macOS）
    ├── apple_speech_provider.rs ← Apple Speech（macOS 内置）
    ├── local_provider.rs       ← 本地 ASR 统一 trait
    ├── cache.rs / download.rs  ← 模型缓存与下载
    ├── models.rs               ← 本地模型元信息
    └── test_run.rs             ← 测试运行器

persistence/                    ← 持久化层
├── preferences.rs              ← 用户偏好（JSON/OS 凭据库）
├── history.rs                  ← 历史记录（CAP=200 环形缓冲）
├── credentials.rs              ← 凭据（OS 原生凭据库 + 旧 JSON 迁移逻辑）
├── android_credentials.rs      ← Android 凭据
├── dictionary.rs               ← 自定义词典
├── correction.rs               ← 语音纠错映射
├── activity.rs                 ← 使用活动记录
├── style_pack.rs / style_pack_archive.rs / style_pack_tests.rs  ← 风格包增删改查
└── paths.rs                    ← 各平台数据目录抽象

android/                        ← Android 平台适配
├── accessibility.rs / jni.rs / native_bridge.rs  ← JNI 桥接
├── overlay.rs                  ← 悬浮窗覆盖层
├── insert.rs                   ← 文本插入
├── types.rs                    ← 类型定义
├── updater.rs / updater_logic.rs  ← 更新逻辑

mobile_stubs/                   ← 移动端桩模块（桌面端编译时占位）
├── hotkey.rs / qa_hotkey.rs / combo_hotkey.rs
├── selection.rs / shortcut_binding.rs
├── side_aware_combo.rs / unicode_keystroke.rs

────────── 独立平台模块 ──────────
hotkey.rs / global_hotkey_runtime.rs / combo_hotkey.rs  ← 快捷键系统（global-hotkey 0.6）
recorder.rs                     ← 录音（cpal 0.15, 16kHz PCM）
insertion.rs                    ← 文本插入（输入法桥接）
polish.rs                       ← 文本润色
correction.rs                   ← 语音纠错
selection.rs                    ← 文本选区获取
device_watch.rs                 ← 音频设备热插拔监听
permissions.rs                  ← 麦克风权限
linux_fcitx.rs                  ← Linux Fcitx 输入法桥接
windows_ime_ipc.rs / windows_ime_profile.rs / windows_ime_protocol.rs / windows_ime_session.rs  ← Windows IME 集成
remote_server/                  ← 远程服务器（axum 0.7, WebSocket）
├── mod.rs / assets/ / pin_persistence.rs
llm_gemini.rs                   ← Google Gemini LLM 集成
coding_agent/                   ← OpenCode 编程助手功能
```

### 2.3 命名惯例

| 模式 | 含义 |
|---|---|
| `*_provider.rs` | 实现某个 trait 的服务提供者（如 `foundry_provider.rs`） |
| `*_runtime.rs` | managed state / 单例运行时（如 `sherpa_runtime.rs`） |
| `*_stubs/` | 移动端桩模块，桌面编译时提供空实现 |
| `*_wiring.rs` | 条件编译 + 依赖注入，按平台选具体实现 |

### 2.4 关键依赖版本

| 依赖 | 版本 | 用途 |
|---|---|---|
| tauri | ~2.11 | 应用框架 |
| tauri-plugin-shell / dialog / updater / single-instance / autostart | 2.x | 插件生态 |
| tokio | 1 (full) | 异步运行时 |
| reqwest | 0.12 | HTTP 客户端（rustls-tls / native-tls 依平台） |
| cpal | 0.15 | 音频采集 |
| global-hotkey | 0.6 | 全局快捷键 |
| enigo | 0.2 | 键盘模拟 |
| sherpa-onnx | 1.13.2 | 本地 ASR（Windows） |
| foundry-local-sdk | 1.2.1 | 本地 ASR（Windows） |
| windows | 0.58 | Win32 API |
| jni | 0.21 | Android JNI 桥接 |
| axum | 0.7 | 远程服务器 + WebSocket |
| ferrous-opencc | 0.4 | 繁简转换 |
| serde / serde_json | 1 | 序列化 |
| base64 | 0.22 | 音频 base64 编码 |
| hmac / sha2 / subtle | 0.12 / 0.10 / 2 | 历史记录完整性校验 |

---

## 三、后端分层调用关系

```
前端 (React)           ──tauri invoke──►  commands/ (IPC 命令层)
                                              │
                                              ▼
                                         coordinator/ (业务编排层)
                                              │
                              ┌───────────────┼───────────────┐
                              ▼               ▼               ▼
                           asr/ (语音识别)  persistence/ (持久化)  其他服务
                          (本地/云端)       (偏好/历史/凭据)
```

- **commands/**: 纯参数校验 + 调用 coordinator，不包含业务逻辑
- **coordinator/**: 状态机 + 编排逻辑；`dictation.rs` 是核心状态机（Idle→Starting→Listening→Processing→Inserting）
- **asr/**: 每个引擎独立文件，实现统一 trait；`asr_wiring.rs` 按平台条件编译选择
- **persistence/**: 读写本地存储，`history.rs` 用 CAP=200 的环形缓冲；`credentials.rs` 含从旧 JSON 到 OS 凭据库的一次性迁移

### 特性标记与条件编译

```
#[cfg(desktop)]   ← 桌面端功能（快捷键、文本插入、托盘图标）
#[cfg(mobile)]    ← 移动端功能（手势、JNI、覆盖层）
#[cfg(target_os = "windows")]  ← Windows IME、Sherpa/Foundry 本地 ASR
#[cfg(target_os = "macos")]    ← Apple Speech、Qwen C 引擎
#[cfg(target_os = "linux")]    ← Fcitx 桥接
#[cfg(target_os = "android")]  ← Android 无障碍 + JNI
```

---

## 四、前端 React 架构（src/）

### 4.1 窗口分发矩阵（main.tsx → App.tsx）

| URL 参数 | 渲染组件 | 加载策略 | 说明 |
|---|---|---|---|
| `?window=capsule` | `<Capsule>` | eager | 听写胶囊，低延迟敏感，体积小 |
| `?window=qa` | `<QaPanel>` | `React.lazy()` | AI 问答面板 |
| `?window=less-computer` | `<LessComputerPanel>` | `React.lazy()` + 条件 import DCE | 仅 macOS，编译期常量折叠 |
| `?window=less-computer-glow` | `<LessComputerGlow>` | `React.lazy()` + 条件 import DCE | 仅 macOS |
| 无（主窗口） | Onboarding 或 FloatingShell | 权限门控后渲染 | 见下方权限门控逻辑 |

### 4.2 权限门控逻辑（App.tsx）

1. **Android**：检查 `localStorage` 中 `openless.androidSetupWizardComplete` 标记 → 未完成则显示 `<Onboarding>`；完成后检查麦克风权限
2. **Windows**：轮询 `getHotkeyStatus()`（每 200ms，最多 50 次 = 10s 超时）→ 超时后强设 `ready` 让用户进入 Permissions 页面
3. **macOS/Linux**：并行检查 accessibility 和 microphone 权限 → 都通过则 `ready`

### 4.3 IPC 封装模式

前端 `lib/ipc/` 的每个文件与后端 `commands/` 的每个文件一一对应。前端通过 `invokeOrMock()` 统一封装 `window.__TAURI_INTERNALS__.invoke()`，在非 Tauri 环境（vite 预览）时返回 mock 数据。

### 4.4 目录结构

```
src/
├── main.tsx                     ← 入口：解析 ?window= 参数 → 分派窗口类型；等待 i18n 初始化后渲染
├── App.tsx                      ← 路由中枢：按 window 类型分发页面
│                                   capsule     → Capsule (eager)
│                                   qa          → QaPanel (lazy)
│                                   less-computer → LessComputerPanel (macOS only, lazy)
│                                   默认        → 权限门控 → Onboarding / FloatingShell
│
├── pages/                       ← 页面级组件
│   ├── FloatingShell.tsx        ← 主浮动壳
│   ├── QaPanel.tsx              ← AI 问答面板
│   ├── Capsule.tsx             ← Capsule 输入面板
│   ├── LessComputerPanel.tsx   ← 少电脑模式（macOS only）
│   ├── LessComputerGlow.tsx    ← 少电脑发光动画（macOS only）
│   ├── History.tsx             ← 历史记录
│   ├── Marketplace.tsx         ← 插件/风格市场
│   ├── Style.tsx               ← 风格包管理
│   ├── Vocab.tsx               ← 用户词典
│   ├── Translation.tsx          ← 翻译功能
│   ├── SelectionAsk.tsx         ← 选中文本提问
│   ├── Overview.tsx            ← 总览页面
│   ├── LocalAsr/               ← 本地语音识别管理
│   │   ├── index.tsx
│   │   ├── components.tsx
│   │   ├── helpers.ts
│   │   └── types.ts
│   └── settings/               ← 设置页（~18 个 Section）
│       ├── tabs.tsx
│       ├── ProvidersSection.tsx
│       ├── ShortcutsSection.tsx
│       ├── ThemeSection.tsx
│       ├── LanguageSection.tsx
│       ├── LocalModelSection.tsx
│       ├── MicrophoneSelect.tsx
│       ├── CodingAgentSection.tsx
│       ├── PermissionsSection.tsx
│       ├── MarketplaceSection.tsx
│       ├── DataStorageSection.tsx
│       ├── RemoteInputSection.tsx
│       ├── RecordingInputSection.tsx
│       ├── AutoUpdateSection.tsx
│       ├── BetaChannelSection.tsx
│       ├── ClaudeConsoleSection.tsx
│       ├── DebugToolsSection.tsx
│       ├── AboutSection.tsx
│       └── CheckUpdateButton.tsx
│
├── components/                  ← 通用组件
│   ├── chat/                   ← 聊天 UI 框架
│   │   ├── bubble.tsx, message.tsx, message-scroller.tsx
│   │   ├── input.tsx, textarea.tsx, markdown.tsx
│   │   ├── avatars.tsx, spinner.tsx, button.tsx
│   │   ├── card.tsx, empty.tsx, marker.tsx, input-group.tsx
│   │   ├── index.ts, lifecycle.ts, orbFeed.ts
│   │   ├── lib/utils.ts, chat.css
│   ├── ui/                     ← 基础 UI 组件
│   │   ├── Modal.tsx, Row.tsx
│   │   ├── SegSimple.tsx, SelectLite.tsx, SwitchLite.tsx
│   ├── SiriGL.tsx              ← Siri 风格动画
│   ├── WindowChrome.tsx        ← 窗口装饰器 + OS 检测
│   ├── Capsule.tsx             ← 听写胶囊
│   ├── AutoUpdate.tsx / AutoUpdateGate.tsx  ← 自动更新
│   ├── Onboarding.tsx          ← 新手引导
│   ├── SettingsModal.tsx
│   ├── GithubLoginModal.tsx    ← GitHub OAuth 登录弹窗
│   ├── Heatmap.tsx             ← 使用热力图
│   ├── ShortcutRecorder.tsx    ← 快捷键录制
│   ├── SavedToast.tsx
│   ├── ThinkingDots.tsx
│   ├── MobileMoreSheet.tsx
│   ├── Icon.tsx, Kbd.tsx
│   ├── AudioCue.tsx
│   ├── Tooltip.tsx
│
├── lib/                         ← 工具库
│   ├── ipc/                    ← Rust 后端 IPC 桥接（18 个文件）
│   │   ├── index.ts            ← 统一直出
│   │   ├── shared.ts, settings.ts, qa.ts, dictation.ts
│   │   ├── hotkeys.ts, history.ts, asr-credentials.ts
│   │   ├── providerSetup.ts, chat-panel.ts
│   │   ├── coding-agent.ts, marketplace.ts
│   │   ├── marketplace-cache.ts, style-packs.ts
│   │   ├── github-oauth.ts, devices.ts
│   │   ├── permissions.ts, updater.ts, vocab.ts
│   │   ├── remote-server.ts, utils.ts
│   │   ├── platform-exports.ts, mock-data.ts
│   ├── types.ts, platform.ts, themeMode.ts
│   ├── hotkey.ts, hotkeyRecorder.ts, hotkeyMigration.ts
│   ├── capsuleLayout.ts, audioCue.ts
│   ├── qaMessage.ts, qaMarkdown.ts
│   ├── stylePrefs.ts, localAsr.ts
│   ├── appVersion.ts, fontScale.ts
│   ├── savedEvent.ts, useMobileLayout.ts
│   ├── useRafThrottle.ts, vocabPresets.ts
│   ├── mockData.ts, windowHotkeyFallback.ts
│   └── androidMicrophonePermission.ts
│
├── i18n/                        ← 国际化
│   ├── index.ts, en.ts, zh-CN.ts
│   ├── zh-TW.ts, ja.ts, ko.ts
│
├── state/                       ← 状态管理
│   ├── HotkeySettingsContext.tsx
│   └── useAppState.ts
│
├── styles/                      ← 样式
│   ├── global.css
│   └── tokens.css
│
└── types/                       ← 类型定义
    └── tauri-plugin-autostart.d.ts
