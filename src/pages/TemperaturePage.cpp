#include "pages/TemperaturePage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"
#include "widgets/ResponsiveSplit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QUrlQuery>
#include <QVBoxLayout>

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
    m_table = new QTableWidget(0, 5, main);
    m_table->setHorizontalHeaderLabels({tr("Id"), tr("Min"), tr("Max"), tr("Fans"), tr("Pump")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *buttons = new QHBoxLayout;
    m_updateButton = new QPushButton(m_i18n->t("txtSave", "Save"), main);
    auto *deleteButton = new QPushButton(m_i18n->t("txtDelete", "Delete"), main);
    buttons->addWidget(m_updateButton);
    buttons->addWidget(deleteButton);
    buttons->addStretch();
    right->addWidget(m_table, 1);
    right->addLayout(buttons);

    layout->addWidget(new ResponsiveSplit(side, main, this));

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) {
            showProfile(item->data(Qt::UserRole).toString());
        }
    });
    connect(createButton, &QPushButton::clicked, this, &TemperaturePage::createProfile);
    connect(deleteButton, &QPushButton::clicked, this, &TemperaturePage::deleteProfile);
    connect(m_updateButton, &QPushButton::clicked, this, &TemperaturePage::saveProfile);
}

QStringList TemperaturePage::visibleProfiles() const
{
    return m_visible;
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
            auto *item = new QListWidgetItem(it.key());
            item->setData(Qt::UserRole, it.key());
            m_list->addItem(item);
        }
        m_visible.sort();
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
    m_client->get(QStringLiteral("/api/temperatures/") + name, [this, name](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject data = Json::object(json, "data");
        const QJsonArray profiles = Json::array(data, "profiles");
        const bool editable = name != QLatin1String("Quiet") && name != QLatin1String("Normal") && name != QLatin1String("Performance") && !Json::boolean(data, "linear");
        m_updateButton->setEnabled(editable);
        m_table->setRowCount(profiles.size());
        int row = 0;
        for (const QJsonValue &value : profiles) {
            const QJsonObject step = value.toObject();
            m_table->setItem(row, 0, new QTableWidgetItem(QString::number(Json::integer(step, "id"))));
            m_table->setItem(row, 1, new QTableWidgetItem(QString::number(Json::integer(step, "min"))));
            m_table->setItem(row, 2, new QTableWidgetItem(QString::number(Json::integer(step, "max"))));
            auto *fans = new QTableWidgetItem(QString::number(Json::integer(step, "fans")));
            auto *pump = new QTableWidgetItem(QString::number(Json::integer(step, "pump")));
            if (editable) {
                fans->setFlags(fans->flags() | Qt::ItemIsEditable);
                pump->setFlags(pump->flags() | Qt::ItemIsEditable);
            } else {
                fans->setFlags(fans->flags() & ~Qt::ItemIsEditable);
                pump->setFlags(pump->flags() & ~Qt::ItemIsEditable);
            }
            m_table->setItem(row, 3, fans);
            m_table->setItem(row, 4, pump);
            ++row;
        }
    });
}

void TemperaturePage::createProfile()
{
    const QString name = m_newName->text().trimmed();
    if (name.size() < 3) {
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
                       reload();
                   });
}

void TemperaturePage::deleteProfile()
{
    if (m_current.isEmpty()) {
        return;
    }
    m_client->del(QStringLiteral("/api/temperatures/delete"),
                  QJsonObject{{QStringLiteral("profile"), m_current}},
                  [this](const QJsonObject &, const QString &) {
                      m_current.clear();
                      reload();
                  });
}

void TemperaturePage::saveProfile()
{
    if (m_current.isEmpty()) {
        return;
    }
    QJsonObject data;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const int id = m_table->item(row, 0)->text().toInt();
        data.insert(QString::number(id),
                    QJsonObject{
                        {QStringLiteral("fans"), m_table->item(row, 3)->text().toInt()},
                        {QStringLiteral("pump"), m_table->item(row, 4)->text().toInt()},
                    });
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("profile"), m_current);
    query.addQueryItem(QStringLiteral("data"), QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
    m_client->putForm(QStringLiteral("/api/temperatures/update"), query, {});
}
