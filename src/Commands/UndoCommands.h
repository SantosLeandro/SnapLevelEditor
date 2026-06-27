#pragma once

#include <QUndoCommand>
#include <QVector>
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
