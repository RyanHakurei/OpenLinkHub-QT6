#include "widgets/Sidebar.h"

#include "widgets/UiHelpers.h"

#include <KConfigGroup>
#include <KSeparator>
#include <KSharedConfig>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QListWidget>
#include <QListWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int PageIdRole = Qt::UserRole;
constexpr int SerialRole = Qt::UserRole + 1;
constexpr int LabelRole = Qt::UserRole + 2;
constexpr int TipRole = Qt::UserRole + 3;
constexpr int ExpandedWidth = 260;
constexpr int CollapsedWidth = 56;

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

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 12, 8, 12);
    m_layout->setSpacing(8);

    m_title = new QLabel(tr("OpenLinkHub"), this);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    m_collapse = new QToolButton(this);
    m_collapse->setAutoRaise(true);
    m_collapse->setIconSize(QSize(16, 16));
    m_collapse->setToolTip(tr("Collapse sidebar"));
    connect(m_collapse, &QToolButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
    });

    m_header = new QHBoxLayout;
    m_header->setContentsMargins(0, 0, 0, 0);
    m_header->addWidget(m_title, 1);
    m_header->addWidget(m_collapse);

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

    m_layout->addLayout(m_header);
    m_layout->addWidget(m_nav, 1);
    m_layout->addWidget(m_status);

    const KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("Interface"));
    m_collapsed = group.readEntry(QStringLiteral("SidebarCollapsed"), false);
    applyChrome();

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
                setItemLabel(item, tool.label);
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
        auto *item = new QListWidgetItem(QIcon::fromTheme(tool.icon), QString());
        item->setData(PageIdRole, tool.id);
        setItemLabel(item, tool.label);
        m_nav->addItem(item);
    }
}

void Sidebar::rebuild(const QList<SidebarDevice> &devices)
{
    const QString current = currentPageId();
    m_updating = true;
    m_nav->clear();

    auto *dashboard = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("view-dashboard"), QIcon::fromTheme(QStringLiteral("user-desktop"))), QString());
    dashboard->setData(PageIdRole, QStringLiteral("dashboard"));
    setItemLabel(dashboard, tr("Dashboard"));
    m_nav->addItem(dashboard);

    for (const SidebarDevice &device : devices) {
        auto *item = new QListWidgetItem(device.icon, QString());
        item->setData(PageIdRole, QString(QStringLiteral("device:") + device.serial));
        item->setData(SerialRole, device.serial);
        setItemLabel(item, deviceLabel(device), device.serial);
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
            setItemLabel(item, deviceLabel(merged.at(i)), merged.at(i).serial);
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
        setItemLabel(item, deviceLabel(current), serial);
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

void Sidebar::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) {
        return;
    }
    m_collapsed = collapsed;
    applyChrome();
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("Interface"));
    group.writeEntry(QStringLiteral("SidebarCollapsed"), m_collapsed);
    group.sync();
}

void Sidebar::setItemLabel(QListWidgetItem *item, const QString &label, const QString &expandedTip)
{
    item->setData(LabelRole, label);
    item->setData(TipRole, expandedTip);
    applyItemLabel(item);
}

void Sidebar::applyItemLabel(QListWidgetItem *item)
{
    const QString label = item->data(LabelRole).toString();
    if (label.isEmpty()) {
        return;
    }
    if (m_collapsed) {
        item->setText(QString());
        item->setToolTip(label);
        return;
    }
    item->setText(label);
    item->setToolTip(item->data(TipRole).toString());
}

void Sidebar::applyChrome()
{
    m_title->setVisible(!m_collapsed);
    m_status->setVisible(!m_collapsed);
    m_header->setAlignment(m_collapsed ? Qt::AlignHCenter : Qt::AlignVCenter);
    setFixedWidth(m_collapsed ? CollapsedWidth : ExpandedWidth);
    m_layout->setContentsMargins(m_collapsed ? 4 : 8, 12, m_collapsed ? 4 : 8, 12);
    m_collapse->setIcon(QIcon::fromTheme(m_collapsed ? QStringLiteral("sidebar-expand") : QStringLiteral("sidebar-collapse")));
    m_collapse->setToolTip(m_collapsed ? tr("Expand sidebar") : tr("Collapse sidebar"));
    for (int i = 0; i < m_nav->count(); ++i) {
        applyItemLabel(m_nav->item(i));
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
