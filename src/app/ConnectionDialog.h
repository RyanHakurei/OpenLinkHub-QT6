#pragma once

#include <QDialog>

class AppSettings;
class QLineEdit;
class QSpinBox;
class QCheckBox;

class ConnectionDialog : public QDialog
{
    Q_OBJECT

public:
    ConnectionDialog(AppSettings *settings, QWidget *parent = nullptr);

private:
    void save();

    AppSettings *m_settings;
    QLineEdit *m_host;
    QSpinBox *m_port;
    QSpinBox *m_poll;
    QCheckBox *m_https;
};
