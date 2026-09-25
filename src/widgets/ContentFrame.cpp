#include "widgets/ContentFrame.h"

#include <QPainter>
#include <QPaintEvent>

ContentFrame::ContentFrame(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ContentFrame::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor fill = palette().color(QPalette::Base);
    if (fill.alpha() > 220) {
        fill.setAlpha(200);
    }
    QColor border = palette().color(QPalette::Mid);
    if (border.alpha() < 255) {
        border.setAlpha(qMax(border.alpha(), 160));
    }

    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}
