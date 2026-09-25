#pragma once

#include <QWidget>

class ApiClient;
class HubI18n;
class QListWidget;
class QTableWidget;
class QLineEdit;

class MacrosPage : public QWidget
{
    Q_OBJECT

public:
    MacrosPage(ApiClient *client, HubI18n *i18n, QWidget *parent = nullptr);
    void reload();

private:
    void showMacro(int id);
    void createMacro();
    void deleteMacro();

    ApiClient *m_client;
    HubI18n *m_i18n;
    QListWidget *m_list;
    QTableWidget *m_table;
    QLineEdit *m_name;
    int m_current = -1;
};
