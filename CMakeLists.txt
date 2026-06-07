cmake_minimum_required(VERSION 3.16)
project(GameWatch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Network)
    set(QT_LIBS Qt5::Widgets Qt5::Network)
else()
    set(QT_LIBS Qt6::Widgets Qt6::Network)
endif()

set(SOURCES
    main.cpp
    mainwindow.cpp
    windowmonitor.cpp
    pushdeerclient.cpp
)

set(HEADERS
    mainwindow.h
    windowmonitor.h
    pushdeerclient.h
)

if(WIN32)
    list(APPEND SOURCES windowmonitor_win.cpp)
else()
    list(APPEND SOURCES windowmonitor_mac.mm)
endif()

add_executable(GameWatch WIN32 ${SOURCES} ${HEADERS})
target_link_libraries(GameWatch PRIVATE ${QT_LIBS})