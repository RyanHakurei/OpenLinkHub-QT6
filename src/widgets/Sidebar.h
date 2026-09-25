#pragma once

#include <QIcon>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QLabel;

struct SidebarDevice {
    QString serial;
    QString product;
    QString image;
    QIcon icon;
    int battery = -1;
};

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setDevices(const QList<SidebarDevice> &devices);
    void setToolLabels(const QString &lcd, const QString &macros, const QString &cluster, const QString &rgb, const QString &temperature, const QString &settings);
    void setStatus(const QString &status);
    void updateDeviceIcon(const QString &serial, const QIcon &icon);
    void setBattery(const QString &serial, int level);
    QString currentPageId() const;
    void selectPage(const QString &pageId);

Q_SIGNALS:
    void pageSelected(const QString &pageId);

private:
    struct ToolItem {
        QString id;
        QString label;
        QString icon;
    };

    QListWidgetItem *findItem(const QString &pageId) const;
    void rebuild(const QList<SidebarDevice> &devices);
    void addSeparator();
    void addToolItems();
    void makeTranslucent(QWidget *widget);

    QLabel *m_title;
    QListWidget *m_nav;
    QLabel *m_status;
    bool m_updating = false;
    QStringList m_serials;
    QList<SidebarDevice> m_devices;
    QList<ToolItem> m_tools;
};
