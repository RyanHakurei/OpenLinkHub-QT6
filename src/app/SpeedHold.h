#pragma once

#include <QList>
#include <QString>
#include <functional>

class ApiClient;

// Flat speed profiles named QtHold… The daemon only accepts a direct percent
// while its manual mode is on, and that mode stops temperature curves entirely.
// A flat curve assigned as a normal profile holds the percent, and assigning
// the previous profile lets the curve take over again.
class SpeedHold
{
public:
    struct Channel {
        int id = 0;
        QString profile;
    };

    using Done = std::function<void(const QString &error, const QString &appliedProfile)>;

    static bool isHoldProfile(const QString &name);
    static int storedPercent(const QString &serial, int channelId);
    static void forget(const QString &serial, int channelId);
    static void forgetAll(const QString &serial);
    static void hold(ApiClient *client, const QString &serial, int channelId, int percent, const QList<Channel> &channels, const Done &done);
    static void release(ApiClient *client, const QString &serial, const QList<int> &channelIds, const Done &done);
};
