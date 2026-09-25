#pragma once

#include <QString>
#include <QUrl>

class AppSettings
{
public:
    AppSettings();

    void load();
    void save() const;

    QUrl baseUrl() const;
    QString host() const;
    int port() const;
    bool https() const;
    int pollIntervalMs() const;

    void setHost(const QString &host);
    void setPort(int port);
    void setHttps(bool https);
    void setPollIntervalMs(int ms);

private:
    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = 27003;
    bool m_https = false;
    int m_pollIntervalMs = 2000;
};
