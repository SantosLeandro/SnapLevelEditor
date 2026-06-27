#include "UndoCommands.h"
#include "../Model/Layer.h"

// ─── TileEditCommand ───────────────────────────────────────────────────────

TileEditCommand::TileEditCommand(Layer *layer, int x, int y, int oldId, int newId,
                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layer(layer)
    , m_newId(newId)
{
    m_changes.append({x, y, oldId});
    setText(QString("Edit Tiles"));
}

void TileEditCommand::addChange(int x, int y, int oldId) {
    m_changes.append({x, y, oldId});
}

void TileEditCommand::undo() {
    for (const auto &c : m_changes)
        m_layer->setTile(c.x, c.y, c.oldId);
}

void TileEditCommand::redo() {
    for (const auto &c : m_changes)
        m_layer->setTile(c.x, c.y, m_newId);
}

// ─── DeleteObjectCommand ────────────────────────────────────────────────────

DeleteObjectCommand::DeleteObjectCommand(Layer *layer, const GameObject &obj,
                                         QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layer(layer)
    , m_obj(obj)
{
    setText(QString("Delete %1").arg(QString::fromStdString(obj.type)));
}

void DeleteObjectCommand::undo() {
    m_layer->addObject(m_obj);
}

void DeleteObjectCommand::redo() {
    m_layer->removeObject(m_obj.id);
}

// ─── AddObjectCommand ──────────────────────────────────────────────────────

AddObjectCommand::AddObjectCommand(Layer *layer, const GameObject &obj,
                                   QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layer(layer)
    , m_obj(obj)
{
    setText(QString("Add %1").arg(QString::fromStdString(obj.type)));
}

void AddObjectCommand::undo() {
    m_layer->removeObject(m_obj.id);
}

void AddObjectCommand::redo() {
    m_layer->addObject(m_obj);
}
