#include "widgets/Card.h"

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionGroupBox>

Card::Card(QWidget *parent)
    : QGroupBox(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

Card::Card(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

void Card::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QStyleOptionGroupBox option;
    initStyleOption(&option);

    QColor fill = palette().color(QPalette::Base);
    if (fill.alpha() > 220) {
        fill.setAlpha(190);
    }
    QColor border = palette().color(QPalette::Mid);
    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    if (option.text.isEmpty()) {
        return;
    }
    painter.setPen(palette().color(QPalette::WindowText));
    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    const QRect labelRect = style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxLabel, this);
    painter.drawText(labelRect.isValid() ? labelRect : QRect(12, 4, width() - 24, fontMetrics().height() + 4),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     option.text);
}
