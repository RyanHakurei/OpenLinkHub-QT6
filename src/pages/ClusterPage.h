#pragma once

#include <QWidget>

class ApiClient;
class HubI18n;
class CardGrid;
class QSlider;
class QComboBox;

class ClusterPage : public QWidget
{
    Q_OBJECT

public:
    ClusterPage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);
    void reload();

private:
    void rebuild(const QJsonObject &device);

    ApiClient *m_client;
    HubI18n *m_i18n;
    CardGrid *m_grid;
};
