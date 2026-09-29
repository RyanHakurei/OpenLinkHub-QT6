#pragma once

#include <KXmlGuiWindow>
#include <QCloseEvent>
#include <QHash>
#include <QJsonObject>
#include <QShowEvent>
#include <QResizeEvent>

#include "app/AppSettings.h"

class ApiClient;
class HubI18n;
class IconCache;
class Sidebar;
class TemperatureBar;
class DashboardPage;
class SettingsPage;
class RgbEditorPage;
class TemperaturePage;
class MacrosPage;
class LcdPage;
class ClusterPage;
class DevicePage;
class TrayController;
class QStackedWidget;
class QTimer;
class KMessageWidget;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void present();
    bool trayAvailable() const;

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setupActions();
    void quit();
    void updateBlur();
    void applyConnectionSettings();
    void configureConnection();
    void showPage(const QString &pageId);
    void refreshAll();
    void refreshDevices();
    void refreshTelemetry();
    void refreshBattery();
    void updateConnectionUi(bool reachable);
    void updateTemperatureBar();

    AppSettings m_settings;
    ApiClient *m_client;
    HubI18n *m_i18n;
    IconCache *m_icons;
    QWidget *m_content;
    Sidebar *m_sidebar;
    TemperatureBar *m_temperatureBar;
    KMessageWidget *m_message;
    QStackedWidget *m_stack;
    DashboardPage *m_dashboard;
    SettingsPage *m_settingsPage;
    RgbEditorPage *m_rgb;
    TemperaturePage *m_temperatures;
    MacrosPage *m_macros;
    LcdPage *m_lcd;
    ClusterPage *m_cluster;
    QWidget *m_offline;
    QHash<QString, DevicePage *> m_devicePages;
    QHash<QString, int> m_stackIndex;
    QTimer *m_timer;
    TrayController *m_tray = nullptr;
    bool m_quitting = false;
    QString m_currentPage = QStringLiteral("dashboard");
    QList<QPair<QString, QString>> m_deviceChoices;
    bool m_showCpu = true;
    bool m_showGpu = true;
    bool m_showDisk = true;
    bool m_showTemperatureBar = true;
};
