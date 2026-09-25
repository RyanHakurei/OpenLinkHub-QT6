#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QStringList>

namespace LcdData {

inline QString directory()
{
    const QStringList candidates{
        QStringLiteral("/var/lib/openlinkhub/database/lcd"),
        QStringLiteral("/opt/OpenLinkHub/database/lcd"),
        QDir::homePath() + QStringLiteral("/.local/share/openlinkhub/database/lcd"),
    };
    for (const QString &path : candidates) {
        if (QDir(path).exists()) {
            return path;
        }
    }
    return candidates.first();
}

inline QJsonObject load(const QString &fileName)
{
    QFile file(directory() + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

inline QStringList images()
{
    QStringList names;
    const QDir dir(directory() + QStringLiteral("/images"));
    const QFileInfoList files = dir.entryInfoList(
        {QStringLiteral("*.gif"),
         QStringLiteral("*.jpg"),
         QStringLiteral("*.jpeg"),
         QStringLiteral("*.webp"),
         QStringLiteral("*.bmp")},
        QDir::Files,
        QDir::Name);
    for (const QFileInfo &info : files) {
        names.append(info.completeBaseName());
    }
    return names;
}

inline QList<QPair<int, QString>> sensors()
{
    return {
        {0, QStringLiteral("CPU Temp")},
        {1, QStringLiteral("GPU Temp")},
        {2, QStringLiteral("Liquid Temp")},
        {3, QStringLiteral("CPU Load")},
        {4, QStringLiteral("GPU Load")},
        {5, QStringLiteral("Pump RPM")},
        {20, QStringLiteral("Date")},
        {21, QStringLiteral("Time")},
    };
}

} // namespace LcdData
