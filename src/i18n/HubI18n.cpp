#include "i18n/HubI18n.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"

#include <QJsonObject>

HubI18n::HubI18n(ApiClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

void HubI18n::reload()
{
    m_client->get(QStringLiteral("/api/language/"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject data = Json::object(json, "data");
        m_code = Json::str(data, "code", QStringLiteral("en_US"));
        m_values.clear();
        const QJsonObject values = Json::object(data, "values");
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (it.value().isString()) {
                m_values.insert(it.key(), it.value().toString());
            }
        }
        Q_EMIT changed();
    });
}

QString HubI18n::t(const char *key, const char *fallback) const
{
    const QString found = m_values.value(QLatin1String(key));
    if (!found.isEmpty()) {
        return found;
    }
    if (fallback) {
        return QString::fromUtf8(fallback);
    }
    return QString::fromUtf8(key);
}

QString HubI18n::code() const
{
    return m_code;
}
