#include "MainWindow.h"
#include "Renderer/MapView.h"
#include "TilesetPanel.h"
#include "Model/World.h"
#include "Model/Room.h"
#include "Model/Layer.h"
#include "Model/GameObject.h"
#include "Model/GameObjectDef.h"

#include <cstdlib>

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QActionGroup>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QFrame>
#include <QStatusBar>
#include <QStyle>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>
#include <QUndoStack>
#include <QDir>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include "Serialization/WorldSerializer.h"

// ─── Constructor ───────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    resize(1536, 1024);

    m_world = new World("ForestWorld");

    // ── First room with demo content ────────────────────────────────────
    Room *room = m_world->addRoom("Room_001");
    room->resize(40, 23);

    Layer *bg = room->addLayer("Background");
    Layer *ground = room->addLayer("Ground");
    room->addLayer("Collision");
    room->addLayer("Decoration");
    room->addLayer("Foreground");

    for (int x = 0; x < room->width(); ++x) {
        for (int y = 20; y < room->height(); ++y)
            ground->setTile(x, y, 1);
    }
    for (int x = 8; x < 14; ++x) ground->setTile(x, 18, 1);
    for (int x = 18; x < 22; ++x) ground->setTile(x, 19, 1);

    bg->fill(12);
    for (int x = 0; x < room->width(); ++x) {
        bg->setTile(x, 0, 13);
        bg->setTile(x, room->height() - 1, 13);
    }
    for (int y = 1; y < room->height() - 1; ++y) {
        bg->setTile(0, y, 13);
        bg->setTile(room->width() - 1, y, 13);
    }

    auto addDemoObj = [&](int layerIdx, const QString &name, const QString &type,
                          float x, float y, float w, float h) {
        GameObject obj;
        obj.id = GameObject::nextId();
        obj.name = name.toStdString();
        obj.type = type.toStdString();
        obj.x = x; obj.y = y;
        obj.width = w; obj.height = h;
        room->layer(layerIdx)->addObject(obj);
    };
    addDemoObj(1, "PlayerStart", "PlayerStart", 160, 544, 28, 48);
    addDemoObj(1, "Coin_01", "Coin", 300, 490, 24, 24);
    addDemoObj(1, "Coin_02", "Coin", 340, 490, 24, 24);
    addDemoObj(1, "Coin_03", "Coin", 380, 490, 24, 24);
    addDemoObj(1, "Enemy_01", "Enemy_Goomba", 640, 560, 32, 32);

    // ── Second room ─────────────────────────────────────────────────────
    Room *room2 = m_world->addRoom("Room_002");
    room2->resize(30, 20);
    room2->addLayer("Background");
    room2->addLayer("Ground");
    room2->addLayer("Foreground");
    room2->setWorldX(room->pixelWidth() + 64);

    // ── Undo stack ───────────────────────────────────────────────────
    m_undoStack = new QUndoStack(this);

    // ── UI setup ────────────────────────────────────────────────────────
    setupToolBar();
    setupMenuBar();
    setupCentralArea();
    setupExplorerDock();
    setupPropertiesDock();
    setupBottomDock();
    setupStatusBar();

    connectTools();
    connectSignals();

    m_mapView->setWorld(m_world);
    m_mapView->setUndoStack(m_undoStack);

    // Refresh tree when objects change (add/remove via MapView or undo/redo)
    connect(m_mapView, &MapView::objectsChanged, this, &MainWindow::refreshExplorerTree);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &MainWindow::refreshExplorerTree);

    // GameObject definitions
    m_goDefManager = new GameObjectDefManager();
    m_goDefManager->loadDefault(QApplication::applicationDirPath());
    m_mapView->setGameObjectDefManager(m_goDefManager);
    populateGameObjectList();

    // Mode indicator
    auto updateModeLabel = [this](MapView::ToolType t) {
        if (!m_modeLabel) return;
        switch (t) {
        case MapView::ToolType::Select:       m_modeLabel->setText("Mode: Select");  m_modeLabel->setStyleSheet("color:#5fb0ff; font-weight:bold; padding:0 8px;"); break;
        case MapView::ToolType::Brush:
        case MapView::ToolType::Erase:
        case MapView::ToolType::Fill:
        case MapView::ToolType::Rect:
        case MapView::ToolType::Line:         m_modeLabel->setText("Mode: Tiles");   m_modeLabel->setStyleSheet("color:#9aa0a8; font-weight:bold; padding:0 8px;"); break;
        case MapView::ToolType::GameObject:
        case MapView::ToolType::Trigger:
        case MapView::ToolType::Camera:       m_modeLabel->setText("Mode: Objects"); m_modeLabel->setStyleSheet("color:#caa15a; font-weight:bold; padding:0 8px;"); break;
        }
    };
    connect(m_mapView, &MapView::toolChanged, this, updateModeLabel);
    updateModeLabel(m_mapView->currentTool());

    // Connect zoom combo ↔ MapView
    m_mapView->setZoom(1.25);
    connect(m_zoomCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        bool ok = false;
        double pct = text.trimmed().replace("%", "").toDouble(&ok);
        if (ok && pct > 0.0)
            m_mapView->setZoom(pct / 100.0);
    });
    connect(m_mapView, &MapView::zoomChanged, this, [this](double zoom) {
        m_zoomCombo->blockSignals(true);
        m_zoomCombo->setCurrentText(QString("%1%").arg(qRound(zoom * 100.0)));
        m_zoomCombo->blockSignals(false);
    });

    // Connect m_goTypeList → MapView
    connect(m_goTypeList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *cur, QListWidgetItem *) {
                if (cur) m_mapView->setObjectType(cur->text());
            });
    if (m_goTypeList->currentItem())
        m_mapView->setObjectType(m_goTypeList->currentItem()->text());

    m_tilesetPanel->setSpritesheet(m_mapView->spritesheetImage(),
                                   m_mapView->atlasTileSize(),
                                   m_mapView->atlasCols());

    // Save procedural spritesheet so it can be replaced with a real PNG
    QString assetsPath = QCoreApplication::applicationDirPath() + "/assets";
    QDir().mkpath(assetsPath);
    m_mapView->spritesheetImage().save(assetsPath + "/spritesheet.png");

    refreshExplorerTree();
    refreshRoomTabs();
    switchToRoom(0);
    m_mapView->setTool(MapView::ToolType::Brush);
    setActiveLayer(1);
    updateWindowTitle();
}

MainWindow::~MainWindow() {
    // Prevent QUndoStack::indexChanged from firing after m_world is gone
    // (the stack emits indexChanged during its own destruction, which would
    // trigger refreshExplorerTree with a dangling m_world pointer)
    if (m_undoStack)
        disconnect(m_undoStack, &QUndoStack::indexChanged, this, &MainWindow::refreshExplorerTree);

    delete m_goDefManager;
    m_goDefManager = nullptr;
    delete m_world;
    m_world = nullptr;
}

// ─── Room management ───────────────────────────────────────────────────────

void MainWindow::addRoom() {
    QString base = "Room_";
    int n = m_world->roomCount() + 1;
    QString name = base + QString("%1").arg(n, 3, 10, QChar('0'));

    Room *room = m_world->addRoom(name.toStdString());
    room->addLayer("Background");
    room->addLayer("Ground");
    room->addLayer("Foreground");

    // Auto-position to the right of the last room
    if (m_world->roomCount() >= 2) {
        Room *prev = m_world->room(m_world->roomCount() - 2);
        room->setWorldX(prev->worldX() + prev->pixelWidth() + 64);
        room->setWorldY(prev->worldY());
    }

    refreshExplorerTree();
    refreshRoomTabs();
    switchToRoom(m_world->roomCount() - 1);
}

void MainWindow::removeRoom(int index) {
    if (m_world->roomCount() <= 1) return;
    if (index < 0 || index >= m_world->roomCount()) return;

    m_world->removeRoom(index);

    if (m_activeRoomIndex >= m_world->roomCount())
        m_activeRoomIndex = m_world->roomCount() - 1;

    refreshExplorerTree();
    refreshRoomTabs();
    switchToRoom(m_activeRoomIndex);
}

void MainWindow::switchToRoom(int index) {
    if (!m_world || index < 0 || index >= m_world->roomCount()) return;

    m_activeRoomIndex = index;

    m_roomTabs->setCurrentIndex(index);
    m_mapView->setActiveRoomIndex(index);
    // Center view on the room's world position
    Room *room = m_world->room(index);
    if (room)
        m_mapView->centerOn(room->worldX() + room->pixelWidth() / 2.0f,
                            room->worldY() + room->pixelHeight() / 2.0f);
    updateRoomSizeLabel();
    updateRoomProperties();
    updateLayerLabel();
    updateWindowTitle();
}

void MainWindow::refreshRoomTabs() {
    // Block signals while rebuilding tabs
    m_roomTabs->blockSignals(true);

    int current = m_roomTabs->currentIndex();
    m_roomTabs->clear();

    for (int i = 0; i < m_world->roomCount(); ++i) {
        Room *r = m_world->room(i);
        m_roomTabs->addTab(new QWidget(),
                           QString::fromStdString(r->name()));
    }

    if (current >= 0 && current < m_roomTabs->count())
        m_roomTabs->setCurrentIndex(current);

    m_roomTabs->blockSignals(false);
}

// ─── Layer selection ───────────────────────────────────────────────────────

void MainWindow::setActiveLayer(int index) {
    m_activeLayerIndex = index;
    m_mapView->setActiveLayerIndex(index);
    updateLayerLabel();
}

void MainWindow::updateLayerLabel() {
    Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
    if (!room || m_activeLayerIndex < 0 || m_activeLayerIndex >= room->layerCount()) {
        m_layerLabel->setText("Layer: (none)");
        return;
    }
    m_layerLabel->setText(QString("Layer: %1")
        .arg(QString::fromStdString(room->layer(m_activeLayerIndex)->name())));
    updateLayerTilesetField();
}

void MainWindow::updateLayerTilesetField() {
    Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
    Layer *layer = room ? room->layer(m_activeLayerIndex) : nullptr;
    if (layer) {
        QString ts = QString::fromStdString(layer->tileset());
        if (ts.isEmpty())
            ts = "(procedural)";
        else
            ts = QFileInfo(ts).fileName();
        m_layerTilesetEdit->setText(ts);
    } else {
        m_layerTilesetEdit->setText("—");
    }
}

void MainWindow::setTilesetOnAllLayers(const QString &path) {
    if (!m_world) return;
    QString relPath = path;
    if (!m_currentFilePath.isEmpty()) {
        QString projectDir = QFileInfo(m_currentFilePath).absolutePath();
        relPath = QDir(projectDir).relativeFilePath(path);
    }
    int count = 0;
    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *r = m_world->room(ri);
        if (!r) continue;
        for (int li = 0; li < r->layerCount(); ++li) {
            Layer *l = r->layer(li);
            if (l) { l->setTileset(relPath.toStdString()); ++count; }
        }
    }
    updateLayerTilesetField();
    statusBar()->showMessage(
        QString("Tileset set on %1 layers: %2").arg(count).arg(relPath), 5000);
}

void MainWindow::updateRoomSizeLabel() {
    Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
    if (room) {
        m_roomSizeLabel->setText(
            QString("Room: %1x%2 (%3x%4 px)")
                .arg(room->width()).arg(room->height())
                .arg(room->pixelWidth()).arg(room->pixelHeight()));
    }
}

void MainWindow::updateRoomProperties() {
    Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
    if (!room) return;

    if (m_roomNameEdit) {
        m_roomNameEdit->blockSignals(true);
        m_roomNameEdit->setText(QString::fromStdString(room->name()));
        m_roomNameEdit->blockSignals(false);
    }
    if (m_roomWidth) {
        m_roomWidth->blockSignals(true);
        m_roomWidth->setValue(room->width());
        m_roomWidth->blockSignals(false);
    }
    if (m_roomHeight) {
        m_roomHeight->blockSignals(true);
        m_roomHeight->setValue(room->height());
        m_roomHeight->blockSignals(false);
    }
    if (m_roomPosX && m_roomPosY) {
        m_roomPosX->blockSignals(true);
        m_roomPosY->blockSignals(true);
        m_roomPosX->setValue(room->worldX());
        m_roomPosY->setValue(room->worldY());
        m_roomPosX->blockSignals(false);
        m_roomPosY->blockSignals(false);
    }
}

// ─── Icon helpers ──────────────────────────────────────────────────────────

QIcon MainWindow::makeEyeIcon(bool visible) {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    if (visible) {
        p.setPen(QPen(QColor("#9aa0a8"), 1.5));
        p.setBrush(QColor("#9aa0a8"));
        p.drawEllipse(QPointF(8, 8), 5, 4);
        p.setPen(QPen(pix.devicePixelRatio() > 1 ? Qt::black : QColor("#2d3038"), 1.5));
        p.drawEllipse(QPointF(8, 8), 2, 1.5);
    } else {
        p.setPen(QPen(QColor("#5a5f6a"), 1.5));
        p.drawLine(2, 2, 14, 14);
        p.drawLine(14, 2, 2, 14);
        p.setPen(QPen(QColor("#5a5f6a"), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(8, 8), 5, 4);
    }
    p.end();
    return QIcon(pix);
}

QIcon MainWindow::makeLockIcon(bool locked) {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    if (locked) {
        p.setPen(QPen(QColor("#e8c547"), 1.5));
        p.setBrush(QColor("#e8c547"));
        p.drawRoundedRect(3, 7, 10, 8, 2, 2);
        p.setPen(QPen(QColor("#e8c547"), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawArc(5, 2, 6, 6, 180 * 16, -180 * 16);
    } else {
        p.setPen(QPen(QColor("#5a5f6a"), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(3, 7, 10, 8, 2, 2);
        p.drawArc(5, 2, 6, 6, 180 * 16, -180 * 16);
    }
    p.end();
    return QIcon(pix);
}

void MainWindow::updateLayerTreeItem(QTreeWidgetItem *item, int roomIdx, int layerIdx) {
    Room *room = m_world ? m_world->room(roomIdx) : nullptr;
    Layer *layer = room ? room->layer(layerIdx) : nullptr;
    if (!item || !layer) return;
    item->setIcon(kColVisible, makeEyeIcon(layer->visible()));
    item->setIcon(kColLocked, makeLockIcon(layer->locked()));
}

// ─── Explorer tree ─────────────────────────────────────────────────────────

void MainWindow::refreshExplorerTree() {
    m_explorerTree->blockSignals(true);
    m_explorerTree->clear();
    if (!m_world) { m_explorerTree->blockSignals(false); return; }

    auto *worldItem = new QTreeWidgetItem(m_explorerTree);
    worldItem->setText(kColName, QString::fromStdString("World: " + m_world->name()));

    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *room = m_world->room(ri);
        auto *roomItem = new QTreeWidgetItem(worldItem);
        roomItem->setText(kColName, QString::fromStdString(room->name()));
        roomItem->setData(0, Qt::UserRole + 1, Tag_Room);
        roomItem->setData(0, Qt::UserRole + 2, ri);
        // Room eye/lock (layer-like toggles)
        roomItem->setIcon(kColVisible, makeEyeIcon(true));
        roomItem->setIcon(kColLocked, makeLockIcon(false));

        // Layers
        auto *layersItem = new QTreeWidgetItem(roomItem);
        layersItem->setText(kColName, "Layers");
        layersItem->setData(0, Qt::UserRole + 1, Tag_Container);
        for (int li = 0; li < room->layerCount(); ++li) {
            Layer *layer = room->layer(li);
            auto *layerItem = new QTreeWidgetItem(layersItem);
            layerItem->setText(kColName, QString::fromStdString(layer->name()));
            layerItem->setData(0, Qt::UserRole + 1, Tag_Layer);
            layerItem->setData(0, Qt::UserRole + 2, li);
            layerItem->setIcon(kColVisible, makeEyeIcon(layer->visible()));
            layerItem->setIcon(kColLocked, makeLockIcon(layer->locked()));
        }

        // Game objects
        auto *goItem = new QTreeWidgetItem(roomItem);
        goItem->setText(kColName, "GameObjects");
        goItem->setData(0, Qt::UserRole + 1, Tag_Container);
        for (int li = 0; li < room->layerCount(); ++li) {
            Layer *layer = room->layer(li);
            for (const auto &obj : layer->objects()) {
                auto *objItem = new QTreeWidgetItem(goItem);
                objItem->setText(kColName, QString::fromStdString(obj.name));
                objItem->setData(0, Qt::UserRole + 1, Tag_GameObject);
                QVariantList v; v << li << (qlonglong)obj.id;
                objItem->setData(0, Qt::UserRole + 2, v);
            }
        }

        worldItem->setExpanded(true);
        roomItem->setExpanded(true);
        layersItem->setExpanded(true);
        goItem->setExpanded(true);
    }

    // Highlight and select active layer
    for (int i = 0; i < worldItem->childCount(); ++i) {
        QTreeWidgetItem *ri = worldItem->child(i);
        if (ri->data(0, Qt::UserRole + 2).toInt() != m_activeRoomIndex) continue;
        for (int j = 0; j < ri->childCount(); ++j) {
            QTreeWidgetItem *container = ri->child(j);
            if (container->data(0, Qt::UserRole + 1).toInt() != Tag_Container) continue;
            for (int k = 0; k < container->childCount(); ++k) {
                QTreeWidgetItem *li = container->child(k);
                bool active = (li->data(0, Qt::UserRole + 2).toInt() == m_activeLayerIndex);
                QFont f = li->font(kColName);
                f.setBold(active);
                li->setFont(kColName, f);
                if (active) {
                    li->setBackground(kColName, QColor("#e8f0fe"));
                    m_explorerTree->setCurrentItem(li);
                } else {
                    li->setBackground(kColName, QBrush());
                }
            }
        }
        break;
    }

    // Repaint the map view to reflect model changes (e.g. after undo/redo)
    if (m_mapView) m_mapView->update();
    m_explorerTree->blockSignals(false);
}

// ─── Signal connections ────────────────────────────────────────────────────

void MainWindow::connectTools() {
    QToolBar *bar = findChild<QToolBar*>();
    if (!bar) return;

    auto setTool = [this](MapView::ToolType t) { m_mapView->setTool(t); };

    for (QAction *a : bar->actions()) {
        const QString t = a->text();
        if (t == "Select")      connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Select); });
        else if (t == "Brush")  connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Brush); });
        else if (t == "Erase")  connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Erase); });
        else if (t == "Fill")   connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Fill); });
        else if (t == "Rect")   connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Rect); });
        else if (t == "Line")   connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Line); });
        else if (t == "GameObject") connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::GameObject); });
        else if (t == "Trigger")    connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Trigger); });
        else if (t == "Camera")     connect(a, &QAction::triggered, this, [setTool]{ setTool(MapView::ToolType::Camera); });
        else if (t == "Undo")       connect(a, &QAction::triggered, m_undoStack, &QUndoStack::undo);
        else if (t == "Redo")       connect(a, &QAction::triggered, m_undoStack, &QUndoStack::redo);
        else if (t == "Snap")       connect(a, &QAction::toggled, m_mapView, &MapView::setSnapEnabled);
    }

    connect(m_mapView, &MapView::objectSelected, this, &MainWindow::updatePropertiesForObject);

    // Room selection in map view → update properties
    connect(m_mapView, &MapView::roomSelected, this, [this](int idx) {
        if (idx >= 0) {
            m_activeRoomIndex = idx;
            m_roomTabs->blockSignals(true);
            m_roomTabs->setCurrentIndex(idx);
            m_roomTabs->blockSignals(false);
            updateRoomProperties();
            updateRoomSizeLabel();
            updateLayerLabel();
            updateWindowTitle();
        }
    });
}

void MainWindow::connectSignals() {
    // Room tab switching
    connect(m_roomTabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx >= 0 && idx < m_world->roomCount())
            switchToRoom(idx);
    });

    // Room tab close
    connect(m_roomTabs, &QTabWidget::tabCloseRequested, this, [this](int idx) {
        removeRoom(idx);
    });

    // Add room button (corner of tab widget)
    QToolButton *addRoomCorner = qobject_cast<QToolButton*>(m_roomTabs->cornerWidget(Qt::TopRightCorner));
    if (addRoomCorner) {
        connect(addRoomCorner, &QToolButton::clicked, this, &MainWindow::addRoom);
    }

    // Add room button in explorer
    if (m_addRoomBtn) {
        connect(m_addRoomBtn, &QToolButton::clicked, this, &MainWindow::addRoom);
    }

    // Delete room button in explorer
    if (m_delRoomBtn) {
        connect(m_delRoomBtn, &QToolButton::clicked, this, [this]() {
            removeRoom(m_activeRoomIndex);
        });
    }

    // Tree selection → active layer / room switch
    connect(m_explorerTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                if (!cur) return;
                int tag = cur->data(0, Qt::UserRole + 1).toInt();

                if (tag == Tag_Room) {
                    int idx = cur->data(0, Qt::UserRole + 2).toInt();
                    switchToRoom(idx);
                }
                else if (tag == Tag_Layer) {
                    int idx = cur->data(0, Qt::UserRole + 2).toInt();
                    setActiveLayer(idx);
                }
                else if (tag == Tag_GameObject) {
                    QVariantList v = cur->data(0, Qt::UserRole + 2).toList();
                    if (v.size() == 2) {
                        int layerIdx = v[0].toInt();
                        int64_t objId = v[1].toLongLong();
                        updatePropertiesForObject(objId, layerIdx);
                        m_mapView->selectObject(objId, layerIdx);
                    }
                }
            });

    // Tree eye/lock click → toggle layer visibility / lock
    connect(m_explorerTree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int column) {
                if (!item) return;
                int tag = item->data(0, Qt::UserRole + 1).toInt();
                if (tag != Tag_Layer) return;
                int ri = -1, li = item->data(0, Qt::UserRole + 2).toInt();
                // Find which room this layer belongs to
                QTreeWidgetItem *parent = item->parent() ? item->parent()->parent() : nullptr;
                if (parent)
                    ri = parent->data(0, Qt::UserRole + 2).toInt();

                Room *room = (ri >= 0 && m_world) ? m_world->room(ri) : nullptr;
                Layer *layer = room ? room->layer(li) : nullptr;
                if (!layer) return;

                if (column == kColVisible) {
                    layer->setVisible(!layer->visible());
                    item->setIcon(kColVisible, makeEyeIcon(layer->visible()));
                    m_mapView->update();
                } else if (column == kColLocked) {
                    layer->setLocked(!layer->locked());
                    item->setIcon(kColLocked, makeLockIcon(layer->locked()));
                }
            });

    // Room property editing
    if (m_roomNameEdit) {
        connect(m_roomNameEdit, &QLineEdit::editingFinished, this, [this]() {
            Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
            if (room) {
                room->setName(m_roomNameEdit->text().toStdString());
                refreshRoomTabs();
                refreshExplorerTree();
                updateWindowTitle();
                m_mapView->update();
            }
        });
    }
    if (m_roomWidth) {
        connect(m_roomWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
            if (room) {
                room->resize(val, room->height());
                updateRoomSizeLabel();
                m_mapView->update();
            }
        });
    }
    if (m_roomHeight) {
        connect(m_roomHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
            if (room) {
                room->resize(room->width(), val);
                updateRoomSizeLabel();
                m_mapView->update();
            }
        });
    }
    if (m_roomPosX && m_roomPosY) {
        connect(m_roomPosX, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
            if (room) {
                room->setWorldX(val);
                m_mapView->update();
                updateWindowTitle();
            }
        });
        connect(m_roomPosY, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
            Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
            if (room) {
                room->setWorldY(val);
                m_mapView->update();
                updateWindowTitle();
            }
        });
    }

    // Tileset selection → assign sprite to selected object or set brush tile
    connect(m_tilesetPanel, &TilesetPanel::tileSelected, this,
            [this](int index) {
                if (m_mapView->currentTool() == MapView::ToolType::Select &&
                    m_mapView->hasSelectedObject()) {
                    m_mapView->assignObjectSprite(index);
                } else {
                    m_selectedTileId = index;
                    m_mapView->setSelectedTileId(index);
                }
            });

    // Layer tileset browse button
    connect(m_layerTilesetBtn, &QToolButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Import Spritesheet for Layer",
            QString(), "Images (*.png *.bmp *.jpg *.jpeg);;All Files (*)");
        if (path.isEmpty()) return;
        int ts = m_world ? m_world->defaultTileSize() : 32;
        m_mapView->setAtlasTileSize(ts);
        if (!m_mapView->loadSpritesheet(path)) {
            QMessageBox::warning(this, "Error", "Failed to load spritesheet.");
            return;
        }
        QFileInfo fi(path);
        if (m_tilesetCombo) {
            m_tilesetCombo->blockSignals(true);
            m_tilesetCombo->clear();
            m_tilesetCombo->addItem(fi.fileName());
            m_tilesetCombo->blockSignals(false);
        }
        m_tilesetPanel->setSpritesheet(m_mapView->spritesheetImage(),
                                       m_mapView->atlasTileSize(),
                                       m_mapView->atlasCols());
        setTilesetOnAllLayers(path);
    });
}

// ─── Properties update ─────────────────────────────────────────────────────

void MainWindow::updatePropertiesForObject(int64_t id, int layerIndex) {
    if (id < 0) {
        m_propNameEdit->clear();
        m_propIdEdit->clear();
        m_propPosX->setValue(0);
        m_propPosY->setValue(0);
        m_tileInfoId->setText("—");
        m_tileInfoSprite->setText("—");
        m_tileInfoDef->setText("—");
        return;
    }
    Room *room = m_world ? m_world->room(m_activeRoomIndex) : nullptr;
    Layer *layer = room ? room->layer(layerIndex) : nullptr;
    if (!layer) return;

    GameObject *obj = layer->object(id);
    if (!obj) return;

    m_propNameEdit->setText(QString::fromStdString(obj->name));
    m_propIdEdit->setText(QString::number(obj->id));
    m_propPosX->setValue((int)obj->x);
    m_propPosY->setValue((int)obj->y);
    m_tileInfoId->setText(QString::number(id));

    auto it = obj->properties.find("spriteId");
    if (it != obj->properties.end() && std::atoi(it->second.c_str()) >= 0)
        m_tileInfoSprite->setText(QString("tile #%1").arg(QString::fromStdString(it->second)));
    else
        m_tileInfoSprite->setText("—");

    // Show GameObjectDef info
    if (m_goDefManager) {
        const GameObjectDef *def = m_goDefManager->definition(
            QString::fromStdString(obj->type));
        if (def)
            m_tileInfoDef->setText(QString("%1 (%2,%3 %4x%5)")
                .arg(def->textureFile)
                .arg(def->spriteRect.x()).arg(def->spriteRect.y())
                .arg(def->spriteRect.width()).arg(def->spriteRect.height()));
        else
            m_tileInfoDef->setText("—");
    } else {
        m_tileInfoDef->setText("—");
    }
}

// ─── File I/O ──────────────────────────────────────────────────────────────

void MainWindow::newProject() {
    GameObject::resetIdCounter();
    delete m_world;
    m_world = new World("Untitled");
    Room *room = m_world->addRoom("Room_001");
    room->addLayer("Background");
    room->addLayer("Ground");
    room->addLayer("Foreground");
    m_currentFilePath.clear();

    m_mapView->setWorld(m_world);
    loadTilesetFromWorld();
    refreshExplorerTree();
    refreshRoomTabs();
    switchToRoom(0);
    setActiveLayer(1);
    updateWindowTitle();
    statusBar()->showMessage("New project created", 3000);
}

void MainWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, "Open Project",
        QString(), "SnapLevel Project (*.snap);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    QString error;
    auto world = WorldSerializer::loadFromFile(path, &error);
    if (!world) {
        QMessageBox::warning(this, "Open Failed", error);
        return;
    }

    delete m_world;
    m_world = world.release();
    m_currentFilePath = path;

    m_mapView->setWorld(m_world);

    // Load tileset from the first layer that has one
    loadTilesetFromWorld();

    // Select first room
    refreshExplorerTree();
    refreshRoomTabs();
    switchToRoom(0);
    setActiveLayer(m_world->room(0)->layerCount() > 1 ? 1 : 0);
    updateWindowTitle();
    statusBar()->showMessage("Opened " + QFileInfo(path).fileName(), 3000);
}

void MainWindow::saveProject() {
    if (m_currentFilePath.isEmpty()) {
        saveProjectAs();
        return;
    }
    makeTilesetPathsRelative();
    // Debug: check tileset paths before save
    int tsCount = 0;
    for (int ri = 0; ri < m_world->roomCount(); ++ri)
        if (auto *r = m_world->room(ri))
            for (int li = 0; li < r->layerCount(); ++li)
                if (auto *l = r->layer(li))
                    if (!l->tileset().empty()) ++tsCount;
    QString error;
    if (!WorldSerializer::saveToFile(*m_world, m_currentFilePath, &error)) {
        QMessageBox::warning(this, "Save Failed", error);
        return;
    }
    statusBar()->showMessage(
        QString("Saved – %1 layers have tileset path").arg(tsCount), 3000);
    updateWindowTitle();
}

bool MainWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save Project As",
        QString(), "SnapLevel Project (*.snap);;JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return false;

    m_currentFilePath = path;
    saveProject();
    return true;
}

void MainWindow::updateWindowTitle() {
    QString title = "SnapLevel Editor";
    if (m_world) {
        title += " - " + QString::fromStdString(m_world->name());
        Room *room = m_world->room(m_activeRoomIndex);
        if (room) title += " [" + QString::fromStdString(room->name()) + "]";
    }
    if (!m_currentFilePath.isEmpty())
        title += " (" + QFileInfo(m_currentFilePath).fileName() + ")";
    setWindowTitle(title);
}

void MainWindow::makeTilesetPathsRelative() {
    if (!m_world || m_currentFilePath.isEmpty()) return;
    QString projectDir = QFileInfo(m_currentFilePath).absolutePath();
    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *r = m_world->room(ri);
        if (!r) continue;
        for (int li = 0; li < r->layerCount(); ++li) {
            Layer *l = r->layer(li);
            if (!l) continue;
            QString path = QString::fromStdString(l->tileset());
            if (!path.isEmpty() && QFileInfo(path).isAbsolute()) {
                QString rel = QDir(projectDir).relativeFilePath(path);
                l->setTileset(rel.toStdString());
            }
        }
    }
}

void MainWindow::loadTilesetFromWorld() {
    if (!m_world) return;
    // Find the first layer with a tileset path and load it
    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *r = m_world->room(ri);
        if (!r) continue;
        for (int li = 0; li < r->layerCount(); ++li) {
            Layer *l = r->layer(li);
            if (!l) continue;
            std::string ts = l->tileset();
            if (!ts.empty()) {
                QString path = QString::fromStdString(ts);
                // Resolve relative path against project directory
                if (QFileInfo(path).isRelative() && !m_currentFilePath.isEmpty())
                    path = QDir(QFileInfo(m_currentFilePath).absolutePath()).absoluteFilePath(path);
                if (QFile::exists(path)) {
                    int tileSize = m_world->defaultTileSize();
                    m_mapView->setAtlasTileSize(tileSize);
                    m_mapView->loadSpritesheet(path);
                    m_tilesetPanel->setSpritesheet(m_mapView->spritesheetImage(),
                                                   m_mapView->atlasTileSize(),
                                                   m_mapView->atlasCols());
                    if (m_tilesetCombo) {
                        m_tilesetCombo->blockSignals(true);
                        m_tilesetCombo->clear();
                        m_tilesetCombo->addItem(QFileInfo(path).fileName());
                        m_tilesetCombo->blockSignals(false);
                    }
                    return;
                }
            }
        }
    }
    // No tileset found — reset to procedural
    int tileSize = m_world->defaultTileSize();
    m_mapView->setAtlasTileSize(tileSize);
    m_tilesetPanel->setSpritesheet(m_mapView->spritesheetImage(),
                                   m_mapView->atlasTileSize(),
                                   m_mapView->atlasCols());
    if (m_tilesetCombo) {
        m_tilesetCombo->blockSignals(true);
        m_tilesetCombo->clear();
        m_tilesetCombo->addItem("(procedural)");
        m_tilesetCombo->blockSignals(false);
    }
}

// ─── Menu bar ──────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar() {
    QMenuBar *bar = menuBar();

    QMenu *fileMenu = bar->addMenu("File");
    fileMenu->addAction("New Project",   QKeySequence::New,            this, &MainWindow::newProject);
    fileMenu->addAction("Open...",       QKeySequence::Open,           this, &MainWindow::openProject);
    fileMenu->addSeparator();
    fileMenu->addAction("Save",          QKeySequence::Save,           this, &MainWindow::saveProject);
    fileMenu->addAction("Save As...",    QKeySequence("Ctrl+Shift+S"), this, &MainWindow::saveProjectAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Import Spritesheet...", QKeySequence("Ctrl+I"), this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Import Spritesheet",
            QString(), "Images (*.png *.bmp *.jpg *.jpeg);;All Files (*)");
        if (!path.isEmpty()) {
            int ts = m_world ? m_world->defaultTileSize() : 32;
            m_mapView->setAtlasTileSize(ts);
            if (!m_mapView->loadSpritesheet(path))
                QMessageBox::warning(this, "Error", "Failed to load spritesheet.");
            else {
                QFileInfo fi(path);
                if (m_tilesetCombo) {
                    m_tilesetCombo->blockSignals(true);
                    m_tilesetCombo->clear();
                    m_tilesetCombo->addItem(fi.fileName());
                    m_tilesetCombo->blockSignals(false);
                }
                m_tilesetPanel->setSpritesheet(m_mapView->spritesheetImage(),
                                               m_mapView->atlasTileSize(),
                                               m_mapView->atlasCols());
                setTilesetOnAllLayers(path);
            }
        }
    });
    fileMenu->addAction("Import GameObject Definitions...", QKeySequence("Ctrl+G"), this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Import GameObject Definitions",
            QString(), "JSON (*.json);;All Files (*)");
        if (!path.isEmpty()) {
            if (!m_goDefManager->loadDefinitions(path))
                QMessageBox::warning(this, "Error", "Failed to load GameObject definitions.");
            else {
                // Check that at least one texture file exists
                QStringList missing;
                for (const QString &t : m_goDefManager->uniqueTextureFiles()) {
                    QString resolved = QFileInfo(QFileInfo(path).absolutePath() + "/" + t).absoluteFilePath();
                    if (!QFile::exists(resolved) && !QFile::exists(t))
                        missing << t;
                }
                if (!missing.isEmpty())
                    QMessageBox::warning(this, "Texture Not Found",
                        QString("Texture file(s) not found:\n%1\n\n"
                                "Place the texture file next to the JSON definition file.")
                            .arg(missing.join("\n")));

                m_mapView->setGameObjectDefManager(m_goDefManager);
                populateGameObjectList();
                QMessageBox::information(this, "Success",
                    QString("Loaded %1 GameObject definitions.").arg(m_goDefManager->definitions().size()));
            }
        }
    });
    fileMenu->addSeparator();
    fileMenu->addAction("Quit",          QKeySequence::Quit,           qApp, &QApplication::quit);

    QAction *recent = fileMenu->addAction("Recent Files");
    recent->setEnabled(false);

    // Sync menu/toolbar New & Save actions
    QToolBar *tb = findChild<QToolBar*>();
    if (tb) {
        for (QAction *a : tb->actions()) {
            const QString t = a->text();
            if (t == "New")     connect(a, &QAction::triggered, this, &MainWindow::newProject);
            else if (t == "Open")    connect(a, &QAction::triggered, this, &MainWindow::openProject);
            else if (t == "Save")    connect(a, &QAction::triggered, this, &MainWindow::saveProject);
            else if (t == "Save As") connect(a, &QAction::triggered, this, &MainWindow::saveProjectAs);
        }
    }

    // Edit menu with real Undo/Redo
    {
        QMenu *menu = bar->addMenu("Edit");
        menu->addAction("Undo", QKeySequence::Undo, m_undoStack, &QUndoStack::undo);
        menu->addAction("Redo", QKeySequence::Redo, m_undoStack, &QUndoStack::redo);
    }
    for (const QString &name : {"View", "Room", "Layer",
                                "Tools", "GameObject", "Build", "Help"}) {
        QMenu *menu = bar->addMenu(name);
        menu->addAction(name + " (coming soon)");
    }
}

// ─── Tool bar ──────────────────────────────────────────────────────────────

static QAction *addToolAction(QToolBar *bar, QStyle::StandardPixmap icon,
                               const QString &text, bool checkable,
                               QActionGroup *group = nullptr) {
    QAction *a = new QAction(bar->style()->standardIcon(icon), text, bar);
    a->setCheckable(checkable);
    if (group) group->addAction(a);
    bar->addAction(a);
    return a;
}

void MainWindow::setupToolBar() {
    QToolBar *bar = addToolBar("Main");
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    bar->setIconSize(QSize(22, 22));

    addToolAction(bar, QStyle::SP_FileIcon, "New", false);
    addToolAction(bar, QStyle::SP_DialogOpenButton, "Open", false);
    addToolAction(bar, QStyle::SP_DialogSaveButton, "Save", false);
    addToolAction(bar, QStyle::SP_DriveFDIcon, "Save As", false);
    bar->addSeparator();
    addToolAction(bar, QStyle::SP_ArrowBack, "Undo", false);
    addToolAction(bar, QStyle::SP_ArrowForward, "Redo", false);
    bar->addSeparator();

    auto *tools = new QActionGroup(this);
    tools->setExclusive(true);
    addToolAction(bar, QStyle::SP_FileDialogDetailedView, "Select", true, tools);
    bar->addSeparator();

    auto *tilesLabel = new QLabel("  Tiles"); tilesLabel->setStyleSheet("color:#9aa0a8; font-weight:bold;");
    bar->addWidget(tilesLabel);
    QAction *brush = addToolAction(bar, QStyle::SP_DialogResetButton, "Brush", true, tools);
    addToolAction(bar, QStyle::SP_TrashIcon, "Erase", true, tools);
    addToolAction(bar, QStyle::SP_DialogApplyButton, "Fill", true, tools);
    addToolAction(bar, QStyle::SP_FileDialogContentsView, "Rect", true, tools);
    addToolAction(bar, QStyle::SP_FileDialogInfoView, "Line", true, tools);
    brush->setChecked(true);
    bar->addSeparator();

    auto *objLabel = new QLabel("  Objects"); objLabel->setStyleSheet("color:#caa15a; font-weight:bold;");
    bar->addWidget(objLabel);
    addToolAction(bar, QStyle::SP_ComputerIcon, "GameObject", true, tools);
    addToolAction(bar, QStyle::SP_MessageBoxWarning, "Trigger", true, tools);
    addToolAction(bar, QStyle::SP_DesktopIcon, "Camera", true, tools);
    bar->addSeparator();

    auto *snapAction = addToolAction(bar, QStyle::SP_DialogApplyButton, "Snap", true);
    snapAction->setChecked(true);
    bar->addSeparator();

    QAction *play = addToolAction(bar, QStyle::SP_MediaPlay, "Play", false);
    play->setIconText("Play");
}

// ─── Central area ──────────────────────────────────────────────────────────

void MainWindow::setupCentralArea() {
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Room tabs
    m_roomTabs = new QTabWidget();
    m_roomTabs->setTabsClosable(true);
    m_roomTabs->setDocumentMode(true);
    m_roomTabs->setStyleSheet(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { padding: 4px 12px; }");
    m_roomTabs->setFixedHeight(32);
    rootLayout->addWidget(m_roomTabs);

    auto *addRoomBtn = new QToolButton();
    addRoomBtn->setText("+");
    addRoomBtn->setFixedSize(24, 24);
    m_roomTabs->setCornerWidget(addRoomBtn, Qt::TopRightCorner);

    // View toolbar
    auto *viewBar = new QWidget();
    auto *viewBarLayout = new QHBoxLayout(viewBar);
    viewBarLayout->setContentsMargins(8, 4, 8, 4);
    auto *gridBtn = new QToolButton(); gridBtn->setText("#"); gridBtn->setCheckable(true); gridBtn->setChecked(true);
    auto *lockBtn = new QToolButton(); lockBtn->setIcon(style()->standardIcon(QStyle::SP_VistaShield)); lockBtn->setCheckable(true);
    auto *lightBtn = new QToolButton(); lightBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarShadeButton));
    viewBarLayout->addWidget(gridBtn);
    viewBarLayout->addWidget(lockBtn);
    viewBarLayout->addWidget(lightBtn);
    viewBarLayout->addStretch();
    m_zoomCombo = new QComboBox();
    m_zoomCombo->addItems({"50%", "75%", "100%", "125%", "150%", "200%"});
    m_zoomCombo->setCurrentText("125%");
    m_zoomCombo->setFixedWidth(90);
    viewBarLayout->addWidget(m_zoomCombo);
    rootLayout->addWidget(viewBar);

    QFrame *sep1 = new QFrame();
    sep1->setObjectName("separatorLine");
    sep1->setFixedHeight(1);
    rootLayout->addWidget(sep1);

    // MapView
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_mapView = new MapView();
    scroll->setWidget(m_mapView);
    rootLayout->addWidget(scroll, 1);

    connect(m_mapView, &MapView::mouseInfoChanged, this,
            [this](const QString &mouse, const QString &tile) {
                m_mouseLabel->setText(mouse);
                m_tileLabel->setText(tile);
            });

    // Info bar
    auto *infoBar = new QWidget();
    infoBar->setStyleSheet("background-color:#20242b; border-top:1px solid #2c313a;");
    auto *infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(10, 4, 10, 4);
    m_roomSizeLabel = new QLabel();
    m_mouseLabel = new QLabel("Mouse: 0, 0");
    m_tileLabel = new QLabel("Tile: 0, 0");
    m_layerLabel = new QLabel();
    for (QLabel *l : {m_roomSizeLabel, m_mouseLabel, m_tileLabel, m_layerLabel}) {
        l->setStyleSheet("color:#9aa0a8;");
        infoLayout->addWidget(l);
        infoLayout->addSpacing(20);
    }
    infoLayout->addStretch();
    rootLayout->addWidget(infoBar);

    setCentralWidget(central);
}

// ─── Explorer dock ─────────────────────────────────────────────────────────

void MainWindow::setupExplorerDock() {
    auto *dock = new QDockWidget("Explorer", this);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto *panel = new QWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *worldRow = new QHBoxLayout();
    worldRow->addWidget(new QLabel("World"));
    m_worldCombo = new QComboBox();
    m_worldCombo->addItems({"ForestWorld", "CaveWorld", "SkyWorld"});
    worldRow->addWidget(m_worldCombo, 1);
    layout->addLayout(worldRow);

    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText("Filter...");
    layout->addWidget(m_filterEdit);

    m_explorerTree = new QTreeWidget();
    m_explorerTree->setColumnCount(3);
    m_explorerTree->setHeaderLabels({"Name", "", ""});
    m_explorerTree->setColumnWidth(kColVisible, 22);
    m_explorerTree->setColumnWidth(kColLocked, 22);
    m_explorerTree->header()->setStretchLastSection(false);
    m_explorerTree->header()->setSectionResizeMode(kColName, QHeaderView::Stretch);
    m_explorerTree->header()->setMinimumSectionSize(18);
    layout->addWidget(m_explorerTree, 1);

    auto *roomsLabel = new QLabel("Rooms");
    roomsLabel->setObjectName("sectionLabel");
    layout->addWidget(roomsLabel);

    auto *roomsToolbar = new QHBoxLayout();
    m_addRoomBtn = new QToolButton(); m_addRoomBtn->setText("+");
    auto *newFolder = new QToolButton(); newFolder->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_delRoomBtn = new QToolButton(); m_delRoomBtn->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    roomsToolbar->addWidget(m_addRoomBtn);
    roomsToolbar->addWidget(newFolder);
    roomsToolbar->addWidget(m_delRoomBtn);
    roomsToolbar->addStretch();
    layout->addLayout(roomsToolbar);

    auto *thumbGrid = new QWidget();
    thumbGrid->setFixedHeight(120);
    auto *gridLayout = new QHBoxLayout(thumbGrid);
    gridLayout->setSpacing(4);
    const QStringList colors = {"#4caf50", "#3f8e46", "#1f5247", "#2f8fb0"};
    for (int c = 0; c < 4; ++c) {
        auto *col = new QVBoxLayout();
        for (int r = 0; r < 3; ++r) {
            auto *swatch = new QFrame();
            swatch->setFixedSize(24, 24);
            swatch->setStyleSheet(QString("background-color:%1; border-radius:3px;")
                                       .arg(colors[(r + c) % colors.size()]));
            col->addWidget(swatch);
        }
        gridLayout->addLayout(col);
    }
    layout->addWidget(thumbGrid);

    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

// ─── Properties dock ───────────────────────────────────────────────────────

void MainWindow::populateCustomProperties() {
    m_customPropsTable->setRowCount(2);
    m_customPropsTable->setColumnCount(2);
    m_customPropsTable->setHorizontalHeaderLabels({"Name", "Value"});
    m_customPropsTable->setItem(0, 0, new QTableWidgetItem("Value"));
    m_customPropsTable->setItem(0, 1, new QTableWidgetItem("1"));
    m_customPropsTable->setItem(1, 0, new QTableWidgetItem("Pickup"));
    m_customPropsTable->setItem(1, 1, new QTableWidgetItem("True"));
    m_customPropsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_customPropsTable->verticalHeader()->setVisible(false);
}

void MainWindow::setupPropertiesDock() {
    auto *dock = new QDockWidget("Properties", this);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_propTabs = new QTabWidget();

    auto *propsPage = new QWidget();
    auto *propsLayout = new QVBoxLayout(propsPage);

    auto *goGroup = new QGroupBox("GameObject");
    auto *form = new QFormLayout(goGroup);

    m_propNameEdit = new QLineEdit();
    form->addRow("Name", m_propNameEdit);
    m_propIdEdit = new QLineEdit();
    form->addRow("ID", m_propIdEdit);

    auto *posRow = new QHBoxLayout();
    m_propPosX = new QSpinBox(); m_propPosX->setRange(-9999, 9999);
    m_propPosY = new QSpinBox(); m_propPosY->setRange(-9999, 9999);
    posRow->addWidget(new QLabel("X")); posRow->addWidget(m_propPosX);
    posRow->addWidget(new QLabel("Y")); posRow->addWidget(m_propPosY);
    form->addRow("Position", posRow);

    auto *layerCombo = new QComboBox();
    layerCombo->addItems({"Background", "Ground", "Collision", "Decoration", "Foreground"});
    form->addRow("Layer", layerCombo);

    auto *spriteCombo = new QComboBox();
    spriteCombo->addItems({"coin_anim", "player_idle", "goomba_walk"});
    form->addRow("Sprite", spriteCombo);

    auto *animCombo = new QComboBox();
    animCombo->addItems({"Idle", "Spin", "Pickup"});
    form->addRow("Animation", animCombo);

    auto *flipRow = new QHBoxLayout();
    flipRow->addWidget(new QCheckBox("X"));
    flipRow->addWidget(new QCheckBox("Y"));
    flipRow->addStretch();
    form->addRow("Flip", flipRow);

    auto *respawn = new QCheckBox();
    respawn->setChecked(true);
    form->addRow("Respawn", respawn);

    propsLayout->addWidget(goGroup);

    auto *roomGroup = new QGroupBox("Room");
    auto *roomForm = new QFormLayout(roomGroup);
    m_roomNameEdit = new QLineEdit();
    roomForm->addRow("Name", m_roomNameEdit);
    m_roomWidth = new QSpinBox();
    m_roomWidth->setRange(1, 999);
    roomForm->addRow("Width (tiles)", m_roomWidth);
    m_roomHeight = new QSpinBox();
    m_roomHeight->setRange(1, 999);
    roomForm->addRow("Height (tiles)", m_roomHeight);
    m_roomPosX = new QSpinBox();
    m_roomPosX->setRange(-99999, 99999);
    m_roomPosY = new QSpinBox();
    m_roomPosY->setRange(-99999, 99999);
    auto *roomPosRow = new QHBoxLayout();
    roomPosRow->addWidget(new QLabel("X")); roomPosRow->addWidget(m_roomPosX);
    roomPosRow->addWidget(new QLabel("Y")); roomPosRow->addWidget(m_roomPosY);
    roomForm->addRow("World Pos", roomPosRow);
    propsLayout->addWidget(roomGroup);

    // Layer tileset
    auto *tilesetRow = new QHBoxLayout();
    m_layerTilesetEdit = new QLineEdit();
    m_layerTilesetEdit->setReadOnly(true);
    m_layerTilesetEdit->setPlaceholderText("(procedural)");
    tilesetRow->addWidget(m_layerTilesetEdit, 1);
    m_layerTilesetBtn = new QToolButton();
    m_layerTilesetBtn->setText("...");
    m_layerTilesetBtn->setToolTip("Import spritesheet for this layer");
    tilesetRow->addWidget(m_layerTilesetBtn);
    auto *tilesetLabel = new QLabel("Tileset:");
    propsLayout->addWidget(tilesetLabel);
    propsLayout->addLayout(tilesetRow);

    auto *customGroup = new QGroupBox("Custom Properties");
    auto *customLayout = new QVBoxLayout(customGroup);
    m_customPropsTable = new QTableWidget();
    populateCustomProperties();
    customLayout->addWidget(m_customPropsTable);
    customLayout->addWidget(new QPushButton("+  Add Property"));
    propsLayout->addWidget(customGroup);
    propsLayout->addStretch();

    m_propTabs->addTab(propsPage, "Properties");

    auto *tileInfoPage = new QWidget();
    auto *tileInfoLayout = new QFormLayout(tileInfoPage);
    m_tileInfoId = new QLabel("—");
    tileInfoLayout->addRow("Object ID", m_tileInfoId);
    m_tileInfoSprite = new QLabel("—");
    tileInfoLayout->addRow("Sprite Tile", m_tileInfoSprite);
    m_tileInfoDef = new QLabel("—");
    tileInfoLayout->addRow("Definition", m_tileInfoDef);
    m_propTabs->addTab(tileInfoPage, "Tile Info");

    dock->setWidget(m_propTabs);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

// ─── Bottom dock ───────────────────────────────────────────────────────────

void MainWindow::setupBottomDock() {
    auto *dock = new QDockWidget(this);
    dock->setTitleBarWidget(new QWidget());
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_bottomTabs = new QTabWidget();

    // Tileset tab
    auto *tilesetPage = new QWidget();
    auto *tilesetLayout = new QVBoxLayout(tilesetPage);

    auto *toolsRow = new QHBoxLayout();
    m_tilesetCombo = new QComboBox();
    m_tilesetCombo->addItem("(procedural)");
    toolsRow->addWidget(m_tilesetCombo);
    toolsRow->addWidget(new QToolButton());
    auto *gridCheck = new QCheckBox("Grid");
    gridCheck->setChecked(true);
    toolsRow->addWidget(gridCheck);
    toolsRow->addStretch();
    toolsRow->addWidget(new QLabel("Size"));
    auto *sizeCombo = new QComboBox();
    sizeCombo->addItems({"16 x 16", "32 x 32", "64 x 64"});
    // Set current from world's default
    sizeCombo->blockSignals(true);
    if (m_world) {
        int cur = m_world->defaultTileSize();
        int idx = sizeCombo->findText(QString("%1 x %2").arg(cur).arg(cur));
        if (idx >= 0) sizeCombo->setCurrentIndex(idx);
    }
    sizeCombo->blockSignals(false);
    connect(sizeCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        QStringList parts = text.split("x");
        if (parts.size() < 2) return;
        bool ok = false;
        int newSize = parts[0].trimmed().toInt(&ok);
        if (!ok || newSize < 1) return;
        if (!m_world) return;
        m_world->setDefaultTileSize(newSize);
        m_mapView->setAtlasTileSize(newSize);
        // Update all rooms to use the new tile size
        for (int i = 0; i < m_world->roomCount(); ++i) {
            Room *r = m_world->room(i);
            if (r) r->setTileSize(newSize);
        }
        // Refresh tileset panel
        m_tilesetPanel->setSpritesheet(m_mapView->spritesheetImage(),
                                       m_mapView->atlasTileSize(),
                                       m_mapView->atlasCols());
        updateRoomSizeLabel();
        m_mapView->update();
    });
    toolsRow->addWidget(sizeCombo);
    auto *zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setFixedWidth(120);
    zoomSlider->setValue(60);
    toolsRow->addWidget(zoomSlider);
    tilesetLayout->addLayout(toolsRow);

    auto *tilesetScroll = new QScrollArea();
    tilesetScroll->setWidgetResizable(true);
    m_tilesetPanel = new TilesetPanel();
    tilesetScroll->setWidget(m_tilesetPanel);
    tilesetLayout->addWidget(tilesetScroll, 1);

    m_bottomTabs->addTab(tilesetPage, "Tileset");

    // GameObjects tab
    m_goTypeList = new QListWidget();
    m_goTypeList->addItems({"PlayerStart", "Coin", "Enemy_Goomba", "Trigger_Exit", "Camera_Bounds"});
    m_bottomTabs->addTab(m_goTypeList, "GameObjects");

    auto *prefabList = new QListWidget();
    prefabList->addItems({"Platform_Small", "Platform_Large", "Spike_Row", "Crate", "Bush"});
    m_bottomTabs->addTab(prefabList, "Prefabs");

    auto *animList = new QListWidget();
    animList->addItems({"coin_spin", "player_walk", "player_idle", "goomba_walk"});
    m_bottomTabs->addTab(animList, "Animations");

    auto *console = new QPlainTextEdit();
    console->setReadOnly(true);
    console->setPlainText("[Info] Project loaded: ForestWorld\n"
                           "[Info] Room_001 opened\n"
                           "Ready.");
    m_bottomTabs->addTab(console, "Console");

    dock->setWidget(m_bottomTabs);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
}

// ─── Status bar ────────────────────────────────────────────────────────────

void MainWindow::setupStatusBar() {
    statusBar()->showMessage("Ready");
    m_modeLabel = new QLabel("Mode: Tiles");
    m_modeLabel->setStyleSheet("color:#9aa0a8; font-weight:bold; padding:0 8px;");
    statusBar()->addPermanentWidget(m_modeLabel);
    statusBar()->addPermanentWidget(new QLabel("100%"));
    m_snapLabel = new QLabel("Snap: ON");
    statusBar()->addPermanentWidget(m_snapLabel);

    if (m_mapView)
        connect(m_mapView, &MapView::snapEnabledChanged, this, [this](bool on) {
            m_snapLabel->setText(on ? "Snap: ON" : "Snap: OFF");
        });
}

void MainWindow::populateGameObjectList() {
    m_goTypeList->clear();
    if (m_goDefManager && m_goDefManager->hasDefinitions()) {
        for (const QString &name : m_goDefManager->definitionNames())
            m_goTypeList->addItem(name);
    } else {
        m_goTypeList->addItems({"PlayerStart", "Coin", "Enemy_Goomba", "Trigger_Exit", "Camera_Bounds"});
    }
    if (m_goTypeList->count() > 0)
        m_goTypeList->setCurrentRow(0);
}
