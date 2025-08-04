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
#include <QTransform>
#include <exiv2/exiv2.hpp>

class ImageViewer : public QScrollArea {
Q_OBJECT

public:
    ImageViewer(QWidget *parent);
    bool batchMode = false;
    QString fullImagePath;

    enum ZoomMode {
        ZoomOriginal = 0,
        ZoomToFit,
        ZoomToFill
    };
    enum Edit {
        Crop = 0,
        Blackout,
        Cartouche,
        Annotate
    };

    void clearImage();
    void configureLetterbox();
    bool crossfade() const { return m_crossfade; }
    QSize currentImageSize() const;
    void edit();
    bool isNewImage();
    QRect lastCropGeometry() const { return m_isoCropRect; }
    void loadImage(QString imageFileName, const QImage &preview = QImage());
    void preload(QString imageFileName);
    void refresh();
    void resizeImage(QPoint focus = QPoint(-1, -1));
    void scaleImage(QSize newSize);
    void secureEdits();
    void setBackgroundColor();
    void setContextMenu(QMenu *);
    void setCrossfade(bool yesno);
    void setCursorHiding(bool hide);
    void setEditMode(Edit mode);
    void setFeedback(QString feedbackString, int timeLimited = 3000);
    void setInfo(QString infoString);
    void showFileName(bool yesno) { myFilenameLabel->setVisible(yesno); }
    void slideImage(QPoint delta);
    float zoom() const { return m_zoom; }
    ZoomMode zoomMode() const { return m_zoomMode; }
    void zoomTo(ZoomMode mode, QPoint focus = QPoint(-1,-1));
    void zoomTo(float percent, QPoint focus = QPoint(-1,-1));

    QPoint contextSpot();

signals:
    void gotFocus();
    void toolsUpdated();
    void imageEdited(bool);
    void imageSaved(const QString &imageFileName);

public slots:
    void applyCropAndRotation();
    void lockZoom(bool);
    void monitorCursorState();
    void saveImage();
    void saveImageAs();
    void copyImage();
    void pasteImage();
    void reload();

private slots:

    void unsetFeedback();
    void updateRubberBandFeedback(QRect geom);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void focusInEvent(QFocusEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);

private:
    QMenu *myContextMenu;
    QLabel *myFilenameLabel;
    QLabel *movieWidget = nullptr;
    ImageWidget *imageWidget = nullptr;
    QImage origImage;
    QImage viewerImage;
    QImage m_preloadedImage;
    QString m_preloadedPath;
    QString m_preloadPath;
    bool m_loading;
    bool m_crossfade;
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
    QTransform m_exifTransformation;
    QString m_permanentFeedback;
    QRect m_letterbox;
    CropRubberBand *cropRubberBand;
    QRect m_isoCropRect;
    ZoomMode m_zoomMode;
    float m_zoom;
    bool m_lockZoom;
    Edit m_editMode;
    bool m_edited;

    void setMouseMoveData(bool lockMove, int lMouseX, int lMouseY);

    void centerImage(QSize imgSize);

    void colorize();
    void colorize(uchar*, int, int, int, const unsigned char(*)[256], const unsigned char(*)[256]); // thread helper
    void setImage(const QImage &image);
};

#endif // IMAGE_VIEWER_H

