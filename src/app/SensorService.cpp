#include "app/SensorService.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QStringList>
#include <QVariant>

namespace {
constexpr auto ServiceName = "openlinkhub-sensors.service";
}

SensorService::SensorService(QObject *parent)
    : QObject(parent)
{
    refresh();
}

bool SensorService::isRunning() const
{
    return m_running;
}

bool SensorService::isEnabled() const
{
    return m_enabled;
}

QString SensorService::statusText() const
{
    return m_status;
}

bool SensorService::call(const QString &method, const QList<QVariant> &args)
{
    QDBusInterface systemd(QStringLiteral("org.freedesktop.systemd1"),
                           QStringLiteral("/org/freedesktop/systemd1"),
                           QStringLiteral("org.freedesktop.systemd1.Manager"),
                           QDBusConnection::sessionBus());
    QDBusMessage reply = systemd.callWithArgumentList(QDBus::Block, method, args);
    return reply.type() != QDBusMessage::ErrorMessage;
}

void SensorService::refresh()
{
    QDBusInterface systemd(QStringLiteral("org.freedesktop.systemd1"),
                           QStringLiteral("/org/freedesktop/systemd1"),
                           QStringLiteral("org.freedesktop.systemd1.Manager"),
                           QDBusConnection::sessionBus());

    QDBusReply<QString> unitFileState = systemd.call(QStringLiteral("GetUnitFileState"), QLatin1String(ServiceName));
    m_enabled = unitFileState.isValid() && (unitFileState.value() == QLatin1String("enabled") || unitFileState.value() == QLatin1String("enabled-runtime"));

    QDBusReply<QDBusObjectPath> unit = systemd.call(QStringLiteral("GetUnit"), QLatin1String(ServiceName));
    m_running = false;
    m_status = unitFileState.isValid() ? unitFileState.value() : tr("not installed");
    if (unit.isValid()) {
        QDBusInterface unitIface(QStringLiteral("org.freedesktop.systemd1"),
                                 unit.value().path(),
                                 QStringLiteral("org.freedesktop.systemd1.Unit"),
                                 QDBusConnection::sessionBus());
        const QVariant active = unitIface.property("ActiveState");
        m_status = active.toString();
        m_running = active.toString() == QLatin1String("active");
    }
    Q_EMIT changed();
}

bool SensorService::start()
{
    const bool ok = call(QStringLiteral("StartUnit"), {QLatin1String(ServiceName), QStringLiteral("replace")});
    refresh();
    return ok;
}

bool SensorService::stop()
{
    const bool ok = call(QStringLiteral("StopUnit"), {QLatin1String(ServiceName), QStringLiteral("replace")});
    refresh();
    return ok;
}

bool SensorService::setEnabled(bool enabled)
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                          QStringLiteral("/org/freedesktop/systemd1"),
                                                          QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                          enabled ? QStringLiteral("EnableUnitFiles") : QStringLiteral("DisableUnitFiles"));
    if (enabled) {
        message.setArguments({QStringList{QLatin1String(ServiceName)}, false, true});
    } else {
        message.setArguments({QStringList{QLatin1String(ServiceName)}, false});
    }
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message);
    const bool ok = reply.type() != QDBusMessage::ErrorMessage;
    call(QStringLiteral("Reload"));
    if (enabled) {
        start();
    } else {
        stop();
    }
    refresh();
    return ok;
}

bool SensorService::restartSystemMonitor()
{
    return call(QStringLiteral("RestartUnit"), {QStringLiteral("plasma-ksystemstats.service"), QStringLiteral("replace")});
}
