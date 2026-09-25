#pragma once

#include "widgets/FitScrollArea.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>
#include <QWidget>

namespace Ui {

inline void makeTranslucent(QWidget *widget)
{
    if (!widget) {
        return;
    }
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setAutoFillBackground(false);
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Base, Qt::transparent);
    widget->setPalette(palette);
}

inline QGroupBox *card(const QString &title, QWidget *parent = nullptr)
{
    auto *box = new QGroupBox(title, parent);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    box->setMinimumWidth(0);
    auto *layout = new QFormLayout(box);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->setContentsMargins(12, 12, 12, 12);
    return box;
}

inline QFormLayout *form(QGroupBox *box)
{
    return qobject_cast<QFormLayout *>(box->layout());
}

inline QLabel *statLabel(const QString &text, QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSize(font.pointSize() + 6);
    font.setBold(true);
    label->setFont(font);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

inline QScrollArea *scrollWrap(QWidget *inner)
{
    auto *scroll = new FitScrollArea;
    inner->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    scroll->setWidget(inner);
    return scroll;
}

inline QWidget *pageRoot(QVBoxLayout **layoutOut)
{
    auto *root = new QWidget;
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    *layoutOut = layout;
    return root;
}

} // namespace Ui
