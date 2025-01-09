/*
 *  Copyright (C) 2013 Ofer Kashayov <oferkv@live.com>
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

#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

class CropRubberBand;
class ImageWidget;
class QMovie;
#include <QLabel>
#include <QPointer>
#include <QScrollArea>
#include <exiv2/exiv2.hpp>

class ImageViewer : public QScrollArea {
Q_OBJECT

public:
    ImageViewer(QWidget *parent);
    bool tempDisableResize;
    bool batchMode = false;
    QString fullImagePath;
    CropRubberBand *cropRubberBand;

    enum ZoomMethods {
        Disable = 0,
        WidthAndHeight,
        Width,
        Height,
        Disprop
    };

    enum MirrorLayout {
        MirrorNone = 0,
        MirrorDual,
        MirrorTriple,
        MirrorQuad,
        MirrorVDual
    };

    void clearImage();
    bool isNewImage();
    void loadImage(QString imageFileName, const QImage &preview = QImage());
    void refresh();
    void reload();
    void resizeImage(QPoint focus = QPoint(-1, -1));
    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void setBackgroundColor();
    void setContextMenu(QMenu *);
    void setCursorHiding(bool hide);
    void setFeedback(QString feedbackString, bool timeLimited = true);
    void setInfo(QString infoString);
    void setMirror(MirrorLayout layout);
    void showFileName(bool yesno) { myFilenameLabel->setVisible(yesno); }
    void slideImage(QPoint delta);

    int getImageWidthPreCropped();
    int getImageHeightPreCropped();
    QPoint contextSpot();

signals:
    void toolsUpdated();

public slots:

    void monitorCursorState();
    void saveImage();
    void saveImageAs();
    void copyImage();
    void pasteImage();
    void applyCropAndRotation();

private slots:

    void unsetFeedback();
    void updateRubberBandFeedback(QRect geom);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);

private:
    QMenu *myContextMenu;
    QLabel *myFilenameLabel;
    MirrorLayout myMirrorLayout;
    QLabel *movieWidget = nullptr;
    ImageWidget *imageWidget = nullptr;
    QImage origImage;
    QImage viewerImage;
    QImage mirrorImage;
    QTimer *mouseMovementTimer;
    QPointer<QMovie> animation;
    bool newImage;
    bool cursorIsHidden;
    bool moveImageLocked;
    qreal initialRotation = 0;
    int mouseX;
    int mouseY;
    int layoutX;
    int layoutY;
    QLabel *feedbackLabel;
    QPoint cropOrigin;
    QPoint m_contextSpot;

    void setMouseMoveData(bool lockMove, int lMouseX, int lMouseY);

    void centerImage(QSize &imgSize);

    void transform();

    void mirror();

    void colorize();
    void setImage(const QImage &image);
};

#endif // IMAGE_VIEWER_H

