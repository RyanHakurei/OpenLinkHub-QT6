#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

class ApiClient;
class QMenu;
class QSystemTrayIcon;
class QWidget;

class TrayController : public QObject
{
    Q_OBJECT

public:
    TrayController(ApiClient *client, QWidget *window, QObject *parent = nullptr);

    bool available() const;
    void noteWindowHidden();
    void setSpeedProfiles(const QStringList &profiles);
    void setDevices(const QList<QPair<QString, QString>> &devices);

Q_SIGNALS:
    void quitRequested();
    void deviceChanged(const QString &serial);

private:
    struct DeviceState {
        QString serial;
        QString product;
        bool sidetone = false;
        bool sidetoneOn = false;
        bool hasSpeed = false;
        QString speedProfile;
        QStringList userProfiles;
        QString activeUserProfile;
    };

    void refreshDetails();
    void rebuildMenu();
    void toggleWindow();
    void setSidetone(const QString &serial, bool enabled);
    void setSpeedProfile(const QString &serial, const QString &profile);
    void setUserProfile(const QString &serial, const QString &profile);
    DeviceState parseDevice(const QString &serial, const QString &product, const QJsonObject &device) const;

    ApiClient *m_client;
    QWidget *m_window;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QStringList m_speedProfiles;
    QList<QPair<QString, QString>> m_deviceList;
    QList<DeviceState> m_details;
    int m_generation = 0;
    bool m_rebuildWhenReady = false;
};
