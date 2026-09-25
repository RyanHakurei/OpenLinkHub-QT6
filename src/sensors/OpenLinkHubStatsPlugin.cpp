#include "sensors/OpenLinkHubStatsPlugin.h"

#include "sensors/SensorSnapshot.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <systemstats/SensorContainer.h>
#include <systemstats/SensorObject.h>
#include <systemstats/SensorProperty.h>

#include <QSet>
#include <QVariant>

using namespace KSysGuard;

static Unit unitFromName(const QString &name)
{
    if (name == QLatin1String("rpm")) {
        return UnitRpm;
    }
    if (name == QLatin1String("celsius")) {
        return UnitCelsius;
    }
    if (name == QLatin1String("volt")) {
        return UnitVolt;
    }
    if (name == QLatin1String("ampere")) {
        return UnitAmpere;
    }
    if (name == QLatin1String("watt")) {
        return UnitWatt;
    }
    return UnitNone;
}

OpenLinkHubStatsPlugin::OpenLinkHubStatsPlugin(QObject *parent, const QVariantList &args)
    : SensorPlugin(parent, args)
    , m_container(new SensorContainer(QStringLiteral("openlinkhub"), i18nc("@title", "OpenLinkHub"), this))
{
    syncSnapshot();
}

OpenLinkHubStatsPlugin::~OpenLinkHubStatsPlugin() = default;

QString OpenLinkHubStatsPlugin::providerName() const
{
    return QStringLiteral("openlinkhub");
}

void OpenLinkHubStatsPlugin::update()
{
    syncSnapshot();
}

void OpenLinkHubStatsPlugin::syncSnapshot()
{
    const SensorSnapshot::Snapshot snapshot = SensorSnapshot::read();
    QSet<QString> seenObjects;
    QSet<QString> seenProperties;

    for (const SensorSnapshot::Object &object : snapshot.objects) {
        seenObjects.insert(object.id);
        SensorObject *sensorObject = m_objects.value(object.id);
        if (!sensorObject) {
            sensorObject = new SensorObject(object.id, object.name, m_container);
            m_objects.insert(object.id, sensorObject);
        } else if (sensorObject->name() != object.name) {
            sensorObject->setName(object.name);
        }

        for (const SensorSnapshot::Property &property : object.properties) {
            const QString path = object.id + QLatin1Char('/') + property.id;
            seenProperties.insert(path);
            SensorProperty *sensor = m_properties.value(path);
            if (!sensor) {
                sensor = new SensorProperty(property.id, property.name, sensorObject);
                sensor->setShortName(property.shortName);
                sensor->setUnit(unitFromName(property.unit));
                sensor->setVariantType(QVariant::Double);
                sensor->setMin(property.min);
                sensor->setMax(property.max);
                m_properties.insert(path, sensor);
            }
            sensor->setValue(property.value);
        }
    }

    for (auto it = m_properties.begin(); it != m_properties.end();) {
        if (!seenProperties.contains(it.key())) {
            it.value()->deleteLater();
            it = m_properties.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_objects.begin(); it != m_objects.end();) {
        if (!seenObjects.contains(it.key())) {
            m_container->removeObject(it.value());
            it.value()->deleteLater();
            it = m_objects.erase(it);
        } else {
            ++it;
        }
    }
}

K_PLUGIN_CLASS_WITH_JSON(OpenLinkHubStatsPlugin, "metadata.json")

#include "OpenLinkHubStatsPlugin.moc"
