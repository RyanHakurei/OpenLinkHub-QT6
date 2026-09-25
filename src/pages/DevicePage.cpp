#include "pages/DevicePage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "api/LcdData.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/Telemetry.h"
#include "widgets/UiHelpers.h"

#include <KColorButton>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>

DevicePage::DevicePage(ApiClient *client, HubI18n *i18n, const QString &serial, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
    , m_serial(serial)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_grid = new CardGrid(this);
    layout->addWidget(Ui::scrollWrap(m_grid));
}

QString DevicePage::serial() const
{
    return m_serial;
}

void DevicePage::setSpeedProfiles(const QStringList &profiles)
{
    m_speedProfiles = profiles;
}

void DevicePage::reload()
{
    m_client->get(QStringLiteral("/api/devices/") + m_serial, [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        rebuild(Json::object(json, "device"));
    });
}

void DevicePage::refreshTelemetry()
{
    if (m_building) {
        return;
    }
    m_client->get(QStringLiteral("/api/devices/") + m_serial, [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty() || m_building) {
            return;
        }
        const QJsonObject device = Json::object(json, "device");
        const QJsonObject channels = Json::object(device, "devices");
        if (channels.isEmpty()) {
            Telemetry::updateChannelStats(this, device, m_serial, QStringLiteral("0"));
            return;
        }
        for (auto it = channels.begin(); it != channels.end(); ++it) {
            if (it.value().isObject()) {
                Telemetry::updateChannelStats(this, it.value().toObject(), m_serial, it.key());
            }
        }
    });
}

void DevicePage::rebuild(const QJsonObject &device)
{
    m_building = true;
    m_device = device;
    m_grid->clear();
    addHeaderCard(device);
    addChannelCards(device);
    addHeadsetCards(device);
    m_building = false;
}

void DevicePage::fillCombo(QComboBox *combo, const QStringList &values, const QString &current)
{
    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const QString &value : values) {
        combo->addItem(value, value);
    }
    const int index = combo->findData(current);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

void DevicePage::setSpeed(int channelId, const QString &profile)
{
    m_client->post(QStringLiteral("/api/speed"),
                   QJsonObject{
                       {QStringLiteral("deviceId"), m_serial},
                       {QStringLiteral("channelId"), channelId},
                       {QStringLiteral("profile"), profile},
                   },
                   {});
}

void DevicePage::setRgb(int channelId, const QString &profile)
{
    m_client->post(QStringLiteral("/api/color"),
                   QJsonObject{
                       {QStringLiteral("deviceId"), m_serial},
                       {QStringLiteral("channelId"), channelId},
                       {QStringLiteral("profile"), profile},
                   },
                   {});
}

void DevicePage::setLabel(int channelId, const QString &label)
{
    m_client->post(QStringLiteral("/api/label"),
                   QJsonObject{
                       {QStringLiteral("deviceId"), m_serial},
                       {QStringLiteral("channelId"), channelId},
                       {QStringLiteral("deviceType"), 0},
                       {QStringLiteral("label"), label},
                   },
                   {});
}

void DevicePage::addHeaderCard(const QJsonObject &device)
{
    const QString product = Json::strAny(device, {QStringLiteral("product"), QStringLiteral("Product")}, tr("Device"));
    auto *box = Ui::card(product);
    auto *form = Ui::form(box);

    form->addRow(m_i18n->t("txtFirmware", "Firmware"), new QLabel(Json::str(device, "firmware", QStringLiteral("n/a"))));
    form->addRow(m_i18n->t("txtSerial", "Serial"), new QLabel(Json::strAny(device, {QStringLiteral("serial"), QStringLiteral("Serial")}, m_serial)));

    const QJsonObject profile = Json::object(device, "DeviceProfile");
    const QJsonObject userProfiles = Json::object(device, "userProfiles");
    if (!userProfiles.isEmpty()) {
        auto *combo = new QComboBox(box);
        QString active;
        for (auto it = userProfiles.begin(); it != userProfiles.end(); ++it) {
            combo->addItem(it.key(), it.key());
            if (it.value().isObject() && Json::boolean(it.value().toObject(), "Active")) {
                active = it.key();
            }
        }
        const int index = combo->findData(active);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo]() {
            m_client->post(QStringLiteral("/api/userProfile/change"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("userProfileName"), combo->currentData().toString()},
                           },
                           [this](const QJsonObject &, const QString &) {
                               reload();
                           });
        });
        form->addRow(m_i18n->t("txtProfile", "Profile"), combo);
    }

    if (profile.contains(QLatin1String("BrightnessSlider"))) {
        auto *slider = new QSlider(Qt::Horizontal, box);
        slider->setRange(0, 100);
        slider->setValue(Json::integer(profile, "BrightnessSlider", 100));
        auto *value = new QLabel(QStringLiteral("%1 %").arg(slider->value()), box);
        auto *row = new QWidget(box);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(value);
        connect(slider, &QSlider::valueChanged, value, [value](int v) {
            value->setText(QStringLiteral("%1 %").arg(v));
        });
        connect(slider, &QSlider::sliderReleased, this, [this, slider]() {
            m_client->post(QStringLiteral("/api/brightness/gradual"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("brightness"), slider->value()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtBrightness", "Brightness"), row);
    }

    if (profile.contains(QLatin1String("RGBCluster"))) {
        auto *toggle = new QCheckBox(box);
        toggle->setChecked(Json::boolean(profile, "RGBCluster"));
        connect(toggle, &QCheckBox::toggled, this, [this](bool checked) {
            m_client->post(QStringLiteral("/api/color/setCluster"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("mode"), checked ? 1 : 0},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtCluster", "Cluster"), toggle);
    }

    if (profile.contains(QLatin1String("OpenRGBIntegration"))) {
        auto *toggle = new QCheckBox(box);
        toggle->setChecked(Json::boolean(profile, "OpenRGBIntegration"));
        connect(toggle, &QCheckBox::toggled, this, [this](bool checked) {
            m_client->post(QStringLiteral("/api/color/setOpenRgbIntegration"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("mode"), checked ? 1 : 0},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtOpenRGB", "OpenRGB"), toggle);
    }

    const QStringList rgbModes = Json::stringList(device.value(QStringLiteral("RGBModes")));
    if (!rgbModes.isEmpty()) {
        auto *combo = new QComboBox(box);
        combo->addItem(tr("None"), QString());
        fillCombo(combo, rgbModes, {});
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo]() {
            setRgb(-1, combo->currentData().toString());
        });
        form->addRow(m_i18n->t("txtRgb", "RGB"), combo);
    }

    if (!m_speedProfiles.isEmpty() && !Json::object(device, "devices").isEmpty()) {
        auto *combo = new QComboBox(box);
        fillCombo(combo, m_speedProfiles, Json::str(profile, "MultiProfile"));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo]() {
            setSpeed(-1, combo->currentData().toString());
        });
        form->addRow(m_i18n->t("txtSpeed", "Speed"), combo);
    }

    auto *saveProfile = new QPushButton(m_i18n->t("txtSaveUserProfile", "Save user profile"), box);
    auto *name = new QLineEdit(box);
    name->setPlaceholderText(m_i18n->t("txtProfileName", "Profile name"));
    connect(saveProfile, &QPushButton::clicked, this, [this, name]() {
        if (name->text().trimmed().isEmpty()) {
            return;
        }
        m_client->put(QStringLiteral("/api/userProfile"),
                      QJsonObject{
                          {QStringLiteral("deviceId"), m_serial},
                          {QStringLiteral("userProfileName"), name->text().trimmed()},
                      },
                      [this](const QJsonObject &, const QString &) {
                          reload();
                      });
    });
    form->addRow(name, saveProfile);

    m_grid->addCard(box);
}

QWidget *DevicePage::channelCard(const QJsonObject &device, const QJsonObject &channel)
{
    const int channelId = Json::integer(channel, "channelId");
    const QString channelKey = QString::number(channelId);
    const QString name = Json::str(channel, "name", Json::str(device, "product"));
    auto *box = Ui::card(name);
    auto *form = Ui::form(box);

    auto *labelEdit = new QLineEdit(Json::str(channel, "label"), box);
    connect(labelEdit, &QLineEdit::editingFinished, this, [this, channelId, labelEdit]() {
        setLabel(channelId, labelEdit->text());
    });
    form->addRow(m_i18n->t("txtLabel", "Label"), labelEdit);

    if (Json::boolean(channel, "HasSpeed") || channel.contains(QLatin1String("rpm"))) {
        form->addRow(m_i18n->t("txtSpeed", "Speed"),
                     Telemetry::valueLabel(box, Telemetry::id(m_serial, channelKey, QStringLiteral("rpm")),
                                          QStringLiteral("%1 RPM").arg(Json::integer(channel, "rpm"))));
    }

    if (Json::boolean(channel, "HasTemps") || !Json::str(channel, "temperatureString").isEmpty()) {
        const char *tempKey = Json::boolean(channel, "AIO") ? "txtLiquidTemp" : "txtTemperature";
        form->addRow(m_i18n->t(tempKey, "Temperature"),
                     Telemetry::valueLabel(box, Telemetry::id(m_serial, channelKey, QStringLiteral("temp")),
                                          Json::str(channel, "temperatureString", QStringLiteral("—"))));
    }

    Telemetry::addPsuFields(form, box, channel, m_serial, channelKey, m_i18n);

    if (!m_speedProfiles.isEmpty() && Json::boolean(channel, "HasSpeed")) {
        auto *combo = new QComboBox(box);
        fillCombo(combo, m_speedProfiles, Json::str(channel, "profile"));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, channelId]() {
            setSpeed(channelId, combo->currentData().toString());
        });
        form->addRow(m_i18n->t("txtProfile", "Profile"), combo);
    }

    const QStringList rgbModes = Json::stringList(device.value(QStringLiteral("RGBModes")));
    if (!rgbModes.isEmpty() && !Json::str(channel, "rgb").isEmpty()) {
        auto *combo = new QComboBox(box);
        fillCombo(combo, rgbModes, Json::str(channel, "rgb"));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, channelId]() {
            setRgb(channelId, combo->currentData().toString());
        });
        form->addRow(m_i18n->t("txtRgb", "RGB"), combo);
    }

    const QJsonObject lcdModes = Json::object(device, "LCDModes");
    const QJsonObject profile = Json::object(device, "DeviceProfile");
    const QJsonObject perChannelLcd = Json::object(profile, "LCDModes");
    const bool showLcd = Json::boolean(device, "HasLCD") && !lcdModes.isEmpty()
        && (Json::boolean(channel, "AIO") || Json::boolean(channel, "ContainsPump") || !Json::str(channel, "LCDSerial").isEmpty());
    if (showLcd) {
        auto fillSorted = [](QComboBox *combo, const QJsonObject &object, const QVariant &current) {
            QList<QPair<int, QString>> entries;
            for (auto it = object.begin(); it != object.end(); ++it) {
                entries.append({it.key().toInt(), it.value().isString() ? it.value().toString() : it.key()});
            }
            std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
                return a.first < b.first;
            });
            for (const auto &entry : entries) {
                combo->addItem(entry.second, entry.first);
            }
            const int index = combo->findData(current);
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
        };

        auto *mode = new QComboBox(box);
        fillSorted(mode, lcdModes, perChannelLcd.value(QString::number(channelId)).toInt());
        connect(mode, &QComboBox::currentIndexChanged, this, [this, mode, channelId]() {
            m_client->post(QStringLiteral("/api/lcd"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("channelId"), channelId},
                               {QStringLiteral("mode"), mode->currentData().toInt()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtLcdMode", "LCD mode"), mode);

        auto *image = new QComboBox(box);
        const QString currentImage = Json::object(profile, "LCDImages").value(QString::number(channelId)).toString();
        QStringList images = LcdData::images();
        if (!currentImage.isEmpty() && !images.contains(currentImage)) {
            images.prepend(currentImage);
        }
        for (const QString &name : images) {
            image->addItem(name, name);
        }
        const int imageIndex = image->findData(currentImage);
        if (imageIndex >= 0) {
            image->setCurrentIndex(imageIndex);
        }
        connect(image, &QComboBox::currentIndexChanged, this, [this, image, channelId]() {
            m_client->post(QStringLiteral("/api/lcd/image"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("channelId"), channelId},
                               {QStringLiteral("image"), image->currentData().toString()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtLcdImage", "LCD image"), image);

        auto *rotation = new QComboBox(box);
        fillSorted(rotation, Json::object(device, "LCDRotations"), Json::object(profile, "LCDRotations").value(QString::number(channelId)).toInt());
        connect(rotation, &QComboBox::currentIndexChanged, this, [this, rotation, channelId]() {
            m_client->post(QStringLiteral("/api/lcd/rotation"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("channelId"), channelId},
                               {QStringLiteral("rotation"), rotation->currentData().toInt()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtLcdRotation", "LCD rotation"), rotation);

        auto *brightness = new QComboBox(box);
        fillSorted(brightness, Json::object(device, "LCDBrightnessLevels"), Json::object(profile, "LCDBrightness").value(QString::number(channelId)).toInt());
        connect(brightness, &QComboBox::currentIndexChanged, this, [this, brightness, channelId]() {
            m_client->post(QStringLiteral("/api/lcd/brightness"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("channelId"), channelId},
                               {QStringLiteral("brightness"), brightness->currentData().toInt()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtLcdBrightness", "LCD brightness"), brightness);
    }

    return box;
}

void DevicePage::addChannelCards(const QJsonObject &device)
{
    const QJsonObject channels = Json::object(device, "devices");
    if (channels.isEmpty()) {
        return;
    }

    QList<QPair<int, QJsonObject>> sorted;
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        if (it.value().isObject()) {
            sorted.append({it.key().toInt(), it.value().toObject()});
        }
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
    for (const auto &entry : sorted) {
        m_grid->addCard(channelCard(device, entry.second));
    }
}

void DevicePage::addHeadsetCards(const QJsonObject &device)
{
    const QJsonObject profile = Json::object(device, "DeviceProfile");
    const QJsonObject zoneColors = Json::object(profile, "ZoneColors");
    const QJsonObject equalizers = Json::object(profile, "Equalizers");
    const QJsonObject sleepModes = Json::object(device, "SleepModes");
    const QJsonObject muteIndicators = Json::object(device, "MuteIndicators");
    if (zoneColors.isEmpty() && equalizers.isEmpty() && sleepModes.isEmpty()) {
        return;
    }

    if (!zoneColors.isEmpty()) {
        auto *box = Ui::card(m_i18n->t("txtRgb", "RGB"));
        auto *form = Ui::form(box);
        QHash<int, KColorButton *> buttons;
        QList<int> ids;
        for (auto it = zoneColors.begin(); it != zoneColors.end(); ++it) {
            ids.append(it.key().toInt());
        }
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            const QJsonObject zone = Json::object(zoneColors.value(QString::number(id)));
            auto *button = new KColorButton(Json::color(Json::object(zone, "Color")), box);
            buttons.insert(id, button);
            form->addRow(Json::str(zone, "Name", tr("Zone %1").arg(id)), button);
        }
        auto *save = new QPushButton(m_i18n->t("txtSave", "Save"), box);
        connect(save, &QPushButton::clicked, this, [this, buttons]() {
            QJsonObject colors;
            for (auto it = buttons.begin(); it != buttons.end(); ++it) {
                colors.insert(QString::number(it.key()), Json::colorObject(it.value()->color()));
            }
            m_client->post(QStringLiteral("/api/headset/zoneColors"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("colorZones"), colors},
                           },
                           {});
        });
        form->addRow(QString(), save);
        m_grid->addCard(box);
    }

    if (!sleepModes.isEmpty()) {
        auto *box = Ui::card(m_i18n->t("txtSleep", "Sleep"));
        auto *form = Ui::form(box);
        auto *combo = new QComboBox(box);
        for (auto it = sleepModes.begin(); it != sleepModes.end(); ++it) {
            combo->addItem(it.value().isString() ? it.value().toString() : it.key(), it.key().toInt());
        }
        const int index = combo->findData(Json::integer(profile, "SleepMode"));
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo]() {
            m_client->post(QStringLiteral("/api/headset/sleep"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("sleepMode"), combo->currentData().toInt()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtSleep", "Sleep"), combo);

        if (!muteIndicators.isEmpty()) {
            auto *mute = new QComboBox(box);
            for (auto it = muteIndicators.begin(); it != muteIndicators.end(); ++it) {
                mute->addItem(it.value().isString() ? it.value().toString() : it.key(), it.key().toInt());
            }
            const int muteIndex = mute->findData(Json::integer(profile, "MuteIndicator"));
            if (muteIndex >= 0) {
                mute->setCurrentIndex(muteIndex);
            }
            connect(mute, &QComboBox::currentIndexChanged, this, [this, mute]() {
                m_client->post(QStringLiteral("/api/headset/muteIndicator"),
                               QJsonObject{
                                   {QStringLiteral("deviceId"), m_serial},
                                   {QStringLiteral("muteIndicator"), mute->currentData().toInt()},
                               },
                               {});
            });
            form->addRow(m_i18n->t("txtMuteIndicator", "Mute indicator"), mute);
        }
        m_grid->addCard(box);
    }

    if (profile.contains(QLatin1String("SideTone")) || profile.contains(QLatin1String("SideToneValue"))) {
        auto *box = Ui::card(m_i18n->t("txtSidetone", "Sidetone"));
        auto *form = Ui::form(box);

        auto *enabled = new QCheckBox(box);
        enabled->setChecked(Json::integer(profile, "SideTone") != 0);
        connect(enabled, &QCheckBox::toggled, this, [this](bool checked) {
            m_client->post(QStringLiteral("/api/headset/sidetone"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("sideTone"), checked ? 1 : 0},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtSidetone", "Sidetone"), enabled);

        auto *slider = new QSlider(Qt::Horizontal, box);
        slider->setRange(1, 100);
        slider->setValue(qBound(1, Json::integer(profile, "SideToneValue", 50), 100));
        auto *value = new QLabel(QStringLiteral("%1 %").arg(slider->value()), box);
        auto *row = new QWidget(box);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(value);
        connect(slider, &QSlider::valueChanged, value, [value](int v) {
            value->setText(QStringLiteral("%1 %").arg(v));
        });
        connect(slider, &QSlider::sliderReleased, this, [this, slider]() {
            m_client->post(QStringLiteral("/api/headset/sidetoneValue"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("sideToneValue"), slider->value()},
                           },
                           {});
        });
        form->addRow(m_i18n->t("txtSidetoneValue", "Sidetone level"), row);
        m_grid->addCard(box);
    }

    if (!equalizers.isEmpty()) {
        auto *box = Ui::card(m_i18n->t("txtEqualizer", "Equalizer"));
        auto *form = Ui::form(box);
        QHash<int, QSlider *> sliders;
        QList<int> ids;
        for (auto it = equalizers.begin(); it != equalizers.end(); ++it) {
            ids.append(it.key().toInt());
        }
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            const QJsonObject band = Json::object(equalizers.value(QString::number(id)));
            auto *slider = new QSlider(Qt::Horizontal, box);
            slider->setRange(-10, 10);
            slider->setValue(Json::integer(band, "Value"));
            sliders.insert(id, slider);
            form->addRow(Json::str(band, "Name", QString::number(id)), slider);
        }
        auto *save = new QPushButton(m_i18n->t("txtSave", "Save"), box);
        connect(save, &QPushButton::clicked, this, [this, sliders]() {
            QJsonObject values;
            for (auto it = sliders.begin(); it != sliders.end(); ++it) {
                values.insert(QString::number(it.key()), it.value()->value());
            }
            m_client->post(QStringLiteral("/api/headset/equalizer"),
                           QJsonObject{
                               {QStringLiteral("deviceId"), m_serial},
                               {QStringLiteral("equalizers"), values},
                           },
                           {});
        });
        form->addRow(QString(), save);
        m_grid->addCard(box);
    }
}
