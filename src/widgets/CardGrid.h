#pragma once

#include <QList>
#include <QWidget>

class QGridLayout;

class CardGrid : public QWidget
{
    Q_OBJECT

public:
    explicit CardGrid(QWidget *parent = nullptr);

    void clear();
    void takeAfter(int keep);
    void addCard(QWidget *card, int columnSpan = 1);
    void setMinimumCardWidth(int width);
    int count() const;

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    int columnsForWidth(int width) const;
    int rowsForWidth(int width) const;
    int rowHeight() const;
    void relayout();

    QGridLayout *m_layout;
    QList<QWidget *> m_cards;
    QList<int> m_spans;
    int m_minCardWidth = 260;
    int m_columns = 0;
};
