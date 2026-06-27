#pragma once

#include <QString>
#include <QRect>
#include <QVector>
#include <QMap>
#include <QImage>

struct GameObjectDef {
    QString name;
    QString textureFile;
    QRect spriteRect;
    QString actionName;
};

class GameObjectDefManager {
public:
    bool loadDefinitions(const QString &jsonPath);
    bool loadDefault(const QString &appDir);

    const GameObjectDef *definition(const QString &name) const;
    const QVector<GameObjectDef> &definitions() const { return m_defs; }
    QStringList definitionNames() const;
    bool hasDefinitions() const { return !m_defs.isEmpty(); }

    const QImage *textureImage(const QString &textureFile) const;
    QStringList uniqueTextureFiles() const;

private:
    QVector<GameObjectDef> m_defs;
    mutable QMap<QString, QImage> m_textures;
    QString m_baseDir;
};
