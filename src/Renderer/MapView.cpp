#include "MapView.h"
#include "../Model/World.h"
#include "../Model/Room.h"
#include "../Model/Layer.h"
#include "../Model/GameObject.h"
#include "../Model/GameObjectDef.h"
#include "../Commands/UndoCommands.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QUndoStack>
#include <QUndoCommand>
#include <QtMath>
#include <QPainter>
#include <QFileInfo>
#include <QMap>
#include <algorithm>
#include <cstdlib>
#include <stack>

static const char *kVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
out vec2 vTexCoord;
out vec4 vColor;
uniform mat4 uProjView;
void main() {
    gl_Position = uProjView * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

static const char *kFragSrc = R"(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uTexture;
void main() {
    fragColor = texture(uTexture, vTexCoord) * vColor;
}
)";

// ─── Construction ──────────────────────────────────────────────────────────

MapView::MapView(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    generateSpritesheet();
}

MapView::~MapView() {
    makeCurrent();
    delete m_pendingCmd;
    if (m_tileVAO) glDeleteVertexArrays(1, &m_tileVAO);
    if (m_tileVBO) glDeleteBuffers(1, &m_tileVBO);
    if (m_gridVAO) glDeleteVertexArrays(1, &m_gridVAO);
    if (m_gridVBO) glDeleteBuffers(1, &m_gridVBO);
    if (m_overlayVAO) glDeleteVertexArrays(1, &m_overlayVAO);
    if (m_overlayVBO) glDeleteBuffers(1, &m_overlayVBO);
    if (m_program) glDeleteProgram(m_program);
    delete m_spritesheet;
    delete m_whiteTex;
    qDeleteAll(m_defTextures);
    m_defTextures.clear();
    doneCurrent();
}

// ─── Public API ────────────────────────────────────────────────────────────

void MapView::setWorld(World *world) {
    m_world = world;
    if (world && world->defaultTileSize() != m_atlasTileSize) {
        setAtlasTileSize(world->defaultTileSize());
    }
    delete m_pendingCmd;
    m_pendingCmd = nullptr;
    m_leftDown = false;
    m_dragging = false;
    update();
}

Room *MapView::activeRoom() const {
    if (!m_world) return nullptr;
    if (m_activeRoomIndex < 0 || m_activeRoomIndex >= m_world->roomCount())
        return nullptr;
    return m_world->room(m_activeRoomIndex);
}

void MapView::setActiveRoomIndex(int index) {
    m_activeRoomIndex = index;
    m_selectedObjectId = -1;
    m_selectedObjectLayer = -1;
    m_leftDown = false;
    m_dragging = false;
    m_draggingObject = false;
    delete m_pendingCmd;
    m_pendingCmd = nullptr;
    update();
}

void MapView::setTool(ToolType tool) {
    if (m_tool == tool) return;
    m_tool = tool;
    m_leftDown = false;
    m_dragging = false;
    m_draggingObject = false;
    delete m_pendingCmd;
    m_pendingCmd = nullptr;
    // Clear selection when switching away from Select
    if (m_selectedObjectId >= 0) {
        m_selectedObjectId = -1;
        m_selectedObjectLayer = -1;
    }
    m_showPreview = (tool == ToolType::GameObject || tool == ToolType::Trigger || tool == ToolType::Camera);
    // Cursor per mode
    switch (tool) {
    case ToolType::Select:      setCursor(Qt::ArrowCursor); break;
    case ToolType::Brush:
    case ToolType::Erase:
    case ToolType::Fill:
    case ToolType::Rect:
    case ToolType::Line:        setCursor(Qt::CrossCursor); break;
    case ToolType::GameObject:
    case ToolType::Trigger:
    case ToolType::Camera:      setCursor(Qt::PointingHandCursor); break;
    }
    emit toolChanged(tool);
}

void MapView::setActiveLayerIndex(int index) {
    m_activeLayerIndex = index;
    m_selectedObjectId = -1;
    m_selectedObjectLayer = -1;
}

void MapView::setSelectedTileId(int id) {
    m_selectedTileId = id;
}

void MapView::selectObject(int64_t objectId, int layerIndex) {
    m_selectedObjectId = objectId;
    m_selectedObjectLayer = layerIndex;
    update();
}

void MapView::assignObjectSprite(int tileIndex) {
    if (m_selectedObjectId < 0) return;
    Room *room = activeRoom();
    if (!room) return;
    Layer *layer = room->layer(m_selectedObjectLayer);
    if (!layer) return;
    GameObject *obj = layer->object(m_selectedObjectId);
    if (!obj) return;

    auto it = obj->properties.find("spriteId");
    std::string oldVal = (it != obj->properties.end()) ? it->second : std::string();
    std::string newVal = std::to_string(tileIndex);

    auto *cmd = new SetObjectSpriteCommand(layer, m_selectedObjectId, oldVal, newVal);
    if (m_undoStack)
        m_undoStack->push(cmd);
    else {
        cmd->redo();
        delete cmd;
    }
    emit objectsChanged();
}

void MapView::setZoom(double zoom) {
    m_zoom = qBound(0.1, zoom, 10.0);
    updateProjView();
    update();
    emit zoomChanged(m_zoom);
}

void MapView::zoomIn() { setZoom(m_zoom * 1.25); }
void MapView::zoomOut() { setZoom(m_zoom / 1.25); }

void MapView::zoomToFit() {
    if (!m_world || m_world->roomCount() == 0) return;

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (int i = 0; i < m_world->roomCount(); ++i) {
        Room *r = m_world->room(i);
        minX = std::min(minX, (float)r->worldX());
        minY = std::min(minY, (float)r->worldY());
        maxX = std::max(maxX, (float)(r->worldX() + r->pixelWidth()));
        maxY = std::max(maxY, (float)(r->worldY() + r->pixelHeight()));
    }

    double worldW = maxX - minX;
    double worldH = maxY - minY;
    if (worldW == 0 || worldH == 0) return;

    double scaleX = (width() - 48.0) / worldW;
    double scaleY = (height() - 48.0) / worldH;
    m_zoom = qBound(0.1, qMin(scaleX, scaleY), 10.0);

    double cx = (minX + maxX) / 2.0;
    double cy = (minY + maxY) / 2.0;
    m_offset = QVector2D(
        (float)(cx - (double)width() / (2.0 * (double)m_zoom)),
        (float)(cy - (double)height() / (2.0 * (double)m_zoom))
    );
    updateProjView();
    update();
    emit zoomChanged(m_zoom);
}

void MapView::centerOn(float worldX, float worldY) {
    m_offset = QVector2D(
        worldX - (float)width() / (2.0f * (float)m_zoom),
        worldY - (float)height() / (2.0f * (float)m_zoom)
    );
    updateProjView();
    update();
}

void MapView::setAtlasTileSize(int size) {
    if (size < 1) return;
    m_atlasTileSize = size;
    if (!m_spritesheetPath.isEmpty()) {
        // Reload the imported spritesheet with the new tile size
        loadSpritesheet(m_spritesheetPath);
    } else {
        // Regenerate procedural spritesheet with new tile size
        makeCurrent();
        generateSpritesheet();
        if (isValid()) {
            if (m_spritesheet) {
                delete m_spritesheet;
                m_spritesheet = nullptr;
            }
            m_spritesheet = new QOpenGLTexture(m_spritesheetImage);
            m_spritesheet->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
            m_spritesheet->setWrapMode(QOpenGLTexture::ClampToEdge);
        }
        doneCurrent();
    }
    update();
}

bool MapView::loadSpritesheet(const QString &path) {
    QImage img(path);
    if (img.isNull()) return false;

    m_spritesheetImage = img.convertToFormat(QImage::Format_RGBA8888);
    m_spritesheetPath = path;

    // Compute columns from image dimensions / tile size
    if (m_atlasTileSize > 0 && m_spritesheetImage.width() >= m_atlasTileSize)
        m_atlasCols = m_spritesheetImage.width() / m_atlasTileSize;
    else
        m_atlasCols = 16;

    if (isValid()) {
        makeCurrent();
        if (m_spritesheet) {
            delete m_spritesheet;
            m_spritesheet = nullptr;
        }
        m_spritesheet = new QOpenGLTexture(m_spritesheetImage);
        m_spritesheet->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
        m_spritesheet->setWrapMode(QOpenGLTexture::ClampToEdge);
        doneCurrent();
        update();
    }
    return true;
}

// ─── Spritesheet ───────────────────────────────────────────────────────────

void MapView::generateSpritesheet() {
    int rows = 4;
    m_atlasCols = 16;
    int w = m_atlasCols * m_atlasTileSize;
    int h = rows * m_atlasTileSize;
    int tileCount = m_atlasCols * rows;
    m_spritesheetImage = QImage(w, h, QImage::Format_RGBA8888);
    m_spritesheetImage.fill(Qt::transparent);

    QPainter p(&m_spritesheetImage);
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < tileCount; ++i) {
        int col = i % m_atlasCols;
        int row = i / m_atlasCols;
        int x = col * m_atlasTileSize;
        int y = row * m_atlasTileSize;
        QColor c = tileColor(i);
        p.fillRect(x + 1, y + 1, m_atlasTileSize - 2, m_atlasTileSize - 2, c);
        p.setPen(c.darker(150));
        p.drawRect(x, y, m_atlasTileSize - 1, m_atlasTileSize - 1);
    }
    p.end();
}

void MapView::tileUV(int tileId, float &u0, float &v0, float &u1, float &v1) const {
    if (m_spritesheetImage.isNull()) return;
    float tw = (float)m_spritesheetImage.width();
    float th = (float)m_spritesheetImage.height();
    if (tw < 1 || th < 1) return;
    int cols = (int)tw / m_atlasTileSize;
    if (cols < 1) cols = 1;
    int rows = (int)th / m_atlasTileSize;
    int totalTiles = cols * rows;
    if (totalTiles < 1) totalTiles = 1;
    int id = qAbs(tileId) % totalTiles;
    int col = id % cols;
    int row = id / cols;
    u0 = (col * m_atlasTileSize) / tw;
    v0 = (row * m_atlasTileSize) / th;
    u1 = ((col + 1) * m_atlasTileSize) / tw;
    v1 = ((row + 1) * m_atlasTileSize) / th;
}

// ─── OpenGL lifecycle ──────────────────────────────────────────────────────

void MapView::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.118f, 0.157f, 0.188f, 1.0f);

    GLuint vert = compileShader(GL_VERTEX_SHADER, kVertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
    m_program = linkProgram(vert, frag);
    if (vert) glDeleteShader(vert);
    if (frag) glDeleteShader(frag);

    m_uProjView = glGetUniformLocation(m_program, "uProjView");
    m_uTexture = glGetUniformLocation(m_program, "uTexture");

    auto setupVAO = [this](GLuint &vao, GLuint &vbo) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    };

    setupVAO(m_tileVAO, m_tileVBO);
    setupVAO(m_gridVAO, m_gridVBO);
    setupVAO(m_overlayVAO, m_overlayVBO);

    m_spritesheet = new QOpenGLTexture(m_spritesheetImage);
    m_spritesheet->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    m_spritesheet->setWrapMode(QOpenGLTexture::ClampToEdge);

    QImage white(1, 1, QImage::Format_RGBA8888);
    white.fill(Qt::white);
    m_whiteTex = new QOpenGLTexture(white);
    m_whiteTex->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    updateProjView();
}

void MapView::resizeGL(int, int) { updateProjView(); }

void MapView::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (!m_world || m_world->roomCount() == 0) return;

    glUseProgram(m_program);
    glUniformMatrix4fv(m_uProjView, 1, GL_FALSE, m_projView.constData());
    glUniform1i(m_uTexture, 0);

    // Tiles
    QVector<Vertex> tileVerts;
    buildTileVertices(tileVerts);
    if (!tileVerts.isEmpty()) {
        m_spritesheet->bind(0);
        glBindVertexArray(m_tileVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_tileVBO);
        glBufferData(GL_ARRAY_BUFFER, tileVerts.size() * (int)sizeof(Vertex),
                     tileVerts.constData(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, tileVerts.size());
    }

    // Game objects (colored fallback — no sprite)
    QVector<Vertex> objVerts;
    buildObjectVertices(objVerts);
    if (!objVerts.isEmpty()) {
        m_whiteTex->bind(0);
        glBindVertexArray(m_tileVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_tileVBO);
        glBufferData(GL_ARRAY_BUFFER, objVerts.size() * (int)sizeof(Vertex),
                     objVerts.constData(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, objVerts.size());
    }

    // Game objects (sprites)
    QVector<Vertex> objSpriteVerts;
    buildObjectSpriteVertices(objSpriteVerts);
    if (!objSpriteVerts.isEmpty()) {
        m_spritesheet->bind(0);
        glBindVertexArray(m_tileVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_tileVBO);
        glBufferData(GL_ARRAY_BUFFER, objSpriteVerts.size() * (int)sizeof(Vertex),
                     objSpriteVerts.constData(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, objSpriteVerts.size());
    }

    // Game objects (from GameObjectDefManager)
    if (m_goDefMgr && m_goDefMgr->hasDefinitions()) {
        for (const QString &texFile : m_goDefMgr->uniqueTextureFiles()) {
            QVector<Vertex> defVerts;
            buildDefObjectVertices(defVerts, texFile);
            if (defVerts.isEmpty()) continue;

            // Lazily create / cache GL texture for this file
            QOpenGLTexture *tex = m_defTextures.value(texFile);
            if (!tex) {
                const QImage *img = m_goDefMgr->textureImage(texFile);
                if (img && !img->isNull()) {
                    tex = new QOpenGLTexture(*img);
                    tex->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
                    tex->setWrapMode(QOpenGLTexture::ClampToEdge);
                    m_defTextures[texFile] = tex;
                }
            }
            if (!tex) continue;

            tex->bind(0);
            glBindVertexArray(m_tileVAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_tileVBO);
            glBufferData(GL_ARRAY_BUFFER, defVerts.size() * (int)sizeof(Vertex),
                         defVerts.constData(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, defVerts.size());
        }
    }

    // Selection border (on top of everything)
    QVector<Vertex> selVerts;
    buildSelectionBorderVertices(selVerts);
    if (!selVerts.isEmpty()) {
        m_whiteTex->bind(0);
        glBindVertexArray(m_tileVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_tileVBO);
        glBufferData(GL_ARRAY_BUFFER, selVerts.size() * (int)sizeof(Vertex),
                     selVerts.constData(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, selVerts.size());
    }

    // Grid
    QVector<Vertex> gridVerts;
    buildGridVertices(gridVerts);
    if (!gridVerts.isEmpty()) {
        m_whiteTex->bind(0);
        glBindVertexArray(m_gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * (int)sizeof(Vertex),
                     gridVerts.constData(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, gridVerts.size());
    }

    // Overlay
    QVector<Vertex> overlayVerts;
    buildOverlayVertices(overlayVerts);
    if (!overlayVerts.isEmpty()) {
        m_whiteTex->bind(0);
        glBindVertexArray(m_overlayVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_overlayVBO);
        glBufferData(GL_ARRAY_BUFFER, overlayVerts.size() * (int)sizeof(Vertex),
                     overlayVerts.constData(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, overlayVerts.size());
    }

    // Object placement preview (ghost)
    if (m_showPreview) {
        Room *r = activeRoom();
        if (r) {
            QColor ghost = objectColor(m_objectType.isEmpty() ? QString("Coin") : m_objectType);
            ghost.setAlpha(100);
            QVector<Vertex> pv;
            float px = r->worldX() + m_previewWorldPos.x();
            float py = r->worldY() + m_previewWorldPos.y();
            quad(pv, px, py, m_previewSize.width(), m_previewSize.height(), ghost);
            QColor border = ghost.lighter(150);
            border.setAlpha(180);
            quad(pv, px - 1, py - 1, m_previewSize.width() + 2, m_previewSize.height() + 2, border);
            if (!pv.isEmpty()) {
                m_whiteTex->bind(0);
                glBindVertexArray(m_tileVAO);
                glBindBuffer(GL_ARRAY_BUFFER, m_tileVBO);
                glBufferData(GL_ARRAY_BUFFER, pv.size() * (int)sizeof(Vertex),
                             pv.constData(), GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, pv.size());
            }
        }
    }

    glBindVertexArray(0);
}

// ─── Vertex builders ───────────────────────────────────────────────────────

void MapView::quad(QVector<Vertex> &v, float x, float y, float w, float h,
                   const QColor &c)
{
    float r = c.redF(), g = c.greenF(), b = c.blueF(), a = c.alphaF();
    v.append({x,     y,     0, 0, r, g, b, a});
    v.append({x + w, y,     0, 0, r, g, b, a});
    v.append({x,     y + h, 0, 0, r, g, b, a});
    v.append({x + w, y,     0, 0, r, g, b, a});
    v.append({x + w, y + h, 0, 0, r, g, b, a});
    v.append({x,     y + h, 0, 0, r, g, b, a});
}

void MapView::texQuad(QVector<Vertex> &v, float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1)
{
    v.append({x,     y,     u0, v0, 1, 1, 1, 1});
    v.append({x + w, y,     u1, v0, 1, 1, 1, 1});
    v.append({x,     y + h, u0, v1, 1, 1, 1, 1});
    v.append({x + w, y,     u1, v0, 1, 1, 1, 1});
    v.append({x + w, y + h, u1, v1, 1, 1, 1, 1});
    v.append({x,     y + h, u0, v1, 1, 1, 1, 1});
}

void MapView::buildTileVertices(QVector<Vertex> &verts) {
    if (!m_world) return;

    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *room = m_world->room(ri);
        if (!room) continue;
        int ox = room->worldX();
        int oy = room->worldY();
        int ts = room->tileSize();
        int w = room->width();
        int h = room->height();

        QVector<Layer*> layers;
        for (int i = 0; i < room->layerCount(); ++i) {
            Layer *l = room->layer(i);
            if (l && l->visible()) layers.append(l);
        }
        std::sort(layers.begin(), layers.end(),
                  [](const Layer *a, const Layer *b) { return a->zOrder() < b->zOrder(); });

        for (const Layer *layer : layers) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int id = layer->tileId(x, y);
                    if (id < 0) continue;
                    float u0, v0, u1, v1;
                    tileUV(id, u0, v0, u1, v1);
                    texQuad(verts, (float)(ox + x * ts), (float)(oy + y * ts),
                            (float)ts, (float)ts, u0, v0, u1, v1);
                }
            }
        }
    }
}

void MapView::buildObjectVertices(QVector<Vertex> &verts) {
    if (!m_world) return;

    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *room = m_world->room(ri);
        if (!room) continue;
        int ox = room->worldX();
        int oy = room->worldY();

        for (int i = 0; i < room->layerCount(); ++i) {
            Layer *layer = room->layer(i);
            if (!layer || !layer->visible()) continue;
            for (const GameObject &obj : layer->objects()) {
                // Skip objects with tileset spriteId
                auto sit = obj.properties.find("spriteId");
                if (sit != obj.properties.end()) {
                    int sid = std::atoi(sit->second.c_str());
                    if (sid >= 0) continue;
                }
                // Skip objects that have a matching GameObjectDef
                if (m_goDefMgr && m_goDefMgr->hasDefinitions()) {
                    const GameObjectDef *def = m_goDefMgr->definition(
                        QString::fromStdString(obj.type));
                    if (def) continue;
                }
                QColor fill = objectColor(QString::fromStdString(obj.type));
                fill.setAlpha(160);
                quad(verts, ox + obj.x, oy + obj.y, obj.width, obj.height, fill);
                // Subtle outline for non-selected objects
                QColor outline = fill.lighter(150);
                outline.setAlpha(220);
                quad(verts, ox + obj.x - 1, oy + obj.y - 1,
                     obj.width + 2, obj.height + 2, outline);
            }
        }
    }
}

void MapView::buildObjectSpriteVertices(QVector<Vertex> &verts) {
    if (!m_world || !m_spritesheet) return;

    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *room = m_world->room(ri);
        if (!room) continue;
        int ox = room->worldX();
        int oy = room->worldY();

        for (int i = 0; i < room->layerCount(); ++i) {
            Layer *layer = room->layer(i);
            if (!layer || !layer->visible()) continue;
            for (const GameObject &obj : layer->objects()) {
                auto it = obj.properties.find("spriteId");
                if (it == obj.properties.end()) continue;
                int spriteId = std::atoi(it->second.c_str());
                if (spriteId < 0) continue;

                float u0, v0, u1, v1;
                tileUV(spriteId, u0, v0, u1, v1);

                if (obj.flipX) std::swap(u0, u1);
                if (obj.flipY) std::swap(v0, v1);

                texQuad(verts, ox + obj.x, oy + obj.y,
                        obj.width, obj.height, u0, v0, u1, v1);
            }
        }
    }
}

void MapView::buildSelectionBorderVertices(QVector<Vertex> &verts) {
    if (m_selectedObjectId < 0 || !m_world) return;
    Room *room = activeRoom();
    if (!room) return;
    Layer *layer = room->layer(m_selectedObjectLayer);
    if (!layer || !layer->visible()) return;
    GameObject *obj = layer->object(m_selectedObjectId);
    if (!obj) return;

    int ox = room->worldX();
    int oy = room->worldY();
    float x = ox + obj->x, y = oy + obj->y;
    float w = obj->width, h = obj->height;

    QColor c("#ffdd44");
    c.setAlpha(200);
    float bw = 2.0f;
    // top, bottom, left, right
    quad(verts, x - bw, y - bw, w + bw * 2, bw, c);
    quad(verts, x - bw, y + h, w + bw * 2, bw, c);
    quad(verts, x - bw, y, bw, h, c);
    quad(verts, x + w, y, bw, h, c);
}

void MapView::buildDefObjectVertices(QVector<Vertex> &verts, const QString &textureFile) {
    if (!m_world || !m_goDefMgr) return;
    const QImage *texImg = m_goDefMgr->textureImage(textureFile);
    if (!texImg) return;
    float texW = (float)texImg->width();
    float texH = (float)texImg->height();

    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *room = m_world->room(ri);
        if (!room) continue;
        int ox = room->worldX();
        int oy = room->worldY();

        for (int i = 0; i < room->layerCount(); ++i) {
            Layer *layer = room->layer(i);
            if (!layer || !layer->visible()) continue;
            for (const GameObject &obj : layer->objects()) {
                // Skip if it has a tileset spriteId (higher priority)
                auto it = obj.properties.find("spriteId");
                if (it != obj.properties.end()) {
                    int sid = std::atoi(it->second.c_str());
                    if (sid >= 0) continue;
                }
                const GameObjectDef *def = m_goDefMgr->definition(
                    QString::fromStdString(obj.type));
                if (!def) continue;
                if (def->textureFile != textureFile) continue;

                const QRect &r = def->spriteRect;
                float u0 = (float)r.x() / texW;
                float v0 = (float)r.y() / texH;
                float u1 = (float)(r.x() + r.width()) / texW;
                float v1 = (float)(r.y() + r.height()) / texH;

                if (obj.flipX) std::swap(u0, u1);
                if (obj.flipY) std::swap(v0, v1);

                texQuad(verts, ox + obj.x, oy + obj.y,
                        obj.width, obj.height, u0, v0, u1, v1);
            }
        }
    }
}

void MapView::buildGridVertices(QVector<Vertex> &verts) {
    if (!m_world) return;
    const QColor c(255, 255, 255, 28);
    float r = c.redF(), g = c.greenF(), b = c.blueF(), a = c.alphaF();

    for (int ri = 0; ri < m_world->roomCount(); ++ri) {
        Room *room = m_world->room(ri);
        if (!room) continue;
        int ox = room->worldX();
        int oy = room->worldY();
        int ts = room->tileSize();
        int pw = room->pixelWidth();
        int ph = room->pixelHeight();

        for (int x = 0; x <= pw; x += ts) {
            verts.append({ox + (float)x, oy + 0.0f,       0, 0, r, g, b, a});
            verts.append({ox + (float)x, oy + (float)ph, 0, 0, r, g, b, a});
        }
        for (int y = 0; y <= ph; y += ts) {
            verts.append({ox + 0.0f,      oy + (float)y, 0, 0, r, g, b, a});
            verts.append({ox + (float)pw, oy + (float)y, 0, 0, r, g, b, a});
        }
    }
}

void MapView::buildOverlayVertices(QVector<Vertex> &verts) {
    Room *room = activeRoom();
    if (!room || !m_dragging) return;

    int ts = room->tileSize();
    int ox = room->worldX();
    int oy = room->worldY();
    QPointF start = m_dragStartWorld;
    QPointF end = m_dragEndWorld;

    if (m_tool == ToolType::Rect) {
        float x0 = std::min(start.x(), end.x());
        float y0 = std::min(start.y(), end.y());
        float x1 = std::max(start.x(), end.x()) + ts;
        float y1 = std::max(start.y(), end.y()) + ts;
        QColor highlight(255, 255, 255, 60);
        quad(verts, x0, y0, x1 - x0, y1 - y0, highlight);
        QColor border(255, 255, 255, 160);
        float bw = 2.0f;
        quad(verts, x0, y0, x1 - x0, bw, border);
        quad(verts, x0, y1 - bw, x1 - x0, bw, border);
        quad(verts, x0, y0, bw, y1 - y0, border);
        quad(verts, x1 - bw, y0, bw, y1 - y0, border);
    }
    else if (m_tool == ToolType::Line) {
        int x0 = (int)((start.x() - ox) / ts);
        int y0 = (int)((start.y() - oy) / ts);
        int x1 = (int)((end.x() - ox) / ts);
        int y1 = (int)((end.y() - oy) / ts);

        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        QColor highlight(255, 255, 255, 80);
        while (true) {
            if (x0 >= 0 && x0 < room->width() &&
                y0 >= 0 && y0 < room->height()) {
                quad(verts, (float)(ox + x0 * ts), (float)(oy + y0 * ts),
                     (float)ts, (float)ts, highlight);
            }
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }
}

// ─── Color helpers ─────────────────────────────────────────────────────────

QColor MapView::tileColor(int tileId) {
    if (tileId < 0) return Qt::transparent;
    static const QColor palette[] = {
        QColor("#4caf50"), QColor("#5b3a23"), QColor("#c9ced4"),
        QColor("#caa15a"), QColor("#3f8e46"), QColor("#2f6fdb"),
        QColor("#b14b3a"), QColor("#f2c14e"), QColor("#d99a1e"),
        QColor("#6b3d2e"), QColor("#3a2218"), QColor("#1f5247"),
        QColor("#173a3f"), QColor("#0f2b30"), QColor("#3a5fa0"),
        QColor("#5fb0ff")
    };
    return palette[tileId % 16];
}

QColor MapView::objectColor(const QString &type) {
    if (type == "Player" || type == "PlayerStart") return QColor("#caa15a");
    if (type == "Coin") return QColor("#f2c14e");
    if (type == "Enemy" || type == "Enemy_Goomba") return QColor("#6b3d2e");
    if (type == "Trigger" || type == "Trigger_Exit") return QColor("#5fb0ff");
    if (type == "Camera" || type == "Camera_Bounds") return QColor("#9aa0a8");
    return QColor("#b084cc");
}

// ─── Projection ────────────────────────────────────────────────────────────

void MapView::updateProjView() {
    float w = (float)qMax(1, width());
    float h = (float)qMax(1, height());

    QMatrix4x4 proj;
    proj.ortho(0.0, (double)w, (double)h, 0.0, -1.0, 1.0);

    QMatrix4x4 view;
    view.scale((float)m_zoom, (float)m_zoom);
    view.translate(-m_offset.x(), -m_offset.y());

    m_projView = proj * view;
}

QPointF MapView::screenToWorld(const QPointF &screen) const {
    return QPointF(
        (float)screen.x() / (float)m_zoom + m_offset.x(),
        (float)screen.y() / (float)m_zoom + m_offset.y()
    );
}

QPointF MapView::snapToTile(const QPointF &world, int tileSize) const {
    int tx = (int)(world.x() / tileSize);
    int ty = (int)(world.y() / tileSize);
    return QPointF((float)(tx * tileSize), (float)(ty * tileSize));
}

// ─── Tool operations ───────────────────────────────────────────────────────

void MapView::applyBrush(int tx, int ty) {
    Room *room = activeRoom();
    if (!room) return;
    Layer *layer = room->layer(m_activeLayerIndex);
    if (!layer || layer->locked()) return;
    if (tx < 0 || tx >= layer->width() || ty < 0 || ty >= layer->height())
        return;

    int newId = (m_tool == ToolType::Brush) ? m_selectedTileId : -1;
    int oldId = layer->tileId(tx, ty);
    if (oldId == newId) return;

    if (m_pendingCmd) {
        static_cast<TileEditCommand*>(m_pendingCmd)->addChange(tx, ty, oldId);
    } else {
        m_pendingCmd = new TileEditCommand(layer, tx, ty, oldId, newId);
    }
    layer->setTile(tx, ty, newId);
}

void MapView::applyLine(int x0, int y0, int x1, int y1) {
    Room *room = activeRoom();
    if (!room) return;
    Layer *layer = room->layer(m_activeLayerIndex);
    if (!layer || layer->locked()) return;

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    bool first = true;
    while (true) {
        if (x0 >= 0 && x0 < layer->width() && y0 >= 0 && y0 < layer->height()) {
            int oldId = layer->tileId(x0, y0);
            if (oldId != m_selectedTileId) {
                if (first) {
                    m_pendingCmd = new TileEditCommand(layer, x0, y0, oldId, m_selectedTileId);
                    first = false;
                } else {
                    static_cast<TileEditCommand*>(m_pendingCmd)->addChange(x0, y0, oldId);
                }
                layer->setTile(x0, y0, m_selectedTileId);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

// ─── Input ─────────────────────────────────────────────────────────────────

void MapView::mouseMoveEvent(QMouseEvent *event) {
    QPointF world = screenToWorld(QPointF(event->pos()));
    Room *room = activeRoom();
    int ts = room ? room->tileSize() : 32;
    int tx = (int)(world.x() / ts);
    int ty = (int)(world.y() / ts);

    // Find which room the cursor is over
    QString roomInfo;
    if (m_world) {
        for (int i = 0; i < m_world->roomCount(); ++i) {
            Room *r = m_world->room(i);
            if (!r) continue;
            float rx = r->worldX(), ry = r->worldY();
            float rw = r->pixelWidth(), rh = r->pixelHeight();
            if (world.x() >= rx && world.x() <= rx + rw &&
                world.y() >= ry && world.y() <= ry + rh) {
                roomInfo = QString::fromStdString(r->name());
                break;
            }
        }
    }

    emit mouseInfoChanged(
        QString("Mouse: %1, %2").arg((int)world.x()).arg((int)world.y()),
        roomInfo.isEmpty()
            ? QString("Tile: %1, %2").arg(tx).arg(ty)
            : QString("Tile: %1, %2 [%3]").arg(tx).arg(ty).arg(roomInfo));

    // Pan
    if (m_panning) {
        QPoint delta = event->pos() - m_lastMouse;
        m_offset -= QVector2D((float)delta.x() / (float)m_zoom,
                              (float)delta.y() / (float)m_zoom);
        m_lastMouse = event->pos();
        updateProjView();
        update();
        return;
    }

    // Drag object (Select tool)
    if (m_leftDown && m_draggingObject && m_tool == ToolType::Select && room) {
        Layer *l = room->layer(m_dragObjectLayer);
        if (l && !l->locked()) {
            GameObject *obj = l->object(m_selectedObjectId);
            if (obj) {
                int ts = room->tileSize();
                QPointF snapped = snapToTile(world, ts);
                obj->x = snapped.x() - room->worldX();
                obj->y = snapped.y() - room->worldY();
                update();
                emit objectsChanged();
            }
        }
        return;
    }

    // Drag tool
    if (m_leftDown && room) {
        Layer *layer = room->layer(m_activeLayerIndex);
        if (layer && !layer->locked()) {
            if (m_tool == ToolType::Brush || m_tool == ToolType::Erase) {
                int ts = room->tileSize();
                int ox = room->worldX();
                int oy = room->worldY();
                int tx = (int)((world.x() - ox) / ts);
                int ty = (int)((world.y() - oy) / ts);
                if (tx != m_lastTx || ty != m_lastTy) {
                    applyBrush(tx, ty);
                    m_lastTx = tx;
                    m_lastTy = ty;
                    update();
                }
            }
            else if (m_tool == ToolType::Rect || m_tool == ToolType::Line) {
                m_dragEndWorld = snapToTile(world, ts);
                if (!m_dragging) {
                    float dx = m_dragEndWorld.x() - m_dragStartWorld.x();
                    float dy = m_dragEndWorld.y() - m_dragStartWorld.y();
                    if (dx * dx + dy * dy >= (float)(ts * ts)) {
                        m_dragging = true;
                    }
                }
                if (m_dragging) {
                    m_dragEndWorld = snapToTile(world, ts);
                    update();
                }
            }
        }
    }

    // Placement preview for game object tools
    if (m_showPreview && room) {
        QPointF snapped = snapToTile(world, room->tileSize());
        m_previewWorldPos = QPointF(
            snapped.x() - room->worldX(),
            snapped.y() - room->worldY()
        );
        update();
    }
}

void MapView::mousePressEvent(QMouseEvent *event) {
    setFocus();
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastMouse = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPointF world = screenToWorld(QPointF(event->pos()));
        Room *room = activeRoom();
        int ts = room ? room->tileSize() : 32;

        if (!room) return;
        int ox = room->worldX();
        int oy = room->worldY();
        int tx = (int)((world.x() - ox) / ts);
        int ty = (int)((world.y() - oy) / ts);

        m_leftDown = true;
        m_dragging = false;
        m_dragStartWorld = snapToTile(world, ts);
        m_dragEndWorld = m_dragStartWorld;
        m_lastTx = tx;
        m_lastTy = ty;
        delete m_pendingCmd;
        m_pendingCmd = nullptr;

        Layer *layer = room->layer(m_activeLayerIndex);

        switch (m_tool) {
        case ToolType::Brush:
        case ToolType::Erase:
            applyBrush(tx, ty);
            update();
            break;

        case ToolType::Fill: {
            if (layer && !layer->locked()) {
                int oldId = layer->tileId(tx, ty);
                int newId = m_selectedTileId;
                if (oldId != newId) {
                    struct Pos { int x, y; };
                    std::vector<Pos> changed;
                    std::vector<Pos> stk;
                    std::vector<bool> visited(layer->width() * layer->height(), false);
                    stk.push_back({tx, ty});

                    while (!stk.empty()) {
                        Pos p = stk.back();
                        stk.pop_back();
                        if (p.x < 0 || p.x >= layer->width() || p.y < 0 || p.y >= layer->height())
                            continue;
                        int idx = p.y * layer->width() + p.x;
                        if (visited[idx]) continue;
                        if (layer->tileId(p.x, p.y) != oldId) continue;
                        visited[idx] = true;
                        changed.push_back({p.x, p.y});
                        stk.push_back({p.x + 1, p.y});
                        stk.push_back({p.x - 1, p.y});
                        stk.push_back({p.x, p.y + 1});
                        stk.push_back({p.x, p.y - 1});
                    }

                    if (!changed.empty()) {
                        auto *cmd = new TileEditCommand(layer, changed[0].x, changed[0].y,
                                                        oldId, newId);
                        for (size_t i = 1; i < changed.size(); ++i)
                            cmd->addChange(changed[i].x, changed[i].y, oldId);
                        if (m_undoStack)
                            m_undoStack->push(cmd);
                        else {
                            cmd->redo();
                            delete cmd;
                        }
                    }
                    update();
                }
            }
            break;
        }

        case ToolType::Rect:
        case ToolType::Line:
            break;

        case ToolType::Select: {
            // Check if clicking on the already-selected object → start drag
            bool hitSelected = false;
            if (m_selectedObjectId >= 0 && m_selectedObjectLayer >= 0) {
                Layer *l = room->layer(m_selectedObjectLayer);
                if (l && l->visible() && !l->locked()) {
                    GameObject *sObj = l->object(m_selectedObjectId);
                    if (sObj) {
                        float ox2 = ox + sObj->x, oy2 = oy + sObj->y;
                        if (world.x() >= ox2 && world.x() <= ox2 + sObj->width &&
                            world.y() >= oy2 && world.y() <= oy2 + sObj->height) {
                            m_draggingObject = true;
                            m_dragObjStartPos = QPointF(sObj->x, sObj->y);
                            m_dragObjectLayer = m_selectedObjectLayer;
                            hitSelected = true;
                        }
                    }
                }
            }
            if (!hitSelected) {
                // Look for a different object
                for (int i = 0; i < room->layerCount(); ++i) {
                    Layer *l = room->layer(i);
                    if (!l || !l->visible() || l->locked()) continue;
                    for (const GameObject &obj : l->objects()) {
                        float ox2 = ox + obj.x, oy2 = oy + obj.y;
                        float ow = obj.width, oh = obj.height;
                        if (world.x() >= ox2 && world.x() <= ox2 + ow &&
                            world.y() >= oy2 && world.y() <= oy2 + oh) {
                            m_selectedObjectId = obj.id;
                            m_selectedObjectLayer = i;
                            m_draggingObject = true;
                            m_dragObjStartPos = QPointF(obj.x, obj.y);
                            m_dragObjectLayer = i;
                            emit objectSelected(obj.id, i);
                            update();
                            return;
                        }
                    }
                }
                // Clicked empty space — clear selection
                m_selectedObjectId = -1;
                m_selectedObjectLayer = -1;
                emit objectSelected(-1, -1);
                update();
            }
            break;
        }

        case ToolType::GameObject:
        case ToolType::Trigger:
        case ToolType::Camera: {
            if (layer && !layer->locked()) {
                GameObject obj;
                obj.id = GameObject::nextId();
                obj.x = (float)(tx * ts);
                obj.y = (float)(ty * ts);
                obj.width = 24.0f;
                obj.height = 24.0f;

                if (!m_objectType.isEmpty()) {
                    obj.type = m_objectType.toStdString();
                    obj.name = m_objectType.toStdString() + "_" + std::to_string(obj.id);
                    // Apply dimensions from GameObjectDef if available
                    if (m_goDefMgr) {
                        const GameObjectDef *def = m_goDefMgr->definition(m_objectType);
                        if (def) {
                            obj.width = (float)def->spriteRect.width();
                            obj.height = (float)def->spriteRect.height();
                            if (!def->actionName.isEmpty())
                                obj.properties["action"] = def->actionName.toStdString();
                        }
                    }
                } else {
                    switch (m_tool) {
                    case ToolType::GameObject:
                        obj.type = "Coin";
                        obj.name = "GameObject_" + std::to_string(obj.id);
                        break;
                    case ToolType::Trigger:
                        obj.type = "Trigger_Exit";
                        obj.name = "Trigger_" + std::to_string(obj.id);
                        obj.width = 48.0f;
                        obj.height = 48.0f;
                        break;
                    case ToolType::Camera:
                        obj.type = "Camera_Bounds";
                        obj.name = "Camera_" + std::to_string(obj.id);
                        obj.width = 320.0f;
                        obj.height = 180.0f;
                        break;
                    default: break;
                    }
                }

                auto *cmd = new AddObjectCommand(layer, obj);
                if (m_undoStack)
                    m_undoStack->push(cmd);
                else {
                    cmd->redo();
                    delete cmd;
                }
                emit objectSelected(obj.id, m_activeLayerIndex);
                emit objectsChanged();
                update();
            }
            break;
        }

        default: break;
        }
    }
}

void MapView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }

    if (event->button() == Qt::LeftButton && m_leftDown) {
        m_leftDown = false;

        // Finalize object drag
        if (m_draggingObject) {
            m_draggingObject = false;
            Room *room = activeRoom();
            if (room) {
                Layer *layer = room->layer(m_dragObjectLayer);
                if (!layer) { m_dragging = false; return; }
                GameObject *obj = layer->object(m_selectedObjectId);
                if (obj) {
                    QPointF newPos(obj->x, obj->y);
                    if (newPos != m_dragObjStartPos) {
                        auto *cmd = new MoveObjectCommand(layer, m_selectedObjectId,
                                                          m_dragObjStartPos, newPos);
                        if (m_undoStack)
                            m_undoStack->push(cmd);
                        else {
                            cmd->redo();
                            delete cmd;
                        }
                    }
                }
            }
            m_dragging = false;
            update();
            return;
        }

        if (m_dragging) {
            Room *room = activeRoom();
            if (room) {
                Layer *layer = room->layer(m_activeLayerIndex);
                if (layer && !layer->locked()) {
                    int ts = room->tileSize();
                    int ox = room->worldX();
                    int oy = room->worldY();
                    int x0 = (int)((m_dragStartWorld.x() - ox) / ts);
                    int y0 = (int)((m_dragStartWorld.y() - oy) / ts);
                    int x1 = (int)((m_dragEndWorld.x() - ox) / ts);
                    int y1 = (int)((m_dragEndWorld.y() - oy) / ts);

                    if (m_tool == ToolType::Rect) {
                        int rx = std::min(x0, x1);
                        int ry = std::min(y0, y1);
                        int rw = abs(x1 - x0) + 1;
                        int rh = abs(y1 - y0) + 1;

                        auto *cmd = new TileEditCommand(layer, rx, ry,
                                                        layer->tileId(rx, ry), m_selectedTileId);
                        for (int y = ry; y < ry + rh; ++y) {
                            for (int x = rx; x < rx + rw; ++x) {
                                if (x == rx && y == ry) continue;
                                cmd->addChange(x, y, layer->tileId(x, y));
                            }
                        }
                        cmd->redo();
                        if (m_undoStack)
                            m_undoStack->push(cmd);
                        else
                            delete cmd;
                    } else if (m_tool == ToolType::Line) {
                        applyLine(x0, y0, x1, y1);
                        if (m_pendingCmd) {
                            m_pendingCmd->setText("Draw Line");
                            if (m_undoStack)
                                m_undoStack->push(m_pendingCmd);
                            else
                                delete m_pendingCmd;
                            m_pendingCmd = nullptr;
                        }
                    }
                    update();
                }
            }
        }

        if (m_pendingCmd) {
            if (m_undoStack)
                m_undoStack->push(m_pendingCmd);
            else
                delete m_pendingCmd;
            m_pendingCmd = nullptr;
        }

        m_dragging = false;
    }
}

void MapView::wheelEvent(QWheelEvent *event) {
    double factor = event->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1;
    double newZoom = qBound(0.1, m_zoom * factor, 10.0);

    QPointF mouse = event->position();
    QPointF worldBefore = screenToWorld(mouse);
    m_zoom = newZoom;
    updateProjView();
    QPointF worldAfter = screenToWorld(mouse);
    m_offset += QVector2D((float)(worldAfter.x() - worldBefore.x()),
                          (float)(worldAfter.y() - worldBefore.y()));
    updateProjView();
    update();
    emit zoomChanged(m_zoom);
}

void MapView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_selectedObjectId < 0) return;
        Room *room = activeRoom();
        if (!room) return;
        Layer *layer = room->layer(m_selectedObjectLayer);
        if (!layer) return;
        GameObject *obj = layer->object(m_selectedObjectId);
        if (!obj) return;

        auto *cmd = new DeleteObjectCommand(layer, *obj);
        if (m_undoStack)
            m_undoStack->push(cmd);
        else {
            cmd->redo();
            delete cmd;
        }
        m_selectedObjectId = -1;
        m_selectedObjectLayer = -1;
        emit objectsChanged();
        update();
    }
}

// ─── Shader helpers ────────────────────────────────────────────────────────

GLuint MapView::compileShader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        qCritical("Shader compile error (%s): %s",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint MapView::linkProgram(GLuint vert, GLuint frag) {
    if (!vert || !frag) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        qCritical("Program link error: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}
