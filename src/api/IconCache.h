#pragma once

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QSet>
#include <QString>

class ApiClient;

class IconCache : public QObject
{
    Q_OBJECT

public:
    explicit IconCache(ApiClient *client, QObject *parent = nullptr);

    QIcon iconFor(const QString &product, const QString &imageName);

Q_SIGNALS:
    void iconReady(const QString &imageName, const QIcon &icon);

private:
    QIcon themeIconForProduct(const QString &product) const;
    void fetch(const QString &imageName);

    ApiClient *m_client;
    QHash<QString, QIcon> m_remote;
    QSet<QString> m_pending;
};
