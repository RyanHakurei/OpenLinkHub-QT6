#pragma once

#include <QGroupBox>

// One rounded frame per device or settings group, instead of a frame around
// the whole page. The fill stays translucent so window blur can show through.
class Card : public QGroupBox
{
public:
    explicit Card(QWidget *parent = nullptr);
    explicit Card(const QString &title, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
