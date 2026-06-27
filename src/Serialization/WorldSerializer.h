#pragma once

#include <QString>
#include <memory>

class World;
class QJsonDocument;

class WorldSerializer {
public:
    static QJsonDocument toJson(const World &world);
    static std::unique_ptr<World> fromJson(const QJsonDocument &doc, QString *errorOut = nullptr);

    static bool saveToFile(const World &world, const QString &path, QString *errorOut = nullptr);
    static std::unique_ptr<World> loadFromFile(const QString &path, QString *errorOut = nullptr);
};
