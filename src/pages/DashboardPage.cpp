#include "pages/DashboardPage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "app/SpeedHold.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/Telemetry.h"
#include "widgets/UiHelpers.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

DashboardPage::DashboardPage(ApiClient *client, HubI18n *i18n, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_grid = new CardGrid(this);

    auto *addBox = Ui::card(m_i18n->t("txtAddDeviceToDashboard", "Add device to dashboard"));
    auto *form = Ui::form(addBox);
    m_deviceSelect = new QComboBox(addBox);
    auto *buttons = new QWidget(addBox);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    auto *addButton = new QPushButton(m_i18n->t("txtSave", "Save"), buttons);
    auto *removeButton = new QPushButton(m_i18n->t("txtDelete", "Delete"), buttons);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    form->addRow(m_i18n->t("txtSelectDevice", "Select device"), m_deviceSelect);
    form->addRow(QString(), buttons);

    connect(addButton, &QPushButton::clicked, this, [this]() {
        const QString serial = m_deviceSelect->currentData().toString();
        if (serial.isEmpty()) {
            return;
        }
        m_client->post(QStringLiteral("/api/dashboard/devices/add"),
                       QJsonObject{{QStringLiteral("deviceId"), serial}},
                       [this](const QJsonObject &, const QString &) {
                           reload();
                       });
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        const QString serial = m_deviceSelect->currentData().toString();
        if (serial.isEmpty()) {
            return;
        }
        m_client->del(QStringLiteral("/api/dashboard/devices/delete"),
                      QJsonObject{{QStringLiteral("deviceId"), serial}},
                      [this](const QJsonObject &, const QString &) {
                          reload();
                      });
    });

    layout->addWidget(Ui::scrollWrap(m_grid), 1);
    layout->addWidget(addBox);
}

void DashboardPage::setShowLabels(bool show)
{
    m_showLabels = show;
}

void DashboardPage::setDeviceChoices(const QList<QPair<QString, QString>> &devices)
{
    const QString current = m_deviceSelect->currentData().toString();
    m_deviceSelect->clear();
    for (const auto &device : devices) {
        m_deviceSelect->addItem(device.second, device.first);
    }
    const int index = m_deviceSelect->findData(current);
    if (index >= 0) {
        m_deviceSelect->setCurrentIndex(index);
    }
}

void DashboardPage::reload()
{
    m_client->get(QStringLiteral("/api/dashboard/devices/get"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        QStringList serials;
        const QJsonValue devices = json.value(QStringLiteral("devices"));
        if (devices.isArray()) {
            for (const QJsonValue &value : devices.toArray()) {
                serials.append(value.toString());
            }
        }
        renderPinned(serials);
    });
}

void DashboardPage::refreshTelemetry()
{
    m_client->get(QStringLiteral("/api/dashboard/devices/get"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        QStringList serials;
        const QJsonValue devices = json.value(QStringLiteral("devices"));
        if (devices.isArray()) {
            for (const QJsonValue &value : devices.toArray()) {
                serials.append(value.toString());
            }
        }
        if (serials != m_pinned || m_grid->count() == 0) {
            renderPinned(serials);
            return;
        }
        for (const QString &serial : serials) {
            m_client->get(QStringLiteral("/api/devices/") + serial, [this, serial](const QJsonObject &json, const QString &error) {
                if (!error.isEmpty()) {
                    return;
                }
                updateDeviceCard(serial, Json::object(json, "device"));
            });
        }
    });
}

void DashboardPage::renderPinned(const QStringList &serials)
{
    m_pinned = serials;
    m_grid->clear();
    for (const QString &serial : serials) {
        m_client->get(QStringLiteral("/api/devices/") + serial, [this, serial](const QJsonObject &json, const QString &error) {
            if (!error.isEmpty()) {
                return;
            }
            addDeviceCard(serial, Json::object(json, "device"));
        });
    }
}

void DashboardPage::addDeviceCard(const QString &serial, const QJsonObject &device)
{
    const QJsonObject channels = Json::object(device, "devices");
    auto addChannel = [this, serial, &device](const QJsonObject &channel) {
        const QString name = Json::strAny(channel, {QStringLiteral("name"), QStringLiteral("product")}, Json::str(device, "product"));
        QString title = name;
        const QString label = Json::str(channel, "label");
        if (m_showLabels && !label.isEmpty()) {
            title += QStringLiteral(" — ") + label;
        }
        auto *box = Ui::card(title);
        auto *form = Ui::form(box);
        const QString channelKey = QString::number(Json::integer(channel, "channelId"));
        if (Json::boolean(channel, "HasSpeed") || channel.contains(QLatin1String("rpm"))) {
            form->addRow(m_i18n->t("txtSpeed", "Speed"),
                         Telemetry::valueLabel(box, Telemetry::id(serial, channelKey, QStringLiteral("rpm")),
                                              QStringLiteral("%1 RPM").arg(Json::integer(channel, "rpm"))));
        }
        if (Telemetry::hasTemperature(channel)) {
            const QString temp = Json::str(channel, "temperatureString");
            form->addRow(m_i18n->t("txtTemperature", "Temperature"),
                         Telemetry::valueLabel(box, Telemetry::id(serial, channelKey, QStringLiteral("temp")),
                                              temp.isEmpty() ? QString::number(Json::number(channel, "temperature"), 'f', 1) : temp));
        }
        Telemetry::addPsuFields(form, box, channel, serial, channelKey, m_i18n);
        const QString profile = Json::str(channel, "profile");
        if (!profile.isEmpty()) {
            QString profileText = profile;
            if (SpeedHold::isHoldProfile(profile)) {
                const int percent = SpeedHold::storedPercent(serial, channelKey.toInt());
                profileText = percent < 0 ? tr("Hold") : tr("Hold %1%").arg(percent);
            }
            form->addRow(m_i18n->t("txtProfile", "Profile"), new QLabel(profileText));
        }
        const QString rgb = Json::str(channel, "rgb");
        if (!rgb.isEmpty()) {
            form->addRow(m_i18n->t("txtRgb", "RGB"), new QLabel(rgb));
        }
        m_grid->addCard(box);
    };

    if (channels.isEmpty()) {
        addChannel(device);
        return;
    }

    QList<QPair<int, QJsonObject>> sorted;
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        const QJsonObject channel = it.value().toObject();
        const bool hasSpeed = Json::boolean(channel, "HasSpeed") || channel.contains(QLatin1String("rpm"));
        if (!hasSpeed && !Telemetry::hasTemperature(channel) && !Telemetry::hasPsuPower(channel) && Json::integer(channel, "rpm") == 0) {
            continue;
        }
        sorted.append({it.key().toInt(), channel});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
    for (const auto &entry : sorted) {
        addChannel(entry.second);
    }
}

void DashboardPage::updateDeviceCard(const QString &serial, const QJsonObject &device)
{
    const QJsonObject channels = Json::object(device, "devices");
    if (channels.isEmpty()) {
        Telemetry::updateChannelStats(this, device, serial, QStringLiteral("0"));
        return;
    }
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        if (it.value().isObject()) {
            Telemetry::updateChannelStats(this, it.value().toObject(), serial, it.key());
        }
    }
}
