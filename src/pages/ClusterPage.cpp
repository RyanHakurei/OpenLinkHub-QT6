#include "pages/ClusterPage.h"

#include "api/ApiClient.h"
#include "api/HubPaths.h"
#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/UiHelpers.h"

#include <QComboBox>
#include <QFile>
#include <QRegularExpression>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSignalBlocker>
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

namespace {

// The daemon cannot JSON-encode the live cluster object, so /api/devices/cluster
// comes back empty. These are the modes cluster.Init registers.
QStringList clusterRgbModes()
{
    return {
        QStringLiteral("circle"),
        QStringLiteral("circleshift"),
        QStringLiteral("colorpulse"),
        QStringLiteral("colorshift"),
        QStringLiteral("colorwarp"),
        QStringLiteral("cpu-temperature"),
        QStringLiteral("flickering"),
        QStringLiteral("gpu-temperature"),
        QStringLiteral("gradient"),
        QStringLiteral("marquee"),
        QStringLiteral("nebula"),
        QStringLiteral("rain"),
        QStringLiteral("rainbow"),
        QStringLiteral("pastelrainbow"),
        QStringLiteral("rotator"),
        QStringLiteral("sequential"),
        QStringLiteral("spinner"),
        QStringLiteral("spiralrainbow"),
        QStringLiteral("pastelspiralrainbow"),
        QStringLiteral("static"),
        QStringLiteral("storm"),
        QStringLiteral("visor"),
        QStringLiteral("watercolor"),
        QStringLiteral("wave"),
    };
}

QJsonObject clusterMembers(const QString &html)
{
    QJsonObject controllers;
    const int table = html.indexOf(QStringLiteral("id=\"table\""));
    const int body = html.indexOf(QStringLiteral("<tbody>"), qMax(0, table));
    const int end = html.indexOf(QStringLiteral("</tbody>"), body);
    if (body < 0 || end < body) {
        return controllers;
    }
    const QRegularExpression cell(QStringLiteral("<td>\\s*([^<]*?)\\s*</td>"));
    QStringList cells;
    auto it = cell.globalMatch(html.mid(body, end - body));
    while (it.hasNext()) {
        cells.append(it.next().captured(1).trimmed());
    }
    for (int i = 0; i + 2 < cells.size(); i += 3) {
        const QString product = cells.at(i);
        const QString serial = cells.at(i + 1);
        QJsonObject member;
        member.insert(QStringLiteral("product"), product);
        member.insert(QStringLiteral("serial"), serial);
        member.insert(QStringLiteral("rgb"), cells.at(i + 2));
        controllers.insert(serial.isEmpty() ? product : serial, member);
    }
    return controllers;
}

QJsonObject clusterDevice(const QJsonObject &profile, const QJsonObject &controllers)
{
    QJsonObject device;
    device.insert(QStringLiteral("product"), QStringLiteral("Cluster"));
    device.insert(QStringLiteral("DeviceProfile"), profile);
    QJsonArray modes;
    const QString current = Json::str(profile, "RGBProfile");
    QStringList names = clusterRgbModes();
    if (!current.isEmpty() && !names.contains(current)) {
        names.prepend(current);
    }
    for (const QString &mode : names) {
        modes.append(mode);
    }
    device.insert(QStringLiteral("RGBModes"), modes);
    device.insert(QStringLiteral("Controllers"), controllers);
    return device;
}

QJsonObject clusterProfile()
{
    const QString path = HubPaths::profileFile(QStringLiteral("cluster"));
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

} // namespace

void ClusterPage::reload()
{
    const QJsonObject profile = clusterProfile();
    m_client->getBinary(QStringLiteral("/rgbCluster"), [this, profile](const QByteArray &data, const QString &error, const QString &) {
        const QJsonObject controllers = error.isEmpty() ? clusterMembers(QString::fromUtf8(data)) : QJsonObject{};
        rebuild(clusterDevice(profile, controllers));
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
    {
        const QSignalBlocker blocker(combo);
        for (const QString &mode : modes) {
            combo->addItem(mode, mode);
        }
        const QString current = Json::str(profile, "RGBProfile");
        const int index = combo->findData(current);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
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
    if (controllers.isEmpty()) {
        auto *empty = Ui::card(m_i18n->t("txtDevices", "Devices"));
        auto *emptyForm = Ui::form(empty);
        emptyForm->addRow(new QLabel(tr("No devices are in the cluster."), empty));
        m_grid->addCard(empty);
    }
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
