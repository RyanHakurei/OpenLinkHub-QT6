#pragma once

#include <QWidget>

class ApiClient;
class FanCurveChart;
class HubI18n;
class QListWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
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
    void saveCurve(int updateType);
    static bool isBuiltIn(const QString &name);

    ApiClient *m_client;
    HubI18n *m_i18n;
    QListWidget *m_list;
    QLineEdit *m_newName;
    QComboBox *m_sensor;
    QCheckBox *m_zeroRpm;
    QCheckBox *m_staticMode;
    QCheckBox *m_linear;
    QLabel *m_details;
    FanCurveChart *m_pump = nullptr;
    FanCurveChart *m_fans = nullptr;
    QPushButton *m_savePump = nullptr;
    QPushButton *m_saveFans = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QString m_current;
    QStringList m_visible;
};
