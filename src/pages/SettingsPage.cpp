#include "pages/SettingsPage.h"

#include "api/ApiClient.h"
#include "api/JsonUtil.h"
#include "app/SensorService.h"
#include "i18n/HubI18n.h"
#include "widgets/CardGrid.h"
#include "widgets/UiHelpers.h"

#include <KColorButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

SettingsPage::SettingsPage(ApiClient *client, HubI18n *i18n, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_i18n(i18n)
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    auto *grid = new CardGrid(this);
    grid->setMinimumCardWidth(280);

    auto *dash = Ui::card(m_i18n->t("txtDashboard", "Dashboard"));
    auto *dashForm = Ui::form(dash);
    m_celsius = new QCheckBox(dash);
    m_temperatureBar = new QCheckBox(dash);
    m_labels = new QCheckBox(dash);
    m_addDevice = new QCheckBox(dash);
    m_rgbOff = new QCheckBox(dash);
    m_showCpu = new QCheckBox(dash);
    m_showGpu = new QCheckBox(dash);
    m_showDisk = new QCheckBox(dash);
    m_showBattery = new QCheckBox(dash);
    m_language = new QComboBox(dash);
    m_theme = new QComboBox(dash);
    m_keyboardLayout = new QComboBox(dash);
    m_globalRgb = new QComboBox(dash);
    m_allColor = new KColorButton(Qt::red, dash);

    dashForm->addRow(m_i18n->t("txtShowTempInC", "Temperature in Celsius"), m_celsius);
    dashForm->addRow(m_i18n->t("txtShowTemperatureBar", "Show temperature bar"), m_temperatureBar);
    dashForm->addRow(m_i18n->t("txtShowDeviceLabels", "Show device labels"), m_labels);
    dashForm->addRow(m_i18n->t("txtAddDeviceToDashboard", "Add device to dashboard"), m_addDevice);
    dashForm->addRow(m_i18n->t("txtRgbOnOff", "RGB off"), m_rgbOff);
    dashForm->addRow(tr("Show CPU"), m_showCpu);
    dashForm->addRow(tr("Show GPU"), m_showGpu);
    dashForm->addRow(tr("Show storage"), m_showDisk);
    dashForm->addRow(tr("Show battery"), m_showBattery);
    dashForm->addRow(m_i18n->t("txtLanguage", "Language"), m_language);
    dashForm->addRow(m_i18n->t("txtStyle", "Style"), m_theme);
    dashForm->addRow(m_i18n->t("txtKeyboardLayout", "Keyboard layout"), m_keyboardLayout);
    dashForm->addRow(m_i18n->t("txtRgb", "RGB"), m_globalRgb);
    dashForm->addRow(m_i18n->t("txtApplyColorToAll", "Apply color to all"), m_allColor);

    auto *saveDash = new QPushButton(m_i18n->t("txtSave", "Save"), dash);
    auto *applyColor = new QPushButton(m_i18n->t("txtApplyToAll", "Apply to all"), dash);
    dashForm->addRow(saveDash, applyColor);

    connect(m_globalRgb, &QComboBox::currentIndexChanged, this, [this]() {
        const QString profile = m_globalRgb->currentData().toString();
        if (profile.isEmpty() || profile == QLatin1String("none")) {
            return;
        }
        m_client->post(QStringLiteral("/api/color/global"), QJsonObject{{QStringLiteral("profile"), profile}}, {});
    });
    connect(applyColor, &QPushButton::clicked, this, [this]() {
        m_client->post(QStringLiteral("/api/color/all"), QJsonObject{{QStringLiteral("color"), Json::colorObject(m_allColor->color())}}, {});
    });
    connect(saveDash, &QPushButton::clicked, this, &SettingsPage::saveDashboard);

    auto *backup = Ui::card(m_i18n->t("txtBackupRestore", "Backup and restore"));
    auto *backupForm = Ui::form(backup);
    auto *backupButton = new QPushButton(m_i18n->t("txtBackup", "Backup"), backup);
    auto *restoreButton = new QPushButton(m_i18n->t("txtRestore", "Restore"), backup);
    backupForm->addRow(m_i18n->t("txtBackup", "Backup"), backupButton);
    backupForm->addRow(m_i18n->t("txtRestore", "Restore"), restoreButton);
    connect(backupButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, m_i18n->t("txtBackup", "Backup"), QStringLiteral("openlinkhub-backup.zip"));
        if (path.isEmpty()) {
            return;
        }
        m_client->getBinary(QStringLiteral("/api/backup"), [path](const QByteArray &data, const QString &error, const QString &) {
            if (error.isEmpty() && !data.isEmpty()) {
                QFile file(path);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                }
            }
        });
    });
    connect(restoreButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, m_i18n->t("txtRestore", "Restore"), {}, QStringLiteral("Zip (*.zip)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"backupFile\"; filename=\"%1\"").arg(QFileInfo(path).fileName()));
        part.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/zip"));
        part.setBody(file.readAll());
        multi->append(part);
        m_client->postMultipart(QStringLiteral("/api/restore"), multi, {});
    });

    auto *scheduler = Ui::card(m_i18n->t("txtRgbScheduler", "RGB scheduler"));
    auto *schedForm = Ui::form(scheduler);
    m_rgbControl = new QCheckBox(scheduler);
    m_lcdControl = new QCheckBox(scheduler);
    m_rgbOffTime = new QTimeEdit(scheduler);
    m_rgbOnTime = new QTimeEdit(scheduler);
    m_rgbOffTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_rgbOnTime->setDisplayFormat(QStringLiteral("HH:mm"));
    auto *saveSched = new QPushButton(m_i18n->t("txtSave", "Save"), scheduler);
    schedForm->addRow(m_i18n->t("txtEnable", "Enable"), m_rgbControl);
    schedForm->addRow(m_i18n->t("txtRgbOffTime", "RGB off time"), m_rgbOffTime);
    schedForm->addRow(tr("RGB on time"), m_rgbOnTime);
    schedForm->addRow(m_i18n->t("txtLcdOffDuringDarkHours", "LCD off during dark hours"), m_lcdControl);
    schedForm->addRow(QString(), saveSched);
    connect(saveSched, &QPushButton::clicked, this, &SettingsPage::saveScheduler);

    m_sensorService = new SensorService(this);
    auto *sensors = Ui::card(tr("System Monitor sensors"));
    auto *sensorForm = Ui::form(sensors);
    m_sensorStatus = new QLabel(sensors);
    m_sensorEnabled = new QCheckBox(sensors);
    m_sensorStart = new QPushButton(tr("Start"), sensors);
    m_sensorStop = new QPushButton(tr("Stop"), sensors);
    auto *restartStats = new QPushButton(tr("Reload System Monitor sensors"), sensors);
    auto *sensorButtons = new QWidget(sensors);
    auto *sensorButtonLayout = new QHBoxLayout(sensorButtons);
    sensorButtonLayout->setContentsMargins(0, 0, 0, 0);
    sensorButtonLayout->addWidget(m_sensorStart);
    sensorButtonLayout->addWidget(m_sensorStop);
    sensorForm->addRow(tr("Collector"), m_sensorStatus);
    sensorForm->addRow(tr("Start at login"), m_sensorEnabled);
    sensorForm->addRow(sensorButtons);
    sensorForm->addRow(restartStats);
    auto *sensorHint = new QLabel(tr("Publishes fan, pump, and PSU sensors to KDE System Monitor. Install the app, then start the collector. Each rail voltage, current, and power is a separate sensor."), sensors);
    sensorHint->setWordWrap(true);
    sensorForm->addRow(sensorHint);
    connect(m_sensorStart, &QPushButton::clicked, this, [this]() {
        m_sensorService->start();
        refreshSensorService();
    });
    connect(m_sensorStop, &QPushButton::clicked, this, [this]() {
        m_sensorService->stop();
        refreshSensorService();
    });
    connect(m_sensorEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        const QSignalBlocker blocker(m_sensorEnabled);
        m_sensorService->setEnabled(enabled);
        refreshSensorService();
    });
    connect(restartStats, &QPushButton::clicked, this, [this]() {
        m_sensorService->restartSystemMonitor();
        refreshSensorService();
    });
    connect(m_sensorService, &SensorService::changed, this, &SettingsPage::refreshSensorService);
    refreshSensorService();

    auto *supported = Ui::card(m_i18n->t("txtSupportedDevices", "Supported devices"));
    auto *supportedLayout = new QVBoxLayout;
    m_supported = new QTableWidget(0, 3, supported);
    m_supported->setHorizontalHeaderLabels({m_i18n->t("txtProductId", "Product Id"), m_i18n->t("txtProductName", "Product Name"), m_i18n->t("txtEnabled", "Enabled")});
    m_supported->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_supported->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_supported->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_supported->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_supported->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_supported->setMinimumHeight(180);
    auto *saveSupported = new QPushButton(m_i18n->t("txtSave", "Save"), supported);
    supportedLayout->addWidget(m_supported);
    supportedLayout->addWidget(new QLabel(m_i18n->t("txtServiceRestartRequired", "Service restart required"), supported));
    supportedLayout->addWidget(saveSupported);
    qobject_cast<QFormLayout *>(supported->layout())->addRow(supportedLayout);
    connect(saveSupported, &QPushButton::clicked, this, &SettingsPage::saveSupported);

    grid->addCard(dash);
    grid->addCard(backup);
    grid->addCard(scheduler);
    grid->addCard(sensors);
    grid->addCard(supported);
    pageLayout->addWidget(Ui::scrollWrap(grid));
}

void SettingsPage::reload()
{
    loadDashboard();
    loadSupportedDevices();
    loadScheduler();
    if (m_sensorService) {
        m_sensorService->refresh();
    }
}

void SettingsPage::loadScheduler()
{
    const QStringList paths{
        QStringLiteral("/var/lib/openlinkhub/database/scheduler.json"),
        QStringLiteral("/opt/OpenLinkHub/database/scheduler.json"),
        QDir::homePath() + QStringLiteral("/.local/share/openlinkhub/database/scheduler.json"),
    };
    QJsonObject data;
    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        data = QJsonDocument::fromJson(file.readAll()).object();
        if (!data.isEmpty()) {
            break;
        }
    }
    if (data.isEmpty()) {
        return;
    }

    const QSignalBlocker rgbBlocker(m_rgbControl);
    const QSignalBlocker lcdBlocker(m_lcdControl);
    m_rgbControl->setChecked(Json::boolean(data, "rgbControl"));
    m_lcdControl->setChecked(Json::boolean(data, "lcdControl"));

    const QTime off = QTime::fromString(Json::str(data, "rgbOff"), QStringLiteral("HH:mm"));
    const QTime on = QTime::fromString(Json::str(data, "rgbOn"), QStringLiteral("HH:mm"));
    if (off.isValid()) {
        m_rgbOffTime->setTime(off);
    }
    if (on.isValid()) {
        m_rgbOnTime->setTime(on);
    }
}

void SettingsPage::refreshSensorService()
{
    if (!m_sensorService || !m_sensorStatus) {
        return;
    }
    const QSignalBlocker blocker(m_sensorEnabled);
    m_sensorEnabled->setChecked(m_sensorService->isEnabled());
    m_sensorStart->setEnabled(!m_sensorService->isRunning());
    m_sensorStop->setEnabled(m_sensorService->isRunning());
    m_sensorStatus->setText(m_sensorService->isRunning() ? tr("Running (%1)").arg(m_sensorService->statusText())
                                                         : tr("Stopped (%1)").arg(m_sensorService->statusText()));
}

void SettingsPage::loadDashboard()
{
    m_client->get(QStringLiteral("/api/dashboard"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonObject dash = Json::object(json, "dashboard");
        m_celsius->setChecked(Json::boolean(dash, "celsius", true));
        m_temperatureBar->setChecked(Json::boolean(dash, "temperatureBar", true));
        m_labels->setChecked(Json::boolean(dash, "showLabels", true));
        m_addDevice->setChecked(Json::boolean(dash, "addDeviceToDashboard", true));
        m_rgbOff->setChecked(Json::boolean(dash, "rgbOff"));
        m_showCpu->setChecked(Json::boolean(dash, "showCpu", true));
        m_showGpu->setChecked(Json::boolean(dash, "showGpu", true));
        m_showDisk->setChecked(Json::boolean(dash, "showDisk", true));
        m_showBattery->setChecked(Json::boolean(dash, "showBattery"));

        m_language->clear();
        const QStringList languages = {QStringLiteral("en_US"), QStringLiteral("de_DE"), QStringLiteral("fr_FR"), QStringLiteral("hr_HR"), QStringLiteral("pt_BR"), QStringLiteral("ru_RU"), QStringLiteral("se_SV")};
        for (const QString &code : languages) {
            m_language->addItem(code, code);
        }
        const int lang = m_language->findData(Json::str(dash, "languageCode", QStringLiteral("en_US")));
        if (lang >= 0) {
            m_language->setCurrentIndex(lang);
        }

        m_theme->clear();
        for (const QString &theme : Json::stringList(dash.value(QStringLiteral("themes")))) {
            m_theme->addItem(theme, theme);
        }
        const int theme = m_theme->findData(Json::str(dash, "theme", QStringLiteral("default")));
        if (theme >= 0) {
            m_theme->setCurrentIndex(theme);
        }

        m_keyboardLayout->clear();
        const QJsonObject layouts = Json::object(dash, "keyboardLayouts");
        for (auto it = layouts.begin(); it != layouts.end(); ++it) {
            m_keyboardLayout->addItem(it.value().toString(), it.key().toInt());
        }
        const int layout = m_keyboardLayout->findData(Json::integer(dash, "keyboardLayout"));
        if (layout >= 0) {
            m_keyboardLayout->setCurrentIndex(layout);
        }
    });

    m_client->get(QStringLiteral("/api/color/"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        m_globalRgb->clear();
        m_globalRgb->addItem(tr("None"), QStringLiteral("none"));
        const QJsonObject data = Json::object(json, "data");
        QSet<QString> modes;
        for (auto it = data.begin(); it != data.end(); ++it) {
            const QJsonObject device = Json::object(it.value());
            const QJsonObject profiles = Json::object(device, "profiles");
            for (auto profile = profiles.begin(); profile != profiles.end(); ++profile) {
                modes.insert(profile.key());
            }
        }
        QStringList sorted = QStringList(modes.begin(), modes.end());
        sorted.sort();
        for (const QString &mode : sorted) {
            m_globalRgb->addItem(mode, mode);
        }
    });
}

void SettingsPage::loadSupportedDevices()
{
    m_client->get(QStringLiteral("/api/getSupportedDevices"), [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        const QJsonArray data = json.value(QStringLiteral("data")).toArray();
        m_supported->setRowCount(data.size());
        int row = 0;
        for (const QJsonValue &value : data) {
            const QJsonObject device = value.toObject();
            const int productId = Json::integer(device, "ProductId");
            auto *idItem = new QTableWidgetItem(QStringLiteral("0x%1").arg(productId, 4, 16, QLatin1Char('0')).toUpper());
            idItem->setData(Qt::UserRole, productId);
            m_supported->setItem(row, 0, idItem);
            m_supported->setItem(row, 1, new QTableWidgetItem(Json::str(device, "Name")));
            auto *enabled = new QTableWidgetItem;
            enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
            enabled->setCheckState(Json::boolean(device, "Enabled") ? Qt::Checked : Qt::Unchecked);
            m_supported->setItem(row, 2, enabled);
            ++row;
        }
        m_supported->resizeColumnsToContents();
    });
}

void SettingsPage::saveDashboard()
{
    const QJsonObject body{
        {QStringLiteral("showCpu"), m_showCpu->isChecked()},
        {QStringLiteral("showGpu"), m_showGpu->isChecked()},
        {QStringLiteral("showDisk"), m_showDisk->isChecked()},
        {QStringLiteral("showLabels"), m_labels->isChecked()},
        {QStringLiteral("celsius"), m_celsius->isChecked()},
        {QStringLiteral("showBattery"), m_showBattery->isChecked()},
        {QStringLiteral("temperatureBar"), m_temperatureBar->isChecked()},
        {QStringLiteral("addDeviceToDashboard"), m_addDevice->isChecked()},
        {QStringLiteral("rgbOff"), m_rgbOff->isChecked()},
        {QStringLiteral("languageCode"), m_language->currentData().toString()},
        {QStringLiteral("theme"), m_theme->currentData().toString()},
        {QStringLiteral("keyboardLayout"), m_keyboardLayout->currentData().toInt()},
    };
    m_client->post(QStringLiteral("/api/dashboard/update"), body, [this](const QJsonObject &, const QString &) {
        Q_EMIT dashboardChanged();
    });
}

void SettingsPage::saveSupported()
{
    QJsonObject supported;
    for (int row = 0; row < m_supported->rowCount(); ++row) {
        const int id = m_supported->item(row, 0)->data(Qt::UserRole).toInt();
        const bool enabled = m_supported->item(row, 2)->checkState() == Qt::Checked;
        supported.insert(QString::number(id), enabled);
    }
    m_client->post(QStringLiteral("/api/setSupportedDevices"), QJsonObject{{QStringLiteral("supportedDevices"), supported}}, {});
}

void SettingsPage::saveScheduler()
{
    m_client->post(QStringLiteral("/api/scheduler/rgb"),
                   QJsonObject{
                       {QStringLiteral("rgbControl"), m_rgbControl->isChecked()},
                       {QStringLiteral("rgbOff"), m_rgbOffTime->time().toString(QStringLiteral("HH:mm"))},
                       {QStringLiteral("rgbOn"), m_rgbOnTime->time().toString(QStringLiteral("HH:mm"))},
                       {QStringLiteral("lcdControl"), m_lcdControl->isChecked()},
                   },
                   {});
}
