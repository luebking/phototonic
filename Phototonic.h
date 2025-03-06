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
class FileSystemTree;
class ImageTags;
class ImageViewer;
class InfoView;
class SettingsDialog;
class ThumbsViewer;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QProgressBar;
class QStackedLayout;
class QToolButton;

#include <QMainWindow>
#include <QPointer>

#include <memory>

#define VERSION "Phototonic v2.98"

class Phototonic : public QMainWindow {
Q_OBJECT

public:
    Phototonic(QStringList argumentsList, int filesStartAt, QWidget *parent = 0);
    QMenu *createPopupMenu() override;

    enum CentralWidgets {
        ThumbViewWidget = 0,
        ImageViewWidget
    };

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *o, QEvent *e) override;

public slots:
    void setSaveDirectory(QString path = QString());

private slots:
    void loadSelectedThumbImage(const QModelIndex &idx);
    void showViewer();
    void hideViewer();
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString copyMoveDirPath);
    void sortThumbnails();
    void setFileListMode(bool on);

    void reload();

    void setIncludeSubDirs();

    void showSettings();

    void toggleFullScreen();

    void updateActions();

    void reloadThumbs();

    void renameDir();

    void setImageViewerWindowTitle();
    void setThumbsViewerWindowTitle();

    void rename();

    void removeMetadata();

    void viewImage();

    void newImage();

    void deleteDirectory(bool trash);

    void createSubDirectory();

    void checkDirState(const QModelIndex &, int, int);

    void goPathBarDir();

    void goTo(QString path);

    void toggleSlideShow();

    void slideShowHandler();

    void selectAllThumbs();

    void deleteOperation();

    void deletePermanentlyOperation();

    void pasteThumbs();

    void resizeThumbs();

    void rotate(int deg);

    void flipVertical();

    void cropImage();

    void scaleImage();

    void freeRotate(int deg);

    void batchTransform();

    void showColorsDialog();

    void flipHorizontal();

    void setDocksVisibility(bool visible);

    void showHiddenFiles();

    void setToolbarIconSize();

    void chooseExternalApp();

    void updateExternalApps();

    void runExternalApp();

    void lockDocks();

    void filterImagesFocus();

    void setPathFocus();



private:
    enum SpecialImageIndex {
        First = 0, Previous, Next, Last, Random
    };
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
    QAction *copyImageAction;
    QAction *pasteImageAction;
    QAction *showClipboardAction;
    QAction *addBookmarkAction;
    QAction *removeBookmarkAction;

    QAction *setClassicThumbsAction;
    QAction *setSquareThumbsAction;
    QAction *setCompactThumbsAction;

    QAction *sortMenuAction;
    QAction *sortByNameAction;
    QAction *sortByTimeAction;
    QAction *sortByExifTimeAction;
    QAction *sortBySizeAction;
    QAction *sortByTypeAction;
    QAction *sortBySimilarityAction;
    QAction *sortByBrightnessAction;
    QAction *sortByColorAction;
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
    QAction *m_wallpaperAction;
    QAction *invertSelectionAction;
    QAction *batchTransformAction;
    QAction *feedbackImageInfoAction;

    QProgressBar *m_progressBar;
    QAction *m_progressBarAction;
    QLineEdit *pathLineEdit;
    QAction *m_pathLineEditAction;
    QLineEdit *filterLineEdit;
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
    QFileSystemModel *fileSystemModel;
    QStackedLayout *stackedLayout;

    int currentHistoryIdx;
    bool needHistoryRecord;
    bool initComplete;
    bool needThumbsRefresh;

    bool m_deleteInProgress;
    InfoView *m_infoViewer;
    QToolButton *m_menuButton;
    QLabel *m_statusLabel;
    int m_thumbSizeDelta;
    bool m_logHistogram;
    bool m_reloadPending;
    int m_editSteps;
    ImageTags *m_imageTags;

    QPointer<ColorsDialog> colorsDialog;

    void refreshThumbs(bool noScroll);
    void loadImage(SpecialImageIndex idx);
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

    void createFileSystemDock();

    void createBookmarksDock();

    void createImagePreviewDock();

    void createImageTagsDock();

    void writeSettings();

    void readSettings();

    void addPathHistoryRecord(QString dir);

    QString getSelectedPath();

    void copyOrCutThumbs(bool copy);

    void showNewImageWarning();

    bool removeDirectoryOperation(QString dirToDelete);

    void addBookmark(QString path);

    void copyOrMoveImages(bool isCopyOperation);

    void setViewerKeyEventsEnabled(bool enabled);
    void zoom(float multiplier = 1., QPoint focus = QPoint(-1, -1));
    int copyCutThumbsCount;
    void setStatus(QString state);
};

#endif // PHOTOTONIC_H

