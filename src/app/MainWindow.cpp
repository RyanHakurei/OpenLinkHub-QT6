#include "app/MainWindow.h"

#include "api/ApiClient.h"
#include "api/IconCache.h"
#include "api/JsonUtil.h"
#include "app/ConnectionDialog.h"
#include "app/TrayController.h"
#include "i18n/HubI18n.h"
#include "pages/ClusterPage.h"
#include "pages/DashboardPage.h"
#include "pages/DevicePage.h"
#include "pages/LcdPage.h"
#include "pages/MacrosPage.h"
#include "pages/RgbEditorPage.h"
#include "pages/SettingsPage.h"
#include "pages/TemperaturePage.h"
#include "widgets/Sidebar.h"
#include "widgets/TemperatureBar.h"
#include "widgets/UiHelpers.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KStandardAction>
#include <KWindowEffects>
#include <QAction>
#include <QIcon>
#include <QJsonArray>
#include <QLabel>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_client(new ApiClient(this))
    , m_i18n(new HubI18n(m_client, this))
    , m_icons(new IconCache(m_client, this))
    , m_timer(new QTimer(this))
{
    setWindowTitle(i18n("OpenLinkHub"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("openlinkhub-qt"), QIcon::fromTheme(QStringLiteral("input-gaming"))));
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    applyConnectionSettings();

    auto *shell = new QWidget(this);
    Ui::makeTranslucent(shell);
    auto *shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    m_sidebar = new Sidebar(shell);

    m_content = new QWidget(shell);
    Ui::makeTranslucent(m_content);
    auto *right = m_content;
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->setSpacing(12);

    m_message = new KMessageWidget(right);
    m_message->setCloseButtonVisible(true);
    m_message->setWordWrap(true);
    m_message->hide();

    m_temperatureBar = new TemperatureBar(right);

    m_stack = new QStackedWidget(right);
    Ui::makeTranslucent(m_stack);
    m_dashboard = new DashboardPage(m_client, m_i18n, m_stack);
    m_lcd = new LcdPage(m_client, m_i18n, m_stack);
    m_macros = new MacrosPage(m_client, m_i18n, m_stack);
    m_cluster = new ClusterPage(m_client, m_i18n, m_stack);
    m_rgb = new RgbEditorPage(m_client, m_i18n, m_stack);
    m_temperatures = new TemperaturePage(m_client, m_i18n, m_stack);
    m_settingsPage = new SettingsPage(m_client, m_i18n, m_stack);

    m_offline = new QWidget(m_stack);
    auto *offlineLayout = new QVBoxLayout(m_offline);
    auto *offlineLabel = new QLabel(m_offline);
    offlineLabel->setWordWrap(true);
    offlineLabel->setAlignment(Qt::AlignCenter);
    offlineLabel->setObjectName(QStringLiteral("offlineLabel"));
    offlineLayout->addStretch();
    offlineLayout->addWidget(offlineLabel);
    offlineLayout->addStretch();

    auto addPage = [this](const QString &id, QWidget *page) {
        m_stackIndex.insert(id, m_stack->addWidget(page));
    };
    addPage(QStringLiteral("dashboard"), m_dashboard);
    addPage(QStringLiteral("lcd"), m_lcd);
    addPage(QStringLiteral("macros"), m_macros);
    addPage(QStringLiteral("cluster"), m_cluster);
    addPage(QStringLiteral("rgb"), m_rgb);
    addPage(QStringLiteral("temperature"), m_temperatures);
    addPage(QStringLiteral("settings"), m_settingsPage);
    addPage(QStringLiteral("offline"), m_offline);

    rightLayout->addWidget(m_message);
    rightLayout->addWidget(m_temperatureBar);
    rightLayout->addWidget(m_stack, 1);

    shellLayout->addWidget(m_sidebar);
    shellLayout->addWidget(right, 1);
    setCentralWidget(shell);
    setMinimumSize(900, 600);

    setupActions();
    setupGUI(Keys | Save | Create, QStringLiteral(":/kxmlgui6/openlinkhub-qt/openlinkhub-qtui.rc"));
    // KXmlGui adds these to Help. This app has no handbook or What's This mode.
    const QStringList hiddenHelpActions{
        QStringLiteral("help_contents"),
        QStringLiteral("help_whats_this"),
        QStringLiteral("open_kcommand_bar"),
    };
    for (const QString &name : hiddenHelpActions) {
        if (QAction *action = actionCollection()->action(name)) {
            actionCollection()->removeAction(action);
            delete action;
        }
    }
    m_tray = new TrayController(m_client, this, this);
    connect(m_tray, &TrayController::quitRequested, this, &MainWindow::quit);
    connect(m_tray, &TrayController::deviceChanged, this, [this](const QString &serial) {
        if (m_devicePages.contains(serial)) {
            m_devicePages.value(serial)->reload();
        }
    });
    if (width() < 1100 || height() < 720) {
        resize(1280, 840);
    }

    connect(m_sidebar, &Sidebar::pageSelected, this, &MainWindow::showPage);
    connect(m_client, &ApiClient::reachableChanged, this, &MainWindow::updateConnectionUi);
    connect(m_client, &ApiClient::requestFailed, this, [this](const QString &message) {
        m_message->setText(message);
        m_message->setMessageType(KMessageWidget::Warning);
        m_message->animatedShow();
    });
    connect(m_icons, &IconCache::iconReady, this, [this](const QString &, const QIcon &) {
        refreshDevices();
    });
    connect(m_settingsPage, &SettingsPage::dashboardChanged, this, &MainWindow::refreshAll);
    connect(m_temperatures, &TemperaturePage::profilesChanged, this, [this]() {
        const QStringList profiles = m_temperatures->visibleProfiles();
        m_tray->setSpeedProfiles(profiles);
        for (DevicePage *page : std::as_const(m_devicePages)) {
            page->setSpeedProfiles(profiles);
        }
    });
    connect(m_i18n, &HubI18n::changed, this, [this]() {
        m_sidebar->setTitle(i18n("OpenLinkHub"));
        setWindowTitle(i18n("OpenLinkHub"));
        m_sidebar->setToolLabels(m_i18n->t("txtLcd", "LCD"),
                                 m_i18n->t("txtMacros", "Macros"),
                                 m_i18n->t("txtRgbCluster", "RGB Cluster"),
                                 m_i18n->t("txtRgbEditor", "RGB Editor"),
                                 m_i18n->t("txtProfiles", "Temperature Profiles"),
                                 m_i18n->t("txtSettings", "Settings"));
    });

    m_timer->setInterval(m_settings.pollIntervalMs());
    connect(m_timer, &QTimer::timeout, this, [this]() {
        refreshTelemetry();
        refreshBattery();
        if (m_currentPage.startsWith(QLatin1String("device:"))) {
            const QString serial = m_currentPage.mid(7);
            if (m_devicePages.contains(serial)) {
                m_devicePages.value(serial)->refreshTelemetry();
            }
        } else if (m_currentPage == QLatin1String("dashboard")) {
            m_dashboard->refreshTelemetry();
        }
        refreshDevices();
    });
    m_timer->start();

    refreshAll();
}

MainWindow::~MainWindow() = default;

void MainWindow::showEvent(QShowEvent *event)
{
    KXmlGuiWindow::showEvent(event);
    updateBlur();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    KXmlGuiWindow::resizeEvent(event);
    updateBlur();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_quitting && m_tray && m_tray->available()) {
        hide();
        event->ignore();
        return;
    }
    KXmlGuiWindow::closeEvent(event);
}

void MainWindow::quit()
{
    m_quitting = true;
    qApp->quit();
}

void MainWindow::updateBlur()
{
    QWindow *window = windowHandle();
    if (!window || !m_sidebar) {
        return;
    }
    if (!KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind)) {
        return;
    }
    KWindowEffects::enableBlurBehind(window, true);
}

void MainWindow::setupActions()
{
    KStandardAction::quit(this, &MainWindow::quit, actionCollection());
    KStandardAction::preferences(this, &MainWindow::configureConnection, actionCollection());

    auto *refresh = actionCollection()->addAction(QStringLiteral("file_refresh"), this, &MainWindow::refreshAll);
    refresh->setText(i18n("Refresh"));
    refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    actionCollection()->setDefaultShortcut(refresh, QKeySequence::Refresh);
}

void MainWindow::applyConnectionSettings()
{
    m_client->setBaseUrl(m_settings.baseUrl());
    if (m_timer) {
        m_timer->setInterval(m_settings.pollIntervalMs());
    }
}

void MainWindow::configureConnection()
{
    ConnectionDialog dialog(&m_settings, this);
    if (dialog.exec() == QDialog::Accepted) {
        applyConnectionSettings();
        refreshAll();
    }
}

void MainWindow::showPage(const QString &pageId)
{
    m_currentPage = pageId;
    updateTemperatureBar();
    if (pageId.startsWith(QLatin1String("device:"))) {
        const QString serial = pageId.mid(7);
        if (!m_devicePages.contains(serial)) {
            auto *page = new DevicePage(m_client, m_i18n, serial, m_stack);
            page->setSpeedProfiles(m_temperatures->visibleProfiles());
            m_devicePages.insert(serial, page);
            m_stackIndex.insert(pageId, m_stack->addWidget(page));
            page->reload();
        } else {
            m_devicePages.value(serial)->refreshTelemetry();
        }
    } else if (pageId == QLatin1String("dashboard")) {
        m_dashboard->reload();
    } else if (pageId == QLatin1String("settings")) {
        m_settingsPage->reload();
    } else if (pageId == QLatin1String("rgb")) {
        m_rgb->reload();
    } else if (pageId == QLatin1String("temperature")) {
        m_temperatures->reload();
    } else if (pageId == QLatin1String("macros")) {
        m_macros->reload();
    } else if (pageId == QLatin1String("lcd")) {
        m_lcd->reload();
    } else if (pageId == QLatin1String("cluster")) {
        m_cluster->reload();
    }

    if (m_stackIndex.contains(pageId)) {
        m_stack->setCurrentIndex(m_stackIndex.value(pageId));
    }
}

void MainWindow::refreshAll()
{
    m_i18n->reload();
    m_temperatures->reload();
    m_settingsPage->reload();
    refreshDevices();
    refreshTelemetry();
    refreshBattery();
    m_dashboard->reload();
    if (m_currentPage.startsWith(QLatin1String("device:"))) {
        const QString serial = m_currentPage.mid(7);
        if (m_devicePages.contains(serial)) {
            m_devicePages.value(serial)->reload();
        }
    }
}

void MainWindow::refreshDevices()
{
    m_client->get(QStringLiteral("/api/"), [this](const QJsonObject &overview, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        m_client->get(QStringLiteral("/api/devices/"), [this, overview](const QJsonObject &details, const QString &error) {
            if (!error.isEmpty()) {
                return;
            }
            const QJsonObject overviewDevices = Json::object(overview, "device");
            const QJsonObject detailed = Json::object(details, "devices");
            QList<SidebarDevice> sidebar;
            m_deviceChoices.clear();
            for (auto it = detailed.begin(); it != detailed.end(); ++it) {
                const QJsonObject wrapper = Json::object(it.value());
                if (Json::boolean(wrapper, "Hidden")) {
                    continue;
                }
                const QString serial = Json::strAny(wrapper, {QStringLiteral("Serial"), QStringLiteral("serial")}, it.key());
                if (serial.isEmpty() || serial == QLatin1String("cluster")) {
                    continue;
                }
                const QString product = Json::strAny(wrapper, {QStringLiteral("Product"), QStringLiteral("product")}, serial);
                const QJsonObject summary = Json::object(overviewDevices.value(serial));
                const QString image = Json::strAny(wrapper, {QStringLiteral("Image")}, Json::str(summary, "Image"));
                SidebarDevice device;
                device.serial = serial;
                device.product = product;
                device.image = image;
                device.icon = m_icons->iconFor(product, image);
                sidebar.append(device);
                m_deviceChoices.append({serial, product});
            }
            m_sidebar->setDevices(sidebar);
            m_dashboard->setDeviceChoices(m_deviceChoices);
            m_tray->setDevices(m_deviceChoices);
        });
    });

    m_client->get(QStringLiteral("/api/dashboard"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject dash = Json::object(json, "dashboard");
        m_showCpu = Json::boolean(dash, "showCpu", true);
        m_showGpu = Json::boolean(dash, "showGpu", true);
        m_showDisk = Json::boolean(dash, "showDisk", true);
        m_showTemperatureBar = Json::boolean(dash, "temperatureBar", true);
        updateTemperatureBar();
        m_dashboard->setShowLabels(Json::boolean(dash, "showLabels", true));
        m_sidebar->setTitle(i18n("OpenLinkHub"));
        setWindowTitle(i18n("OpenLinkHub"));
    });
}

void MainWindow::refreshTelemetry()
{
    m_client->get(QStringLiteral("/api/cpuTemp"), [this](const QJsonObject &cpuJson, const QString &) {
        m_client->get(QStringLiteral("/api/gpuTemps"), [this, cpuJson](const QJsonObject &gpuJson, const QString &) {
            m_client->get(QStringLiteral("/api/storageTemp"), [this, cpuJson, gpuJson](const QJsonObject &storageJson, const QString &) {
                const QString cpu = m_showCpu ? cpuJson.value(QStringLiteral("data")).toString() : QString();
                QList<QPair<QString, QString>> gpus;
                if (m_showGpu) {
                    const QJsonValue data = gpuJson.value(QStringLiteral("data"));
                    if (data.isObject()) {
                        const QJsonObject object = data.toObject();
                        for (auto it = object.begin(); it != object.end(); ++it) {
                            gpus.append({it.value().toString(), tr("GPU %1").arg(it.key())});
                        }
                    } else if (data.isString()) {
                        gpus.append({data.toString(), tr("GPU")});
                    }
                }
                QList<QPair<QString, QString>> storage;
                if (m_showDisk) {
                    const QJsonValue data = storageJson.value(QStringLiteral("data"));
                    if (data.isArray()) {
                        for (const QJsonValue &value : data.toArray()) {
                            const QJsonObject disk = value.toObject();
                            storage.append({Json::str(disk, "TemperatureString"), Json::str(disk, "Model")});
                        }
                    }
                }
                m_temperatureBar->setReadings(cpu.isEmpty() ? QStringLiteral("—") : cpu, tr("CPU"), gpus, storage);
            });
        });
    });
}

void MainWindow::refreshBattery()
{
    m_client->get(QStringLiteral("/api/batteryStats"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject data = Json::object(json, "data");
        for (auto it = data.begin(); it != data.end(); ++it) {
            const QJsonObject battery = Json::object(it.value());
            m_sidebar->setBattery(it.key(), Json::integer(battery, "Level", -1));
        }
    });
}

void MainWindow::updateTemperatureBar()
{
    m_temperatureBar->setVisible(m_showTemperatureBar && m_currentPage == QLatin1String("dashboard"));
}

void MainWindow::updateConnectionUi(bool reachable)
{
    if (reachable) {
        m_sidebar->setStatus(i18n("Connected to %1", m_client->baseUrl().toString()));
        if (m_stack->currentWidget() == m_offline) {
            showPage(m_currentPage == QLatin1String("offline") ? QStringLiteral("dashboard") : m_currentPage);
        }
        m_message->animatedHide();
    } else {
        m_sidebar->setStatus(i18n("Disconnected"));
        if (auto *label = m_offline->findChild<QLabel *>(QStringLiteral("offlineLabel"))) {
            label->setText(i18n("Cannot connect to OpenLinkHub at %1.\nStart the OpenLinkHub service, then press Refresh.\nUse Settings to change the host and port.", m_client->baseUrl().toString()));
        }
        m_stack->setCurrentIndex(m_stackIndex.value(QStringLiteral("offline")));
    }
}
