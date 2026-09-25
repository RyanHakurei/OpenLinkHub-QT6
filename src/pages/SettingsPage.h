#pragma once

#include <QWidget>

class ApiClient;
class HubI18n;
class QCheckBox;
class QComboBox;
class QTableWidget;
class KColorButton;
class QTimeEdit;
class QLabel;
class QPushButton;
class DaemonService;
class SensorService;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    SettingsPage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);
    void reload();

Q_SIGNALS:
    void dashboardChanged();

private:
    void loadDashboard();
    void loadSupportedDevices();
    void saveDashboard();
    void saveSupported();
    void saveScheduler();
    void loadScheduler();
    void refreshSensorService();
    void refreshDaemon();

    ApiClient *m_client;
    HubI18n *m_i18n;
    QCheckBox *m_celsius;
    QCheckBox *m_temperatureBar;
    QCheckBox *m_labels;
    QCheckBox *m_addDevice;
    QCheckBox *m_rgbOff;
    QCheckBox *m_showCpu;
    QCheckBox *m_showGpu;
    QCheckBox *m_showDisk;
    QCheckBox *m_showBattery;
    QComboBox *m_language;
    QComboBox *m_theme;
    QComboBox *m_keyboardLayout;
    QComboBox *m_globalRgb;
    KColorButton *m_allColor;
    QCheckBox *m_rgbControl;
    QCheckBox *m_lcdControl;
    QTimeEdit *m_rgbOffTime;
    QTimeEdit *m_rgbOnTime;
    QTableWidget *m_supported;
    SensorService *m_sensorService = nullptr;
    QLabel *m_sensorStatus = nullptr;
    QCheckBox *m_sensorEnabled = nullptr;
    QPushButton *m_sensorStart = nullptr;
    QPushButton *m_sensorStop = nullptr;
    DaemonService *m_daemon = nullptr;
    QLabel *m_daemonStatus = nullptr;
    QLabel *m_daemonMessage = nullptr;
    QPushButton *m_daemonRestart = nullptr;
};
