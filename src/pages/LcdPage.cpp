#include "pages/LcdPage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "api/LcdData.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/UiHelpers.h"

#include <KColorButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

namespace {
void fillSensors(QComboBox *combo, int current)
{
    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const auto &sensor : LcdData::sensors()) {
        combo->addItem(sensor.second, sensor.first);
    }
    const int index = combo->findData(current);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

QJsonObject colorJson(KColorButton *button)
{
    if (!button) {
        return {};
    }
    return Json::colorObject(button->color());
}
}

LcdPage::LcdPage(ApiClient *client, HubI18n *i18n, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_grid = new CardGrid(this);
    layout->addWidget(m_status);
    layout->addWidget(Ui::scrollWrap(m_grid), 1);
}

void LcdPage::reload()
{
    rebuild();
}

void LcdPage::rebuild()
{
    m_grid->clear();

    const QJsonObject arc = LcdData::load(QStringLiteral("arc.json"));
    const QJsonObject doubleArc = LcdData::load(QStringLiteral("double-arc.json"));
    const QJsonObject animation = LcdData::load(QStringLiteral("animation.json"));

    if (arc.isEmpty() && doubleArc.isEmpty() && animation.isEmpty()) {
        m_status->setText(tr("Could not read LCD profiles from %1. The OpenLinkHub daemon stores Arc, Double Arc, and Animation settings there.")
                              .arg(LcdData::directory()));
    } else {
        m_status->clear();
    }

    if (!arc.isEmpty()) {
        m_grid->addCard(arcCard(arc));
    }
    if (!doubleArc.isEmpty()) {
        m_grid->addCard(doubleArcCard(doubleArc));
    }
    if (!animation.isEmpty()) {
        m_grid->addCard(animationCard(animation));
    }
}

QWidget *LcdPage::arcCard(const QJsonObject &profile)
{
    auto *box = Ui::card(Json::str(profile, "name", m_i18n->t("txtArc", "Arc")));
    auto *form = Ui::form(box);

    auto *sensor = new QComboBox(box);
    sensor->setObjectName(QStringLiteral("arc_sensor"));
    fillSensors(sensor, Json::integer(profile, "sensor"));

    auto *margin = new QDoubleSpinBox(box);
    margin->setObjectName(QStringLiteral("arc_margin"));
    margin->setRange(0, 240);
    margin->setValue(Json::number(profile, "margin", 20));

    auto *thickness = new QDoubleSpinBox(box);
    thickness->setObjectName(QStringLiteral("arc_thickness"));
    thickness->setRange(1, 240);
    thickness->setValue(Json::number(profile, "thickness", 50));

    auto *gap = new QDoubleSpinBox(box);
    gap->setObjectName(QStringLiteral("arc_gap"));
    gap->setDecimals(2);
    gap->setSingleStep(0.05);
    gap->setRange(0, 3);
    gap->setValue(Json::number(profile, "gapRadians", 0.5));

    auto *background = new KColorButton(Json::color(Json::object(profile, "background")), box);
    background->setObjectName(QStringLiteral("arc_background"));
    auto *border = new KColorButton(Json::color(Json::object(profile, "borderColor")), box);
    border->setObjectName(QStringLiteral("arc_border"));
    auto *start = new KColorButton(Json::color(Json::object(profile, "startColor")), box);
    start->setObjectName(QStringLiteral("arc_start"));
    auto *end = new KColorButton(Json::color(Json::object(profile, "endColor")), box);
    end->setObjectName(QStringLiteral("arc_end"));
    auto *text = new KColorButton(Json::color(Json::object(profile, "textColor")), box);
    text->setObjectName(QStringLiteral("arc_text"));

    auto *save = new QPushButton(m_i18n->t("txtSaveProfile", "Save profile"), box);
    connect(save, &QPushButton::clicked, this, &LcdPage::saveArc);

    form->addRow(m_i18n->t("txtSensor", "Sensor"), sensor);
    form->addRow(m_i18n->t("txtMargin", "Margin"), margin);
    form->addRow(m_i18n->t("txtThickness", "Thickness"), thickness);
    form->addRow(m_i18n->t("txtBottomGap", "Bottom gap"), gap);
    form->addRow(m_i18n->t("txtBackground", "Background"), background);
    form->addRow(m_i18n->t("txtBorderColor", "Border color"), border);
    form->addRow(m_i18n->t("txtStartColor", "Start color"), start);
    form->addRow(m_i18n->t("txtEndColor", "End color"), end);
    form->addRow(m_i18n->t("txtTextColor", "Text color"), text);
    form->addRow(save);
    return box;
}

QWidget *LcdPage::doubleArcCard(const QJsonObject &profile)
{
    auto *box = Ui::card(Json::str(profile, "name", tr("Double Arc")));
    auto *form = Ui::form(box);

    auto *margin = new QDoubleSpinBox(box);
    margin->setObjectName(QStringLiteral("da_margin"));
    margin->setRange(0, 240);
    margin->setValue(Json::number(profile, "margin", 20));

    auto *thickness = new QDoubleSpinBox(box);
    thickness->setObjectName(QStringLiteral("da_thickness"));
    thickness->setRange(1, 240);
    thickness->setValue(Json::number(profile, "thickness", 50));

    auto *gap = new QDoubleSpinBox(box);
    gap->setObjectName(QStringLiteral("da_gap"));
    gap->setDecimals(2);
    gap->setSingleStep(0.05);
    gap->setRange(0, 3);
    gap->setValue(Json::number(profile, "gapRadians", 0.5));

    auto *background = new KColorButton(Json::color(Json::object(profile, "background")), box);
    background->setObjectName(QStringLiteral("da_background"));
    auto *border = new KColorButton(Json::color(Json::object(profile, "borderColor")), box);
    border->setObjectName(QStringLiteral("da_border"));
    auto *separator = new KColorButton(Json::color(Json::object(profile, "separatorColor")), box);
    separator->setObjectName(QStringLiteral("da_separator"));

    form->addRow(m_i18n->t("txtMargin", "Margin"), margin);
    form->addRow(m_i18n->t("txtThickness", "Thickness"), thickness);
    form->addRow(m_i18n->t("txtBottomGap", "Bottom gap"), gap);
    form->addRow(m_i18n->t("txtBackground", "Background"), background);
    form->addRow(m_i18n->t("txtBorderColor", "Border color"), border);
    form->addRow(m_i18n->t("txtSeparatorColor", "Separator color"), separator);

    const QJsonObject arcs = Json::object(profile, "arcs");
    QList<int> ids;
    for (auto it = arcs.begin(); it != arcs.end(); ++it) {
        ids.append(it.key().toInt());
    }
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        const QJsonObject arc = Json::object(arcs.value(QString::number(id)));
        form->addRow(new QLabel(Json::str(arc, "name", tr("Arc %1").arg(id)), box));

        auto *sensor = new QComboBox(box);
        sensor->setObjectName(QStringLiteral("da_sensor_%1").arg(id));
        fillSensors(sensor, Json::integer(arc, "sensor"));

        auto *start = new KColorButton(Json::color(Json::object(arc, "startColor")), box);
        start->setObjectName(QStringLiteral("da_start_%1").arg(id));
        auto *end = new KColorButton(Json::color(Json::object(arc, "endColor")), box);
        end->setObjectName(QStringLiteral("da_end_%1").arg(id));
        auto *text = new KColorButton(Json::color(Json::object(arc, "textColor")), box);
        text->setObjectName(QStringLiteral("da_text_%1").arg(id));

        form->addRow(m_i18n->t("txtSensor", "Sensor"), sensor);
        form->addRow(m_i18n->t("txtStartColor", "Start color"), start);
        form->addRow(m_i18n->t("txtEndColor", "End color"), end);
        form->addRow(m_i18n->t("txtTextColor", "Text color"), text);
    }

    auto *save = new QPushButton(m_i18n->t("txtSaveProfile", "Save profile"), box);
    connect(save, &QPushButton::clicked, this, &LcdPage::saveDoubleArc);
    form->addRow(save);
    return box;
}

QWidget *LcdPage::animationCard(const QJsonObject &profile)
{
    auto *box = Ui::card(Json::str(profile, "name", m_i18n->t("txtAnimation", "Animation")));
    auto *form = Ui::form(box);

    auto *background = new QComboBox(box);
    background->setObjectName(QStringLiteral("anim_background"));
    const QString currentImage = Json::str(profile, "background");
    for (const QString &name : LcdData::images()) {
        background->addItem(name, name);
    }
    const int imageIndex = background->findData(currentImage);
    if (imageIndex >= 0) {
        background->setCurrentIndex(imageIndex);
    } else if (!currentImage.isEmpty()) {
        background->addItem(currentImage, currentImage);
        background->setCurrentIndex(background->count() - 1);
    }

    auto *margin = new QDoubleSpinBox(box);
    margin->setObjectName(QStringLiteral("anim_margin"));
    margin->setRange(0, 240);
    margin->setValue(Json::number(profile, "margin", 60));

    auto *workers = new QSpinBox(box);
    workers->setObjectName(QStringLiteral("anim_workers"));
    workers->setRange(1, 32);
    workers->setValue(Json::integer(profile, "workers", 4));
    workers->setToolTip(m_i18n->t("txtCpuWorkersInfo", "Number of CPU workers used to render animation frames."));

    auto *frameDelay = new QSpinBox(box);
    frameDelay->setObjectName(QStringLiteral("anim_framedelay"));
    frameDelay->setRange(0, 1000);
    frameDelay->setSuffix(tr(" ms"));
    frameDelay->setValue(Json::integer(profile, "frameDelay"));
    frameDelay->setToolTip(m_i18n->t("txtFrameDelayInfo", "Extra delay added to each animation frame."));

    auto *separator = new KColorButton(Json::color(Json::object(profile, "separatorColor")), box);
    separator->setObjectName(QStringLiteral("anim_separator"));

    form->addRow(m_i18n->t("txtBackground", "Background"), background);
    form->addRow(m_i18n->t("txtMargin", "Margin"), margin);
    form->addRow(m_i18n->t("txtWorkers", "Workers"), workers);
    form->addRow(m_i18n->t("txtFrameDelay", "Frame delay"), frameDelay);
    form->addRow(m_i18n->t("txtSeparatorColor", "Separator color"), separator);

    const QJsonObject sensors = Json::object(profile, "sensors");
    QList<int> ids;
    for (auto it = sensors.begin(); it != sensors.end(); ++it) {
        ids.append(it.key().toInt());
    }
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        const QJsonObject sensor = Json::object(sensors.value(QString::number(id)));
        form->addRow(new QLabel(Json::str(sensor, "name", tr("Sensor %1").arg(id + 1)), box));

        auto *enabled = new QCheckBox(box);
        enabled->setObjectName(QStringLiteral("anim_enabled_%1").arg(id));
        enabled->setChecked(Json::boolean(sensor, "enabled", true));

        auto *type = new QComboBox(box);
        type->setObjectName(QStringLiteral("anim_sensor_%1").arg(id));
        fillSensors(type, Json::integer(sensor, "sensor"));

        auto *text = new KColorButton(Json::color(Json::object(sensor, "textColor")), box);
        text->setObjectName(QStringLiteral("anim_text_%1").arg(id));

        form->addRow(m_i18n->t("txtEnable", "Enable"), enabled);
        form->addRow(m_i18n->t("txtSensor", "Sensor"), type);
        form->addRow(m_i18n->t("txtTextColor", "Text color"), text);
    }

    auto *save = new QPushButton(m_i18n->t("txtSaveProfile", "Save profile"), box);
    connect(save, &QPushButton::clicked, this, &LcdPage::saveAnimation);
    form->addRow(save);

    auto *uploadHeading = new QLabel(m_i18n->t("txtUploadImage", "Upload image"), box);
    QFont uploadFont = uploadHeading->font();
    uploadFont.setBold(true);
    uploadHeading->setFont(uploadFont);
    auto *hint = new QLabel(tr("GIF, JPEG, WebP or BMP. Maximum 5 MB."), box);
    hint->setWordWrap(true);
    auto *upload = new QPushButton(m_i18n->t("txtUpload", "Upload"), box);
    form->addRow(uploadHeading);
    form->addRow(hint);
    form->addRow(upload);
    connect(upload, &QPushButton::clicked, this, &LcdPage::upload);
    return box;
}

void LcdPage::upload()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                     m_i18n->t("txtUploadImage", "Upload image"),
                                                     {},
                                                     tr("Images (*.gif *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_status->setText(tr("Unable to read file"));
        return;
    }
    const QByteArray data = file.readAll();
    if (data.size() > 5 * 1024 * 1024) {
        m_status->setText(m_i18n->t("txtFileTooLarge", "File is too large (max 5 MB)"));
        return;
    }

    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"animationFile\"; filename=\"%1\"").arg(QFileInfo(path).fileName()));
    filePart.setBody(data);
    multi->append(filePart);

    m_client->postMultipart(QStringLiteral("/api/lcd/upload"), multi, [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        m_status->setText(Json::str(json, "message", m_i18n->t("txtUpload", "Upload complete")));
        rebuild();
    });
}

void LcdPage::saveArc()
{
    auto *sensor = findChild<QComboBox *>(QStringLiteral("arc_sensor"));
    auto *margin = findChild<QDoubleSpinBox *>(QStringLiteral("arc_margin"));
    auto *thickness = findChild<QDoubleSpinBox *>(QStringLiteral("arc_thickness"));
    auto *gap = findChild<QDoubleSpinBox *>(QStringLiteral("arc_gap"));
    if (!sensor || !margin || !thickness || !gap) {
        return;
    }
    m_client->put(QStringLiteral("/api/lcd/modes"),
                  QJsonObject{
                      {QStringLiteral("profileId"), 100},
                      {QStringLiteral("sensor"), sensor->currentData().toInt()},
                      {QStringLiteral("margin"), margin->value()},
                      {QStringLiteral("thickness"), thickness->value()},
                      {QStringLiteral("gapRadians"), gap->value()},
                      {QStringLiteral("backgroundColor"), colorJson(findChild<KColorButton *>(QStringLiteral("arc_background")))},
                      {QStringLiteral("borderColor"), colorJson(findChild<KColorButton *>(QStringLiteral("arc_border")))},
                      {QStringLiteral("startColor"), colorJson(findChild<KColorButton *>(QStringLiteral("arc_start")))},
                      {QStringLiteral("endColor"), colorJson(findChild<KColorButton *>(QStringLiteral("arc_end")))},
                      {QStringLiteral("textColor"), colorJson(findChild<KColorButton *>(QStringLiteral("arc_text")))},
                  },
                  [this](const QJsonObject &json, const QString &error) {
                      m_status->setText(error.isEmpty() ? Json::str(json, "message", tr("Saved")) : error);
                  });
}

void LcdPage::saveDoubleArc()
{
    auto *margin = findChild<QDoubleSpinBox *>(QStringLiteral("da_margin"));
    auto *thickness = findChild<QDoubleSpinBox *>(QStringLiteral("da_thickness"));
    auto *gap = findChild<QDoubleSpinBox *>(QStringLiteral("da_gap"));
    if (!margin || !thickness || !gap) {
        return;
    }

    QJsonObject arcs;
    for (int id = 0; id < 2; ++id) {
        auto *sensor = findChild<QComboBox *>(QStringLiteral("da_sensor_%1").arg(id));
        auto *start = findChild<KColorButton *>(QStringLiteral("da_start_%1").arg(id));
        auto *end = findChild<KColorButton *>(QStringLiteral("da_end_%1").arg(id));
        auto *text = findChild<KColorButton *>(QStringLiteral("da_text_%1").arg(id));
        if (!sensor || !start || !end || !text) {
            continue;
        }
        arcs.insert(QString::number(id),
                    QJsonObject{
                        {QStringLiteral("sensor"), sensor->currentData().toInt()},
                        {QStringLiteral("startColor"), colorJson(start)},
                        {QStringLiteral("endColor"), colorJson(end)},
                        {QStringLiteral("textColor"), colorJson(text)},
                    });
    }

    m_client->put(QStringLiteral("/api/lcd/modes"),
                  QJsonObject{
                      {QStringLiteral("profileId"), 101},
                      {QStringLiteral("margin"), margin->value()},
                      {QStringLiteral("thickness"), thickness->value()},
                      {QStringLiteral("gapRadians"), gap->value()},
                      {QStringLiteral("backgroundColor"), colorJson(findChild<KColorButton *>(QStringLiteral("da_background")))},
                      {QStringLiteral("borderColor"), colorJson(findChild<KColorButton *>(QStringLiteral("da_border")))},
                      {QStringLiteral("separatorColor"), colorJson(findChild<KColorButton *>(QStringLiteral("da_separator")))},
                      {QStringLiteral("arcs"), arcs},
                  },
                  [this](const QJsonObject &json, const QString &error) {
                      m_status->setText(error.isEmpty() ? Json::str(json, "message", tr("Saved")) : error);
                  });
}

void LcdPage::saveAnimation()
{
    auto *background = findChild<QComboBox *>(QStringLiteral("anim_background"));
    auto *margin = findChild<QDoubleSpinBox *>(QStringLiteral("anim_margin"));
    auto *workers = findChild<QSpinBox *>(QStringLiteral("anim_workers"));
    auto *frameDelay = findChild<QSpinBox *>(QStringLiteral("anim_framedelay"));
    if (!background || !margin || !workers || !frameDelay) {
        return;
    }

    QJsonObject sensors;
    for (int id = 0; id <= 2; ++id) {
        auto *enabled = findChild<QCheckBox *>(QStringLiteral("anim_enabled_%1").arg(id));
        auto *type = findChild<QComboBox *>(QStringLiteral("anim_sensor_%1").arg(id));
        auto *text = findChild<KColorButton *>(QStringLiteral("anim_text_%1").arg(id));
        if (!enabled || !type || !text) {
            continue;
        }
        sensors.insert(QString::number(id),
                       QJsonObject{
                           {QStringLiteral("sensor"), type->currentData().toInt()},
                           {QStringLiteral("textColor"), colorJson(text)},
                           {QStringLiteral("enabled"), enabled->isChecked()},
                       });
    }

    m_client->put(QStringLiteral("/api/lcd/modes"),
                  QJsonObject{
                      {QStringLiteral("profileId"), 102},
                      {QStringLiteral("margin"), margin->value()},
                      {QStringLiteral("workers"), workers->value()},
                      {QStringLiteral("frameDelay"), frameDelay->value()},
                      {QStringLiteral("backgroundImage"), background->currentData().toString()},
                      {QStringLiteral("separatorColor"), colorJson(findChild<KColorButton *>(QStringLiteral("anim_separator")))},
                      {QStringLiteral("sensors"), sensors},
                  },
                  [this](const QJsonObject &json, const QString &error) {
                      m_status->setText(error.isEmpty() ? Json::str(json, "message", tr("Saved")) : error);
                  });
}
