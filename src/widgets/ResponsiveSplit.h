#pragma once

#include <QGridLayout>
#include <QResizeEvent>
#include <QWidget>

class ResponsiveSplit : public QWidget
{
public:
    ResponsiveSplit(QWidget *side, QWidget *main, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_side(side)
        , m_main(main)
        , m_layout(new QGridLayout(this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(12);
        m_side->setParent(this);
        m_main->setParent(this);
        apply(true);
    }

    void setBreakpoint(int width)
    {
        m_breakpoint = qMax(400, width);
    }

    void setSideWidth(int width)
    {
        m_sideWidth = qMax(120, width);
        if (m_horizontal) {
            m_side->setMaximumWidth(m_sideWidth);
        }
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        apply(width() >= m_breakpoint);
    }

private:
    void apply(bool horizontal)
    {
        if (horizontal == m_horizontal && m_layout->count() == 2) {
            return;
        }
        m_horizontal = horizontal;
        m_layout->removeWidget(m_side);
        m_layout->removeWidget(m_main);

        if (horizontal) {
            m_side->setMaximumWidth(m_sideWidth);
            m_side->setMaximumHeight(QWIDGETSIZE_MAX);
            m_side->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            m_main->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
            m_layout->addWidget(m_side, 0, 0);
            m_layout->addWidget(m_main, 0, 1);
            m_layout->setColumnStretch(0, 0);
            m_layout->setColumnStretch(1, 1);
            m_layout->setRowStretch(0, 1);
            m_layout->setRowStretch(1, 0);
        } else {
            m_side->setMaximumWidth(QWIDGETSIZE_MAX);
            m_side->setMaximumHeight(QWIDGETSIZE_MAX);
            m_side->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            m_main->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
            m_layout->addWidget(m_side, 0, 0);
            m_layout->addWidget(m_main, 1, 0);
            m_layout->setColumnStretch(0, 1);
            m_layout->setColumnStretch(1, 0);
            m_layout->setRowStretch(0, 0);
            m_layout->setRowStretch(1, 1);
        }
    }

    QWidget *m_side;
    QWidget *m_main;
    QGridLayout *m_layout;
    int m_breakpoint = 780;
    int m_sideWidth = 240;
    bool m_horizontal = true;
};
