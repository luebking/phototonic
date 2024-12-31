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

#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QOpenGLWidget>

class ImageWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget *parent = nullptr);
    bool empty();
    const QImage &image();
    const QPoint &imagePosition() { return m_imagePos; }
    const QSize &imageSize() { return m_imageSize; }
    void setImage(const QImage &i);
    void setImagePosition(const QPoint &p);
    void setImageSize(const QSize &s);
    qreal rotation() { return m_rotation; }
    void setRotation(qreal r);
    QPoint mapToImage(QPoint p);

protected:

    QSize sizeHint() const override;

    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
    qreal m_rotation = 0;
    QSize m_imageSize;
    QPoint m_imagePos;
};

#endif // IMAGEWIDGET_H
