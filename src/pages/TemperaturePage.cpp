#include "pages/TemperaturePage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"
#include "widgets/FanCurveChart.h"
#include "widgets/ResponsiveSplit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QVector<QPointF> readPoints(const QJsonObject &series, int maxTemperature)
{
    QVector<QPointF> points;
    const QJsonArray values = Json::array(series, "points");
    for (const QJsonValue &value : values) {
        const QJsonObject point = value.toObject();
        const double temperature = qBound(0.0, point.value(QStringLiteral("x")).toDouble(), static_cast<double>(maxTemperature));
        const double speed = qBound(0.0, point.value(QStringLiteral("y")).toDouble(), 100.0);
        points.append({temperature, speed});
    }
    return points;
}

QJsonArray writePoints(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points) {
        array.append(QJsonObject{
            {QStringLiteral("x"), point.x()},
            {QStringLiteral("y"), point.y()},
        });
    }
    return array;
}

} // namespace

TemperaturePage::TemperaturePage(ApiClient *client, HubI18n *i18n, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *side = new QWidget(this);
    auto *left = new QVBoxLayout(side);
    left->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(side);
    left->addWidget(m_list, 1);

    auto *createBox = new QGroupBox(m_i18n->t("txtNewTempProfile", "New temperature profile"), this);
    auto *createForm = new QFormLayout(createBox);
    m_newName = new QLineEdit(createBox);
    m_sensor = new QComboBox(createBox);
    m_sensor->addItem(tr("CPU"), 0);
    m_sensor->addItem(tr("GPU"), 1);
    m_sensor->addItem(tr("Liquid"), 2);
    m_zeroRpm = new QCheckBox(createBox);
    m_staticMode = new QCheckBox(createBox);
    m_linear = new QCheckBox(createBox);
    auto *createButton = new QPushButton(m_i18n->t("txtSave", "Save"), createBox);
    createForm->addRow(m_i18n->t("txtProfileName", "Profile name"), m_newName);
    createForm->addRow(m_i18n->t("txtSensor", "Sensor"), m_sensor);
    createForm->addRow(tr("Zero RPM"), m_zeroRpm);
    createForm->addRow(tr("Static"), m_staticMode);
    createForm->addRow(tr("Linear"), m_linear);
    createForm->addRow(QString(), createButton);
    left->addWidget(createBox);

    auto *main = new QWidget(this);
    auto *right = new QVBoxLayout(main);
    right->setContentsMargins(0, 0, 0, 0);
    m_details = new QLabel(tr("Select a profile"), main);
    m_details->setWordWrap(true);
    right->addWidget(m_details);

    auto *graphs = new QHBoxLayout;
    graphs->setSpacing(12);
    auto addCurve = [this, graphs](const QString &title, FanCurveChart **chart, QPushButton **save) {
        auto *box = new QGroupBox(title, m_details->parentWidget());
        auto *boxLayout = new QVBoxLayout(box);
        *chart = new FanCurveChart(box);
        *save = new QPushButton(m_i18n->t("txtSave", "Save"), box);
        boxLayout->addWidget(*chart, 1);
        boxLayout->addWidget(*save, 0, Qt::AlignLeft);
        graphs->addWidget(box, 1);
    };
    addCurve(tr("Pump speed"), &m_pump, &m_savePump);
    addCurve(tr("Fan speed"), &m_fans, &m_saveFans);
    right->addLayout(graphs, 1);

    auto *buttons = new QHBoxLayout;
    m_deleteButton = new QPushButton(m_i18n->t("txtDelete", "Delete"), main);
    buttons->addWidget(m_deleteButton);
    buttons->addStretch();
    right->addLayout(buttons);

    layout->addWidget(new ResponsiveSplit(side, main, this));

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (!item) {
            return;
        }
        const QString sensor = item->data(Qt::UserRole + 1).toString();
        const bool zeroRpm = item->data(Qt::UserRole + 2).toBool();
        m_details->setText(tr("%1 · Zero RPM %2").arg(sensor.isEmpty() ? tr("Sensor") : sensor, zeroRpm ? tr("enabled") : tr("disabled")));
        showProfile(item->data(Qt::UserRole).toString());
    });
    connect(createButton, &QPushButton::clicked, this, &TemperaturePage::createProfile);
    connect(m_deleteButton, &QPushButton::clicked, this, &TemperaturePage::deleteProfile);
    connect(m_savePump, &QPushButton::clicked, this, [this]() {
        saveCurve(0);
    });
    connect(m_saveFans, &QPushButton::clicked, this, [this]() {
        saveCurve(1);
    });
}

QStringList TemperaturePage::visibleProfiles() const
{
    return m_visible;
}

bool TemperaturePage::isBuiltIn(const QString &name)
{
    return name == QLatin1String("Quiet") || name == QLatin1String("Normal") || name == QLatin1String("Performance");
}

void TemperaturePage::reload()
{
    m_client->get(QStringLiteral("/api/temperatures/"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QString current = m_current;
        m_list->clear();
        m_visible.clear();
        const QJsonObject data = Json::object(json, "data");
        for (auto it = data.begin(); it != data.end(); ++it) {
            const QJsonObject profile = Json::object(it.value());
            if (Json::boolean(profile, "Hidden")) {
                continue;
            }
            m_visible.append(it.key());
            if (isBuiltIn(it.key())) {
                continue;
            }
            auto *item = new QListWidgetItem(it.key());
            item->setData(Qt::UserRole, it.key());
            item->setData(Qt::UserRole + 1, Json::str(profile, "sensorString"));
            item->setData(Qt::UserRole + 2, Json::boolean(profile, "zeroRpm"));
            m_list->addItem(item);
        }
        m_visible.sort();
        m_list->sortItems();
        Q_EMIT profilesChanged();
        if (!current.isEmpty()) {
            for (int i = 0; i < m_list->count(); ++i) {
                if (m_list->item(i)->data(Qt::UserRole).toString() == current) {
                    m_list->setCurrentRow(i);
                    return;
                }
            }
        }
        if (m_list->count() > 0) {
            m_list->setCurrentRow(0);
        }
    });
}

void TemperaturePage::showProfile(const QString &name)
{
    m_current = name;
    m_client->get(QStringLiteral("/api/temperatures/graph/") + name, [this, name](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty() || m_current != name) {
            return;
        }
        const QJsonObject data = Json::object(json, "data");
        const QJsonObject pump = Json::object(data, "0");
        const QJsonObject fans = Json::object(data, "1");
        const int sensor = Json::integer(pump, "sensor", Json::integer(fans, "sensor"));
        const int maxTemperature = sensor == 2 ? 60 : 100;
        m_pump->setMaxTemperature(maxTemperature);
        m_fans->setMaxTemperature(maxTemperature);
        m_pump->setPoints(readPoints(pump, maxTemperature));
        m_fans->setPoints(readPoints(fans, maxTemperature));
    });
}

void TemperaturePage::createProfile()
{
    const QString name = m_newName->text().trimmed();
    if (name.size() < 3 || isBuiltIn(name)) {
        return;
    }
    m_client->post(QStringLiteral("/api/temperatures/new"),
                   QJsonObject{
                       {QStringLiteral("profile"), name},
                       {QStringLiteral("sensor"), m_sensor->currentData().toInt()},
                       {QStringLiteral("zeroRpm"), m_zeroRpm->isChecked()},
                       {QStringLiteral("static"), m_staticMode->isChecked()},
                       {QStringLiteral("linear"), m_linear->isChecked()},
                   },
                   [this](const QJsonObject &, const QString &) {
                       m_newName->clear();
                       reload();
                   });
}

void TemperaturePage::deleteProfile()
{
    if (m_current.isEmpty() || isBuiltIn(m_current)) {
        return;
    }
    m_client->del(QStringLiteral("/api/temperatures/delete"),
                  QJsonObject{{QStringLiteral("profile"), m_current}},
                  [this](const QJsonObject &, const QString &) {
                      m_current.clear();
                      reload();
                  });
}

void TemperaturePage::saveCurve(int updateType)
{
    if (m_current.isEmpty()) {
        return;
    }
    const FanCurveChart *chart = updateType == 0 ? m_pump : m_fans;
    m_client->put(QStringLiteral("/api/temperatures/updateGraph"),
                  QJsonObject{
                      {QStringLiteral("profile"), m_current},
                      {QStringLiteral("updateType"), updateType},
                      {QStringLiteral("points"), writePoints(chart->points())},
                  },
                  {});
}
