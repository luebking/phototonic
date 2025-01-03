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

#ifndef PHOTOTONIC_H
#define PHOTOTONIC_H

class BookMarks;
class ColorsDialog;
class CopyMoveToDialog;
class CropDialog;
class FileListWidget;
class FileSystemTree;
class ImageViewer;
class MetadataCache;
class ResizeDialog;
class SettingsDialog;
class ThumbsViewer;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QStackedLayout;

#include <QMainWindow>
#include <QPointer>

#include <memory>

#define VERSION "Phototonic v2.1"

class Phototonic : public QMainWindow {
Q_OBJECT

public:

    int copyCutThumbsCount;

    Phototonic(QStringList argumentsList, int filesStartAt, QWidget *parent = 0);

    QMenu *createPopupMenu();

    void setStatus(QString state);

    enum CentralWidgets {
        ThumbViewWidget = 0,
        ImageViewWidget
    };

protected:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *o, QEvent *e) override;
    void keyPressEvent(QKeyEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);

public slots:

    bool event(QEvent *event);

    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString copyMoveDirPath);

    void showViewer();

    void loadSelectedThumbImage(const QModelIndex &idx);

    void loadImageFromCliArguments(QString cliFileName);

    void hideViewer();

    void setSaveDirectory(QString path = QString());

private slots:

    void sortThumbnails();

    void reload();

    void setIncludeSubDirs();

    void showSettings();

    void toggleFullScreen();

    void updateActions();

    void reloadThumbs();

    void renameDir();

    void setThumbsViewerWindowTitle();

    void rename();

    void removeMetadata();

    void viewImage();

    void newImage();

    void deleteDirectory(bool trash);

    void createSubDirectory();

    void checkDirState(const QModelIndex &, int, int);

    void goSelectedDir(const QModelIndex &currDir);

    void goPathBarDir();

    void goBack();

    void goTo(QString path);

    void goForward();

    void toggleSlideShow();

    void slideShowHandler();

    void loadNextImage();

    void loadPreviousImage();

    void loadFirstImage();

    void loadLastImage();

    void loadRandomImage();

    void updateIndexByViewerImage();

    void selectAllThumbs();

    void selectByBrightness();

    void deleteOperation();

    void deletePermanentlyOperation();

    void cutThumbs();

    void copyThumbs();

    void pasteThumbs();

    void thumbsZoomIn();

    void thumbsZoomOut();

    void resetZoom();

    void origZoom();

    void keepZoom();

    void keepTransformClicked();

    void rotateLeft();

    void rotateRight();

    void flipVertical();

    void cropImage();

    void scaleImage();

    void freeRotateLeft();

    void freeRotateRight();

    void batchTransform();

    void showColorsDialog();

    void flipHorizontal();

    void setDocksVisibility(bool visible);

    void goTop();

    void goBottom();

    void showHiddenFiles();

    void toggleImageViewerToolbar();

    void setToolbarIconSize();

    void chooseExternalApp();

    void updateExternalApps();

    void runExternalApp();

    void setImageToolBarVisibility();

    void setFileSystemDockVisibility();

    void setBookmarksDockVisibility();

    void setImagePreviewDockVisibility();

    void setTagsDockVisibility();

    void setImageInfoDockVisibility();

    void lockDocks();

    void filterImagesFocus();

    void setPathFocus();

    void copyImagesTo();

    void moveImagesTo();


private:
    QMenu *myMainMenu;
    QMenu *openWithSubMenu;

    QToolBar *myMainToolBar;
    QToolBar *imageToolBar;

    QAction *exitAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *copyToAction;
    QAction *moveToAction;
    QAction *deleteAction;
    QAction *deletePermanentlyAction;
    QAction *saveAction;
    QAction *saveAsAction;
    QAction *renameAction;
    QAction *removeMetadataAction;
    QAction *selectAllAction;
    QAction *selectByBrightnesAction;
    QAction *copyImageAction;
    QAction *pasteImageAction;
    QAction *showClipboardAction;
    QAction *addBookmarkAction;
    QAction *removeBookmarkAction;

    QAction *setClassicThumbsAction;
    QAction *setSquareThumbsAction;
    QAction *setCompactThumbsAction;

    QAction *sortByNameAction;
    QAction *sortByTimeAction;
    QAction *sortBySizeAction;
    QAction *sortByTypeAction;
    QAction *sortBySimilarityAction;
    QAction *sortReverseAction;
    QAction *refreshAction;
    QAction *includeSubDirectoriesAction;
    QAction *fullScreenAction;
    QAction *thumbsGoToTopAction;
    QAction *thumbsGoToBottomAction;
    QAction *CloseImageAction;
    QAction *settingsAction;
    QAction *thumbsZoomInAction;
    QAction *thumbsZoomOutAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *resetZoomAction;
    QAction *origZoomAction;
    QAction *keepZoomAction;
    QAction *keepTransformAction;
    QAction *transformSubMenuAction;
    QAction *batchSubMenuAction;
    QAction *rotateLeftAction;
    QAction *rotateRightAction;
    QAction *rotateToolAction;
    QAction *flipHorizontalAction;
    QAction *flipVerticalAction;
    QAction *cropAction;
    QAction *applyCropAndRotationAction;
    QAction *resizeAction;
    QAction *freeRotateLeftAction;
    QAction *freeRotateRightAction;
    QAction *colorsAction;
    QAction *mirrorDisabledAction;
    QAction *mirrorDualAction;
    QAction *mirrorTripleAction;
    QAction *mirrorDualVerticalAction;
    QAction *mirrorQuadAction;
    QAction *moveLeftAction;
    QAction *moveRightAction;
    QAction *moveUpAction;
    QAction *moveDownAction;

    QAction *aboutAction;
    QAction *showHiddenFilesAction;
    QAction *smallToolbarIconsAction;
    QAction *lockDocksAction;
    QAction *showViewerToolbarAction;

    QAction *pasteAction;
    QAction *createDirectoryAction;
    QAction *setSaveDirectoryAction;

    QAction *goBackAction;
    QAction *goFrwdAction;
    QAction *goUpAction;
    QAction *goHomeAction;

    QAction *slideShowAction;
    QAction *nextImageAction;
    QAction *prevImageAction;
    QAction *firstImageAction;
    QAction *lastImageAction;
    QAction *randomImageAction;
    QAction *viewImageAction;
    QAction *filterImagesFocusAction;
    QAction *setPathFocusAction;
    QAction *findDupesAction;

    QAction *openWithMenuAction;
    QAction *externalAppsAction;
    QAction *invertSelectionAction;
    QAction *batchTransformAction;

    QLineEdit *pathLineEdit;
    QLineEdit *filterLineEdit;
    QLabel *statusLabel;
    QDockWidget *fileSystemDock;
    QDockWidget *bookmarksDock;
    QDockWidget *imagePreviewDock;
    QDockWidget *tagsDock;
    FileSystemTree *fileSystemTree;
    BookMarks *bookmarks;
    QDockWidget *imageInfoDock;
    ThumbsViewer *thumbsViewer;
    ImageViewer *imageViewer;
    QList<QString> pathHistoryList;
    QTimer *SlideShowTimer;
    QPointer<CopyMoveToDialog> copyMoveToDialog;
    QWidget *fileSystemDockOrigWidget;
    QWidget *bookmarksDockOrigWidget;
    QWidget *imagePreviewDockOrigWidget;
    QWidget *tagsDockOrigWidget;
    QWidget *imageInfoDockOrigWidget;
    QWidget *fileSystemDockEmptyWidget;
    QWidget *bookmarksDockEmptyWidget;
    QWidget *imagePreviewDockEmptyWidget;
    QWidget *tagsDockEmptyWidget;
    QWidget *imageInfoDockEmptyWidget;
    bool interfaceDisabled;
    std::shared_ptr<MetadataCache> metadataCache;
    FileListWidget *fileListWidget;
    QFileSystemModel *fileSystemModel;
    QStackedLayout *stackedLayout;

    int currentHistoryIdx;
    bool needHistoryRecord;
    bool initComplete;
    bool needThumbsRefresh;
    bool shouldMaximize;

    bool imageToolBarVisible;
    bool m_deleteInProgress;

    QPointer<ResizeDialog> resizeDialog;
    QPointer<ColorsDialog> colorsDialog;
    QPointer<CropDialog> cropDialog;

    void refreshThumbs(bool noScroll);

    void loadShortcuts();

    void setupDocks();

    void deleteImages(bool trash);

    void deleteFromViewer(bool trash);

    void loadCurrentImage(int currentRow);

    void selectCurrentViewDir();

    void processStartupArguments(QStringList argumentsList, int filesStartAt);

    void loadStartupFileList(QStringList argumentsList, int filesStartAt);

    void createImageViewer();

    void createThumbsViewer();

    void createActions();

    void createMenus();

    void createToolBars();

    void createStatusBar();

    void createFileSystemDock();

    void createBookmarksDock();

    void createImagePreviewDock();

    void createImageTagsDock();

    void writeSettings();

    void readSettings();

    void addPathHistoryRecord(QString dir);

    bool isValidPath(QString &path);

    QString getSelectedPath();

    void setCopyCutActions(bool setEnabled);

    void copyOrCutThumbs(bool copy);

    void showNewImageWarning();

    bool removeDirectoryOperation(QString dirToDelete);

    void setInterfaceEnabled(bool enable);

    void addBookmark(QString path);

    void copyOrMoveImages(bool move);

    void setViewerKeyEventsEnabled(bool enabled);
    void zoom(double multiplier = 1., QPoint focus = QPoint(-1, -1));
};

#endif // PHOTOTONIC_H

