#include "api/IconCache.h"

#include "api/ApiClient.h"

#include <QIcon>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>

IconCache::IconCache(ApiClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

QIcon IconCache::iconFor(const QString &product, const QString &imageName)
{
    if (!imageName.isEmpty() && m_remote.contains(imageName)) {
        return m_remote.value(imageName);
    }
    if (!imageName.isEmpty()) {
        fetch(imageName);
    }
    return themeIconForProduct(product);
}

QIcon IconCache::themeIconForProduct(const QString &product) const
{
    const QString lower = product.toLower();
    auto theme = [](const char *name, const char *fallback) {
        QIcon icon = QIcon::fromTheme(QLatin1String(name));
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QLatin1String(fallback));
        }
        return icon;
    };

    if (lower.contains(QLatin1String("keyboard")) || lower.contains(QLatin1String("k70")) || lower.contains(QLatin1String("k100")) || lower.contains(QLatin1String("k55"))) {
        return theme("input-keyboard", "input-gaming");
    }
    if (lower.contains(QLatin1String("mouse")) || lower.contains(QLatin1String("scimitar")) || lower.contains(QLatin1String("harpoon")) || lower.contains(QLatin1String("m65")) || lower.contains(QLatin1String("ironclaw"))) {
        return theme("input-mouse", "input-gaming");
    }
    if (lower.contains(QLatin1String("headset")) || lower.contains(QLatin1String("void")) || lower.contains(QLatin1String("virtuoso")) || lower.contains(QLatin1String("hs80"))) {
        return theme("audio-headphones", "audio-headset");
    }
    if (lower.contains(QLatin1String("cluster"))) {
        return theme("folder-colors", "color-management");
    }
    if (lower.contains(QLatin1String("psu")) || lower.contains(QLatin1String("hx"))) {
        return theme("battery", "preferences-system-power-management");
    }
    if (lower.contains(QLatin1String("hub")) || lower.contains(QLatin1String("link")) || lower.contains(QLatin1String("commander"))) {
        return theme("network-server", "computer");
    }
    if (lower.contains(QLatin1String("memory")) || lower.contains(QLatin1String("ram"))) {
        return theme("media-flash-memory-stick", "cpu");
    }
    return theme("input-gaming", "computer");
}

void IconCache::fetch(const QString &imageName)
{
    if (m_pending.contains(imageName) || !m_client) {
        return;
    }
    m_pending.insert(imageName);

    const QString path = QStringLiteral("/static/img/icons/") + imageName;
    m_client->getBinary(path, [this, imageName](const QByteArray &data, const QString &error, const QString &) {
        m_pending.remove(imageName);
        if (!error.isEmpty() || data.isEmpty()) {
            return;
        }

        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QSvgRenderer renderer(data);
        if (renderer.isValid()) {
            QPainter painter(&pixmap);
            renderer.render(&painter);
        } else {
            pixmap.loadFromData(data);
        }
        if (pixmap.isNull()) {
            return;
        }

        const QIcon icon(pixmap);
        m_remote.insert(imageName, icon);
        Q_EMIT iconReady(imageName, icon);
    });
}
