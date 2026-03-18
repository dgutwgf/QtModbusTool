cmake_minimum_required(VERSION 3.16)
project(QtModbusTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 只使用 Qt6，避免版本混淆
find_package(Qt6 REQUIRED COMPONENTS Widgets SerialBus)

qt_add_executable(QtModbusTool
    MANUAL_FINALIZATION
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

target_link_libraries(QtModbusTool PRIVATE
    Qt6::Widgets
    Qt6::SerialBus
)

set_target_properties(QtModbusTool PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    BUNDLE_ID "com.dgutwgf.QtModbusTool"
)

qt_finalize_executable(QtModbusTool)
