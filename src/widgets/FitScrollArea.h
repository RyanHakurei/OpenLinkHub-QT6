#pragma once

#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>

class FitScrollArea : public QScrollArea
{
public:
    explicit FitScrollArea(QWidget *parent = nullptr)
        : QScrollArea(parent)
    {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAutoFillBackground(false);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        setPalette(palette);
        viewport()->setPalette(palette);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QScrollArea::resizeEvent(event);
        if (QWidget *inner = widget()) {
            inner->setFixedWidth(viewport()->width());
        }
    }
};
