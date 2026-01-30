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

#include "ImageWidget.h"
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QVariantAnimation>

ImageWidget::ImageWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    m_fadeout = 0.0;
    m_crossfade = false;
    m_showGrid= false;
    setAutoFillBackground(true);
}

bool ImageWidget::empty()
{
    return m_image.isNull();
}

const QImage &ImageWidget::image()
{
    return m_image;
}

void ImageWidget::setCrossfade(bool yesno)
{
    m_crossfade = yesno;
    if (!m_crossfade)
        m_prevImage = QImage();
}

void ImageWidget::setImage(const QImage &i, QTransform matrix, bool resetTransform)
{
    if (m_crossfade) {
        m_prevImage = m_image;
        m_prevImagePos = m_imagePos;
        m_prevImageSize = m_imageSize;
    }
    m_image = i;
    if (resetTransform) {
        m_imageSize = i.size();
        m_rotation = 0;
    }
    m_exifTransformation = matrix;
    if (m_crossfade) {
        m_fadeout = 1.0;
        static QVariantAnimation *fadeAnimator = nullptr;
        if (!fadeAnimator) {
            fadeAnimator = new QVariantAnimation(this);
            fadeAnimator->setStartValue(1.0);
            fadeAnimator->setEndValue(0.0);
            fadeAnimator->setDuration(250);
            connect(fadeAnimator, &QVariantAnimation::valueChanged, [=](const QVariant &value) {
                if (fadeAnimator->state() != QAbstractAnimation::Running)
                    return;
                m_fadeout = value.toFloat();
                update();
            });
            connect(fadeAnimator, &QVariantAnimation::finished, [=]() {m_prevImage = QImage();});
            connect(fadeAnimator, &QObject::destroyed, [=]() {fadeAnimator = nullptr;});
        }
        fadeAnimator->start();
    } else {
        update();
    }
}

void ImageWidget::setRotation(qreal r)
{
    m_rotation = r;
    update();
}

QTransform ImageWidget::transformation() const {
    return transformation(m_image, m_imageSize, m_imagePos);
}

QTransform ImageWidget::transformation(const QImage &img, const QSize &sz, const QPoint &pos) const {
    float scale = qMax(float(sz.width()) / img.width(), float(sz.height()) / img.height());
    QTransform matrix;
    QPoint center(width() / 2, height() / 2);
    matrix.translate(center.x(), center.y());
    matrix.rotate(m_rotation);
    matrix.translate(-center.x(), -center.y());

    // translate
    QPoint origin;
    if (m_flip & Qt::Horizontal)
        origin.setX(sz.width());
    if (m_flip & Qt::Vertical)
        origin.setY(sz.height());
    origin += pos;
    matrix.translate(origin.x(), origin.y());

    // scale
    matrix.scale((m_flip & Qt::Horizontal) ? -scale : scale, (m_flip & Qt::Vertical) ? -scale : scale);
    return matrix;
}

void ImageWidget::setImageSize(const QSize &s)
{
    m_imageSize = s;
    update();
}

void ImageWidget::setImagePosition(const QPoint &p)
{
    m_imagePos = p;
    update();
}

void ImageWidget::setFlip(Qt::Orientations o)
{
    m_flip = o;
    update();
}

void ImageWidget::setLetterbox(const QRect &letterbox)
{
    m_letterBox = letterbox;
    update();
}

QSize ImageWidget::sizeHint() const
{
    return m_image.size();
}

void ImageWidget::paintEvent(QPaintEvent *ev)
{
    float scale = qMax(float(m_imageSize.width()) / m_image.width(), float(m_imageSize.height()) / m_image.height());
    if (scale == 0.0f)
        return;

    QPainter painter(this);
    painter.fillRect(rect(), palette().color(backgroundRole()));
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QRect clip = rect();
    clip.adjust(qRound(width()*m_letterBox.x()/100.0),
                qRound(height()*m_letterBox.y()/100.0),
               -qRound(width()*(100-m_letterBox.right())/100.0),
               -qRound(height()*(100-m_letterBox.bottom())/100.0));
    painter.setClipRect(clip);

    // exif
    /// @todo  this doesn't work, because the width/height are swapped and the translation is off/inverted
    // I don't want to copy the Image into a pre-translation, but for now that's what we'll do
//    painter.setWorldTransform(m_exifTransformation);

    painter.setTransform(transformation());
    if (m_crossfade)
        painter.setOpacity(1.0 - m_fadeout);
    painter.drawImage(0,0, m_image);

    if (m_crossfade) {
        if (m_prevImage.size().isNull())
            return;

        scale = qMax(float(m_imageSize.width()) / m_prevImage.width(), float(m_imageSize.height()) / m_prevImage.height());
        if (scale == 0.0f)
            return;
        painter.setTransform(transformation(m_prevImage, m_prevImageSize, m_prevImagePos));
        painter.setOpacity(m_fadeout);
        painter.drawImage(0,0, m_prevImage);
    }
    if (m_showGrid) {
        painter.setTransform(QTransform());
        painter.setCompositionMode(QPainter::CompositionMode_Exclusion);
        painter.setPen(Qt::white);
        const int d = qMax(10, qMax(height()/100, width()/100));
        for (int x = d; x < width(); x += d)
            painter.drawLine(x, 0, x, height());
        for (int y = d; y < height(); y += d)
            painter.drawLine(0, y, width(), y);
    }
}
