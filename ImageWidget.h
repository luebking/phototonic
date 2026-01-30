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
    void setCrossfade(bool yesno);
    void setImage(const QImage &i, QTransform matrix, bool resetTransform = true);
    void setImagePosition(const QPoint &p);
    void setImageSize(const QSize &s);
    void setLetterbox(const QRect &letterbox);
    void showGrid(bool show) { m_showGrid = show; update(); }
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal r);
    QTransform transformation() const;

protected:

    QSize sizeHint() const override;

    void paintEvent(QPaintEvent *event) override;

private:
    QTransform transformation(const QImage &img, const QSize &sz, const QPoint &pos) const;
    QImage m_image;
    QImage m_prevImage;
    qreal m_rotation = 0;
    QSize m_imageSize;
    QPoint m_imagePos;
    QSize m_prevImageSize;
    QPoint m_prevImagePos;
    QTransform m_exifTransformation;
    Qt::Orientations m_flip;
    QRect m_letterBox;
    float m_fadeout;
    bool m_crossfade;
    bool m_showGrid;
};

#endif // IMAGEWIDGET_H
