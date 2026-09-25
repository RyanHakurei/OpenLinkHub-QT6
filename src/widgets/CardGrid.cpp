#include "widgets/CardGrid.h"

#include <QGridLayout>
#include <QResizeEvent>

CardGrid::CardGrid(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QGridLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(12);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

void CardGrid::setMinimumCardWidth(int width)
{
    m_minCardWidth = qMax(160, width);
    m_columns = 0;
    relayout();
}

int CardGrid::count() const
{
    return m_cards.size();
}

int CardGrid::columnsForWidth(int width) const
{
    const int spacing = m_layout->spacing();
    const int usable = qMax(1, width);
    return qMax(1, (usable + spacing) / (m_minCardWidth + spacing));
}

int CardGrid::rowHeight() const
{
    int height = 0;
    for (QWidget *card : m_cards) {
        height = qMax(height, card->sizeHint().height());
    }
    if (height <= 0) {
        height = 120;
    }
    return height;
}

bool CardGrid::hasHeightForWidth() const
{
    return true;
}

int CardGrid::heightForWidth(int width) const
{
    if (m_cards.isEmpty()) {
        return 0;
    }
    const int columns = columnsForWidth(width);
    const int rows = (m_cards.size() + columns - 1) / columns;
    return rows * rowHeight() + (rows - 1) * m_layout->spacing();
}

QSize CardGrid::sizeHint() const
{
    const int width = qMax(m_minCardWidth, this->width());
    return QSize(width, heightForWidth(width));
}

QSize CardGrid::minimumSizeHint() const
{
    return QSize(160, m_cards.isEmpty() ? 0 : rowHeight());
}

void CardGrid::clear()
{
    takeAfter(0);
}

void CardGrid::takeAfter(int keep)
{
    keep = qBound(0, keep, m_cards.size());
    while (m_cards.size() > keep) {
        QWidget *card = m_cards.takeLast();
        m_layout->removeWidget(card);
        card->deleteLater();
    }
    m_columns = 0;
    relayout();
}

void CardGrid::addCard(QWidget *card)
{
    card->setParent(this);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    card->setMinimumWidth(0);
    m_cards.append(card);
    m_columns = 0;
    relayout();
}

void CardGrid::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void CardGrid::relayout()
{
    const int columns = columnsForWidth(qMax(1, width()));
    if (columns == m_columns && m_layout->count() == m_cards.size()) {
        return;
    }
    m_columns = columns;

    for (QWidget *card : std::as_const(m_cards)) {
        m_layout->removeWidget(card);
    }

    constexpr int maxTrackedColumns = 16;
    for (int column = 0; column < maxTrackedColumns; ++column) {
        m_layout->setColumnStretch(column, column < columns ? 1 : 0);
        m_layout->setColumnMinimumWidth(column, 0);
    }

    int index = 0;
    for (QWidget *card : std::as_const(m_cards)) {
        m_layout->addWidget(card, index / columns, index % columns);
        ++index;
    }
    updateGeometry();
}
