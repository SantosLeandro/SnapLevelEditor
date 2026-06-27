#include "GameObjectDef.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool GameObjectDefManager::loadDefinitions(const QString &jsonPath) {
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    QString texture = root["texture"].toString();
    if (texture.isEmpty()) return false;

    m_baseDir = QFileInfo(jsonPath).absolutePath();
    m_defs.clear();

    QJsonArray arr = root["gameobjects"].toArray();
    for (const auto &val : arr) {
        QJsonObject o = val.toObject();
        GameObjectDef def;
        def.name = o["name"].toString();
        def.textureFile = texture;
        def.spriteRect = QRect(
            o["x"].toString().toInt(),
            o["y"].toString().toInt(),
            o["width"].toString().toInt(),
            o["height"].toString().toInt()
        );
        def.actionName = o["actionName"].toString();
        if (!def.name.isEmpty())
            m_defs.append(def);
    }

    return !m_defs.isEmpty();
}

bool GameObjectDefManager::loadDefault(const QString &appDir) {
    QString path = appDir + "/gameobjects.json";
    if (!QFile::exists(path))
        return false;
    return loadDefinitions(path);
}

const GameObjectDef *GameObjectDefManager::definition(const QString &name) const {
    for (const auto &d : m_defs) {
        if (d.name == name)
            return &d;
    }
    return nullptr;
}

QStringList GameObjectDefManager::definitionNames() const {
    QStringList names;
    for (const auto &d : m_defs)
        names.append(d.name);
    return names;
}

const QImage *GameObjectDefManager::textureImage(const QString &textureFile) const {
    auto it = m_textures.constFind(textureFile);
    if (it != m_textures.constEnd())
        return &it.value();

    // Try several path resolutions
    QStringList candidates;
    candidates << (m_baseDir + "/" + textureFile)   // relative to JSON dir
               << textureFile;                       // absolute or cwd-relative

    QImage img;
    for (const QString &c : candidates) {
        img = QImage(c);
        if (!img.isNull()) break;
    }

    if (img.isNull())
        return nullptr;

    m_textures.insert(textureFile, img);
    return &m_textures[textureFile];
}

QStringList GameObjectDefManager::uniqueTextureFiles() const {
    QStringList list;
    for (const auto &d : m_defs) {
        if (!list.contains(d.textureFile))
            list.append(d.textureFile);
    }
    return list;
}
