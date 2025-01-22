/*
 *  Copyright (C) 2013-2015 Ofer Kashayov <oferkv@live.com>
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

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QMovie>
#include <QProcess>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSettings>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QThreadPool>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>

#include "Bookmarks.h"
#include "CopyMoveDialog.h"
#include "CopyMoveToDialog.h"
#include "ColorsDialog.h"
#include "DirCompleter.h"
#include "ExternalAppsDialog.h"
#include "FileListWidget.h"
#include "FileSystemTree.h"
#include "GuideWidget.h"
#include "IconProvider.h"
#include "ImageViewer.h"
#include "InfoViewer.h"
#include "MessageBox.h"
#include "MetadataCache.h"
#include "Phototonic.h"
#include "ProgressDialog.h"
#include "RangeInputDialog.h"
#include "RenameDialog.h"
#include "ResizeDialog.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Tags.h"
#include "ThumbsViewer.h"
#include "Trashcan.h"

#include <exiv2/exiv2.hpp>

Phototonic::Phototonic(QStringList argumentsList, int filesStartAt, QWidget *parent) : QMainWindow(parent) {
    Settings::appSettings = new QSettings("phototonic", "phototonic");

    fileSystemModel = new QFileSystemModel(this);
    fileSystemModel->setFilter(QDir::AllDirs | QDir::Dirs | QDir::NoDotAndDotDot);
    fileSystemModel->setIconProvider(new IconProvider);

    setDockOptions(QMainWindow::AllowNestedDocks);
    readSettings();
    createThumbsViewer();
    createActions();
    myMainMenu = new QMenu(this);
    createToolBars();
    statusBar()->setVisible(false);
    createFileSystemDock();
    createBookmarksDock();
    createImagePreviewDock();
    createImageTagsDock();
    setupDocks();
    createMenus();
    createImageViewer();
    updateExternalApps();
    loadShortcuts();

    m_statusLabel = new QLabel(this);
    m_statusLabel->hide();
    m_statusLabel->setMargin(3);
    m_statusLabel->setAutoFillBackground(true);
    m_statusLabel->setFrameStyle(QFrame::Plain|QFrame::NoFrame);
    QPalette pal = m_statusLabel->palette();
    pal.setColor(m_statusLabel->backgroundRole(), QColor(0,0,0,192));
    pal.setColor(m_statusLabel->foregroundRole(), QColor(255,255,255,192));
    m_statusLabel->setPalette(pal);

    connect (thumbsViewer, &ThumbsViewer::currentIndexChanged, [=](const QModelIndex &current) {
        if (!current.isValid())
            return;
        if (imageViewer->isVisible()) {
            imageViewer->loadImage(thumbsViewer->fullPathOf(current.row()),
                                   Settings::slideShowActive ? QImage() : thumbsViewer->icon(current.row()).pixmap(THUMB_SIZE_MAX).toImage());
            if (Settings::layoutMode == ImageViewWidget)
                setImageViewerWindowTitle();
            if (feedbackImageInfoAction->isChecked()) {
                QApplication::processEvents();
                m_infoViewer->read(imageViewer->fullImagePath);
                imageViewer->setFeedback(m_infoViewer->html(), false);
            }
        }
        if (m_infoViewer->isVisible()) {
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            m_infoViewer->hint(tr("Average brightness"),
                               QString::number(thumbModel->item(current.row())->data(ThumbsViewer::BrightnessRole).toReal(), 'f', 2));
            m_infoViewer->read(thumbsViewer->fullPathOf(current.row()));
        }
        });
    connect(qApp, SIGNAL(focusChanged(QWidget * , QWidget * )), this, SLOT(updateActions()));

    restoreGeometry(Settings::value(Settings::optionGeometry).toByteArray());
    restoreState(Settings::value(Settings::optionWindowState).toByteArray());
    QApplication::setWindowIcon(QIcon(":/images/phototonic.png"));

    stackedLayout = new QStackedLayout;
    QWidget *stackedLayoutWidget = new QWidget;
    stackedLayout->addWidget(thumbsViewer);
    stackedLayout->addWidget(imageViewer);
    stackedLayoutWidget->setLayout(stackedLayout);
    setCentralWidget(stackedLayoutWidget);
    processStartupArguments(argumentsList, filesStartAt);
    if (Settings::currentDirectory.isEmpty())
        Settings::currentDirectory = QDir::currentPath();

    copyMoveToDialog = nullptr;
    colorsDialog = nullptr;
    initComplete = true;
    m_deleteInProgress = false;
    currentHistoryIdx = -1;
    needHistoryRecord = true;
    interfaceDisabled = false;

    refreshThumbs(true);
    if (Settings::layoutMode == ThumbViewWidget) {
        thumbsViewer->setFocus(Qt::OtherFocusReason);
    }
}

void Phototonic::processStartupArguments(QStringList argumentsList, int filesStartAt) {
    if (argumentsList.size() > filesStartAt) {
        QFileInfo firstArgument(argumentsList.at(filesStartAt));
        if (firstArgument.isDir()) {
            // Confusingly we need the absoluteFile and not absolutePath if it's a directory
            Settings::currentDirectory = firstArgument.absoluteFilePath();
        } else if (argumentsList.size() > filesStartAt + 1) {
            loadStartupFileList(argumentsList, filesStartAt);
            return;
        } else {
            Settings::currentDirectory = firstArgument.absolutePath();
            const QString cliFileName = Settings::currentDirectory + QDir::separator() + firstArgument.fileName();
            if (QFile::exists(cliFileName)) {
                showViewer();
                imageViewer->loadImage(cliFileName);
                thumbsViewer->setCurrentIndex(cliFileName);
                setWindowTitle(cliFileName + " - Phototonic");
            } else {
                MessageBox(this).critical(tr("Error"), tr("Failed to open file %1, file not found.").arg(cliFileName));
            }
        }
    } else {
        if (Settings::startupDir == Settings::SpecifiedDir) {
            Settings::currentDirectory = Settings::specifiedStartDir;
        } else if (Settings::startupDir == Settings::RememberLastDir) {
            Settings::currentDirectory = Settings::value(Settings::optionLastDir).toString();
        }
    }
    selectCurrentViewDir();
}

void Phototonic::loadStartupFileList(QStringList argumentsList, int filesStartAt) {
    Settings::filesList.clear();
    for (int i = filesStartAt; i < argumentsList.size(); i++) {
        QFile currentFileFullPath(argumentsList[i]);
        QFileInfo currentFileInfo(currentFileFullPath);

        if (!Settings::filesList.contains(currentFileInfo.absoluteFilePath())) {
            Settings::filesList << currentFileInfo.absoluteFilePath();
        }
    }
    fileSystemTree->clearSelection();
    fileListWidget->show();
    fileListWidget->itemAt(0, 0)->setSelected(true);
    Settings::isFileListLoaded = true;
}

bool Phototonic::event(QEvent *event) {
    if (event->type() == QEvent::ActivationChange ||
        (Settings::layoutMode == ThumbViewWidget && event->type() == QEvent::MouseButtonRelease)) {
        thumbsViewer->loadVisibleThumbs();
    }

    return QMainWindow::event(event);
}

void Phototonic::createThumbsViewer() {
    thumbsViewer = new ThumbsViewer(this);
    thumbsViewer->installEventFilter(this);
    thumbsViewer->viewport()->installEventFilter(this);
    thumbsViewer->thumbsSortFlags = (QDir::SortFlags) Settings::value(
            Settings::optionThumbsSortFlags).toInt();
    thumbsViewer->thumbsSortFlags |= QDir::IgnoreCase;

    connect(thumbsViewer->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this, SLOT(updateActions()));
    connect (thumbsViewer, &ThumbsViewer::status, this, &Phototonic::setStatus);
    connect (thumbsViewer, &ThumbsViewer::progress, [=](unsigned int v, unsigned int t) {
                m_progressBar->setMaximum(t);
                m_progressBar->setValue(v);
            });
    connect (thumbsViewer, &ThumbsViewer::doubleClicked, this, &Phototonic::loadSelectedThumbImage);

    imageInfoDock = new QDockWidget(tr("Image Info"), this);
    imageInfoDock->setObjectName("Image Info");
    m_infoViewer = new InfoView(this);
    imageInfoDock->setWidget(m_infoViewer);
    connect(imageInfoDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::imageInfoDockVisible = visible;
        }
        if (visible) {
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            int currentRow = thumbsViewer->currentIndex().row();
            if (currentRow > -1) {
                m_infoViewer->hint(tr("Average brightness"),
                                    QString::number(thumbModel->item(currentRow)->data(ThumbsViewer::BrightnessRole).toReal(), 'f', 2));
                m_infoViewer->read(thumbsViewer->fullPathOf(currentRow));
            }
        }
    } );
}

void Phototonic::createImageViewer() {
    imageViewer = new ImageViewer(this);
    imageViewer->viewport()->installEventFilter(this);
    connect(saveAction, SIGNAL(triggered()), imageViewer, SLOT(saveImage()));
    connect(saveAsAction, SIGNAL(triggered()), imageViewer, SLOT(saveImageAs()));
    connect(copyImageAction, SIGNAL(triggered()), imageViewer, SLOT(copyImage()));
    connect(pasteImageAction, SIGNAL(triggered()), imageViewer, SLOT(pasteImage()));
    connect(imageViewer, &ImageViewer::toolsUpdated, [=](){ rotateToolAction->setChecked(Settings::mouseRotateEnabled); });
    QMenu *contextMenu = new QMenu(imageViewer);

    // Widget actions
    imageViewer->addAction(slideShowAction);
    imageViewer->addAction(nextImageAction);
    imageViewer->addAction(prevImageAction);
    imageViewer->addAction(firstImageAction);
    imageViewer->addAction(lastImageAction);
    imageViewer->addAction(randomImageAction);
    imageViewer->addAction(zoomInAction);
    imageViewer->addAction(zoomOutAction);
    imageViewer->addAction(origZoomAction);
    imageViewer->addAction(resetZoomAction);
    imageViewer->addAction(rotateRightAction);
    imageViewer->addAction(rotateLeftAction);
    imageViewer->addAction(freeRotateRightAction);
    imageViewer->addAction(freeRotateLeftAction);
    imageViewer->addAction(flipHorizontalAction);
    imageViewer->addAction(flipVerticalAction);
    imageViewer->addAction(cropAction);
    imageViewer->addAction(resizeAction);
    imageViewer->addAction(saveAction);
    imageViewer->addAction(saveAsAction);
    imageViewer->addAction(copyImageAction);
    imageViewer->addAction(pasteImageAction);
    imageViewer->addAction(deleteAction);
    imageViewer->addAction(deletePermanentlyAction);
    imageViewer->addAction(renameAction);
    imageViewer->addAction(CloseImageAction);
    imageViewer->addAction(fullScreenAction);
    imageViewer->addAction(settingsAction);
    imageViewer->addAction(mirrorDisabledAction);
    imageViewer->addAction(mirrorDualAction);
    imageViewer->addAction(mirrorTripleAction);
    imageViewer->addAction(mirrorDualVerticalAction);
    imageViewer->addAction(mirrorQuadAction);
    imageViewer->addAction(keepTransformAction);
    imageViewer->addAction(keepZoomAction);
    imageViewer->addAction(refreshAction);
    imageViewer->addAction(colorsAction);
    imageViewer->addAction(moveRightAction);
    imageViewer->addAction(moveLeftAction);
    imageViewer->addAction(moveUpAction);
    imageViewer->addAction(moveDownAction);
    imageViewer->addAction(showClipboardAction);
    imageViewer->addAction(copyToAction);
    imageViewer->addAction(moveToAction);
    imageViewer->addAction(resizeAction);
    imageViewer->addAction(viewImageAction);
    imageViewer->addAction(exitAction);
    imageViewer->addAction(showViewerToolbarAction);
    imageViewer->addAction(externalAppsAction);
    imageViewer->addAction(feedbackImageInfoAction);

    // Actions
    contextMenu->addAction(m_wallpaperAction);
    contextMenu->addAction(openWithMenuAction);
    contextMenu->addAction(feedbackImageInfoAction);
    contextMenu->addSeparator();

    QMenu *menu = contextMenu->addMenu(tr("Navigate"));
    menu->addAction(nextImageAction);
    menu->addAction(prevImageAction);
    menu->addAction(firstImageAction);
    menu->addAction(lastImageAction);
    menu->addAction(randomImageAction);
    menu->addAction(slideShowAction);

    menu = contextMenu->addMenu(tr("Transform"));

    QMenu *submenu = menu->addMenu(QIcon::fromTheme("edit-find", QIcon(":/images/zoom.png")), tr("Zoom"));
    submenu->addAction(zoomInAction);
    submenu->addAction(zoomOutAction);
    submenu->addAction(origZoomAction);
    submenu->addAction(resetZoomAction);

    submenu->addAction(keepZoomAction);

    submenu = menu->addMenu(tr("Flip and Flop and Rotate"));
    submenu->addAction(flipHorizontalAction);
    submenu->addAction(flipVerticalAction);
    submenu->addSeparator();
    submenu->addAction(rotateRightAction);
    submenu->addAction(rotateLeftAction);
    submenu->addAction(freeRotateRightAction);
    submenu->addAction(freeRotateLeftAction);
    submenu->addSeparator();
    submenu->addAction(keepTransformAction);


    menu = contextMenu->addMenu(tr("Edit"));
    menu->addAction(resizeAction);
    menu->addAction(colorsAction);
    submenu = menu->addMenu(tr("Mirror"));
    QActionGroup *group = new QActionGroup(submenu);
    group->addAction(mirrorDisabledAction);
    group->addAction(mirrorDualAction);
    group->addAction(mirrorTripleAction);
    group->addAction(mirrorDualVerticalAction);
    group->addAction(mirrorQuadAction);
    submenu->addActions(group->actions());

    menu = contextMenu->addMenu(tr("File"));
    menu->addAction(copyToAction);
    menu->addAction(moveToAction);
    menu->addAction(saveAction);
    menu->addAction(saveAsAction);
    menu->addAction(renameAction);
    menu->addSeparator();
    menu->addAction(deleteAction);
    menu->addAction(deletePermanentlyAction);


    menu = contextMenu->addMenu(tr("View"));
    menu->addAction(fullScreenAction);
    menu->addSeparator();
    menu->addAction(cropAction);

    //: The guides a lines across the image for orientation
    submenu = menu->addMenu(tr("Guides"));
    QAction *act = new QAction(tr("Add vertical guide"), submenu);
    connect(act, &QAction::triggered, [=]() { new GuideWidget(imageViewer, Qt::Vertical, imageViewer->contextSpot().x()); });
    submenu->addAction(act);
    act = new QAction(tr("Add horizontal guide"), submenu);
    connect(act, &QAction::triggered, [=]() { new GuideWidget(imageViewer, Qt::Horizontal, imageViewer->contextSpot().y()); });
    submenu->addAction(act);

    menu->addSeparator();
    menu->addAction(showViewerToolbarAction);
    menu->addAction(showClipboardAction);
    menu->addSeparator();
    menu->addAction(refreshAction);

    contextMenu->addSeparator();

    contextMenu->addAction(copyImageAction);
    contextMenu->addAction(pasteImageAction);
    contextMenu->addSeparator();
    contextMenu->addAction(CloseImageAction);

    imageViewer->setContextMenuPolicy(Qt::DefaultContextMenu);
    imageViewer->setContextMenu(contextMenu);
    Settings::isFullScreen = Settings::value(Settings::optionFullScreenMode).toBool();
    fullScreenAction->setChecked(Settings::isFullScreen);
}

void Phototonic::createActions() {
    thumbsGoToTopAction = new QAction(tr("Top"), this);
    thumbsGoToTopAction->setObjectName("thumbsGoTop");
    thumbsGoToTopAction->setIcon(QIcon::fromTheme("go-top", QIcon(":/images/top.png")));
    connect(thumbsGoToTopAction, &QAction::triggered, thumbsViewer, &ThumbsViewer::scrollToTop);

    thumbsGoToBottomAction = new QAction(tr("Bottom"), this);
    thumbsGoToBottomAction->setObjectName("thumbsGoBottom");
    thumbsGoToBottomAction->setIcon(QIcon::fromTheme("go-bottom", QIcon(":/images/bottom.png")));
    connect(thumbsGoToBottomAction, &QAction::triggered, thumbsViewer, &ThumbsViewer::scrollToBottom);

    CloseImageAction = new QAction(tr("Close Viewer"), this);
    CloseImageAction->setObjectName("closeImage");
    connect(CloseImageAction, SIGNAL(triggered()), this, SLOT(hideViewer()));

    fullScreenAction = new QAction(tr("Full Screen"), this);
    fullScreenAction->setObjectName("fullScreen");
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    settingsAction = new QAction(tr("Preferences"), this);
    settingsAction->setObjectName("settings");
    settingsAction->setIcon(QIcon::fromTheme("preferences-system", QIcon(":/images/settings.png")));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(showSettings()));

    exitAction = new QAction(tr("Exit"), this);
    exitAction->setObjectName("exit");
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    thumbsZoomInAction = new QAction(tr("Enlarge Thumbnails"), this);
    thumbsZoomInAction->setObjectName("thumbsZoomIn");
    connect(thumbsZoomInAction, SIGNAL(triggered()), this, SLOT(thumbsZoomIn()));
    thumbsZoomInAction->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/images/zoom_in.png")));
    if (thumbsViewer->thumbSize == THUMB_SIZE_MAX) {
        thumbsZoomInAction->setEnabled(false);
    }

    thumbsZoomOutAction = new QAction(tr("Shrink Thumbnails"), this);
    thumbsZoomOutAction->setObjectName("thumbsZoomOut");
    connect(thumbsZoomOutAction, SIGNAL(triggered()), this, SLOT(thumbsZoomOut()));
    thumbsZoomOutAction->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/images/zoom_out.png")));
    if (thumbsViewer->thumbSize == THUMB_SIZE_MIN) {
        thumbsZoomOutAction->setEnabled(false);
    }

    cutAction = new QAction(tr("Cut"), this);
    cutAction->setObjectName("cut");
    cutAction->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/images/cut.png")));
    connect(cutAction, &QAction::triggered, [=]() { copyOrCutThumbs(false); });
    cutAction->setEnabled(false);

    copyAction = new QAction(tr("Copy"), this);
    copyAction->setObjectName("copy");
    copyAction->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/images/copy.png")));
    connect(copyAction, &QAction::triggered, [=]() { copyOrCutThumbs(true); });
    copyAction->setEnabled(false);

    setClassicThumbsAction = new QAction(tr("Show classic thumbnails"), this);
    setClassicThumbsAction->setCheckable(true);
    setClassicThumbsAction->setChecked(Settings::thumbsLayout == ThumbsViewer::Classic);
    setClassicThumbsAction->setObjectName("setClassicThumbs");
    connect(setClassicThumbsAction, &QAction::triggered, [=](){ Settings::thumbsLayout = ThumbsViewer::Classic; refreshThumbs(false); });

    setSquareThumbsAction = new QAction(tr("Show square thumbnails"), this);
    setSquareThumbsAction->setCheckable(true);
    setSquareThumbsAction->setChecked(Settings::thumbsLayout == ThumbsViewer::Squares);
    setSquareThumbsAction->setObjectName("setSquareThumbs");
    connect(setSquareThumbsAction, &QAction::triggered, [=](){ Settings::thumbsLayout = ThumbsViewer::Squares; refreshThumbs(false); });

    setCompactThumbsAction = new QAction(tr("Show compact thumbnails"), this);
    setCompactThumbsAction->setCheckable(true);
    setCompactThumbsAction->setChecked(Settings::thumbsLayout == ThumbsViewer::Compact);
    setCompactThumbsAction->setObjectName("setCompactThumbs");
    connect(setCompactThumbsAction, &QAction::triggered, [=](){ Settings::thumbsLayout = ThumbsViewer::Compact; refreshThumbs(false); });

    copyToAction = new QAction(tr("Copy to..."), this);
    copyToAction->setObjectName("copyTo");
    connect(copyToAction, &QAction::triggered, [=]() { copyOrMoveImages(true); });

    moveToAction = new QAction(tr("Move to..."), this);
    moveToAction->setObjectName("moveTo");
    connect(moveToAction, &QAction::triggered, [=]() { copyOrMoveImages(false); });

    deleteAction = new QAction(tr("Move to Trash"), this);
    deleteAction->setObjectName("moveToTrash");
    deleteAction->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteOperation()));

    deletePermanentlyAction = new QAction(tr("Delete"), this);
    deletePermanentlyAction->setObjectName("delete");
    deletePermanentlyAction->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));
    connect(deletePermanentlyAction, SIGNAL(triggered()), this, SLOT(deletePermanentlyOperation()));

    saveAction = new QAction(tr("Save"), this);
    saveAction->setObjectName("save");
    saveAction->setIcon(QIcon::fromTheme("document-save", QIcon(":/images/save.png")));

    saveAsAction = new QAction(tr("Save As"), this);
    saveAsAction->setObjectName("saveAs");
    saveAsAction->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/images/save_as.png")));

    copyImageAction = new QAction(tr("Copy Image"), this);
    copyImageAction->setObjectName("copyImage");
    pasteImageAction = new QAction(tr("Paste Image"), this);
    pasteImageAction->setObjectName("pasteImage");

    renameAction = new QAction(tr("Rename"), this);
    renameAction->setObjectName("rename");
    connect(renameAction, SIGNAL(triggered()), this, SLOT(rename()));

    removeMetadataAction = new QAction(tr("Remove Metadata"), this);
    removeMetadataAction->setObjectName("removeMetadata");
    connect(removeMetadataAction, SIGNAL(triggered()), this, SLOT(removeMetadata()));

    selectAllAction = new QAction(tr("Select All"), this);
    selectAllAction->setObjectName("selectAll");
    connect(selectAllAction, SIGNAL(triggered()), this, SLOT(selectAllThumbs()));

    selectByBrightnesAction = new QAction(tr("Select by Brightness"), this);
    selectByBrightnesAction->setObjectName("selectByBrightness");
    connect(selectByBrightnesAction, SIGNAL(triggered()), this, SLOT(selectByBrightness()));

    aboutAction = new QAction(tr("About"), this);
    aboutAction->setObjectName("about");
    connect(aboutAction, &QAction::triggered, [=](){MessageBox(this).about();});

    // Sort actions
    sortByNameAction = new QAction(tr("Sort by Name"), this);
    sortByNameAction->setObjectName("name");
    sortByTimeAction = new QAction(tr("Sort by Time"), this);
    sortByTimeAction->setObjectName("time");
    sortBySizeAction = new QAction(tr("Sort by Size"), this);
    sortBySizeAction->setObjectName("size");
    sortByTypeAction = new QAction(tr("Sort by Type"), this);
    sortByTypeAction->setObjectName("type");
    sortBySimilarityAction = new QAction(tr("Sort by Similarity"), this);
    sortBySimilarityAction->setObjectName("similarity");
    sortReverseAction = new QAction(tr("Reverse Sort Order"), this);
    sortReverseAction->setObjectName("reverse");
    sortByNameAction->setCheckable(true);
    sortByTimeAction->setCheckable(true);
    sortBySizeAction->setCheckable(true);
    sortByTypeAction->setCheckable(true);
    sortBySimilarityAction->setCheckable(true);
    sortReverseAction->setCheckable(true);
    connect(sortByNameAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(sortByTimeAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(sortBySizeAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(sortByTypeAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(sortBySimilarityAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    connect(sortReverseAction, SIGNAL(triggered()), this, SLOT(sortThumbnails()));

    if (thumbsViewer->thumbsSortFlags & QDir::Time) {
        sortByTimeAction->setChecked(true);
    } else if (thumbsViewer->thumbsSortFlags & QDir::Size) {
        sortBySizeAction->setChecked(true);
    } else if (thumbsViewer->thumbsSortFlags & QDir::Type) {
        sortByTypeAction->setChecked(true);
    } else {
        sortByNameAction->setChecked(true);
    }
    sortReverseAction->setChecked(thumbsViewer->thumbsSortFlags & QDir::Reversed);

    showHiddenFilesAction = new QAction(tr("Show Hidden Files"), this);
    showHiddenFilesAction->setObjectName("showHidden");
    showHiddenFilesAction->setCheckable(true);
    showHiddenFilesAction->setChecked(Settings::showHiddenFiles);
    connect(showHiddenFilesAction, SIGNAL(triggered()), this, SLOT(showHiddenFiles()));

    smallToolbarIconsAction = new QAction(tr("Small Toolbar Icons"), this);
    smallToolbarIconsAction->setObjectName("smallToolbarIcons");
    smallToolbarIconsAction->setCheckable(true);
    smallToolbarIconsAction->setChecked(Settings::smallToolbarIcons);
    connect(smallToolbarIconsAction, SIGNAL(triggered()), this, SLOT(setToolbarIconSize()));

    lockDocksAction = new QAction(tr("Hide Dock Title Bars"), this);
    lockDocksAction->setObjectName("lockDocks");
    lockDocksAction->setCheckable(true);
    lockDocksAction->setChecked(Settings::hideDockTitlebars);
    connect(lockDocksAction, SIGNAL(triggered()), this, SLOT(lockDocks()));

    showViewerToolbarAction = new QAction(tr("Show Toolbar"), this);
    showViewerToolbarAction->setObjectName("showViewerToolbars");
    showViewerToolbarAction->setCheckable(true);
    showViewerToolbarAction->setChecked(Settings::showViewerToolbar);
    connect(showViewerToolbarAction, &QAction::triggered, [=]() {
        Settings::showViewerToolbar = showViewerToolbarAction->isChecked();
        imageToolBar->setVisible(Settings::showViewerToolbar);
        addToolBar(imageToolBar);
    });

    refreshAction = new QAction(tr("Reload"), this);
    refreshAction->setObjectName("refresh");
    refreshAction->setIcon(QIcon::fromTheme("view-refresh", QIcon(":/images/refresh.png")));
    connect(refreshAction, SIGNAL(triggered()), this, SLOT(reload()));

    includeSubDirectoriesAction = new QAction(tr("Include Sub-directories"), this);
    includeSubDirectoriesAction->setObjectName("subFolders");
    includeSubDirectoriesAction->setIcon(QIcon(":/images/tree.png"));
    includeSubDirectoriesAction->setCheckable(true);
    connect(includeSubDirectoriesAction, SIGNAL(triggered()), this, SLOT(setIncludeSubDirs()));

    pasteAction = new QAction(tr("Paste Here"), this);
    pasteAction->setObjectName("paste");
    pasteAction->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/images/paste.png")));
    connect(pasteAction, SIGNAL(triggered()), this, SLOT(pasteThumbs()));
    pasteAction->setEnabled(false);

    createDirectoryAction = new QAction(tr("New Directory"), this);
    createDirectoryAction->setObjectName("createDir");
    connect(createDirectoryAction, SIGNAL(triggered()), this, SLOT(createSubDirectory()));
    createDirectoryAction->setIcon(QIcon::fromTheme("folder-new", QIcon(":/images/new_folder.png")));

    setSaveDirectoryAction = new QAction(tr("Set Save Directory"), this);
    setSaveDirectoryAction->setObjectName("setSaveDir");
    connect(setSaveDirectoryAction, SIGNAL(triggered()), this, SLOT(setSaveDirectory()));
    setSaveDirectoryAction->setIcon(QIcon::fromTheme("folder-visiting", QIcon(":/images/folder-visiting.png")));

    goBackAction = new QAction(tr("Back"), this);
    goBackAction->setObjectName("goBack");
    goBackAction->setIcon(QIcon::fromTheme("go-previous", QIcon(":/images/back.png")));
    connect(goBackAction, SIGNAL(triggered()), this, SLOT(goBack()));
    goBackAction->setEnabled(false);

    goFrwdAction = new QAction(tr("Forward"), this);
    goFrwdAction->setObjectName("goFrwd");
    goFrwdAction->setIcon(QIcon::fromTheme("go-next", QIcon(":/images/next.png")));
    connect(goFrwdAction, SIGNAL(triggered()), this, SLOT(goForward()));
    goFrwdAction->setEnabled(false);

    goUpAction = new QAction(tr("Go Up"), this);
    goUpAction->setObjectName("up");
    goUpAction->setIcon(QIcon::fromTheme("go-up", QIcon(":/images/up.png")));
    connect(goUpAction, &QAction::triggered, [=](){ goTo(QFileInfo(Settings::currentDirectory).dir().absolutePath()); });

    goHomeAction = new QAction(tr("Home"), this);
    goHomeAction->setObjectName("home");
    connect(goHomeAction, &QAction::triggered, [=](){ goTo(QDir::homePath()); });
    goHomeAction->setIcon(QIcon::fromTheme("go-home", QIcon(":/images/home.png")));

    slideShowAction = new QAction(tr("Slide Show"), this);
    slideShowAction->setObjectName("toggleSlideShow");
    connect(slideShowAction, SIGNAL(triggered()), this, SLOT(toggleSlideShow()));
    slideShowAction->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/images/play.png")));

    nextImageAction = new QAction(tr("Next Image"), this);
    nextImageAction->setObjectName("nextImage");
    nextImageAction->setIcon(QIcon::fromTheme("go-next", QIcon(":/images/next.png")));
    connect(nextImageAction, &QAction::triggered, [=](){ loadImage(Phototonic::Next); });

    prevImageAction = new QAction(tr("Previous Image"), this);
    prevImageAction->setObjectName("prevImage");
    prevImageAction->setIcon(QIcon::fromTheme("go-previous", QIcon(":/images/back.png")));
    connect(prevImageAction, &QAction::triggered, [=](){ loadImage(Phototonic::Previous); });

    firstImageAction = new QAction(tr("First Image"), this);
    firstImageAction->setObjectName("firstImage");
    firstImageAction->setIcon(QIcon::fromTheme("go-first", QIcon(":/images/first.png")));
    connect(firstImageAction, &QAction::triggered, [=](){ loadImage(Phototonic::First); });

    lastImageAction = new QAction(tr("Last Image"), this);
    lastImageAction->setObjectName("lastImage");
    lastImageAction->setIcon(QIcon::fromTheme("go-last", QIcon(":/images/last.png")));
    connect(lastImageAction, &QAction::triggered, [=](){ loadImage(Phototonic::Last); });

    randomImageAction = new QAction(tr("Random Image"), this);
    randomImageAction->setObjectName("randomImage");
    connect(randomImageAction, &QAction::triggered, [=](){ loadImage(Phototonic::Random); });

    viewImageAction = new QAction(tr("View Image"), this);
    viewImageAction->setObjectName("open");
    viewImageAction->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
    connect(viewImageAction, SIGNAL(triggered()), this, SLOT(viewImage()));

    showClipboardAction = new QAction(tr("Load Clipboard"), this);
    showClipboardAction->setObjectName("showClipboard");
    showClipboardAction->setIcon(QIcon::fromTheme("insert-image", QIcon(":/images/new.png")));
    connect(showClipboardAction, SIGNAL(triggered()), this, SLOT(newImage()));

    m_wallpaperAction = new QAction(tr("Set Wallpaper"));
    m_wallpaperAction->setObjectName("setwallpaper");
    connect(m_wallpaperAction, &QAction::triggered, this, &Phototonic::runExternalApp);

    openWithSubMenu = new QMenu(tr("Open With..."));
    openWithMenuAction = new QAction(tr("Open With..."), this);
    openWithMenuAction->setObjectName("openWithMenu");
    openWithMenuAction->setMenu(openWithSubMenu);
    externalAppsAction = new QAction(tr("External Applications"), this);
    externalAppsAction->setIcon(QIcon::fromTheme("preferences-other", QIcon(":/images/settings.png")));
    externalAppsAction->setObjectName("chooseApp");
    connect(externalAppsAction, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    addBookmarkAction = new QAction(tr("Add Bookmark"), this);
    addBookmarkAction->setObjectName("addBookmark");
    addBookmarkAction->setIcon(QIcon(":/images/new_bookmark.png"));
    connect(addBookmarkAction, &QAction::triggered, [=](){ addBookmark(getSelectedPath()); });

    removeBookmarkAction = new QAction(tr("Delete Bookmark"), this);
    removeBookmarkAction->setObjectName("deleteBookmark");
    removeBookmarkAction->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));

    zoomOutAction = new QAction(tr("Zoom Out"), this);
    zoomOutAction->setObjectName("zoomOut");
    connect(zoomOutAction, &QAction::triggered, [=](){ zoom(-1.0f); });
    zoomOutAction->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/images/zoom_out.png")));

    zoomInAction = new QAction(tr("Zoom In"), this);
    zoomInAction->setObjectName("zoomIn");
    connect(zoomInAction, &QAction::triggered, [=](){ zoom(1.0f); });
    zoomInAction->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/images/zoom_out.png")));

    resetZoomAction = new QAction(tr("Reset Zoom"), this);
    resetZoomAction->setObjectName("resetZoom");
    resetZoomAction->setIcon(QIcon::fromTheme("zoom-fit-best", QIcon(":/images/zoom.png")));
    connect(resetZoomAction, SIGNAL(triggered()), this, SLOT(resetZoom()));

    origZoomAction = new QAction(tr("Original Size"), this);
    origZoomAction->setObjectName("origZoom");
    origZoomAction->setIcon(QIcon::fromTheme("zoom-original", QIcon(":/images/zoom1.png")));
    connect(origZoomAction, SIGNAL(triggered()), this, SLOT(origZoom()));

    keepZoomAction = new QAction(tr("Keep Zoom"), this);
    keepZoomAction->setObjectName("keepZoom");
    keepZoomAction->setCheckable(true);
    connect(keepZoomAction, SIGNAL(triggered()), this, SLOT(keepZoom()));

    rotateLeftAction = new QAction(tr("Rotate 90° CCW"), this);
    rotateLeftAction->setObjectName("rotateLeft");
    rotateLeftAction->setIcon(QIcon::fromTheme("object-rotate-left", QIcon(":/images/rotate_left.png")));
    connect(rotateLeftAction, SIGNAL(triggered()), this, SLOT(rotateLeft()));

    rotateRightAction = new QAction(tr("Rotate 90° CW"), this);
    rotateRightAction->setObjectName("rotateRight");
    rotateRightAction->setIcon(QIcon::fromTheme("object-rotate-right", QIcon(":/images/rotate_right.png")));
    connect(rotateRightAction, SIGNAL(triggered()), this, SLOT(rotateRight()));

    rotateToolAction = new QAction(tr("Rotate with mouse"), this);
    rotateToolAction->setObjectName("rotateRight");
    rotateToolAction->setIcon(QIcon::fromTheme("rotation-allowed", QIcon(":/images/rotate.png")));
    rotateToolAction->setCheckable(true);
    connect(rotateToolAction, &QAction::triggered, [=](){
        Settings::mouseRotateEnabled = rotateToolAction->isChecked();
        imageViewer->setFeedback(tr("Or try holding Shift"));
    });

    flipHorizontalAction = new QAction(tr("Flip Horizontally"), this);
    flipHorizontalAction->setObjectName("flipH");
    flipHorizontalAction->setIcon(QIcon::fromTheme("object-flip-horizontal", QIcon(":/images/flipH.png")));
    connect(flipHorizontalAction, SIGNAL(triggered()), this, SLOT(flipHorizontal()));

    flipVerticalAction = new QAction(tr("Flip Vertically"), this);
    flipVerticalAction->setObjectName("flipV");
    flipVerticalAction->setIcon(QIcon::fromTheme("object-flip-vertical", QIcon(":/images/flipV.png")));
    connect(flipVerticalAction, SIGNAL(triggered()), this, SLOT(flipVertical()));

    cropAction = new QAction(tr("Letterbox"), this);
    cropAction->setObjectName("letterbox");
    cropAction->setIcon(QIcon(":/images/crop.png"));
    connect(cropAction, SIGNAL(triggered()), this, SLOT(cropImage()));

    resizeAction = new QAction(tr("Scale Image"), this);
    resizeAction->setObjectName("resize");
    resizeAction->setIcon(QIcon::fromTheme("transform-scale", QIcon(":/images/scale.png")));
    connect(resizeAction, SIGNAL(triggered()), this, SLOT(scaleImage()));

    freeRotateLeftAction = new QAction(tr("Rotate 1° CCW"), this);
    freeRotateLeftAction->setObjectName("freeRotateLeft");
    connect(freeRotateLeftAction, SIGNAL(triggered()), this, SLOT(freeRotateLeft()));

    freeRotateRightAction = new QAction(tr("Rotate 1° CW"), this);
    freeRotateRightAction->setObjectName("freeRotateRight");
    connect(freeRotateRightAction, SIGNAL(triggered()), this, SLOT(freeRotateRight()));

    colorsAction = new QAction(tr("Colors"), this);
    colorsAction->setObjectName("colors");
    connect(colorsAction, SIGNAL(triggered()), this, SLOT(showColorsDialog()));
    colorsAction->setIcon(QIcon(":/images/colors.png"));

    findDupesAction = new QAction(tr("Find Duplicate Images"), this);
    findDupesAction->setObjectName("findDupes");
    findDupesAction->setIcon(QIcon(":/images/duplicates.png"));
    findDupesAction->setCheckable(true);
    connect(findDupesAction, &QAction::triggered, [=](){ refreshThumbs(true); });

    mirrorDisabledAction = new QAction(tr("Disable Mirror"), this);
    mirrorDisabledAction->setObjectName("mirrorDisabled");
    mirrorDualAction = new QAction(tr("Dual Mirror"), this);
    mirrorDualAction->setObjectName("mirrorDual");
    mirrorTripleAction = new QAction(tr("Triple Mirror"), this);
    mirrorTripleAction->setObjectName("mirrorTriple");
    mirrorDualVerticalAction = new QAction(tr("Dual Vertical Mirror"), this);
    mirrorDualVerticalAction->setObjectName("mirrorVDual");
    mirrorQuadAction = new QAction(tr("Quad Mirror"), this);
    mirrorQuadAction->setObjectName("mirrorQuad");

    mirrorDisabledAction->setCheckable(true);
    mirrorDualAction->setCheckable(true);
    mirrorTripleAction->setCheckable(true);
    mirrorDualVerticalAction->setCheckable(true);
    mirrorQuadAction->setCheckable(true);
    connect(mirrorDisabledAction, &QAction::triggered, [=](){ imageViewer->setMirror(ImageViewer::MirrorNone); });
    connect(mirrorDualAction, &QAction::triggered, [=](){ imageViewer->setMirror(ImageViewer::MirrorDual); });
    connect(mirrorTripleAction, &QAction::triggered, [=](){ imageViewer->setMirror(ImageViewer::MirrorTriple); });
    connect(mirrorDualVerticalAction, &QAction::triggered, [=](){ imageViewer->setMirror(ImageViewer::MirrorVDual); });
    connect(mirrorQuadAction, &QAction::triggered, [=](){ imageViewer->setMirror(ImageViewer::MirrorQuad); });
    mirrorDisabledAction->setChecked(true);

    keepTransformAction = new QAction(tr("Keep Transformations"), this);
    keepTransformAction->setObjectName("keepTransform");
    keepTransformAction->setCheckable(true);
    connect(keepTransformAction, &QAction::triggered, [=](){
        Settings::keepTransform = keepTransformAction->isChecked();
        imageViewer->setFeedback(Settings::keepTransform ? tr("Transformations Locked") : tr("Transformations Unlocked"));
//        imageViewer->refresh();
        });

    moveLeftAction = new QAction(tr("Slide Image Left"), this);
    moveLeftAction->setObjectName("moveLeft");
    connect(moveLeftAction, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(50, 0)); });
    moveRightAction = new QAction(tr("Slide Image Right"), this);
    moveRightAction->setObjectName("moveRight");
    connect(moveRightAction, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(-50, 0)); });
    moveUpAction = new QAction(tr("Slide Image Up"), this);
    moveUpAction->setObjectName("moveUp");
    connect(moveUpAction, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(0, 50)); });
    moveDownAction = new QAction(tr("Slide Image Down"), this);
    moveDownAction->setObjectName("moveDown");
    connect(moveDownAction, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(0, -50)); });

    invertSelectionAction = new QAction(tr("Invert Selection"), this);
    invertSelectionAction->setObjectName("invertSelection");
    connect(invertSelectionAction, SIGNAL(triggered()), thumbsViewer, SLOT(invertSelection()));

    // There could be a Batch submenu if we had any more items to put there
    QMenu *batchSubMenu = new QMenu(tr("Batch"));
    batchSubMenuAction = new QAction(tr("Batch"), this);
    batchSubMenuAction->setMenu(batchSubMenu);
    batchTransformAction = new QAction(tr("Repeat Rotate and Crop"), this);
    batchTransformAction->setObjectName("batchTransform");
    connect(batchTransformAction, SIGNAL(triggered()), this, SLOT(batchTransform()));
    batchSubMenu->addAction(batchTransformAction);

    filterImagesFocusAction = new QAction(tr("Filter by Name"), this);
    filterImagesFocusAction->setObjectName("filterImagesFocus");
    connect(filterImagesFocusAction, SIGNAL(triggered()), this, SLOT(filterImagesFocus()));
    setPathFocusAction = new QAction(tr("Edit Current Path"), this);
    setPathFocusAction->setObjectName("setPathFocus");
    connect(setPathFocusAction, SIGNAL(triggered()), this, SLOT(setPathFocus()));

    feedbackImageInfoAction = new QAction(tr("Image Info"));
    feedbackImageInfoAction->setCheckable(true);
    connect(feedbackImageInfoAction, &QAction::triggered, [=]() {
        if (feedbackImageInfoAction->isChecked()) {
            m_infoViewer->read(imageViewer->fullImagePath);
            imageViewer->setFeedback(m_infoViewer->html(), false);
        } else {
            imageViewer->setFeedback("", false);
        }
    });

}

void Phototonic::createMenus() {
    QMenu *menu;
    menu = myMainMenu->addMenu(tr("&File"));
    menu->addAction(createDirectoryAction);
    menu->addAction(setSaveDirectoryAction);
    menu->addSeparator();
    menu->addAction(showClipboardAction);
    menu->addAction(addBookmarkAction);
    menu->addSeparator();
    menu->addAction(findDupesAction);
    menu->addSeparator();
    menu->addAction(exitAction);

    menu = myMainMenu->addMenu(tr("&Edit"));
    menu->addAction(cutAction);
    menu->addAction(copyAction);
    menu->addAction(copyToAction);
    menu->addAction(moveToAction);
    menu->addAction(pasteAction);
    menu->addAction(renameAction);
    menu->addAction(removeMetadataAction);
    menu->addAction(deleteAction);
    menu->addAction(deletePermanentlyAction);
    menu->addSeparator();
    menu->addAction(selectAllAction);
    menu->addAction(selectByBrightnesAction);
    menu->addAction(invertSelectionAction);
    menu->addAction(batchSubMenuAction);
    addAction(filterImagesFocusAction);
    addAction(setPathFocusAction);
    menu->addSeparator();
    menu->addAction(externalAppsAction);
    menu->addAction(settingsAction);

    //: "go" like in go forward, backward, etc
    menu = myMainMenu->addMenu(tr("&Go"));
    menu->addAction(goBackAction);
    menu->addAction(goFrwdAction);
    menu->addAction(goUpAction);
    menu->addAction(goHomeAction);
    menu->addSeparator();
    menu->addAction(prevImageAction);
    menu->addAction(nextImageAction);
    menu->addSeparator();
    menu->addAction(thumbsGoToTopAction);
    menu->addAction(thumbsGoToBottomAction);
    menu->addSeparator();
    menu->addAction(slideShowAction);

    //: configure visual features of the app
    menu = myMainMenu->addMenu(tr("&View"));
    menu->addMenu(createPopupMenu())->setText(tr("Window"));
    menu->addSeparator();

    QActionGroup *thumbLayoutsGroup = new QActionGroup(this);
    thumbLayoutsGroup->addAction(setClassicThumbsAction);
    thumbLayoutsGroup->addAction(setSquareThumbsAction);
    thumbLayoutsGroup->addAction(setCompactThumbsAction);
    menu->addActions(thumbLayoutsGroup->actions());
    menu->addSeparator();

    menu->addAction(thumbsZoomInAction);
    menu->addAction(thumbsZoomOutAction);
    QMenu *sortMenu = menu->addMenu(tr("Thumbnails Sorting"));
    QActionGroup *sortTypesGroup = new QActionGroup(this);
    sortTypesGroup->addAction(sortByNameAction);
    sortTypesGroup->addAction(sortByTimeAction);
    sortTypesGroup->addAction(sortBySizeAction);
    sortTypesGroup->addAction(sortByTypeAction);
    sortTypesGroup->addAction(sortBySimilarityAction);
    sortMenu->addActions(sortTypesGroup->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(sortReverseAction);
    menu->addSeparator();
    menu->addAction(includeSubDirectoriesAction);
    menu->addAction(showHiddenFilesAction);
    menu->addSeparator();
    menu->addAction(refreshAction);
    menu->addSeparator();

    myMainMenu->addAction(settingsAction);
    myMainMenu->addAction(aboutAction);

    // thumbs viewer context menu
    thumbsViewer->addAction(viewImageAction);
    thumbsViewer->addAction(m_wallpaperAction);
    thumbsViewer->addAction(openWithMenuAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(sortMenu->menuAction());
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(cutAction);
    thumbsViewer->addAction(copyAction);
    thumbsViewer->addAction(pasteAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(copyToAction);
    thumbsViewer->addAction(moveToAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(selectAllAction);
    thumbsViewer->addAction(selectByBrightnesAction);
    thumbsViewer->addAction(invertSelectionAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(batchSubMenuAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(deleteAction);
    thumbsViewer->addAction(deletePermanentlyAction);
    thumbsViewer->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void Phototonic::createToolBars() {

    myMainToolBar = addToolBar("Toolbar");
    myMainToolBar->setObjectName("MainBar");

    // edit
//    myMainToolBar->addAction(cutAction);
//    myMainToolBar->addAction(copyAction);
//    myMainToolBar->addAction(pasteAction);
//    myMainToolBar->addAction(deleteAction);
//    myMainToolBar->addAction(deletePermanentlyAction);
//    myMainToolBar->addAction(showClipboardAction);

    // navi
    myMainToolBar->addAction(goBackAction);
    myMainToolBar->addAction(goFrwdAction);
    myMainToolBar->addAction(goUpAction);
    myMainToolBar->addAction(goHomeAction);
    myMainToolBar->addAction(refreshAction);

    /* path bar */
    m_progressBarAction = myMainToolBar->addWidget(m_progressBar = new QProgressBar);
    m_progressBarAction->setVisible(false);
    pathLineEdit = new QLineEdit;
    DirCompleter *dirCompleter = new DirCompleter(pathLineEdit, fileSystemModel);
    pathLineEdit->setCompleter(dirCompleter);
    connect(dirCompleter, SIGNAL(activated(QString)), this, SLOT(goPathBarDir()));
    std::unique_ptr<QMetaObject::Connection> pconn{new QMetaObject::Connection};
    QMetaObject::Connection &conn = *pconn;
    conn = connect(pathLineEdit, &QLineEdit::textEdited, [=](){fileSystemModel->setRootPath("/"); QObject::disconnect(conn);});
    pathLineEdit->setMinimumWidth(200);
    connect(pathLineEdit, SIGNAL(returnPressed()), this, SLOT(goPathBarDir()));
    m_pathLineEditAction = myMainToolBar->addWidget(pathLineEdit);
    myMainToolBar->addAction(includeSubDirectoriesAction);
    myMainToolBar->addAction(findDupesAction);

    myMainToolBar->addAction(slideShowAction);

    /* filter bar */
    filterLineEdit = new QLineEdit;
    filterLineEdit->setMinimumWidth(100);
    filterLineEdit->setMaximumWidth(200);
    //: hint for the filter lineedit, "/" triggers more hints at extended features
    filterLineEdit->setPlaceholderText(tr("Filter - try \"/\"..."));
    connect(filterLineEdit, &QLineEdit::returnPressed, [=](){
        QString error;
        if (thumbsViewer->setFilter(filterLineEdit->text(), &error))
            refreshThumbs(true);
        else
            QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height()*6/5)),
                                error, filterLineEdit);
    });
    static const QString rtfm =
    //: This is a tooltip explaining extended filter features
    tr( "<h2>[substring] [/ constraint [/ more constraints]]</h2>"
        "<tt>foo / &gt; 5d &lt; 1M / &lt; 10kb</tt><br>"
        "<i>matches foo, older than 5 days but younger than a month - or below 10kB</i>"
        "<ul><li>Bigger than/After: &gt;</li>"
        "<li>Smaller than/Before: &lt;</li>"
        "<li>The exact age or (rounded) size is otherwise implied or explicit with: =</li></ul><hr>"
        "<ul><li>Dates are absolute (YYYY-MM-DD) or relative (5m:h:d:w:M:y)</li>"
        "<li>Sizes are suffixed 4kB:MB:GB or 4MP (mega-pixel)</li>"
        "<li>Dimensions are pre/in/suffixed \"x\" ([width]x[height])</li></ul>"
        "<i>All suffixes are case-insensitive but m|inute and M|onth</i><br>"
        "Subsequent \"/\" start a new sufficient condition group, the substring match is optional."
    );
    connect(filterLineEdit, &QLineEdit::textEdited, [=](){
        if (filterLineEdit->text() == "") {
            thumbsViewer->setFilter("");
            refreshThumbs(true);
        } else if (filterLineEdit->text().contains("/")) {
            QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height()*6/5)),
                                rtfm, filterLineEdit);
        }
    });

    myMainToolBar->addSeparator();
    myMainToolBar->addWidget(filterLineEdit);

    QAction *act = new QAction;;
    act->setShortcut(Qt::Key_Escape);
    act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(act, &QAction::triggered, [=]() {
        thumbsViewer->setFocus(Qt::OtherFocusReason);
    });
    pathLineEdit->addAction(act);
    filterLineEdit->addAction(act);

    QAction *mainMenu = new QAction(tr("Menu"), this);
    mainMenu->setIcon(QIcon::fromTheme("preferences-system", QIcon(":/images/settings.png")));
    mainMenu->setMenu(myMainMenu);
    myMainToolBar->addAction(mainMenu);
    m_menuButton = static_cast<QToolButton*>(myMainToolBar->widgetForAction(mainMenu));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    m_menuButton->installEventFilter(this);

    /* image */
    imageToolBar = new QToolBar(tr("Viewer Toolbar"));
    imageToolBar->setObjectName("Image");
    imageToolBar->addAction(prevImageAction);
    imageToolBar->addAction(nextImageAction);
    imageToolBar->addAction(firstImageAction);
    imageToolBar->addAction(lastImageAction);
    imageToolBar->addAction(slideShowAction);
    imageToolBar->addSeparator();
    imageToolBar->addAction(saveAction);
    imageToolBar->addAction(saveAsAction);
    imageToolBar->addAction(deleteAction);
    imageToolBar->addAction(deletePermanentlyAction);
    imageToolBar->addSeparator();
    imageToolBar->addAction(zoomInAction);
    imageToolBar->addAction(zoomOutAction);
    imageToolBar->addAction(resetZoomAction);
    imageToolBar->addAction(origZoomAction);
    imageToolBar->addSeparator();
    imageToolBar->addAction(resizeAction);
    imageToolBar->addAction(rotateRightAction);
    imageToolBar->addAction(rotateLeftAction);
    imageToolBar->addAction(rotateToolAction);
    imageToolBar->addAction(flipHorizontalAction);
    imageToolBar->addAction(flipVerticalAction);
    imageToolBar->addAction(cropAction);
    imageToolBar->addAction(colorsAction);
    imageToolBar->setVisible(false);

    setToolbarIconSize();
}

void Phototonic::setToolbarIconSize() {
    if (initComplete) {
        Settings::smallToolbarIcons = smallToolbarIconsAction->isChecked();
    }
    int iconSize = Settings::smallToolbarIcons ? 16 : 24;
    QSize iconQSize(iconSize, iconSize);

    myMainToolBar->setIconSize(iconQSize);
    imageToolBar->setIconSize(iconQSize);
}

void Phototonic::createFileSystemDock() {
    fileSystemDock = new QDockWidget(tr("File System"), this);
    fileSystemDock->setObjectName("File System");

    fileListWidget = new FileListWidget(fileSystemDock);
    connect(fileListWidget, &FileListWidget::itemSelectionChanged, [=](){
        if (initComplete && fileListWidget->itemAt(0, 0)->isSelected()) {
            Settings::isFileListLoaded = true;
            fileSystemTree->clearSelection();
            refreshThumbs(true);
        }
    });
    fileListWidget->hide();

    fileSystemTree = new FileSystemTree(fileSystemDock);
    fileSystemTree->addAction(createDirectoryAction);
    fileSystemTree->addAction(renameAction);
    fileSystemTree->addAction(deleteAction);
    fileSystemTree->addAction(deletePermanentlyAction);
    fileSystemTree->addAction(m_wallpaperAction);
    fileSystemTree->addAction(openWithMenuAction);
    fileSystemTree->addAction(addBookmarkAction);
    fileSystemTree->setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(fileSystemTree, &FileSystemTree::clicked, this, &Phototonic::goSelectedDir);
    connect(fileSystemModel, &QFileSystemModel::rowsRemoved, this, &Phototonic::checkDirState);
    connect(fileSystemTree, &FileSystemTree::dropOp, this, &Phototonic::dropOp);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(fileListWidget);
    mainLayout->addWidget(fileSystemTree);

    QWidget *fileSystemTreeMainWidget = new QWidget(fileSystemDock);
    fileSystemTreeMainWidget->setLayout(mainLayout);

    fileSystemDock->setWidget(fileSystemTreeMainWidget);
    connect(fileSystemDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (visible && !fileSystemTree->model()) {
            QTimer::singleShot(50, [=](){
                fileSystemTree->setModel(fileSystemModel);
                for (int i = 1; i < fileSystemModel->columnCount(); ++i) {
                    fileSystemTree->hideColumn(i);
                }
                fileSystemModel->setRootPath("/");
                fileSystemTree->setCurrentIndex(fileSystemModel->index(Settings::currentDirectory));
                fileSystemTree->scrollTo(fileSystemModel->index(Settings::currentDirectory));
                connect(fileSystemTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(updateActions()) );
            });
        }
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::fileSystemDockVisible = visible;
        }
    });
    addDockWidget(Qt::LeftDockWidgetArea, fileSystemDock);
}

void Phototonic::createBookmarksDock() {
    bookmarksDock = new QDockWidget(tr("Bookmarks"), this);
    bookmarksDock->setObjectName("Bookmarks");
    bookmarks = new BookMarks(bookmarksDock);
    bookmarksDock->setWidget(bookmarks);

    connect(bookmarksDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::bookmarksDockVisible = visible;
        }
    });
    connect(bookmarks, &BookMarks::itemClicked, [=](QTreeWidgetItem *item, int col) { goTo(item->toolTip(col)); });
    connect(removeBookmarkAction, SIGNAL(triggered()), bookmarks, SLOT(removeBookmark()));
    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));

    addDockWidget(Qt::LeftDockWidgetArea, bookmarksDock);

    bookmarks->addAction(pasteAction);
    bookmarks->addAction(removeBookmarkAction);
    bookmarks->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void Phototonic::createImagePreviewDock() {
    imagePreviewDock = new QDockWidget(tr("Preview"), this);
    imagePreviewDock->setObjectName("ImagePreview");
    connect(imagePreviewDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::imagePreviewDockVisible = visible;
            if (visible) {
                stackedLayout->takeAt(1);
                imagePreviewDock->setWidget(imageViewer);
                int currentRow = thumbsViewer->currentIndex().row();
                if (currentRow > -1)
                    imageViewer->loadImage(thumbsViewer->fullPathOf(currentRow), thumbsViewer->icon(currentRow).pixmap(THUMB_SIZE_MAX).toImage());
            }
        }
    });
    addDockWidget(Qt::RightDockWidgetArea, imagePreviewDock);
}

void Phototonic::createImageTagsDock() {
    //: tags are image metadata
    tagsDock = new QDockWidget(tr("Tags"), this);
    tagsDock->setObjectName("Tags");
    thumbsViewer->imageTags = new ImageTags(tagsDock, thumbsViewer);
    tagsDock->setWidget(thumbsViewer->imageTags);

    connect(tagsDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::tagsDockVisible = visible;
        }
    });
    connect(thumbsViewer->imageTags, SIGNAL(reloadThumbs()), this, SLOT(reloadThumbs()));
    connect(thumbsViewer->imageTags->removeTagAction, SIGNAL(triggered()), this, SLOT(deleteOperation()));
}

void Phototonic::sortThumbnails() {
    thumbsViewer->thumbsSortFlags = QDir::IgnoreCase;

    QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
    if (sortByNameAction->isChecked()) {
        thumbModel->setSortRole(ThumbsViewer::SortRole);
    } else if (sortByTimeAction->isChecked()) {
        thumbModel->setSortRole(ThumbsViewer::TimeRole);
    } else if (sortBySizeAction->isChecked()) {
        thumbModel->setSortRole(ThumbsViewer::SizeRole);
    } else if (sortByTypeAction->isChecked()) {
        thumbModel->setSortRole(ThumbsViewer::TypeRole);
    } else if (sortBySimilarityAction->isChecked()) {
        thumbsViewer->sortBySimilarity();
    }
    thumbModel->sort(0, sortReverseAction->isChecked() ? Qt::AscendingOrder : Qt::DescendingOrder);
    thumbsViewer->loadVisibleThumbs(-1);
}

void Phototonic::reload() {
    findDupesAction->setChecked(false);
    if (Settings::layoutMode == ThumbViewWidget) {
        refreshThumbs(false);
    } else {
        imageViewer->reload();
    }
}

void Phototonic::setIncludeSubDirs() {
    findDupesAction->setChecked(false);
    Settings::includeSubDirectories = includeSubDirectoriesAction->isChecked();
    refreshThumbs(false);
}

void Phototonic::refreshThumbs(bool scrollToTop) {
    thumbsViewer->setNeedToScroll(scrollToTop);
    QMetaObject::invokeMethod(this, "reloadThumbs", Qt::QueuedConnection);
}

void Phototonic::showHiddenFiles() {
    Settings::showHiddenFiles = showHiddenFilesAction->isChecked();
    if (Settings::showHiddenFiles)
        fileSystemModel->setFilter(fileSystemModel->filter() | QDir::Hidden);
    else
        fileSystemModel->setFilter(fileSystemModel->filter() & ~QDir::Hidden);
    refreshThumbs(false);
}

void Phototonic::filterImagesFocus() {
    if (Settings::layoutMode == ThumbViewWidget) {
        myMainToolBar->show();
        filterLineEdit->setFocus(Qt::OtherFocusReason);
        filterLineEdit->selectAll();
    }
}

void Phototonic::setPathFocus() {
    if (Settings::layoutMode == ThumbViewWidget) {
        myMainToolBar->show();
        pathLineEdit->setFocus(Qt::OtherFocusReason);
        pathLineEdit->selectAll();
    }
}

void Phototonic::runExternalApp() {
    QString execCommand = sender() == m_wallpaperAction ? Settings::wallpaperCommand :
                        Settings::externalApps[static_cast<QAction*>(sender())->text()];

    auto substituteCommand = [=,&execCommand](QString path) {
        if (execCommand.contains("%f", Qt::CaseInsensitive))
            execCommand.replace("%f", path, Qt::CaseInsensitive);
        else if (execCommand.contains("%u", Qt::CaseInsensitive))
            execCommand.replace("%u", QUrl::fromLocalFile(path).url(), Qt::CaseInsensitive);
        else
            execCommand += " \"" + path + "\"";
    };
    if (Settings::layoutMode == ImageViewWidget) {
        if (imageViewer->isNewImage()) {
            showNewImageWarning();
            return;
        }
        substituteCommand(imageViewer->fullImagePath);
    } else {
        if (QApplication::focusWidget() == fileSystemTree) {
            substituteCommand(getSelectedPath());
        } else {

            QModelIndexList selectedIdxList = thumbsViewer->selectionModel()->selectedIndexes();
            if (selectedIdxList.size() < 1) {
                setStatus(tr("Invalid selection."));
                return;
            }
            if (selectedIdxList.size() == 1) {
                substituteCommand(thumbsViewer->fullPathOf(selectedIdxList.at(0).row()));
            } else {
                if (execCommand.contains("%f", Qt::CaseInsensitive) || execCommand.contains("%u", Qt::CaseInsensitive)) {
                    setStatus(tr("Commands using %f or %u cannot be used with multiple files."));
                    return;
                }
                for (int tn = selectedIdxList.size() - 1; tn >= 0; --tn) {
                    execCommand += " \"" + thumbsViewer->fullPathOf(selectedIdxList.at(tn).row()) + "\"";
                }
            }
        }
    }

    QProcess *externalProcess = new QProcess();
    externalProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(externalProcess, &QProcess::finished, externalProcess, &QObject::deleteLater);
    connect(externalProcess, &QProcess::errorOccurred, [=](){
                        MessageBox msgBox(this);
                        msgBox.critical(tr("Error"), tr("Failed to start external application."));
    });
    externalProcess->startCommand(execCommand);
}

void Phototonic::updateExternalApps() {
    m_wallpaperAction->setVisible(!Settings::wallpaperCommand.isEmpty());
    int actionNumber = 0;
    QMapIterator<QString, QString> externalAppsIterator(Settings::externalApps);

    QList<QAction *> actionList = openWithSubMenu->actions();
    if (!actionList.empty()) {

        for (int i = 0; i < actionList.size(); ++i) {
            QAction *action = actionList.at(i);
            if (action->isSeparator()) {
                break;
            }

            openWithSubMenu->removeAction(action);
            imageViewer->removeAction(action);
            action->deleteLater();
        }

        openWithSubMenu->clear();
    }

    while (externalAppsIterator.hasNext()) {
        ++actionNumber;
        externalAppsIterator.next();
        QAction *extAppAct = new QAction(externalAppsIterator.key(), this);
        if (actionNumber < 10) {
            extAppAct->setShortcut(QKeySequence("Alt+" + QString::number(actionNumber)));
        }
        extAppAct->setIcon(QIcon::fromTheme(externalAppsIterator.key()));
        connect(extAppAct, SIGNAL(triggered()), this, SLOT(runExternalApp()));
        openWithSubMenu->addAction(extAppAct);
        imageViewer->addAction(extAppAct);
    }

    openWithSubMenu->addSeparator();
    openWithSubMenu->addAction(externalAppsAction);
}

void Phototonic::chooseExternalApp() {
    ExternalAppsDialog *externalAppsDialog = new ExternalAppsDialog(this);

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }
    imageViewer->setCursorHiding(false);

    externalAppsDialog->exec();
    updateExternalApps();
    externalAppsDialog->deleteLater();

    if (isFullScreen()) {
        imageViewer->setCursorHiding(true);
    }
}

void Phototonic::showSettings() {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    imageViewer->setCursorHiding(false);

    SettingsDialog *settingsDialog = new SettingsDialog(this);
    if (settingsDialog->exec()) {
        imageViewer->setBackgroundColor();
        thumbsViewer->setThumbColors();
        Settings::imageZoomFactor = 1.0;
        imageViewer->showFileName(Settings::showImageName);

        if (Settings::layoutMode == ImageViewWidget) {
            imageViewer->reload();
            needThumbsRefresh = true;
            rotateToolAction->setChecked(Settings::mouseRotateEnabled);
        } else {
            refreshThumbs(false);
        }

        if (!Settings::setWindowIcon) {
            setWindowIcon(QApplication::windowIcon());
        }
        writeSettings();
    }

    if (isFullScreen()) {
        imageViewer->setCursorHiding(true);
    }
    settingsDialog->deleteLater();
}

void Phototonic::toggleFullScreen() {
    Settings::isFullScreen = fullScreenAction->isChecked();
    imageViewer->setCursorHiding(Settings::isFullScreen);
    if (fullScreenAction->isChecked())
        setWindowState(windowState() | Qt::WindowFullScreen);
    else
        setWindowState(windowState() & ~Qt::WindowFullScreen);
}

void Phototonic::selectAllThumbs() {
    thumbsViewer->selectAll();
}

void Phototonic::selectByBrightness() {
    RangeInputDialog dlg(this);
    if (dlg.exec()) {
        qreal min = dlg.minimumValue();
        qreal max = dlg.maximumValue();
        thumbsViewer->selectByBrightness(min, max);
    }
}

#define ASSERT_IMAGES_SELECTED if(thumbsViewer->selectionModel()->selectedIndexes().size()<1){setStatus(tr("No images selected"));return;}

void Phototonic::copyOrCutThumbs(bool isCopyOperation) {
    ASSERT_IMAGES_SELECTED

    Settings::copyCutIndexList = thumbsViewer->selectionModel()->selectedIndexes();
    copyCutThumbsCount = Settings::copyCutIndexList.size();

    Settings::copyCutFileList.clear();

    QList<QUrl> urlList;
    for (int thumb = 0; thumb < copyCutThumbsCount; ++thumb) {
        const QString filePath = thumbsViewer->fullPathOf(Settings::copyCutIndexList.at(thumb).row());
        Settings::copyCutFileList.append(filePath);
        urlList.append(QUrl::fromLocalFile(filePath)); // The standard apparently is URLs even for local files...
    }

    QMimeData *mimedata = new QMimeData;
    mimedata->setUrls(urlList);
    QGuiApplication::clipboard()->setMimeData(mimedata);

    Settings::isCopyOperation = isCopyOperation;
    pasteAction->setEnabled(true);

    QString state = Settings::isCopyOperation ? tr("Copied %n image(s) to clipboard", "", copyCutThumbsCount)
                                              : tr("Cut %n image(s) to clipboard", "", copyCutThumbsCount);
    setStatus(state);
}

void Phototonic::copyOrMoveImages(bool isCopyOperation) {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }
    imageViewer->setCursorHiding(false);

    copyMoveToDialog = new CopyMoveToDialog(this, getSelectedPath(), !isCopyOperation);
    if (copyMoveToDialog->exec()) {
        if (Settings::layoutMode == ThumbViewWidget) {
            copyOrCutThumbs(copyMoveToDialog->copyOp);
            pasteThumbs();
        } else {
            if (imageViewer->isNewImage()) {
                showNewImageWarning();
                if (isFullScreen()) {
                    imageViewer->setCursorHiding(true);
                }
                return;
            }

            QFileInfo fileInfo = QFileInfo(imageViewer->fullImagePath);
            QString fileName = fileInfo.fileName();
            QString destFile = copyMoveToDialog->selectedPath + QDir::separator() + fileInfo.fileName();

            int result = CopyMoveDialog::copyOrMoveFile(copyMoveToDialog->copyOp, fileName,
                                                        imageViewer->fullImagePath,
                                                        destFile, copyMoveToDialog->selectedPath);

            if (!result) {
                MessageBox msgBox(this);
                msgBox.critical(tr("Error"), tr("Failed to copy or move image."));
            } else {
                if (!copyMoveToDialog->copyOp) {
                    int currentRow = thumbsViewer->currentIndex().row();
                    thumbsViewer->model()->removeRow(currentRow);
                    loadCurrentImage(currentRow);
                }
            }
        }
    }

    bookmarks->reloadBookmarks();
    copyMoveToDialog->deleteLater();
    copyMoveToDialog = nullptr;

    if (isFullScreen()) {
        imageViewer->setCursorHiding(true);
    }
}

void Phototonic::thumbsZoomIn() {
    if (thumbsViewer->thumbSize < THUMB_SIZE_MAX) {
        thumbsViewer->thumbSize += THUMB_SIZE_MIN;
        thumbsZoomOutAction->setEnabled(true);
        if (thumbsViewer->thumbSize == THUMB_SIZE_MAX)
            thumbsZoomInAction->setEnabled(false);
        refreshThumbs(false);
    }
}

void Phototonic::thumbsZoomOut() {
    if (thumbsViewer->thumbSize > THUMB_SIZE_MIN) {
        thumbsViewer->thumbSize -= THUMB_SIZE_MIN;
        thumbsZoomInAction->setEnabled(true);
        if (thumbsViewer->thumbSize == THUMB_SIZE_MIN)
            thumbsZoomOutAction->setEnabled(false);
        refreshThumbs(false);
    }
}

void Phototonic::zoom(double multiplier, QPoint focus) {
    if (imageViewer->tempDisableResize) {
        // coming from unscaled image "zoom", sanitize to 10% step
        Settings::imageZoomFactor = qRound(Settings::imageZoomFactor*10)*0.1;
        imageViewer->tempDisableResize = false; // … and unlock
    }

    if (multiplier > 0.0 && Settings::imageZoomFactor == 16.0) {
        imageViewer->setFeedback(tr("Maximum Zoom"));
        return;
    }
    if (multiplier < 0.0 && Settings::imageZoomFactor == 0.1) {
        imageViewer->setFeedback(tr("Minimum Zoom"));
        return;
    }

    // by size
    multiplier *= Settings::imageZoomFactor * 0.5;

    // by speed
    static QElapsedTimer speedometer;
    if (!speedometer.isValid() || speedometer.elapsed() > 250)
        multiplier *= 0.05;
    else if (speedometer.elapsed() > 150)
        multiplier *= 0.1;
    else if (speedometer.elapsed() > 75)
        multiplier *= 0.5;
    speedometer.restart();

    // round and limit to 10%
    multiplier = multiplier > 0.0 ? qMax(0.1, qRound(multiplier*10)*0.1) : qMin(-0.1, qRound(multiplier*10)*0.1);

    Settings::imageZoomFactor = qMin(16.0, qMax(0.1, Settings::imageZoomFactor + multiplier));
    imageViewer->resizeImage(focus);
    //: nb the trailing "%" for eg. 80%
    imageViewer->setFeedback(tr("Zoom %1%").arg(QString::number(Settings::imageZoomFactor * 100)));
}

void Phototonic::resetZoom() {
    Settings::imageZoomFactor = 1.0;
    imageViewer->tempDisableResize = false;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Zoom Reset"));
}

void Phototonic::origZoom() {
    // Settings::imageZoomFactor gets fixed by imageViewer->resizeImage()
    imageViewer->tempDisableResize = true;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Original Size"));
}

void Phototonic::keepZoom() {
    Settings::keepZoomFactor = keepZoomAction->isChecked();
    if (Settings::keepZoomFactor) {
        imageViewer->setFeedback(tr("Zoom Locked"));
    } else {
        imageViewer->setFeedback(tr("Zoom Unlocked"));
    }
}

void Phototonic::rotateLeft() {
    Settings::rotation -= 90;
    if (qAbs(Settings::rotation) > 360.0)
        Settings::rotation -= int(360*Settings::rotation)/360;
    Settings::rotation = 90*qCeil(Settings::rotation/90);
    if (Settings::rotation < 0)
        Settings::rotation += 360;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Rotation %1°").arg(QString::number(Settings::rotation)));
}

void Phototonic::rotateRight() {
    Settings::rotation += 90;
    if (qAbs(Settings::rotation) > 360.0)
        Settings::rotation -= int(360*Settings::rotation)/360;
    Settings::rotation = 90*int(Settings::rotation/90);
    if (Settings::rotation > 270)
        Settings::rotation -= 360;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Rotation %1°").arg(QString::number(Settings::rotation)));
}

void Phototonic::flipVertical() {
    Settings::flipV = !Settings::flipV;
    imageViewer->resizeImage();
    imageViewer->setFeedback(Settings::flipV ? tr("Flipped Vertically") : tr("Unflipped Vertically"));
}

void Phototonic::flipHorizontal() {
    Settings::flipH = !Settings::flipH;
    imageViewer->resizeImage();
    imageViewer->setFeedback(Settings::flipH ? tr("Flipped Horizontally") : tr("Unflipped Horizontally"));
}

void Phototonic::cropImage() {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    setInterfaceEnabled(false);
    imageViewer->configureLetterbox();
    setInterfaceEnabled(true);
}

void Phototonic::scaleImage() {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    if (Settings::layoutMode == ImageViewWidget) {
        ResizeDialog dlg(imageViewer->currentImageSize(), imageViewer);
        if (dlg.exec() == QDialog::Accepted) {
            imageViewer->scaleImage(dlg.newSize());
        }
    } else {
        ASSERT_IMAGES_SELECTED
        /// @todo: looks like there were plans to allow mass-resizing from the thumbnail browser
    }
}

void Phototonic::freeRotateLeft() {
    --Settings::rotation;
    if (Settings::rotation < 0)
        Settings::rotation = 359;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Rotation %1°").arg(QString::number(Settings::rotation)));
}

void Phototonic::freeRotateRight() {
    ++Settings::rotation;
    if (Settings::rotation > 360)
        Settings::rotation = 1;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Rotation %1°").arg(QString::number(Settings::rotation)));
}

void Phototonic::batchTransform() {
    QModelIndexList idxs = thumbsViewer->selectionModel()->selectedIndexes();
    MessageBox msgBox(this);
    if (idxs.count() < 1) {
        msgBox.critical(tr("No images selected"), tr("Please select the images to transform."));
        return;
    }
    QRect cropRect = imageViewer->lastCropGeometry();
    QString message;
    if (Settings::saveDirectory.isEmpty())
        message = tr("Rotate %1 images by %2 degrees, then crop them to %3, %4 %5 x %6, overwiting the original files?")
                        .arg(idxs.count()).arg(Settings::rotation, 0, 'f', 2)
                        .arg(cropRect.x()).arg(cropRect.y()).arg(cropRect.width()).arg(cropRect.height());
    else
        message = tr("Rotate %1 images by %2 degrees, then crop them to %3, %4 %5 x %6, saving the transformed images to %7?")
                        .arg(idxs.count()).arg(Settings::rotation, 0, 'f', 2)
                        .arg(cropRect.x()).arg(cropRect.y()).arg(cropRect.width()).arg(cropRect.height())
                        .arg(Settings::saveDirectory);

    msgBox.setInformativeText(tr("Perform batch transformation?"));
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    if (msgBox.exec() == QMessageBox::Ok) {
        bool keepTransformWas = Settings::keepTransform;
        imageViewer->batchMode = true;
        Settings::keepTransform = true;
        for (QModelIndex i : idxs) {
            loadSelectedThumbImage(i);
            imageViewer->applyCropAndRotation();
            imageViewer->saveImage();
        }
        Settings::keepTransform = keepTransformWas;
        imageViewer->batchMode = false;
    }
}

void Phototonic::showColorsDialog() {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    if (!colorsDialog) {
        colorsDialog = new ColorsDialog(this, imageViewer);
        connect(colorsDialog, &QDialog::finished, [=](){ Settings::colorsActive = false; setInterfaceEnabled(true); });
    }

    Settings::colorsActive = true;
    colorsDialog->show();
    colorsDialog->applyColors(0);
    setInterfaceEnabled(false);
}

static bool isWritableDir(const QString &path) {
    QFileInfo info(path);
    return info.exists() && info.isDir() && info.isWritable();
}
static bool isReadableDir(const QString &path) {
    QDir checkPath(path);
    return checkPath.exists() && checkPath.isReadable();
}

void Phototonic::pasteThumbs() {
    if (!copyCutThumbsCount) {
        return;
    }

    QString destDir;
    if (copyMoveToDialog) {
        destDir = copyMoveToDialog->selectedPath;
    } else if (QApplication::focusWidget() != bookmarks) {
        destDir = getSelectedPath();
    } else if (bookmarks->currentItem()) {
        destDir = bookmarks->currentItem()->toolTip(0);
    }

    if (!isWritableDir(destDir)) {
        if (destDir.isEmpty())
            destDir = "0x0000";
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("Can not copy or move to %1").arg(destDir));
        selectCurrentViewDir();
        return;
    }

    bool pasteInCurrDir = (Settings::currentDirectory == destDir);
    QFileInfo fileInfo;
    if (!Settings::isCopyOperation && pasteInCurrDir) {
        for (int thumb = 0; thumb < Settings::copyCutFileList.size(); ++thumb) {
            fileInfo = QFileInfo(Settings::copyCutFileList.at(thumb));
            if (fileInfo.absolutePath() == destDir) {
                MessageBox msgBox(this);
                msgBox.critical(tr("Error"), tr("Can not move to the same directory"));
                return;
            }
        }
    }

    CopyMoveDialog *copyMoveDialog = new CopyMoveDialog(this);
    copyMoveDialog->execute(thumbsViewer, destDir, pasteInCurrDir);
    if (pasteInCurrDir) {
        for (int thumb = 0; thumb < Settings::copyCutFileList.size(); ++thumb) {
            thumbsViewer->addThumb(Settings::copyCutFileList.at(thumb));
        }
    } else if (thumbsViewer->model()->rowCount()) {
        thumbsViewer->setCurrentIndex(qMin(copyMoveDialog->latestRow, thumbsViewer->model()->rowCount() - 1));
    }
    QString state = Settings::isCopyOperation ? tr("Copied %n image(s)", "", copyMoveDialog->nFiles)
                                              : tr("Moved %n image(s)", "", copyMoveDialog->nFiles);
    setStatus(state);
    copyMoveDialog->deleteLater();
    selectCurrentViewDir();

    copyCutThumbsCount = 0;
    Settings::copyCutIndexList.clear();
    Settings::copyCutFileList.clear();
    pasteAction->setEnabled(false);

    thumbsViewer->loadVisibleThumbs();
}

void Phototonic::loadCurrentImage(int currentRow) {
    if (thumbsViewer->model()->rowCount() == 0) {
        hideViewer();
        refreshThumbs(true);
        return;
    }

    bool wrapImageListTmp = Settings::wrapImageList;
    Settings::wrapImageList = false;

    if (currentRow > thumbsViewer->model()->rowCount() - 1) {
        currentRow = thumbsViewer->model()->rowCount() - 1;
    }
    thumbsViewer->setCurrentIndex(currentRow);

    Settings::wrapImageList = wrapImageListTmp;
}

void Phototonic::deleteImages(bool trash) { // Deleting selected thumbnails
    ASSERT_IMAGES_SELECTED

    QStringList deathRow;
    for (QModelIndex idx : thumbsViewer->selectionModel()->selectedIndexes())
            deathRow << thumbsViewer->fullPathOf(idx.row());

    if (Settings::deleteConfirm) {
        QMessageBox msgBox(this);
        msgBox.setText(trash ? tr("Move %n selected image(s) to the trash?", "", deathRow.size())
                             : tr("Permanently delete %n selected image(s)?", "", deathRow.size()));
        QString fileList;
        msgBox.setDetailedText(deathRow.join("\n"));
        msgBox.setWindowTitle(trash ? tr("Move to Trash") : tr("Delete images"));
        msgBox.setIcon(MessageBox::Warning);
        msgBox.setStandardButtons(MessageBox::Yes | MessageBox::Cancel);
        msgBox.setDefaultButton(MessageBox::Yes);

        if (msgBox.exec() != MessageBox::Yes) {
            return;
        }
    }

    // wait until thumbnail loading is done
    QThreadPool::globalInstance()->waitForDone(-1);

    // To only show progress dialog if deleting actually takes time
    QElapsedTimer timer;
    timer.start();

    // Avoid a lot of not interesting updates while deleting
    QSignalBlocker fsBlocker(fileSystemModel);
    QSignalBlocker scrollbarBlocker(thumbsViewer->verticalScrollBar());

    // Avoid reloading thumbnails all the time
    m_deleteInProgress = true;

    ProgressDialog *progressDialog = nullptr;
    int deleteFilesCount = 0;
    QList<int> rows;

    for (QString fileNameFullPath : deathRow) {

        // Only show if it takes a lot of time, since popping this up for just
        // deleting a single image is annoying
        if (timer.elapsed() > 100) {
            if (!progressDialog)
               progressDialog = new ProgressDialog(this);
            progressDialog->opLabel->setText(tr("Deleting %1").arg(fileNameFullPath));
            progressDialog->show();
        }

        QString deleteError;
        bool deleteOk;
        if (trash) {
            deleteOk = Trash::moveToTrash(fileNameFullPath, deleteError) == Trash::Success;
        } else {
            QFile fileToRemove(fileNameFullPath);
            deleteOk = fileToRemove.remove();
            if (!deleteOk) {
                deleteError = fileToRemove.errorString();
            }
        }

        ++deleteFilesCount;
        if (deleteOk) {
            QModelIndexList indexList = thumbsViewer->model()->match(thumbsViewer->model()->index(0, 0), ThumbsViewer::FileNameRole, fileNameFullPath);
            if (indexList.size()) {
                rows << indexList.at(0).row();
                thumbsViewer->model()->removeRow(rows.last());
            }
        } else {
            MessageBox msgBox(this);
            msgBox.critical(tr("Error"),
                            (trash ? tr("Failed to move image to the trash.") : tr("Failed to delete image.")) + "\n" +
                            deleteError);
            break;
        }

        Settings::filesList.removeOne(fileNameFullPath);

        if (progressDialog && progressDialog->abortOp) {
            break;
        }
    }

    if (thumbsViewer->model()->rowCount() && rows.count()) {
        std::sort(rows.begin(), rows.end());
        thumbsViewer->setCurrentIndex(qMax(rows.at(0), thumbsViewer->model()->rowCount() - 1));
    }

    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
    }

    setStatus(tr("Deleted %n image(s)", "", deleteFilesCount));

    m_deleteInProgress = false;
}

void Phototonic::deleteFromViewer(bool trash) {
    if (imageViewer->isNewImage()) {
        showNewImageWarning();
        return;
    }

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }
    imageViewer->setCursorHiding(false);

    const QString fullPath = imageViewer->fullImagePath;
    const QString fileName = QFileInfo(fullPath).fileName();

    if (Settings::deleteConfirm) {
        MessageBox msgBox(this);
        msgBox.setText(trash ? tr("Move %1 to the trash").arg(fileName) : tr("Permanently delete %1").arg(fileName));
        msgBox.setWindowTitle(trash ? tr("Move to Trash") : tr("Delete images"));
        msgBox.setIcon(MessageBox::Warning);
        msgBox.setStandardButtons(MessageBox::Yes | MessageBox::Cancel);
        msgBox.setDefaultButton(MessageBox::Yes);

        if (msgBox.exec() != MessageBox::Yes) {
            if (isFullScreen())
                imageViewer->setCursorHiding(true);
            return;
        }
    }

    QThreadPool::globalInstance()->waitForDone(-1);

    QString trashError;
    if (trash ? (Trash::moveToTrash(fullPath, trashError) == Trash::Success) :
                QFile::remove(fullPath)) {
        int currentRow = thumbsViewer->currentIndex().row();
        thumbsViewer->model()->removeRow(currentRow);
        imageViewer->setFeedback(tr("Deleted %1").arg(fileName));
        loadCurrentImage(currentRow);
    } else {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), trash ? trashError : tr("Failed to delete image"));
    }

    if (isFullScreen())
        imageViewer->setCursorHiding(true);
}

// Main delete operation
void Phototonic::deleteOperation() {
    if (QApplication::focusWidget() == thumbsViewer->imageTags->tagsTree) {
        thumbsViewer->imageTags->removeTag();
        return;
    }

    if (QApplication::focusWidget() == bookmarks) {
        bookmarks->removeBookmark();
        return;
    }

    if (QApplication::focusWidget() == fileSystemTree) {
        deleteDirectory(true);
        return;
    }

    if (Settings::layoutMode == ImageViewWidget) {
        deleteFromViewer(true);
        return;
    }

    deleteImages(true);
}

void Phototonic::deletePermanentlyOperation() {
    if (QApplication::focusWidget() == fileSystemTree) {
        deleteDirectory(false);
        return;
    }

    if (Settings::layoutMode == ImageViewWidget) {
        deleteFromViewer(false);
        return;
    }

    deleteImages(false);
}

void Phototonic::goTo(QString path) {
    findDupesAction->setChecked(false);
    Settings::isFileListLoaded = false;
    fileListWidget->clearSelection();
    fileSystemTree->setCurrentIndex(fileSystemModel->index(path));
    Settings::currentDirectory = path;
    refreshThumbs(true);
}

void Phototonic::goSelectedDir(const QModelIndex &idx) {
    findDupesAction->setChecked(false);
    Settings::isFileListLoaded = false;
    fileListWidget->clearSelection();
    Settings::currentDirectory = getSelectedPath();
    refreshThumbs(true);
    fileSystemTree->expand(idx);
}

void Phototonic::goPathBarDir() {
    findDupesAction->setChecked(false);

    if (pathLineEdit->completer()->popup())
        pathLineEdit->completer()->popup()->hide();
    if (!isReadableDir(pathLineEdit->text())) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("Invalid Path: %1").arg(pathLineEdit->text()));
        pathLineEdit->setText(Settings::currentDirectory);
        return;
    }

    Settings::currentDirectory = pathLineEdit->text();
    refreshThumbs(true);
    selectCurrentViewDir();
    thumbsViewer->setFocus(Qt::OtherFocusReason);
}

void Phototonic::goBack() {
    if (currentHistoryIdx > 0) {
        needHistoryRecord = false;
        goTo(pathHistoryList.at(--currentHistoryIdx));
        goFrwdAction->setEnabled(true);
        if (currentHistoryIdx == 0)
            goBackAction->setEnabled(false);
    }
}

void Phototonic::goForward() {

    if (currentHistoryIdx < pathHistoryList.size() - 1) {
        needHistoryRecord = false;
        goTo(pathHistoryList.at(++currentHistoryIdx));
        if (currentHistoryIdx == (pathHistoryList.size() - 1))
            goFrwdAction->setEnabled(false);
    }
}

void Phototonic::updateActions() {

    auto toggleFileSpecificActions = [&](bool on) {
        cutAction->setEnabled(on);
        copyAction->setEnabled(on);
        copyToAction->setEnabled(on);
        moveToAction->setEnabled(on);
        viewImageAction->setEnabled(on);
        m_wallpaperAction->setEnabled(on);
        openWithMenuAction->setEnabled(on);
        batchSubMenuAction->setEnabled(on);
        deleteAction->setEnabled(on);
        deletePermanentlyAction->setEnabled(on);
    };

    if (QApplication::focusWidget() == thumbsViewer) {
        toggleFileSpecificActions(thumbsViewer->selectionModel()->selectedIndexes().size() > 0);
    } else if (QApplication::focusWidget() == bookmarks) {
        toggleFileSpecificActions(false);
    } else if (QApplication::focusWidget() == fileSystemTree) {
        toggleFileSpecificActions(false);
    } else if (Settings::layoutMode == ImageViewWidget || QApplication::focusWidget() == imageViewer) {
        toggleFileSpecificActions(true);
    } else {
        toggleFileSpecificActions(false);
    }

    if (Settings::layoutMode == ImageViewWidget && !interfaceDisabled) {
        setViewerKeyEventsEnabled(true);
        fullScreenAction->setEnabled(true);
        CloseImageAction->setEnabled(true);
    } else {
        if (QApplication::focusWidget() == imageViewer) {
            setViewerKeyEventsEnabled(true);
            fullScreenAction->setEnabled(false);
            CloseImageAction->setEnabled(false);
        } else {
            setViewerKeyEventsEnabled(false);
            fullScreenAction->setEnabled(false);
            CloseImageAction->setEnabled(false);
        }
    }
}

void Phototonic::writeSettings() {
    if (Settings::layoutMode == ThumbViewWidget) {
        // withdraw the max/fullscreen states - Qt sucks at tracking them
        // the (then to be restored size) more or less encodes the state and we rely on the WM
        // to make the best out of what will look like a clumsy maximization attempt *shrug*
        setWindowState(windowState() & ~(Qt::WindowMaximized|Qt::WindowFullScreen));
        Settings::setValue(Settings::optionGeometry, saveGeometry());
        Settings::setValue(Settings::optionWindowState, saveState());
    }

    Settings::setValue(Settings::optionThumbsSortFlags, (int) thumbsViewer->thumbsSortFlags);
    Settings::setValue(Settings::optionThumbsZoomLevel, thumbsViewer->thumbSize);
    Settings::setValue(Settings::optionFullScreenMode, (bool) Settings::isFullScreen);
    Settings::setValue(Settings::optionViewerBackgroundColor, Settings::viewerBackgroundColor);
    Settings::setValue(Settings::optionThumbsBackgroundColor, Settings::thumbsBackgroundColor);
    Settings::setValue(Settings::optionThumbsTextColor, Settings::thumbsTextColor);
    Settings::setValue(Settings::optionThumbsPagesReadCount, (int) Settings::thumbsPagesReadCount);
    Settings::setValue(Settings::optionThumbsLayout, (int) Settings::thumbsLayout);
    Settings::setValue(Settings::optionEnableAnimations, (bool) Settings::enableAnimations);
    Settings::setValue(Settings::optionExifRotationEnabled, (bool) Settings::exifRotationEnabled);
    Settings::setValue(Settings::optionExifThumbRotationEnabled,
                                    (bool) Settings::exifThumbRotationEnabled);
    Settings::setValue(Settings::optionReverseMouseBehavior, (bool) Settings::reverseMouseBehavior);
    Settings::setValue(Settings::optionScrollZooms, (bool) Settings::scrollZooms);
    Settings::setValue(Settings::optionDeleteConfirm, (bool) Settings::deleteConfirm);
    Settings::setValue(Settings::optionShowHiddenFiles, (bool) Settings::showHiddenFiles);
    Settings::setValue(Settings::optionWrapImageList, (bool) Settings::wrapImageList);
    Settings::setValue(Settings::optionImageZoomFactor, Settings::imageZoomFactor);
    Settings::setValue(Settings::optionDefaultSaveQuality, Settings::defaultSaveQuality);
    Settings::setValue(Settings::optionSlideShowDelay, Settings::slideShowDelay);
    Settings::setValue(Settings::optionSlideShowRandom, (bool) Settings::slideShowRandom);
    Settings::setValue(Settings::optionFileSystemDockVisible, (bool) Settings::fileSystemDockVisible);
    Settings::setValue(Settings::optionImageInfoDockVisible, (bool) Settings::imageInfoDockVisible);
    Settings::setValue(Settings::optionBookmarksDockVisible, (bool) Settings::bookmarksDockVisible);
    Settings::setValue(Settings::optionTagsDockVisible, (bool) Settings::tagsDockVisible);
    Settings::setValue(Settings::optionImagePreviewDockVisible, (bool) Settings::imagePreviewDockVisible);
    Settings::setValue(Settings::optionStartupDir, (int) Settings::startupDir);
    Settings::setValue(Settings::optionSpecifiedStartDir, Settings::specifiedStartDir);
    Settings::setValue(Settings::optionThumbsBackgroundImage, Settings::thumbsBackgroundImage);
    Settings::setValue(Settings::optionLastDir,
                                    Settings::startupDir == Settings::RememberLastDir ? Settings::currentDirectory
                                                                                      : "");
    Settings::setValue(Settings::optionShowImageName, (bool) Settings::showImageName);
    Settings::setValue(Settings::optionSmallToolbarIcons, (bool) Settings::smallToolbarIcons);
    Settings::setValue(Settings::optionHideDockTitlebars, (bool) Settings::hideDockTitlebars);
    Settings::setValue(Settings::optionShowViewerToolbar, (bool) Settings::showViewerToolbar);
    Settings::setValue(Settings::optionSetWindowIcon, (bool) Settings::setWindowIcon);
    Settings::setValue(Settings::optionUpscalePreview, (bool) Settings::upscalePreview);

    /* Action shortcuts */
    Settings::beginGroup(Settings::optionShortcuts);
    QMapIterator<QString, QAction *> shortcutsIterator(Settings::actionKeys);
    while (shortcutsIterator.hasNext()) {
        shortcutsIterator.next();
        Settings::appSettings->setValue(shortcutsIterator.key(), shortcutsIterator.value()->shortcut().toString());
    }
    Settings::appSettings->endGroup();

    Settings::setValue(Settings::optionWallpaperCommand, Settings::wallpaperCommand);
    /* External apps */
    Settings::beginGroup(Settings::optionExternalApps);
    Settings::appSettings->remove("");
    QMapIterator<QString, QString> eaIter(Settings::externalApps);
    while (eaIter.hasNext()) {
        eaIter.next();
        Settings::appSettings->setValue(eaIter.key(), eaIter.value());
    }
    Settings::appSettings->endGroup();

    /* save bookmarks */
    int idx = 0;
    Settings::beginGroup(Settings::optionCopyMoveToPaths);
    Settings::appSettings->remove("");
    QSetIterator<QString> pathsIter(Settings::bookmarkPaths);
    while (pathsIter.hasNext()) {
        Settings::appSettings->setValue("path" + QString::number(++idx), pathsIter.next());
    }
    Settings::appSettings->endGroup();

    /* save known Tags */
    idx = 0;
    Settings::beginGroup(Settings::optionKnownTags);
    Settings::appSettings->remove("");
    QSetIterator<QString> tagsIter(Settings::knownTags);
    while (tagsIter.hasNext()) {
        Settings::appSettings->setValue("tag" + QString::number(++idx), tagsIter.next());
    }
    Settings::appSettings->endGroup();
}

void Phototonic::readSettings() {
    initComplete = false;
    needThumbsRefresh = false;

    if (!Settings::appSettings->contains(QByteArray(Settings::optionThumbsZoomLevel))) {
        resize(800, 600);
        Settings::setValue(Settings::optionThumbsSortFlags, (int) 0);
        Settings::setValue(Settings::optionThumbsZoomLevel, (int) 200);
        Settings::setValue(Settings::optionFullScreenMode, (bool) false);
        Settings::setValue(Settings::optionViewerBackgroundColor, QColor(25, 25, 25));
        Settings::setValue(Settings::optionThumbsBackgroundColor, QColor(200, 200, 200));
        Settings::setValue(Settings::optionThumbsTextColor, QColor(25, 25, 25));
        Settings::setValue(Settings::optionThumbsPagesReadCount, (int) 2);
        Settings::setValue(Settings::optionThumbsLayout, (int) ThumbsViewer::Classic);
        Settings::setValue(Settings::optionViewerZoomOutFlags, (int) 1);
        Settings::setValue(Settings::optionViewerZoomInFlags, (int) 0);
        Settings::setValue(Settings::optionWrapImageList, (bool) false);
        Settings::setValue(Settings::optionImageZoomFactor, (float) 1.0);
        Settings::setValue(Settings::optionDefaultSaveQuality, (int) 90);
        Settings::setValue(Settings::optionEnableAnimations, (bool) true);
        Settings::setValue(Settings::optionExifRotationEnabled, (bool) true);
        Settings::setValue(Settings::optionExifThumbRotationEnabled, (bool) false);
        Settings::setValue(Settings::optionReverseMouseBehavior, (bool) false);
        Settings::setValue(Settings::optionScrollZooms, (bool) false);
        Settings::setValue(Settings::optionDeleteConfirm, (bool) true);
        Settings::setValue(Settings::optionShowHiddenFiles, (bool) false);
        Settings::setValue(Settings::optionSlideShowDelay, (int) 5);
        Settings::setValue(Settings::optionSlideShowRandom, (bool) false);
        Settings::setValue(Settings::optionFileSystemDockVisible, (bool) true);
        Settings::setValue(Settings::optionBookmarksDockVisible, (bool) true);
        Settings::setValue(Settings::optionTagsDockVisible, (bool) true);
        Settings::setValue(Settings::optionImagePreviewDockVisible, (bool) true);
        Settings::setValue(Settings::optionImageInfoDockVisible, (bool) true);
        Settings::setValue(Settings::optionShowImageName, (bool) false);
        Settings::setValue(Settings::optionSmallToolbarIcons, (bool) false);
        Settings::setValue(Settings::optionHideDockTitlebars, (bool) false);
        Settings::setValue(Settings::optionShowViewerToolbar, (bool) false);
        Settings::setValue(Settings::optionSmallToolbarIcons, (bool) true);
        Settings::setValue(Settings::optionUpscalePreview, (bool) false);
        Settings::bookmarkPaths.insert(QDir::homePath());
        const QString picturesLocation = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (!picturesLocation.isEmpty()) {
            Settings::bookmarkPaths.insert(picturesLocation);
        }
    }

    Settings::viewerBackgroundColor = Settings::value(
            Settings::optionViewerBackgroundColor).value<QColor>();
    Settings::enableAnimations = Settings::value(Settings::optionEnableAnimations).toBool();
    Settings::exifRotationEnabled = Settings::value(Settings::optionExifRotationEnabled).toBool();
    Settings::exifThumbRotationEnabled = Settings::value(
            Settings::optionExifThumbRotationEnabled).toBool();
    Settings::thumbsLayout = Settings::value(
            Settings::optionThumbsLayout).toInt();
    Settings::reverseMouseBehavior = Settings::value(Settings::optionReverseMouseBehavior).toBool();
    Settings::scrollZooms = Settings::value(Settings::optionScrollZooms).toBool();
    Settings::deleteConfirm = Settings::value(Settings::optionDeleteConfirm).toBool();
    Settings::showHiddenFiles = Settings::value(Settings::optionShowHiddenFiles).toBool();
    Settings::wrapImageList = Settings::value(Settings::optionWrapImageList).toBool();
    Settings::imageZoomFactor = Settings::value(Settings::optionImageZoomFactor).toFloat();
    Settings::zoomOutFlags = Settings::value(Settings::optionViewerZoomOutFlags).toUInt();
    Settings::zoomInFlags = Settings::value(Settings::optionViewerZoomInFlags).toUInt();
    Settings::rotation = 0;
    Settings::keepTransform = false;
    Settings::flipH = false;
    Settings::flipV = false;
    Settings::defaultSaveQuality = Settings::value(Settings::optionDefaultSaveQuality).toInt();
    Settings::slideShowDelay = Settings::value(Settings::optionSlideShowDelay).toInt();
    Settings::slideShowRandom = Settings::value(Settings::optionSlideShowRandom).toBool();
    Settings::slideShowActive = false;
    Settings::fileSystemDockVisible = Settings::value(Settings::optionFileSystemDockVisible).toBool();
    Settings::bookmarksDockVisible = Settings::value(Settings::optionBookmarksDockVisible).toBool();
    Settings::tagsDockVisible = Settings::value(Settings::optionTagsDockVisible).toBool();
    Settings::imagePreviewDockVisible = Settings::value(Settings::optionImagePreviewDockVisible).toBool();
    Settings::imageInfoDockVisible = Settings::value(Settings::optionImageInfoDockVisible).toBool();
    Settings::startupDir = (Settings::StartupDir) Settings::value(Settings::optionStartupDir).toInt();
    Settings::specifiedStartDir = Settings::value(Settings::optionSpecifiedStartDir).toString();
    Settings::thumbsBackgroundImage = Settings::value(Settings::optionThumbsBackgroundImage).toString();
    Settings::showImageName = Settings::value(Settings::optionShowImageName).toBool();
    Settings::smallToolbarIcons = Settings::value(Settings::optionSmallToolbarIcons).toBool();
    Settings::hideDockTitlebars = Settings::value(Settings::optionHideDockTitlebars).toBool();
    Settings::showViewerToolbar = Settings::value(Settings::optionShowViewerToolbar).toBool();
    Settings::setWindowIcon = Settings::value(Settings::optionSetWindowIcon).toBool();
    Settings::upscalePreview = Settings::value(Settings::optionUpscalePreview).toBool();

    Settings::wallpaperCommand = Settings::value(Settings::optionWallpaperCommand).toString();
    /* read external apps */
    Settings::beginGroup(Settings::optionExternalApps);
    QStringList extApps = Settings::appSettings->childKeys();
    for (int i = 0; i < extApps.size(); ++i) {
        Settings::externalApps[extApps.at(i)] = Settings::appSettings->value(extApps.at(i)).toString();
    }
    Settings::appSettings->endGroup();

    /* read bookmarks */
    Settings::beginGroup(Settings::optionCopyMoveToPaths);
    QStringList paths = Settings::appSettings->childKeys();
    for (int i = 0; i < paths.size(); ++i) {
        Settings::bookmarkPaths.insert(Settings::appSettings->value(paths.at(i)).toString());
    }
    Settings::appSettings->endGroup();

    /* read known tags */
    Settings::beginGroup(Settings::optionKnownTags);
    QStringList tags = Settings::appSettings->childKeys();
    for (int i = 0; i < tags.size(); ++i) {
        Settings::knownTags.insert(Settings::appSettings->value(tags.at(i)).toString());
    }
    Settings::appSettings->endGroup();

    Settings::isFileListLoaded = false;
}

void Phototonic::setupDocks() {

    addDockWidget(Qt::RightDockWidgetArea, imageInfoDock);
    addDockWidget(Qt::RightDockWidgetArea, tagsDock);

    fileSystemDockOrigWidget = fileSystemDock->titleBarWidget();
    bookmarksDockOrigWidget = bookmarksDock->titleBarWidget();
    imagePreviewDockOrigWidget = imagePreviewDock->titleBarWidget();
    tagsDockOrigWidget = tagsDock->titleBarWidget();
    imageInfoDockOrigWidget = imageInfoDock->titleBarWidget();
    fileSystemDockEmptyWidget = new QWidget;
    bookmarksDockEmptyWidget = new QWidget;
    imagePreviewDockEmptyWidget = new QWidget;
    tagsDockEmptyWidget = new QWidget;
    imageInfoDockEmptyWidget = new QWidget;
    lockDocks();
}

void Phototonic::lockDocks() {
    if (initComplete)
        Settings::hideDockTitlebars = lockDocksAction->isChecked();

    if (Settings::hideDockTitlebars) {
        fileSystemDock->setTitleBarWidget(fileSystemDockEmptyWidget);
        bookmarksDock->setTitleBarWidget(bookmarksDockEmptyWidget);
        imagePreviewDock->setTitleBarWidget(imagePreviewDockEmptyWidget);
        tagsDock->setTitleBarWidget(tagsDockEmptyWidget);
        imageInfoDock->setTitleBarWidget(imageInfoDockEmptyWidget);
    } else {
        fileSystemDock->setTitleBarWidget(fileSystemDockOrigWidget);
        bookmarksDock->setTitleBarWidget(bookmarksDockOrigWidget);
        imagePreviewDock->setTitleBarWidget(imagePreviewDockOrigWidget);
        tagsDock->setTitleBarWidget(tagsDockOrigWidget);
        imageInfoDock->setTitleBarWidget(imageInfoDockOrigWidget);
    }
}

QMenu *Phototonic::createPopupMenu() {
    QMenu *extraActsMenu = QMainWindow::createPopupMenu();
    extraActsMenu->addSeparator();
    extraActsMenu->addAction(smallToolbarIconsAction);
    extraActsMenu->addAction(lockDocksAction);
    return extraActsMenu;
}

void Phototonic::loadShortcuts() {
    // Add customizable key shortcut actions
    Settings::actionKeys[thumbsGoToTopAction->objectName()] = thumbsGoToTopAction;
    Settings::actionKeys[thumbsGoToBottomAction->objectName()] = thumbsGoToBottomAction;
    Settings::actionKeys[CloseImageAction->objectName()] = CloseImageAction;
    Settings::actionKeys[fullScreenAction->objectName()] = fullScreenAction;
    Settings::actionKeys[settingsAction->objectName()] = settingsAction;
    Settings::actionKeys[exitAction->objectName()] = exitAction;
    Settings::actionKeys[thumbsZoomInAction->objectName()] = thumbsZoomInAction;
    Settings::actionKeys[thumbsZoomOutAction->objectName()] = thumbsZoomOutAction;
    Settings::actionKeys[cutAction->objectName()] = cutAction;
    Settings::actionKeys[copyAction->objectName()] = copyAction;
    Settings::actionKeys[nextImageAction->objectName()] = nextImageAction;
    Settings::actionKeys[prevImageAction->objectName()] = prevImageAction;
    Settings::actionKeys[deletePermanentlyAction->objectName()] = deletePermanentlyAction;
    Settings::actionKeys[deleteAction->objectName()] = deleteAction;
    Settings::actionKeys[saveAction->objectName()] = saveAction;
    Settings::actionKeys[saveAsAction->objectName()] = saveAsAction;
    Settings::actionKeys[keepTransformAction->objectName()] = keepTransformAction;
    Settings::actionKeys[keepZoomAction->objectName()] = keepZoomAction;
    Settings::actionKeys[showClipboardAction->objectName()] = showClipboardAction;
    Settings::actionKeys[copyImageAction->objectName()] = copyImageAction;
    Settings::actionKeys[pasteImageAction->objectName()] = pasteImageAction;
    Settings::actionKeys[renameAction->objectName()] = renameAction;
    Settings::actionKeys[refreshAction->objectName()] = refreshAction;
    Settings::actionKeys[pasteAction->objectName()] = pasteAction;
    Settings::actionKeys[goBackAction->objectName()] = goBackAction;
    Settings::actionKeys[goFrwdAction->objectName()] = goFrwdAction;
    Settings::actionKeys[slideShowAction->objectName()] = slideShowAction;
    Settings::actionKeys[firstImageAction->objectName()] = firstImageAction;
    Settings::actionKeys[lastImageAction->objectName()] = lastImageAction;
    Settings::actionKeys[randomImageAction->objectName()] = randomImageAction;
    Settings::actionKeys[viewImageAction->objectName()] = viewImageAction;
    Settings::actionKeys[zoomOutAction->objectName()] = zoomOutAction;
    Settings::actionKeys[zoomInAction->objectName()] = zoomInAction;
    Settings::actionKeys[resetZoomAction->objectName()] = resetZoomAction;
    Settings::actionKeys[origZoomAction->objectName()] = origZoomAction;
    Settings::actionKeys[rotateLeftAction->objectName()] = rotateLeftAction;
    Settings::actionKeys[rotateRightAction->objectName()] = rotateRightAction;
    Settings::actionKeys[freeRotateLeftAction->objectName()] = freeRotateLeftAction;
    Settings::actionKeys[freeRotateRightAction->objectName()] = freeRotateRightAction;
    Settings::actionKeys[flipHorizontalAction->objectName()] = flipHorizontalAction;
    Settings::actionKeys[flipVerticalAction->objectName()] = flipVerticalAction;
    Settings::actionKeys[cropAction->objectName()] = cropAction;
    Settings::actionKeys[colorsAction->objectName()] = colorsAction;
    Settings::actionKeys[mirrorDisabledAction->objectName()] = mirrorDisabledAction;
    Settings::actionKeys[mirrorDualAction->objectName()] = mirrorDualAction;
    Settings::actionKeys[mirrorTripleAction->objectName()] = mirrorTripleAction;
    Settings::actionKeys[mirrorDualVerticalAction->objectName()] = mirrorDualVerticalAction;
    Settings::actionKeys[mirrorQuadAction->objectName()] = mirrorQuadAction;
    Settings::actionKeys[moveDownAction->objectName()] = moveDownAction;
    Settings::actionKeys[moveUpAction->objectName()] = moveUpAction;
    Settings::actionKeys[moveRightAction->objectName()] = moveRightAction;
    Settings::actionKeys[moveLeftAction->objectName()] = moveLeftAction;
    Settings::actionKeys[copyToAction->objectName()] = copyToAction;
    Settings::actionKeys[moveToAction->objectName()] = moveToAction;
    Settings::actionKeys[goUpAction->objectName()] = goUpAction;
    Settings::actionKeys[resizeAction->objectName()] = resizeAction;
    Settings::actionKeys[filterImagesFocusAction->objectName()] = filterImagesFocusAction;
    Settings::actionKeys[setPathFocusAction->objectName()] = setPathFocusAction;
    Settings::actionKeys[invertSelectionAction->objectName()] = invertSelectionAction;
    Settings::actionKeys[includeSubDirectoriesAction->objectName()] = includeSubDirectoriesAction;
    Settings::actionKeys[createDirectoryAction->objectName()] = createDirectoryAction;
    Settings::actionKeys[addBookmarkAction->objectName()] = addBookmarkAction;
    Settings::actionKeys[removeMetadataAction->objectName()] = removeMetadataAction;
    Settings::actionKeys[externalAppsAction->objectName()] = externalAppsAction;
    Settings::actionKeys[goHomeAction->objectName()] = goHomeAction;
    Settings::actionKeys[sortByNameAction->objectName()] = sortByNameAction;
    Settings::actionKeys[sortBySizeAction->objectName()] = sortBySizeAction;
    Settings::actionKeys[sortByTimeAction->objectName()] = sortByTimeAction;
    Settings::actionKeys[sortByTypeAction->objectName()] = sortByTypeAction;
    Settings::actionKeys[sortBySimilarityAction->objectName()] = sortBySimilarityAction;
    Settings::actionKeys[sortReverseAction->objectName()] = sortReverseAction;
    Settings::actionKeys[showHiddenFilesAction->objectName()] = showHiddenFilesAction;
    Settings::actionKeys[showViewerToolbarAction->objectName()] = showViewerToolbarAction;

    Settings::beginGroup(Settings::optionShortcuts);
    QStringList groupKeys = Settings::appSettings->childKeys();

    if (groupKeys.size()) {
        if (groupKeys.contains(thumbsGoToTopAction->text())) {
            QMapIterator<QString, QAction *> key(Settings::actionKeys);
            while (key.hasNext()) {
                key.next();
                if (groupKeys.contains(key.value()->text())) {
                    key.value()->setShortcut(Settings::appSettings->value(key.value()->text()).toString());
                    Settings::appSettings->remove(key.value()->text());
                    Settings::appSettings->setValue(key.key(), key.value()->shortcut().toString());
                }
            }
        } else {
            for (int i = 0; i < groupKeys.size(); ++i) {
                if (Settings::actionKeys.value(groupKeys.at(i)))
                    Settings::actionKeys.value(groupKeys.at(i))->setShortcut
                            (Settings::appSettings->value(groupKeys.at(i)).toString());
            }
        }
    } else {
        thumbsGoToTopAction->setShortcut(QKeySequence("Ctrl+Home"));
        thumbsGoToBottomAction->setShortcut(QKeySequence("Ctrl+End"));
        CloseImageAction->setShortcut(Qt::Key_Escape);
        fullScreenAction->setShortcut(QKeySequence("Alt+Return"));
        settingsAction->setShortcut(QKeySequence("Ctrl+P"));
        exitAction->setShortcut(QKeySequence("Ctrl+Q"));
        cutAction->setShortcut(QKeySequence("Ctrl+X"));
        copyAction->setShortcut(QKeySequence("Ctrl+C"));
        deleteAction->setShortcut(QKeySequence("Del"));
        deletePermanentlyAction->setShortcut(QKeySequence("Shift+Del"));
        saveAction->setShortcut(QKeySequence("Ctrl+S"));
        copyImageAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        pasteImageAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
        renameAction->setShortcut(QKeySequence("F2"));
        refreshAction->setShortcut(QKeySequence("F5"));
        pasteAction->setShortcut(QKeySequence("Ctrl+V"));
        goBackAction->setShortcut(QKeySequence("Alt+Left"));
        goFrwdAction->setShortcut(QKeySequence("Alt+Right"));
        goUpAction->setShortcut(QKeySequence("Alt+Up"));
        slideShowAction->setShortcut(QKeySequence("Ctrl+W"));
        nextImageAction->setShortcut(QKeySequence("PgDown"));
        prevImageAction->setShortcut(QKeySequence("PgUp"));
        firstImageAction->setShortcut(QKeySequence("Home"));
        lastImageAction->setShortcut(QKeySequence("End"));
        randomImageAction->setShortcut(QKeySequence("Ctrl+D"));
        viewImageAction->setShortcut(QKeySequence("Return"));
        zoomOutAction->setShortcut(QKeySequence("-"));
        zoomInAction->setShortcut(QKeySequence("+"));
        resetZoomAction->setShortcut(QKeySequence("*"));
        origZoomAction->setShortcut(QKeySequence("/"));
        rotateLeftAction->setShortcut(QKeySequence("Ctrl+Left"));
        rotateRightAction->setShortcut(QKeySequence("Ctrl+Right"));
        freeRotateLeftAction->setShortcut(QKeySequence("Ctrl+Shift+Left"));
        freeRotateRightAction->setShortcut(QKeySequence("Ctrl+Shift+Right"));
        flipHorizontalAction->setShortcut(QKeySequence("Ctrl+Down"));
        flipVerticalAction->setShortcut(QKeySequence("Ctrl+Up"));
        cropAction->setShortcut(QKeySequence("Ctrl+G"));
        colorsAction->setShortcut(QKeySequence("Ctrl+O"));
        mirrorDisabledAction->setShortcut(QKeySequence("Ctrl+1"));
        mirrorDualAction->setShortcut(QKeySequence("Ctrl+2"));
        mirrorTripleAction->setShortcut(QKeySequence("Ctrl+3"));
        mirrorDualVerticalAction->setShortcut(QKeySequence("Ctrl+4"));
        mirrorQuadAction->setShortcut(QKeySequence("Ctrl+5"));
        moveDownAction->setShortcut(QKeySequence("Down"));
        moveUpAction->setShortcut(QKeySequence("Up"));
        moveLeftAction->setShortcut(QKeySequence("Left"));
        moveRightAction->setShortcut(QKeySequence("Right"));
        copyToAction->setShortcut(QKeySequence("Ctrl+Y"));
        moveToAction->setShortcut(QKeySequence("Ctrl+M"));
        resizeAction->setShortcut(QKeySequence("Ctrl+I"));
        filterImagesFocusAction->setShortcut(QKeySequence("Ctrl+F"));
        setPathFocusAction->setShortcut(QKeySequence("Ctrl+L"));
        keepTransformAction->setShortcut(QKeySequence("Ctrl+K"));
        showHiddenFilesAction->setShortcut(QKeySequence("Ctrl+H"));
    }

    Settings::appSettings->endGroup();
}

void Phototonic::closeEvent(QCloseEvent *event) {
    thumbsViewer->abort(true);
    writeSettings();
    hide();
    QClipboard *clip = QApplication::clipboard();
    if (clip->ownsClipboard() && !clip->image().isNull()) {
        clip->clear();
    }
    event->accept();
}

void Phototonic::setStatus(QString state) {
    if (Settings::layoutMode == ImageViewWidget) {
        return; // use feedback still?
    }
    m_statusLabel->setText(state);
    m_statusLabel->adjustSize();
    m_statusLabel->move(16, height() - (m_statusLabel->height() + 10));
    m_statusLabel->raise();
    m_statusLabel->show();
    static QTimer *statusTimer = nullptr;
    if (!statusTimer) {
        statusTimer = new QTimer(this);
        statusTimer->setSingleShot(true);
        statusTimer->setInterval(3000);
        connect(statusTimer, &QTimer::timeout, m_statusLabel, &QWidget::hide);
    }
    statusTimer->start();
}

void Phototonic::mouseDoubleClickEvent(QMouseEvent *event) {
    if (interfaceDisabled) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (Settings::layoutMode == ImageViewWidget) {
            if (Settings::reverseMouseBehavior) {
                fullScreenAction->setChecked(!(fullScreenAction->isChecked()));
                toggleFullScreen();
                event->accept();
            } else if (CloseImageAction->isEnabled()) {
                hideViewer();
                event->accept();
            }
        } else {
            if (QApplication::focusWidget() == imageViewer) {
                viewImage();
            }
        }
    }
}

void Phototonic::mousePressEvent(QMouseEvent *event) {
    if (interfaceDisabled) {
        return;
    }

    if (Settings::layoutMode == ImageViewWidget) {
        if (event->button() == Qt::MiddleButton) {

            if (event->modifiers() == Qt::ShiftModifier) {
                origZoom();
                event->accept();
                return;
            }
            if (event->modifiers() == Qt::ControlModifier) {
                resetZoom();
                event->accept();
                return;
            }

            if (Settings::reverseMouseBehavior && CloseImageAction->isEnabled()) {
                hideViewer();
                event->accept();
            } else {
                fullScreenAction->setChecked(!(fullScreenAction->isChecked()));
                toggleFullScreen();
                event->accept();
            }
        }
    } else if (QApplication::focusWidget() == imageViewer) {
        if (event->button() == Qt::MiddleButton) {
            viewImage();
        }
    }
}

void Phototonic::keyPressEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_Left) {
		loadImage(Phototonic::Previous);
   } else if (event->key() == Qt::Key_Right) {
		loadImage(Phototonic::Next);
   }
   event->accept();
}

void Phototonic::newImage() {
    if (Settings::layoutMode == ThumbViewWidget) {
        showViewer();
    }

    imageViewer->loadImage("");
}

void Phototonic::setDocksVisibility(bool visible) {
    layout()->setEnabled(false);
    fileSystemDock->setVisible(visible && Settings::fileSystemDockVisible);
    bookmarksDock->setVisible(visible && Settings::bookmarksDockVisible);
    imagePreviewDock->setVisible(visible && Settings::imagePreviewDockVisible);
    tagsDock->setVisible(visible && Settings::tagsDockVisible);
    imageInfoDock->setVisible(visible && Settings::imageInfoDockVisible);

    myMainToolBar->setVisible(visible);
    imageToolBar->setVisible(!visible && Settings::showViewerToolbar);
    addToolBar(imageToolBar);

    setContextMenuPolicy(Qt::PreventContextMenu);
    layout()->setEnabled(true);
}

void Phototonic::viewImage() {
    if (Settings::layoutMode == ImageViewWidget) {
        hideViewer();
        return;
    }

    if (QApplication::focusWidget() == fileSystemTree) {
        goSelectedDir(fileSystemTree->getCurrentIndex());
        return;
    } else if (QApplication::focusWidget() == thumbsViewer || QApplication::focusWidget() == imageViewer) {
        QModelIndex selectedImageIndex;
        QModelIndexList selectedIndexes = thumbsViewer->selectionModel()->selectedIndexes();
        if (selectedIndexes.size() > 0) {
            selectedImageIndex = selectedIndexes.first();
        } else {
            if (thumbsViewer->model()->rowCount() == 0) {
                setStatus(tr("No images"));
                return;
            }
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            selectedImageIndex = thumbModel->indexFromItem(thumbModel->item(0));
            thumbsViewer->selectionModel()->select(selectedImageIndex, QItemSelectionModel::Toggle);
            thumbsViewer->setCurrentIndex(0);
        }

        loadSelectedThumbImage(selectedImageIndex);
        return;
    } else if (QApplication::focusWidget() == filterLineEdit) {
        QString error;
        if (thumbsViewer->setFilter(filterLineEdit->text(), &error))
            refreshThumbs(true);
        else
            QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height())),
                                error, filterLineEdit);
        return;
    } else if (QApplication::focusWidget() == pathLineEdit) {
        goPathBarDir();
        return;
    }
}

void Phototonic::showViewer() {
    if (Settings::layoutMode == ThumbViewWidget) {
        Settings::layoutMode = ImageViewWidget;
//        Settings::setValue("Geometry", saveGeometry());
//        Settings::setValue("WindowState", saveState());

        stackedLayout->addWidget(imageViewer);
        stackedLayout->setCurrentWidget(imageViewer);
        setDocksVisibility(false);
        m_statusLabel->hide();

        if (Settings::isFullScreen) {
            setWindowState(windowState() | Qt::WindowFullScreen);
            imageViewer->setCursorHiding(true);
        }
        imageViewer->setFocus(Qt::OtherFocusReason);
        setImageViewerWindowTitle();
        QApplication::processEvents();
    }
}
/// @todo looks like redundant calls?
void Phototonic::loadSelectedThumbImage(const QModelIndex &idx) {
    showViewer();
    thumbsViewer->setCurrentIndex(idx);
    imageViewer->loadImage(thumbsViewer->fullPathOf(idx.row()), thumbsViewer->icon(idx.row()).pixmap(THUMB_SIZE_MAX).toImage());
    setImageViewerWindowTitle();
}

void Phototonic::toggleSlideShow() {
    if (Settings::slideShowActive) {
        Settings::slideShowActive = false;
        slideShowAction->setText(tr("Slide Show"));
        imageViewer->setFeedback(tr("Slide show stopped"));

        SlideShowTimer->stop();
        SlideShowTimer->deleteLater();
        slideShowAction->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/images/play.png")));
    } else {
        if (thumbsViewer->model()->rowCount() <= 0) {
            return;
        }

        if (Settings::layoutMode == ThumbViewWidget) {
            QModelIndexList indexesList = thumbsViewer->selectionModel()->selectedIndexes();
            if (indexesList.size() != 1) {
                thumbsViewer->setCurrentIndex(0);
            } else {
                thumbsViewer->setCurrentIndex(indexesList.first());
            }

            showViewer();
        }

        Settings::slideShowActive = true;

        SlideShowTimer = new QTimer(this);
        connect(SlideShowTimer, SIGNAL(timeout()), this, SLOT(slideShowHandler()));
        SlideShowTimer->start(Settings::slideShowDelay * 1000);

        slideShowAction->setText(tr("Stop Slide Show"));
        imageViewer->setFeedback(tr("Slide show started"));
        slideShowAction->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/images/stop.png")));

        const int currentRow = thumbsViewer->currentIndex().row();
        imageViewer->loadImage(thumbsViewer->fullPathOf(currentRow),
                               thumbsViewer->icon(currentRow).pixmap(THUMB_SIZE_MAX).toImage());
    }
}

void Phototonic::slideShowHandler() {
    if (Settings::slideShowActive) {
        if (Settings::slideShowRandom) {
            loadImage(Phototonic::Random);
        } else {
            if (thumbsViewer->getNextRow() > 0) {
                thumbsViewer->setCurrentIndex(thumbsViewer->getNextRow());
            } else {
                if (Settings::wrapImageList) {
                    thumbsViewer->setCurrentIndex(0);
                } else {
                    toggleSlideShow();
                }
            }
        }
    }
}

void Phototonic::loadImage(SpecialImageIndex idx) {
    if (thumbsViewer->model()->rowCount() <= 0) {
        return;
    }

    int thumb;
    switch (idx) {
        case Phototonic::First:
            thumb = 0;
            break;
        case Phototonic::Next:
            thumb = thumbsViewer->getNextRow();
            if (thumb < 0 && Settings::wrapImageList)
                thumb = 0;
            break;
        case Phototonic::Previous:
            thumb = thumbsViewer->getPrevRow();
            if (thumb < 0 && Settings::wrapImageList)
                thumb = thumbsViewer->model()->rowCount() - 1;
            break;
        case Phototonic::Last:
            thumb = thumbsViewer->model()->rowCount() - 1;
            break;
        case Phototonic::Random:
            thumb = QRandomGenerator::global()->bounded(thumbsViewer->model()->rowCount());
            break;
        default:
            qDebug() << "bogus special index" << idx;
            return;
    }
    if (thumb < 0)
        return;

//    if (imageViewer->isVisible())
//        imageViewer->loadImage(thumbsViewer->fullPathOf(thumb), thumbsViewer->icon(thumb).pixmap(THUMB_SIZE_MAX).toImage());

    thumbsViewer->setCurrentIndex(thumb);
}

void Phototonic::setViewerKeyEventsEnabled(bool enabled) {
    nextImageAction->setEnabled(enabled);
    prevImageAction->setEnabled(enabled);
    moveLeftAction->setEnabled(enabled);
    moveRightAction->setEnabled(enabled);
    moveUpAction->setEnabled(enabled);
    moveDownAction->setEnabled(enabled);
}

void Phototonic::hideViewer() {
    setWindowState(windowState() & ~Qt::WindowFullScreen);
    imageViewer->setCursorHiding(false);

//    restoreGeometry(Settings::value(Settings::optionGeometry).toByteArray());
//    restoreState(Settings::value(Settings::optionWindowState).toByteArray());

    Settings::layoutMode = ThumbViewWidget;
    stackedLayout->setCurrentWidget(thumbsViewer);

    setDocksVisibility(true);
    while (QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    setThumbsViewerWindowTitle();

    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (needThumbsRefresh) {
        needThumbsRefresh = false;
        refreshThumbs(true);
    } else {
        if (thumbsViewer->model()->rowCount() > 0) {
            thumbsViewer->setCurrentIndex(imageViewer->fullImagePath);
        }
        thumbsViewer->scrollTo(thumbsViewer->currentIndex());
        thumbsViewer->loadVisibleThumbs();
    }

    if (!imageViewer->isVisible())
        imageViewer->clearImage();
    thumbsViewer->setFocus(Qt::OtherFocusReason);
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void Phototonic::dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString copyMoveDirPath) {
    QApplication::restoreOverrideCursor();
    Settings::isCopyOperation = (keyMods == Qt::ControlModifier);
    QString destDir;

    if (QObject::sender() == fileSystemTree) {
        destDir = getSelectedPath();
    } else if (QObject::sender() == bookmarks) {
        if (bookmarks->currentItem()) {
            destDir = bookmarks->currentItem()->toolTip(0);
        } else {
            addBookmark(copyMoveDirPath);
            return;
        }
    } else {
        // Unknown sender
        return;
    }

    MessageBox msgBox(this);
    if (!isWritableDir(destDir)) {
        msgBox.critical(tr("Error"), tr("Can not move or copy images to this directory."));
        selectCurrentViewDir();
        return;
    }

    if (destDir == Settings::currentDirectory) {
        msgBox.critical(tr("Error"), tr("Destination directory is the same as the source directory."));
        return;
    }

    if (dirOp) {
        QString baseName = copyMoveDirPath.section(QDir::separator(), -1);

        MessageBox moveDirMessageBox(this);
        moveDirMessageBox.setText(tr("Move directory %1 to %2?").arg(baseName).arg(destDir));
        moveDirMessageBox.setWindowTitle(tr("Move directory"));
        moveDirMessageBox.setIcon(MessageBox::Warning);
        moveDirMessageBox.setStandardButtons(MessageBox::Yes | MessageBox::Cancel);
        moveDirMessageBox.addButton(tr("Move Directory"), MessageBox::YesRole);
        moveDirMessageBox.setDefaultButton(MessageBox::Cancel);

        if (moveDirMessageBox.exec() == MessageBox::Yes) {
            QFile dir(copyMoveDirPath);
            if (!dir.rename(destDir + QDir::separator() + baseName)) {
                moveDirMessageBox.critical(tr("Error"), tr("Failed to move directory."));
            }
            setStatus(tr("Directory moved"));
        }
    } else {
        CopyMoveDialog *copyMoveDialog = new CopyMoveDialog(this);
        Settings::copyCutIndexList = thumbsViewer->selectionModel()->selectedIndexes();
        copyMoveDialog->execute(thumbsViewer, destDir, false);

        if (!Settings::isCopyOperation) {
            if (thumbsViewer->model()->rowCount()) {
                thumbsViewer->setCurrentIndex(qMin(copyMoveDialog->latestRow, thumbsViewer->model()->rowCount() - 1));
            }
        }
        QString state = Settings::isCopyOperation ? tr("Copied %n image(s)", "", copyMoveDialog->nFiles)
                                                  : tr("Moved %n image(s)", "", copyMoveDialog->nFiles);
        setStatus(state);
        copyMoveDialog->deleteLater();
    }

    thumbsViewer->loadVisibleThumbs();
}

void Phototonic::selectCurrentViewDir() {
    QModelIndex idx = fileSystemModel->index(Settings::currentDirectory);
    if (idx.isValid()) {
        fileSystemTree->setCurrentIndex(idx);
        fileSystemTree->scrollTo(idx);
    }
}

void Phototonic::checkDirState(const QModelIndex &, int, int) {
    if (!initComplete) {
        return;
    }

    thumbsViewer->abort();
    if (!QDir().exists(Settings::currentDirectory)) {
        Settings::currentDirectory.clear();
        QMetaObject::invokeMethod(this, "reloadThumbs", Qt::QueuedConnection);
    }
}

void Phototonic::addPathHistoryRecord(QString dir) {
    if (!needHistoryRecord) {
        needHistoryRecord = true;
        return;
    }

    if (pathHistoryList.size() && dir == pathHistoryList.at(currentHistoryIdx)) {
        return;
    }

    pathHistoryList.insert(++currentHistoryIdx, dir);

    // Need to clear irrelevant items from list
    if (currentHistoryIdx != pathHistoryList.size() - 1) {
        goFrwdAction->setEnabled(false);
        for (int i = pathHistoryList.size() - 1; i > currentHistoryIdx; --i) {
            pathHistoryList.removeAt(i);
        }
    }
}

void Phototonic::reloadThumbs() {
    if (m_deleteInProgress || !initComplete) {
        thumbsViewer->abort();
        QTimer::singleShot(32, this, SLOT(reloadThumbs())); // rate control @30Hz
        return;
    }

    if (!Settings::isFileListLoaded) {
        if (Settings::currentDirectory.isEmpty()) {
            Settings::currentDirectory = getSelectedPath();
            if (Settings::currentDirectory.isEmpty()) {
                return;
            }
        }

        if (!isReadableDir(Settings::currentDirectory)) {
            MessageBox msgBox(this);
            msgBox.critical(tr("Error"), tr("Failed to open directory %1").arg(Settings::currentDirectory));
            setStatus(tr("No directory selected"));
            return;
        }

        m_infoViewer->clear();
        if (Settings::setWindowIcon && Settings::layoutMode == Phototonic::ThumbViewWidget) {
            setWindowIcon(QApplication::windowIcon());
        }
        pathLineEdit->setText(Settings::currentDirectory);
        addPathHistoryRecord(Settings::currentDirectory);
        if (currentHistoryIdx > 0) {
            goBackAction->setEnabled(true);
        }
    }

    if (Settings::layoutMode == ThumbViewWidget) {
        setThumbsViewerWindowTitle();
    }

    if (findDupesAction->isChecked()) {
        const bool actionEnabled[5] = { goBackAction->isEnabled(), goFrwdAction->isEnabled(), 
                                        goUpAction->isEnabled(), goHomeAction->isEnabled(), refreshAction->isEnabled() };
        goBackAction->setEnabled(false);
        goFrwdAction->setEnabled(false);
        goUpAction->setEnabled(false);
        goHomeAction->setEnabled(false);
        refreshAction->setEnabled(false);
        fileSystemTree->setEnabled(false);
        m_pathLineEditAction->setVisible(false);
        //: %v and %m are literal pattterns for QProgressBar (value and maximum)
        m_progressBar->setFormat(tr("Searching duplicates: %v / %m"));
        if (!m_progressBarAction->isVisible())
            m_progressBar->reset();
        m_progressBarAction->setVisible(true);

        thumbsViewer->loadDuplicates();

        m_progressBarAction->setVisible(false);
        m_pathLineEditAction->setVisible(true);
        fileSystemTree->setEnabled(true);
        goBackAction->setEnabled(actionEnabled[0]);
        goFrwdAction->setEnabled(actionEnabled[1]);
        goUpAction->setEnabled(actionEnabled[2]);
        goHomeAction->setEnabled(actionEnabled[3]);
        refreshAction->setEnabled(actionEnabled[4]);
    } else {
        m_progressBarAction->setVisible(false);
        m_pathLineEditAction->setVisible(true);
        m_progressBar->reset();
        thumbsViewer->reLoad();
    }
}

void Phototonic::setImageViewerWindowTitle() {
    QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
    const int currentRow = thumbsViewer->currentIndex().row();
    QString title = thumbModel->item(currentRow)->data(Qt::DisplayRole).toString()
                    + " - ["
                    + QString::number(currentRow + 1)
                    + "/"
                    + QString::number(thumbModel->rowCount())
                    + "] - Phototonic";

    setWindowTitle(title);
}

void Phototonic::setThumbsViewerWindowTitle() {

    if (findDupesAction->isChecked()) {
        setWindowTitle(tr("Duplicate images in %1").arg(Settings::currentDirectory) + " - Phototonic");
    } else if (Settings::isFileListLoaded) {
        setWindowTitle(tr("Files List") + " - Phototonic");
    } else {
        setWindowTitle(Settings::currentDirectory + " - Phototonic");
    }
}

void Phototonic::renameDir() {
    QModelIndexList selectedDirs = fileSystemTree->selectionModel()->selectedRows();
    QFileInfo dirInfo = QFileInfo(fileSystemModel->filePath(selectedDirs[0]));

    bool renameOk;
    QString title = tr("Rename %1").arg(dirInfo.completeBaseName());
    QString newDirName = QInputDialog::getText(this, title,
                                               tr("New name:"), QLineEdit::Normal, dirInfo.completeBaseName(),
                                               &renameOk);

    if (!renameOk) {
        selectCurrentViewDir();
        return;
    }

    if (newDirName.isEmpty()) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("Invalid name entered."));
        selectCurrentViewDir();
        return;
    }

    QFile dir(dirInfo.absoluteFilePath());
    QString newFullPathName = dirInfo.absolutePath() + QDir::separator() + newDirName;
    renameOk = dir.rename(newFullPathName);
    if (!renameOk) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("Failed to rename directory."));
        selectCurrentViewDir();
        return;
    }

    if (Settings::currentDirectory == dirInfo.absoluteFilePath()) {
        fileSystemTree->setCurrentIndex(fileSystemModel->index(newFullPathName));
    } else {
        selectCurrentViewDir();
    }
}

void Phototonic::rename() {
    if (QApplication::focusWidget() == fileSystemTree) {
        renameDir();
        return;
    }

    if (Settings::layoutMode == ImageViewWidget) {
        if (imageViewer->isNewImage()) {
            showNewImageWarning();
            return;
        }

        if (thumbsViewer->model()->rowCount() > 0) {
            thumbsViewer->setCurrentIndex(imageViewer->fullImagePath);
        }
    }

    QString selectedImageFileName = thumbsViewer->getSingleSelectionFilename();
    if (selectedImageFileName.isEmpty()) {
        setStatus(tr("Invalid selection"));
        return;
    }

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }
    imageViewer->setCursorHiding(false);

    QFile currentFileFullPath(selectedImageFileName);
    QFileInfo currentFileInfo(currentFileFullPath);
    int renameConfirmed;

    RenameDialog *renameDialog = new RenameDialog(this);
    renameDialog->setModal(true);
    renameDialog->setFileName(currentFileInfo.fileName());
    renameConfirmed = renameDialog->exec();

    QString newFileName = renameDialog->getFileName();
    renameDialog->deleteLater();

    if (renameConfirmed && newFileName.isEmpty()) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("No name entered."));
        renameConfirmed = 0;
    }

    if (renameConfirmed) {
        QString newFileNameFullPath = currentFileInfo.absolutePath() + QDir::separator() + newFileName;
        if (currentFileFullPath.rename(newFileNameFullPath)) {
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            QModelIndexList indexesList = thumbsViewer->selectionModel()->selectedIndexes();
            thumbModel->item(indexesList.first().row())->setData(newFileNameFullPath, thumbsViewer->FileNameRole);
            thumbModel->item(indexesList.first().row())->setData(newFileName, Qt::DisplayRole);

            imageViewer->setInfo(newFileName);
            imageViewer->fullImagePath = newFileNameFullPath;

            if (Settings::filesList.contains(currentFileInfo.absoluteFilePath())) {
                Settings::filesList.replace(Settings::filesList.indexOf(currentFileInfo.absoluteFilePath()),
                                            newFileNameFullPath);
            }

            if (Settings::layoutMode == ImageViewWidget) {
                setImageViewerWindowTitle();
            }
        } else {
            MessageBox msgBox(this);
            msgBox.critical(tr("Error"), tr("Failed to rename image."));
        }
    }

    if (isFullScreen()) {
        imageViewer->setCursorHiding(true);
    }
}

void Phototonic::removeMetadata() {

    QModelIndexList indexList = thumbsViewer->selectionModel()->selectedIndexes();
    QStringList fileList;
    copyCutThumbsCount = indexList.size();

    for (int thumb = 0; thumb < copyCutThumbsCount; ++thumb) {
        fileList.append(thumbsViewer->fullPathOf(indexList[thumb].row()));
    }

    if (fileList.isEmpty()) {
        setStatus(tr("Invalid selection"));
        return;
    }

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    MessageBox msgBox(this);
    msgBox.setText(tr("Permanently remove all Exif metadata from selected images?"));
    msgBox.setWindowTitle(tr("Remove Metadata"));
    msgBox.setIcon(MessageBox::Warning);
    msgBox.setStandardButtons(MessageBox::Yes | MessageBox::Cancel);
    msgBox.addButton(tr("Remove Metadata"), MessageBox::YesRole);
    msgBox.setDefaultButton(MessageBox::Cancel);
    int ret = msgBox.exec();

    if (ret == MessageBox::Yes) {
        for (int file = 0; file < fileList.size(); ++file) {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if EXIV2_TEST_VERSION(0,28,0)
            Exiv2::Image::UniquePtr image;
#else
            Exiv2::Image::AutoPtr image;
#endif
#pragma clang diagnostic pop

            try {
                image = Exiv2::ImageFactory::open(fileList[file].toStdString());
                image->clearMetadata();
                image->writeMetadata();
                Metadata::forget(fileList[file]);
            }
            catch (Exiv2::Error &error) {
                msgBox.critical(tr("Error"), tr("Failed to remove Exif metadata."));
                return;
            }
        }

        thumbsViewer->onSelectionChanged();
        QString state = QString(tr("Metadata removed from selected images"));
        setStatus(state);
    }
}

void Phototonic::deleteDirectory(bool trash) {
    bool removeDirectoryOk;
    QModelIndexList selectedDirs = fileSystemTree->selectionModel()->selectedRows();
    QString deletePath = fileSystemModel->filePath(selectedDirs[0]);
    QModelIndex idxAbove = fileSystemTree->indexAbove(selectedDirs[0]);
    QFileInfo dirInfo = QFileInfo(deletePath);
    QString question = (trash ? tr("Move directory %1 to the trash?")
                              : tr("Permanently delete the directory %1 and all of its contents?")
                        ).arg(dirInfo.completeBaseName());

    MessageBox msgBox(this);
    msgBox.setText(question);
    msgBox.setWindowTitle(tr("Delete Directory"));
    msgBox.setIcon(MessageBox::Warning);
    msgBox.setStandardButtons(MessageBox::Yes | MessageBox::Cancel);
    msgBox.addButton(trash ? tr("OK") : tr("Delete Directory"), MessageBox::YesRole);
    msgBox.setDefaultButton(MessageBox::Cancel);
    int ret = msgBox.exec();

    QString trashError;
    if (ret == MessageBox::Yes) {
        if (trash) {
            removeDirectoryOk = Trash::moveToTrash(deletePath, trashError) == Trash::Success;
        } else {
            removeDirectoryOk = removeDirectoryOperation(deletePath);
        }
    } else {
        selectCurrentViewDir();
        return;
    }

    if (!removeDirectoryOk) {
        msgBox.critical(tr("Error"), trash ? tr("Failed to move directory to the trash:") + "\n" + trashError
                                           : tr("Failed to delete directory."));
        selectCurrentViewDir();
        return;
    }

    QString state = QString(tr("Removed \"%1\"").arg(deletePath));
    setStatus(state);

    if (Settings::currentDirectory == deletePath) {
        if (idxAbove.isValid()) {
            fileSystemTree->setCurrentIndex(idxAbove);
        }
    } else {
        selectCurrentViewDir();
    }
}

void Phototonic::createSubDirectory() {
    QModelIndexList selectedDirs = fileSystemTree->selectionModel()->selectedRows();
    QFileInfo dirInfo = QFileInfo(fileSystemModel->filePath(selectedDirs[0]));

    bool ok;
    QString newDirName = QInputDialog::getText(this, tr("New Sub directory"),
                                               tr("New directory name:"), QLineEdit::Normal, "", &ok);

    if (!ok) {
        selectCurrentViewDir();
        return;
    }

    if (newDirName.isEmpty()) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("Invalid name entered."));
        selectCurrentViewDir();
        return;
    }

    QDir dir(dirInfo.absoluteFilePath());
    ok = dir.mkdir(dirInfo.absoluteFilePath() + QDir::separator() + newDirName);

    if (!ok) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("Failed to create new directory."));
        selectCurrentViewDir();
        return;
    }

    setStatus(tr("Created %1").arg(newDirName));
    fileSystemTree->expand(selectedDirs[0]);
}

void Phototonic::setSaveDirectory(QString path) {
    Settings::saveDirectory = path.isEmpty() ?
        QFileDialog::getExistingDirectory(this, tr("Directory to save images into:"),
            QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks) :
        path;
}

QString Phototonic::getSelectedPath() {
    if (!fileSystemTree->selectionModel())
        return Settings::currentDirectory; // if there's no filesystem tree, this means the open directory
    QModelIndexList selectedDirs = fileSystemTree->selectionModel()->selectedRows();
    if (selectedDirs.size() && selectedDirs[0].isValid()) {
        QFileInfo dirInfo = QFileInfo(fileSystemModel->filePath(selectedDirs[0]));
        return dirInfo.absoluteFilePath();
    } else
        return "";
}

bool Phototonic::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_menuButton) {
        if (e->type() == QEvent::Enter) {
            m_statusLabel->move(16, height() - (m_statusLabel->height() + 10));
            m_statusLabel->raise();
            m_statusLabel->show();
        }
        else if (e->type() == QEvent::Leave) {
            m_statusLabel->hide();
        }
        return QMainWindow::eventFilter(o, e);
    }

    static QPropertyAnimation *animator = nullptr;
    auto scrollThumbs = [=](int steps) {
        if (!animator) {
                animator = new QPropertyAnimation(thumbsViewer->verticalScrollBar(), "value");
                animator->setDuration(150); // default is 250
                animator->setEasingCurve(QEasingCurve::InOutQuad);
            }
            const int grid = thumbsViewer->gridSize().height();
            int v = (animator->state() == QAbstractAnimation::Running) ? animator->endValue().toInt() : 
                                                                         thumbsViewer->verticalScrollBar()->value();
            animator->setStartValue(v);
            if (qAbs(steps) == 1000) {
                if (steps > 0)
                    v = thumbsViewer->verticalScrollBar()->maximum();
                else
                    v = thumbsViewer->verticalScrollBar()->minimum();
            } else {
                if (qAbs(steps) == 100)
                    v += grid*qMax(1,int(thumbsViewer->height()/grid))*(steps/qAbs(steps));
                else
                    v += grid*steps;
                v = grid*int(steps < 0 ? qCeil(v/float(grid)) : v/grid);
            }
            animator->setEndValue(v);
            animator->start();
    };

    if (o == thumbsViewer && e->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_PageUp) {
            scrollThumbs(-100);
            return true;
        } else if (ke->key() == Qt::Key_PageDown) {
            scrollThumbs(100);
            return true;
        } else if (ke->key() == Qt::Key_Home) {
            scrollThumbs(-1000);
            return true;
        } else if (ke->key() == Qt::Key_End) {
            scrollThumbs(1000);
            return true;
        } else if (ke->key() == Qt::Key_Up) {
            if (!ke->modifiers() && !thumbsViewer->rect().intersects(thumbsViewer->visualRect(thumbsViewer->currentIndex()))) {
                thumbsViewer->setCurrentIndex(thumbsViewer->indexAt(thumbsViewer->rect().bottomLeft() + QPoint(32, -64)));
                return true;
            }
        } else if (ke->key() == Qt::Key_Down) {
            if (!ke->modifiers() && !thumbsViewer->rect().intersects(thumbsViewer->visualRect(thumbsViewer->currentIndex()))) {
                thumbsViewer->setCurrentIndex(thumbsViewer->indexAt(thumbsViewer->rect().topLeft() + QPoint(32, 32)));
                return true;
            }
        } else if (copyAction->shortcut()[0] == ke->keyCombination() || // these are sucked away by an enabled action
                   cutAction->shortcut()[0] == ke->keyCombination()) { // issue a warning for the disabled one
            setStatus(tr("No images selected"));
        }
        return QMainWindow::eventFilter(o, e);
    }

    if (e->type() != QEvent::Wheel)
        return QMainWindow::eventFilter(o, e);
    QWheelEvent *we = static_cast<QWheelEvent*>(e);
    const int scrollDelta = we->angleDelta().y();
    if (scrollDelta == 0) {
        return QMainWindow::eventFilter(o, e);
    }

    if (o == imageViewer->viewport()) {
        if (we->modifiers() == Qt::ControlModifier || Settings::scrollZooms) {
            zoom(scrollDelta / 120.0, we->position().toPoint());
        } else if (nextImageAction->isEnabled()) {
            if (scrollDelta < 0) {
                loadImage(Phototonic::Next);
            } else {
                loadImage(Phototonic::Previous);
            }
            return true;
        }
    } else if (o == thumbsViewer->viewport()) {
        if (we->modifiers() == Qt::ControlModifier) {
            if (scrollDelta < 0) {
                thumbsZoomOut();
            } else {
                thumbsZoomIn();
            }
        } else {
            if (we->modifiers() == Qt::ShiftModifier)
                scrollThumbs(100*(scrollDelta/-qAbs(scrollDelta)));
            else
                scrollThumbs(qRound(scrollDelta / -120.0));
        }
        return true;
    }
    return QMainWindow::eventFilter(o, e);
}

void Phototonic::showNewImageWarning() {
    MessageBox msgBox(this);
    msgBox.warning(tr("Warning"), tr("Cannot perform action with temporary image."));
}

bool Phototonic::removeDirectoryOperation(QString dirToDelete) {
    bool removeDirOk;
    QDir dir(dirToDelete);

    Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                                                QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) {
                removeDirOk = removeDirectoryOperation(info.absoluteFilePath());
            } else {
                removeDirOk = QFile::remove(info.absoluteFilePath());
            }

            if (!removeDirOk) {
                return removeDirOk;
            }
        }
    removeDirOk = dir.rmdir(dirToDelete);
    return removeDirOk;
}

void Phototonic::setInterfaceEnabled(bool enable) {
    // actions
    colorsAction->setEnabled(enable);
    renameAction->setEnabled(enable);
    removeMetadataAction->setEnabled(enable);
    cropAction->setEnabled(enable);
    resizeAction->setEnabled(enable);
    CloseImageAction->setEnabled(enable);
    nextImageAction->setEnabled(enable);
    prevImageAction->setEnabled(enable);
    firstImageAction->setEnabled(enable);
    lastImageAction->setEnabled(enable);
    randomImageAction->setEnabled(enable);
    slideShowAction->setEnabled(enable);
    copyToAction->setEnabled(enable);
    moveToAction->setEnabled(enable);
    deleteAction->setEnabled(enable);
    deletePermanentlyAction->setEnabled(enable);
    settingsAction->setEnabled(enable);
    viewImageAction->setEnabled(enable);

    // other
    thumbsViewer->setEnabled(enable);
    fileSystemTree->setEnabled(enable);
    bookmarks->setEnabled(enable);
    thumbsViewer->imageTags->setEnabled(enable);
    myMainToolBar->setEnabled(enable);
    interfaceDisabled = !enable;

    if (enable) {
        if (isFullScreen()) {
            imageViewer->setCursorHiding(true);
        }
    } else {
        imageViewer->setCursorHiding(false);
    }
}

void Phototonic::addBookmark(QString path) {
    Settings::bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}
