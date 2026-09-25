#pragma once

#include <QWidget>

class ApiClient;
class HubI18n;
class QListWidget;
class QTableWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QPushButton;

class TemperaturePage : public QWidget
{
    Q_OBJECT

public:
    TemperaturePage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);
    void reload();
    QStringList visibleProfiles() const;

Q_SIGNALS:
    void profilesChanged();

private:
    void showProfile(const QString &name);
    void createProfile();
    void deleteProfile();
    void saveProfile();

    ApiClient *m_client;
    HubI18n *m_i18n;
    QListWidget *m_list;
    QTableWidget *m_table;
    QLineEdit *m_newName;
    QComboBox *m_sensor;
    QCheckBox *m_zeroRpm;
    QCheckBox *m_staticMode;
    QCheckBox *m_linear;
    QPushButton *m_updateButton;
    QString m_current;
    QStringList m_visible;
};
