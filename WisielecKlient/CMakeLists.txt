cmake_minimum_required(VERSION 3.16)
project(WisielecClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebSockets)

qt_standard_project_setup()

qt_add_executable(WisielecClient
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
    klient_sieciowy.h
    klient_sieciowy.cpp
)

target_link_libraries(WisielecClient PRIVATE Qt6::Widgets Qt6::WebSockets)
