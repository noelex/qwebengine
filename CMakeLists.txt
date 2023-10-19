cmake_minimum_required(VERSION 3.16)

project(qwebengine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Gui Core WebEngineQuick)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_library(qwebengine
    WebViewRenderer.h
    WebViewRenderer.cpp
    c_api.h
    c_api.cpp
)

qt_add_qml_module(qwebengine
    URI main
    VERSION 1.0
    QML_FILES
        webview.qml
)

target_link_libraries(qwebengine PRIVATE 
    Qt6::Gui
    Qt6::Quick
    Qt6::WebEngineQuick
    Qt6::WebEngineQuickPrivate
    Qt6::CorePrivate)
