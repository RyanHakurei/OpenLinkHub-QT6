#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

class ApiClient;
class QAction;
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
    void toggleWindow();
    void toggleLights();
    void toggleSidetone();

Q_SIGNALS:
    void quitRequested();
    void deviceChanged(const QString &serial);

private:
    struct ChannelState {
        int id = 0;
        QString profile;
        bool hasSpeed = false;
        bool psu = false;
    };

    struct DeviceState {
        QString serial;
        QString product;
        bool sidetone = false;
        bool sidetoneOn = false;
        bool hasSpeed = false;
        QString speedProfile;
        QStringList userProfiles;
        QString activeUserProfile;
        QList<ChannelState> channels;
    };

    void refreshDetails();
    void refreshLights();
    void setLightsOff(bool off);
    void rebuildMenu();
    void setSidetone(const QString &serial, bool enabled);
    void setSpeedProfile(const QString &serial, const QString &profile);
    void setUserProfile(const QString &serial, const QString &profile);
    void holdDevice(const QString &serial, int percent);
    void releaseDevice(const QString &serial);
    DeviceState parseDevice(const QString &serial, const QString &product, const QJsonObject &device) const;

    ApiClient *m_client;
    QWidget *m_window;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_lightsAction = nullptr;
    bool m_lightsOff = false;
    bool m_haveDashboard = false;
    QJsonObject m_dashboard;
    QStringList m_speedProfiles;
    QList<QPair<QString, QString>> m_deviceList;
    QList<DeviceState> m_details;
    int m_generation = 0;
    bool m_rebuildWhenReady = false;
};
