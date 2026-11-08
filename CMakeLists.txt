# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.21)
project(zavo VERSION 0.1.0 LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick WebEngineQuick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DESKTOP REQUIRED IMPORTED_TARGET gio-unix-2.0 json-glib-1.0)

add_executable(zavo main.cpp desktop.c)
set_target_properties(zavo PROPERTIES AUTOMOC ON C_STANDARD 11 CXX_STANDARD 17)
target_link_libraries(zavo PRIVATE Qt6::Quick Qt6::WebEngineQuick PkgConfig::DESKTOP)
qt_add_resources(zavo ui PREFIX / FILES Shell.qml Maps.qml TouchKeys.qml wallpaper.svg)
