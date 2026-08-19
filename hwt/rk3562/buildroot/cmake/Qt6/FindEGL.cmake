# B-4 hwt patch: 交叉编译强制 EGL (libmali 已提供, Qt6Gui 依赖)
set(HAVE_EGL ON)
set(EGL_FOUND TRUE)
set(EGL_LIBRARY "${CMAKE_FIND_ROOT_PATH}/usr/lib/libEGL.so")
set(EGL_INCLUDE_DIR "${CMAKE_FIND_ROOT_PATH}/usr/include")

if(NOT TARGET EGL::EGL)
    add_library(EGL::EGL UNKNOWN IMPORTED)
    set_target_properties(EGL::EGL PROPERTIES
        IMPORTED_LOCATION "${EGL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${EGL_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(EGL_LIBRARY EGL_INCLUDE_DIR)
