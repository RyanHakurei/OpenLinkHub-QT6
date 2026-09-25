#include "widgets/TemperatureBar.h"

#include "widgets/CardGrid.h"
#include "widgets/Telemetry.h"
#include "widgets/UiHelpers.h"

#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

TemperatureBar::TemperatureBar(QWidget *parent)
    : QWidget(parent)
    , m_grid(new CardGrid(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_grid->setMinimumCardWidth(200);
    layout->addWidget(m_grid);
    m_grid->addCard(makeCard(&m_cpuTitle, &m_cpuValue, &m_cpuSubtitle));
    m_cpuTitle->setText(tr("CPU"));
}

QWidget *TemperatureBar::makeCard(QLabel **title, QLabel **value, QLabel **subtitle)
{
    auto *box = new QGroupBox(this);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    box->setMinimumWidth(0);
    auto *layout = new QVBoxLayout(box);
    *title = new QLabel(box);
    *value = Ui::statLabel(QStringLiteral("—"), box);
    *subtitle = new QLabel(box);
    (*subtitle)->setWordWrap(true);
    layout->addWidget(*title);
    layout->addWidget(*value);
    layout->addWidget(*subtitle);
    return box;
}

void TemperatureBar::setReadings(const QString &cpuTemperature,
                                 const QString &cpuSubtitle,
                                 const QList<QPair<QString, QString>> &gpus,
                                 const QList<QPair<QString, QString>> &storage)
{
    Telemetry::setTextIfChanged(m_cpuValue, cpuTemperature.isEmpty() ? QStringLiteral("—") : cpuTemperature);
    Telemetry::setTextIfChanged(m_cpuSubtitle, cpuSubtitle);

    QList<QPair<QString, QString>> extras;
    extras.reserve(gpus.size() + storage.size());
    for (const auto &gpu : gpus) {
        extras.append({gpu.first, gpu.second});
    }
    for (const auto &disk : storage) {
        extras.append({disk.first, disk.second});
    }

    if (extras.size() != m_extraCount) {
        m_grid->takeAfter(1);
        m_extraCount = 0;
        int index = 0;
        for (const auto &extra : extras) {
            QLabel *titleLabel = nullptr;
            QLabel *valueLabel = nullptr;
            QLabel *subtitleLabel = nullptr;
            QWidget *card = makeCard(&titleLabel, &valueLabel, &subtitleLabel);
            titleLabel->setObjectName(QStringLiteral("extra_title_%1").arg(index));
            valueLabel->setObjectName(QStringLiteral("extra_value_%1").arg(index));
            subtitleLabel->setObjectName(QStringLiteral("extra_sub_%1").arg(index));
            titleLabel->setText(index < gpus.size() ? tr("GPU") : tr("Storage"));
            valueLabel->setText(extra.first);
            subtitleLabel->setText(extra.second);
            m_grid->addCard(card);
            ++index;
        }
        m_extraCount = extras.size();
        return;
    }

    for (int index = 0; index < extras.size(); ++index) {
        Telemetry::setTextIfChanged(findChild<QLabel *>(QStringLiteral("extra_value_%1").arg(index)), extras.at(index).first);
        Telemetry::setTextIfChanged(findChild<QLabel *>(QStringLiteral("extra_sub_%1").arg(index)), extras.at(index).second);
        Telemetry::setTextIfChanged(findChild<QLabel *>(QStringLiteral("extra_title_%1").arg(index)),
                                    index < gpus.size() ? tr("GPU") : tr("Storage"));
    }
}
