/*
 *  Copyright (C) 2013-2014 Ofer Kashayov - oferkv@live.com
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

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QWheelEvent>
#include <QVariantAnimation>

#include <unistd.h>

#include "CropDialog.h"
#include "CropRubberband.h"
#include "ImageWidget.h"
#include "ImageViewer.h"
#include "MessageBox.h"
#include "MetadataCache.h"
#include "Settings.h"


#define CLIPBOARD_IMAGE_NAME "clipboard.png"
#define ROUND(x) ((int) ((x) + 0.5))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

namespace { // anonymous, not visible outside of this file
Q_DECLARE_LOGGING_CATEGORY(PHOTOTONIC_EXIV2_LOG)
Q_LOGGING_CATEGORY(PHOTOTONIC_EXIV2_LOG, "phototonic.exif", QtCriticalMsg)

struct Exiv2LogHandler {
    static void handleMessage(int level, const char *message) {
        switch(level) {
            case Exiv2::LogMsg::debug:
                qCDebug(PHOTOTONIC_EXIV2_LOG) << message;
                break;
            case Exiv2::LogMsg::info:
                qCInfo(PHOTOTONIC_EXIV2_LOG) << message;
                break;
            case Exiv2::LogMsg::warn:
            case Exiv2::LogMsg::error:
            case Exiv2::LogMsg::mute:
                qCWarning(PHOTOTONIC_EXIV2_LOG) << message;
                break;
            default:
                qCWarning(PHOTOTONIC_EXIV2_LOG) << "unhandled log level" << level << message;
                break;
        }
    }

    Exiv2LogHandler() {
        Exiv2::LogMsg::setHandler(&Exiv2LogHandler::handleMessage);
    }
};

class ClickToClose : public QObject {
    public:
        ClickToClose() : QObject() {}
    protected:
        bool eventFilter(QObject *o, QEvent *e) {
            if (e->type() == QEvent::MouseButtonRelease)
                static_cast<QWidget*>(o)->hide();
            return false;
        }
};

} // anonymous namespace

ClickToClose *gs_clickToClose = nullptr;


ImageViewer::ImageViewer(QWidget *parent) : QScrollArea(parent) {
    // This is a threadsafe way to ensure that we only register it once
    static Exiv2LogHandler handler;

    if (!gs_clickToClose)
        gs_clickToClose = new ClickToClose;

    m_letterbox = QRect(QPoint(0,0), QPoint(100,100));
    myContextMenu = nullptr;
    cursorIsHidden = false;
    moveImageLocked = false;
    imageWidget = new ImageWidget;
    imageWidget->setLetterbox(m_letterbox);
    m_crossfade = false;
    m_loading = false;
    imageWidget->setCrossfade(m_crossfade);
    animation = nullptr;
    m_zoomMode = ZoomToFit;
    m_zoom = 1.0;
    m_lockZoom = false;
    m_edited = false;
    m_editMode = Crop;

    setContentsMargins(0, 0, 0, 0);
    setAlignment(Qt::AlignCenter);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(0);
    setWidget(imageWidget);
    setWidgetResizable(false);
    setBackgroundColor();

    myFilenameLabel = new QLabel(this);
    myFilenameLabel->setVisible(Settings::showImageName);
    myFilenameLabel->setMargin(3);
    myFilenameLabel->move(10, 10);
    myFilenameLabel->setAutoFillBackground(true);
    myFilenameLabel->setFrameStyle(QFrame::Plain|QFrame::NoFrame);
    QPalette pal = myFilenameLabel->palette();
    pal.setColor(myFilenameLabel->backgroundRole(), QColor(0,0,0,128));
    pal.setColor(myFilenameLabel->foregroundRole(), QColor(255,255,255,128));
    myFilenameLabel->setPalette(pal);

    feedbackLabel = new QLabel(this);
    feedbackLabel->setVisible(false);
    feedbackLabel->setMargin(3);
    feedbackLabel->setFrameStyle(QFrame::Plain|QFrame::NoFrame);
    feedbackLabel->setAutoFillBackground(true);
    feedbackLabel->setPalette(pal);
    feedbackLabel->installEventFilter(gs_clickToClose);

    mouseMovementTimer = new QTimer(this);
    connect(mouseMovementTimer, SIGNAL(timeout()), this, SLOT(monitorCursorState()));

    Settings::hueVal = 0;
    Settings::saturationVal = 100;
    Settings::lightnessVal = 100;
    Settings::hueRedChannel = true;
    Settings::hueGreenChannel = true;
    Settings::hueBlueChannel = true;

    Settings::contrastVal = 78;
    Settings::brightVal = 100;

    Settings::dialogLastX = Settings::dialogLastY = 0;

    Settings::mouseRotateEnabled = false;

    newImage = false;
    cropRubberBand = 0;
    m_preloadThread = nullptr;
}

void ImageViewer::lockZoom(bool locked) {
    m_lockZoom = locked;
    setFeedback(locked ? tr("Zoom Locked") : tr("Zoom Unlocked"));
}

void ImageViewer::zoomTo(float goal, QPoint focus, int duration) {
    static QVariantAnimation *zoominator = nullptr;
    if (goal == m_zoom && (!zoominator || zoominator->state() != QAbstractAnimation::Running))
        return; // idempotent
    if (focus.x() < 0)
        focus = rect().center();
    m_zoomMode = ZoomOriginal;
    if (!zoominator) {
        zoominator = new QVariantAnimation(this);
        connect(zoominator, &QVariantAnimation::valueChanged, [=](const QVariant &value) {
            if (zoominator->state() != QAbstractAnimation::Running)
                return;
            m_zoom = value.toFloat();
            resizeImage(zoominator->property("zoomfocus").toPoint());
        });
        connect(zoominator, &QObject::destroyed, [=]() {zoominator = nullptr;});
    }
    zoominator->setDuration(duration);
    zoominator->setProperty("zoomfocus", focus);
    zoominator->setStartValue(m_zoom);
    zoominator->setEndValue(goal);
    zoominator->start();
}

void ImageViewer::zoomTo(ImageViewer::ZoomMode mode, QPoint focus, int duration) {
    QSize imageSize = animation ? animation->currentPixmap().size() : imageWidget->image().size();
    if (imageSize.isEmpty())
        return;

    float factor = 1.0;
    if (mode == ZoomToFit) {
        QSize targetSize = imageSize.scaled(size(), Qt::KeepAspectRatio);
        factor = targetSize.width()/float(imageSize.width());
        setFeedback(tr("Fit View"));
    }
    else if (mode == ZoomToFill) {
        QSize targetSize = imageSize.scaled(size(), Qt::KeepAspectRatioByExpanding);
        factor = targetSize.width()/float(imageSize.width());
        setFeedback(tr("Fill View"));
    } else {
        setFeedback(tr("Original Size"));
    }
    zoomTo(factor, focus, duration);
    QTimer::singleShot(duration, this, [=]() {m_zoomMode = mode;});
}

void ImageViewer::resizeImage(QPoint focus) {
    QSize imageSize = animation ? animation->currentPixmap().size() : imageWidget->image().size();
    if (imageSize.isEmpty())
        return;

    if (m_zoomMode == ZoomToFit) {
        QSize oSize = imageSize;
        imageSize.scale(size(), Qt::KeepAspectRatio);
        m_zoom = imageSize.width()/float(oSize.width());
    }
    else if (m_zoomMode == ZoomToFill) {
        QSize oSize = imageSize;
        imageSize.scale(size(), Qt::KeepAspectRatioByExpanding);
        m_zoom = imageSize.width()/float(oSize.width());
    } else {
        imageSize = QSize(qRound(imageSize.width() * m_zoom),
                          qRound(imageSize.height() * m_zoom));
    }

    if (imageWidget) {
        imageWidget->setFlip(m_flip);
        imageWidget->setRotation(Settings::rotation);
        imageWidget->setFixedSize(size());
        if (imageSize.width() < width() || imageSize.height() < height()) {
            centerImage(imageSize);
        } else {
            const double fx = double(imageSize.width())/imageWidget->imageSize().width(),
                         fy = double(imageSize.height())/imageWidget->imageSize().height();
            int x,y;
            if (focus.x() > -1 && focus.y() > -1) {
                x = qRound(fx*(imageWidget->imagePosition().x() - focus.x()) + focus.x());
                y = qRound(fy*(imageWidget->imagePosition().y() - focus.y()) + focus.y());
            } else {
                x = qRound(imageWidget->imagePosition().x()*fx);
                y = qRound(imageWidget->imagePosition().y()*fy);
            }

            if (imageSize.width() >= width())
                x = qMax(qMin(x, 0),  width() - imageSize.width());
            if (imageSize.height() >= height())
                y = qMax(qMin(y, 0), height() - imageSize.height());
            imageWidget->setImagePosition(QPoint(x,y));
        }
        imageWidget->setImageSize(imageSize);
    } else {
        widget()->setFixedSize(imageSize);
//        widget()->adjustSize();
        if (imageSize.width() < width() + 100 || imageSize.height() < height() + 100) {
            centerImage(imageSize);
        } else {
            float positionY = verticalScrollBar()->value() > 0 ? verticalScrollBar()->value() / float(verticalScrollBar()->maximum()) : 0;
            float positionX = horizontalScrollBar()->value() > 0 ? horizontalScrollBar()->value() / float(horizontalScrollBar()->maximum()) : 0;
            horizontalScrollBar()->setValue(horizontalScrollBar()->maximum() * positionX);
            verticalScrollBar()->setValue(verticalScrollBar()->maximum() * positionY);
        }
    }
}

void ImageViewer::scaleImage(QSize newSize) {
    origImage = origImage.scaled(newSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    refresh();
    setFeedback(tr("New image size: %1x%2").arg(origImage.width()).arg(origImage.height()));
    emit imageEdited(true);
}

void ImageViewer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    const bool mapCrop = imageWidget && cropRubberBand && cropRubberBand->isVisibleTo(this);
    QRect isoCropRect;
    if (mapCrop) {
        QTransform matrix = imageWidget->transformation().inverted();
        isoCropRect = matrix.mapRect(cropRubberBand->geometry());
    }
    resizeImage();
    if (mapCrop) {
        QTransform matrix = imageWidget->transformation();
        cropRubberBand->setGeometry(matrix.mapRect(isoCropRect));
    }
}

void ImageViewer::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    resizeImage();
}

void ImageViewer::showGrid(bool show) {
    if (imageWidget)
        imageWidget->showGrid(show);
}

void ImageViewer::centerImage(QSize imgSize) {
    if (imageWidget) {
        imageWidget->setImagePosition(QPoint((imageWidget->width() - imgSize.width())/2, (imageWidget->height() - imgSize.height())/2));
    } else {
        ensureVisible(imgSize.width()/2, imgSize.height()/2, width()/2, height()/2);
    }
}

static inline int bound0To255(int val) {
    return ((val > 255) ? 255 : (val < 0) ? 0 : val);
}

static inline int hslValue(float n1, float n2, int hue) {
    double value;

    if (hue > 255) {
        hue -= 255;
    } else if (hue < 0) {
        hue += 255;
    }

    if (hue < 42.5f) {
        value = n1 + (n2 - n1) * (hue / 42.5f);
    } else if (hue < 127.5f) {
        value = n2;
    } else if (hue < 170) {
        value = n1 + (n2 - n1) * ((170 - hue) / 42.5f);
    } else {
        value = n1;
    }

    return ROUND(value * 255.0f);
}

void rgbToHsl(int r, int g, int b, unsigned char *hue, unsigned char *sat, unsigned char *light) {
    float h, s, l;
    int min, max;
    int delta;

    if (r > g) {
        max = MAX(r, b);
        min = MIN(g, b);
    } else {
        max = MAX(g, b);
        min = MIN(r, b);
    }

    l = (max + min) / 2.0f;

    if (max == min) {
        s = 0.0;
        h = 0.0;
    } else {
        delta = (max - min);

        if (l < 128) {
            s = 255 * delta / float(max + min);
        } else {
            s = 255 * delta / float(511 - max - min);
        }

        if (r == max) {
            h = (g - b) / float(delta);
        } else if (g == max) {
            h = 2 + (b - r) / float(delta);
        } else {
            h = 4 + (r - g) / float(delta);
        }

        h = h * 42.5f;
        if (h < 0) {
            h += 255;
        } else if (h > 255) {
            h -= 255;
        }
    }

    *hue = ROUND(h);
    *sat = ROUND(s);
    *light = ROUND(l);
}

void hslToRgb(int h, int s, int l,
              unsigned char *red, unsigned char *green, unsigned char *blue) {
    if (s == 0) {
        /* achromatic case */
        *red = l;
        *green = l;
        *blue = l;
    } else {
        float m1, m2;

        if (l < 128)
            m2 = (l * (255 + s)) / 65025.0f;
        else
            m2 = (l + s - (l * s) / 255.0f) / 255.0f;

        m1 = (l / 127.5f) - m2;

        /* chromatic case */
        *red = hslValue(m1, m2, h + 85);
        *green = hslValue(m1, m2, h);
        *blue = hslValue(m1, m2, h - 85);
    }
}

static int linesDone = 0;
void ImageViewer::colorize(uchar *bits, int bytesPerLine, int startLine, int endLine, const unsigned char (*contrastTransform)[256], const unsigned char (*brightTransform)[256]) {
    unsigned char hr, hg, hb;
    unsigned char h, s, l;
    int r, g, b;
    QRgb *line;

    bool hasAlpha = viewerImage.hasAlphaChannel();

//    int ciddqd = 0;
    QRgb iddqd[2] = {qRgba(0,0,0,0), qRgba(0,0,0,0)};

    for (int y = startLine; y < endLine; ++y) {
        line = reinterpret_cast<QRgb*>(bits + (y * bytesPerLine));
        for (int x = 0; x < viewerImage.width(); ++x) {
            // Cheat, maybe the pixel is the same as the previous
            QRgb rgb = qRgb(qRed(line[x]), qGreen(line[x]), qBlue(line[x]));
            if (iddqd[0] == rgb) {
//                ++ciddqd;
                rgb = iddqd[1];
                line[x] = hasAlpha ? qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), qAlpha(line[x])) : rgb;
                continue;
            }

            r = qRed(rgb);
            if (Settings::hueRedChannel) {
                if (Settings::rNegateEnabled)
                    r = 255 - r;
                r = bound0To255((r * (Settings::redVal + 100)) / 100);
                r = (*brightTransform)[r];
                r = (*contrastTransform)[r];
            }

            g = qGreen(rgb);
            if (Settings::hueGreenChannel) {
                if (Settings::gNegateEnabled)
                    g = 255 - g;
                g = bound0To255((g * (Settings::greenVal + 100)) / 100);
                g = (*brightTransform)[g];
                g = (*contrastTransform)[g];
            }

            b = qBlue(rgb);
            if (Settings::hueBlueChannel) {
                if (Settings::bNegateEnabled)
                    b = 255 - b;
                b = bound0To255((b * (Settings::blueVal + 100)) / 100);
                b = (*brightTransform)[b];
                b = (*contrastTransform)[b];
            }

            if (Settings::hueVal != 0 || Settings::saturationVal != 100 || Settings::lightnessVal != 100) {
                rgbToHsl(r, g, b, &h, &s, &l);
                h = Settings::colorizeEnabled ? Settings::hueVal : h + Settings::hueVal;
                s = bound0To255(((s * Settings::saturationVal) / 100));
                l = bound0To255(((l * Settings::lightnessVal) / 100));
                hslToRgb(h, s, l, &hr, &hg, &hb);
                if (Settings::hueRedChannel)
                    r = hr;
                if (Settings::hueGreenChannel)
                    g = hg;
                if (Settings::hueBlueChannel)
                    b = hb;
            }

            iddqd[0] = rgb;
            iddqd[1] = rgb = qRgb(r, g, b);
            line[x] = hasAlpha ? qRgba(r, g, b, qAlpha(line[x])) : rgb;
            linesDone = y;
        }
    }
}

void ImageViewer::colorize() {
    switch(viewerImage.format()) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        break;
    default:
        viewerImage = viewerImage.convertToFormat(QImage::Format_RGB32);
    }

    float contrast = ((float) Settings::contrastVal / 100.0);
    float brightness = ((float) Settings::brightVal / 100.0);

    static unsigned char contrastTransform[256];
    static unsigned char brightTransform[256];
    for (int i = 0; i < 256; ++i) {
        if (i < (int) (128.0f + 128.0f * tan(contrast)) && i > (int) (128.0f - 128.0f * tan(contrast))) {
            contrastTransform[i] = bound0To255((i - 128) / tan(contrast) + 128);
        } else if (i >= (int) (128.0f + 128.0f * tan(contrast))) {
            contrastTransform[i] = 255;
        } else {
            contrastTransform[i] = 0;
        }
    }

    for (int i = 0; i < 256; ++i) {
        brightTransform[i] = bound0To255(int((255.0 * pow(i / 255.0, 1.0 / brightness)) + 0.5));
    }

//    QElapsedTimer profiler;
//    profiler.start();

    const int threadCount = qMin(QThread::idealThreadCount(), viewerImage.height()/512);

    uchar *bits;
    if (viewerImage.constBits() == origImage.constBits()) {
        bits = viewerImage.bits(); // detach to preserve origImage
    } else {
        bits = const_cast<uchar*>(viewerImage.constBits());
    }

    const int bpl = viewerImage.bytesPerLine();
    if (!threadCount) {
        colorize(bits, bpl, 0, viewerImage.height(), &contrastTransform, &brightTransform);
    } else {
        QThread **threads = new QThread*[threadCount];
        int batch = viewerImage.height()/threadCount;
        if (threadCount > 1) {
            for (int i = 0; i < threadCount-1; ++i) {
                threads[i] = QThread::create([=](){colorize(bits, bpl, i*batch, (i+1)*batch, &contrastTransform, &brightTransform);});
                threads[i]->start();
            }
        }
        threads[threadCount-1] = QThread::create([=](){colorize(bits, bpl, (threadCount-1)*batch, viewerImage.height(), &contrastTransform, &brightTransform);});
        threads[threadCount-1]->start();

#if 1
        // live updates make things slower but give the user the comfort that sth's going on
        bool done = false;
        int cycle = 1;
        while (!done) {
            done = true;
            for (int i = 0; i < threadCount; ++i) {
                if (!threads[i]->wait(250)) {
                    done = false;
                    // the threads update linesDone independently and we just guess that they run at
                    // the same speed, then predict how long this is gonna take and avoid "late" previews
                    // that would cause overall slowdown
                    if (float(threadCount*linesDone)/viewerImage.height() < 1.0f-1.0f/++cycle) {
                        // we need to wrap the altered bits in a new image to make Qt/QGLWidget
                        // understand/believe that there's sth. to update here
                        // the final update in ::refresh is fine because viewerImage got detached
                        imageWidget->setImage(QImage(viewerImage.constBits(), viewerImage.width(),
                                                     viewerImage.height(), viewerImage.bytesPerLine(),
                                                     viewerImage.format()), m_exifTransformation);
                        resizeImage();
                        imageWidget->repaint();
                    }
                    break;
                }
            }
        }
#else
        for (int i = 0; i < threadCount; ++i)
            threads[i]->wait();
#endif
        for (int i = 0; i < threadCount; ++i)
            delete threads[i];
        delete [] threads;
    }

//    qDebug() << profiler.elapsed() << threadCount; // << "IDDQD:" << ciddqd;
    emit imageEdited(true);
}

void ImageViewer::refresh() {
    if (!imageWidget) {
        return;
    }

    viewerImage = origImage;

    if (Settings::colorsActive) {
        colorize();
    }

    imageWidget->setImage(viewerImage, m_exifTransformation);
    resizeImage();
}

void ImageViewer::setImage(const QImage &image) {
    imageWidget->setImage(image, m_exifTransformation);
}

void ImageViewer::reload() {
    emit imageEdited(false);
    static bool s_busy = false;
    static bool s_abort = false;
    if (s_busy) {
        s_abort = true;
        QMetaObject::invokeMethod(this, "reload", Qt::QueuedConnection);
        return;
    }
//    setFeedback("",false);
    if (Settings::showImageName) {
        if (fullImagePath.left(1) == ":") {
            setInfo("No Image");
        } else if (fullImagePath.isEmpty()) {
            setInfo("Clipboard");
        } else {
            setInfo(QFileInfo(fullImagePath).fileName());
        }
    }

    if (!Settings::keepTransform) {
        Settings::rotation = 0;
        m_flip = Qt::Orientations();
    }

    if (!batchMode) {
        Settings::mouseRotateEnabled = false;
        emit toolsUpdated();

        if (newImage || fullImagePath.isEmpty()) {

            newImage = true;
            fullImagePath = CLIPBOARD_IMAGE_NAME;
            origImage.load(":/images/no_image.png");
            viewerImage = origImage;
            setImage(viewerImage);
            pasteImage();
            return;
        }
    }

    QImageReader imageReader(fullImagePath);
    if (batchMode && imageReader.supportsAnimation()) {
        //: this is a warning on the console
        qWarning() << tr("skipping animation in batch mode:") << fullImagePath;
        return;
    }

    imageWidget->setCrossfade(m_crossfade);
    if (Settings::enableAnimations && imageReader.supportsAnimation()) {
        if (animation) {
            delete animation;
            animation = nullptr;
        }
        animation = new QMovie(fullImagePath);

        if (animation->frameCount() > 1) {
            viewerImage = origImage = QImage();
            animation->setParent(imageWidget);
            connect(animation, &QMovie::updated, this, [=]() {
                imageWidget->setCrossfade(false);
                imageWidget->setImage(animation->currentImage(), m_exifTransformation, false);
                imageWidget->setFlip(m_flip);
                imageWidget->setRotation(Settings::rotation);
            });
            animation->start();
            resizeImage();
            centerImage(imageWidget->imageSize());
            if (Settings::keepTransform) {
                imageWidget->setFlip(m_flip);
                imageWidget->setRotation(Settings::rotation);
            }
            return;
        }
    }

    // clean up - don't waste memory and bogus 1-frame gifs will otherwise kill resizeImage()
    delete animation;
    animation = nullptr;

    auto loadThreaded = [=](QThread *thread) {
        s_busy = true;
        while (!thread->wait(30)) {
            QApplication::processEvents();
            if (s_abort) {
                thread->terminate();
                thread->wait();
                break;
            }
        }
        s_busy = false;
        thread->deleteLater();
        if (s_abort) {
            s_abort = false;
            return false;
        }
        return true;
    };

    // It's not a movie
    if (fullImagePath == m_preloadedPath) {
        bool ok = true;
        if (m_preloadThread) {
            ok = loadThreaded(m_preloadThread);
            m_preloadThread = nullptr; // clean up
        }
        if (ok)
            viewerImage = origImage = m_preloadedImage;
        m_preloadedImage = QImage();
        m_preloadedPath.clear();
        if (!ok)
            return;
    } else if (imageReader.size().isValid()) {
        QSize sz = imageReader.size();
        if (sz.width() * sz.height() > 8192*8192) { // allocation limit
            /// @todo this and the correct sqrt(double…) of the below still runs into the size limts?
            // perhaps because qimagereader overreads and needs some extra memory
            // sz.scale(8192,8192, Qt::KeepAspectRatio);
            double factor = double(8192*8192)/(sz.width() * sz.height());
            // the sqrt is correct, but too large - the square of the true factor will over"punish" large images
            // so we draw an average - this is by "shows the most annoying supersize image I have"
            factor = (2*factor + sqrt(factor))/3.0;
            sz *= factor;
            imageReader.setScaledSize(sz);
            setFeedback(tr( "<h1>Warning</h1>Original image size %1x%2 exceeds limits<br>"
                            "Downscaled to %3x%4<br><h3>Saving edits will save the smaller image!</h3>")
                            .arg(imageReader.size().width()).arg(imageReader.size().height())
                            .arg(sz.width()).arg(sz.height()), 10000);
        }
        bool imageOk = false;
        if (batchMode || Settings::slideShowActive) {
            imageOk = imageReader.read(&origImage);
        } else {
            QThread *thread = QThread::create([&](){imageOk = imageReader.read(&origImage);});
            thread->start();
            if (!loadThreaded(thread))
                return;
        }

        if (imageOk) {
            if (Settings::exifRotationEnabled) {
                m_exifTransformation = Metadata::transformation(fullImagePath);
                origImage = origImage.transformed(Metadata::transformation(fullImagePath), Qt::SmoothTransformation);
            }
            viewerImage = origImage;

            if (Settings::colorsActive) {
                colorize();
            }
        } else {
            viewerImage = QImage(":/images/error_image.png");
            setFeedback(QFileInfo(imageReader.fileName()).fileName() + ": " + imageReader.errorString());
        }
    }

    setImage(viewerImage);
    resizeImage();
    centerImage(imageWidget->imageSize());
    if (Settings::keepTransform) {
        imageWidget->setFlip(m_flip);
        imageWidget->setRotation(Settings::rotation);
    }
    if (Settings::setWindowIcon) {
        window()->setWindowIcon(QPixmap::fromImage(viewerImage.scaled(WINDOW_ICON_SIZE, WINDOW_ICON_SIZE,
                                                                      Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
}

void ImageViewer::setInfo(QString infoString) {
    myFilenameLabel->setText(infoString);
    myFilenameLabel->adjustSize();
}

void ImageViewer::unsetFeedback() {
    if (m_permanentFeedback.isEmpty()) {
        feedbackLabel->clear();
        feedbackLabel->setVisible(false);
    } else {
        setFeedback(m_permanentFeedback, false);
    }
}

void ImageViewer::setFeedback(QString feedbackString, int timeLimited) {
    if (!timeLimited)
        m_permanentFeedback = feedbackString;
    if (feedbackString.isEmpty()) {
        unsetFeedback();
        return;
    }
    feedbackLabel->setText(feedbackString);
    feedbackLabel->setVisible(true);

    int margin = myFilenameLabel->isVisible() ? (myFilenameLabel->height() + 15) : 10;
    feedbackLabel->move(10, margin);

    feedbackLabel->adjustSize();
    if (timeLimited) {
        static QTimer *unsetFeedbackTimer = nullptr;
        if (!unsetFeedbackTimer) {
            unsetFeedbackTimer = new QTimer(this);
            unsetFeedbackTimer->setSingleShot(true);
            connect (unsetFeedbackTimer, &QTimer::timeout, this, &ImageViewer::unsetFeedback);
        }
        unsetFeedbackTimer->setInterval(timeLimited);
        unsetFeedbackTimer->start();
    }
}

void ImageViewer::loadImage(QString imageFileName, const QImage &preview) {
    if (fullImagePath == imageFileName)
        return;
    secureEdits();
    unsetFeedback();
    newImage = false;
    fullImagePath = imageFileName;

    const QSize fullSize = QImageReader(fullImagePath).size();
    const bool largeImage = !fullSize.isValid() || fullSize.width() >= width() || fullSize.height() >= height();

    if (!m_lockZoom) {
        m_zoomMode = ZoomToFit;
        if (!largeImage) {
            m_zoomMode = ZoomOriginal;
            m_zoom = 1.0f;
        }
    }
    if (!preview.isNull()) {
        // don't preview small images w/ a huge thumbnail upscale
        // it's pointless and causes ugly flicker
        if (largeImage) {
            setImage(preview);
            const float o_zoom = m_zoom;
            if (m_zoomMode == ZoomOriginal && fullSize.isValid())
                m_zoom *= (fullSize.width()/float(preview.width()) + fullSize.height()/float(preview.height()))/2.0f;
            resizeImage();
            centerImage(imageWidget->imageSize());
            m_zoom = o_zoom;
        }
    }

    QApplication::processEvents();
    m_loading = true;
    reload();
    m_loading = false;
    if (!m_preloadPath.isEmpty()) {
        preload(m_preloadPath);
    }
}

void ImageViewer::preload(QString imageFileName) {
    if (m_preloadedPath == imageFileName)
        return;
    if (m_loading) {
        m_preloadPath = imageFileName;
        return;
    }
    if (m_preloadThread) {
        while (!m_preloadThread->wait(15)) { // we're already preloading stuff, so maybe wait a moment…
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (!m_preloadThread)
                break; // we've lost this to the main loader or below
        }
        QTimer::singleShot(0, this, [=](){ preload(imageFileName); }); // allow event processing, notably reload
        return;
    }

    m_preloadPath = QString();
    m_preloadedPath = imageFileName;
    // reload current one - maybe the file has changed on disk?
    /*if (m_preloadedPath == fullImagePath) {
        m_preloadedImage = origImage;
        return;
    }*/

    QImageReader imageReader(m_preloadedPath);
    if (m_preloadedPath.isEmpty() || imageReader.supportsAnimation()) {
        m_preloadedImage = QImage();
        m_preloadedPath.clear();
        return; // no preloading of animations
    }
    bool imageOk = false;
    if (imageReader.size().isValid()) {
        m_preloadThread = QThread::create([&](){
//            usleep(8000000);
            imageOk = imageReader.read(&m_preloadedImage);
            if (imageOk && Settings::exifRotationEnabled)
                m_preloadedImage = m_preloadedImage.transformed(Metadata::transformation(fullImagePath), Qt::SmoothTransformation);
            });
        m_preloadThread->start();
        while (!m_preloadThread->wait(15)) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (!m_preloadThread)
                return; // we've been taken over by the main loader
        }
        m_preloadThread->deleteLater();
        m_preloadThread = nullptr;
    }
    if (!imageOk) {
        m_preloadedImage = QImage();
        m_preloadedPath.clear();
    }
}

void ImageViewer::secureEdits() {
    if (m_edited) {
        if (MessageBox(this, MessageBox::Save|MessageBox::Discard, MessageBox::Save).ask(
                tr("Save edits?"),tr("The image was edited.\nDo you want to save a copy?"))
                == MessageBox::Save)
            saveImageAs();
        m_edited = false;
    }
}

void ImageViewer::clearImage() {
    secureEdits();
    fullImagePath.clear();
    origImage.load(":/images/no_image.png");
    viewerImage = origImage;
    setImage(viewerImage);
}

void ImageViewer::setContextMenu(QMenu *menu) {
    delete myContextMenu;
    myContextMenu = menu;
    myContextMenu->setParent(this);
}

void ImageViewer::setCrossfade(bool yesno) {
    m_crossfade = yesno;
    if (imageWidget)
        imageWidget->setCrossfade(m_crossfade);
}

void ImageViewer::monitorCursorState() {
    static QPoint lastPos;

    if (QCursor::pos() != lastPos) {
        lastPos = QCursor::pos();
        if (cursorIsHidden) {
            QApplication::restoreOverrideCursor();
            cursorIsHidden = false;
        }
    } else {
        if (!cursorIsHidden) {
            QApplication::setOverrideCursor(Qt::BlankCursor);
            cursorIsHidden = true;
        }
    }
}

void ImageViewer::setCursorHiding(bool hide) {
    if (hide) {
        mouseMovementTimer->start(500);
    } else {
        mouseMovementTimer->stop();
        if (cursorIsHidden) {
            QApplication::restoreOverrideCursor();
            cursorIsHidden = false;
        }
    }
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    QWidget::mouseDoubleClickEvent(event);
    while (QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    if (!imageWidget) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == Qt::ControlModifier) {
            cropOrigin = event->pos();
            if (!cropRubberBand) {
                cropRubberBand = new CropRubberBand(this);
                connect(cropRubberBand, &CropRubberBand::selectionChanged,
                        this, &ImageViewer::updateRubberBandFeedback);
                connect(cropRubberBand, &CropRubberBand::cropConfirmed,
                        this, &ImageViewer::applyCropAndRotation);
            }
            cropRubberBand->show();
            cropRubberBand->setGeometry(QRect(cropOrigin, event->pos()).normalized());
        } else if (!(Settings::mouseRotateEnabled || event->modifiers() == Qt::ShiftModifier)) {
            if (cropRubberBand && cropRubberBand->isVisible()) {
                cropRubberBand->hide();
                setFeedback("", false);
            }
        }
        QPointF fulcrum(QPointF(imageWidget->width() / 2.0, imageWidget->height() / 2.0));
        QLineF vector(fulcrum, event->position());
        initialRotation = imageWidget->rotation() + vector.angle();
        setMouseMoveData(true, event->position().x(), event->position().y());
        QApplication::setOverrideCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        setMouseMoveData(false, 0, 0);
        while (QApplication::overrideCursor()) {
            QApplication::restoreOverrideCursor();
        }
    }

    QWidget::mouseReleaseEvent(event);
}

void ImageViewer::updateRubberBandFeedback(QRect geom) {
    if (!imageWidget) {
        return;
    }
    bool ok;
    QTransform matrix = imageWidget->transformation().inverted(&ok);
    if (!ok)
        qDebug() << "something's fucked up about the transformation matrix!";
    m_isoCropRect = geom = matrix.mapRect(geom);
    setFeedback(tr("Selection: ") + QString("%1x%2").arg(geom.width()).arg(geom.height())
                                  + QString(geom.x() < 0 ? "%1" : "+%1").arg(geom.x())
                                  + QString(geom.y() < 0 ? "%1" : "+%1").arg(geom.y()), false);
    static QTimer *doubleclickhint = nullptr;
    if (!doubleclickhint) {
        doubleclickhint = new QTimer(this);
        doubleclickhint->setInterval(2000);
        doubleclickhint->setSingleShot(true);
        connect(doubleclickhint, &QTimer::timeout, [=]() {
                                        if (cropRubberBand && cropRubberBand->isVisible())
                                            setFeedback(tr("Doubleclick to crop, right click to abort"), 10000);
                                        });
    }
    doubleclickhint->start();
}

void ImageViewer::setEditMode(Edit mode) {
    m_editMode = mode;
    QString msg;
    switch (m_editMode) {
    case Crop:
        msg = tr("Select the crop area with Ctrl + left mouse button"); break;
    case Blackout:
        msg = tr("Select the blackout area with Ctrl + left mouse button"); break;
    case Cartouche:
        msg = tr("Select the cartouche area with Ctrl + left mouse button"); break;
    case Annotate:
        msg = tr("Select the annotation area with Ctrl + left mouse button"); break;
    default:
        qDebug() << "wtf";
        break;
    }
    setFeedback(msg, 5000);
}

void ImageViewer::edit() {
    if (!imageWidget || ! cropRubberBand)
        return;
    cropRubberBand->hide();
    bool ok;
    QTransform matrix = imageWidget->transformation().inverted(&ok);
    if (!ok) {
        qDebug() << "something's fucked up about the transformation matrix! Not cropping";
        return;
    }
    QPainter p(&origImage);
    p.setTransform(matrix);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect rect = cropRubberBand->geometry();
    switch (m_editMode) {
    case Blackout:
    case Cartouche: {
        QColor c = QColorDialog::getColor(Qt::black, this, tr("Pick a color"));
        if (!c.isValid())
            return setFeedback("", false);
        if (m_editMode == Blackout) {
            p.setPen(Qt::transparent);
            p.setBrush(c);
        } else {
            p.setBrush(Qt::transparent);
            p.setPen(QPen(c, 4));
        }
        const int radius = qMin(rect.width(), rect.height())/2;
        p.drawRoundedRect(rect, radius, radius);
        break;
    }
    case Annotate: {
        QDialog dlg(this);
        QColor c = Qt::black;
        QVBoxLayout *vl = new QVBoxLayout(&dlg);
        QHBoxLayout *hl = new QHBoxLayout;
        QFontComboBox *fonts = new QFontComboBox(&dlg);
        hl->addWidget(fonts);
        QPushButton *cb = new QPushButton(tr("Color"), &dlg);
        connect(cb, &QPushButton::clicked, [=,&c]() { c = QColorDialog::getColor(Qt::black, this, tr("Pick a color")); });
        hl->addWidget(cb);
        vl->addLayout(hl);
        QTextEdit *te = new QTextEdit(&dlg);
        vl->addWidget(te);
        QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(btns);
        dlg.setLayout(vl);
        if (dlg.exec() == QDialog::Rejected)
            return setFeedback("", false);
        QString text = te->toPlainText();
        if (text.isEmpty())
            return setFeedback("", false);
        QFont fnt = fonts->currentFont();
        QSize ts = QFontMetrics(fnt).size(0, text);
        qreal factor = qMin(rect.width() / qreal(ts.width()), rect.height() / qreal(ts.height()));
        if (factor != 1.0)
            fnt.setPointSize(fnt.pointSize()*factor);
        p.setPen(c);
        p.setFont(fnt);
        p.drawText(rect, Qt::AlignCenter, text);
        break;
    }
    default:
        break;
    }
    p.end();
    refresh();
    setFeedback("", false);
    m_edited = true;
    emit imageEdited(true);
}

void ImageViewer::applyCropAndRotation() {
    if (m_editMode != Crop)
        return edit();
    if (!imageWidget || ! cropRubberBand)
        return;

    cropRubberBand->hide();

    QTransform matrix = imageWidget->transformation();
    bool ok;
    // the inverted mapping of the crop area matches the coordinates of the original image
    m_isoCropRect = matrix.inverted(&ok).mapRect(cropRubberBand->geometry());
    if (!ok) {
        qDebug() << "something's fucked up about the transformation matrix! Not cropping";
        return;
    }

    if (!matrix.isRotating()) {
        // … we can just copy the area, later apply flips and be done
        origImage = origImage.copy(m_isoCropRect);
    } else {
        // The rotated case is more involved. The inverted matrix still maps image coordinates
        // but that's not what the user sees or expects.
        //
        // This is inherently lossy because of the pixel transpositon, so special-case it

        const QSize visualSize = imageWidget->imageSize();
        float scale = qMax(float(visualSize.width()) / viewerImage.width(), float(visualSize.height()) / viewerImage.height());
        if (scale <= 0.0) {
            qDebug() << "something is seriously wrong with the scale, not cropping" << scale;
            return;
        }

        // The new image size must be the size of the visible crop area, compensated for the current scale factor
        QImage target(cropRubberBand->geometry().size()/scale, origImage.format());
        // but still be at the same position
        QRect sourceRect = target.rect();
        sourceRect.moveCenter(m_isoCropRect.center());

        target.fill(Qt::black /* Qt::green */);
        QPainter painter(&target);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        // rotate in relation to the paint device
        QPoint center(target.width() / 2, target.height() / 2);
        painter.translate(center);
        // onedirectional flipping inverts the rotation
        if (m_flip == Qt::Horizontal || m_flip == Qt::Vertical)
            painter.rotate(360.0 - imageWidget->rotation());
        else
            painter.rotate(imageWidget->rotation());
        painter.translate(-center);

        // offset by crop rect
        painter.translate(-sourceRect.topLeft());

        // crop
        painter.drawImage(0,0, origImage);
        painter.end();
        origImage = target;
    }

    // apply flip-flop
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    origImage.mirror(m_flip & Qt::Horizontal, m_flip & Qt::Vertical);
#else
    origImage.flip(m_flip);
#endif

    // reset transformations for the new image
    if (!batchMode) {
        m_flip = Qt::Orientations();
        Settings::rotation = 0;
        imageWidget->setRotation(Settings::rotation);
        if (!m_lockZoom) {
            m_zoomMode = ZoomToFit;
        }
        m_isoCropRect = QRect(); // invalidate
    }
    refresh();
    setFeedback("", false);
    setFeedback(tr("New image size: %1x%2").arg(origImage.width()).arg(origImage.height()));
    m_edited = true;
    emit imageEdited(true);
}

bool ImageViewer::flip(Qt::Orientations o) {
    m_flip ^= o;
    if (imageWidget)
        imageWidget->setFlip(m_flip);
    QStringList feedback;
    if (o & Qt::Horizontal)
        feedback << ((m_flip & Qt::Horizontal) ? tr("Flipped Horizontally") : tr("Unflipped Horizontally"));
    if (o & Qt::Vertical)
        feedback << ((m_flip & Qt::Vertical) ? tr("Flipped Vertically") : tr("Unflipped Vertically"));
    setFeedback(feedback.join("\n"));
    return (m_flip & o) == o;
}

void ImageViewer::configureLetterbox() {
    static CropDialog *dlg = nullptr;
    if (!dlg) {
        dlg = new CropDialog(this);
        connect(dlg, &CropDialog::valuesChanged, [=](int left, int top, int right, int bottom) {
            m_letterbox = QRect(QPoint(left, top), QPoint(100-right, 100-bottom));
            imageWidget->setLetterbox(m_letterbox);
        });
    }
    dlg->exec();
}

QSize ImageViewer::currentImageSize() const {
    return origImage.size();
}

void ImageViewer::setMouseMoveData(bool lockMove, int lMouseX, int lMouseY) {
    if (!imageWidget) {
        return;
    }
    moveImageLocked = lockMove;
    mouseX = lMouseX;
    mouseY = lMouseY;
    layoutX = imageWidget->imagePosition().x();
    layoutY = imageWidget->imagePosition().y();
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event) {
    if (!imageWidget) {
        return;
    }

    if (event->modifiers() == Qt::ControlModifier) {
        if (!cropRubberBand || !cropRubberBand->isVisible()) {
            return;
        }
        QRect newRect;
        newRect = QRect(cropOrigin, event->pos());
/*** @todo this doesn't work at all and also the resize typically happens unconstrained using the qsizegrip
        figure whether to keep this at all
        // Force square
        if (event->modifiers() & Qt::ShiftModifier) {
            const int deltaX = cropOrigin.x() - event->pos().x();
            const int deltaY = cropOrigin.y() - event->pos().y();
            newRect.setSize(QSize(-deltaX, deltaY < 0 ? qAbs(deltaX) : -qAbs(deltaX)));
        }
        **/
        cropRubberBand->setGeometry(newRect.normalized());

    } else if (Settings::mouseRotateEnabled || event->modifiers() == Qt::ShiftModifier) {
        QPointF fulcrum(QPointF(imageWidget->width() / 2.0, imageWidget->height() / 2.0));
        QLineF vector(fulcrum, event->position());
        Settings::rotation = initialRotation - vector.angle();
        if (qAbs(Settings::rotation) > 360.0)
            Settings::rotation -= int(360*Settings::rotation)/360;
        if (Settings::rotation < 0)
            Settings::rotation += 360.0;
        imageWidget->setRotation(Settings::rotation);
        setFeedback(tr("Rotation %1°").arg(Settings::rotation));
        // qDebug() << "image center" << fulcrum << "line" << vector << "angle" << vector.angle() << "geom" << imageWidget->geometry();

    } else if (moveImageLocked) {
        int newX = layoutX;
        int newY = layoutY;
        if (Settings::rotation != 0) {
            QPointF fulcrum(QPointF(imageWidget->width() / 2.0, imageWidget->height() / 2.0));
            QLineF vector(fulcrum, event->pos());
            vector.setAngle(vector.angle() + Settings::rotation);
            newX += qRound(vector.x2());
            newY += qRound(vector.y2());
            vector.setP2(QPoint(mouseX, mouseY));
            vector.setAngle(vector.angle() + Settings::rotation);
            newX -= qRound(vector.x2());
            newY -= qRound(vector.y2());
        } else {
            newX += event->pos().x() - mouseX;
            newY += event->pos().y() - mouseY;
        }

        bool needToMove = Settings::rotation != 0;
        if (!needToMove) {
            if (imageWidget->imageSize().width() > size().width()) {
                if (newX > 0) {
                    newX = 0;
                } else if (newX < (size().width() - imageWidget->imageSize().width())) {
                    newX = (size().width() - imageWidget->imageSize().width());
                }
                needToMove = true;
            } else {
                newX = layoutX;
            }

            if (imageWidget->imageSize().height() > size().height()) {
                if (newY > 0) {
                    newY = 0;
                } else if (newY < (size().height() - imageWidget->imageSize().height())) {
                    newY = (size().height() - imageWidget->imageSize().height());
                }
                needToMove = true;
            } else {
                newY = layoutY;
            }
        }

        if (needToMove) {
            imageWidget->setImagePosition(QPoint(newX, newY));
        }
    }
}

void ImageViewer::slideImage(QPoint delta) {
    if (!imageWidget) {
        return;
    }

    QPoint newPos = imageWidget->imagePosition() + delta;
    layoutX = newPos.x();
    layoutY = newPos.y();
    if (Settings::rotation != 0) {
        QPointF fulcrum(QPointF(imageWidget->width() / 2.0, imageWidget->height() / 2.0));
        QLineF vector(QPoint(0,0), delta);
        vector.setAngle(vector.angle() + Settings::rotation);
        newPos = imageWidget->imagePosition() + vector.p2().toPoint();
    }
    bool needToMove = Settings::rotation != 0;
    if (!needToMove) {
        if (imageWidget->imageSize().width() > size().width()) {
            if (newPos.x() > 0) {
                newPos.setX(0);
            } else if (newPos.x() < (size().width() - imageWidget->imageSize().width())) {
                newPos.setX(size().width() - imageWidget->imageSize().width());
            }
            needToMove = true;
        }

        if (imageWidget->imageSize().height() > size().height()) {
            if (newPos.y() > 0) {
                newPos.setY(0);
            } else if (newPos.y() < (size().height() - imageWidget->imageSize().height())) {
                newPos.setY(size().height() - imageWidget->imageSize().height());
            }
            needToMove = true;
        }
    }

    if (needToMove) {
        imageWidget->setImagePosition(newPos);
    }
}

void ImageViewer::saveImage() {
#if __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if EXIV2_TEST_VERSION(0,28,0)
    Exiv2::Image::UniquePtr image;
#else
    Exiv2::Image::AutoPtr image;
#endif
#if __clang__
#pragma GCC diagnostic pop
#endif

    bool exifError = false;
    static bool showExifError = true;

    if (newImage) {
        saveImageAs();
        return;
    }

    setFeedback(tr("Saving..."));

    try {
        image = Exiv2::ImageFactory::open(fullImagePath.toStdString());
        image->readMetadata();
    }
    catch (const Exiv2::Error &error) {
        qWarning() << "EXIV2:" << error.what();
        exifError = true;
    }

    QImageReader imageReader(fullImagePath);
    QString savePath = fullImagePath;
    if (!Settings::saveDirectory.isEmpty()) {
        QDir saveDir(Settings::saveDirectory);
        savePath = saveDir.filePath(QFileInfo(fullImagePath).fileName());
    }
    QTransform matrix;
    if (Settings::exifRotationEnabled) // undo previous exif rotation for saving
        matrix = Metadata::transformation(fullImagePath).inverted();
    int rotation = qRound(Settings::rotation);
    if (!batchMode && (m_flip || !(rotation % 90))) {
        matrix.scale(m_flip & Qt::Horizontal ? -1 : 1, m_flip & Qt::Vertical ? -1 : 1);
        if (!(rotation % 90))
            matrix.rotate((m_flip == Qt::Horizontal || m_flip == Qt::Vertical) ? 360-rotation : rotation);
        viewerImage = viewerImage.transformed(matrix);
    }
    if (!viewerImage.save(savePath, imageReader.format().toUpper(), Settings::defaultSaveQuality)) {
        return MessageBox(this).critical(tr("Error"), tr("Failed to save image."));
    }

    if (!exifError) {
        try {
            if (Settings::saveDirectory.isEmpty()) {
                image->writeMetadata();
            } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if EXIV2_TEST_VERSION(0,28,0)
                Exiv2::Image::UniquePtr imageOut = Exiv2::ImageFactory::open(savePath.toStdString());
#else
                Exiv2::Image::AutoPtr imageOut = Exiv2::ImageFactory::open(savePath.toStdString());
#endif
#pragma clang diagnostic pop

                imageOut->setMetadata(*image);
                Exiv2::ExifThumb thumb(imageOut->exifData());
                thumb.erase();
                // TODO: thumb.setJpegThumbnail(thumbnailPath);
                imageOut->writeMetadata();
            }
        }
        catch (Exiv2::Error &error) {
            if (showExifError) {
                MessageBox msgBox(this);
                QCheckBox cb(tr("Don't show this message again"));
                msgBox.setCheckBox(&cb);
                msgBox.critical(tr("Error"), tr("Failed to save Exif metadata."));
                showExifError = !(cb.isChecked());
            } else {
                //: this is a warning on the console
                qWarning() << tr("Failed to safe Exif metadata:") << error.what();
            }
        }
    }

    m_edited = false;
    reload();
    setFeedback(tr("Image saved."));
    emit imageSaved(savePath);
}

void ImageViewer::saveImageAs() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if EXIV2_TEST_VERSION(0,28,0)
    Exiv2::Image::UniquePtr exifImage;
    Exiv2::Image::UniquePtr newExifImage;
#else
    Exiv2::Image::AutoPtr exifImage;
    Exiv2::Image::AutoPtr newExifImage;
#endif
#pragma clang diagnostic pop

    bool exifError = false;

    setCursorHiding(false);

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save image as"),
                                                    fullImagePath,
                                                    tr("Images") +
                                                    " (*.jpg *.jpeg *.png *.bmp *.tif *.tiff *.ppm *.pgm *.pbm *.xbm *.xpm *.cur *.ico *.icns *.wbmp *.webp)");

    if (!fileName.isEmpty()) {
        try {
            exifImage = Exiv2::ImageFactory::open(fullImagePath.toStdString());
            exifImage->readMetadata();
        }
        catch (const Exiv2::Error &error) {
            qWarning() << "EXIV2" << error.what();
            exifError = true;
        }

        int rotation = qRound(Settings::rotation);
        if (!batchMode && (m_flip || !(rotation % 90))) {
            QTransform matrix;
            matrix.scale(m_flip & Qt::Horizontal ? -1 : 1, m_flip & Qt::Vertical ? -1 : 1);
            if (!(rotation % 90))
                matrix.rotate((m_flip == Qt::Horizontal || m_flip == Qt::Vertical) ? 360-rotation : rotation);
            viewerImage = viewerImage.transformed(matrix);
        }
        if (!viewerImage.save(fileName, 0, Settings::defaultSaveQuality)) {
            MessageBox(this).critical(tr("Error"), tr("Failed to save image."));
        } else {
            if (!exifError) {
                try {
                    newExifImage = Exiv2::ImageFactory::open(fileName.toStdString());
                    newExifImage->setMetadata(*exifImage);
                    newExifImage->writeMetadata();
                }
                catch (Exiv2::Error &error) {
                    exifError = true;
                }
            }
            m_edited = false;
            setFeedback(tr("Image saved."));
        }
    }
    if (window()->isFullScreen()) {
        setCursorHiding(true);
    }
}

void ImageViewer::contextMenuEvent(QContextMenuEvent *) {
//    if (Settings::layoutMode != Phototonic::ImageViewWidget)
//        return;

    while (QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }
    m_contextSpot = mapFromGlobal(QCursor::pos());
    myContextMenu->exec(QCursor::pos());
}

void ImageViewer::focusInEvent(QFocusEvent *event) {
    QScrollArea::focusInEvent(event);
    emit gotFocus();
}

bool ImageViewer::isNewImage() {
    return newImage;
}

void ImageViewer::copyImage() {
    QApplication::clipboard()->setImage(viewerImage);
}

void ImageViewer::pasteImage() {
    if (!imageWidget) {
        return;
    }

    if (!QApplication::clipboard()->image().isNull()) {
        origImage = QApplication::clipboard()->image();
        refresh();
    }
    window()->setWindowTitle(tr("Clipboard") + " - Phototonic");
    if (Settings::setWindowIcon) {
        window()->setWindowIcon(QApplication::windowIcon());
    }
}

void ImageViewer::setBackgroundColor() {
    QPalette pal = palette();
    pal.setColor(backgroundRole(), QColor(Settings::viewerBackgroundColor.red(), Settings::viewerBackgroundColor.green(), Settings::viewerBackgroundColor.blue()));
    setPalette(pal);
}

QPoint ImageViewer::contextSpot() {
    return m_contextSpot;
}

