cmake_minimum_required(VERSION 3.24)
project(worldtv VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Quick QuickControls2 Gui Qml Network Test)
# SDL2 is desktop-only (Android: no SDL in the NDK, gamepad stubbed)
if(NOT ANDROID)
    find_package(SDL2 REQUIRED)
endif()

add_subdirectory(third_party/mpvqt)

qt_add_executable(worldtv
    app/main.cpp
    app/channelstore.h app/channelstore.cpp
    app/favorites.h app/favorites.cpp
    app/gamepad.h app/gamepad.cpp
    app/wtvmpvitem.h app/wtvmpvitem.cpp
    app/recents.h app/recents.cpp
)

target_compile_definitions(worldtv PRIVATE
    APP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/app"
    QT_NO_CAST_FROM_ASCII
)

target_include_directories(worldtv PRIVATE app)

target_link_libraries(worldtv PRIVATE
    mpvqt_vendor
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Gui
    Qt6::Network
)
if(NOT ANDROID)
    target_link_libraries(worldtv PRIVATE SDL2::SDL2)
endif()

# ---- embedded catalog resource -------------------------------------------
# QT_RESOURCE_ALIAS is REQUIRED: without it the file lands at
# /worldtv/app/catalog.json (full repo-relative path) while the code reads
# qrc:/worldtv/catalog.json. The source-tree fallback masked this in every
# dev/CI test; only true packaged binaries (no checkout) expose it
# (v1.0.0 AppImage: UI fine, channels=0).
set_property(SOURCE app/catalog.json PROPERTY QT_RESOURCE_ALIAS catalog.json)
qt_add_resources(worldtv "catalog"
    PREFIX "/worldtv"
    FILES app/catalog.json
)

# ---- tests ----------------------------------------------------------------
option(WORLDTV_BUILD_TESTS "build the catalog test" ON)
if(WORLDTV_BUILD_TESTS)
    enable_testing()
    add_executable(tst_catalog test/tst_catalog.cpp)
    target_compile_definitions(tst_catalog PRIVATE
        APP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/app"
    )
    target_link_libraries(tst_catalog PRIVATE
        Qt6::Test Qt6::Gui appcore
    )
    add_test(NAME catalog COMMAND tst_catalog)
endif()

add_library(appcore STATIC
    app/channelstore.h app/channelstore.cpp
    app/favorites.h app/favorites.cpp
)
target_include_directories(appcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)
target_link_libraries(appcore PUBLIC Qt6::Gui Qt6::Network)
target_compile_definitions(appcore PUBLIC
    APP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/app"
)

# ---- Android: package libmpv + its ffmpeg deps into the APK -------------
# Same two-layer silent-exit root cause viddeck hit (2026-10-01): the app
# lib NEEDs libmpv.so, which NEEDs libav*/libsw*/libc++_shared; none are
# packaged by default. libc++_shared is swapped post-build for the
# mpv-android prebuilt copy (cmake/swap_libcpp.sh) - NDK 27's libc++
# lacks the __ndk1 symbols this libmpv bundles.
if(ANDROID)
    if(NOT MPV_LIBRARY)
        message(FATAL_ERROR
            "Android build needs -DMPV_LIBRARY=<arm64-v8a/libmpv.so>")
    endif()
    get_filename_component(MPV_LIB_DIR "${MPV_LIBRARY}" DIRECTORY)
    set(WEXTRA
        "${MPV_LIB_DIR}/libmpv.so"
        "${MPV_LIB_DIR}/libavcodec.so"
        "${MPV_LIB_DIR}/libavdevice.so"
        "${MPV_LIB_DIR}/libavfilter.so"
        "${MPV_LIB_DIR}/libavformat.so"
        "${MPV_LIB_DIR}/libavutil.so"
        "${MPV_LIB_DIR}/libswresample.so"
        "${MPV_LIB_DIR}/libswscale.so"
    )
    list(JOIN WEXTRA ";" WEXTRA_JOINED)
    set_target_properties(worldtv PROPERTIES
        QT_ANDROID_EXTRA_LIBS "${WEXTRA_JOINED}"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app/android"
    )
endif()

# ---- Embedded QML: portable binaries must carry their UI ----------------
# Desktop dev keeps the source-tree load (APP_SOURCE_DIR); Android and
# WORLDTV_EMBED_QML=ON release builds embed Main.qml at /worldtv/ and
# load qrc:/worldtv/Main.qml (main.cpp dual-candidates). AppImage rule:
# the entry QML rides in the EXECUTABLE's own qrc - a module-lib qrc is
# NOT visible to the packaged binary (viddeck 5-round lesson).
set(WEMBED_QML OFF)
if(ANDROID OR WORLDTV_EMBED_QML)
    set(WEMBED_QML ON)
endif()
if(WEMBED_QML)
    set_property(SOURCE app/Main.qml PROPERTY
        QT_RESOURCE_ALIAS Main.qml)
    qt_add_resources(worldtv "worldtv_qml"
        PREFIX "/worldtv"
        FILES app/Main.qml)
    # mpv TLS on Android/ffmpeg: no system CA store exists there; the
    # vendored mpvcontroller extracts this and sets mpv tls-ca-file.
    set(WTV_CA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/res/ca-bundle.crt)
    set_property(SOURCE ${WTV_CA_FILE} PROPERTY QT_RESOURCE_ALIAS ca-bundle.crt)
    qt_add_resources(worldtv "worldtv_ca"
        PREFIX "/ca"
        FILES ${WTV_CA_FILE})
    target_compile_definitions(worldtv PRIVATE WORLDTV_QML_EMBEDDED)
endif()

# SDL2 is a desktop-only couch input; Android ships no SDL (touch covers
# it). gamepad.cpp's WORLDTV_NO_SDL block compiles the stub there; the
# rest of the app is untouched.
if(ANDROID)
    target_compile_definitions(worldtv PRIVATE WORLDTV_NO_SDL=1)
endif()

