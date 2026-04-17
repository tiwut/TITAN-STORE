cmake_minimum_required(VERSION 3.16)
project(ModernLauncher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets Network WebEngineWidgets REQUIRED)

add_executable(ModernLauncher
    main.cpp
    StoreWindow.cpp
    StoreWindow.h
    resources.qrc
)

# Mac OS X Bundle Konfiguration
set_target_properties(ModernLauncher PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "100% TITAN STORE"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.titan.store"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
)

target_link_libraries(ModernLauncher PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets)
