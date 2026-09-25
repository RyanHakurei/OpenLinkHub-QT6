#pragma once

#include "api/JsonUtil.h"
#include "i18n/HubI18n.h"

#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QWidget>
#include <algorithm>

namespace Telemetry {

inline void setTextIfChanged(QLabel *label, const QString &text)
{
    if (!label || label->text() == text) {
        return;
    }
    label->setText(text);
}

inline QString metricString(const QJsonValue &value)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QString text = Json::str(object, "ValueString");
        if (!text.isEmpty()) {
            return text;
        }
        if (object.contains(QLatin1String("Value"))) {
            return QString::number(Json::number(object, "Value"), 'f', 2);
        }
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'f', 2);
    }
    if (value.isString()) {
        return value.toString();
    }
    return {};
}

inline QString powerOutText(const QJsonObject &channel)
{
    const QString text = Json::str(channel, "powerOutString");
    if (!text.isEmpty()) {
        return text + QStringLiteral(" W");
    }
    if (channel.contains(QLatin1String("powerOut"))) {
        return QString::number(Json::number(channel, "powerOut"), 'f', 2) + QStringLiteral(" W");
    }
    return {};
}

inline bool hasPsuPower(const QJsonObject &channel)
{
    return Json::boolean(channel, "IsPSU") || Json::boolean(channel, "MainPSU")
        || channel.contains(QLatin1String("volts")) || channel.contains(QLatin1String("powerOut"))
        || channel.contains(QLatin1String("powerOutString"));
}

inline QLabel *valueLabel(QWidget *parent, const QString &objectName, const QString &text)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

inline QString id(const QString &serial, const QString &channelKey, const QString &field)
{
    return QStringLiteral("%1_%2_%3").arg(serial, channelKey, field);
}

inline void addPsuFields(QFormLayout *form, QWidget *parent, const QJsonObject &channel, const QString &serial, const QString &channelKey, HubI18n *i18n)
{
    if (!hasPsuPower(channel)) {
        return;
    }

    const QString power = powerOutText(channel);
    if (!power.isEmpty()) {
        form->addRow(i18n->t("txtPowerOut", "Power out"), valueLabel(parent, id(serial, channelKey, QStringLiteral("power")), power));
    }

    const QJsonObject volts = Json::object(channel, "volts");
    const QJsonObject amps = Json::object(channel, "amps");
    const QJsonObject watts = Json::object(channel, "watts");
    QList<int> keys;
    for (auto it = volts.begin(); it != volts.end(); ++it) {
        keys.append(it.key().toInt());
    }
    std::sort(keys.begin(), keys.end());

    for (int key : keys) {
        const QString keyText = QString::number(key);
        const char *nameKey = "txt3VRail";
        const char *fallback = "3.3V Rail";
        if (key == 1) {
            nameKey = "txt5VRail";
            fallback = "5V Rail";
        } else if (key == 2) {
            nameKey = "txt12VRail";
            fallback = "12V Rail";
        } else if (key != 0) {
            nameKey = "txtOutput";
            fallback = "Rail";
        }

        auto *row = new QWidget(parent);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        layout->addWidget(valueLabel(parent, id(serial, channelKey, QStringLiteral("volts_") + keyText), metricString(volts.value(keyText)) + QStringLiteral(" V")));
        layout->addWidget(valueLabel(parent, id(serial, channelKey, QStringLiteral("amps_") + keyText), metricString(amps.value(keyText)) + QStringLiteral(" A")));
        layout->addWidget(valueLabel(parent, id(serial, channelKey, QStringLiteral("watts_") + keyText), metricString(watts.value(keyText)) + QStringLiteral(" W")));
        form->addRow(key == 0 || key == 1 || key == 2 ? i18n->t(nameKey, fallback) : i18n->t("txtOutput", "Rail %1").arg(key), row);
    }
}

inline void updateChannelStats(QWidget *root, const QJsonObject &channel, const QString &serial, const QString &channelKey)
{
    setTextIfChanged(root->findChild<QLabel *>(id(serial, channelKey, QStringLiteral("rpm"))),
                     QStringLiteral("%1 RPM").arg(Json::integer(channel, "rpm")));
    const QString temp = Json::str(channel, "temperatureString");
    setTextIfChanged(root->findChild<QLabel *>(id(serial, channelKey, QStringLiteral("temp"))),
                     temp.isEmpty() ? QStringLiteral("—") : temp);
    const QString power = powerOutText(channel);
    if (!power.isEmpty()) {
        setTextIfChanged(root->findChild<QLabel *>(id(serial, channelKey, QStringLiteral("power"))), power);
    }

    const QJsonObject volts = Json::object(channel, "volts");
    const QJsonObject amps = Json::object(channel, "amps");
    const QJsonObject watts = Json::object(channel, "watts");
    for (auto it = volts.begin(); it != volts.end(); ++it) {
        const QString key = it.key();
        setTextIfChanged(root->findChild<QLabel *>(id(serial, channelKey, QStringLiteral("volts_") + key)),
                         metricString(it.value()) + QStringLiteral(" V"));
        setTextIfChanged(root->findChild<QLabel *>(id(serial, channelKey, QStringLiteral("amps_") + key)),
                         metricString(amps.value(key)) + QStringLiteral(" A"));
        setTextIfChanged(root->findChild<QLabel *>(id(serial, channelKey, QStringLiteral("watts_") + key)),
                         metricString(watts.value(key)) + QStringLiteral(" W"));
    }
}

} // namespace Telemetry
