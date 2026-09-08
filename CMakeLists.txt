cmake_minimum_required(VERSION 3.16)
project(NavigatorHMI_FW VERSION 1.1.0 LANGUAGES C CXX)

# ============================================================
# NavigatorHMI FW 应用（跨平台）
# 目标：RK3562（Linux ARM，Qt 6.4.3）+ Windows 桌面（仿真器，同一份代码）
# 输入：组态软件编译的 .navihmi（proto/navihmi.proto 契约）
# 交叉编译：-DCMAKE_TOOLCHAIN_FILE=<Rockchip SDK toolchain>（Ubuntu 18 宿主，CMake 3.16+）
# ============================================================

# C++17（Qt 6 要求；GCC 8+ / MinGW 11 均支持）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# ============================================================
# Qt（优先 Qt6，兼容 Qt5 便于过渡期验证）
# RK3562：Qt 6.5.6（Rockchip SDK/Buildroot 交叉编译产物，CMAKE_PREFIX_PATH 指定）
# ============================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Core Gui Qml Quick Widgets)
if(NOT QT_FOUND)
    message(FATAL_ERROR "未找到 Qt（Qt6 或 Qt5）——RK3562 用 CMAKE_PREFIX_PATH 指向 Qt 6.4.3 ARM 安装目录")
endif()
# Qml/Quick 可选：先单独探测 Qt6Qml/Qt6Quick 包（find_package(Qt6 COMPONENTS) 的组件在首次调用时解析，
# 二次调用会被跳过——故分别用独立包名 find_package(Qt6Qml)/find_package(Qt6Quick)）
set(HAVE_QT_QML FALSE)
find_package(Qt${QT_VERSION_MAJOR}Qml QUIET)
find_package(Qt${QT_VERSION_MAJOR}Quick QUIET)
if(Qt${QT_VERSION_MAJOR}Qml_FOUND AND Qt${QT_VERSION_MAJOR}Quick_FOUND)
    set(HAVE_QT_QML TRUE)
endif()
# 核心组件（Core/Gui/Widgets 必需；Sql 为 G-2 数据记录模块——板端 Qt 6.4.3 已验证 qsqlite 驱动；
# SerialBus 为 H-8 数据采集 Modbus——板端已装 libQt6SerialBus）
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql SerialBus)
# P-6: QtMultimedia（官方仓库交叉编译, libQt6Multimedia 固化 fs-overlay + sysroot cmake 配置后可用——ffmpeg 后端）
find_package(Qt${QT_VERSION_MAJOR}Multimedia QUIET)
if(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
    message(STATUS "QtMultimedia 可用（P-6 Frame 视频启用）")
else()
    message(WARNING "QtMultimedia 未找到——P-6 Frame 视频不可用（需先构建 qtmultimedia 并安装到 sysroot）")
endif()
# K-8a: QtHttpServer（官方仓库交叉编译, libQt6HttpServer 已固化 fs-overlay/usr/lib + sysroot cmake 配置）
find_package(Qt${QT_VERSION_MAJOR}HttpServer QUIET)
if(Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
    message(STATUS "QtHttpServer 可用（K-8b 接收端启用）")
else()
    message(WARNING "QtHttpServer 未找到——K-8b 接收端不可用（需先跑 build-qthttp.sh 构建并安装到 sysroot）")
endif()
# Y-4: Qt6Mqtt（官方仓库 qt/qtmqtt v6.4.3 交叉编译——build-qtmqtt.sh；MqttDriver 需链接 Qt6::Mqtt）
find_package(Qt${QT_VERSION_MAJOR}Mqtt QUIET)
if(Qt${QT_VERSION_MAJOR}Mqtt_FOUND)
    message(STATUS "Qt6Mqtt 可用（Y-4 MQTT 驱动启用）")
else()
    message(WARNING "Qt6Mqtt 未找到——Y-4 MQTT 驱动不可用（需先跑 build-qtmqtt.sh 构建并安装到 sysroot）")
endif()
message(STATUS "Qt 版本: ${QT_VERSION_MAJOR}.${Qt${QT_VERSION_MAJOR}_VERSION}  QML: ${HAVE_QT_QML}")

# ============================================================
# protobuf（解析 .navihmi 契约）
# 交叉编译时：protoc 用宿主版（FindProtobuf 自动），libprotobuf 用 ARM 版（Rockchip SDK 提供）
# ============================================================
find_package(Protobuf REQUIRED)

# ============================================================
# 平台后端（Linux 嵌入式默认 linuxfb；RK3562 带 GPU 可用 eglfs/wayland，-D 覆盖）
# ============================================================
if(WIN32)
    set(FW_PLATFORM_BACKEND "windows" CACHE STRING "Qt QPA 平台后端")
else()
    set(FW_PLATFORM_BACKEND "linuxfb" CACHE STRING "Qt QPA 平台后端（RK3562: eglfs）")
endif()

# ============================================================
# 源文件 + proto 生成
# ============================================================
# 审查 7e0143b8: GLOB 加 CONFIGURE_DEPENDS——新增源文件自动触发重配置, 免手动重跑 cmake
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
)
# 排除 src/tests/（qt-test 独立目标，不并入主程序）
list(FILTER SOURCES EXCLUDE REGEX ".*/src/tests/.*")
# K-8b: QtHttpServer 缺失时排除 httpreceiver（降级编译——不因 TP 模块缺失破坏整体构建，如 Windows 仿真器）
if(NOT Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
    list(FILTER SOURCES EXCLUDE REGEX ".*/src/httpreceiver/.*")
    message(STATUS "httpreceiver 已排除（QtHttpServer 未找到——接收端编译被跳过，主程序正常）")
endif()
# Y-4: Qt6Mqtt 缺失时排除 mqtt_driver（reviewer 🔴2——mqtt_driver.cpp 无条件 #include <QMqttClient>，
# 无 Qt6Mqtt 环境（Windows 仿真器/未跑 build-qtmqtt.sh）编译硬失败；宏 NAVIHMI_HAVE_MQTT_DRIVER 仅控注册/链接）
if(NOT Qt${QT_VERSION_MAJOR}Mqtt_FOUND)
    list(FILTER SOURCES EXCLUDE REGEX ".*/src/drivers/mqtt_driver\\.cpp")
    message(STATUS "mqtt_driver 已排除（Qt6Mqtt 未找到——MQTT 驱动编译被跳过，主程序正常）")
endif()
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.hpp
)
list(FILTER HEADERS EXCLUDE REGEX ".*/src/tests/.*")

# navihmi.proto → C++ 解析类（protoc 生成到 build 目录；命名空间 navihmi_pb 避免与运行时模型 navihmi 冲突）
set(NAVIHMI_PROTO "${CMAKE_SOURCE_DIR}/proto/navihmi.proto")
if(EXISTS "${NAVIHMI_PROTO}" AND Protobuf_PROTOC_EXECUTABLE)
    set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/pbgen)
    file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
    set(PROTO_PB_CC ${PROTO_GEN_DIR}/navihmi.pb.cc)
    set(PROTO_PB_H ${PROTO_GEN_DIR}/navihmi.pb.h)
    add_custom_command(
        OUTPUT ${PROTO_PB_CC} ${PROTO_PB_H}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                --proto_path=${CMAKE_SOURCE_DIR}/proto
                --cpp_out=${PROTO_GEN_DIR}
                ${NAVIHMI_PROTO}
        DEPENDS ${NAVIHMI_PROTO}
        COMMENT "protoc 生成 navihmi.pb (命名空间 navihmi_pb, 由 proto package 决定)"
        VERBATIM
    )
    set(PROTO_SRCS ${PROTO_PB_CC})
    set(PROTO_HDRS ${PROTO_PB_H})
    message(STATUS "navihmi.proto 已纳入生成 (protoc: ${Protobuf_PROTOC_EXECUTABLE})")
else()
    message(WARNING "缺少 proto/navihmi.proto 或 protoc，契约解析不可用")
endif()

# QML 资源（仅 QML 可用时）
if(HAVE_QT_QML)
    qt_add_resources(RESOURCES ${CMAKE_SOURCE_DIR}/src/qml.qrc)
else()
    set(RESOURCES "")
endif()

if(SOURCES)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${PROTO_SRCS} ${RESOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/pbgen
        ${CMAKE_SOURCE_DIR}/src
        ${Protobuf_INCLUDE_DIRS}
        $<$<BOOL:${HAVE_QT_QML}>:${Qt${QT_VERSION_MAJOR}Gui_PRIVATE_INCLUDE_DIRS}>   # R3: qzipreader_p.h private 头
    )
    if(HAVE_QT_QML)
        target_link_libraries(${PROJECT_NAME}
            Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets
            Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::SerialBus
            protobuf::libprotobuf
        )
        if(Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
            target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::HttpServer)   # K-8b 接收端
            target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_QT_HTTPSERVER=1)
        endif()
        if(Qt${QT_VERSION_MAJOR}Mqtt_FOUND)
            # Y-4: MQTT 驱动（MqttDriver 直接引用 QMqttClient——必须链接 Qt6::Mqtt；宏驱动注册/裁剪）
            target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::Mqtt)
            target_compile_definitions(${PROJECT_NAME} PRIVATE NAVIHMI_HAVE_MQTT_DRIVER=1)
        endif()
        if(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
            # P-6 Frame 视频（审查 2026-09-04）：**不链接** Qt::Multimedia——主程序 C++ 零 QtMultimedia 符号引用，
            # QML 侧 HmiFrameVideo.qml 自 import QtMultimedia 动态加载模块（Loader 隔离：模块缺失只降级视频 Frame，
            # 不拖垮整机启动）；find_package QUIET + 上方 STATUS/WARNING 仅作构建期诊断。libQt6Multimedia.so 由
            # build-qtmm-overlay.sh 固化进镜像（部署不变量，但非本二进制 DT_NEEDED——旧镜像缺模块不致 FW 黑屏）
        endif()
    else()
        target_link_libraries(${PROJECT_NAME}
            Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Widgets
            Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::SerialBus
            protobuf::libprotobuf
        )
    endif()
    if(HAVE_QT_QML)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_QT_QML=1)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        FW_PLATFORM_BACKEND="${FW_PLATFORM_BACKEND}"
        FW_QT_MAJOR=${QT_VERSION_MAJOR}
        NAVIHMI_HAVE_MODBUS_DRIVER=1   # V-1 驱动插件化：Modbus TCP 驱动注册（裁剪关掉则不注册）
    )
    message(STATUS "已添加可执行目标: ${PROJECT_NAME}")
    message(STATUS "源文件数量: ${SOURCES} + proto: ${PROTO_SRCS}")
else()
    message(WARNING "未找到任何源文件，请检查 src/ 目录")
endif()

# ============================================================
# navihmi-cli（I-1 SSH CLI 客户端工具）——SSH 登录后执行的 FW 命令客户端
# 仅需 Core + Network（QLocalSocket）；tools/ 不在主程序 GLOB 内
# ============================================================
if(HAVE_QT_QML)
    add_executable(navihmi-cli
        tools/navihmi-cli/main.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/cliprotocol.h
    )
    target_include_directories(navihmi-cli PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(navihmi-cli Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)
    target_compile_definitions(navihmi-cli PRIVATE FW_QT_MAJOR=${QT_VERSION_MAJOR})
    message(STATUS "已添加可执行目标: navihmi-cli（SSH CLI 客户端）")
endif()

# ============================================================
# 打印编译信息
# ============================================================
message(STATUS "========================================")
message(STATUS "项目: ${PROJECT_NAME}")
message(STATUS "版本: ${PROJECT_VERSION}")
message(STATUS "编译器: ${CMAKE_C_COMPILER}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt: ${QT_VERSION_MAJOR}.${Qt${QT_VERSION_MAJOR}_VERSION}")
message(STATUS "平台后端: ${FW_PLATFORM_BACKEND}")
message(STATUS "输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
