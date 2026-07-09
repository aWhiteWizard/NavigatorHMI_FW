# ============================================================
# i.MX6ULL ARM 交叉编译工具链配置
# 目标: arm-linux-gnueabihf
# 用途: 用于 NavigatorHMI_FW 项目的交叉编译
# ============================================================

# 目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 编译器前缀
set(CROSS_COMPILE arm-linux-gnueabihf-)

# 指定编译器
set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)

# 指定汇编器、链接器等工具
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_AR ${CROSS_COMPILE}ar)
set(CMAKE_LINKER ${CROSS_COMPILE}ld)
set(CMAKE_OBJCOPY ${CROSS_COMPILE}objcopy)
set(CMAKE_OBJDUMP ${CROSS_COMPILE}objdump)
set(CMAKE_STRIP ${CROSS_COMPILE}strip)
set(CMAKE_RANLIB ${CROSS_COMPILE}ranlib)
set(CMAKE_SIZE ${CROSS_COMPILE}size)

# 搜索路径（在容器内交叉编译工具链在系统路径中）
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
