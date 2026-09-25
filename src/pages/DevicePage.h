#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QWidget>

class ApiClient;
class HubI18n;
class CardGrid;
class QComboBox;
class QLabel;
class QSlider;
class QCheckBox;

class DevicePage : public QWidget
{
    Q_OBJECT

public:
    DevicePage(ApiClient *client, HubI18n *i18n, const QString &serial, QWidget *parent = nullptr);

    QString serial() const;
    void setSpeedProfiles(const QStringList &profiles);
    void reload();
    void refreshTelemetry();

private:
    void rebuild(const QJsonObject &device);
    void addHeaderCard(const QJsonObject &device);
    void addChannelCards(const QJsonObject &device);
    void addHeadsetCards(const QJsonObject &device);
    void fillCombo(QComboBox *combo, const QStringList &values, const QString &current);
    void setSpeed(int channelId, const QString &profile);
    void setRgb(int channelId, const QString &profile);
    void setLabel(int channelId, const QString &label);
    QWidget *channelCard(const QJsonObject &device, const QJsonObject &channel);

    ApiClient *m_client;
    HubI18n *m_i18n;
    QString m_serial;
    QStringList m_speedProfiles;
    CardGrid *m_grid;
    QJsonObject m_device;
    bool m_building = false;
};
