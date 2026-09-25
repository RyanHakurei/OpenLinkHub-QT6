#include "api/ApiClient.h"
#include "app/AppSettings.h"
#include "sensors/SensorSnapshot.h"

#include <QCoreApplication>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationDomain(QStringLiteral("freyja.pw"));
    QCoreApplication::setApplicationName(QStringLiteral("openlinkhub-qt"));

    AppSettings settings;
    auto *client = new ApiClient(&app);
    client->setBaseUrl(settings.baseUrl());

    auto poll = [client, &settings]() {
        settings.load();
        client->setBaseUrl(settings.baseUrl());
        client->get(QStringLiteral("/api/devices/"), [](const QJsonObject &json, const QString &error) {
            if (!error.isEmpty()) {
                return;
            }
            SensorSnapshot::write(SensorSnapshot::fromDevices(json));
        });
    };

    auto *timer = new QTimer(&app);
    timer->setInterval(qMax(500, settings.pollIntervalMs()));
    QObject::connect(timer, &QTimer::timeout, &app, poll);
    poll();
    timer->start();
    return app.exec();
}
