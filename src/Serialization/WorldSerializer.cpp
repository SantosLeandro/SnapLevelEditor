#include "WorldSerializer.h"
#include "../Model/World.h"
#include "../Model/Room.h"
#include "../Model/Layer.h"
#include "../Model/GameObject.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QIODevice>

// ─── Helpers ───────────────────────────────────────────────────────────────

static QString s(const QJsonObject &o, const char *k, const QString &d = {}) {
    return o[QString::fromLatin1(k)].toString(d);
}
static int i(const QJsonObject &o, const char *k, int d = 0) {
    return o[QString::fromLatin1(k)].toInt(d);
}
static double d(const QJsonObject &o, const char *k, double def = 0.0) {
    return o[QString::fromLatin1(k)].toDouble(def);
}
static bool b(const QJsonObject &o, const char *k, bool def = false) {
    return o[QString::fromLatin1(k)].toBool(def);
}

// ─── Serialize ─────────────────────────────────────────────────────────────

QJsonDocument WorldSerializer::toJson(const World &world) {
    QJsonObject root;
    root["name"] = QString::fromStdString(world.name());
    root["defaultTileSize"] = world.defaultTileSize();

    QJsonArray rooms;
    for (int i = 0; i < world.roomCount(); ++i) {
        const Room *room = world.room(i);
        QJsonObject rObj;
        rObj["name"] = QString::fromStdString(room->name());
        rObj["width"] = room->width();
        rObj["height"] = room->height();
        rObj["tileSize"] = room->tileSize();
        rObj["x"] = room->worldX();
        rObj["y"] = room->worldY();

        QJsonArray layers;
        for (int li = 0; li < room->layerCount(); ++li) {
            const Layer *layer = room->layer(li);
            QJsonObject lObj;
            lObj["name"] = QString::fromStdString(layer->name());
            lObj["visible"] = layer->visible();
            lObj["locked"] = layer->locked();
            lObj["opacity"] = layer->opacity();
            lObj["zOrder"] = layer->zOrder();

            QJsonArray tiles;
            for (int y = 0; y < layer->height(); ++y) {
                for (int x = 0; x < layer->width(); ++x) {
                    int id = layer->tileId(x, y);
                    if (id < 0) continue;
                    QJsonObject t;
                    t["x"] = x; t["y"] = y; t["id"] = id;
                    tiles.append(t);
                }
            }
            lObj["tiles"] = tiles;

            QJsonArray objects;
            for (const auto &go : layer->objects()) {
                QJsonObject oObj;
                oObj["id"] = (qint64)go.id;
                oObj["name"] = QString::fromStdString(go.name);
                oObj["type"] = QString::fromStdString(go.type);
                oObj["x"] = go.x;
                oObj["y"] = go.y;
                oObj["width"] = go.width;
                oObj["height"] = go.height;
                oObj["flipX"] = go.flipX;
                oObj["flipY"] = go.flipY;

                if (!go.properties.empty()) {
                    QJsonObject props;
                    for (const auto &[k, v] : go.properties)
                        props[QString::fromStdString(k)] = QString::fromStdString(v);
                    oObj["properties"] = props;
                }

                objects.append(oObj);
            }
            lObj["objects"] = objects;
            layers.append(lObj);
        }
        rObj["layers"] = layers;
        rooms.append(rObj);
    }
    root["rooms"] = rooms;

    return QJsonDocument(root);
}

// ─── Deserialize ───────────────────────────────────────────────────────────

std::unique_ptr<World> WorldSerializer::fromJson(const QJsonDocument &doc, QString *errorOut) {
    QJsonObject root = doc.object();
    if (root.isEmpty()) {
        if (errorOut) *errorOut = "Empty JSON document";
        return nullptr;
    }

    auto world = std::make_unique<World>(s(root, "name", "Untitled").toStdString());
    world->setDefaultTileSize(i(root, "defaultTileSize", 32));

    GameObject::resetIdCounter(1);
    int64_t maxId = 0;

    QJsonArray roomsArr = root["rooms"].toArray();
    for (const auto &rv : roomsArr) {
        QJsonObject rObj = rv.toObject();
        QString roomName = s(rObj, "name", "Unnamed");
        int rw = i(rObj, "width", 20);
        int rh = i(rObj, "height", 15);
        int ts = i(rObj, "tileSize", 32);

        auto room = std::make_unique<Room>(roomName.toStdString(), rw, rh, ts);
        room->setWorldX(i(rObj, "x", 0));
        room->setWorldY(i(rObj, "y", 0));

        QJsonArray layersArr = rObj["layers"].toArray();
        for (const auto &lv : layersArr) {
            QJsonObject lObj = lv.toObject();
            QString layerName = s(lObj, "name", "Layer");

            Layer *layer = room->addLayer(layerName.toStdString());
            layer->setVisible(b(lObj, "visible", true));
            layer->setLocked(b(lObj, "locked", false));
            layer->setOpacity((float)d(lObj, "opacity", 1.0));
            layer->setZOrder(i(lObj, "zOrder", 0));

            QJsonArray tilesArr = lObj["tiles"].toArray();
            for (const auto &tv : tilesArr) {
                QJsonObject tObj = tv.toObject();
                int tx = i(tObj, "x");
                int ty = i(tObj, "y");
                int id = i(tObj, "id", -1);
                if (tx >= 0 && tx < rw && ty >= 0 && ty < rh && id >= 0)
                    layer->setTile(tx, ty, id);
            }

            QJsonArray objectsArr = lObj["objects"].toArray();
            for (const auto &ov : objectsArr) {
                QJsonObject oObj = ov.toObject();
                GameObject go;
                go.id = (int64_t)d(oObj, "id");
                go.name = s(oObj, "name").toStdString();
                go.type = s(oObj, "type").toStdString();
                go.x = (float)d(oObj, "x");
                go.y = (float)d(oObj, "y");
                go.width = (float)d(oObj, "width", 32.0);
                go.height = (float)d(oObj, "height", 32.0);
                go.flipX = b(oObj, "flipX");
                go.flipY = b(oObj, "flipY");

                if (oObj.contains("properties")) {
                    QJsonObject props = oObj["properties"].toObject();
                    for (auto it = props.begin(); it != props.end(); ++it)
                        go.properties[it.key().toStdString()] = it.value().toString().toStdString();
                }

                if (go.id == 0) go.id = GameObject::nextId();
                if (go.id > maxId) maxId = go.id;
                layer->addObject(go);
            }
        }

        world->addRoom(std::move(room));
    }

    if (maxId > 0) GameObject::updateIdCounter(maxId);
    if (errorOut) *errorOut = QString();
    return world;
}

// ─── File I/O ──────────────────────────────────────────────────────────────

bool WorldSerializer::saveToFile(const World &world, const QString &path, QString *errorOut) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = "Cannot write: " + file.errorString();
        return false;
    }

    QJsonDocument doc = toJson(world);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (errorOut) *errorOut = "Write error: " + file.errorString();
        return false;
    }

    return true;
}

std::unique_ptr<World> WorldSerializer::loadFromFile(const QString &path, QString *errorOut) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = "Cannot open: " + file.errorString();
        return nullptr;
    }

    QByteArray data = file.readAll();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        if (errorOut) *errorOut = "JSON error: " + err.errorString();
        return nullptr;
    }

    return fromJson(doc, errorOut);
}
