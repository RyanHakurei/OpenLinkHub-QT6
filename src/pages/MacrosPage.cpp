#include "pages/MacrosPage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"
#include "widgets/ResponsiveSplit.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

MacrosPage::MacrosPage(ApiClient *client, HubI18n *i18n, QWidget *parent)
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
    m_name = new QLineEdit(side);
    m_name->setPlaceholderText(m_i18n->t("txtProfileName", "Profile name"));
    auto *createButton = new QPushButton(m_i18n->t("txtSave", "Save"), side);
    auto *deleteButton = new QPushButton(m_i18n->t("txtDelete", "Delete"), side);
    left->addWidget(m_list, 1);
    left->addWidget(m_name);
    left->addWidget(createButton);
    left->addWidget(deleteButton);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({m_i18n->t("txtType", "Type"), m_i18n->t("txtValue", "Value"), m_i18n->t("txtDelay", "Delay")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    layout->addWidget(new ResponsiveSplit(side, m_table, this));

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) {
            showMacro(item->data(Qt::UserRole).toInt());
        }
    });
    connect(createButton, &QPushButton::clicked, this, &MacrosPage::createMacro);
    connect(deleteButton, &QPushButton::clicked, this, &MacrosPage::deleteMacro);
}

void MacrosPage::reload()
{
    m_client->get(QStringLiteral("/api/macro/"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        m_list->clear();
        const QJsonObject data = Json::object(json, "data");
        for (auto it = data.begin(); it != data.end(); ++it) {
            const QJsonObject macro = Json::object(it.value());
            auto *item = new QListWidgetItem(Json::str(macro, "name", it.key()));
            item->setData(Qt::UserRole, Json::integer(macro, "id", it.key().toInt()));
            m_list->addItem(item);
        }
        if (m_list->count() == 0) {
            m_table->setRowCount(0);
        }
    });
}

void MacrosPage::showMacro(int id)
{
    m_current = id;
    m_client->get(QStringLiteral("/api/macro/%1").arg(id), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject data = Json::object(json, "data");
        const QJsonObject actions = Json::object(data, "actions");
        QList<int> keys;
        for (auto it = actions.begin(); it != actions.end(); ++it) {
            keys.append(it.key().toInt());
        }
        std::sort(keys.begin(), keys.end());
        m_table->setRowCount(keys.size());
        int row = 0;
        for (int key : keys) {
            const QJsonObject action = Json::object(actions.value(QString::number(key)));
            m_table->setItem(row, 0, new QTableWidgetItem(QString::number(Json::integer(action, "actionType"))));
            m_table->setItem(row, 1, new QTableWidgetItem(QString::number(Json::integer(action, "actionCommand"))));
            m_table->setItem(row, 2, new QTableWidgetItem(QString::number(Json::integer(action, "actionDelay"))));
            ++row;
        }
    });
}

void MacrosPage::createMacro()
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        return;
    }
    m_client->put(QStringLiteral("/api/macro/new"),
                  QJsonObject{{QStringLiteral("macroName"), name}},
                  [this](const QJsonObject &, const QString &) {
                      reload();
                  });
}

void MacrosPage::deleteMacro()
{
    if (m_current < 0) {
        return;
    }
    m_client->del(QStringLiteral("/api/macro/profile"),
                  QJsonObject{{QStringLiteral("macroId"), m_current}},
                  [this](const QJsonObject &, const QString &) {
                      m_current = -1;
                      reload();
                  });
}
