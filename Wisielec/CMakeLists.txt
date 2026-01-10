cmake_minimum_required(VERSION 3.16)
project(MultiGameLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(MultiGameLauncher
    main.cpp
    launcher.h
    launcher.cpp
    wisielec_window.h
    wisielec_window.cpp
    game_logic.h
    game_logic.cpp
    game_config.h
)

target_link_libraries(MultiGameLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)
