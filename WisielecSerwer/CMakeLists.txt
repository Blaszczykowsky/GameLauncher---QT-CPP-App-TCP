cmake_minimum_required(VERSION 3.16)
project(SerwerWisielec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)

qt_standard_project_setup()

qt_add_executable(SerwerWisielec
    main.cpp
    serwer_wisielec.h
    serwer_wisielec.cpp
)

target_link_libraries(SerwerWisielec PRIVATE Qt6::Core Qt6::WebSockets)

set_target_properties(SerwerWisielec PROPERTIES WIN32_EXECUTABLE FALSE)
