/*
 *  Copyright (C) 2018 Shawn Rutledge <s@ecloud.org>
 *  This file is part of Phototonic Image Viewer.
 *
 *  Phototonic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Phototonic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Phototonic.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "GuideWidget.h"
#include <QAction>
#include <QMouseEvent>
#include <QPainter>

static const int RuleWidgetWidth = 5; // determines the active area for dragging

QAction *GuideWidget::m_deleteAction = nullptr;

GuideWidget::GuideWidget(QWidget *parent, Qt::Orientation o, int offset) : QWidget(parent), m_vertical(o == Qt::Vertical)
{
    if (!parent) {
        qWarning() << "GuideWidget requires a parent to guide around! I'm gonna die and you're gonna crash…";
        deleteLater();
        return;
    }
    resizeToParent();
    parent->installEventFilter(this);
    if (offset)
        offset -= RuleWidgetWidth / 2;
    setCursor(m_vertical ? Qt::SplitHCursor : Qt::SplitVCursor);
    move(m_vertical ? offset : 0, m_vertical ? 0 : offset);
    if (!m_deleteAction)
        m_deleteAction = new QAction(GuideWidget::tr("Remove guide"));
    addAction(m_deleteAction);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(m_deleteAction, &QAction::triggered, this, &QObject::deleteLater);
    show();
}

bool GuideWidget::eventFilter(QObject *o, QEvent *e)
{
    if (o == parent() && e->type() == QEvent::Resize)
        resizeToParent();
    return false;
}

void GuideWidget::resizeToParent()
{
    if (m_vertical)
        resize(RuleWidgetWidth, parentWidget()->height());
    else
        resize(parentWidget()->width(), RuleWidgetWidth);
}

void GuideWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->modifiers() != Qt::NoModifier) {
        QWidget::mouseMoveEvent(event);
        return; // don't interfere with crop selection etc.
    }
    if (m_vertical)
        move(x() + event->pos().x() - RuleWidgetWidth/2, 0);
    else
        move(0, y() + event->pos().y() - RuleWidgetWidth/2);
}

void GuideWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setPen(Qt::cyan);
    int xy = RuleWidgetWidth / 2;
    if (m_vertical)
        painter.drawLine(xy, 0, xy, height());
    else
        painter.drawLine(0, xy, width(), xy);
}
