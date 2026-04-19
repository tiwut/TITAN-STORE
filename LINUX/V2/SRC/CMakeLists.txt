cmake_minimum_required(VERSION 3.16)
project(TitanStore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets Network WebEngineWidgets REQUIRED)

add_executable(TitanStore
    main.cpp
    StoreWindow.cpp
    StoreWindow.h
    resources.qrc
)

target_link_libraries(TitanStore PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets)
