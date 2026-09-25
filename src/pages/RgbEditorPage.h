#pragma once

#include <QJsonObject>
#include <QWidget>

class ApiClient;
class HubI18n;
class CardGrid;
class QListWidget;

class RgbEditorPage : public QWidget
{
    Q_OBJECT

public:
    RgbEditorPage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);
    void reload();

private:
    void showDevice(const QString &serial);
    void configureProfile(const QString &serial, const QString &profile, const QJsonObject &data);

    ApiClient *m_client;
    HubI18n *m_i18n;
    QListWidget *m_devices;
    CardGrid *m_grid;
};
