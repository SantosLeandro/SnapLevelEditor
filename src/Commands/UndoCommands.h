#pragma once

#include <QUndoCommand>
#include <QVector>
#include <QPointF>
#include <string>
#include <cstdint>
#include "../Model/GameObject.h"

class Layer;

// ─── Tile change (brush, erase, fill, rect, line, flood fill) ──────────────

class TileEditCommand : public QUndoCommand {
public:
    TileEditCommand(Layer *layer, int x, int y, int oldId, int newId,
                    QUndoCommand *parent = nullptr);

    void addChange(int x, int y, int oldId);
    void undo() override;
    void redo() override;

private:
    struct Change { int x, y; int oldId; };
    Layer *m_layer;
    int m_newId;
    QVector<Change> m_changes;
};

// ─── Delete game object ────────────────────────────────────────────────────

class DeleteObjectCommand : public QUndoCommand {
public:
    DeleteObjectCommand(Layer *layer, const GameObject &obj,
                        QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    Layer *m_layer;
    GameObject m_obj;
};

// ─── Add game object ───────────────────────────────────────────────────────

class AddObjectCommand : public QUndoCommand {
public:
    AddObjectCommand(Layer *layer, const GameObject &obj,
                     QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Layer *m_layer;
    GameObject m_obj;
};

// ─── Move game object ──────────────────────────────────────────────────────

class MoveObjectCommand : public QUndoCommand {
public:
    MoveObjectCommand(Layer *layer, int64_t objectId,
                      QPointF oldPos, QPointF newPos,
                      QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    Layer *m_layer;
    int64_t m_objectId;
    QPointF m_oldPos;
    QPointF m_newPos;
};

// ─── Set object sprite (tile index) ────────────────────────────────────────

class SetObjectSpriteCommand : public QUndoCommand {
public:
    SetObjectSpriteCommand(Layer *layer, int64_t objectId,
                           const std::string &oldSpriteId,
                           const std::string &newSpriteId,
                           QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
private:
    Layer *m_layer;
    int64_t m_objectId;
    std::string m_oldSpriteId;
    std::string m_newSpriteId;
};
