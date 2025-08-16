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
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QSocketNotifier>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTextEdit>
#include <QThread>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QWidgetAction>

#include <unistd.h>

#include "Bookmarks.h"
#include "CopyMoveDialog.h"
#include "CopyMoveToDialog.h"
#include "ColorsDialog.h"
#include "DirCompleter.h"
#include "ExternalAppsDialog.h"
#include "FileSystemTree.h"
#include "GuideWidget.h"
#include "IconProvider.h"
#include "ImageViewer.h"
#include "InfoViewer.h"
#include "MessageBox.h"
#include "MetadataCache.h"
#include "Phototonic.h"
#include "RenameDialog.h"
#include "ResizeDialog.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Tags.h"
#include "ThumbsViewer.h"

Phototonic::Phototonic(QStringList argumentsList, int filesStartAt, QWidget *parent) : QMainWindow(parent) {
    imageViewer = nullptr;
    Settings::appSettings = new QSettings("phototonic", "phototonic");

    fileSystemModel = new QFileSystemModel(this);
    fileSystemModel->setFilter(QDir::AllDirs | QDir::Dirs | QDir::NoDotAndDotDot);
    fileSystemModel->setOptions(QFileSystemModel::DontWatchForChanges);
    fileSystemModel->setIconProvider(new IconProvider);

    setDockOptions(QMainWindow::AllowNestedDocks);
    readSettings();
    createThumbsViewer();
    createActions();
    myMainMenu = new QMenu(this);
    statusBar()->setVisible(false);
    createFileSystemDock();
    createBookmarksDock();
    createImagePreviewDock();
    createImageTagsDock();
    setupDocks();
    myMainToolBar = nullptr; // accessed but undesired by …
    createMenus(); // createPopupMenu …
    createToolBars(); // and created only here
    createImageViewer();
    m_imageToolBar->setParent(imageViewer);
    m_imageToolBar->setVisible(Settings::showViewerToolbar);
    positionImageToolbar();
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

    connect (thumbsViewer, &ThumbsViewer::currentIndexChanged, this, [=](const QModelIndex &current) {
        if (!current.isValid())
            return;
        if (m_infoViewer->isVisible()) {
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            if (QStandardItem *citem = thumbModel->item(current.row()))
                m_infoViewer->hint(tr("Average brightness"), QString::number(citem->data(ThumbsViewer::BrightnessRole).toInt()/255.0f, 'f', 2));
            const QString filePath = thumbsViewer->fullPathOf(current.row());
            m_infoViewer->read(filePath, thumbsViewer->renderHistogram(filePath, m_logHistogram));
        }
        if (imageViewer->isVisible()) {
            const QString &imagePath = thumbsViewer->fullPathOf(current.row());
            if (m_imageInfoAction->isChecked()) {
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                if (!m_infoViewer->isVisible()) {
                    QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
                    if (QStandardItem *citem = thumbModel->item(current.row()))
                        m_infoViewer->hint(tr("Average brightness"), QString::number(citem->data(ThumbsViewer::BrightnessRole).toInt()/255.0f, 'f', 2));
                    m_infoViewer->read(imagePath);
                }
                imageViewer->setFeedback(m_infoViewer->html(), false);
            }
            if (Settings::layoutMode == ImageViewWidget)
                setImageViewerWindowTitle();
            imageViewer->loadImage(imagePath, Settings::slideShowActive ? QImage() : thumbsViewer->icon(current.row()).pixmap(THUMB_SIZE_MAX).toImage());
        }
        }, Qt::QueuedConnection);
    connect(qApp, SIGNAL(focusChanged(QWidget * , QWidget * )), this, SLOT(updateActions()));

    QApplication::setWindowIcon(QIcon(":/images/phototonic.png"));

    stackedLayout = new QStackedLayout;
    QWidget *stackedLayoutWidget = new QWidget;
    stackedLayout->addWidget(thumbsViewer);
    stackedLayout->addWidget(imageViewer);
    stackedLayoutWidget->setLayout(stackedLayout);
    setCentralWidget(stackedLayoutWidget);

    restoreGeometry(Settings::value(Settings::optionGeometry, QByteArray()).toByteArray());
    restoreState(Settings::value(Settings::optionWindowState, QByteArray()).toByteArray());

    processStartupArguments(argumentsList, filesStartAt);
    if (Settings::currentDirectory.isEmpty())
        Settings::currentDirectory = QDir::currentPath();

    m_thumbSizeDelta = 0;
    colorsDialog = nullptr;
    initComplete = true;
    m_deleteInProgress = false;
    currentHistoryIdx = -1;
    needHistoryRecord = true;
    m_reloadPending = false;

    refreshThumbs(true);
    if (Settings::layoutMode == ThumbViewWidget) {
        thumbsViewer->setFocus(Qt::OtherFocusReason);
    }
    action(QString()); // clear hash
}

static QString localFile(const QString &fileOrUrl) {
    QUrl url(fileOrUrl);
    if (url.scheme().isEmpty())
        return fileOrUrl;
    return url.toLocalFile();
}

void Phototonic::processStartupArguments(QStringList argumentsList, int filesStartAt) {
    if (Settings::startupDir == Settings::SpecifiedDir) {
        Settings::currentDirectory = Settings::specifiedStartDir;
    } else if (Settings::startupDir == Settings::RememberLastDir) {
        Settings::currentDirectory = Settings::value(Settings::optionLastDir, QString()).toString();
    }

    QFile *input = nullptr;
    if (filesStartAt < argumentsList.size() && argumentsList.at(filesStartAt) == "-") { // stdin ?
#ifdef Q_OS_WIN
        if (!_isatty(_fileno(stdin))) {
#else
        if (!isatty(fileno(stdin))) {
#endif
            input = new QFile;
            QByteArray ba;
            if (input->open(stdin, QFile::ReadOnly))
                ba = input->readLine();
            if (ba.isEmpty()) { // catches /dev/null or <&-
                input->close();
                delete input;
                input = nullptr;
            } else { // load first file into list
                QString line = QString::fromLocal8Bit(ba.trimmed());
                if (!line.isEmpty())
                    loadStartupFileList(QStringList() << line, 0);
            }
        }
        if (!input)
            argumentsList.remove(filesStartAt);
    }
    if (input) {
        QSocketNotifier *snr = new QSocketNotifier(input->handle(), QSocketNotifier::Read, input);
        static QStringList newFiles;
        static QTimer *bouncer;
        if (!bouncer) {
            bouncer = new QTimer(this);
            bouncer->setInterval(30);
            bouncer->setSingleShot(true);
            connect(bouncer, &QTimer::timeout, [=]() {
                loadStartupFileList(newFiles, 0);
                newFiles.clear();
                thumbsViewer->reload(true);
            });
            if (!Settings::filesList.isEmpty())
                bouncer->start(); // for the first file
        }
        connect (snr, &QSocketNotifier::activated, [=](){
            QByteArray ba = input->readLine();
            if (ba.isEmpty()) { // effectively EOF
                snr->setEnabled(false);
                input->close();
                delete input;
//                snr->deleteLater(); // segfault
                return;
            }
            QString line = QString::fromLocal8Bit(ba.trimmed());
            if (!line.isEmpty()) {
                newFiles << line;
                if (newFiles.size() < 25) // if we got some batch, let the timer run out to load it
                    bouncer->start();
            }
        });
    } else if (filesStartAt < argumentsList.size()) {
        QFileInfo firstArgument(localFile(argumentsList.at(filesStartAt)));
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
    }
    selectCurrentViewDir();
}

// for singleton update
void Phototonic::setCurrentFileOrDirectory(const QString &path) {
    activateWindow();
    QFileInfo info(localFile(path));
    if (info.isDir()) {
        if (Settings::currentDirectory != info.absoluteFilePath())
            goTo(info.absoluteFilePath());
    } else if (info.exists()) {
        showViewer();
        imageViewer->loadImage(info.absoluteFilePath());
        setWindowTitle(info.absoluteFilePath() + " - Phototonic");
        if (Settings::currentDirectory != info.absolutePath())
            goTo(info.absolutePath());
        thumbsViewer->setCurrentIndex(info.absoluteFilePath());
        thumbsViewer->scrollTo(thumbsViewer->currentIndex());
    }
}

void Phototonic::loadStartupFileList(QStringList argumentsList, int filesStartAt) {
    const int oldSize = Settings::filesList.size();
    for (int i = filesStartAt; i < argumentsList.size(); i++) {
        QFileInfo file(localFile(argumentsList[i]));
        if (!file.exists() || file.isDir())
            continue;

        QString path = file.canonicalFilePath();
        if (path.isEmpty())
            path = file.absoluteFilePath();

        if (!Settings::filesList.contains(path)) {
            Settings::filesList << path;
        }
    }
    if (oldSize == Settings::filesList.size())
        return; // stale
    if (!oldSize) { // only on first update, don't undo user interactions
        QToolButton *btn = static_cast<QToolButton*>(myMainToolBar->widgetForAction(m_goHomeAction));
        QMenu *btnmenu = new QMenu(btn);
        QAction *act = btnmenu->addAction(tr("Home"));
        connect(act, &QAction::triggered, [=]() { goTo(QDir::homePath()); });
        //: The file list is the optional list of files in the execution parameters, some virtual directory
        act = btnmenu->addAction(tr("File List"));
        connect(act, &QAction::triggered, [=]() { goTo("Phototonic::FileList"); });
        btn->setMenu(btnmenu);
        setFileListMode(true);
    }
}

void Phototonic::setFileListMode(bool on) {
    Settings::isFileListLoaded = on;
    if (on) {
        m_includeSubDirsAction->setChecked(false);
        fileSystemTree->clearSelection();
    }
    m_includeSubDirsAction->setEnabled(!on);
}

void Phototonic::createThumbsViewer() {
    thumbsViewer = new ThumbsViewer(this);
    thumbsViewer->installEventFilter(this);
    thumbsViewer->viewport()->installEventFilter(this);
    thumbsViewer->thumbsSortFlags = (QDir::SortFlags) Settings::value(Settings::optionThumbsSortFlags, 0).toInt();

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
    m_logHistogram = false;
    connect(imageInfoDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::imageInfoDockVisible = visible;
        }
        if (visible) {
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            int currentRow = thumbsViewer->currentIndex().row();
            if (currentRow > -1) {
                m_infoViewer->hint(tr("Average brightness"),
                                    QString::number(thumbModel->item(currentRow)->data(ThumbsViewer::BrightnessRole).toInt()/255.0f, 'f', 2));
                const QString filePath = thumbsViewer->fullPathOf(currentRow);
                m_infoViewer->read(filePath, thumbsViewer->renderHistogram(filePath, m_logHistogram));
            }
        }
    } );
    connect(m_infoViewer, &InfoView::histogramClicked, [=](){
        m_logHistogram = !m_logHistogram;
        int currentRow = thumbsViewer->currentIndex().row();
        if (currentRow > -1) {
            const QString filePath = thumbsViewer->fullPathOf(currentRow);
            m_infoViewer->read(filePath, thumbsViewer->renderHistogram(filePath, m_logHistogram));
        }
    });
    connect(m_infoViewer, &InfoView::exifChanged, [=](const QString &fileName) {
        thumbsViewer->updateThumbnail(fileName);
        m_imageTags->setSelectedFiles(thumbsViewer->selectedFiles());
    });
}

void Phototonic::createImageViewer() {
    imageViewer = new ImageViewer(this);
    imageViewer->viewport()->installEventFilter(this);
    connect(m_saveAction, SIGNAL(triggered()), imageViewer, SLOT(saveImage()));
    connect(m_saveAsAction, SIGNAL(triggered()), imageViewer, SLOT(saveImageAs()));
    connect(action("copyImage"), SIGNAL(triggered()), imageViewer, SLOT(copyImage()));
    connect(action("pasteImage"), SIGNAL(triggered()), imageViewer, SLOT(pasteImage()));
    QAction *rotateMouse = action("rotateMouse");
    connect(imageViewer, &ImageViewer::toolsUpdated, [=](){ rotateMouse->setChecked(Settings::mouseRotateEnabled); });
    connect(imageViewer, &ImageViewer::gotFocus, [=](){
        if (thumbsViewer->selectionModel()->selectedIndexes().size() < 1)
            thumbsViewer->setCurrentIndex(imageViewer->fullImagePath);
    });
    connect(imageViewer, &ImageViewer::imageSaved, thumbsViewer, &ThumbsViewer::updateThumbnail);
    m_editSteps = 0;
    connect(imageViewer, &ImageViewer::imageEdited, [=](bool yes) {
        m_editSteps = yes ? m_editSteps+1 : 0;
        m_saveAction->setEnabled(m_editSteps);
        m_saveAsAction->setEnabled(m_editSteps);
    });

    QMenu *contextMenu = new QMenu(imageViewer);

    // Widget actions
    imageViewer->addAction(action("toggleSlideShow"));
    imageViewer->addAction(action("nextImage"));
    imageViewer->addAction(action("prevImage"));
    imageViewer->addAction(action("firstImage"));
    imageViewer->addAction(action("lastImage"));
    imageViewer->addAction(action("randomImage"));
    imageViewer->addAction(action("zoomIn"));
    imageViewer->addAction(action("zoomOut"));
    imageViewer->addAction(action("origZoom"));
    imageViewer->addAction(action("resetZoom"));
    imageViewer->addAction(action("rotateLeft"));
    imageViewer->addAction(action("rotateRight"));
    imageViewer->addAction(action("freeRotateLeft"));
    imageViewer->addAction(action("freeRotateRight"));
    imageViewer->addAction(action("flipH"));
    imageViewer->addAction(action("flipV"));
    imageViewer->addAction(action("letterbox"));
    imageViewer->addAction(action("resize"));
    imageViewer->addAction(action("save"));
    imageViewer->addAction(action("saveAs"));
    imageViewer->addAction(action("copyImage"));
    imageViewer->addAction(action("pasteImage"));
    imageViewer->addAction(action("moveToTrash"));
    imageViewer->addAction(action("delete"));
    imageViewer->addAction(action("rename"));
    imageViewer->addAction(action("closeImage"));
    imageViewer->addAction(action("fullScreen"));
    imageViewer->addAction(action("settings"));
    imageViewer->addAction(action("keepTransform"));
    imageViewer->addAction(action("keepZoom"));
    imageViewer->addAction(action("refresh"));
    imageViewer->addAction(action("colors"));
    imageViewer->addAction(action("moveLeft"));
    imageViewer->addAction(action("moveRight"));
    imageViewer->addAction(action("moveUp"));
    imageViewer->addAction(action("moveDown"));
    imageViewer->addAction(action("showClipboard"));
    imageViewer->addAction(action("copyTo"));
    imageViewer->addAction(action("moveTo"));
    imageViewer->addAction(action("open"));
    imageViewer->addAction(action("exit"));
    imageViewer->addAction(action("showViewerToolbar"));
    imageViewer->addAction(action("chooseApp"));
    imageViewer->addAction(action("imageinfo"));

    // Actions
    contextMenu->addAction(action("setwallpaper"));
    contextMenu->addAction(action("openWithMenu"));
    contextMenu->addAction(action("imageinfo"));
    contextMenu->addSeparator();
    contextMenu->addAction(action("showViewerToolbar"));

    QMenu *menu = contextMenu->addMenu(tr("Navigate"));
    menu->addAction(action("nextImage"));
    menu->addAction(action("prevImage"));
    menu->addAction(action("firstImage"));
    menu->addAction(action("lastImage"));
    menu->addAction(action("randomImage"));
    menu->addAction(action("toggleSlideShow"));

    menu = contextMenu->addMenu(tr("Transform"));

    QMenu *submenu = menu->addMenu(QIcon::fromTheme("edit-find", QIcon(":/images/zoom.png")), tr("Zoom"));
    submenu->addAction(action("zoomIn"));
    submenu->addAction(action("zoomOut"));
    submenu->addAction(action("origZoom"));
    submenu->addAction(action("resetZoom"));

    submenu->addAction(action("keepZoom"));

    submenu = menu->addMenu(tr("Flip and Flop and Rotate"));
    submenu->addAction(action("flipH"));
    submenu->addAction(action("flipV"));
    submenu->addSeparator();
    submenu->addAction(action("rotateLeft"));
    submenu->addAction(action("rotateRight"));
    submenu->addAction(action("freeRotateLeft"));
    submenu->addAction(action("freeRotateRight"));
    submenu->addSeparator();
    submenu->addAction(action("keepTransform"));


    menu = contextMenu->addMenu(tr("Edit"));
    menu->addAction(action("resize"));
    menu->addAction(action("colors"));
    menu->addAction(action("crop"));
    menu->addSeparator();
    menu->addAction(action("blackout"));
    menu->addAction(action("cartouche"));
    menu->addAction(action("annotate"));


    menu = contextMenu->addMenu(tr("File"));
    menu->addAction(action("copyTo"));
    menu->addAction(action("moveTo"));
    menu->addAction(action("save"));
    menu->addAction(action("saveAs"));
    menu->addAction(action("rename"));
    menu->addSeparator();
    menu->addAction(action("removeMetadata"));
    menu->addAction(action("moveToTrash"));
    menu->addAction(action("delete"));


    menu = contextMenu->addMenu(tr("View"));
    menu->addAction(action("fullScreen"));
    menu->addSeparator();
    menu->addAction(action("letterbox"));

    //: The guides a lines across the image for orientation
    submenu = menu->addMenu(tr("Guides"));
    QAction *act = new QAction(tr("Add vertical guide"), submenu);
    connect(act, &QAction::triggered, [=]() { new GuideWidget(imageViewer, Qt::Vertical, imageViewer->contextSpot().x()); });
    submenu->addAction(act);
    act = new QAction(tr("Add horizontal guide"), submenu);
    connect(act, &QAction::triggered, [=]() { new GuideWidget(imageViewer, Qt::Horizontal, imageViewer->contextSpot().y()); });
    submenu->addAction(act);

    menu->addSeparator();
    menu->addAction(action("showClipboard"));
    menu->addSeparator();
    menu->addAction(action("refresh"));

    contextMenu->addSeparator();

    contextMenu->addAction(action("copyImage"));
    contextMenu->addAction(action("pasteImage"));
    contextMenu->addSeparator();
    contextMenu->addAction(action("closeImage"));

    imageViewer->setContextMenuPolicy(Qt::DefaultContextMenu);
    imageViewer->setContextMenu(contextMenu);
    Settings::isFullScreen = Settings::value(Settings::optionFullScreenMode, false).toBool();
    m_fullScreenAction->setChecked(Settings::isFullScreen);
}

void Phototonic::createActions() {

#define MAKE_ACTION_NOSC(_TEXT_, _NAME_) \
    action = new QAction(_TEXT_, this); action->setObjectName(_NAME_)

#define MAKE_ACTION(_TEXT_, _NAME_, _SHORTCUT_) \
    MAKE_ACTION_NOSC(_TEXT_, _NAME_); action->setProperty("sc_default", _SHORTCUT_)

    QAction *action;
    MAKE_ACTION(tr("Top"), "thumbsGoTop", "Ctrl+Home");
    action->setIcon(QIcon::fromTheme("go-top", QIcon(":/images/top.png")));
    connect(action, &QAction::triggered, thumbsViewer, &ThumbsViewer::scrollToTop);

    MAKE_ACTION(tr("Bottom"), "thumbsGoBottom", "Ctrl+End");
    action->setIcon(QIcon::fromTheme("go-bottom", QIcon(":/images/bottom.png")));
    connect(action, &QAction::triggered, thumbsViewer, &ThumbsViewer::scrollToBottom);

    m_closeImageAction = MAKE_ACTION(tr("Close Viewer"), "closeImage", "Esc");
    connect(action, SIGNAL(triggered()), this, SLOT(hideViewer()));

    m_fullScreenAction = MAKE_ACTION(tr("Full Screen"), "fullScreen", "Alt+Return");
    action->setCheckable(true);
    connect(action, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    MAKE_ACTION(tr("Preferences"), "settings", "Ctrl+P");
    action->setIcon(QIcon::fromTheme("preferences-system", QIcon(":/images/settings.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(showSettings()));

    MAKE_ACTION(tr("Exit"), "exit", "Ctrl+Q");
    connect(action, SIGNAL(triggered()), this, SLOT(close()));

    MAKE_ACTION_NOSC(tr("Enlarge Thumbnails"), "thumbsZoomIn");
    connect(action, &QAction::triggered, [=]() {m_thumbSizeDelta = 1; resizeThumbs();});
    action->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/images/zoom_in.png")));
    if (thumbsViewer->thumbSize == THUMB_SIZE_MAX) {
        action->setEnabled(false);
    }

    MAKE_ACTION_NOSC(tr("Shrink Thumbnails"), "thumbsZoomOut");
    connect(action, &QAction::triggered, [=]() {m_thumbSizeDelta = -1; resizeThumbs();});
    action->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/images/zoom_out.png")));
    if (thumbsViewer->thumbSize == THUMB_SIZE_MIN) {
        action->setEnabled(false);
    }

    m_cutAction = MAKE_ACTION(tr("Cut"), "cut", "Ctrl+X");
    action->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/images/cut.png")));
    connect(action, &QAction::triggered, [=]() { copyOrCutThumbs(false); });
    action->setEnabled(false);

    m_copyAction = MAKE_ACTION(tr("Copy"), "copy", "Ctrl+C");
    action->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/images/copy.png")));
    connect(action, &QAction::triggered, [=]() { copyOrCutThumbs(true); });
    action->setEnabled(false);

    MAKE_ACTION_NOSC(tr("Show classic thumbnails"), "setClassicThumbs");
    action->setCheckable(true);
    action->setChecked(Settings::thumbsLayout == ThumbsViewer::Classic);
    connect(action, &QAction::triggered, [=](){ Settings::thumbsLayout = ThumbsViewer::Classic; thumbsViewer->refreshThumbs(); });

    MAKE_ACTION_NOSC(tr("Show square thumbnails"), "setSquareThumbs");
    action->setCheckable(true);
    action->setChecked(Settings::thumbsLayout == ThumbsViewer::Squares);
    connect(action, &QAction::triggered, [=](){ Settings::thumbsLayout = ThumbsViewer::Squares; thumbsViewer->refreshThumbs(); });

    MAKE_ACTION_NOSC(tr("Show compact thumbnails"), "setCompactThumbs");
    action->setCheckable(true);
    action->setChecked(Settings::thumbsLayout == ThumbsViewer::Compact);
    connect(action, &QAction::triggered, [=](){ Settings::thumbsLayout = ThumbsViewer::Compact; thumbsViewer->refreshThumbs(); });

    m_copyToAction = MAKE_ACTION(tr("Copy to..."), "copyTo", "Ctrl+Y");
    connect(action, &QAction::triggered, [=]() { copyOrMoveImages(true); });

    m_moveToAction = MAKE_ACTION(tr("Move to..."), "moveTo", "Ctrl+M");
    connect(action, &QAction::triggered, [=]() { copyOrMoveImages(false); });

    m_trashAction = MAKE_ACTION(tr("Move to Trash"), "moveToTrash", "Del");
    action->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(action, SIGNAL(triggered()), this, SLOT(deleteOperation()));

    m_deleteAction = MAKE_ACTION(tr("Delete"), "delete", "Shift+Del");
    action->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(deletePermanentlyOperation()));

    m_saveAction = MAKE_ACTION(tr("Save"), "save", "Ctrl+S");
    action->setIcon(QIcon::fromTheme("document-save", QIcon(":/images/save.png")));
    action->setEnabled(false);

    m_saveAsAction = MAKE_ACTION_NOSC(tr("Save As"), "saveAs");
    action->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/images/save_as.png")));
    action->setEnabled(false);

    MAKE_ACTION(tr("Copy Image"), "copyImage", "Ctrl+Shift+C");
    MAKE_ACTION(tr("Paste Image"), "pasteImage", "Ctrl+Shift+V");

    MAKE_ACTION(tr("Rename"), "rename", "F2");
    connect(action, SIGNAL(triggered()), this, SLOT(rename()));

    MAKE_ACTION_NOSC(tr("Remove Metadata"), "removeMetadata");
    connect(action, SIGNAL(triggered()), this, SLOT(removeMetadata()));

    MAKE_ACTION_NOSC(tr("Select All"), "selectAll");
    connect(action, SIGNAL(triggered()), this, SLOT(selectAllThumbs()));

    MAKE_ACTION_NOSC(tr("About"), "about");
    connect(action, &QAction::triggered, [=](){MessageBox(this).about();});

    // Sort actions
    #define CHECKABLE_SORT action->setCheckable(true); connect(action, SIGNAL(triggered()), this, SLOT(sortThumbnails()));
    QAction *sortByName, *sortByTime, *sortByExifTime, *sortBySize, *sortByType, *sortBySimilarity,
            *sortByBrightness, *sortByColor;

    sortByName = MAKE_ACTION_NOSC(tr("Sort by Name"), "sortByName");
    CHECKABLE_SORT
    sortByTime = MAKE_ACTION_NOSC(tr("Sort by Time"), "sortByTime");
    CHECKABLE_SORT
    sortByExifTime = MAKE_ACTION_NOSC(tr("Sort by Time") + " (Exif)", "sortByExifTime");
    CHECKABLE_SORT
    sortBySize = MAKE_ACTION_NOSC(tr("Sort by Size"), "sortBySize");
    CHECKABLE_SORT
    sortByType = MAKE_ACTION_NOSC(tr("Sort by Type"), "sortByType");
    CHECKABLE_SORT
    sortBySimilarity = MAKE_ACTION_NOSC(tr("Sort by Similarity"), "sortBySimilarity");
    CHECKABLE_SORT
    sortByBrightness = MAKE_ACTION_NOSC(tr("Sort by Brightness"), "sortByBrightness");
    CHECKABLE_SORT
    sortByColor = MAKE_ACTION_NOSC(tr("Sort by Color"), "sortByColor");
    CHECKABLE_SORT

    if (thumbsViewer->thumbsSortFlags & QDir::Time)
        sortByTime->setChecked(true);
    else if (thumbsViewer->thumbsSortFlags & QDir::Size)
        sortBySize->setChecked(true);
    else if (thumbsViewer->thumbsSortFlags & QDir::Type)
        sortByType->setChecked(true);
    else
        sortByName->setChecked(true);

    MAKE_ACTION_NOSC(tr("Reverse Sort Order"), "sortReverse");
    CHECKABLE_SORT
    action->setChecked(thumbsViewer->thumbsSortFlags & QDir::Reversed);

    MAKE_ACTION(tr("Show Hidden Files"), "showHidden", "Ctrl+H");
    action->setCheckable(true);
    action->setChecked(Settings::showHiddenFiles);
    connect(action, SIGNAL(triggered()), this, SLOT(showHiddenFiles()));

    MAKE_ACTION_NOSC(tr("Small Toolbar Icons"), "smallToolbarIcons");
    action->setCheckable(true);
    action->setChecked(Settings::smallToolbarIcons);
    connect(action, SIGNAL(triggered()), this, SLOT(setToolbarIconSize()));

    MAKE_ACTION_NOSC(tr("Hide Dock Title Bars"), "lockDocks");
    action->setCheckable(true);
    action->setChecked(Settings::hideDockTitlebars);
    connect(action, SIGNAL(triggered()), this, SLOT(lockDocks()));

    MAKE_ACTION_NOSC(tr("Show Toolbar"), "showViewerToolbar");
    action->setCheckable(true);
    action->setChecked(Settings::showViewerToolbar);
    connect(action, &QAction::triggered, [=]() {
        Settings::showViewerToolbar = action->isChecked();
        m_imageToolBar->setVisible(Settings::showViewerToolbar);
        positionImageToolbar();
    });

    m_refreshAction = MAKE_ACTION(tr("Reload"), "refresh", "F5");
    action->setIcon(QIcon::fromTheme("view-refresh", QIcon(":/images/refresh.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(reload()));

    m_includeSubDirsAction = MAKE_ACTION_NOSC(tr("Include Sub-directories"), "includeSubDirs");
    action->setIcon(QIcon(":/images/tree.png"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered()), this, SLOT(setIncludeSubDirs()));

    MAKE_ACTION(tr("Paste Here"), "paste", "Ctrl+V");
    action->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/images/paste.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(pasteThumbs()));
    action->setEnabled(false);

    MAKE_ACTION_NOSC(tr("New Directory"), "createDir");
    connect(action, SIGNAL(triggered()), this, SLOT(createSubDirectory()));
    action->setIcon(QIcon::fromTheme("folder-new", QIcon(":/images/new_folder.png")));

    MAKE_ACTION_NOSC(tr("Set Save Directory"), "setSaveDir");
    connect(action, SIGNAL(triggered()), this, SLOT(setSaveDirectory()));
    action->setIcon(QIcon::fromTheme("folder-visiting", QIcon(":/images/folder-visiting.png")));

    m_goBackAction = MAKE_ACTION(tr("Back"), "goBack", "Alt+Left");
    m_goBackAction->setIcon(QIcon::fromTheme("go-previous", QIcon(":/images/back.png")));
    connect(m_goBackAction, &QAction::triggered, this, [=]() {
        if (currentHistoryIdx < 1)
            return;
        needHistoryRecord = false;
        goTo(pathHistoryList.at(--currentHistoryIdx));
        m_goFrwdAction->setEnabled(true);
        if (currentHistoryIdx == 0)
            m_goBackAction->setEnabled(false);
    });
    m_goBackAction->setEnabled(false);

    m_goFrwdAction = MAKE_ACTION(tr("Forward"), "goFrwd", "Alt+Right");
    m_goFrwdAction->setIcon(QIcon::fromTheme("go-next", QIcon(":/images/next.png")));
    connect(m_goFrwdAction, &QAction::triggered, this, [=]() {
        if (currentHistoryIdx > pathHistoryList.size() - 2)
            return;
        needHistoryRecord = false;
        goTo(pathHistoryList.at(++currentHistoryIdx));
        if (currentHistoryIdx == (pathHistoryList.size() - 1))
            m_goFrwdAction->setEnabled(false);
    });
    m_goFrwdAction->setEnabled(false);

    m_goUpAction = MAKE_ACTION(tr("Go Up"), "up", "Alt+Up");
    action->setIcon(QIcon::fromTheme("go-up", QIcon(":/images/up.png")));
    connect(action, &QAction::triggered, [=](){ goTo(QFileInfo(Settings::currentDirectory).dir().absolutePath()); });

    m_goHomeAction = MAKE_ACTION_NOSC(tr("Home"), "goHome");
    connect(action, &QAction::triggered, [=](){
        if (Settings::isFileListLoaded || Settings::filesList.isEmpty() || Settings::currentDirectory != QDir::homePath())
            goTo(QDir::homePath());
        else
            goTo("Phototonic::FileList");
    });
    action->setIcon(QIcon::fromTheme("go-home", QIcon(":/images/home.png")));

    MAKE_ACTION(tr("Slide Show"), "toggleSlideShow", "Ctrl+W");
    connect(action, SIGNAL(triggered()), this, SLOT(toggleSlideShow()));
    action->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/images/play.png")));

    m_nextImageAction = MAKE_ACTION(tr("Next Image"), "nextImage", "PgDown");
    action->setIcon(QIcon::fromTheme("go-next", QIcon(":/images/next.png")));
    connect(action, &QAction::triggered, [=](){ loadImage(Phototonic::Next); });

    m_prevImageAction = MAKE_ACTION(tr("Previous Image"), "prevImage", "PgUp");
    action->setIcon(QIcon::fromTheme("go-previous", QIcon(":/images/back.png")));
    connect(action, &QAction::triggered, [=](){ loadImage(Phototonic::Previous); });

    MAKE_ACTION(tr("First Image"), "firstImage", "Home");
    action->setIcon(QIcon::fromTheme("go-first", QIcon(":/images/first.png")));
    connect(action, &QAction::triggered, [=](){ loadImage(Phototonic::First); });

    MAKE_ACTION(tr("Last Image"), "lastImage", "End");
    action->setIcon(QIcon::fromTheme("go-last", QIcon(":/images/last.png")));
    connect(action, &QAction::triggered, [=](){ loadImage(Phototonic::Last); });

    MAKE_ACTION(tr("Random Image"), "randomImage", "Ctrl+D");
    connect(action, &QAction::triggered, [=](){ loadImage(Phototonic::Random); });

    m_viewImageAction = MAKE_ACTION(tr("View Image"), "open", "Return");
    action->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(viewImage()));

    MAKE_ACTION_NOSC(tr("Load Clipboard"), "showClipboard");
    action->setIcon(QIcon::fromTheme("insert-image", QIcon(":/images/new.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(newImage()));

    m_wallpaperAction = MAKE_ACTION_NOSC(tr("Set Wallpaper"), "setwallpaper");
    connect(action, &QAction::triggered, this, &Phototonic::runExternalApp);

    openWithSubMenu = new QMenu(tr("Open With..."));
    m_openWithMenuAction = MAKE_ACTION_NOSC(tr("Open With..."), "openWithMenu");
    action->setMenu(openWithSubMenu);

    MAKE_ACTION_NOSC(tr("External Applications"), "chooseApp");
    action->setIcon(QIcon::fromTheme("preferences-other", QIcon(":/images/settings.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(chooseExternalApp()));

    MAKE_ACTION_NOSC(tr("Add Bookmark"), "addBookmark");
    action->setIcon(QIcon(":/images/new_bookmark.png"));
    connect(action, &QAction::triggered, [=](){ addBookmark(getSelectedPath()); });

    MAKE_ACTION_NOSC(tr("Delete Bookmark"), "removeBookmark");
    action->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));

    MAKE_ACTION(tr("Zoom Out"), "zoomOut", "-");
    connect(action, &QAction::triggered, [=](){ zoom(-1.0f); });
    action->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/images/zoom_out.png")));

    MAKE_ACTION(tr("Zoom In"), "zoomIn", "+");
    connect(action, &QAction::triggered, [=](){ zoom(1.0f); });
    action->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/images/zoom_out.png")));

    MAKE_ACTION(tr("Reset Zoom"), "resetZoom", "*");
    action->setIcon(QIcon::fromTheme("zoom-fit-best", QIcon(":/images/zoom.png")));
    connect(action, &QAction::triggered, this, [=](){
        imageViewer->zoomTo(imageViewer->zoomMode() == ImageViewer::ZoomToFit ?
                                                                    ImageViewer::ZoomToFill :
                                                                    ImageViewer::ZoomToFit);
    });

    MAKE_ACTION(tr("Original Size"), "origZoom", "/");
    action->setIcon(QIcon::fromTheme("zoom-original", QIcon(":/images/zoom1.png")));
    connect(action, &QAction::triggered, this, [=](){ imageViewer->zoomTo(ImageViewer::ZoomOriginal); });

    MAKE_ACTION_NOSC(tr("Keep Zoom"), "keepZoom");
    action->setCheckable(true);
    connect(action, &QAction::toggled, this, [=](bool keep) {imageViewer->lockZoom(keep);});

    MAKE_ACTION(tr("Rotate 90° CCW"), "rotateLeft", "Ctrl+Left");
    action->setIcon(QIcon::fromTheme("object-rotate-left", QIcon(":/images/rotate_left.png")));
    connect(action, &QAction::triggered, this, [=]() { rotate(-90); });

    MAKE_ACTION(tr("Rotate 90° CW"), "rotateRight", "Ctrl+Right");
    action->setIcon(QIcon::fromTheme("object-rotate-right", QIcon(":/images/rotate_right.png")));
    connect(action, &QAction::triggered, this, [=]() { rotate(+90); });

    MAKE_ACTION_NOSC(tr("Rotate with mouse"), "rotateMouse");
    action->setIcon(QIcon::fromTheme("rotation-allowed", QIcon(":/images/rotate.png")));
    action->setCheckable(true);
    connect(action, &QAction::triggered, [=](){
        Settings::mouseRotateEnabled = action->isChecked();
        imageViewer->setFeedback(tr("Or try holding Shift"));
    });

    MAKE_ACTION(tr("Flip Horizontally"), "flipH", "Ctrl+Down");
    action->setIcon(QIcon::fromTheme("object-flip-horizontal", QIcon(":/images/flipH.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(flipHorizontal()));

    MAKE_ACTION(tr("Flip Vertically"), "flipV", "Ctrl+Up");
    action->setIcon(QIcon::fromTheme("object-flip-vertical", QIcon(":/images/flipV.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(flipVertical()));

    MAKE_ACTION(tr("Letterbox"), "letterbox", "Ctrl+G");
    action->setIcon(QIcon(":/images/crop.png"));
    connect(action, SIGNAL(triggered()), this, SLOT(cropImage()));

    MAKE_ACTION(tr("Scale Image"), "resize", "Ctrl+I");
    action->setIcon(QIcon::fromTheme("transform-scale", QIcon(":/images/scale.png")));
    connect(action, SIGNAL(triggered()), this, SLOT(scaleImage()));

    MAKE_ACTION(tr("Rotate 1° CCW"), "freeRotateLeft", "Ctrl+Shift+Left");
    connect(action, &QAction::triggered, this, [=]() { freeRotate(-1); });

    MAKE_ACTION(tr("Rotate 1° CW"), "freeRotateRight", "Ctrl+Shift+Right");
    connect(action, &QAction::triggered, this, [=]() { freeRotate(+1); });

    MAKE_ACTION(tr("Colors"), "colors", "Ctrl+O");
    connect(action, SIGNAL(triggered()), this, SLOT(showColorsDialog()));
    action->setIcon(QIcon(":/images/colors.png"));

    MAKE_ACTION_NOSC(tr("Crop"), "crop");
    action->setIcon(QIcon(":/images/crop.png"));
    connect(action, &QAction::triggered, [=](){ imageViewer->setEditMode(ImageViewer::Crop); });

    MAKE_ACTION_NOSC(tr("Blackout"), "blackout");
    action->setIcon(QIcon(":/images/blackout.png"));
    connect(action, &QAction::triggered, [=](){ imageViewer->setEditMode(ImageViewer::Blackout); });

    MAKE_ACTION_NOSC(tr("Cartouche"), "cartouche");
    action->setIcon(QIcon(":/images/cartouche.png"));
    connect(action, &QAction::triggered, [=](){ imageViewer->setEditMode(ImageViewer::Cartouche);  });

    MAKE_ACTION_NOSC(tr("Annotate"), "annotate");
    action->setIcon(QIcon(":/images/annotate.png"));
    connect(action, &QAction::triggered, [=](){ imageViewer->setEditMode(ImageViewer::Annotate); });

    m_findDupesAction = MAKE_ACTION_NOSC(tr("Find Duplicate Images"), "findDupes");
    action->setIcon(QIcon(":/images/duplicates.png"));
    action->setCheckable(true);
    connect(action, &QAction::triggered, [=]() {
        if (action->isChecked()) {
            sortByName->setChecked(false);
            sortByTime->setChecked(false);
            sortByExifTime->setChecked(false);
            sortBySize->setChecked(false);
            sortByType->setChecked(false);
            sortBySimilarity->setChecked(false);
            sortByBrightness->setChecked(false);
            sortByColor->setChecked(false);
            // scenario: user enters a filter and clicks the duplicate button
            QString error;
            if (!thumbsViewer->setFilter(filterLineEdit->text(), &error))
                QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height()*6/5)),
                                error, filterLineEdit);
        }
        refreshThumbs(true);
    });

    MAKE_ACTION(tr("Keep Transformations"), "keepTransform", "Ctrl+K");
    action->setCheckable(true);
    connect(action, &QAction::triggered, [=](){
        Settings::keepTransform = action->isChecked();
        imageViewer->setFeedback(Settings::keepTransform ? tr("Transformations Locked") : tr("Transformations Unlocked"));
//        imageViewer->refresh();
        });

    m_moveLeftAction = MAKE_ACTION(tr("Slide Image Left"), "moveLeft", "Left");
    connect(action, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(50, 0)); });
    m_moveRightAction = MAKE_ACTION(tr("Slide Image Right"), "moveRight", "Right");
    connect(action, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(-50, 0)); });
    m_moveUpAction = MAKE_ACTION(tr("Slide Image Up"), "moveUp", "Up");
    connect(action, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(0, 50)); });
    m_moveDownAction = MAKE_ACTION(tr("Slide Image Down"), "moveDown", "Down");
    connect(action, &QAction::triggered, [=](){ imageViewer->slideImage(QPoint(0, -50)); });

    MAKE_ACTION_NOSC(tr("Invert Selection"), "invertSelection");
    connect(action, SIGNAL(triggered()), thumbsViewer, SLOT(invertSelection()));

    // There could be a Batch submenu if we had any more items to put there
//    QMenu *batchSubMenu = new QMenu(tr("Batch"));
//    batchSubMenuAction = new QAction(tr("Batch"), this);
//    batchSubMenuAction->setMenu(batchSubMenu);
    m_batchTransformAction = MAKE_ACTION_NOSC(tr("Rotate and Crop images"), "batchTransform");
    connect(action, SIGNAL(triggered()), this, SLOT(batchTransform()));
//    batchSubMenu->addAction(batchTransformAction);

    MAKE_ACTION(tr("Filter by Name"), "filterImagesFocus", "Ctrl+F");
    connect(action, SIGNAL(triggered()), this, SLOT(filterImagesFocus()));
    MAKE_ACTION(tr("Edit Current Path"), "setPathFocus", "Ctrl+L");
    connect(action, SIGNAL(triggered()), this, SLOT(setPathFocus()));

    m_imageInfoAction = MAKE_ACTION_NOSC(tr("Image Info"), "imageinfo");
    action->setCheckable(true);
    connect(action, &QAction::triggered, [=]() {
        if (action->isChecked()) {
            m_infoViewer->read(imageViewer->fullImagePath);
            imageViewer->setFeedback(m_infoViewer->html(), false);
        } else {
            imageViewer->setFeedback("", false);
        }
    });

}

QAction *Phototonic::action(const QString name, bool dropCache) const {
    static QHash<QString,QAction*> actions;
    if (name.isEmpty()) {
        actions.clear();
//        qDebug() << "drop cache";
        return nullptr;
    }
    if (actions.isEmpty()) {
//        qDebug() << "build cache";
        QList<QAction*> actionlist = findChildren<QAction*>(Qt::FindDirectChildrenOnly);
        for (QAction *action : actionlist)
            actions[action->objectName()] = action;
    }
    QAction *act = actions.value(name);
    if (!act)
        qWarning() << "NO SUCH ACTION" << name;
    if (dropCache) {
//        qDebug() << "drop cache";
        actions.clear();
    }
    return act;
}

void Phototonic::createMenus() {

    QMenu *menu;
    menu = myMainMenu->addMenu(tr("&File"));
    menu->addAction(action("createDir"));
    menu->addAction(action("setSaveDir"));
    menu->addSeparator();
    menu->addAction(action("showClipboard"));
    menu->addAction(action("addBookmark"));
    menu->addSeparator();
    menu->addAction(action("findDupes"));
    menu->addSeparator();
    menu->addAction(action("exit"));

    menu = myMainMenu->addMenu(tr("&Edit"));
    menu->addAction(action("cut"));
    menu->addAction(action("copy"));
    menu->addAction(action("copyTo"));
    menu->addAction(action("moveTo"));
    menu->addAction(action("paste"));
    menu->addAction(action("rename"));
    menu->addAction(action("removeMetadata"));
    menu->addAction(action("moveToTrash"));
    menu->addAction(action("delete"));
    menu->addSeparator();
    menu->addAction(action("selectAll"));
    menu->addAction(action("invertSelection"));
    menu->addAction(action("batchTransform"));
//    menu->addAction(batchSubMenuAction);
    addAction(action("filterImagesFocus"));
    addAction(action("setPathFocus"));
    menu->addSeparator();
    menu->addAction(action("chooseApp"));
    menu->addAction(action("settings"));

    //: "go" like in go forward, backward, etc
    menu = myMainMenu->addMenu(tr("&Go"));
    menu->addAction(action("goBack"));
    menu->addAction(action("goFrwd"));
    menu->addAction(action("up"));
    menu->addAction(action("goHome"));
    menu->addSeparator();
    menu->addAction(action("prevImage"));
    menu->addAction(action("nextImage"));
    menu->addSeparator();
    menu->addAction(action("thumbsGoTop"));
    menu->addAction(action("thumbsGoBottom"));
    menu->addSeparator();
    menu->addAction(action("toggleSlideShow"));

    //: configure visual features of the app
    menu = myMainMenu->addMenu(tr("&View"));
    menu->addMenu(createPopupMenu())->setText(tr("Window"));
    menu->addSeparator();

    QActionGroup *thumbLayoutsGroup = new QActionGroup(this);
    thumbLayoutsGroup->addAction(action("setClassicThumbs"));
    thumbLayoutsGroup->addAction(action("setSquareThumbs"));
    thumbLayoutsGroup->addAction(action("setCompactThumbs"));
    menu->addActions(thumbLayoutsGroup->actions());
    menu->addSeparator();

    menu->addAction(action("thumbsZoomIn"));
    menu->addAction(action("thumbsZoomOut"));
    QMenu *sortMenu = menu->addMenu(tr("Thumbnails Sorting"));
    sortMenu->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    QActionGroup *sortTypesGroup = new QActionGroup(this);
    sortTypesGroup->addAction(action("sortByName"));
    sortTypesGroup->addAction(action("sortByTime"));
    sortTypesGroup->addAction(action("sortByExifTime"));
    sortTypesGroup->addAction(action("sortBySize"));
    sortTypesGroup->addAction(action("sortByType"));
    sortTypesGroup->addAction(action("sortBySimilarity"));
    sortTypesGroup->addAction(action("sortByBrightness"));
    sortTypesGroup->addAction(action("sortByColor"));
    sortMenu->addActions(sortTypesGroup->actions());
    sortMenu->addSeparator();
    sortMenu->addAction(action("sortReverse"));
    m_sortMenuAction = sortMenu->menuAction();
    menu->addSeparator();
    menu->addAction(action("includeSubDirs"));
    menu->addAction(action("showHidden"));
    menu->addSeparator();
    menu->addAction(action("refresh"));
    menu->addSeparator();

    myMainMenu->addAction(action("settings"));
    myMainMenu->addAction(action("about"));

    // thumbs viewer context menu
    thumbsViewer->addAction(action("open"));
    thumbsViewer->addAction(action("setwallpaper"));
    thumbsViewer->addAction(action("openWithMenu"));
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(m_sortMenuAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(action("cut"));
    thumbsViewer->addAction(action("copy"));
    thumbsViewer->addAction(action("paste"));
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(action("copyTo"));
    thumbsViewer->addAction(action("moveTo"));
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(action("selectAll"));
    thumbsViewer->addAction(action("invertSelection"));
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(action("batchTransform"));
//    thumbsViewer->addAction(batchSubMenuAction);
    thumbsViewer->addAction("")->setSeparator(true);
    thumbsViewer->addAction(action("removeMetadata"));
    thumbsViewer->addAction(action("moveToTrash"));
    thumbsViewer->addAction(action("delete"));
    thumbsViewer->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void Phototonic::createToolBars() {

    myMainToolBar = addToolBar("Toolbar");
    myMainToolBar->setObjectName("MainBar");
    myMainToolBar->setMovable(false);

    // edit
//    myMainToolBar->addAction(cutAction);
//    myMainToolBar->addAction(copyAction);
//    myMainToolBar->addAction(pasteAction);
//    myMainToolBar->addAction(deleteAction);
//    myMainToolBar->addAction(deletePermanentlyAction);
//    myMainToolBar->addAction(showClipboardAction);

    // navi
    myMainToolBar->addAction(action("goBack"));
    myMainToolBar->addAction(action("goFrwd"));
    myMainToolBar->addAction(action("up"));
    myMainToolBar->addAction(action("goHome"));
    myMainToolBar->addAction(action("refresh"));

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
    myMainToolBar->addAction(action("includeSubDirs"));
    myMainToolBar->addAction(m_sortMenuAction);
    static_cast<QToolButton*>(myMainToolBar->widgetForAction(m_sortMenuAction))->setPopupMode(QToolButton::InstantPopup);

    myMainToolBar->addAction(action("findDupes"));
    QToolButton *btn = static_cast<QToolButton*>(myMainToolBar->widgetForAction(m_findDupesAction));
    QMenu *btnmenu = new QMenu(btn);
    QWidgetAction *qwa = new QWidgetAction(btnmenu);
    QSpinBox *precision = new QSpinBox();
    precision->setRange(0, 100);
    precision->setSingleStep(5);
    precision->setPrefix(tr("Accuracy: "));
    precision->setSuffix("%");
    precision->setValue(Settings::dupeAccuracy);
    m_findDupesAction->setToolTip(m_findDupesAction->text() + "\n" + precision->text());
    connect(precision, &QSpinBox::valueChanged, [=](int v) {
        Settings::dupeAccuracy = v;
        m_findDupesAction->setToolTip(m_findDupesAction->text() + "\n" + precision->text());
    });
    QAction *act = new QAction(btn->text());
    act->setShortcut(Qt::Key_Enter);
    act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(act, &QAction::triggered, [=]() {
        m_findDupesAction->setChecked(false); m_findDupesAction->trigger();
    });
    precision->addAction(act);
    btnmenu->addAction(act);
    static const QString rtfmalso =
    tr( "Ok, this isn't exactly AI driven."
        "<p>Duplicates are detected via a grayscale mosaic<br>"
        "(<i>do the desaturated images look the same from very far away?</i>)<br>"
        "and by comparing the color distribution<br>"
        "(<i>immune against mirrors, rotation, anamorphic scales …</i>)<br>"
        "Both can cause funny false positives.</p>"
        "<p>The required proximity of the color distribution can be configured here<br>"
        "60% is a sensible default, but can be too easy if you're dealing with monochrome pictures<br>"
        "Going much lower will cause too many false positives, increase the accuracy to get rid of such</p>"
        "<h3>Notice that this can cause disjunct match groups!</h3>"
        "<p>[A] can be similar to [B] and [C], while [B] and [C] are not close enough.<br>"
        "The result is that [A] the <b>same image can show up multiple times!</b><br>"
        "Don't just assume the sorting is wrong these are clearly duplicates<br>"
        "and press delete. They are <b>the same image</b> and deleting one means to<br>"
        "delete both.</p><h3>Pay attention to the file names!</h3>"
    );
    precision->setToolTip(rtfmalso);
    precision->setToolTipDuration(120000);
    qwa->setDefaultWidget(precision);
    btnmenu->addAction(qwa);
    btn->setMenu(btnmenu);
    btn->setPopupMode(QToolButton::MenuButtonPopup);

    myMainToolBar->addAction(action("toggleSlideShow"));

    /* filter bar */
    filterLineEdit = new QLineEdit;
    filterLineEdit->setMinimumWidth(100);
    filterLineEdit->setMaximumWidth(200);
    //: hint for the filter lineedit, "/" triggers more hints at extended features
    filterLineEdit->setPlaceholderText(tr("Filter - try \"/?\"..."));
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
        "<li>Dimensions are pre/in/suffixed \"x\" ([width]x[height])</li>"
        "<li>Chromatic variance is suffixed [0-255]cr (real values will rarely be > 100)</li>"
        "<li>Luminance is suffixed [0.0-1.0]|[0-255]lm</li></ul>"
        "<i>All suffixes are case-insensitive but m|inute and M|onth</i><br>"
        "Subsequent \"/\" start a new sufficient condition group, the substring match is optional."
        "<hr>In addition you can filter for<br>"
        "<b>black, white, brown, dark, bright, warm, cold, monochrome, gray</b> and the colors<br>"
        "<b>red, orange, yellow, lime, green, mint, cyan, azure, blue, purple, magenta, pink</b>"
        "<hr>Leading colons <b>match tags</b>, so '/<b>:foo</b>' finds all images with the tag 'foo'");

    QTimer *filterBouncer = new QTimer(this);
    filterBouncer->setSingleShot(true);
    filterBouncer->setInterval(250);
    connect (filterBouncer, &QTimer::timeout, this, [=]() {
        if (filterLineEdit->text().contains("/?"))
            return;
        QString error;
        if (thumbsViewer->setFilter(filterLineEdit->text(), &error))
            QToolTip::showText(QPoint(), QString());
        else
            QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height()*6/5)),
                                error, filterLineEdit);
    });
    connect(filterLineEdit, &QLineEdit::textEdited, [=](){
        if (filterLineEdit->text().contains("/?"))
            QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height()*6/5)),
                                rtfm, filterLineEdit, {}, 300000);
        filterBouncer->start();
    });
    filterLineEdit->setMouseTracking(true);
    filterLineEdit->installEventFilter(this);

    myMainToolBar->addSeparator();
    myMainToolBar->addWidget(filterLineEdit);

    act = new QAction;
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
    m_imageToolBar = new QToolBar(tr("Viewer Toolbar"));
    m_imageToolBar->setObjectName("m_imageToolBar");
    for (const QString &s : Settings::imageToolActions) {
        const QString ts = s.trimmed();
        if (ts == "|")
            m_imageToolBar->addSeparator();
        else
            m_imageToolBar->addAction(action(ts));
    }
    m_imageToolBar->setOrientation(Qt::Vertical);

    QPalette pal = m_imageToolBar->palette();
    pal.setColor(m_imageToolBar->backgroundRole(), QColor(0,0,0,128));
    pal.setColor(m_imageToolBar->foregroundRole(), QColor(255,255,255,192));
    m_imageToolBar->setPalette(pal);
    m_imageToolBar->setAutoFillBackground(true);
    m_imageToolBar->setPalette(pal);
    m_imageToolBar->setIconSize(QSize(24,24));

    setToolbarIconSize();
}

void Phototonic::setToolbarIconSize() {
    if (initComplete) {
        Settings::smallToolbarIcons = action("smallToolbarIcons", true)->isChecked();
    }
    int iconSize = Settings::smallToolbarIcons ? 16 : 24;
    myMainToolBar->setIconSize(QSize(iconSize, iconSize));
}

void Phototonic::createFileSystemDock() {
    fileSystemDock = new QDockWidget(tr("File System"), this);
    fileSystemDock->setObjectName("File System");

    fileSystemTree = new FileSystemTree(fileSystemDock);
    fileSystemTree->addAction(action("createDir"));
    fileSystemTree->addAction(action("rename"));
    fileSystemTree->addAction(action("moveToTrash"));
    fileSystemTree->addAction(action("delete"));
    fileSystemTree->addAction(action("setwallpaper"));
    fileSystemTree->addAction(action("openWithMenu"));
    fileSystemTree->addAction(action("addBookmark"));
    fileSystemTree->setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(fileSystemTree, &FileSystemTree::clicked, this, [=]() { goTo(getSelectedPath()); });
    connect(fileSystemModel, &QFileSystemModel::rowsRemoved, this, &Phototonic::checkDirState);
    connect(fileSystemTree, &FileSystemTree::dropOp, this, &Phototonic::dropOp);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(fileSystemTree);

    QWidget *fileSystemTreeMainWidget = new QWidget(fileSystemDock);
    fileSystemTreeMainWidget->setLayout(mainLayout);

    fileSystemDock->setWidget(fileSystemTreeMainWidget);
    connect(fileSystemDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (visible && !fileSystemTree->model()) {
            QTimer::singleShot(50, [=](){
                fileSystemModel->setOptions(QFileSystemModel::Options());
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
    connect(action("removeBookmark"), SIGNAL(triggered()), bookmarks, SLOT(removeBookmark()));
    connect(bookmarks, SIGNAL(dropOp(Qt::KeyboardModifiers, bool, QString)),
            this, SLOT(dropOp(Qt::KeyboardModifiers, bool, QString)));

    addDockWidget(Qt::LeftDockWidgetArea, bookmarksDock);

    bookmarks->addAction(action("paste"));
    bookmarks->addAction(action("removeBookmark"));
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
                imagePreviewDock->setContentsMargins(0,0,0,0);
                int currentRow = thumbsViewer->currentIndex().row();
                if (currentRow > -1)
                    imageViewer->loadImage(thumbsViewer->fullPathOf(currentRow), thumbsViewer->icon(currentRow).pixmap(THUMB_SIZE_MAX).toImage());
            } else {
                QTimer::singleShot(100, this, [=](){if (!imageViewer->isVisible()) imageViewer->clearImage();});
            }
        }
    });
    addDockWidget(Qt::RightDockWidgetArea, imagePreviewDock);
}

void Phototonic::createImageTagsDock() {
    //: tags are image metadata
    tagsDock = new QDockWidget(tr("Tags"), this);
    tagsDock->setObjectName("Tags");
    m_imageTags = new ImageTags(tagsDock);
    tagsDock->setWidget(m_imageTags);

    connect(tagsDock, &QDockWidget::visibilityChanged, [=](bool visible) {
        if (Settings::layoutMode != ImageViewWidget) {
            Settings::tagsDockVisible = visible;
        }
    });
    connect(m_imageTags, &ImageTags::filterChanged, this,
        [=](const QStringList &mandatory, const QStringList &sufficient, bool invert) {
            thumbsViewer->setTagFilters(mandatory, sufficient, invert);
    });
    connect(m_imageTags, &ImageTags::tagRequest, [=](const QStringList &tagsAdded, const QStringList &tagsRemoved) {
        thumbsViewer->tagSelected(tagsAdded, tagsRemoved);
        if (m_infoViewer && m_infoViewer->isVisible())
            m_infoViewer->reloadExifData();
    });

    connect(thumbsViewer, &ThumbsViewer::filesHidden, m_imageTags, &ImageTags::removeTagsFor);
    connect(thumbsViewer, &ThumbsViewer::filesShown, m_imageTags, &ImageTags::addTagsFor);

    connect(thumbsViewer, &ThumbsViewer::selectionChanged, m_imageTags, [=]() {
        m_imageTags->setSelectedFiles(thumbsViewer->selectedFiles());
    });

    m_imageTags->populateTagsTree();
}

void Phototonic::sortThumbnails() {
    QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
    if (QAction *trigger = qobject_cast<QAction*>(sender())) {
        thumbsViewer->thumbsSortFlags &= QDir::IgnoreCase|QDir::Reversed;

        if (trigger->objectName() == "sortByName") {
            thumbModel->setSortRole(Qt::DisplayRole/* ThumbsViewer::SortRole */);
        } else if (trigger->objectName() == "sortByTime") {
            thumbsViewer->thumbsSortFlags |= QDir::Time;
            thumbModel->setSortRole(ThumbsViewer::TimeRole);
        } else if (trigger->objectName() == "sortByExifTime") {
            thumbModel->setSortRole(ThumbsViewer::DateTimeOriginal);
        } else if (trigger->objectName() == "sortBySize") {
            thumbsViewer->thumbsSortFlags |= QDir::Size;
            thumbModel->setSortRole(ThumbsViewer::SizeRole);
        } else if (trigger->objectName() == "sortByType") {
            thumbsViewer->thumbsSortFlags |= QDir::Type;
            thumbModel->setSortRole(ThumbsViewer::TypeRole);
        } else if (trigger->objectName() == "sortBySimilarity") {
            thumbModel->setSortRole(ThumbsViewer::HistogramRole);
        } else if (trigger->objectName() == "sortByBrightness") {
            thumbModel->setSortRole(ThumbsViewer::BrightnessRole);
        } else if (trigger->objectName() == "sortByColor") {
            thumbModel->setSortRole(ThumbsViewer::ColorRole);
        } else if (trigger->objectName() == "sortReverse") {
            if (trigger->isChecked())
                thumbsViewer->thumbsSortFlags |= QDir::Reversed;
            else
                thumbsViewer->thumbsSortFlags &= ~QDir::Reversed;
        }
    }

    if (thumbModel->sortRole() == ThumbsViewer::HistogramRole)
        thumbsViewer->scanForSort(ThumbsViewer::HistogramRole);
    else if (thumbModel->sortRole() == ThumbsViewer::BrightnessRole || thumbModel->sortRole() == ThumbsViewer::ColorRole)
        thumbsViewer->scanForSort(ThumbsViewer::BrightnessRole);

    thumbModel->sort(0, (thumbsViewer->thumbsSortFlags & QDir::Reversed) ? Qt::DescendingOrder : Qt::AscendingOrder);
    thumbsViewer->loadVisibleThumbs(-1);
}

void Phototonic::reload() {
    m_findDupesAction->setChecked(false);
    if (Settings::layoutMode == ThumbViewWidget) {
        refreshThumbs(false);
    } else {
        imageViewer->reload();
    }
}

void Phototonic::setIncludeSubDirs() {
    m_findDupesAction->setChecked(false);
    Settings::includeSubDirectories = m_includeSubDirsAction->isChecked();
    refreshThumbs(false);
}

void Phototonic::refreshThumbs(bool scrollToTop) {
    thumbsViewer->setNeedToScroll(scrollToTop);
    if (m_reloadPending)
        return;
    m_reloadPending = true;
    QMetaObject::invokeMethod(this, "reloadThumbs", Qt::QueuedConnection);
}

void Phototonic::showHiddenFiles() {
    QAction *showHiddenFiles = qobject_cast<QAction*>(sender());
    if (!showHiddenFiles || showHiddenFiles->objectName() != "showHidden") {
        qWarning() << "showHiddenFiles called w/o showHiddenFiles action!";
        return;
    }
    Settings::showHiddenFiles = showHiddenFiles->isChecked();
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
        char parameter = 2;
        if (execCommand.contains("%f", Qt::CaseInsensitive))
            execCommand.replace("%f", path, Qt::CaseInsensitive);
        else if (execCommand.contains("%u", Qt::CaseInsensitive))
            execCommand.replace("%u", QUrl::fromLocalFile(path).url(), Qt::CaseInsensitive);
        else
            --parameter;
        if (execCommand.contains("%tf", Qt::CaseInsensitive))
            execCommand.replace("%tf", thumbsViewer->locateThumbnail(path), Qt::CaseInsensitive);
        else if (execCommand.contains("%tu", Qt::CaseInsensitive))
            execCommand.replace("%tu", QUrl::fromLocalFile(thumbsViewer->locateThumbnail(path)).url(), Qt::CaseInsensitive);
        else if (execCommand.contains("%lat", Qt::CaseInsensitive) ||
                 execCommand.contains("%lon", Qt::CaseInsensitive) ||
                 execCommand.contains("%alt", Qt::CaseInsensitive)) {
            double lat, lon, alt;
            Metadata::gpsData(path, lat, lon, alt);
            execCommand.replace("%lat", QString::number(lat), Qt::CaseInsensitive);
            execCommand.replace("%lon", QString::number(lon), Qt::CaseInsensitive);
            execCommand.replace("%alt", QString::number(alt), Qt::CaseInsensitive);
        } else {
            --parameter;
        }
        if (!parameter)
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
                if (execCommand.contains("%f", Qt::CaseInsensitive) || execCommand.contains("%u", Qt::CaseInsensitive) ||
                    execCommand.contains("%tf", Qt::CaseInsensitive) || execCommand.contains("%tu", Qt::CaseInsensitive)) {
                    setStatus(tr("Commands using %f or %u cannot be used with multiple files."));
                    return;
                }
                for (int tn = selectedIdxList.size() - 1; tn >= 0; --tn) {
                    execCommand += " \"" + thumbsViewer->fullPathOf(selectedIdxList.at(tn).row()) + "\"";
                }
            }
        }
    }
    QStringList command = QProcess::splitCommand(execCommand);
    if (!QProcess::startDetached(command.takeFirst(), command)) {
        MessageBox(this).critical(tr("Error"), tr("Failed to start external application."));
    }
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
    openWithSubMenu->addAction(action("chooseApp", true));
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
        imageViewer->showFileName(Settings::showImageName);

        if (Settings::layoutMode == ImageViewWidget) {
            imageViewer->reload();
            needThumbsRefresh = true;
            action("rotateMouse", true)->setChecked(Settings::mouseRotateEnabled);
        } else {
            thumbsViewer->refreshThumbs();
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
    Settings::isFullScreen = m_fullScreenAction->isChecked();
    imageViewer->setCursorHiding(Settings::isFullScreen);
    const bool useDock = imagePreviewDock->isFloating() && imagePreviewDock->isVisible();
    if (m_fullScreenAction->isChecked()) {
        if (useDock) {
            delete imagePreviewDock->titleBarWidget();
            imagePreviewDock->setTitleBarWidget(new QWidget(imagePreviewDock));
            imagePreviewDock->setWindowState(imagePreviewDock->windowState() | Qt::WindowFullScreen);
        } else {
            setWindowState(windowState() | Qt::WindowFullScreen);
        }
    }
    else {
        if (useDock) {
            imagePreviewDock->setWindowState(imagePreviewDock->windowState() & ~Qt::WindowFullScreen);
            delete imagePreviewDock->titleBarWidget();
            imagePreviewDock->setTitleBarWidget(nullptr);
        }
        // unconditionally in case the dock state changed and we need to get out of the fullscreen
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
}

void Phototonic::selectAllThumbs() {
    thumbsViewer->selectAll();
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
    action("paste", true)->setEnabled(true);

    QString state = Settings::isCopyOperation ? tr("Copied %n image(s) to clipboard", "", copyCutThumbsCount)
                                              : tr("Cut %n image(s) to clipboard", "", copyCutThumbsCount);
    setStatus(state);
}

void Phototonic::copyOrMoveImages(bool isCopyOperation) {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    if (!isCopyOperation && thumbsViewer->isBusy()) { // defer, don't alter while the thumbsviewer is loading stuff
        QTimer::singleShot(100, this, [=](){copyOrMoveImages(isCopyOperation);});
        return;
    }

    imageViewer->setCursorHiding(false);

    auto cleanup = [=]() { if (isFullScreen()) imageViewer->setCursorHiding(true); };

    CopyMoveToDialog copyMoveToDialog(this, getSelectedPath(), isCopyOperation);
    if (!copyMoveToDialog.exec())
        return cleanup();

    bookmarks->reloadBookmarks();
    if (Settings::layoutMode == ThumbViewWidget) {
        copyOrCutThumbs(isCopyOperation);
        pasteThumbs(copyMoveToDialog.destination());
        return cleanup();
    }
    if (imageViewer->isNewImage()) {
        showNewImageWarning();
        return cleanup();
    }

    QString destFile = copyMoveToDialog.destination() + QDir::separator() + QFileInfo(imageViewer->fullImagePath).fileName();
    if (QFileInfo::exists(destFile)) {
        QMap<QString,QString> conflict;
        conflict[imageViewer->fullImagePath] = destFile;

        if (CopyOrMove::resolveConflicts(conflict, this) != QDialog::Accepted)
            return cleanup();

        destFile = conflict.value(imageViewer->fullImagePath);
        if (destFile.isEmpty())
            return cleanup();
    }

    int result = CopyOrMove::file(imageViewer->fullImagePath, destFile, isCopyOperation);
    if (!result) {
        MessageBox(this).critical(tr("Error"), tr("Failed to copy or move image."));
    } else {
        if (!isCopyOperation) {
            int currentRow = thumbsViewer->currentIndex().row();
            thumbsViewer->model()->removeRow(currentRow);
            loadCurrentImage(currentRow);
        }
    }
}

void Phototonic::resizeThumbs() {
    if (!m_thumbSizeDelta)
        return;
    const int previous = thumbsViewer->thumbSize;
    thumbsViewer->thumbSize += m_thumbSizeDelta*THUMB_SIZE_MIN;
    if (m_thumbSizeDelta > 0)
        thumbsViewer->thumbSize = qMin(thumbsViewer->thumbSize,THUMB_SIZE_MAX);
    else
       thumbsViewer->thumbSize = qMax(thumbsViewer->thumbSize,THUMB_SIZE_MIN);
    m_thumbSizeDelta = 0;
    action("thumbsZoomOut")->setEnabled(thumbsViewer->thumbSize > THUMB_SIZE_MIN);
    action("thumbsZoomIn", true)->setEnabled(thumbsViewer->thumbSize < THUMB_SIZE_MAX);
    if (thumbsViewer->thumbSize != previous)
        thumbsViewer->refreshThumbs();
}


void Phototonic::zoom(float multiplier, QPoint focus) {
    // sanitize to 10% step, necessary for unscale image and zoominator
    float zoomTarget = qRound(imageViewer->zoom()*10)*0.1;

    if (multiplier > 0.0 && zoomTarget == 16.0) {
        imageViewer->setFeedback(tr("Maximum Zoom"));
        return;
    }
    if (multiplier < 0.0 && zoomTarget == 0.1) {
        imageViewer->setFeedback(tr("Minimum Zoom"));
        return;
    }

    // by size
    multiplier *= zoomTarget * 0.5;
    if (focus.x() >= 0) {
        // by speed
        static QElapsedTimer speedometer;
        if (!speedometer.isValid() || speedometer.elapsed() > 250)
            multiplier *= 0.05;
        else if (speedometer.elapsed() > 150)
            multiplier *= 0.1;
        else if (speedometer.elapsed() > 75)
            multiplier *= 0.5;
        speedometer.restart();
    } else {
        multiplier *= 0.5;
    }

    // round and limit to 10%
    multiplier = multiplier > 0.0 ? qMax(0.1, qRound(multiplier*10)*0.1) : qMin(-0.1, qRound(multiplier*10)*0.1);


    zoomTarget = qMin(16.0, qMax(0.1, zoomTarget + multiplier));
    imageViewer->zoomTo(zoomTarget, focus);

    //: nb the trailing "%" for eg. 80%
    imageViewer->setFeedback(tr("Zoom %1%").arg(QString::number(zoomTarget * 100)));
}

void Phototonic::rotate(int deg) {
    if (!deg)
        return;

    qreal rotation = Settings::rotation + deg;
    if (qAbs(rotation) > 360.0)
        rotation -= int(360*rotation)/360;
    if (deg > 0) {
        rotation = 90*int(rotation/90);
        if (rotation > 270)
            rotation -= 360;
    } else {
        rotation = 90*qCeil(rotation/90);
        if (rotation < 0)
            rotation += 360;
    }
    // wrap the starting angle for the animation, so we don't rotate backwards
    if (deg > 0 && rotation < Settings::rotation)
        Settings::rotation -= 360.0;
    if (deg < 0 && rotation > Settings::rotation)
        Settings::rotation += 360.0;
#if 1
    static QVariantAnimation *rotator = nullptr;
    if (!rotator) {
        rotator = new QVariantAnimation(this);
        rotator->setEasingCurve(QEasingCurve::InOutCubic);
        connect(rotator, &QVariantAnimation::valueChanged, [=](const QVariant &value) {
            if (rotator->state() != QAbstractAnimation::Running)
                    return;
            Settings::rotation = value.toReal();
            imageViewer->resizeImage();
        });
        connect(rotator, &QObject::destroyed, [=]() {rotator = nullptr;});
    }
    rotator->setDuration(2*qAbs(rotation-Settings::rotation));
    rotator->setStartValue(Settings::rotation);
    rotator->setEndValue(rotation);
    rotator->start();
#else
    Settings::rotation = rotation;
    imageViewer->resizeImage();
#endif
    imageViewer->setFeedback(tr("Rotation %1°").arg(QString::number(rotation)));
    m_editSteps = qMax(0, m_editSteps + (rotation ? 1 : -1));
    m_saveAction->setEnabled(m_editSteps);
    m_saveAsAction->setEnabled(m_editSteps);
}

void Phototonic::flipVertical() {
    const bool flips = imageViewer->flip(Qt::Vertical);
    m_editSteps = qMax(0, m_editSteps + (flips ? 1 : -1));
    m_saveAction->setEnabled(m_editSteps);
    m_saveAsAction->setEnabled(m_editSteps);
}

void Phototonic::flipHorizontal() {
    const bool flips = imageViewer->flip(Qt::Horizontal);
    m_editSteps = qMax(0, m_editSteps + (flips ? 1 : -1));
    m_saveAction->setEnabled(m_editSteps);
    m_saveAsAction->setEnabled(m_editSteps);
}

void Phototonic::cropImage() {
    if (Settings::slideShowActive)
        toggleSlideShow();

    imageViewer->setCursorHiding(false);
    imageViewer->configureLetterbox();
    if (isFullScreen())
        imageViewer->setCursorHiding(true);
}

void Phototonic::scaleImage() {
    if (Settings::slideShowActive)
        toggleSlideShow();

    imageViewer->setCursorHiding(false);
//    if (Settings::layoutMode == ImageViewWidget) {
        ResizeDialog dlg(imageViewer->currentImageSize(), imageViewer);
        if (dlg.exec() == QDialog::Accepted) {
            imageViewer->scaleImage(dlg.newSize());
        }
//    } else {
//        ASSERT_IMAGES_SELECTED
        /// @todo: looks like there were plans to allow mass-resizing from the thumbnail browser
//    }
    if (isFullScreen())
        imageViewer->setCursorHiding(true);
}

void Phototonic::freeRotate(int deg) {
    Settings::rotation += deg;
    if (Settings::rotation < 0)
        Settings::rotation = 359;
    if (Settings::rotation > 360)
        Settings::rotation = 1;
    imageViewer->resizeImage();
    imageViewer->setFeedback(tr("Rotation %1°").arg(QString::number(Settings::rotation)));
}

void Phototonic::batchTransform() {
    QModelIndexList idxs = thumbsViewer->selectionModel()->selectedIndexes();
    if (idxs.count() < 2) {
        return MessageBox(this).critical(tr("No images selected"), tr("Please select the images to transform."));
    }
    QRect cropRect = imageViewer->lastCropGeometry();
    if (!cropRect.isValid()) {
        MessageBox(this).warning(   tr("No crop area defined"),
                                    tr( "<h3>Define a crop area</h3>"
                                        "<p>Open an image, maybe rotate it.<br>"
                                        "Then press and hold ctrl to select a crop rect.<br>"
                                        "Do <b>not</b> apply the crop by double clicking the selection!<br>"
                                        "If not using the preview, exit the Viewer.</p>"
                                        "You can now replay the action on multiple images."));
        return;
    }
    QString message;
    bool createBackups = true;
    if (Settings::saveDirectory.isEmpty()) {
        if (MessageBox(this, MessageBox::Yes|MessageBox::No, MessageBox::Yes).warning(
                tr("Create backups?"), tr("No global save directory is defined, the images will be overwritten."
                                          "<h3>Do you want to create backups?</h3>")) == MessageBox::No) {
            createBackups = false;
        }
        message = createBackups ? tr("Create backups and overwrite the original files") : tr("Overwrite the original files");
    }
    else {
        message = tr("Save the transformed images to %1").arg(Settings::saveDirectory);
    }
    MessageBox msgBox(this, MessageBox::Ok | MessageBox::Cancel, MessageBox::Ok);
    msgBox.setInformativeText(tr("<ul><li>Rotate %1 images by %2°</li>"
                                 "<li>Crop them to %3+%4+%5x%6</li>"
                                 "<li>%7</li></ul>").arg(idxs.count()).arg(Settings::rotation, 0, 'f', 1)
                                                    .arg(cropRect.x()).arg(cropRect.y())
                                                    .arg(cropRect.width()).arg(cropRect.height())
                                                    .arg(message));
    if (msgBox.ask(tr("Batch transformation"), tr("<h3>Perform batch transformation?</h3>")) != MessageBox::Ok)
        return;

    if (createBackups) {
        for (QModelIndex i : idxs) {
            QString path = thumbsViewer->fullPathOf(i.row()); // copyFile rewrites the destination
            int result = CopyOrMove::copyFile(path, path);
            if (!result) {
                return MessageBox(this).critical(tr("Error"), tr("Failed to copy or move image."));
            }
        }
    }

    bool keepTransformWas = Settings::keepTransform;
    Settings::keepTransform = imageViewer->batchMode = true;
    setUpdatesEnabled(false);
    for (QModelIndex i : idxs) {
        loadSelectedThumbImage(i);
        imageViewer->applyCropAndRotation();
        imageViewer->saveImage();
    }
    Settings::keepTransform = keepTransformWas;
    imageViewer->batchMode = false;
    setUpdatesEnabled(true);
    thumbsViewer->refreshThumbs();
}

void Phototonic::showColorsDialog() {
    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    if (!colorsDialog) {
        colorsDialog = new ColorsDialog(this, imageViewer);
        connect(colorsDialog, &QDialog::finished, [=](){ Settings::colorsActive = false; });
        connect(imageViewer, &ImageViewer::imageEdited, [=](bool dirty){ if (!dirty)colorsDialog->reset(); });
    }

    imageViewer->setCursorHiding(false);
    Settings::colorsActive = true;
    colorsDialog->exec();
    if (isFullScreen())
        imageViewer->setCursorHiding(true);
}

static bool isWritableDir(const QString &path) {
    QFileInfo info(path);
    return info.exists() && info.isDir() && info.isWritable();
}
static bool isReadableDir(const QString &path) {
    QDir checkPath(path);
    return checkPath.exists() && checkPath.isReadable();
}

void Phototonic::pasteThumbs(QString destDir) {
    static bool pasteInProgress = false;
    if (!copyCutThumbsCount) {
        return;
    }

    if (destDir.isNull()) {
        if (QApplication::focusWidget() != bookmarks) {
            destDir = getSelectedPath();
        } else if (bookmarks->currentItem()) {
            destDir = bookmarks->currentItem()->toolTip(0);
        }
    }

    if (!isWritableDir(destDir)) {
        if (destDir.isEmpty())
            destDir = "0x0000";
        MessageBox(this).critical(tr("Error"), tr("Can not copy or move to %1").arg(destDir));
        return selectCurrentViewDir();
    }

    bool pasteInCurrDir = (Settings::currentDirectory == destDir);
    QFileInfo fileInfo;
    if (!Settings::isCopyOperation && pasteInCurrDir) {
        for (int thumb = 0; thumb < Settings::copyCutFileList.size(); ++thumb) {
            fileInfo = QFileInfo(Settings::copyCutFileList.at(thumb));
            if (fileInfo.absolutePath() == destDir) {
                return MessageBox(this).critical(tr("Error"), tr("Can not move to the same directory"));
            }
        }
    }

    if (pasteInProgress || thumbsViewer->isBusy()) { // defer, don't alter while the thumbsviewer is loading stuff
        QTimer::singleShot(100, this, [=](){pasteThumbs();});
        return;
    }

    pasteInProgress = true;
    copyCutThumbsCount = 0; // setting this here avoids nested pastes
    int latestRow = CopyOrMove::list(thumbsViewer, destDir, pasteInCurrDir, this);
    int n = 0;
    if (!pasteInCurrDir && thumbsViewer->model()->rowCount()) {
        n = Settings::copyCutIndexList.size();
        thumbsViewer->setCurrentIndex(qMin(latestRow, thumbsViewer->model()->rowCount() - 1));
    }
    QString state = Settings::isCopyOperation ? tr("Copied %n image(s)", "", n) : tr("Moved %n image(s)", "", n);
    setStatus(state);
    selectCurrentViewDir();

    Settings::copyCutIndexList.clear();
    Settings::copyCutFileList.clear();
    action("paste", true)->setEnabled(false);

    thumbsViewer->loadVisibleThumbs();
    pasteInProgress = false;
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
    if (m_deleteInProgress)
        return; // no.
    if (thumbsViewer->isBusy()) { // defer, don't alter while the thumbsviewer is loading stuff
        QTimer::singleShot(100, this, [=](){deleteImages(trash);});
        return;
    }
    ASSERT_IMAGES_SELECTED

    QStringList deathRow;
    for (QModelIndex idx : thumbsViewer->selectionModel()->selectedIndexes())
            deathRow << thumbsViewer->fullPathOf(idx.row());

    if (Settings::deleteConfirm) {
        MessageBox msgBox(this, MessageBox::Yes|MessageBox::Cancel, MessageBox::Yes);
        QString ttl = trash ? tr("Move to Trash") : tr("Delete images");
        QString msg = trash ? tr("Move %n selected image(s) to the trash?", "", deathRow.size())
                            : tr("Permanently delete %n selected image(s)?", "", deathRow.size());
        QString fileList;
        msgBox.setDetailedText(deathRow.join("\n"));
        if (QTextEdit *details = msgBox.findChild<QTextEdit*>())
            details->setWordWrapMode(QTextOption::NoWrap);
        if (msgBox.warning(ttl, msg) != MessageBox::Yes) {
            return;
        }
    }

    // To only show progress dialog if deleting actually takes time
    QElapsedTimer timer;
    timer.start();
    int totalTime = 0;
    // Avoid a lot of not interesting updates while deleting
    QSignalBlocker fsBlocker(fileSystemModel);
    QSignalBlocker scrollbarBlocker(thumbsViewer->verticalScrollBar());

    // Avoid reloading thumbnails all the time
    m_deleteInProgress = true;

    QProgressDialog *progress = nullptr;
    int deleteFilesCount = 0;
    int cycle = 1;
    QStringList filesRemoved;
    for (int i = 0; i < deathRow.size(); ++i) {

        // Only show if it takes a lot of time, since popping this up for just
        // deleting a single image is annoying
        const QString &fileNameFullPath = deathRow.at(i);
        if (timer.elapsed() > 30) {
            if ((totalTime += timer.elapsed()) > 250) {
                totalTime = 0;
                if (!progress && float(i)/deathRow.size() < 1.0f-1.0f/++cycle) {
                    progress = new QProgressDialog(this);
                    progress->setMaximum(deathRow.size());
                    QLabel *l = new QLabel(progress);
                    l->setWordWrap(true);
                    l->setFixedWidth(QFontMetrics(l->font()).averageCharWidth()*80);
                    progress->setLabel(l);
                    progress->show();
                }
            }
            timer.restart();
            if (progress) {
                progress->setValue(i);
                progress->setLabelText(tr("Deleting %1").arg(fileNameFullPath));
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }

        // w/o the file the canonical path cannot be resolved
        ThumbsViewer::removeFromCache(fileNameFullPath);

        QFile fileToRemove(fileNameFullPath);
        bool deleteOk;
        if (trash)
            deleteOk = fileToRemove.moveToTrash();
        else
            deleteOk = fileToRemove.remove();

        ++deleteFilesCount;
        if (deleteOk) {
            filesRemoved << fileNameFullPath;
            QModelIndexList indexList = thumbsViewer->model()->match(thumbsViewer->model()->index(0, 0), ThumbsViewer::FileNameRole, fileNameFullPath);
            if (indexList.size())
                thumbsViewer->model()->removeRow(indexList.at(0).row());
        } else {
            MessageBox(this).critical(tr("Error"), (trash ? tr("Failed to move image to the trash.") :
                                                            tr("Failed to delete image.")) + "\n" + fileToRemove.errorString());
            break;
        }

        Settings::filesList.removeOne(fileNameFullPath);

        if (progress && progress->wasCanceled()) {
            break;
        }
    }

    if (progress) {
        progress->close();
        progress->deleteLater();
    }

    m_imageTags->removeTagsFor(filesRemoved);
    for (const QString &path : filesRemoved)
        Metadata::forget(path);

    setStatus(tr("Deleted %n image(s)", "", deleteFilesCount));

    m_deleteInProgress = false;
}

void Phototonic::deleteFromViewer(bool trash) {
    if (m_deleteInProgress)
        return; // no.

    if (imageViewer->isNewImage()) {
        showNewImageWarning();
        return;
    }

    if (Settings::slideShowActive) { // needs to happen now
        toggleSlideShow();
    }
    imageViewer->setCursorHiding(false); // tells that sth. is happening

    if (thumbsViewer->isBusy()) { // defer, don't alter while the thumbsviewer is loading stuff
        QTimer::singleShot(100, this, [=](){deleteFromViewer(trash);});
        return;
    }

    const QString fullPath = imageViewer->fullImagePath;
    const QString fileName = QFileInfo(fullPath).fileName();

    if (Settings::deleteConfirm) {
        MessageBox msgBox(imageViewer, MessageBox::Yes|MessageBox::Cancel, MessageBox::Yes);
        QString ttl = trash ? tr("Move to Trash") : tr("Delete images");
        QString msg = trash ? tr("Move %1 to the trash").arg(fileName) : tr("Permanently delete %1").arg(fileName);

        if (msgBox.warning(ttl, msg) != MessageBox::Yes) {
            if (isFullScreen())
                imageViewer->setCursorHiding(true);
            return;
        }
    }

    QString trashError;
    if (trash ? QFile::moveToTrash(fullPath) : QFile::remove(fullPath)) {
        m_deleteInProgress = true;
        int currentRow = thumbsViewer->currentIndex().row();
        loadImage(Phototonic::Next);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents); // process index changed, it's a queued connection
        thumbsViewer->model()->removeRow(currentRow);
        imageViewer->setFeedback(tr("Deleted %1").arg(fileName));
        m_deleteInProgress = false;
    } else {
        MessageBox(imageViewer).critical(tr("Error"), tr("Failed to delete image"));
    }
    Metadata::forget(fullPath);
    ThumbsViewer::removeFromCache(fullPath);

    if (isFullScreen())
        imageViewer->setCursorHiding(true);
}

// Main delete operation
void Phototonic::deleteOperation() {

    if (QApplication::focusWidget() == bookmarks) {
        bookmarks->removeBookmark();
        return;
    }

    if (QApplication::focusWidget() == fileSystemTree) {
        deleteDirectory(true);
        return;
    }

    if (Settings::layoutMode == ImageViewWidget ||
            (imagePreviewDock->isFloating() && QApplication::focusWidget() == imageViewer)) {
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

    if (Settings::layoutMode == ImageViewWidget ||
            (imagePreviewDock->isFloating() && QApplication::focusWidget() == imageViewer)) {
        deleteFromViewer(false);
        return;
    }

    deleteImages(false);
}

void Phototonic::goTo(QString path) {
    m_includeSubDirsAction->setChecked(false);
    m_findDupesAction->setChecked(false);
    setFileListMode(path == "Phototonic::FileList");
    if (!Settings::isFileListLoaded) {
        Settings::currentDirectory = path;
        fileSystemTree->setCurrentIndex(fileSystemModel->index(path));
        selectCurrentViewDir();
    }
    refreshThumbs(true);
}

void Phototonic::goPathBarDir() {
    if (pathLineEdit->completer()->popup())
        pathLineEdit->completer()->popup()->hide();
    QMap<QString,QString>::const_iterator it = Settings::bangs.constFind(pathLineEdit->text().section(':', 0, 0));
    if (it != Settings::bangs.constEnd()) {
        QString command = it.value();
        command.replace("%s", pathLineEdit->text().section(':', 1));
        QProcess *job = new QProcess(this);
        connect(job, &QProcess::readyReadStandardOutput, this, [=]() {
            QByteArray ba = job->readAllStandardOutput();
            QStringList list = QString::fromLocal8Bit(ba).split('\n');
            loadStartupFileList(list, 0);
            thumbsViewer->reload(true);
        });
        connect(job, &QProcess::finished, job, &QObject::deleteLater);
        Settings::filesList.clear();
        setFileListMode(true);
        reload();
        job->startCommand(command);
        return;
    }
    if (!isReadableDir(pathLineEdit->text())) {
        MessageBox(this).critical(tr("Error"), tr("Invalid Path: %1").arg(pathLineEdit->text()));
        pathLineEdit->setText(Settings::currentDirectory);
        return;
    }
    thumbsViewer->setFocus(Qt::OtherFocusReason);
    goTo(pathLineEdit->text());
}

void Phototonic::updateActions() {

    auto toggleFileSpecificActions = [&](bool on) {
        m_cutAction->setEnabled(on);
        m_copyAction->setEnabled(on);
        m_copyToAction->setEnabled(on);
        m_moveToAction->setEnabled(on);
        m_viewImageAction->setEnabled(on);
        m_wallpaperAction->setEnabled(on);
        m_openWithMenuAction->setEnabled(on);
//        batchSubMenuAction->setEnabled(on);
        m_batchTransformAction->setEnabled(on);
        m_deleteAction->setEnabled(on);
        m_trashAction->setEnabled(on);
    };

    if (QApplication::focusWidget() == thumbsViewer) {
        toggleFileSpecificActions(thumbsViewer->selectionModel()->selectedIndexes().size() > 0);
        m_batchTransformAction->setEnabled(thumbsViewer->selectionModel()->selectedIndexes().size() > 1);
    } else if (QApplication::focusWidget() == bookmarks) {
        toggleFileSpecificActions(false);
    } else if (QApplication::focusWidget() == fileSystemTree) {
        toggleFileSpecificActions(false);
        m_deleteAction->setEnabled(true); // also used for the filesystem tree
        m_trashAction->setEnabled(true);
    } else if (Settings::layoutMode == ImageViewWidget || QApplication::focusWidget() == imageViewer) {
        toggleFileSpecificActions(true);
    } else {
        toggleFileSpecificActions(false);
    }

    if (Settings::layoutMode == ImageViewWidget) {
        setViewerKeyEventsEnabled(true);
        m_fullScreenAction->setEnabled(true);
        m_closeImageAction->setEnabled(true);
        m_imageInfoAction->setShortcuts(QList<QKeySequence>() << m_imageInfoAction->shortcut() << Qt::Key_I);
    } else {
        if (QApplication::focusWidget() == imageViewer) {
            setViewerKeyEventsEnabled(true);
            m_imageInfoAction->setShortcuts(QList<QKeySequence>() << m_imageInfoAction->shortcut() << Qt::Key_I);
            m_fullScreenAction->setEnabled(imagePreviewDock->isFloating());
            m_closeImageAction->setEnabled(false);
        } else {
            m_imageInfoAction->setShortcuts(QList<QKeySequence>() << m_imageInfoAction->shortcut());
            setViewerKeyEventsEnabled(false);
            m_fullScreenAction->setEnabled(false);
            m_closeImageAction->setEnabled(false);
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
    Settings::setValue(Settings::optionDefaultSaveQuality, Settings::defaultSaveQuality);
    Settings::setValue(Settings::optionSlideShowDelay, Settings::slideShowDelay);
    Settings::setValue(Settings::optionSlideShowRandom, (bool) Settings::slideShowRandom);
    Settings::setValue(Settings::optionSlideShowCrossfade, (bool) Settings::slideShowCrossfade);
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
    Settings::setValue("DuplicateHistogramProximity", (int) Settings::dupeAccuracy);

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

    Settings::beginGroup(Settings::optionBangs);
    Settings::appSettings->remove("");
    QMapIterator<QString, QString> bIter(Settings::bangs);
    while (bIter.hasNext()) {
        bIter.next();
        Settings::appSettings->setValue(bIter.key(), bIter.value());
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

    /* save exif filters */
    Settings::beginGroup(Settings::optionExifFilters);
    Settings::appSettings->remove("");
    QMapIterator<QString, QString> efIter(Settings::exifFilters);
    while (efIter.hasNext()) {
        efIter.next();
        Settings::appSettings->setValue(efIter.key(), efIter.value());
    }
    Settings::appSettings->endGroup();
}

void Phototonic::readSettings() {
    initComplete = false;
    needThumbsRefresh = false;

    Settings::viewerBackgroundColor = Settings::value(Settings::optionViewerBackgroundColor, QColor(39,39,39)).value<QColor>();
    Settings::enableAnimations = Settings::value(Settings::optionEnableAnimations, true).toBool();
    Settings::exifRotationEnabled = Settings::value(Settings::optionExifRotationEnabled, true).toBool();
    Settings::exifThumbRotationEnabled = Settings::value(Settings::optionExifThumbRotationEnabled, true).toBool();
    Settings::thumbsLayout = Settings::value(Settings::optionThumbsLayout, ThumbsViewer::Classic).toInt();
    Settings::reverseMouseBehavior = Settings::value(Settings::optionReverseMouseBehavior, false).toBool();
    Settings::scrollZooms = Settings::value(Settings::optionScrollZooms, false).toBool();
    Settings::deleteConfirm = Settings::value(Settings::optionDeleteConfirm, true).toBool();
    Settings::showHiddenFiles = Settings::value(Settings::optionShowHiddenFiles, false).toBool();
    Settings::wrapImageList = Settings::value(Settings::optionWrapImageList, false).toBool();
    Settings::defaultSaveQuality = Settings::value(Settings::optionDefaultSaveQuality, 90).toInt();
    Settings::slideShowDelay = Settings::value(Settings::optionSlideShowDelay, 5.0).toDouble();
    Settings::slideShowRandom = Settings::value(Settings::optionSlideShowRandom, false).toBool();
    Settings::slideShowCrossfade = Settings::value(Settings::optionSlideShowCrossfade, true).toBool();
    Settings::showImageName = Settings::value(Settings::optionShowImageName, false).toBool();
    Settings::smallToolbarIcons = Settings::value(Settings::optionSmallToolbarIcons, true).toBool();
    Settings::hideDockTitlebars = Settings::value(Settings::optionHideDockTitlebars, false).toBool();
    Settings::showViewerToolbar = Settings::value(Settings::optionShowViewerToolbar, false).toBool();
    Settings::setWindowIcon = Settings::value(Settings::optionSetWindowIcon, false).toBool();
    Settings::upscalePreview = Settings::value(Settings::optionUpscalePreview, false).toBool();
    Settings::dupeAccuracy = Settings::value("DuplicateHistogramProximity", 60).toInt();
    const QString defaultTools(
        "prevImage,nextImage,firstImage,lastImage,toggleSlideShow,|,save,saveAs,moveToTrash,delete,"
        "|,zoomIn,zoomOut,resetZoom,origZoom,rotateRight,rotateLeft,rotateMouse,flipH,flipV,|,"
        "resize,crop,blackout,cartouche,annotate,colors");
    Settings::imageToolActions = Settings::value("ImageToolActions", defaultTools.split(',')).toStringList();

    // meehh
    Settings::fileSystemDockVisible = Settings::value(Settings::optionFileSystemDockVisible, true).toBool();
    Settings::bookmarksDockVisible = Settings::value(Settings::optionBookmarksDockVisible, true).toBool();
    Settings::tagsDockVisible = Settings::value(Settings::optionTagsDockVisible, true).toBool();
    Settings::imagePreviewDockVisible = Settings::value(Settings::optionImagePreviewDockVisible, true).toBool();
    Settings::imageInfoDockVisible = Settings::value(Settings::optionImageInfoDockVisible, true).toBool();

    Settings::startupDir = (Settings::StartupDir) Settings::value(Settings::optionStartupDir, Settings::RememberLastDir).toInt();
    Settings::specifiedStartDir = Settings::value(Settings::optionSpecifiedStartDir, QString()).toString();
    Settings::thumbsBackgroundImage = Settings::value(Settings::optionThumbsBackgroundImage, QString()).toString();
    Settings::wallpaperCommand = Settings::value(Settings::optionWallpaperCommand, QString()).toString();

    /// @todo, these are not settings, the namespace is abused as transactional global object
    Settings::rotation = 0;
    Settings::keepTransform = false;
    Settings::slideShowActive = false;

    /* read external apps */
    Settings::beginGroup(Settings::optionExternalApps);
    QStringList extApps = Settings::appSettings->childKeys();
    for (int i = 0; i < extApps.size(); ++i) {
        Settings::externalApps[extApps.at(i)] = Settings::appSettings->value(extApps.at(i)).toString();
    }
    Settings::appSettings->endGroup();

    Settings::beginGroup(Settings::optionBangs);
    QStringList bangs = Settings::appSettings->childKeys();
    for (int i = 0; i < bangs.size(); ++i) {
        Settings::bangs[bangs.at(i)] = Settings::appSettings->value(bangs.at(i)).toString();
    }
    Settings::appSettings->endGroup();

    /* read bookmarks */
    Settings::beginGroup(Settings::optionCopyMoveToPaths);
    QStringList paths = Settings::appSettings->childKeys();
    if (paths.isEmpty()) {
        Settings::bookmarkPaths.insert(QDir::homePath());
        const QString picturesLocation = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (!picturesLocation.isEmpty()) {
            Settings::bookmarkPaths.insert(picturesLocation);
        }
    } else {
        for (int i = 0; i < paths.size(); ++i) {
            Settings::bookmarkPaths.insert(Settings::appSettings->value(paths.at(i)).toString());
        }
    }
    Settings::appSettings->endGroup();

    /* read known tags */
    Settings::beginGroup(Settings::optionKnownTags);
    QStringList tags = Settings::appSettings->childKeys();
    for (int i = 0; i < tags.size(); ++i) {
        Settings::knownTags.insert(Settings::appSettings->value(tags.at(i)).toString());
    }
    Settings::appSettings->endGroup();

    /* read exif filters */
    Settings::beginGroup(Settings::optionExifFilters);
    QStringList eFilters = Settings::appSettings->childKeys();
    for (int i = 0; i < eFilters.size(); ++i) {
        Settings::exifFilters[eFilters.at(i)] = Settings::appSettings->value(eFilters.at(i)).toString();
    }
    Settings::appSettings->endGroup();

    Settings::isFileListLoaded = false;
}

void Phototonic::setupDocks() {
    addDockWidget(Qt::RightDockWidgetArea, imageInfoDock);
    addDockWidget(Qt::RightDockWidgetArea, tagsDock);
    lockDocks();
}

void Phototonic::lockDocks() {
    if (initComplete) {
        Settings::hideDockTitlebars = action("lockDocks", true)->isChecked();
    }

    delete fileSystemDock->titleBarWidget();
    delete bookmarksDock->titleBarWidget();
    delete imagePreviewDock->titleBarWidget();
    delete tagsDock->titleBarWidget();
    delete imageInfoDock->titleBarWidget();
    if (Settings::hideDockTitlebars) {
        fileSystemDock->setTitleBarWidget(new QWidget(fileSystemDock));
        bookmarksDock->setTitleBarWidget(new QWidget(bookmarksDock));
        imagePreviewDock->setTitleBarWidget(new QWidget(imagePreviewDock));
        tagsDock->setTitleBarWidget(new QWidget(tagsDock));
        imageInfoDock->setTitleBarWidget(new QWidget(imageInfoDock));
    } else {
        fileSystemDock->setTitleBarWidget(nullptr);
        bookmarksDock->setTitleBarWidget(nullptr);
        imagePreviewDock->setTitleBarWidget(nullptr);
        tagsDock->setTitleBarWidget(nullptr);
        imageInfoDock->setTitleBarWidget(nullptr);
    }
}

QMenu *Phototonic::createPopupMenu() {
    QMenu *extraActsMenu = QMainWindow::createPopupMenu();
    if (myMainToolBar)
        extraActsMenu->removeAction(myMainToolBar->toggleViewAction());
    extraActsMenu->addSeparator();
    extraActsMenu->addAction(action("smallToolbarIcons"));
    extraActsMenu->addAction(action("lockDocks"));
    return extraActsMenu;
}

void Phototonic::loadShortcuts() {
    Settings::beginGroup(Settings::optionShortcuts);
    QStringList shortcuts = Settings::appSettings->childKeys();

    QList<QAction*> actionlist = findChildren<QAction*>(Qt::FindDirectChildrenOnly);
    QStringList objectNames; // sanity check only
    for (QAction *action : actionlist) {
        const QString name = action->objectName();
        if (!name.isEmpty()) {
            objectNames << name;
            Settings::actionKeys[name] = action;
//            qDebug() << name << action->property("sc_default");
            action->setShortcut(Settings::appSettings->value(name, action->property("sc_default")).toString());
        }
    }

//    objectNames.sort();
//    qDebug() << objectNames;
    if (objectNames.removeDuplicates()) // sanity check
        qWarning() << "DUPLICATE OBJECT NAME FOUND - THIS IS A BUG";

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

void Phototonic::newImage() {
    if (Settings::layoutMode == ThumbViewWidget) {
        showViewer();
    }

    imageViewer->loadImage("");
}

void Phototonic::setDocksVisibility(bool visible) {
    layout()->setEnabled(false);
    auto vis = [=](QDockWidget *d) { return visible || d->isFloating(); };
    fileSystemDock->setVisible(vis(fileSystemDock) && Settings::fileSystemDockVisible);
    bookmarksDock->setVisible(vis(bookmarksDock) && Settings::bookmarksDockVisible);
    imagePreviewDock->setVisible(vis(imagePreviewDock) && Settings::imagePreviewDockVisible);
    tagsDock->setVisible(vis(tagsDock) && Settings::tagsDockVisible);
    imageInfoDock->setVisible(vis(imageInfoDock) && Settings::imageInfoDockVisible);
    myMainToolBar->setVisible(visible);

    setContextMenuPolicy(Qt::PreventContextMenu);
    layout()->setEnabled(true);
}

void Phototonic::viewImage() {
    if (Settings::layoutMode == ImageViewWidget) {
        hideViewer();
        return;
    }

    if (QApplication::focusWidget() == fileSystemTree) {
        goTo(getSelectedPath());
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
        if (!thumbsViewer->setFilter(filterLineEdit->text(), &error))
            QToolTip::showText(filterLineEdit->mapToGlobal(QPoint(0, filterLineEdit->height())),
                                error, filterLineEdit);
        return;
    } else if (QApplication::focusWidget() == pathLineEdit) {
        goPathBarDir();
        return;
    }
}

void Phototonic::showViewer() {
    if (imagePreviewDock->isFloating() && imagePreviewDock->isVisible()) {
        if (Settings::isFullScreen) {
            imagePreviewDock->setWindowState(imagePreviewDock->windowState() | Qt::WindowFullScreen);
            imageViewer->setCursorHiding(true);
        }
        imagePreviewDock->raise();
        return;
    }
    if (Settings::layoutMode == ThumbViewWidget) {
        Settings::layoutMode = ImageViewWidget;
//        Settings::setValue("Geometry", saveGeometry());
//        Settings::setValue("WindowState", saveState());
        thumbsViewer->setResizeEnabled(false);
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
        QApplication::processEvents(); /// @todo why?
        if (m_imageToolBar->isVisible())
            positionImageToolbar();
    }
}
/// @todo looks like redundant calls?
void Phototonic::loadSelectedThumbImage(const QModelIndex &idx) {
    if (!imageViewer->batchMode)
        showViewer();
    thumbsViewer->setCurrentIndex(idx);
    const QString imagePath = thumbsViewer->fullPathOf(idx.row());
    if (m_imageInfoAction->isChecked()) {
        m_infoViewer->read(imagePath);
        imageViewer->setFeedback(m_infoViewer->html(), false);
    }
    imageViewer->loadImage(imagePath, thumbsViewer->icon(idx.row()).pixmap(THUMB_SIZE_MAX).toImage());
    setImageViewerWindowTitle();
}

void Phototonic::toggleSlideShow() {
    QAction *toggle = action("toggleSlideShow", true);
    if (Settings::slideShowActive) {
        Settings::slideShowActive = false;
        imageViewer->setCrossfade(false);
        slideShowHandler(); // reset
        toggle->setText(tr("Slide Show"));
        imageViewer->setFeedback(tr("Slide show stopped"));

        SlideShowTimer->stop();
        SlideShowTimer->deleteLater();
        toggle->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/images/play.png")));
    } else {
        if (thumbsViewer->model()->rowCount() <= 0) {
            return;
        }

        if (Settings::layoutMode == ThumbViewWidget) {
            QModelIndexList selection = thumbsViewer->selectionModel()->selectedIndexes();
            if (selection.size() < 2) {
                loadImage(Phototonic::First);
            } else {
                thumbsViewer->selectionModel()->setCurrentIndex(selection.first(), QItemSelectionModel::NoUpdate);
//                thumbsViewer->setCurrentIndex(selection.first());
            }

            showViewer();
        }

        Settings::slideShowActive = true;
        slideShowHandler(); // init/preload

        SlideShowTimer = new QTimer(this);
        const int ssTimeout = Settings::slideShowDelay * 1000.0;
        connect(SlideShowTimer, SIGNAL(timeout()), this, SLOT(slideShowHandler()));
        SlideShowTimer->start(ssTimeout);

        toggle->setText(tr("Stop Slide Show"));
        imageViewer->setFeedback(tr("Slide show started"));
        toggle->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/images/stop.png")));

        const int currentRow = thumbsViewer->currentIndex().row();
        imageViewer->loadImage(thumbsViewer->fullPathOf(currentRow),
                               thumbsViewer->icon(currentRow).pixmap(THUMB_SIZE_MAX).toImage());
        imageViewer->setCrossfade(Settings::slideShowCrossfade && ssTimeout >= 1000);
    }
}

void Phototonic::slideShowHandler() {
    static int next = -1;
    static int last = -1;
    if (!Settings::slideShowActive) {
        next = -1;
        last = -1;
        imageViewer->preload("");
        return;
    }

    if (next < -1) {
        toggleSlideShow();
        return;
    }

    QModelIndexList selection = thumbsViewer->selectionModel()->selectedIndexes();
    QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
    if (next > -1 && next < thumbModel->rowCount() && last == thumbsViewer->currentIndex().row()) {
        if (selection.size() > 1) {
            QModelIndex idx = thumbModel->indexFromItem(thumbModel->item(next));
            if (idx.isValid())
                thumbsViewer->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
        } else {
            thumbsViewer->setCurrentIndex(next);
        }
#if 0 // poor man's ken burns - disabled for now
        if (imageViewer->crossfade() && !action("keepZoom")->isChecked())
            QTimer::singleShot(300, this, [=]() {imageViewer->zoomTo(ImageViewer::ZoomToFill, QPoint(-1,-1), Settings::slideShowDelay * 1000.0 - 500);});
#endif
    }
    last = thumbsViewer->currentIndex().row();

    if (selection.size() > 1) { // use only selected images
        if (Settings::slideShowRandom) {
            next = QRandomGenerator::global()->bounded(selection.size());
            next = selection.at(next).row();
        } else {
            next = -1;
            for (int i = 0; i < selection.size(); ++i) {
                if (selection.at(i).row() == last) {
                    if (i < selection.size() - 1)
                        next = selection.at(i+1).row();
                    else if (Settings::wrapImageList)
                        next = selection.at(0).row();
                    break;
                }
            }
        }
    } else { // use all visible images
        if (Settings::slideShowRandom) {
            next = QRandomGenerator::global()->bounded(thumbsViewer->visibleThumbs());
            if (thumbsViewer->visibleThumbs() != thumbsViewer->model()->rowCount()) {
                // for the filtered case we've to figure the n-th non-hidden thumb, naively picking some
                // number and moving to the next visible will cause less than random results if a huge
                // block is hidden because we'll often end at its adjacent indexes
                int n = next;
                for (next = 0; next < thumbsViewer->model()->rowCount(); ++next) {
                    if (!thumbsViewer->isRowHidden(next) && --n < 0)
                        break;
                }
            }
        } else {
            next = thumbsViewer->nextRow();
            if (next < 0 && Settings::wrapImageList) {
                next = 0;
                while (next < thumbsViewer->model()->rowCount() && thumbsViewer->isRowHidden(next))
                    ++next;
                if (next >= thumbsViewer->model()->rowCount())
                    next = -1;
            }
        }
    }

    if (next < 0)
        next = Settings::wrapImageList ? -1 : -2;

    if (next > -1 && next < thumbsViewer->model()->rowCount())
        QTimer::singleShot(0, this, [=]() {imageViewer->preload(thumbsViewer->fullPathOf(next));});
}

void Phototonic::loadImage(SpecialImageIndex idx) {
    if (thumbsViewer->model()->rowCount() <= 0) {
        return;
    }

    int thumb;
    switch (idx) {
        case Phototonic::First:
            thumb = 0;
            while (thumb < thumbsViewer->model()->rowCount() && thumbsViewer->isRowHidden(thumb))
                ++thumb;
            if (thumb >= thumbsViewer->model()->rowCount())
                thumb = -1;
            break;
        case Phototonic::Next:
            thumb = thumbsViewer->nextRow();
            if (thumb < 0 && Settings::wrapImageList)
                thumb = 0;
            break;
        case Phototonic::Previous:
            thumb = thumbsViewer->previousRow();
            if (thumb < 0 && Settings::wrapImageList)
                thumb = thumbsViewer->model()->rowCount() - 1;
            break;
        case Phototonic::Last:
            thumb = thumbsViewer->model()->rowCount() - 1;
            while (thumb > -1 && thumbsViewer->isRowHidden(thumb))
                --thumb;
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
    m_nextImageAction->setEnabled(enabled);
    m_prevImageAction->setEnabled(enabled);
    m_moveLeftAction->setEnabled(enabled);
    m_moveRightAction->setEnabled(enabled);
    m_moveUpAction->setEnabled(enabled);
    m_moveDownAction->setEnabled(enabled);
}

void Phototonic::hideViewer() {
    if (imagePreviewDock->isFloating()) {
        imagePreviewDock->setWindowState(imagePreviewDock->windowState() & ~Qt::WindowFullScreen);
    }
    if (!Settings::imagePreviewDockVisible)
        imageViewer->secureEdits();

    setWindowState(windowState() & ~Qt::WindowFullScreen);
    imageViewer->setCursorHiding(false);

//    restoreGeometry(Settings::value(Settings::optionGeometry).toByteArray());
//    restoreState(Settings::value(Settings::optionWindowState).toByteArray());

    Settings::layoutMode = ThumbViewWidget;
    stackedLayout->setCurrentWidget(thumbsViewer);

    setDocksVisibility(true);
    thumbsViewer->setResizeEnabled(true);

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
        thumbsViewer->refreshThumbs();
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

    if (!isWritableDir(destDir)) {
        MessageBox(this).critical(tr("Error"), tr("Can not move or copy images to this directory."));
        return selectCurrentViewDir();
    }

    if (destDir == (dirOp ? QFileInfo(copyMoveDirPath).absolutePath() : Settings::currentDirectory)) {
        return MessageBox(this).critical(tr("Error"), tr("Destination directory is the same as the source directory."));
    }

    if (!Settings::isCopyOperation && (m_deleteInProgress || thumbsViewer->isBusy())) { // defer, don't alter while the thumbsviewer is loading stuff
        QTimer::singleShot(100, this, [=](){dropOp(keyMods, dirOp, copyMoveDirPath);});
        return;
    }

    if (dirOp) {
        QString baseName = copyMoveDirPath.section(QDir::separator(), -1);

        MessageBox msgBox(this, MessageBox::Yes|MessageBox::Cancel, MessageBox::Cancel);
        msgBox.button(MessageBox::Yes)->setText(tr("Move Directory"));
        if (msgBox.warning(tr("Move directory"), tr("Move directory %1 to %2?").arg(baseName).arg(destDir)) == MessageBox::Yes) {
            QFile dir(copyMoveDirPath);
            if (!dir.rename(destDir + QDir::separator() + baseName)) {
                msgBox.critical(tr("Error"), tr("Failed to move directory."));
            }
            setStatus(tr("Directory moved"));
        }
    } else {
        if (!Settings::isCopyOperation)
            m_deleteInProgress = true;
        Settings::copyCutIndexList = thumbsViewer->selectionModel()->selectedIndexes();
        int latestRow = CopyOrMove::list(thumbsViewer, destDir, false, this);

        if (!Settings::isCopyOperation) {
            if (thumbsViewer->model()->rowCount()) {
                thumbsViewer->setCurrentIndex(qMin(latestRow, thumbsViewer->model()->rowCount() - 1));
            }
            m_deleteInProgress = false;
        }
        QString state = Settings::isCopyOperation ? tr("Copied %n image(s)", "", Settings::copyCutIndexList.size())
                                                  : tr("Moved %n image(s)", "", Settings::copyCutIndexList.size());
        setStatus(state);
    }

    thumbsViewer->loadVisibleThumbs();
}

void Phototonic::selectCurrentViewDir() {
    QModelIndex idx = fileSystemModel->index(Settings::currentDirectory);
    if (idx.isValid()) {
        fileSystemTree->expand(idx);
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
        refreshThumbs(false);
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
        m_goFrwdAction->setEnabled(false);
        for (int i = pathHistoryList.size() - 1; i > currentHistoryIdx; --i) {
            pathHistoryList.removeAt(i);
        }
    }
}

void Phototonic::reloadThumbs() {
    if (m_deleteInProgress || !initComplete || thumbsViewer->isBusy()) {
        thumbsViewer->abort();
        QTimer::singleShot(32, this, SLOT(reloadThumbs())); // rate control @30Hz
        return;
    }
    m_reloadPending = false;

    if (Settings::isFileListLoaded) {
        addPathHistoryRecord("Phototonic::FileList");
    } else {
        if (Settings::currentDirectory.isEmpty()) {
            Settings::currentDirectory = getSelectedPath();
            if (Settings::currentDirectory.isEmpty()) {
                return;
            }
        }

        if (!isReadableDir(Settings::currentDirectory)) {
            setStatus(tr("No directory selected"));
            return MessageBox(this).critical(tr("Error"), tr("Failed to open directory %1").arg(Settings::currentDirectory));
        }

        m_infoViewer->clear();
        if (Settings::setWindowIcon && Settings::layoutMode == Phototonic::ThumbViewWidget) {
            setWindowIcon(QApplication::windowIcon());
        }
        pathLineEdit->setText(Settings::currentDirectory);
        addPathHistoryRecord(Settings::currentDirectory);
        if (currentHistoryIdx > 0) {
            m_goBackAction->setEnabled(true);
        }
    }

    if (Settings::layoutMode == ThumbViewWidget) {
        setThumbsViewerWindowTitle();
    }

    m_imageTags->removeTransientTags();

    if (m_findDupesAction->isChecked()) {
        const bool actionEnabled[5] = { m_goBackAction->isEnabled(), m_goFrwdAction->isEnabled(),
                                        m_goUpAction->isEnabled(), m_goHomeAction->isEnabled(), m_refreshAction->isEnabled() };
        m_goBackAction->setEnabled(false);
        m_goFrwdAction->setEnabled(false);
        m_goUpAction->setEnabled(false);
        m_goHomeAction->setEnabled(false);
        m_refreshAction->setEnabled(false);
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
        m_goBackAction->setEnabled(actionEnabled[0]);
        m_goFrwdAction->setEnabled(actionEnabled[1]);
        m_goUpAction->setEnabled(actionEnabled[2]);
        m_goHomeAction->setEnabled(actionEnabled[3]);
        m_refreshAction->setEnabled(actionEnabled[4]);
    } else {
        m_progressBarAction->setVisible(false);
        m_pathLineEditAction->setVisible(true);
        m_progressBar->reset();
        thumbsViewer->reload();
    }
    sortThumbnails();
}

void Phototonic::setImageViewerWindowTitle() {
    QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
    const int currentRow = thumbsViewer->currentIndex().row();
    if (currentRow < 0) { // model is still loading, yay threads
        setWindowTitle(QFileInfo(imageViewer->fullImagePath).fileName());
        return;
    }
    QString title = thumbModel->item(currentRow)->data(Qt::DisplayRole).toString()
                  + QString::fromLatin1(" - [ %1 / %2 ] - Phototonic").arg(currentRow + 1).arg(thumbModel->rowCount());

    setWindowTitle(title);
}

void Phototonic::setThumbsViewerWindowTitle() {

    if (m_findDupesAction->isChecked()) {
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
        return selectCurrentViewDir();
    }

    if (newDirName.isEmpty()) {
        MessageBox(this).critical(tr("Error"), tr("Invalid name entered."));
        return selectCurrentViewDir();
    }

    QFile dir(dirInfo.absoluteFilePath());
    QString newFullPathName = dirInfo.absolutePath() + QDir::separator() + newDirName;
    renameOk = dir.rename(newFullPathName);
    if (!renameOk) {
        MessageBox(this).critical(tr("Error"), tr("Failed to rename directory."));
        return selectCurrentViewDir();
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

    QModelIndexList selection = thumbsViewer->selectionModel()->selectedIndexes();
    if (selection.size() < 1) {
        setStatus(tr("Invalid selection"));
        return;
    }

    RenameDialog *renameDialog = new RenameDialog(this);
    if (selection.size() == 1) {
        QFile file(thumbsViewer->fullPathOf(selection.first().row()));
        QFileInfo fileInfo(file);
        renameDialog->setFileName(fileInfo.fileName());
    }

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }
    imageViewer->setCursorHiding(false);

    QString newNameOrPattern;
    bool ack = renameDialog->exec();
    if (ack)
        newNameOrPattern = renameDialog->fileName();
    renameDialog->deleteLater();

    if (ack && newNameOrPattern.isEmpty()) {
        ack = false;
        MessageBox(this).critical(tr("Error"), tr("No name entered."));
    }

    if (!ack) {
        if (isFullScreen())
            imageViewer->setCursorHiding(true);
        return;
    }

    QString failures;
    QStringList sources, destinations;
    QList<int> rows;

    for (QModelIndex idx : selection) {
        sources << thumbsViewer->fullPathOf(idx.row());
        rows << idx.row();
    }

    int index = 0;
    bool needDualPass = false, needConflictResolution = false;
    for (const QString &oldPath : sources) {
        QFile file(oldPath);
        QFileInfo fileInfo(file);
        QString newPath, mdate, exifdate, size;
        if (newNameOrPattern.contains("%date"))
            mdate = fileInfo.lastModified().toString(Qt::ISODate);
        if (newNameOrPattern.contains("%exifdate")) {
            qint64 exifTime = Metadata::dateTimeOriginal(fileInfo.filePath());
            if (!exifTime)
                exifTime = fileInfo.birthTime().toSecsSinceEpoch();
            exifdate = QDateTime::fromSecsSinceEpoch(exifTime).toString(Qt::ISODate);
        }
        if (newNameOrPattern.contains("%size")) {
            QSize res = QImageReader(fileInfo.filePath()).size();
            size = QString("%1x%2").arg(res.width()).arg(res.height());
        }
        while (true) {
            QString newFileName = newNameOrPattern;
            newFileName.replace("%index", QStringLiteral("%1").arg(++index, 1+log10(selection.size()), 10, QLatin1Char('0')));
            newFileName.replace("%date", mdate);
            newFileName.replace("%exifdate", exifdate);
            newFileName.replace("%size", size);
            if (sources.size() > 1)
                newFileName.append("." + oldPath.section('.', -1));
            newPath = fileInfo.absolutePath() + QDir::separator() + newFileName;
            if (!QFileInfo::exists(newPath))
                break;
            // needs some conflict reolution
            if (sources.contains(newPath)) {
                needDualPass = true;
                break;
            } else if (newNameOrPattern.contains("%index")) {
                needConflictResolution = true;
                continue; // try again w/ increased index
            } else { // cannot be resolved
                return MessageBox(this).critical(tr("File collision!"), tr("Existing files collide with the rename."));
            }
        }
        destinations << newPath;
    }

    if (destinations.removeDuplicates()) {
        // ambiguous destination pattern provided. ABORT
        return MessageBox(this).critical(tr("Error"), tr("Refusing ambigious rename pattern.\nMultiple files would get the same name."));
    }

    if (needConflictResolution && newNameOrPattern.contains("%index")) {
        if (MessageBox(this, MessageBox::Yes|MessageBox::Cancel, MessageBox::Yes).ask(
                tr("File collision!"), tr("Existing files collide with the rename.") + "\n" +
                tr("Do you want to incorporate them (ie. skip their indexes)?")) == MessageBox::Cancel)
            return;
    }

    if (needDualPass) {
//        qDebug() << "dual pass rename";
        for (int i = sources.size() - 1; i >=0 ; --i) {
            if (QFile(sources.at(i)).rename(sources.at(i) + ".ptt_helper"))
                sources[i].append(".ptt_helper");
            else {
                failures += QFileInfo(sources.at(i)).fileName() + " => " + QFileInfo(destinations.at(i)).fileName() + "\n";
                sources.remove(i);
                destinations.remove(i);
                rows.remove(i);
            }
        }
    }

    for (int i = 0; i < destinations.size(); ++i) {
        const QString oldPath = sources.at(i);
        const QString newPath = destinations.at(i);
        const QString newFileName = QFileInfo(newPath).fileName();
        QFile file(oldPath);
        QFileInfo fileInfo(file);
        if (file.rename(newPath)) {
            ThumbsViewer::moveCache(oldPath, newPath);
            Metadata::rename(oldPath, newPath);
            QStandardItemModel *thumbModel = static_cast<QStandardItemModel*>(thumbsViewer->model());
            const int row = rows.at(i);
            thumbModel->item(row)->setData(newPath, thumbsViewer->FileNameRole);
            thumbModel->item(row)->setData(newFileName, Qt::DisplayRole);

            if (row == thumbsViewer->currentIndex().row()) {
                imageViewer->setInfo(newFileName);
                imageViewer->fullImagePath = newPath;
            }

            if (Settings::filesList.contains(fileInfo.absoluteFilePath())) {
                Settings::filesList.replace(Settings::filesList.indexOf(fileInfo.absoluteFilePath()), newPath);
            }

            if (Settings::layoutMode == ImageViewWidget) {
                setImageViewerWindowTitle();
            }
        } else {
            failures += fileInfo.fileName() + " => " + newFileName + "\n";
        }
    }
    if (!failures.isEmpty()) {
        MessageBox msgBox(this);
        msgBox.setDetailedText(failures);
        msgBox.critical(tr("Error"), tr("Failed to rename image."));
    }

    if (isFullScreen())
        imageViewer->setCursorHiding(true);
}

void Phototonic::removeMetadata() {

    QStringList fileList = thumbsViewer->selectedFiles();

    if (fileList.isEmpty()) {
        setStatus(tr("Invalid selection"));
        return;
    }

    if (Settings::slideShowActive) {
        toggleSlideShow();
    }

    MessageBox msgBox(this, MessageBox::Yes|MessageBox::Cancel, MessageBox::Cancel);
    msgBox.button(MessageBox::Yes)->setText(tr("Remove Metadata"));

    if (msgBox.warning(tr("Remove Metadata"), tr("Permanently remove all Exif metadata from selected images?")) == MessageBox::Yes) {
        for (int i = 0; i < fileList.size(); ++i) {
            if (!Metadata::wipeFrom(fileList.at(i)))
                msgBox.critical(tr("Error"), tr("Failed to remove Exif metadata."));
        }

        if (m_imageTags->currentDisplayMode == SelectionTagsDisplay)
            m_imageTags->showSelectedImagesTags();
        setStatus(tr("Metadata removed from selected images"));
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

    MessageBox msgBox(this, MessageBox::Yes|MessageBox::Cancel, MessageBox::Cancel);
    msgBox.button(MessageBox::Yes)->setText(trash ? tr("OK") : tr("Delete Directory"));

    if (msgBox.warning(tr("Delete Directory"), question) == MessageBox::Yes) {
        removeDirectoryOk = trash ? QFile::moveToTrash(deletePath) : removeDirectoryOperation(deletePath);
    } else {
        return selectCurrentViewDir();
    }

    if (!removeDirectoryOk) {
        msgBox.critical(tr("Error"), trash ? tr("Failed to move directory to the trash.")
                                           : tr("Failed to delete directory."));
        return selectCurrentViewDir();
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
        return selectCurrentViewDir();
    }

    if (newDirName.isEmpty()) {
        MessageBox(this).critical(tr("Error"), tr("Invalid name entered."));
        return selectCurrentViewDir();
    }

    QDir dir(dirInfo.absoluteFilePath());
    ok = dir.mkdir(dirInfo.absoluteFilePath() + QDir::separator() + newDirName);

    if (!ok) {
        MessageBox(this).critical(tr("Error"), tr("Failed to create new directory."));
        return selectCurrentViewDir();
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

void Phototonic::positionImageToolbar() {
    const QSize s16(16,16);
    if (m_imageToolBar->orientation() == Qt::Horizontal) {
        // can we get this back to vertical?
        if (m_imageToolBar->width() < imageViewer->height()) {
            m_imageToolBar->setOrientation(Qt::Vertical); // yay
            m_imageToolBar->adjustSize();
        } else if (m_imageToolBar->iconSize() != s16 && 17*m_imageToolBar->width()/24 < imageViewer->height()) {
            m_imageToolBar->setIconSize(s16);
            m_imageToolBar->setOrientation(Qt::Vertical); // yay
            m_imageToolBar->adjustSize();
        }
    }
    int tl = m_imageToolBar->height();
    int vl = imageViewer->height();
    if (m_imageToolBar->orientation() == Qt::Horizontal) {
        tl = m_imageToolBar->width();
        vl = imageViewer->width();
    }
    if (tl > vl) {
        // shrink
        if (m_imageToolBar->iconSize() != s16) {
            m_imageToolBar->setIconSize(s16);
            m_imageToolBar->adjustSize();
        }
        if (m_imageToolBar->orientation() == Qt::Vertical &&
                    m_imageToolBar->height() > imageViewer->height() &&
                    imageViewer->width() > 4*imageViewer->height()/3) {
            m_imageToolBar->setOrientation(Qt::Horizontal); // sacrifice position
            m_imageToolBar->adjustSize();
        }
    } else if (m_imageToolBar->iconSize() == s16) {
        // can we upscale this?
        if (tl < 17*vl/24) {
            m_imageToolBar->setIconSize(QSize(24,24));
            m_imageToolBar->adjustSize();
        }
    }

    if (m_imageToolBar->orientation() == Qt::Horizontal)
        m_imageToolBar->move((imageViewer->width() - m_imageToolBar->width())/2,
                              imageViewer->height() - 2*m_imageToolBar->height());
    else
        m_imageToolBar->move(imageViewer->width() - 2*m_imageToolBar->width(),
                            (imageViewer->height() - m_imageToolBar->height())/2);
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

    if (o == filterLineEdit) {
        if (e->type() == QEvent::Enter) {
            filterLineEdit->setClearButtonEnabled(!filterLineEdit->text().isEmpty());
        } else if (e->type() == QEvent::Leave) {
            filterLineEdit->setClearButtonEnabled(false);
        } else if (e->type() == QEvent::KeyPress) {
            // since we filter it anyway, allow some thumb navigation w/ keys irrelevant to the lineedit
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_PageUp || ke->key() == Qt::Key_PageDown ||
                ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
                QApplication::sendEvent(thumbsViewer, e);
                return true;
            }
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
            int grid = thumbsViewer->gridSize().height();
            if (grid < 1)
                grid = thumbsViewer->thumbSize;
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
        if (ke->key() == Qt::Key_PageUp && !ke->modifiers()) {
            scrollThumbs(-100);
            return true;
        } else if (ke->key() == Qt::Key_PageDown && !ke->modifiers()) {
            scrollThumbs(100);
            return true;
        } else if (ke->key() == Qt::Key_Home && !ke->modifiers()) {
            scrollThumbs(-1000);
            return true;
        } else if (ke->key() == Qt::Key_End && !ke->modifiers()) {
            scrollThumbs(1000);
            return true;
        } else if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Left) {
            if (!ke->modifiers() && !thumbsViewer->rect().intersects(thumbsViewer->visualRect(thumbsViewer->currentIndex()))) {
                thumbsViewer->setCurrentIndex(thumbsViewer->lastVisibleThumb());
                return true;
            }
        } else if (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Right) {
            if (!ke->modifiers() && !thumbsViewer->rect().intersects(thumbsViewer->visualRect(thumbsViewer->currentIndex()))) {
                thumbsViewer->setCurrentIndex(thumbsViewer->firstVisibleThumb());
                return true;
            }
        } else if (m_copyAction->shortcut() == ke->keyCombination() || // these are sucked away by an enabled action
                   m_cutAction->shortcut() == ke->keyCombination()) { // issue a warning for the disabled one
            setStatus(tr("No images selected"));
        }
        return QMainWindow::eventFilter(o, e);
    }

    if (e->type() == QEvent::Resize && imageViewer && o == imageViewer->viewport()) {
        if (m_imageToolBar->isVisible())
            positionImageToolbar();
        return QMainWindow::eventFilter(o, e);
    }

    if ((e->type() == QEvent::MouseButtonDblClick ||
         e->type() == QEvent::MouseButtonPress)         && o == imageViewer->viewport()) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        bool dblclk = (me->button() == Qt::LeftButton && e->type() == QEvent::MouseButtonDblClick);
        if (Settings::reverseMouseBehavior)
            dblclk = (me->button() == Qt::MiddleButton && e->type() == QEvent::MouseButtonPress);

        if (dblclk) {
            if (me->modifiers() == Qt::ControlModifier) {
                imageViewer->zoomTo(imageViewer->zoom() == 1.0 ?
                                                    ImageViewer::ZoomToFit :
                                                    ImageViewer::ZoomOriginal, me->position().toPoint());
            } else if (Settings::layoutMode == ImageViewWidget) {
                if (me->modifiers() == Qt::ShiftModifier) {
                    m_fullScreenAction->trigger();
                } else
                    hideViewer();
            } else {
                viewImage();
                if (me->modifiers() == Qt::ShiftModifier && !m_fullScreenAction->isChecked()) {
                    m_fullScreenAction->setChecked(true);
                    toggleFullScreen();
                }
            }
            return QMainWindow::eventFilter(o, e);
        }
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
        } else if (m_nextImageAction->isEnabled()) {
            if (scrollDelta < 0) {
                loadImage(Phototonic::Next);
            } else {
                loadImage(Phototonic::Previous);
            }
            return true;
        }
    } else if (o == thumbsViewer->viewport()) {
        if (we->modifiers() == Qt::ControlModifier) {
            m_thumbSizeDelta += qRound(scrollDelta / 120.0);
            static QTimer *thumbResizer = nullptr;
            if (!thumbResizer) {
                thumbResizer = new QTimer(this);
                thumbResizer->setInterval(125);
                thumbResizer->setSingleShot(true);
                connect(thumbResizer, &QTimer::timeout, [=](){resizeThumbs();});
            }
            thumbResizer->start();
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
    MessageBox(this).warning(tr("Warning"), tr("Cannot perform action with temporary image."));
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

void Phototonic::addBookmark(QString path) {
    Settings::bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
}
