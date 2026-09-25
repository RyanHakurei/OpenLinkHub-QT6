#pragma once

#include <QJsonObject>
#include <QWidget>

class ApiClient;
class HubI18n;
class CardGrid;
class QComboBox;
class QLabel;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    DashboardPage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);

    void setDeviceChoices(const QList<QPair<QString, QString>> &devices);
    void reload();
    void refreshTelemetry();
    void setShowLabels(bool show);

private:
    void renderPinned(const QStringList &serials);
    void addDeviceCard(const QString &serial, const QJsonObject &device);
    void updateDeviceCard(const QString &serial, const QJsonObject &device);

    ApiClient *m_client;
    HubI18n *m_i18n;
    CardGrid *m_grid;
    QComboBox *m_deviceSelect;
    bool m_showLabels = true;
    QStringList m_pinned;
};
