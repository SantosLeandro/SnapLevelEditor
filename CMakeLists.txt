cmake_minimum_required(VERSION 3.16)
project(SnapLevelEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

qt_standard_project_setup()

add_executable(SnapLevelEditor
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/CanvasWidget.h
    src/CanvasWidget.cpp
    src/TilesetPanel.h
    src/TilesetPanel.cpp
    src/StyleSheet.h
    src/Model/Tile.h
    src/Model/GameObject.h
    src/Model/GameObjectDef.h
    src/Model/GameObjectDef.cpp
    src/Model/Layer.h
    src/Model/Layer.cpp
    src/Model/Room.h
    src/Model/Room.cpp
    src/Model/World.h
    src/Model/World.cpp
    src/Renderer/MapView.h
    src/Renderer/MapView.cpp
    src/Serialization/WorldSerializer.h
    src/Serialization/WorldSerializer.cpp
    src/Commands/UndoCommands.h
    src/Commands/UndoCommands.cpp
)

target_link_libraries(SnapLevelEditor PRIVATE Qt6::Widgets Qt6::OpenGLWidgets OpenGL::GL)
