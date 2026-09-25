#pragma once

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace HubPaths {

inline QString systemdWorkingDirectory(bool userInstance)
{
    const QStringList units{
        QStringLiteral("openlinkhub.service"),
        QStringLiteral("OpenLinkHub.service"),
    };
    for (const QString &unit : units) {
        QStringList args{QStringLiteral("show"), QStringLiteral("-p"), QStringLiteral("WorkingDirectory"), QStringLiteral("--value"), unit};
        if (userInstance) {
            args.prepend(QStringLiteral("--user"));
        }
        QProcess process;
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(QStringLiteral("systemctl"), args);
        if (!process.waitForFinished(1500)) {
            process.kill();
            continue;
        }
        const QString dir = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!dir.isEmpty() && dir != QLatin1Char('/') && QDir(dir).exists()) {
            return dir;
        }
    }
    return {};
}

inline QStringList candidateDirs()
{
    QStringList dirs;
    auto add = [&dirs](const QString &dir) {
        if (!dir.isEmpty() && !dirs.contains(dir)) {
            dirs.append(dir);
        }
    };
    add(systemdWorkingDirectory(false));
    add(systemdWorkingDirectory(true));
    add(QStringLiteral("/opt/OpenLinkHub"));
    add(QStringLiteral("/etc/OpenLinkHub"));
    add(QStringLiteral("/var/lib/openlinkhub"));
    add(QDir::homePath() + QStringLiteral("/.local/share/openlinkhub"));
    add(QDir::homePath() + QStringLiteral("/OpenLinkHub"));
    return dirs;
}

inline QString configDir()
{
    for (const QString &dir : candidateDirs()) {
        if (QFile::exists(dir + QStringLiteral("/database/scheduler.json"))
            || QFile::exists(dir + QStringLiteral("/config.json"))) {
            return dir;
        }
    }
    return {};
}

inline QString schedulerFile()
{
    const QString dir = configDir();
    if (dir.isEmpty()) {
        return {};
    }
    return dir + QStringLiteral("/database/scheduler.json");
}

} // namespace HubPaths
