cmake_minimum_required(VERSION 3.16)
project(NavigatorHMI_FW VERSION 1.0.0 LANGUAGES C CXX)

# ============================================================
# 项目名称: NavigatorHMI_FW
# 描述:     Embedded device code for NavigatorHMI (i.MX6ULL)
# 架构:     ARM (arm-linux-gnueabihf)
# 编译方式: 交叉编译，在 Docker 容器中进行
# ============================================================

# 设置 C++ 标准（Linaro 4.9.4 最大支持 C++14）
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 全局编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# ============================================================
# Qt5 依赖（ARM 版，由 -Target qt 交叉编译产出）
# 工具链文件检测到 build/qt5.12.9-arm 时已自动设置 CMAKE_PREFIX_PATH
# ============================================================
find_package(Qt5 QUIET COMPONENTS Core Gui Widgets)
if(NOT Qt5_FOUND)
    message(FATAL_ERROR
        "未找到 ARM 版 Qt5！\n"
        "请先执行: .\\docker-build.ps1 -Target qt   (首次约 1~2 小时)\n"
        "产物应位于: build/qt5.12.9-arm/")
endif()
message(STATUS "Qt5 版本: ${Qt5_VERSION}")

# AUTOMOC: 自动处理 Q_OBJECT 宏（moc）
set(CMAKE_AUTOMOC ON)

# ============================================================
# 源文件收集
# ============================================================
# 收集 src/ 目录下所有源文件
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
)

# ============================================================
# 可执行文件目标
# ============================================================
if(SOURCES)
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Gui Qt5::Widgets)
    
    # 如果有公共头文件目录，可以在这里添加
    # target_include_directories(${PROJECT_NAME} PRIVATE
    #     ${CMAKE_SOURCE_DIR}/include
    # )
    
    message(STATUS "已添加可执行目标: ${PROJECT_NAME}")
    message(STATUS "源文件数量: ${SOURCES}")
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
message(STATUS "输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
