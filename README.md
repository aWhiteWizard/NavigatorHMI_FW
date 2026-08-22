# NavigatorHMI_FW

NavigatorHMI 设备端运行时（HMI panel，RK3562 / Qt 6.4.3 / QML）。

输入：PC 组态软件（navigator_hmi 仓库）编译的 `.navihmi` 工程（proto3 二进制，或含瓦片地图的 ZIP 工程包）。
输出：设备屏上的 HMI 运行画面（世界地图 + 多画面 + 导航页），支持 VNC 远程镜像、触摸校准、屏上虚拟键盘。

## 当前架构（2026-08 版）

```
PC 组态软件 ──compile──> .navihmi（proto3 二进制）或 ZIP 工程包（app.navihmi + tiles/ 瓦片）
                              │ 下载到设备 /mnt/user/userdata/
                              ▼
NavigatorHMI_FW（本仓库）
  src/main.cpp             主壳：解析工程包（ZIP 解压 / 纯二进制）→ 生成 QML → 注入运行
  src/converter/           转换器：.navihmi → 每画面 QML 文件
    projectparser.*        proto 解析 → navihmi::Project 运行时模型
    qmlgenerator.*         模型 → QML 文本（画面/控件/事件/世界地图）
  src/runtime/             运行时服务（C++，QML 经 context property 访问）
    runtimebus.*           事件总线：QML emitEvent → 按工程配置执行动作（画面切换/变量写/系统命令）
    datamanager.*          变量中心（TagStore 雏形）：变量值存储/读写/变化通知
    deviceinfo.*           设备信息（IP/MAC/版本/内核/运行时间，真实读取）
    storageinfo.*          存储管理（SD/USB/内存检测、工程扫描/替换默认工程）
    vncmirror.*            VNC 镜像服务（RFB 3.3，脏矩形增量 + 心跳帧率，远程显示/输入）
    projectmodel.*         运行时模型定义（枚举/结构，与 proto 对齐）
  src/qml/                 QML 界面
    main.qml               主壳：画面 Loader + overlay + 导航 + 3 秒自动进工程 + InputPanel 虚拟键盘
    nav.qml                导航页（首页/设备信息/存储管理/通信控制 + 日/夜主题）
    components/            Hmi* 组件库（19 控件：Button/Text/Label/.../WorldMap，与 PC 端控件一一对应）
  src/qml.qrc              QML 资源打包
  proto/navihmi.proto      契约：.navihmi 二进制格式（与 PC 端 NavihmiDto 严格对齐）
  hwt/rk3562/              Buildroot 覆盖（内核 dtb / buildroot 配置 / fs-overlay 固化产物）
  cmake/                   交叉编译工具链（aarch64 buildroot sysroot）
  tools/                   工具脚本（测试工程生成 gen-test-project.ps1 / 瓦片下载 download-amap-tiles.py / 升级工具）
```

## 模块职责

| 模块 | 职责 | 关键机制 |
|------|------|---------|
| **main.cpp** | 启动入口 | `--project` 接受**纯 .navihmi 或 ZIP 工程包**（PK 魔数识别 → 解压 /tmp/navihmi_pkg）；`--convert` 转模型摘要；`--genqml` 生成 QML；QML 引擎 + context property 注入（runtimeBus/dataManager/deviceInfo/storageInfo/vncMirror）；`QT_IM_MODULE=qtvirtualkeyboard` 启用屏上键盘 |
| **converter** | .navihmi → QML | projectparser 解 proto（protobuf LITE）；qmlgenerator 生成每画面 QML（控件属性并集输出 + 事件信号绑定）；世界地图输出 bounds/作业点/范围点/tileBasePath |
| **runtimebus** | 事件路由 | QML `runtimeBus.emitEvent(obj, type)` → 按工程配置匹配控件事件 → 执行动作（screen_switch/tag_write/tag_add/.../run_command）；⑪同名事件限当前+上一+全局画面；TraceLog（NAVIHMI_TRACE） |
| **datamanager** | 变量中心 | 变量值存储（baseValue 初始化）；`value/setValue/hasTag`；valueChanged 通知（同值跳过防回环）；交互控件绑定（R1） |
| **vncmirror** | VNC 镜像 | 生产端 markDirty 报告（西门子 dirty-rect 模式）+ 局部读回 + 心跳帧率；切页分条带；NAVIHMI_VNC 开关 |
| **QML 组件库** | 19 控件 | 与 PC 端控件契约对齐（属性并集容忍）；交互控件 boundTag 绑 DataManager（状态持久化） |
| **hwt** | 板级覆盖 | buildroot 配置（rk3562_navihmi_defconfig：Qt6.4.3 + protobuf + openssh）；fs-overlay（navigatorhmi-fw / VNC 键盘模块 / logo 素材） |

## 工程包格式（ZIP 模式）

```
xxx.navihmi（实际是 ZIP）
├── app.navihmi   ← 工程二进制（proto3）
└── tiles/        ← 地图瓦片 z/x/y.png（高德街道图，世界地图显示区域）
```

设备端 `--project xxx.navihmi` → 检测 ZIP 魔数 → 解压到 `/tmp/navihmi_pkg/` → 加载 `app.navihmi` + 注入 `tileBasePath` → 世界地图铺贴瓦片。纯二进制 .navihmi 向后兼容。

## 编译 / 部署 / 上板验证

### 编译（Docker 容器交叉编译）

```powershell
docker run --rm -v "D:\workspace\code\NavigatorHMI_FW:/workspace" -v "D:\workspace\rk3562-sdk\rk3562-linux-6.1:/sdk" `
  -e BR2_DL_DIR="/sdk/buildroot/dl" -e FORCE_UNSAFE_CONFIGURE=1 -w /workspace `
  swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.1-ubuntu20 `
  /bin/bash -c "cd /workspace/build/rk3562/fw-app && cmake -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/aarch64-buildroot-toolchain.cmake -DCMAKE_PREFIX_PATH=/sdk/buildroot/output/rk3562_navihmi/host/aarch64-buildroot-linux-gnu/sysroot/usr -DProtobuf_PROTOC_EXECUTABLE=/sdk/buildroot/output/rk3562_navihmi/host/bin/protoc /workspace && make -j8"
# 产物: build/rk3562/fw-app/bin/NavigatorHMI_FW（aarch64 ELF）
```

### 部署（SSH 上板）

- 二进制 → `/usr/bin/navigatorhmi-fw`（旧版先备份）
- 工程 → `/mnt/user/userdata/`（app.navihmi 或 demo.navihmi ZIP）
- VNC 键盘模块（fs-overlay 固化，或 SSH 手动部署）→ `/usr/qml/QtQuick/VirtualKeyboard/` + `/usr/lib/libQt6VirtualKeyboard.so*` + `/usr/plugins/platforminputcontexts/`

### 启动 / 验证

```bash
# 板端（S99 自启或手动）
export QT_QPA_PLATFORM=eglfs QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms \
       QT_PLUGIN_PATH=/usr/plugins QML_IMPORT_PATH=/usr/qml QML2_IMPORT_PATH=/usr/qml
navigatorhmi-fw --project /mnt/user/userdata/demo.navihmi   # ZIP 工程包（含瓦片）
# 日志: /tmp/navihmi-mirror.log（VNC 模式）; NAVIHMI_VNC=2 强制开 VNC 5900
# 验证: 世界地图瓦片显示 / 画面切换 / 触摸校准长按 / 虚拟键盘 / VNC 远程
```

## 测试

- **测试工程生成**：`tools/gen-test-project.ps1`（PC 端 navihmi.exe CLI + 输出到 D:\workspace\test_project）——世界地图 + 画面A/B + 19 控件 + 事件/动作全覆盖
- **转换器自测**：`navigatorhmi-fw --convert <工程>`（解析并打印模型摘要）/ `--genqml <工程> <outdir>`

## 文档

- 构建/部署/迁移详见 `doc/rk3562-build-deploy.md`
- 知识库（4_bugs/5_sessions/2_system rk3562 层）记录踩坑与归档

> 历史：早期 i.MX6ULL / Qt 5.12 平台代码已废弃（遗留文件清理见 commit 记录）。
