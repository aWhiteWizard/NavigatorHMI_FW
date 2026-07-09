cmake_minimum_required(VERSION 3.16)
project(NavigatorHMI_FW VERSION 1.0.0 LANGUAGES C CXX)

# ============================================================
# 项目名称: NavigatorHMI_FW
# 描述:     Embedded device code for NavigatorHMI (i.MX6ULL)
# 架构:     ARM (arm-linux-gnueabihf)
# 编译方式: 交叉编译，在 Docker 容器中进行
# ============================================================

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
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
