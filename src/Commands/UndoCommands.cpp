#include "UndoCommands.h"
#include "../Model/Layer.h"
#include "../Model/Room.h"
#include <cmath>

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

// ─── MoveObjectCommand ─────────────────────────────────────────────────────

MoveObjectCommand::MoveObjectCommand(Layer *layer, int64_t objectId,
                                     QPointF oldPos, QPointF newPos,
                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layer(layer)
    , m_objectId(objectId)
    , m_oldPos(oldPos)
    , m_newPos(newPos)
{
    setText(QString("Move Object"));
}

void MoveObjectCommand::undo() {
    GameObject *obj = m_layer->object(m_objectId);
    if (obj) {
        obj->x = m_oldPos.x();
        obj->y = m_oldPos.y();
    }
}

void MoveObjectCommand::redo() {
    GameObject *obj = m_layer->object(m_objectId);
    if (obj) {
        obj->x = m_newPos.x();
        obj->y = m_newPos.y();
    }
}

// ─── SetObjectSpriteCommand ─────────────────────────────────────────────────

SetObjectSpriteCommand::SetObjectSpriteCommand(Layer *layer, int64_t objectId,
                                               const std::string &oldSpriteId,
                                               const std::string &newSpriteId,
                                               QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layer(layer)
    , m_objectId(objectId)
    , m_oldSpriteId(oldSpriteId)
    , m_newSpriteId(newSpriteId)
{
    setText(QString("Set Object Sprite"));
}

void SetObjectSpriteCommand::undo() {
    GameObject *obj = m_layer->object(m_objectId);
    if (!obj) return;
    if (m_oldSpriteId.empty())
        obj->properties.erase("spriteId");
    else
        obj->properties["spriteId"] = m_oldSpriteId;
}

void SetObjectSpriteCommand::redo() {
    GameObject *obj = m_layer->object(m_objectId);
    if (!obj) return;
    if (m_newSpriteId.empty())
        obj->properties.erase("spriteId");
    else
        obj->properties["spriteId"] = m_newSpriteId;
}

// ─── MoveRoomCommand ────────────────────────────────────────────────────────

MoveRoomCommand::MoveRoomCommand(Room *room, int oldX, int oldY, int newX, int newY,
                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_room(room)
    , m_oldX(oldX)
    , m_oldY(oldY)
    , m_newX(newX)
    , m_newY(newY)
{
    setText(QString("Move Room"));
}

void MoveRoomCommand::undo() {
    m_room->setWorldX(m_oldX);
    m_room->setWorldY(m_oldY);
}

void MoveRoomCommand::redo() {
    m_room->setWorldX(m_newX);
    m_room->setWorldY(m_newY);
}
