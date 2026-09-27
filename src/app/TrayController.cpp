#include "app/TrayController.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QSystemTrayIcon>

#include <memory>

namespace {

// Plasma's tray looks up a themed icon by name and will substitute the color
// Papirus icon for openlinkhub-qt-symbolic. A pixmap-only icon has no name,
// so the white artwork is what actually gets shown.
QIcon whiteTrayIcon()
{
    QSvgRenderer renderer(QStringLiteral(":/icons/openlinkhub-qt-symbolic.svg"));
    QIcon icon;
    if (!renderer.isValid()) {
        return icon;
    }
    // Do not set a device pixel ratio. Plasma's tray uses the pixmap width as
    // the image width and clips a scaled pixmap down to its left edge.
    for (const int size : {16, 22, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

TrayController::TrayController(ApiClient *client, QWidget *window, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_window(window)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_menu = new QMenu(window);
    m_tray = new QSystemTrayIcon(window);
    QIcon icon = whiteTrayIcon();
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("openlinkhub-qt-symbolic"));
    }
    m_tray->setIcon(icon);
    m_tray->setToolTip(tr("OpenLinkHub"));
    m_tray->setContextMenu(m_menu);
    connect(m_menu, &QMenu::aboutToShow, this, [this]() {
        m_rebuildWhenReady = m_details.isEmpty();
        rebuildMenu();
        refreshDetails();
    });
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            toggleWindow();
        }
    });
    m_tray->show();
    QApplication::setQuitOnLastWindowClosed(false);
}

bool TrayController::available() const
{
    return m_tray != nullptr;
}

void TrayController::noteWindowHidden()
{
    if (!m_tray) {
        return;
    }
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("Interface"));
    if (group.readEntry(QStringLiteral("TrayCloseNoticeShown"), false)) {
        return;
    }
    m_tray->showMessage(tr("OpenLinkHub"),
                        tr("OpenLinkHub is still running in the system tray."),
                        QSystemTrayIcon::Information,
                        5000);
    group.writeEntry(QStringLiteral("TrayCloseNoticeShown"), true);
    group.sync();
}

void TrayController::setSpeedProfiles(const QStringList &profiles)
{
    if (m_speedProfiles == profiles) {
        return;
    }
    m_speedProfiles = profiles;
    if (!m_deviceList.isEmpty()) {
        refreshDetails();
    }
}

void TrayController::setDevices(const QList<QPair<QString, QString>> &devices)
{
    if (devices == m_deviceList && !m_details.isEmpty()) {
        return;
    }
    m_deviceList = devices;
    refreshDetails();
}

void TrayController::refreshDetails()
{
    if (!m_tray) {
        return;
    }
    const int generation = ++m_generation;
    if (m_deviceList.isEmpty()) {
        m_details.clear();
        return;
    }

    auto pending = std::make_shared<int>(m_deviceList.size());
    auto collected = std::make_shared<QHash<QString, DeviceState>>();
    for (const QPair<QString, QString> &device : m_deviceList) {
        const QString serial = device.first;
        const QString product = device.second;
        m_client->get(QStringLiteral("/api/devices/") + serial, [this, generation, pending, collected, serial, product](const QJsonObject &json, const QString &error) {
            if (generation != m_generation) {
                return;
            }
            if (error.isEmpty()) {
                collected->insert(serial, parseDevice(serial, product, Json::object(json, "device")));
            }
            if (--(*pending) != 0) {
                return;
            }
            m_details.clear();
            for (const QPair<QString, QString> &listed : m_deviceList) {
                if (collected->contains(listed.first)) {
                    m_details.append(collected->value(listed.first));
                }
            }
            if (m_menu->isVisible() && m_rebuildWhenReady) {
                m_rebuildWhenReady = false;
                rebuildMenu();
            }
        });
    }
}

TrayController::DeviceState TrayController::parseDevice(const QString &serial, const QString &product, const QJsonObject &device) const
{
    DeviceState state;
    state.serial = serial;
    state.product = Json::strAny(device, {QStringLiteral("product"), QStringLiteral("Product")}, product);
    if (state.product.isEmpty()) {
        state.product = product;
    }

    const QJsonObject profile = Json::object(device, "DeviceProfile");
    state.sidetone = profile.contains(QLatin1String("SideTone")) || profile.contains(QLatin1String("SideToneValue"));
    state.sidetoneOn = Json::integer(profile, "SideTone") != 0;
    state.hasSpeed = !m_speedProfiles.isEmpty() && !Json::object(device, "devices").isEmpty();
    state.speedProfile = Json::str(profile, "MultiProfile");

    const QJsonObject userProfiles = Json::object(device, "userProfiles");
    for (auto it = userProfiles.begin(); it != userProfiles.end(); ++it) {
        state.userProfiles.append(it.key());
        if (it.value().isObject() && Json::boolean(it.value().toObject(), "Active")) {
            state.activeUserProfile = it.key();
        }
    }
    state.userProfiles.sort();
    return state;
}

void TrayController::rebuildMenu()
{
    if (!m_menu) {
        return;
    }
    m_menu->clear();

    const bool visible = m_window && m_window->isVisible();
    QAction *toggle = m_menu->addAction(visible ? tr("Hide OpenLinkHub") : tr("Show OpenLinkHub"));
    connect(toggle, &QAction::triggered, this, &TrayController::toggleWindow);
    m_menu->addSeparator();

    bool anyDevice = false;
    for (const DeviceState &device : m_details) {
        const bool hasProfiles = device.userProfiles.size() > 1;
        if (!device.sidetone && !device.hasSpeed && !hasProfiles) {
            continue;
        }
        anyDevice = true;
        QMenu *menu = m_menu->addMenu(device.product);
        if (device.sidetone) {
            QAction *sidetone = menu->addAction(tr("Sidetone"));
            sidetone->setCheckable(true);
            sidetone->setChecked(device.sidetoneOn);
            const QString serial = device.serial;
            connect(sidetone, &QAction::toggled, this, [this, serial](bool enabled) {
                setSidetone(serial, enabled);
            });
        }
        if (device.hasSpeed) {
            QMenu *speeds = menu->addMenu(tr("Speed"));
            auto *group = new QActionGroup(speeds);
            group->setExclusive(true);
            QStringList profiles = m_speedProfiles;
            if (!device.speedProfile.isEmpty() && !profiles.contains(device.speedProfile)) {
                profiles.prepend(device.speedProfile);
            }
            for (const QString &profile : profiles) {
                QAction *action = speeds->addAction(profile);
                action->setCheckable(true);
                action->setChecked(profile == device.speedProfile);
                action->setData(profile);
                group->addAction(action);
            }
            const QString serial = device.serial;
            connect(group, &QActionGroup::triggered, this, [this, serial](QAction *action) {
                setSpeedProfile(serial, action->data().toString());
            });
        }
        if (hasProfiles) {
            QMenu *profiles = menu->addMenu(tr("Profile"));
            auto *group = new QActionGroup(profiles);
            group->setExclusive(true);
            for (const QString &profile : device.userProfiles) {
                QAction *action = profiles->addAction(profile);
                action->setCheckable(true);
                action->setChecked(profile == device.activeUserProfile);
                action->setData(profile);
                group->addAction(action);
            }
            const QString serial = device.serial;
            connect(group, &QActionGroup::triggered, this, [this, serial](QAction *action) {
                setUserProfile(serial, action->data().toString());
            });
        }
    }
    if (!anyDevice && m_deviceList.isEmpty()) {
        QAction *empty = m_menu->addAction(tr("No devices"));
        empty->setEnabled(false);
    } else if (!anyDevice && m_details.isEmpty() && !m_deviceList.isEmpty()) {
        QAction *loading = m_menu->addAction(tr("Loading devices…"));
        loading->setEnabled(false);
    }

    m_menu->addSeparator();
    QAction *quit = m_menu->addAction(tr("Quit"));
    connect(quit, &QAction::triggered, this, &TrayController::quitRequested);
}

void TrayController::toggleWindow()
{
    if (!m_window) {
        return;
    }
    if (m_window->isVisible()) {
        m_window->hide();
        return;
    }
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void TrayController::setSidetone(const QString &serial, bool enabled)
{
    for (DeviceState &device : m_details) {
        if (device.serial == serial) {
            device.sidetoneOn = enabled;
            break;
        }
    }
    m_client->post(QStringLiteral("/api/headset/sidetone"),
                   QJsonObject{
                       {QStringLiteral("deviceId"), serial},
                       {QStringLiteral("sideTone"), enabled ? 1 : 0},
                   },
                   [this, serial](const QJsonObject &, const QString &) {
                       Q_EMIT deviceChanged(serial);
                   });
}

void TrayController::setSpeedProfile(const QString &serial, const QString &profile)
{
    for (DeviceState &device : m_details) {
        if (device.serial == serial) {
            device.speedProfile = profile;
            break;
        }
    }
    m_client->post(QStringLiteral("/api/speed"),
                   QJsonObject{
                       {QStringLiteral("deviceId"), serial},
                       {QStringLiteral("channelId"), -1},
                       {QStringLiteral("profile"), profile},
                   },
                   [this, serial](const QJsonObject &, const QString &) {
                       Q_EMIT deviceChanged(serial);
                   });
}

void TrayController::setUserProfile(const QString &serial, const QString &profile)
{
    for (DeviceState &device : m_details) {
        if (device.serial == serial) {
            device.activeUserProfile = profile;
            break;
        }
    }
    m_client->post(QStringLiteral("/api/userProfile/change"),
                   QJsonObject{
                       {QStringLiteral("deviceId"), serial},
                       {QStringLiteral("userProfileName"), profile},
                   },
                   [this, serial](const QJsonObject &, const QString &) {
                       Q_EMIT deviceChanged(serial);
                   });
}
