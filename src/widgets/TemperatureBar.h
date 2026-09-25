#pragma once

#include <QWidget>

class CardGrid;
class QLabel;

class TemperatureBar : public QWidget
{
    Q_OBJECT

public:
    explicit TemperatureBar(QWidget *parent = nullptr);

    void setReadings(const QString &cpuTemperature,
                     const QString &cpuSubtitle,
                     const QList<QPair<QString, QString>> &gpus,
                     const QList<QPair<QString, QString>> &storage);

private:
    QWidget *makeCard(QLabel **title, QLabel **value, QLabel **subtitle);

    CardGrid *m_grid;
    QLabel *m_cpuTitle = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_cpuSubtitle = nullptr;
    int m_extraCount = 0;
};
