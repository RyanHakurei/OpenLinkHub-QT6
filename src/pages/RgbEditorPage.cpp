#include "pages/RgbEditorPage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/ResponsiveSplit.h"
#include "widgets/UiHelpers.h"

#include <KColorButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

RgbEditorPage::RgbEditorPage(ApiClient *client, HubI18n *i18n, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_devices = new QListWidget(this);
    m_devices->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_grid = new CardGrid(this);
    auto *split = new ResponsiveSplit(m_devices, Ui::scrollWrap(m_grid), this);
    layout->addWidget(split);

    connect(m_devices, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) {
            showDevice(item->data(Qt::UserRole).toString());
        }
    });
}

void RgbEditorPage::reload()
{
    m_client->get(QStringLiteral("/api/color/"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QString current = m_devices->currentItem() ? m_devices->currentItem()->data(Qt::UserRole).toString() : QString();
        m_devices->clear();
        const QJsonObject data = Json::object(json, "data");
        for (auto it = data.begin(); it != data.end(); ++it) {
            const QJsonObject device = Json::object(it.value());
            auto *item = new QListWidgetItem(Json::str(device, "device", it.key()));
            item->setData(Qt::UserRole, it.key());
            m_devices->addItem(item);
        }
        const int index = [&]() {
            for (int i = 0; i < m_devices->count(); ++i) {
                if (m_devices->item(i)->data(Qt::UserRole).toString() == current) {
                    return i;
                }
            }
            return m_devices->count() > 0 ? 0 : -1;
        }();
        if (index >= 0) {
            m_devices->setCurrentRow(index);
        }
    });
}

void RgbEditorPage::showDevice(const QString &serial)
{
    m_client->get(QStringLiteral("/api/color/") + serial, [this, serial](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        m_grid->clear();
        const QJsonObject data = Json::object(json, "data");
        const QJsonObject profiles = Json::object(data, "profiles");
        static const QStringList skip{QStringLiteral("keyboard"), QStringLiteral("mouse"), QStringLiteral("stand"), QStringLiteral("mousepad"), QStringLiteral("headset"), QStringLiteral("custom"), QStringLiteral("off"), QStringLiteral("controller")};
        for (auto it = profiles.begin(); it != profiles.end(); ++it) {
            if (skip.contains(it.key())) {
                continue;
            }
            const QJsonObject profile = Json::object(it.value());
            const QString name = Json::str(profile, "profileName", it.key());
            auto *box = Ui::card(name);
            auto *form = Ui::form(box);
            auto *preview = new QLabel(box);
            preview->setFixedHeight(48);
            const QColor start = Json::color(Json::object(profile, "start"));
            preview->setAutoFillBackground(true);
            QPalette palette = preview->palette();
            palette.setColor(QPalette::Window, start);
            preview->setPalette(palette);
            form->addRow(preview);
            auto *configure = new QPushButton(m_i18n->t("txtConfigure", "Configure"), box);
            const QString profileId = it.key();
            connect(configure, &QPushButton::clicked, this, [this, serial, profileId]() {
                m_client->get(QStringLiteral("/api/color/profile/%1/%2").arg(serial, profileId), [this, serial, profileId](const QJsonObject &json, const QString &error) {
                    if (error.isEmpty()) {
                        configureProfile(serial, profileId, Json::object(json, "data"));
                    }
                });
            });
            form->addRow(configure);
            m_grid->addCard(box);
        }
    });
}

void RgbEditorPage::configureProfile(const QString &serial, const QString &profile, const QJsonObject &data)
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(Json::str(data, "profileName", profile));
    auto *form = new QFormLayout(dialog);

    auto *start = new KColorButton(Json::color(Json::object(data, "start")), dialog);
    auto *middle = new KColorButton(Json::color(Json::object(data, "middle")), dialog);
    auto *end = new KColorButton(Json::color(Json::object(data, "end")), dialog);
    auto *speed = new QSlider(Qt::Horizontal, dialog);
    speed->setRange(1, 10);
    speed->setValue(qBound(1, static_cast<int>(Json::number(data, "speed", 1)), 10));
    auto *alternate = new QCheckBox(dialog);
    alternate->setChecked(Json::boolean(data, "alternateColors"));
    auto *direction = new QComboBox(dialog);
    direction->addItem(tr("Default"), 0);
    direction->addItem(tr("Top to Bottom"), 1);
    direction->addItem(tr("Bottom to Top"), 2);
    direction->addItem(tr("Left to Right"), 4);
    direction->addItem(tr("Right to Left"), 5);
    const int dirIndex = direction->findData(Json::integer(data, "rgbDirection"));
    if (dirIndex >= 0) {
        direction->setCurrentIndex(dirIndex);
    }

    form->addRow(tr("Start"), start);
    form->addRow(tr("Middle"), middle);
    form->addRow(tr("End"), end);
    form->addRow(m_i18n->t("txtSpeed", "Speed"), speed);
    form->addRow(tr("Alternate colors"), alternate);
    form->addRow(tr("Direction"), direction);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, serial, profile, start, middle, end, speed, alternate, direction]() {
        m_client->put(QStringLiteral("/api/color/change"),
                      QJsonObject{
                          {QStringLiteral("deviceId"), serial},
                          {QStringLiteral("profile"), profile},
                          {QStringLiteral("startColor"), Json::colorObject(start->color())},
                          {QStringLiteral("middleColor"), Json::colorObject(middle->color())},
                          {QStringLiteral("endColor"), Json::colorObject(end->color())},
                          {QStringLiteral("speed"), static_cast<double>(speed->value())},
                          {QStringLiteral("alternateColors"), alternate->isChecked()},
                          {QStringLiteral("rgbDirection"), direction->currentData().toInt()},
                      },
                      {});
        dialog->accept();
    });
    dialog->open();
}
