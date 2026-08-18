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
# RK3128：Qt 6.5.6（Rockchip SDK/Buildroot 交叉编译产物，CMAKE_PREFIX_PATH 指定）
# ============================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Core Gui Qml Quick Widgets)
if(NOT QT_FOUND)
    message(FATAL_ERROR "未找到 Qt（Qt6 或 Qt5）——RK3562 用 CMAKE_PREFIX_PATH 指向 Qt 6.4.3 ARM 安装目录")
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
# Qml/Quick 可选：转换器(--convert)只需 Core；QML 界面需要 Qml/Quick（buildroot 补装后启用）
set(HAVE_QT_QML FALSE)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Qml Quick)
if(Qt${QT_VERSION_MAJOR}_Qml_FOUND AND Qt${QT_VERSION_MAJOR}_Quick_FOUND)
    set(HAVE_QT_QML TRUE)
endif()
message(STATUS "Qt 版本: ${QT_VERSION_MAJOR}.${Qt${QT_VERSION_MAJOR}_VERSION}  QML: ${HAVE_QT_QML}")

# ============================================================
# protobuf（解析 .navihmi 契约）
# 交叉编译时：protoc 用宿主版（FindProtobuf 自动），libprotobuf 用 ARM 版（Rockchip SDK 提供）
# ============================================================
find_package(Protobuf REQUIRED)

# ============================================================
# 平台后端（Linux 嵌入式默认 linuxfb；RK3128 带 GPU 可用 eglfs/wayland，-D 覆盖）
# ============================================================
if(WIN32)
    set(FW_PLATFORM_BACKEND "windows" CACHE STRING "Qt QPA 平台后端")
else()
    set(FW_PLATFORM_BACKEND "linuxfb" CACHE STRING "Qt QPA 平台后端（RK3128: eglfs）")
endif()

# ============================================================
# 源文件 + proto 生成
# ============================================================
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
)
# 排除 src/tests/（qt-test 独立目标，不并入主程序）
list(FILTER SOURCES EXCLUDE REGEX ".*/src/tests/.*")
file(GLOB_RECURSE HEADERS
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
    )
    if(HAVE_QT_QML)
        target_link_libraries(${PROJECT_NAME}
            Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets
            protobuf::libprotobuf
        )
    else()
        target_link_libraries(${PROJECT_NAME}
            Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Widgets
            protobuf::libprotobuf
        )
    endif()
    if(HAVE_QT_QML)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_QT_QML=1)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        FW_PLATFORM_BACKEND="${FW_PLATFORM_BACKEND}"
        FW_QT_MAJOR=${QT_VERSION_MAJOR}
    )
    message(STATUS "已添加可执行目标: ${PROJECT_NAME}")
    message(STATUS "源文件数量: ${SOURCES} + proto: ${PROTO_SRCS}")
else()
    message(WARNING "未找到任何源文件，请检查 src/ 目录")
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
