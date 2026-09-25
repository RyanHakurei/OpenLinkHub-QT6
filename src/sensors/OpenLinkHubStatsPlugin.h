#pragma once

#include <systemstats/SensorPlugin.h>

#include <QHash>

namespace KSysGuard
{
class SensorContainer;
class SensorObject;
class SensorProperty;
}

class OpenLinkHubStatsPlugin : public KSysGuard::SensorPlugin
{
    Q_OBJECT

public:
    OpenLinkHubStatsPlugin(QObject *parent, const QVariantList &args);
    ~OpenLinkHubStatsPlugin() override;

    QString providerName() const override;
    void update() override;

private:
    void syncSnapshot();

    KSysGuard::SensorContainer *m_container = nullptr;
    QHash<QString, KSysGuard::SensorObject *> m_objects;
    QHash<QString, KSysGuard::SensorProperty *> m_properties;
};
