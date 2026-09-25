#include "app/ConnectionDialog.h"

#include "app/AppSettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(AppSettings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Connect to OpenLinkHub"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_host = new QLineEdit(settings->host(), this);
    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(settings->port());
    m_poll = new QSpinBox(this);
    m_poll->setRange(500, 60000);
    m_poll->setSingleStep(500);
    m_poll->setSuffix(tr(" ms"));
    m_poll->setValue(settings->pollIntervalMs());
    m_https = new QCheckBox(tr("Use HTTPS"), this);
    m_https->setChecked(settings->https());

    form->addRow(tr("Host"), m_host);
    form->addRow(tr("Port"), m_port);
    form->addRow(tr("Refresh interval"), m_poll);
    form->addRow(QString(), m_https);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ConnectionDialog::save()
{
    m_settings->setHost(m_host->text().trimmed());
    m_settings->setPort(m_port->value());
    m_settings->setPollIntervalMs(m_poll->value());
    m_settings->setHttps(m_https->isChecked());
    m_settings->save();
    accept();
}
