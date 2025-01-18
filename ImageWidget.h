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
#include <QTransform>

class ImageWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget *parent = nullptr);
    bool empty();
    void setFlip(Qt::Orientations o);
    const QImage &image();
    const QPoint &imagePosition() const { return m_imagePos; }
    const QSize &imageSize() const { return m_imageSize; }
    void setImage(const QImage &i, QTransform matrix);
    void setImagePosition(const QPoint &p);
    void setImageSize(const QSize &s);
    void setLetterbox(const QRect &letterbox);
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal r);
    QTransform transformation() const;

protected:

    QSize sizeHint() const override;

    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
    qreal m_rotation = 0;
    QSize m_imageSize;
    QPoint m_imagePos;
    QTransform m_exifTransformation;
    Qt::Orientations m_flip;
    QRect m_letterBox;
};

#endif // IMAGEWIDGET_H
