#pragma once

#include <QObject>
#include <QString>

class DaemonService : public QObject
{
    Q_OBJECT

public:
    explicit DaemonService(QObject *parent = nullptr);

    bool canRestart() const;
    bool isBusy() const;
    QString statusText() const;

    void refresh();
    void restart();

Q_SIGNALS:
    void changed();
    void restartFinished(bool ok, const QString &message);

private:
    struct Unit {
        QString name;
        bool user = false;
        bool found = false;
    };

    Unit locate() const;
    void restartOnBus();
    void restartWithPkexec();
    QString friendlyError(const QString &name, const QString &message) const;
    static QString friendlyState(const QString &active);

    Unit m_unit;
    bool m_busy = false;
    QString m_status;
};
