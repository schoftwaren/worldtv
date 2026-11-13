cmake_minimum_required(VERSION 3.24)
project(worldtv VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Quick QuickControls2 Gui Qml Network Test)
find_package(SDL2 REQUIRED)

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
    SDL2::SDL2
)

# ---- embedded catalog resource -------------------------------------------
qt_add_resources(worldtv "catalog"
    PREFIX "/worldtv"
    FILES app/catalog.json
)

# ---- tests ----------------------------------------------------------------
enable_testing()
add_executable(tst_catalog test/tst_catalog.cpp)
target_compile_definitions(tst_catalog PRIVATE
    APP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/app"
)
target_link_libraries(tst_catalog PRIVATE
    Qt6::Test Qt6::Gui appcore
)
add_test(NAME catalog COMMAND tst_catalog)

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
