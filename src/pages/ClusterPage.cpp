#include "pages/ClusterPage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/UiHelpers.h"

#include <QComboBox>
#include <QGroupBox>
#include <QJsonObject>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

ClusterPage::ClusterPage(ApiClient *client, HubI18n *i18n, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_grid = new CardGrid(this);
    layout->addWidget(Ui::scrollWrap(m_grid));
}

void ClusterPage::reload()
{
    m_client->get(QStringLiteral("/api/devices/cluster"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        rebuild(Json::object(json, "device"));
    });
}

void ClusterPage::rebuild(const QJsonObject &device)
{
    m_grid->clear();
    const QJsonObject profile = Json::object(device, "DeviceProfile");
    auto *box = Ui::card(Json::str(device, "product", m_i18n->t("txtRgbCluster", "RGB Cluster")));
    auto *form = Ui::form(box);

    auto *slider = new QSlider(Qt::Horizontal, box);
    slider->setRange(0, 100);
    slider->setValue(Json::integer(profile, "BrightnessSlider", 100));
    connect(slider, &QSlider::sliderReleased, this, [this, slider]() {
        m_client->post(QStringLiteral("/api/brightness/gradual"),
                       QJsonObject{
                           {QStringLiteral("deviceId"), QStringLiteral("cluster")},
                           {QStringLiteral("brightness"), slider->value()},
                       },
                       {});
    });
    form->addRow(m_i18n->t("txtBrightness", "Brightness"), slider);

    const QStringList modes = Json::stringList(device.value(QStringLiteral("RGBModes")));
    auto *combo = new QComboBox(box);
    combo->addItem(tr("None"), QString());
    for (const QString &mode : modes) {
        combo->addItem(mode, mode);
    }
    connect(combo, &QComboBox::currentIndexChanged, this, [this, combo]() {
        const QString profileName = combo->currentData().toString();
        if (profileName.isEmpty()) {
            return;
        }
        m_client->post(QStringLiteral("/api/color"),
                       QJsonObject{
                           {QStringLiteral("deviceId"), QStringLiteral("cluster")},
                           {QStringLiteral("channelId"), 0},
                           {QStringLiteral("profile"), profileName},
                       },
                       {});
    });
    form->addRow(m_i18n->t("txtRgb", "RGB"), combo);
    m_grid->addCard(box);

    const QJsonObject controllers = Json::object(device, "Controllers");
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        const QJsonObject controller = Json::object(it.value());
        auto *card = Ui::card(Json::strAny(controller, {QStringLiteral("product"), QStringLiteral("name"), QStringLiteral("serial")}, it.key()));
        auto *controllerForm = Ui::form(card);
        controllerForm->addRow(m_i18n->t("txtSerial", "Serial"), new QLabel(it.key()));
        if (controller.contains(QLatin1String("rgb"))) {
            controllerForm->addRow(m_i18n->t("txtRgb", "RGB"), new QLabel(Json::str(controller, "rgb")));
        }
        m_grid->addCard(card);
    }
}
