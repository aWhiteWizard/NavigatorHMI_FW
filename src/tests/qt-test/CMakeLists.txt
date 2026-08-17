# NavigatorHMI RK3562 Qt 窗口测试程序 (B-1)
# CMake 交叉编译: 用 buildroot staging 的 Qt6 (外部工具链 + sysroot)
cmake_minimum_required(VERSION 3.16)
project(qt-test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(qt-test main.cpp)
target_link_libraries(qt-test Qt6::Core Qt6::Gui Qt6::Widgets)
