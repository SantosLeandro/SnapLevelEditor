#pragma once

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include <QMatrix4x4>
#include <QPointF>
#include <QVector2D>
#include <QVector>
#include <QColor>
#include <QImage>
#include <QString>

class Room;
class World;
class GameObject;
class QUndoStack;
class QUndoCommand;

class MapView : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    enum class ToolType {
        Select, Brush, Erase, Fill, Rect, Line,
        GameObject, Trigger, Camera
    };
    Q_ENUM(ToolType)

    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    explicit MapView(QWidget *parent = nullptr);
    ~MapView();

    void setWorld(World *world);
    World *world() const { return m_world; }

    Room *activeRoom() const;
    void setActiveRoomIndex(int index);
    int activeRoomIndex() const { return m_activeRoomIndex; }

    void setTool(ToolType tool);
    ToolType currentTool() const { return m_tool; }

    void setActiveLayerIndex(int index);
    int activeLayerIndex() const { return m_activeLayerIndex; }

    void setSelectedTileId(int id);
    int selectedTileId() const { return m_selectedTileId; }

    void setUndoStack(QUndoStack *stack) { m_undoStack = stack; }

    void setObjectType(const QString &type) { m_objectType = type; }
    const QString &objectType() const { return m_objectType; }

    void selectObject(int64_t objectId, int layerIndex);

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void centerOn(float worldX, float worldY);

    const QImage &spritesheetImage() const { return m_spritesheetImage; }
    int atlasTileSize() const { return m_atlasTileSize; }
    int atlasCols() const { return m_atlasCols; }
    void setAtlasTileSize(int size);
    void setAtlasCols(int cols) { m_atlasCols = cols; }

    bool loadSpritesheet(const QString &path);

signals:
    void mouseInfoChanged(const QString &mouse, const QString &tile);
    void toolChanged(ToolType tool);
    void objectSelected(int64_t objectId, int layerIndex);
    void zoomChanged(double zoom);
    void objectsChanged();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void quad(QVector<Vertex> &v, float x, float y, float w, float h,
              const QColor &c);
    void texQuad(QVector<Vertex> &v, float x, float y, float w, float h,
                 float u0, float v0, float u1, float v1);
    GLuint compileShader(GLenum type, const char *source);
    GLuint linkProgram(GLuint vert, GLuint frag);

    void buildTileVertices(QVector<Vertex> &verts);
    void buildObjectVertices(QVector<Vertex> &verts);
    void buildGridVertices(QVector<Vertex> &verts);
    void buildOverlayVertices(QVector<Vertex> &verts);

    void generateSpritesheet();
    void tileUV(int tileId, float &u0, float &v0, float &u1, float &v1) const;

    static QColor tileColor(int tileId);
    static QColor objectColor(const QString &type);
    void updateProjView();
    QPointF screenToWorld(const QPointF &screen) const;
    QPointF snapToTile(const QPointF &world, int tileSize) const;

    void applyBrush(int tx, int ty);
    void applyLine(int x0, int y0, int x1, int y1);

    World *m_world = nullptr;
    ToolType m_tool = ToolType::Brush;
    int m_activeRoomIndex = 0;
    int m_activeLayerIndex = 0;
    int m_selectedTileId = 1;

    double m_zoom = 1.0;
    QVector2D m_offset;
    bool m_panning = false;
    QPoint m_lastMouse;

    // Drag / tool state
    bool m_leftDown = false;
    QPointF m_dragStartWorld;
    QPointF m_dragEndWorld;
    bool m_dragging = false;
    int m_lastTx = -1;
    int m_lastTy = -1;

    // Undo
    QUndoStack *m_undoStack = nullptr;
    QUndoCommand *m_pendingCmd = nullptr;

    // Object type override (from m_goTypeList)
    QString m_objectType;

    // Currently selected game object
    int64_t m_selectedObjectId = -1;
    int m_selectedObjectLayer = -1;

    // Atlas parameters (dynamic, set from World::defaultTileSize)
    int m_atlasTileSize = 32;
    int m_atlasCols = 16;

    // OpenGL
    GLuint m_program = 0;
    GLuint m_tileVAO = 0;
    GLuint m_tileVBO = 0;
    GLuint m_gridVAO = 0;
    GLuint m_gridVBO = 0;
    GLuint m_overlayVAO = 0;
    GLuint m_overlayVBO = 0;
    GLint m_uProjView = -1;
    GLint m_uTexture = -1;

    QOpenGLTexture *m_spritesheet = nullptr;
    QOpenGLTexture *m_whiteTex = nullptr;
    QImage m_spritesheetImage;
    QString m_spritesheetPath;

    QMatrix4x4 m_projView;
};
