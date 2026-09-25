#pragma once

#include <QWidget>

// Rounded panel around the page contents, in the same role as System Monitor's
// face container. The fill stays translucent so the window blur still shows.
class ContentFrame : public QWidget
{
public:
    explicit ContentFrame(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
