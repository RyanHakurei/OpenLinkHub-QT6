#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace SensorSnapshot {

struct Property {
    QString id;
    QString name;
    QString shortName;
    QString unit;
    double value = 0;
    double min = 0;
    double max = 0;
};

struct Object {
    QString id;
    QString name;
    QString kind;
    QVector<Property> properties;
};

struct Snapshot {
    QVector<Object> objects;
};

QString filePath();
Snapshot fromDevices(const QJsonObject &devicesResponse);
bool write(const Snapshot &snapshot);
Snapshot read();

} // namespace SensorSnapshot
