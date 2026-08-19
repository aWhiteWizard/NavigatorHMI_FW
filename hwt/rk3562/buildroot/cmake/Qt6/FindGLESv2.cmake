# B-4 hwt patch: 交叉编译强制 GLESv2 (libmali 已提供, 跳过编译测试)
# 原始文件来自 Qt6Gui 安装 (Qt6/cmake/FindGLESv2.cmake), 此处打补丁:
# 交叉编译环境 check_cxx_source_compiles 无法链接 GLES, 直接声明可用
set(HAVE_GLESv2 ON)
set(GLESv2_FOUND TRUE)
set(GLESv2_LIBRARY "${CMAKE_FIND_ROOT_PATH}/usr/lib/libGLESv2.so")
set(GLESv2_INCLUDE_DIR "${CMAKE_FIND_ROOT_PATH}/usr/include")

if(NOT TARGET GLESv2::GLESv2)
    add_library(GLESv2::GLESv2 UNKNOWN IMPORTED)
    set_target_properties(GLESv2::GLESv2 PROPERTIES
        IMPORTED_LOCATION "${GLESv2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLESv2_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(GLESv2_LIBRARY GLESv2_INCLUDE_DIR)
