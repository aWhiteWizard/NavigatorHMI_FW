# Qt 交叉编译（ARM 嵌入式，无 GPU）

以 Qt 5.12.9 + i.MX6ULL + Linaro gcc 4.9.4 为验证实例。

## 版本选择原则

- 芯片老 → Qt 版本别追新：Qt 5.12.9 是经典选择（正点原子等厂商验证，gcc 4.9 可编）
- Qt 5.15+ 对老编译器兼容性变差；Qt 6 不适合 Cortex-A7 级芯片

## mkspec 定制

复制 `qtbase/mkspecs/linux-arm-gnueabi-g++/` 为 `linux-arm-gnueabihf-g++/`，
把 `qmake.conf` 中所有 `arm-linux-gnueabi-` 改为 `arm-linux-gnueabihf-`（hard-float）。
`qplatformdefs.h` 内容：`#include "../linux-g++/qplatformdefs.h"`。
放到项目 hwt/qt/mkspecs/ 下版本管理，构建时覆盖进源码。

## configure 参数模板（无 GPU → linuxfb + 软件渲染）

```bash
./configure \
    -prefix /opt/qt5.12.9 \              # 目标机部署路径（固化进 qt_prfxpath）
    -extprefix /workspace/build/qt5.12.9-arm \  # 宿主侧 staging（CMake 用）
    -opensource -confirm-license -release -strip \
    -xplatform linux-arm-gnueabihf-g++ \
    -no-opengl -linuxfb -no-xcb \
    -no-glib -no-dbus -no-cups -no-openssl \
    -no-libudev -no-mtdev \
    -qt-zlib -qt-libpng -qt-libjpeg -qt-freetype -qt-pcre \
    -sql-sqlite \
    -make libs -nomake examples -nomake tests -nomake tools \
    -skip qt3d -skip qtwebengine -skip qttools ... \  # 按需 skip
    -silent
```

**要点**：
- 第三方库全用 `-qt-*` 内置版 → 无需准备 sysroot，工具链自带的 libc/libstdc++ 即可
- `-no-opengl` 时 Qt Quick 自动用软件渲染，可正常显示但帧率低
- 保留模块建议：qtbase、qtdeclarative、qtquickcontrols2、qtgraphicaleffects、qtsvg、qtimageformats、qtserialport（GPS 串口）、qtxmlpatterns
- `-silent` 大幅减少日志量（日志给 AI/人看都友好）
- qmake/moc/uic 自动用宿主 gcc 编译为 x86 工具，无需干预

## 编译与缓存

- 解压和编译都在容器 `/tmp`（原生 ext4）；挂载卷（9p/DrvFs）上 IO 慢 5~10 倍
- staging 安装到挂载卷 `build/qt5.12.9-arm/` 持久化
- 用 `.done` 标记文件防重复全量编译（Qt 全量 1~2 小时）
- 验证产物：`libQt5Core.so` 应是 ARM ELF，`bin/qmake` 应是 x86-64 ELF，
  `strings libQt5Core.so | grep qt_prfxpath` 应显示目标机路径

## CMake 对接（应用侧）

工具链文件中：

```cmake
set(QT_ARM_PREFIX "/workspace/build/qt5.12.9-arm")
if(EXISTS "${QT_ARM_PREFIX}/lib/cmake")
    set(CMAKE_PREFIX_PATH "${QT_ARM_PREFIX}")
endif()
```

应用：`find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)` + `target_link_libraries(app Qt5::Core ...)`

## 部署到 rootfs

只拷 `lib/ plugins/ qml/`，删除开发文件（省几十 MB）：

```bash
find $QT_OVERLAY/lib -type f \( -name "*.a" -o -name "*.la" -o -name "*.prl" \) -delete
rm -rf $QT_OVERLAY/lib/cmake $QT_OVERLAY/lib/pkgconfig
```

环境变量放 rootfs-overlay 的 `/etc/profile.d/qt.sh`（Buildroot 的 /etc/profile 会自动 source）：

```sh
export QT_ROOT=/opt/qt5.12.9
export LD_LIBRARY_PATH=${QT_ROOT}/lib:${LD_LIBRARY_PATH}
export QT_QPA_PLATFORM_PLUGIN_PATH=${QT_ROOT}/plugins
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0
export QT_QPA_FONTDIR=${QT_ROOT}/lib/fonts
export QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/event1   # event 号按实际改
```

## 目标机注意事项

- **字体**：linuxfb 无 fontconfig，必须把 ttf 放进 QT_QPA_FONTDIR；中文需中文字体（DroidSansFallback 等），否则显示方块。字体别进 git（体积+版权），.gitignore 加 `*.ttf *.otf *.ttc`
- **触摸**：优先用 Qt 内置 evdevtouch（无需 tslib）；不灵时 `cat /proc/bus/input/devices` 查 event 号
- **应用窗口**：用 `showFullScreen()` 自适应 fb 分辨率，别写死 `resize()`
