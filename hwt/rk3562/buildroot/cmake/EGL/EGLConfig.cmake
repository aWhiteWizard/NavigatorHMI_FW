# B-4: EGL CMake config (libmali 提供, buildroot 无此包)
# 放置: staging/usr/lib/cmake/EGL/EGLConfig.cmake
# 注意: Qt 的 FindGLESv2.cmake 检查 EGL_LIBRARY 变量 (find_package(EGL) 后)
if(NOT TARGET EGL::EGL)
    add_library(EGL::EGL SHARED IMPORTED)
    set_target_properties(EGL::EGL PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libEGL.so"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
    )
endif()
set(EGL_FOUND TRUE)
set(EGL_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../include")
set(EGL_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../include")   # Qt FindGLESv2 用
set(EGL_LIBRARIES EGL::EGL)
set(EGL_LIBRARY "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libEGL.so") # Qt FindGLESv2 用
set(HAVE_EGL TRUE)
