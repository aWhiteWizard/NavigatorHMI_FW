# B-4: GLESv2 CMake config (libmali 提供, buildroot 无此包)
# 放置: staging/usr/lib/cmake/GLESv2/GLESv2Config.cmake
# 注意: Qt 用 FindGLESv2.cmake (模块模式), 需 GLESv2_LIBRARY/GLESv2_INCLUDE_DIR/HAVE_GLESv2
# 本文件为 config 模式, 供 find_package(GLESv2) 无 FindGLESv2 时兜底; 但 Qt 自带 FindGLESv2.cmake
# 在 CMAKE_MODULE_PATH, 会优先走模块模式 —— 因此本文件主要设变量供其使用
set(GLESv2_FOUND TRUE)
set(GLESv2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../include")
set(GLESv2_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../include")
set(GLESv2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libGLESv2.so")
set(GLESv2_LIBRARY "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libGLESv2.so")
set(HAVE_GLESv2 TRUE)

if(NOT TARGET GLESv2::GLESv2)
    add_library(GLESv2::GLESv2 SHARED IMPORTED)
    set_target_properties(GLESv2::GLESv2 PROPERTIES
        IMPORTED_LOCATION "${GLESv2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLESv2_INCLUDE_DIR}"
    )
endif()
