#include "app/TrayController.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "app/SpeedHold.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QAction>
#include <QActionGroup>
#include <QSignalBlocker>
#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSlider>
#include <QSvgRenderer>
#include <QSystemTrayIcon>
#include <QWidgetAction>

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
        refreshLights();
    });
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            toggleWindow();
        }
    });
    m_tray->show();
    QApplication::setQuitOnLastWindowClosed(false);
    refreshLights();
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

    const QJsonObject channels = Json::object(device, "devices");
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        const QJsonObject channel = it.value().toObject();
        ChannelState row;
        row.id = Json::integer(channel, "channelId", it.key().toInt());
        row.profile = Json::str(channel, "profile");
        row.hasSpeed = Json::boolean(channel, "HasSpeed");
        row.psu = Json::boolean(channel, "IsPSU");
        state.channels.append(row);
    }
    return state;
}

void TrayController::refreshLights()
{
    if (!m_client) {
        return;
    }
    m_client->get(QStringLiteral("/api/dashboard"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject dashboard = Json::object(json, "dashboard");
        if (dashboard.isEmpty()) {
            return;
        }
        m_dashboard = dashboard;
        m_haveDashboard = true;
        m_lightsOff = Json::boolean(dashboard, "rgbOff");
        if (m_lightsAction) {
            const QSignalBlocker blocker(m_lightsAction);
            m_lightsAction->setChecked(m_lightsOff);
        }
    });
}

void TrayController::setLightsOff(bool off)
{
    if (!m_haveDashboard) {
        refreshLights();
        return;
    }
    QJsonObject body = m_dashboard;
    body.insert(QStringLiteral("rgbOff"), off);
    m_client->post(QStringLiteral("/api/dashboard/update"), body, [this, off](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) {
            if (m_lightsAction) {
                const QSignalBlocker blocker(m_lightsAction);
                m_lightsAction->setChecked(m_lightsOff);
            }
            return;
        }
        m_lightsOff = off;
        m_dashboard.insert(QStringLiteral("rgbOff"), off);
    });
}

void TrayController::rebuildMenu()
{
    if (!m_menu) {
        return;
    }
    m_lightsAction = nullptr;
    m_menu->clear();

    const bool visible = m_window && m_window->isVisible();
    QAction *toggle = m_menu->addAction(visible ? tr("Hide OpenLinkHub") : tr("Show OpenLinkHub"));
    connect(toggle, &QAction::triggered, this, &TrayController::toggleWindow);

    m_lightsAction = m_menu->addAction(tr("Lights off"));
    m_lightsAction->setCheckable(true);
    m_lightsAction->setChecked(m_lightsOff);
    m_lightsAction->setEnabled(m_haveDashboard);
    connect(m_lightsAction, &QAction::toggled, this, &TrayController::setLightsOff);
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
            if (!device.speedProfile.isEmpty() && !SpeedHold::isHoldProfile(device.speedProfile) && !profiles.contains(device.speedProfile)) {
                profiles.prepend(device.speedProfile);
            }
            for (const QString &profile : profiles) {
                if (SpeedHold::isHoldProfile(profile)) {
                    continue;
                }
                QAction *action = speeds->addAction(profile);
                action->setCheckable(true);
                action->setChecked(!SpeedHold::isHoldProfile(device.speedProfile) && profile == device.speedProfile);
                action->setData(profile);
                group->addAction(action);
            }
            const QString serial = device.serial;
            connect(group, &QActionGroup::triggered, this, [this, serial](QAction *action) {
                setSpeedProfile(serial, action->data().toString());
            });

            QMenu *hold = menu->addMenu(tr("Hold"));
            // QWidgetAction deletes this widget. A menu parent would delete it too.
            auto *host = new QWidget;
            auto *layout = new QHBoxLayout(host);
            layout->setContentsMargins(12, 4, 12, 4);
            auto *slider = new QSlider(Qt::Horizontal, host);
            slider->setRange(30, 100);
            int heldPercent = -1;
            for (const ChannelState &channel : device.channels) {
                if (!channel.hasSpeed || channel.psu || !SpeedHold::isHoldProfile(channel.profile)) {
                    continue;
                }
                heldPercent = SpeedHold::storedPercent(device.serial, channel.id);
                if (heldPercent >= 30) {
                    break;
                }
            }
            slider->setValue(heldPercent >= 30 ? heldPercent : 40);
            auto *value = new QLabel(QStringLiteral("%1%").arg(slider->value()), host);
            value->setMinimumWidth(value->fontMetrics().horizontalAdvance(QStringLiteral("100%")));
            layout->addWidget(slider, 1);
            layout->addWidget(value);
            auto *sliderAction = new QWidgetAction(hold);
            sliderAction->setDefaultWidget(host);
            hold->addAction(sliderAction);
            connect(slider, &QSlider::valueChanged, value, [value](int level) {
                value->setText(QStringLiteral("%1%").arg(level));
            });
            connect(slider, &QSlider::sliderReleased, this, [this, slider, serial]() {
                holdDevice(serial, slider->value());
            });
            QAction *release = hold->addAction(tr("Use profile"));
            connect(release, &QAction::triggered, this, [this, serial]() {
                releaseDevice(serial);
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
        noteWindowHidden();
        return;
    }
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void TrayController::toggleLights()
{
    if (!m_haveDashboard) {
        refreshLights();
        return;
    }
    setLightsOff(!m_lightsOff);
}

void TrayController::toggleSidetone()
{
    bool any = false;
    bool anyOn = false;
    for (const DeviceState &device : m_details) {
        if (!device.sidetone) {
            continue;
        }
        any = true;
        if (device.sidetoneOn) {
            anyOn = true;
        }
    }
    if (!any) {
        if (m_details.isEmpty()) {
            refreshDetails();
        }
        return;
    }
    const bool enable = !anyOn;
    const QList<DeviceState> devices = m_details;
    for (const DeviceState &device : devices) {
        if (device.sidetone) {
            setSidetone(device.serial, enable);
        }
    }
}

void TrayController::holdDevice(const QString &serial, int percent)
{
    const DeviceState *state = nullptr;
    for (const DeviceState &device : m_details) {
        if (device.serial == serial) {
            state = &device;
            break;
        }
    }
    if (!state) {
        return;
    }
    QList<SpeedHold::Channel> channels;
    for (const ChannelState &channel : state->channels) {
        if (!channel.hasSpeed || channel.psu) {
            continue;
        }
        channels.append({channel.id, channel.profile});
    }
    if (channels.isEmpty()) {
        return;
    }
    SpeedHold::hold(m_client, serial, -1, percent, channels, [this, serial](const QString &, const QString &) {
        Q_EMIT deviceChanged(serial);
    });
}

void TrayController::releaseDevice(const QString &serial)
{
    const DeviceState *state = nullptr;
    for (const DeviceState &device : m_details) {
        if (device.serial == serial) {
            state = &device;
            break;
        }
    }
    if (!state) {
        return;
    }
    QList<int> channelIds;
    for (const ChannelState &channel : state->channels) {
        if (channel.hasSpeed && !channel.psu) {
            channelIds.append(channel.id);
        }
    }
    SpeedHold::release(m_client, serial, channelIds, [this, serial](const QString &, const QString &) {
        Q_EMIT deviceChanged(serial);
    });
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
                   [this, serial](const QJsonObject &json, const QString &error) {
                       if (error.isEmpty() && Json::integer(json, "status") == 1) {
                           SpeedHold::forgetAll(serial);
                       }
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
