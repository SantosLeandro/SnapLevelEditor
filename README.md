# SnapLevel Editor — Qt C++ UI

Réplica da interface do "SnapLevel Editor" (tilemap/level editor) feita em **Qt 6 Widgets (C++)**.

## Estrutura

```
SnapLevelEditor/
├── CMakeLists.txt
└── src/
    ├── main.cpp           # ponto de entrada, aplica o stylesheet dark
    ├── StyleSheet.h        # QSS com o tema escuro do editor
    ├── MainWindow.h/.cpp    # janela principal: menu, toolbar, docks (Explorer,
    │                        # Properties, Tileset/GameObjects/Console)
    ├── CanvasWidget.h/.cpp  # área central do "Room_001" (tiles, player, moedas, inimigo)
    └── TilesetPanel.h/.cpp  # grade de tiles selecionável no painel inferior
```

## O que foi reproduzido

- Menu superior (File, Edit, View, Room, Layer, Tools, GameObject, Build, Help)
- Toolbar com New/Open/Save/Save As, Undo/Redo, ferramentas de pintura
  (Select, Brush, Erase, Fill, Rect, Line — exclusivas entre si, como um
  `QActionGroup`), GameObject/Trigger/Camera e Play
- Painel **Explorer** (esquerda): combo de World, filtro, `QTreeWidget`
  com Layers e GameObjects da Room_001, lista de Rooms e mini-toolbar
- Área central com abas de Room, mini-toolbar de view (grid/lock/brilho/zoom),
  canvas desenhado com `QPainter` (chão, plataformas, espinhos, caixote,
  árvore, moedas, player e inimigo) e barra de status (Room Size / Mouse /
  Tile / Layer)
- Painel **Properties** (direita) com abas Properties / Tile Info, campos do
  GameObject selecionado (Name, ID, Position, Layer, Sprite, Animation,
  Flip, Respawn) e tabela de Custom Properties
- Painel inferior com abas Tileset / GameObjects / Prefabs / Animations /
  Console, grade de tiles clicável e console de log

> As cores dos tiles/sprites são placeholders (retângulos e formas
> desenhadas via `QPainter`), já que não há os arquivos de sprite originais.
> Basta trocar a lógica de `CanvasWidget`/`TilesetPanel` por `QPixmap`
> reais quando tiver os assets.

## Build (Linux/Ubuntu)

```bash
sudo apt-get install qt6-base-dev qt6-base-dev-tools cmake build-essential

cd SnapLevelEditor
mkdir build && cd build
cmake ..
make -j$(nproc)
./SnapLevelEditor
```

## Build (Windows/macOS)

1. Instale o Qt 6 (via [Qt Online Installer](https://www.qt.io/download-qt-installer) ou `vcpkg`/`brew`).
2. Abra a pasta do projeto no **Qt Creator** (File → Open File or Project →
   `CMakeLists.txt`) ou rode o mesmo fluxo de CMake acima em um terminal com
   o ambiente do Qt configurado.

## Próximos passos sugeridos

- Substituir os desenhos vetoriais do `CanvasWidget` por `QGraphicsScene` +
  `QGraphicsPixmapItem` para tiles/sprites reais e permitir pintura
  interativa (clicar para colocar tile, arrastar para pintar, etc.).
- Conectar a seleção da `QTreeWidget` (GameObjects) ao painel de
  Properties para edição real dos dados.
- Persistir o nível em JSON/XML (estrutura de World → Rooms → Layers →
  GameObjects).
