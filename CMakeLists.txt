cmake_minimum_required(VERSION 3.16)
project(NavigatorHMI_FW VERSION 1.1.0 LANGUAGES C CXX)

# ============================================================
# NavigatorHMI FW 应用（跨平台）
# 目标：RK3128（Linux ARM，Qt 6.5.6）+ Windows 桌面（仿真器，同一份代码）
# 输入：组态软件编译的 .navihmi（fw/proto/navihmi.proto 契约）
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
    message(FATAL_ERROR "未找到 Qt（Qt6 或 Qt5）——RK3128 用 CMAKE_PREFIX_PATH 指向 Qt 6.5.6 ARM 安装目录")
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Qml Quick Widgets)
message(STATUS "Qt 版本: ${QT_VERSION_MAJOR}.${Qt${QT_VERSION_MAJOR}_VERSION}")

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
file(GLOB_RECURSE HEADERS
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.hpp
)

# navihmi.proto → C++ 解析类（protoc 生成到 build 目录）
set(NAVIHMI_PROTO "${CMAKE_SOURCE_DIR}/proto/navihmi.proto")
if(EXISTS "${NAVIHMI_PROTO}")
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS "${NAVIHMI_PROTO}")
    message(STATUS "navihmi.proto 已纳入生成")
else()
    message(WARNING "缺少 proto/navihmi.proto，契约解析不可用")
endif()

# QML 资源
qt_add_resources(RESOURCES qml.qrc)

if(SOURCES)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${PROTO_SRCS} ${RESOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}
        Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Widgets
        protobuf::libprotobuf
    )
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
