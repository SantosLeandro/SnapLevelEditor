# SnapLevel Editor

Editor de mapas (tilemaps) 2D desenvolvido em **C++** utilizando **Qt 6 Widgets**.

O projeto fornece uma interface para criação e edição de mapas, com suporte a gerenciamento de Rooms, Layers, GameObjects e Tilesets.

## Recursos

* Menu principal
* Barra de ferramentas
* Explorer para Worlds, Rooms, Layers e GameObjects
* Canvas para edição da Room
* Painel de propriedades
* Painel de Tileset
* Console integrado
* Tema escuro
* Arquitetura modular

## Estrutura

```text
SnapLevelEditor/
├── CMakeLists.txt
└── src/
    ├── main.cpp
    ├── StyleSheet.h
    ├── MainWindow.h
    ├── MainWindow.cpp
    ├── CanvasWidget.h
    ├── CanvasWidget.cpp
    ├── TilesetPanel.h
    └── TilesetPanel.cpp
```

## Build

### Linux

Instale as dependências:

```bash
sudo apt install qt6-base-dev qt6-base-dev-tools cmake build-essential
```

Compile o projeto:

```bash
git clone <repositorio>

cd SnapLevelEditor

mkdir build
cd build

cmake ..
make -j$(nproc)

./SnapLevelEditor
```

### Windows

Abra o projeto no **Qt Creator** ou utilize o **CMake** com uma instalação do Qt 6.

### macOS

Instale o Qt 6 e compile utilizando o mesmo fluxo do CMake.
