#include "sensors/SensorSnapshot.h"

#include "api/JsonUtil.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace SensorSnapshot {

QString filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/openlinkhub-qt");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/sensors.json");
}

static QString displayName(const QJsonObject &channel)
{
    const QString name = Json::str(channel, "name");
    const QString label = Json::str(channel, "label");
    if (!label.isEmpty() && label.compare(name, Qt::CaseInsensitive) != 0) {
        return name + QStringLiteral(" — ") + label;
    }
    return name;
}

static Property property(const QString &id, const QString &name, const QString &shortName, const QString &unit, double value, double max)
{
    Property property;
    property.id = id;
    property.name = name;
    property.shortName = shortName;
    property.unit = unit;
    property.value = value;
    property.max = max;
    return property;
}

static double metricValue(const QJsonObject &map, const QString &key)
{
    const QJsonObject object = Json::object(map.value(key));
    if (object.contains(QLatin1String("Value"))) {
        return Json::number(object, "Value");
    }
    return Json::number(object, "ValueString");
}

static QString sanitize(const QString &serial, int channelId)
{
    QString id = serial;
    id.replace(QLatin1Char('/'), QLatin1Char('_'));
    if (id.isEmpty()) {
        id = QStringLiteral("device");
    }
    return id + QLatin1Char('_') + QString::number(channelId);
}

static Object fromChannel(const QString &serial, const QJsonObject &channel)
{
    Object object;
    const int channelId = Json::integer(channel, "channelId");
    const QString key = sanitize(serial, channelId);
    const QString name = displayName(channel);
    const QJsonObject volts = Json::object(channel, "volts");
    const bool psu = Json::boolean(channel, "IsPSU") || Json::boolean(channel, "MainPSU") || !volts.isEmpty();
    const bool pump = Json::boolean(channel, "AIO") || Json::boolean(channel, "ContainsPump");
    const double rpm = Json::number(channel, "rpm");
    const double temperature = Json::number(channel, "temperature");

    if (psu) {
        object.id = QStringLiteral("psu_") + key;
        object.kind = QStringLiteral("psu");
        object.name = name.isEmpty() ? QStringLiteral("PSU") : name;
        if (Json::boolean(channel, "HasTemps") || temperature > 0) {
            object.properties.append(property(QStringLiteral("temperature"), QStringLiteral("PSU Temperature"), QStringLiteral("Temp"), QStringLiteral("celsius"), temperature, 100));
        }
        if (Json::boolean(channel, "HasSpeed") || channel.contains(QLatin1String("rpm"))) {
            object.properties.append(property(QStringLiteral("rpm"), QStringLiteral("PSU Fan"), QStringLiteral("Fan"), QStringLiteral("rpm"), rpm, 4000));
        }
        const double power = channel.contains(QLatin1String("powerOut")) ? Json::number(channel, "powerOut") : Json::number(channel, "powerOutString");
        object.properties.append(property(QStringLiteral("power"), QStringLiteral("PSU Power Out"), QStringLiteral("Power"), QStringLiteral("watt"), power, 1500));

        const QJsonObject amps = Json::object(channel, "amps");
        const QJsonObject watts = Json::object(channel, "watts");
        struct Rail {
            const char *key;
            const char *label;
            double voltMax;
            double ampMax;
            double wattMax;
        };
        const Rail rails[] = {
            {"0", "3.3V", 4.0, 30.0, 120.0},
            {"1", "5V", 6.0, 30.0, 150.0},
            {"2", "12V", 14.0, 100.0, 1500.0},
        };
        for (const Rail &rail : rails) {
            const QString railKey = QLatin1String(rail.key);
            if (!volts.contains(railKey) && !amps.contains(railKey) && !watts.contains(railKey)) {
                continue;
            }
            const QString prefix = QStringLiteral("rail_%1").arg(QLatin1String(rail.label)).replace(QLatin1Char('.'), QLatin1Char('_'));
            object.properties.append(property(prefix + QStringLiteral("_volt"), QStringLiteral("%1 Voltage").arg(QLatin1String(rail.label)), QStringLiteral("%1 V").arg(QLatin1String(rail.label)), QStringLiteral("volt"), metricValue(volts, railKey), rail.voltMax));
            object.properties.append(property(prefix + QStringLiteral("_amp"), QStringLiteral("%1 Current").arg(QLatin1String(rail.label)), QStringLiteral("%1 A").arg(QLatin1String(rail.label)), QStringLiteral("ampere"), metricValue(amps, railKey), rail.ampMax));
            object.properties.append(property(prefix + QStringLiteral("_watt"), QStringLiteral("%1 Power").arg(QLatin1String(rail.label)), QStringLiteral("%1 W").arg(QLatin1String(rail.label)), QStringLiteral("watt"), metricValue(watts, railKey), rail.wattMax));
        }
        return object;
    }

    if (pump) {
        object.id = QStringLiteral("pump_") + key;
        object.kind = QStringLiteral("pump");
        object.name = name.isEmpty() ? QStringLiteral("Pump") : name;
        if (Json::boolean(channel, "HasSpeed") || rpm > 0) {
            object.properties.append(property(QStringLiteral("rpm"), QStringLiteral("Pump Speed"), QStringLiteral("Pump"), QStringLiteral("rpm"), rpm, 4000));
        }
        if (Json::boolean(channel, "HasTemps") || temperature > 0) {
            object.properties.append(property(QStringLiteral("temperature"), QStringLiteral("Pump Temperature"), QStringLiteral("Temp"), QStringLiteral("celsius"), temperature, 80));
        }
        return object;
    }

    if (Json::boolean(channel, "HasSpeed") || rpm > 0) {
        object.id = QStringLiteral("fan_") + key;
        object.kind = QStringLiteral("fan");
        object.name = name.isEmpty() ? QStringLiteral("Fan") : name;
        object.properties.append(property(QStringLiteral("rpm"), QStringLiteral("Fan Speed"), QStringLiteral("Fan"), QStringLiteral("rpm"), rpm, 3000));
        if (Json::boolean(channel, "HasTemps") && temperature > 0) {
            object.properties.append(property(QStringLiteral("temperature"), QStringLiteral("Fan Temperature"), QStringLiteral("Temp"), QStringLiteral("celsius"), temperature, 80));
        }
        return object;
    }

    if (Json::boolean(channel, "HasTemps") || temperature > 0) {
        object.id = QStringLiteral("temp_") + key;
        object.kind = QStringLiteral("temperature");
        object.name = name.isEmpty() ? QStringLiteral("Temperature") : name;
        object.properties.append(property(QStringLiteral("temperature"), QStringLiteral("Temperature"), QStringLiteral("Temp"), QStringLiteral("celsius"), temperature, 100));
        return object;
    }

    return object;
}

Snapshot fromDevices(const QJsonObject &devicesResponse)
{
    Snapshot snapshot;
    const QJsonObject devices = Json::object(devicesResponse, "devices");
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        const QJsonObject wrapper = Json::object(it.value());
        if (Json::boolean(wrapper, "Hidden")) {
            continue;
        }
        const QString serial = Json::strAny(wrapper, {QStringLiteral("Serial"), QStringLiteral("serial")}, it.key());
        if (serial.isEmpty() || serial == QLatin1String("cluster")) {
            continue;
        }
        const QJsonObject device = Json::object(wrapper, "GetDevice");
        const QJsonObject channels = Json::object(device, "devices");
        if (channels.isEmpty()) {
            continue;
        }
        for (auto channelIt = channels.begin(); channelIt != channels.end(); ++channelIt) {
            if (!channelIt.value().isObject()) {
                continue;
            }
            Object object = fromChannel(serial, channelIt.value().toObject());
            if (!object.id.isEmpty() && !object.properties.isEmpty()) {
                snapshot.objects.append(object);
            }
        }
    }
    return snapshot;
}

bool write(const Snapshot &snapshot)
{
    QJsonArray objects;
    for (const Object &object : snapshot.objects) {
        QJsonArray properties;
        for (const Property &property : object.properties) {
            properties.append(QJsonObject{
                {QStringLiteral("id"), property.id},
                {QStringLiteral("name"), property.name},
                {QStringLiteral("shortName"), property.shortName},
                {QStringLiteral("unit"), property.unit},
                {QStringLiteral("value"), property.value},
                {QStringLiteral("min"), property.min},
                {QStringLiteral("max"), property.max},
            });
        }
        objects.append(QJsonObject{
            {QStringLiteral("id"), object.id},
            {QStringLiteral("name"), object.name},
            {QStringLiteral("kind"), object.kind},
            {QStringLiteral("properties"), properties},
        });
    }

    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("objects"), objects}}).toJson(QJsonDocument::Compact));
    return file.commit();
}

Snapshot read()
{
    Snapshot snapshot;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return snapshot;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray objects = root.value(QStringLiteral("objects")).toArray();
    for (const QJsonValue &value : objects) {
        const QJsonObject objectJson = value.toObject();
        Object object;
        object.id = Json::str(objectJson, "id");
        object.name = Json::str(objectJson, "name");
        object.kind = Json::str(objectJson, "kind");
        const QJsonArray properties = objectJson.value(QStringLiteral("properties")).toArray();
        for (const QJsonValue &propertyValue : properties) {
            const QJsonObject propertyJson = propertyValue.toObject();
            Property property;
            property.id = Json::str(propertyJson, "id");
            property.name = Json::str(propertyJson, "name");
            property.shortName = Json::str(propertyJson, "shortName");
            property.unit = Json::str(propertyJson, "unit");
            property.value = Json::number(propertyJson, "value");
            property.min = Json::number(propertyJson, "min");
            property.max = Json::number(propertyJson, "max");
            if (!property.id.isEmpty()) {
                object.properties.append(property);
            }
        }
        if (!object.id.isEmpty()) {
            snapshot.objects.append(object);
        }
    }
    return snapshot;
}

} // namespace SensorSnapshot
