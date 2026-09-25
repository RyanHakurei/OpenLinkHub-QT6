#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace Json {

inline QJsonValue pick(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        if (object.contains(key)) {
            return object.value(key);
        }
    }
    return {};
}

inline QString str(const QJsonObject &object, const char *key, const QString &fallback = {})
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return fallback;
}

inline QString strAny(const QJsonObject &object, const QStringList &keys, const QString &fallback = {})
{
    const QJsonValue value = pick(object, keys);
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        if (value.toDouble() != static_cast<double>(value.toInt())) {
            return QString::number(value.toDouble());
        }
        return QString::number(value.toInt());
    }
    return fallback;
}

inline bool boolean(const QJsonObject &object, const char *key, bool fallback = false)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return value.toInt() != 0;
    }
    return fallback;
}

inline int integer(const QJsonObject &object, const char *key, int fallback = 0)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isDouble()) {
        return value.toInt();
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        return ok ? parsed : fallback;
    }
    return fallback;
}

inline double number(const QJsonObject &object, const char *key, double fallback = 0.0)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        return ok ? parsed : fallback;
    }
    return fallback;
}

inline QJsonObject object(const QJsonValue &value)
{
    return value.isObject() ? value.toObject() : QJsonObject{};
}

inline QJsonObject object(const QJsonObject &parent, const char *key)
{
    return object(parent.value(QLatin1String(key)));
}

inline QJsonArray array(const QJsonObject &parent, const char *key)
{
    const QJsonValue value = parent.value(QLatin1String(key));
    return value.isArray() ? value.toArray() : QJsonArray{};
}

inline QColor color(const QJsonObject &object)
{
    return QColor(integer(object, "red"), integer(object, "green"), integer(object, "blue"));
}

inline QJsonObject colorObject(const QColor &color, double brightness = 1.0)
{
    const QString hex = color.name(QColor::HexRgb);
    return QJsonObject{
        {QStringLiteral("red"), color.red()},
        {QStringLiteral("green"), color.green()},
        {QStringLiteral("blue"), color.blue()},
        {QStringLiteral("brightness"), brightness},
        {QStringLiteral("Hex"), hex},
        {QStringLiteral("hex"), hex},
    };
}

inline QStringList stringList(const QJsonValue &value)
{
    QStringList result;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        result.reserve(array.size());
        for (const QJsonValue &entry : array) {
            if (entry.isString()) {
                result.append(entry.toString());
            }
        }
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.value().isString()) {
                result.append(it.value().toString());
            } else {
                result.append(it.key());
            }
        }
        result.sort();
    }
    return result;
}

} // namespace Json
