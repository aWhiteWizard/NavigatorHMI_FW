# qtvk 布局补丁（F 循环 2026-08-23 NavigatorHMI）

## 背景

用户需求（Check 续N+1）：语言切换去掉右上角自绘按钮，改键盘自带地球图标**直接点击切换中英文**。
qtvk 6.4.3 默认布局不满足（全键盘 main 布局无 ChangeLanguageKey；数字键盘地球键 customLayoutsOnly: true
导致 zh_CN/en_US（均 fallback）间无法切换）——需要改布局源码重编译。

## 修改内容（4 文件）

| 文件 | 修改 |
|------|------|
| `fallback/main.qml` | 底部行（SymbolModeKey 前）加 `ChangeLanguageKey`（无 customLayoutsOnly）——英文全键盘有地球键 |
| `zh_CN/main.qml` | 底部行（SymbolModeKey 前）加 `ChangeLanguageKey`——中文全键盘有地球键 |
| `fallback/digits.qml` | `ChangeLanguageKey` 去掉 `customLayoutsOnly: true`——数字键盘地球键可在 activeLocales 间切换 |
| `fallback/numbers.qml` | 同上 |

## 应用方式（关键）

布局源码在 `/workspace/build/qtvk-src/qtvirtualkeyboard-everywhere-src-6.4.3/src/layouts/`（build-vk-persist.sh
会 rm -rf 重新解压覆盖源码）——**修改必须重新应用到解压后的源码**：

1. 构建前：把本目录 4 个 qml 复制到解压后源码对应位置：
   ```bash
   cp /workspace/hwt/rk3562/qtvk-layouts-patch/fallback/*.qml \
      /workspace/build/qtvk-src/qtvirtualkeyboard-everywhere-src-6.4.3/src/layouts/fallback/
   cp /workspace/hwt/rk3562/qtvk-layouts-patch/zh_CN/*.qml \
      /workspace/build/qtvk-src/qtvirtualkeyboard-everywhere-src-6.4.3/src/layouts/zh_CN/
   ```
2. 编译两个插件（zh_CN 布局在 pinyin 插件！fallback 在 layouts 插件）：
   ```bash
   cd /workspace/build/qtvk-build
   rm -f src/layouts/CMakeFiles/qtvkblayoutsplugin.dir/qrc_*.cpp src/layouts/.rcc/qrc_*.cpp
   rm -f src/plugins/pinyin/CMakeFiles/qtvkbpinyinplugin.dir/qrc_*.cpp src/plugins/pinyin/.rcc/qrc_*.cpp
   make qtvkblayoutsplugin qtvkbpinyinplugin -j8
   ```
   > 注意：zh_CN 布局源码在 **pinyin 插件**的 qrc（`src/plugins/pinyin/.rcc/qmake_virtualkeyboard_pinyin_layouts.qrc`），
   > 只编 layouts 插件 zh_CN 修改不生效！fallback 布局在 layouts 插件。
3. 部署（覆盖 fs-overlay 与板子）：
   ```bash
   # fs-overlay（rootfs 重建持久化）
   cp build/qtvk-build/qml/QtQuick/VirtualKeyboard/Layouts/libqtvkblayoutsplugin.so \
      hwt/rk3562/buildroot/fs-overlay/usr/qml/QtQuick/VirtualKeyboard/Layouts/
   cp build/qtvk-build/qml/QtQuick/VirtualKeyboard/Plugins/Pinyin/libqtvkbpinyinplugin.so \
      hwt/rk3562/buildroot/fs-overlay/usr/qml/QtQuick/VirtualKeyboard/Plugins/Pinyin/
   # 板子（运行时）
   scp 上述两 .so 到板端对应路径 + 清 QML 缓存（/.cache/NavigatorHMI_FW /root/.cache/NavigatorHMI_FW）+ 重启 FW
   ```

## 坑（踩过）

1. **QML 磁盘缓存**：布局 qml 有 `.cache/NavigatorHMI_FW/qmlcache/`（根目录 .cache，非 ~/.qmlcache）——
   更新布局后必须清除，否则旧编译布局生效
2. **zh_CN 布局在 pinyin 插件**：按语言拆分布局 qrc（zh_CN→pinyin 插件、fallback→layouts 插件），
   改 zh_CN 布局只重编 layouts 插件无效
3. **qmldir prefer qrc**：布局固定从 `qrc:/qt-project.org/imports/QtQuick/VirtualKeyboard/Layouts` 加载，
   磁盘布局 qml 不会生效（调试时误上传过磁盘 qml 无效）
4. **FW 侧 `languagePopupListEnabled=false`**（main.qml）：ChangeLanguageKey 点击走 `changeInputLanguage()`
   循环切换——与布局补丁配合才完整（只改布局不设此开关，点击弹语言列表）

## FW 侧配套

`src/qml/main.qml`：删 langSwitchBtn + `inputPanel.keyboard.style.languagePopupListEnabled = false`
（style 经 Loader 异步加载，Connections onStyleChanged 时设置）
