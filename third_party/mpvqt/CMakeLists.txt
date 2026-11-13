# ECM-free shim for vendored mpvqt (upstream needs extra-cmake-modules,
# which is not available on this machine tonight). See VENDORED.md.
# Builds the three upstream source files into a static lib MpvQt::MpvQt.
include_guard(GLOBAL)

add_library(mpvqt_vendor STATIC
    src/mpvabstractitem.cpp
    src/mpvcontroller.cpp
    src/mpvrenderer.cpp
)
add_library(MpvQt::MpvQt ALIAS mpvqt_vendor)

target_compile_definitions(mpvqt_vendor PRIVATE MPVQT_STATIC_BUILD)

target_include_directories(mpvqt_vendor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/gen
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mpvqt_vendor
    PUBLIC
        Qt6::Quick
        Qt6::OpenGL
)

# Android cross-builds MUST NOT use the desktop pkg-config libmpv
# (x86_64 headers/libs bake into the arm64 build and explode at link or
# load). MPV_INCLUDE_DIR + MPV_LIBRARY point at the mpv-android-libs
# prefix; the .so's deps (ffmpeg, c++_shared) ride in the same dir and
# are packaged by androiddeployqt's extraPrefixFiles/jniLibs mechanism.
find_package(PkgConfig QUIET)
# VIDDECK_MPV_NO_PKGCONFIG: MSYS2/Windows pkgconf bakes MSYS mount paths
# (/ucrt64/include) into PkgConfig::LIBMPV that native CMake cannot
# resolve - there, configure with explicit MPV_LIBRARY + MPV_INCLUDE_DIR
# instead (CI resolves the prefix via pkg-config + cygpath itself).
if(NOT ANDROID AND NOT WORLDTV_MPV_NO_PKGCONFIG AND PkgConfig_FOUND)
    pkg_check_modules(LIBMPV IMPORTED_TARGET mpv)
endif()
if(TARGET PkgConfig::LIBMPV)
    target_link_libraries(mpvqt_vendor PUBLIC PkgConfig::LIBMPV)
else()
    if(ANDROID)
        if(NOT MPV_LIBRARY)
            message(FATAL_ERROR "Android build needs -DMPV_LIBRARY=<path to arm64-v8a/libmpv.so> and -DMPV_INCLUDE_DIR=<prefix with mpv/*.h>")
        endif()
        target_include_directories(mpvqt_vendor PUBLIC "${MPV_INCLUDE_DIR}")
        target_link_libraries(mpvqt_vendor PUBLIC "${MPV_LIBRARY}")
    else()
        # Non-Android explicit path (VIDDECK_MPV_NO_PKGCONFIG): import-lib
        # + headers, same contract as the Android branch.
        if(NOT MPV_LIBRARY OR NOT MPV_INCLUDE_DIR)
            message(FATAL_ERROR "VIDDECK_MPV_NO_PKGCONFIG builds need -DMPV_LIBRARY=<import lib> and -DMPV_INCLUDE_DIR=<prefix with mpv/*.h>")
        endif()
        target_include_directories(mpvqt_vendor PUBLIC "${MPV_INCLUDE_DIR}")
        target_link_libraries(mpvqt_vendor PUBLIC ${MPV_LIBRARY})
    endif()
endif()
