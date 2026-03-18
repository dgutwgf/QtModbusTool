cmake_minimum_required(VERSION 3.16)
project(QtModbusTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialBus)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialBus)

message(STATUS "Qt Version: ${QT_VERSION}")
message(STATUS "Qt Core Version: ${Qt${QT_VERSION_MAJOR}_CORE_VERSION}")

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QtModbusTool
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_finalize_executable(QtModbusTool)
else()
    add_executable(QtModbusTool
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(QtModbusTool PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialBus
)

set_target_properties(QtModbusTool PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    BUNDLE_ID "com.dgutwgf.QtModbusTool"
)
