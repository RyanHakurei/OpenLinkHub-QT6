#pragma once

#include <QWidget>

class ApiClient;
class HubI18n;
class CardGrid;
class QLabel;

class LcdPage : public QWidget
{
    Q_OBJECT

public:
    LcdPage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);
    void reload();

private:
    void rebuild();
    void upload();
    void saveArc();
    void saveDoubleArc();
    void saveAnimation();
    QWidget *arcCard(const QJsonObject &profile);
    QWidget *doubleArcCard(const QJsonObject &profile);
    QWidget *animationCard(const QJsonObject &profile);

    ApiClient *m_client;
    HubI18n *m_i18n;
    CardGrid *m_grid;
    QLabel *m_status;
};
