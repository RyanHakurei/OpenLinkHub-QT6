#pragma once

#include <QObject>
#include <QString>

class SensorService : public QObject
{
    Q_OBJECT

public:
    explicit SensorService(QObject *parent = nullptr);

    bool isRunning() const;
    bool isEnabled() const;
    QString statusText() const;

    bool start();
    bool stop();
    bool setEnabled(bool enabled);
    bool restartSystemMonitor();
    void refresh();

Q_SIGNALS:
    void changed();

private:
    bool call(const QString &method, const QList<QVariant> &args = {});

    bool m_running = false;
    bool m_enabled = false;
    QString m_status = QStringLiteral("unknown");
};
