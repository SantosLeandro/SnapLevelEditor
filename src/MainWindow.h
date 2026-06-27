#pragma once
#include <QMainWindow>

class QTreeWidget;
class QTreeWidgetItem;
class QTabWidget;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QListWidget;
class QCheckBox;
class QLabel;
class QSpinBox;
class QListWidget;
class QToolButton;
class QUndoStack;
class MapView;
class TilesetPanel;
class World;
class Room;
class Layer;
class QIcon;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupExplorerDock();
    void setupPropertiesDock();
    void setupCentralArea();
    void setupBottomDock();
    void setupStatusBar();
    void populateCustomProperties();
    void connectTools();
    void connectSignals();

    void refreshExplorerTree();
    void refreshRoomTabs();
    void switchToRoom(int index);
    void addRoom();
    void removeRoom(int index);

    void setActiveLayer(int index);
    void updateLayerLabel();
    void updateRoomSizeLabel();
    void updateRoomPositionFields();
    void updatePropertiesForObject(int64_t id, int layerIndex);

    // File I/O
    void newProject();
    void openProject();
    void saveProject();
    bool saveProjectAs();
    void updateWindowTitle();
    void loadTilesetFromWorld();
    void makeTilesetPathsRelative();

    // Tree columns
    enum TreeCol { kColName = 0, kColVisible = 1, kColLocked = 2 };

    // Tree item type tags
    enum ItemTag { Tag_Room = 1, Tag_Layer = 2, Tag_GameObject = 3, Tag_Container = 4 };

    static QIcon makeEyeIcon(bool visible);
    static QIcon makeLockIcon(bool locked);
    void updateLayerTreeItem(QTreeWidgetItem *item, int roomIdx, int layerIdx);

    // Data
    World *m_world = nullptr;

    // Left dock
    QTreeWidget *m_explorerTree = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QComboBox *m_worldCombo = nullptr;
    QToolButton *m_addRoomBtn = nullptr;
    QToolButton *m_delRoomBtn = nullptr;

    // Center
    QTabWidget *m_roomTabs = nullptr;
    QComboBox *m_zoomCombo = nullptr;
    MapView *m_mapView = nullptr;
    QLabel *m_roomSizeLabel = nullptr;
    QLabel *m_mouseLabel = nullptr;
    QLabel *m_tileLabel = nullptr;
    QLabel *m_layerLabel = nullptr;
    QLabel *m_modeLabel = nullptr;

    // Right dock
    QTabWidget *m_propTabs = nullptr;
    QTableWidget *m_customPropsTable = nullptr;
    QLineEdit *m_propNameEdit = nullptr;
    QLineEdit *m_propIdEdit = nullptr;
    QSpinBox *m_propPosX = nullptr;
    QSpinBox *m_propPosY = nullptr;
    QSpinBox *m_roomPosX = nullptr;
    QSpinBox *m_roomPosY = nullptr;
    QLabel *m_tileInfoId = nullptr;

    // Bottom dock
    QTabWidget *m_bottomTabs = nullptr;
    TilesetPanel *m_tilesetPanel = nullptr;
    QListWidget *m_goTypeList = nullptr;
    QComboBox *m_tilesetCombo = nullptr;

    // State
    int m_activeRoomIndex = 0;
    int m_activeLayerIndex = 1;
    int m_selectedTileId = 1;
    QString m_currentFilePath;
    QUndoStack *m_undoStack = nullptr;
};
