#include "widgets/Sidebar.h"

#include "widgets/UiHelpers.h"

#include <KSeparator>
#include <QHash>
#include <QLabel>
#include <QPalette>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int PageIdRole = Qt::UserRole;
constexpr int SerialRole = Qt::UserRole + 1;

QString deviceLabel(const SidebarDevice &device)
{
    QString label = device.product;
    if (device.battery >= 0) {
        label += QStringLiteral("  (%1%)").arg(device.battery);
    }
    return label;
}
}

void Sidebar::makeTranslucent(QWidget *widget)
{
    Ui::makeTranslucent(widget);
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Base, Qt::transparent);
    widget->setPalette(palette);
}

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    Ui::makeTranslucent(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 12, 8, 12);
    layout->setSpacing(8);

    m_title = new QLabel(tr("OpenLinkHub"), this);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    m_nav = new QListWidget(this);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setIconSize(QSize(22, 22));
    m_nav->setSpacing(2);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setUniformItemSizes(false);
    makeTranslucent(m_nav);
    makeTranslucent(m_nav->viewport());

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    QFont statusFont = m_status->font();
    statusFont.setPointSize(qMax(8, statusFont.pointSize() - 1));
    m_status->setFont(statusFont);

    layout->addWidget(m_title);
    layout->addWidget(m_nav, 1);
    layout->addWidget(m_status);

    setToolLabels(tr("LCD"), tr("Macros"), tr("RGB Cluster"), tr("RGB Editor"), tr("Temperature Profiles"), tr("Settings"));

    connect(m_nav, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (m_updating || !item) {
            return;
        }
        const QString id = item->data(PageIdRole).toString();
        if (!id.isEmpty()) {
            Q_EMIT pageSelected(id);
        }
    });
}

void Sidebar::setTitle(const QString &title)
{
    m_title->setText(title);
}

void Sidebar::setToolLabels(const QString &lcd, const QString &macros, const QString &cluster, const QString &rgb, const QString &temperature, const QString &settings)
{
    m_tools = {
        {QStringLiteral("lcd"), lcd, QStringLiteral("video-display")},
        {QStringLiteral("macros"), macros, QStringLiteral("input-keyboard")},
        {QStringLiteral("cluster"), cluster, QStringLiteral("folder-colors")},
        {QStringLiteral("rgb"), rgb, QStringLiteral("color-management")},
        {QStringLiteral("temperature"), temperature, QStringLiteral("temperature-normal")},
        {QStringLiteral("settings"), settings, QStringLiteral("settings-configure")},
    };

    if (m_nav->count() == 0) {
        rebuild({});
        return;
    }

    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem *item = m_nav->item(i);
        const QString id = item->data(PageIdRole).toString();
        for (const ToolItem &tool : std::as_const(m_tools)) {
            if (tool.id == id) {
                item->setText(tool.label);
                break;
            }
        }
    }
}

void Sidebar::addSeparator()
{
    auto *item = new QListWidgetItem;
    item->setFlags(Qt::NoItemFlags);
    item->setSizeHint(QSize(1, 18));
    m_nav->addItem(item);

    auto *host = new QWidget(m_nav);
    makeTranslucent(host);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(0);
    layout->addWidget(new KSeparator(Qt::Horizontal, host));
    m_nav->setItemWidget(item, host);
}

void Sidebar::addToolItems()
{
    addSeparator();

    for (const ToolItem &tool : std::as_const(m_tools)) {
        auto *item = new QListWidgetItem(QIcon::fromTheme(tool.icon), tool.label);
        item->setData(PageIdRole, tool.id);
        m_nav->addItem(item);
    }
}

void Sidebar::rebuild(const QList<SidebarDevice> &devices)
{
    const QString current = currentPageId();
    m_updating = true;
    m_nav->clear();

    auto *dashboard = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("view-dashboard"), QIcon::fromTheme(QStringLiteral("user-desktop"))), tr("Dashboard"));
    dashboard->setData(PageIdRole, QStringLiteral("dashboard"));
    m_nav->addItem(dashboard);

    for (const SidebarDevice &device : devices) {
        auto *item = new QListWidgetItem(device.icon, deviceLabel(device));
        item->setData(PageIdRole, QString(QStringLiteral("device:") + device.serial));
        item->setData(SerialRole, device.serial);
        item->setToolTip(device.serial);
        m_nav->addItem(item);
    }

    addToolItems();
    m_updating = false;

    if (!current.isEmpty()) {
        selectPage(current);
    } else if (m_nav->count() > 0) {
        m_nav->setCurrentRow(0);
    }
}

void Sidebar::setDevices(const QList<SidebarDevice> &devices)
{
    QStringList serials;
    serials.reserve(devices.size());
    for (const SidebarDevice &device : devices) {
        serials.append(device.serial);
    }

    QHash<QString, int> batteries;
    for (const SidebarDevice &existing : std::as_const(m_devices)) {
        if (existing.battery >= 0) {
            batteries.insert(existing.serial, existing.battery);
        }
    }
    QList<SidebarDevice> merged = devices;
    for (SidebarDevice &device : merged) {
        if (device.battery < 0 && batteries.contains(device.serial)) {
            device.battery = batteries.value(device.serial);
        }
    }

    if (serials == m_serials && m_nav->count() > 0) {
        for (int i = 0; i < merged.size(); ++i) {
            QListWidgetItem *item = m_nav->item(i + 1);
            if (!item || item->data(SerialRole).toString() != merged.at(i).serial) {
                rebuild(merged);
                m_serials = serials;
                m_devices = merged;
                return;
            }
            item->setIcon(merged.at(i).icon);
            const QString label = deviceLabel(merged.at(i));
            if (item->text() != label) {
                item->setText(label);
            }
        }
        m_devices = merged;
        return;
    }

    m_serials = serials;
    m_devices = merged;
    rebuild(merged);
}

void Sidebar::setStatus(const QString &status)
{
    m_status->setText(status);
}

void Sidebar::updateDeviceIcon(const QString &serial, const QIcon &icon)
{
    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem *item = m_nav->item(i);
        if (item->data(SerialRole).toString() == serial) {
            item->setIcon(icon);
            return;
        }
    }
}

void Sidebar::setBattery(const QString &serial, int level)
{
    QString product;
    for (SidebarDevice &device : m_devices) {
        if (device.serial == serial) {
            device.battery = level;
            product = device.product;
            break;
        }
    }
    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem *item = m_nav->item(i);
        if (item->data(SerialRole).toString() != serial) {
            continue;
        }
        SidebarDevice current;
        current.product = product;
        if (current.product.isEmpty()) {
            current.product = item->text();
            const int cut = current.product.lastIndexOf(QStringLiteral("  ("));
            if (cut > 0) {
                current.product = current.product.left(cut);
            }
        }
        current.battery = level;
        const QString label = deviceLabel(current);
        if (item->text() != label) {
            item->setText(label);
        }
        return;
    }
}

QString Sidebar::currentPageId() const
{
    if (!m_nav->currentItem()) {
        return {};
    }
    return m_nav->currentItem()->data(PageIdRole).toString();
}

void Sidebar::selectPage(const QString &pageId)
{
    if (QListWidgetItem *item = findItem(pageId)) {
        m_updating = true;
        m_nav->setCurrentItem(item);
        m_updating = false;
    }
}

QListWidgetItem *Sidebar::findItem(const QString &pageId) const
{
    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem *item = m_nav->item(i);
        if (item->data(PageIdRole).toString() == pageId) {
            return item;
        }
    }
    return nullptr;
}
