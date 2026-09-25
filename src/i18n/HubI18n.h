#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class ApiClient;

class HubI18n : public QObject
{
    Q_OBJECT

public:
    explicit HubI18n(ApiClient *client, QObject *parent = nullptr);

    void reload();
    QString t(const char *key, const char *fallback = nullptr) const;
    QString code() const;

Q_SIGNALS:
    void changed();

private:
    ApiClient *m_client;
    QHash<QString, QString> m_values;
    QString m_code;
};
