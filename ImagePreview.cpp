/*
 *  Copyright (C) 2013-2014 Ofer Kashayov <oferkv@live.com>
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

#include <QBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>

#include "ImagePreview.h"
#include "ImageViewer.h"
#include "Settings.h"
#include "ThumbsViewer.h"

ImagePreview::ImagePreview(QWidget *parent) : QWidget(parent)
{

    imageLabel = new QLabel;
    imageLabel->setScaledContents(true);

    scrollArea = new QScrollArea;
    scrollArea->setContentsMargins(0, 0, 0, 0);
    scrollArea->setAlignment(Qt::AlignCenter);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->verticalScrollBar()->blockSignals(true);
    scrollArea->horizontalScrollBar()->blockSignals(true);
    scrollArea->setFrameStyle(0);
    scrollArea->setWidget(imageLabel);
    scrollArea->setWidgetResizable(true);

    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(scrollArea);
    setBackgroundColor();

    setLayout(mainLayout);
}

QPixmap& ImagePreview::loadImage(QString imageFileName) {
    if (animation) {
        delete animation;
    }

    QImageReader imageReader(imageFileName);

    if (!imageReader.size().isValid()) {
        previewPixmap = QIcon::fromTheme("image-missing",
                                         QIcon(":/images/error_image.png")).pixmap(BAD_IMAGE_SIZE, BAD_IMAGE_SIZE);
    } else if (Settings::enableAnimations && imageReader.supportsAnimation()){
        animation = new QMovie(imageFileName);
        animation->setParent(imageLabel);
        animation->start();
        imageLabel->setMovie(animation);
        previewPixmap = animation->currentPixmap();
    } else {
        QSize resize = imageReader.size();
        resize.scale(QSize(imageLabel->width(), imageLabel->height()), Qt::KeepAspectRatio);
        QImage previewImage;
        imageReader.read(&previewImage);
        if (Settings::exifRotationEnabled) {
            imageViewer->rotateByExifRotation(previewImage, imageFileName);
        }
        previewPixmap = QPixmap::fromImage(previewImage);
    }
    if (animation) {
        imageLabel->setMovie(animation);
    } else {
        imageLabel->setPixmap(previewPixmap);
    }

    resizeImagePreview();
    return previewPixmap;
}

void ImagePreview::clear() {
    imageLabel->clear();
}

void ImagePreview::resizeImagePreview()
{
    QSize previewSizePixmap = previewPixmap.size();

    if (Settings::upscalePreview || previewSizePixmap.width() > scrollArea->width() || previewSizePixmap.height() > scrollArea->height()) {
        previewSizePixmap.scale(scrollArea->width(), scrollArea->height(), Qt::KeepAspectRatio);
    }

    imageLabel->setFixedSize(previewSizePixmap);
    imageLabel->adjustSize();
}

void ImagePreview::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    resizeImagePreview();
}

void ImagePreview::setBackgroundColor() {
    QPalette pal = scrollArea->palette();
    pal.setColor(backgroundRole(), QColor(Settings::thumbsBackgroundColor.red(), Settings::thumbsBackgroundColor.green(), Settings::thumbsBackgroundColor.blue()));
    scrollArea->setPalette(pal);
}

void ImagePreview::setImageViewer(ImageViewer *imageViewer) {
    this->imageViewer = imageViewer;
}
