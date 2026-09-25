#include "app/DaemonService.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QProcess>
#include <QTimer>

namespace {

QStringList unitNames()
{
    return {
        QStringLiteral("openlinkhub.service"),
        QStringLiteral("OpenLinkHub.service"),
    };
}

QDBusConnection busFor(bool user)
{
    return user ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
}

bool interactiveAuthWasNotAttempted(const QString &name, const QString &message)
{
    return name == QLatin1String("org.freedesktop.DBus.Error.InteractiveAuthorizationRequired")
        || message.contains(QLatin1String("Interactive authentication required"), Qt::CaseInsensitive);
}

bool authenticationCancelled(const QString &name, const QString &message)
{
    return name.contains(QLatin1String("Cancelled"), Qt::CaseInsensitive)
        || name.contains(QLatin1String("Canceled"), Qt::CaseInsensitive)
        || message.contains(QLatin1String("cancel"), Qt::CaseInsensitive);
}

} // namespace

DaemonService::DaemonService(QObject *parent)
    : QObject(parent)
{
    refresh();
}

bool DaemonService::canRestart() const
{
    return m_unit.found && !m_busy;
}

bool DaemonService::isBusy() const
{
    return m_busy;
}

QString DaemonService::statusText() const
{
    return m_status;
}

QString DaemonService::friendlyState(const QString &active)
{
    if (active == QLatin1String("active")) {
        return tr("Running");
    }
    if (active == QLatin1String("activating") || active == QLatin1String("reloading")) {
        return tr("Starting");
    }
    if (active == QLatin1String("deactivating")) {
        return tr("Stopping");
    }
    if (active == QLatin1String("failed")) {
        return tr("Failed");
    }
    return tr("Stopped");
}

DaemonService::Unit DaemonService::locate() const
{
    const struct Candidate {
        bool user;
    } candidates[] = {{false}, {true}};

    for (const Candidate &candidate : candidates) {
        QDBusInterface systemd(QStringLiteral("org.freedesktop.systemd1"),
                               QStringLiteral("/org/freedesktop/systemd1"),
                               QStringLiteral("org.freedesktop.systemd1.Manager"),
                               busFor(candidate.user));
        for (const QString &name : unitNames()) {
            const QDBusReply<QString> state = systemd.call(QStringLiteral("GetUnitFileState"), name);
            if (!state.isValid() || state.value().isEmpty() || state.value() == QLatin1String("not-found")
                || state.value() == QLatin1String("masked")) {
                continue;
            }
            return {name, candidate.user, true};
        }
    }
    return {};
}

void DaemonService::refresh()
{
    m_unit = locate();
    if (!m_unit.found) {
        m_status = tr("Not installed");
        Q_EMIT changed();
        return;
    }

    QDBusInterface systemd(QStringLiteral("org.freedesktop.systemd1"),
                           QStringLiteral("/org/freedesktop/systemd1"),
                           QStringLiteral("org.freedesktop.systemd1.Manager"),
                           busFor(m_unit.user));
    const QDBusReply<QDBusObjectPath> unit = systemd.call(QStringLiteral("GetUnit"), m_unit.name);
    if (!unit.isValid()) {
        m_status = tr("Stopped");
        Q_EMIT changed();
        return;
    }

    QDBusInterface unitIface(QStringLiteral("org.freedesktop.systemd1"),
                             unit.value().path(),
                             QStringLiteral("org.freedesktop.systemd1.Unit"),
                             busFor(m_unit.user));
    const QString active = unitIface.property("ActiveState").toString();
    m_status = friendlyState(active);
    Q_EMIT changed();
}

QString DaemonService::friendlyError(const QString &name, const QString &message) const
{
    if (authenticationCancelled(name, message)) {
        return tr("Authentication cancelled");
    }
    if (name.contains(QLatin1String("AccessDenied")) || name.contains(QLatin1String("Auth"))) {
        return tr("Administrator permission is required to restart OpenLinkHub");
    }
    if (name.contains(QLatin1String("NoSuchUnit"))) {
        return tr("OpenLinkHub service was not found");
    }
    if (!message.isEmpty()) {
        return message;
    }
    return tr("Could not restart OpenLinkHub");
}

void DaemonService::restart()
{
    if (m_busy) {
        return;
    }
    if (!m_unit.found) {
        refresh();
    }
    if (!m_unit.found) {
        Q_EMIT restartFinished(false, tr("OpenLinkHub service was not found"));
        return;
    }
    restartOnBus();
}

void DaemonService::restartOnBus()
{
    m_busy = true;
    Q_EMIT changed();

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                          QStringLiteral("/org/freedesktop/systemd1"),
                                                          QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                          QStringLiteral("RestartUnit"));
    message.setArguments({m_unit.name, QStringLiteral("replace")});
    if (!m_unit.user) {
        message.setInteractiveAuthorizationAllowed(true);
    }

    QDBusPendingCall call = busFor(m_unit.user).asyncCall(message);
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            const QString name = reply.error().name();
            const QString message = reply.error().message();
            if (!m_unit.user && interactiveAuthWasNotAttempted(name, message)) {
                restartWithPkexec();
                return;
            }
            m_busy = false;
            Q_EMIT restartFinished(false, friendlyError(name, message));
            Q_EMIT changed();
            refresh();
            return;
        }
        m_busy = false;
        Q_EMIT restartFinished(true, tr("OpenLinkHub is restarting"));
        Q_EMIT changed();
        QTimer::singleShot(1500, this, &DaemonService::refresh);
        QTimer::singleShot(4000, this, &DaemonService::refresh);
    });
}

void DaemonService::restartWithPkexec()
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        process->deleteLater();
        m_busy = false;
        Q_EMIT restartFinished(false, tr("Could not ask for permission to restart OpenLinkHub"));
        Q_EMIT changed();
    });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
        process->deleteLater();
        m_busy = false;
        if (status == QProcess::NormalExit && code == 0) {
            Q_EMIT restartFinished(true, tr("OpenLinkHub is restarting"));
            QTimer::singleShot(1500, this, &DaemonService::refresh);
            QTimer::singleShot(4000, this, &DaemonService::refresh);
        } else if (code == 126 || authenticationCancelled({}, error)) {
            Q_EMIT restartFinished(false, tr("Authentication cancelled"));
        } else {
            Q_EMIT restartFinished(false, error.isEmpty() ? tr("Could not restart OpenLinkHub") : error);
        }
        Q_EMIT changed();
        refresh();
    });
    process->start(QStringLiteral("pkexec"), {QStringLiteral("systemctl"), QStringLiteral("restart"), m_unit.name});
}
