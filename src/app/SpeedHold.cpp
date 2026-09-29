#include "app/SpeedHold.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonObject>

#include <memory>

namespace {

QString cleanSerial(const QString &serial)
{
    QString clean;
    for (const QChar character : serial) {
        if (character.isLetterOrNumber()) {
            clean.append(character);
        }
    }
    return clean;
}

KConfigGroup config()
{
    return KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("SpeedHold"));
}

QString entryKey(const QString &serial, int channelId)
{
    const QString channel = channelId < 0 ? QStringLiteral("all") : QString::number(channelId);
    return cleanSerial(serial) + QLatin1Char('_') + channel;
}

QString profileName(const QString &serial, int channelId, QChar slot)
{
    const QString channel = channelId < 0 ? QStringLiteral("All") : QString::number(channelId);
    return QStringLiteral("QtHold%1C%2%3").arg(cleanSerial(serial), channel, QString(slot));
}

QJsonArray flatPoints(int percent)
{
    return QJsonArray{
        QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), percent}},
        QJsonObject{{QStringLiteral("x"), 100}, {QStringLiteral("y"), percent}},
    };
}

QString failure(const QJsonObject &json, const QString &error)
{
    if (!error.isEmpty()) {
        return error;
    }
    return Json::str(json, "message");
}

void writeGraph(ApiClient *client, const QString &profile, int updateType, int percent, const std::function<void(const QString &error)> &next)
{
    client->put(QStringLiteral("/api/temperatures/updateGraph"),
                QJsonObject{
                    {QStringLiteral("profile"), profile},
                    {QStringLiteral("updateType"), updateType},
                    {QStringLiteral("points"), flatPoints(percent)},
                },
                [next](const QJsonObject &json, const QString &error) {
                    if (!error.isEmpty() || Json::integer(json, "status") != 1) {
                        next(failure(json, error));
                        return;
                    }
                    next({});
                });
}

} // namespace

bool SpeedHold::isHoldProfile(const QString &name)
{
    return name.startsWith(QLatin1String("QtHold"));
}

int SpeedHold::storedPercent(const QString &serial, int channelId)
{
    return config().readEntry(entryKey(serial, channelId) + QStringLiteral("_percent"), -1);
}

void SpeedHold::forget(const QString &serial, int channelId)
{
    KConfigGroup group = config();
    const QString key = entryKey(serial, channelId);
    group.deleteEntry(key);
    group.deleteEntry(key + QStringLiteral("_percent"));
    group.sync();
}

void SpeedHold::forgetAll(const QString &serial)
{
    KConfigGroup group = config();
    const QString prefix = cleanSerial(serial) + QLatin1Char('_');
    const QStringList keys = group.keyList();
    for (const QString &key : keys) {
        if (key.startsWith(prefix)) {
            group.deleteEntry(key);
        }
    }
    group.sync();
}

void SpeedHold::hold(ApiClient *client, const QString &serial, int channelId, int percent, const QList<Channel> &channels, const Done &done)
{
    if (!client) {
        if (done) {
            done(QStringLiteral("Missing connection"), {});
        }
        return;
    }

    const int clamped = qBound(0, percent, 100);
    KConfigGroup group = config();
    bool usingSlotA = false;
    const QString slotA = profileName(serial, channelId, QLatin1Char('A'));
    for (const Channel &channel : channels) {
        if (channelId >= 0 && channel.id != channelId) {
            continue;
        }
        if (!isHoldProfile(channel.profile) && !channel.profile.isEmpty()) {
            group.writeEntry(entryKey(serial, channel.id), channel.profile);
        }
        group.writeEntry(entryKey(serial, channel.id) + QStringLiteral("_percent"), clamped);
        if (channel.profile == slotA) {
            usingSlotA = true;
        }
    }
    group.sync();

    // The daemon reapplies a curve when its name changes, so alternate between
    // two flat profiles. Updating the profile that is already assigned waits
    // until the temperature changes.
    const QString target = usingSlotA ? profileName(serial, channelId, QLatin1Char('B')) : slotA;
    const auto finish = [done](const QString &error, const QString &profile) {
        if (done) {
            done(error, profile);
        }
    };

    const auto assign = [client, serial, channelId, target, finish]() {
        client->post(QStringLiteral("/api/speed"),
                     QJsonObject{
                         {QStringLiteral("deviceId"), serial},
                         {QStringLiteral("channelId"), channelId},
                         {QStringLiteral("profile"), target},
                     },
                     [finish, target](const QJsonObject &json, const QString &error) {
                         if (!error.isEmpty() || Json::integer(json, "status") != 1) {
                             finish(failure(json, error), {});
                             return;
                         }
                         finish({}, target);
                     });
    };

    const auto writeCurves = [client, target, clamped, finish, assign]() {
        writeGraph(client, target, 0, clamped, [client, target, clamped, finish, assign](const QString &error) {
            if (!error.isEmpty()) {
                finish(error, {});
                return;
            }
            writeGraph(client, target, 1, clamped, [finish, assign](const QString &error) {
                if (!error.isEmpty()) {
                    finish(error, {});
                    return;
                }
                assign();
            });
        });
    };

    client->post(QStringLiteral("/api/temperatures/new"),
                 QJsonObject{
                     {QStringLiteral("profile"), target},
                     {QStringLiteral("sensor"), 0},
                     {QStringLiteral("zeroRpm"), false},
                     {QStringLiteral("static"), true},
                     {QStringLiteral("linear"), false},
                 },
                 [finish, writeCurves](const QJsonObject &, const QString &error) {
                     if (!error.isEmpty()) {
                         finish(error, {});
                         return;
                     }
                     // A second hold finds the profile already created.
                     writeCurves();
                 });
}

void SpeedHold::release(ApiClient *client, const QString &serial, const QList<int> &channelIds, const Done &done)
{
    if (!client) {
        if (done) {
            done(QStringLiteral("Missing connection"), {});
        }
        return;
    }

    auto remaining = std::make_shared<QList<int>>(channelIds);
    auto restored = std::make_shared<QString>();
    auto step = std::make_shared<std::function<void()>>();
    *step = [client, serial, remaining, restored, done, step]() {
        if (remaining->isEmpty()) {
            if (done) {
                done({}, *restored);
            }
            return;
        }
        const int channelId = remaining->takeFirst();
        const QString profile = config().readEntry(entryKey(serial, channelId), QString());
        if (profile.isEmpty() || isHoldProfile(profile)) {
            (*step)();
            return;
        }
        client->post(QStringLiteral("/api/speed"),
                     QJsonObject{
                         {QStringLiteral("deviceId"), serial},
                         {QStringLiteral("channelId"), channelId},
                         {QStringLiteral("profile"), profile},
                     },
                     [serial, channelId, profile, restored, done, step](const QJsonObject &json, const QString &error) {
                         if (!error.isEmpty() || Json::integer(json, "status") != 1) {
                             if (done) {
                                 done(failure(json, error), {});
                             }
                             return;
                         }
                         forget(serial, channelId);
                         *restored = profile;
                         (*step)();
                     });
    };
    (*step)();
}
