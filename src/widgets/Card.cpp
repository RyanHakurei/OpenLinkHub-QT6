#include "widgets/Card.h"

#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QStyleOptionGroupBox>

namespace {
constexpr int CardPadding = 16;
}

Card::Card(QWidget *parent)
    : QGroupBox(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    updateMargins();
}

Card::Card(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    updateMargins();
}

void Card::updateMargins()
{
    const int top = title().isEmpty() ? CardPadding : CardPadding + fontMetrics().height() + 8;
    const QMargins next(CardPadding, top, CardPadding, CardPadding);
    if (contentsMargins() != next) {
        setContentsMargins(next);
    }
}

void Card::resizeEvent(QResizeEvent *event)
{
    QGroupBox::resizeEvent(event);
    updateMargins();
}

void Card::changeEvent(QEvent *event)
{
    QGroupBox::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        updateMargins();
    }
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
    const QFontMetrics metrics(titleFont);
    const QRect titleRect(CardPadding, CardPadding, qMax(0, width() - CardPadding * 2), metrics.height());
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(option.text, Qt::ElideRight, titleRect.width()));
}
