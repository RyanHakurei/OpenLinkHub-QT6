#include "app/AppSettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

AppSettings::AppSettings()
{
    load();
}

void AppSettings::load()
{
    const KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("General"));
    m_host = group.readEntry(QStringLiteral("Host"), m_host);
    m_port = group.readEntry(QStringLiteral("Port"), m_port);
    m_https = group.readEntry(QStringLiteral("Https"), m_https);
    m_pollIntervalMs = group.readEntry(QStringLiteral("PollIntervalMs"), m_pollIntervalMs);
}

void AppSettings::save() const
{
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("General"));
    group.writeEntry(QStringLiteral("Host"), m_host);
    group.writeEntry(QStringLiteral("Port"), m_port);
    group.writeEntry(QStringLiteral("Https"), m_https);
    group.writeEntry(QStringLiteral("PollIntervalMs"), m_pollIntervalMs);
    group.sync();
}

QUrl AppSettings::baseUrl() const
{
    QUrl url;
    url.setScheme(m_https ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    return url;
}

QString AppSettings::host() const
{
    return m_host;
}

int AppSettings::port() const
{
    return m_port;
}

bool AppSettings::https() const
{
    return m_https;
}

int AppSettings::pollIntervalMs() const
{
    return m_pollIntervalMs;
}

void AppSettings::setHost(const QString &host)
{
    m_host = host;
}

void AppSettings::setPort(int port)
{
    m_port = port;
}

void AppSettings::setHttps(bool https)
{
    m_https = https;
}

void AppSettings::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qMax(500, ms);
}
