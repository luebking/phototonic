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

#define VERSION "Phototonic v3.1.0"

class Phototonic : public QMainWindow {
Q_OBJECT

public:
    Phototonic(QStringList argumentsList, int filesStartAt, QWidget *parent = 0);
    QMenu *createPopupMenu() override;
    void setCurrentFileOrDirectory(const QString &path);

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

    void pasteThumbs(QString destDir = QString());

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
    QToolBar *m_imageToolBar;

    // used frequently in ::updateActions, m_imageInfoAction on every tumbnail change
    QAction *m_cutAction, *m_copyAction, *m_copyToAction, *m_moveToAction, *m_deleteAction,
            *m_trashAction, *m_viewImageAction, *m_openWithMenuAction, *m_fullScreenAction,
            *m_closeImageAction, *m_imageInfoAction, *m_batchTransformAction, *m_wallpaperAction,
            *m_showGridAction;
    // dto. via ::setViewerKeyEventsEnabled
    QAction *m_nextImageAction,  *m_prevImageAction, *m_moveLeftAction,
            *m_moveRightAction, *m_moveUpAction, *m_moveDownAction;

    // setFileListMode, called on path changes - maybe drop
    QAction *m_includeSubDirsAction, *m_findDupesAction;
    // dto.
    QAction *m_refreshAction, *m_goBackAction, *m_goFrwdAction, *m_goUpAction, *m_goHomeAction;

    // updated on edits, rotations, etc.
    QAction *m_saveAction, *m_saveAsAction;

    // not a child of ours
    QAction *m_sortMenuAction;

    QProgressBar *m_progressBar;
    QAction *m_progressBarAction;
    QLineEdit *pathLineEdit;
    QAction *m_pathLineEditAction;

    QLineEdit *filterLineEdit;
    QDockWidget *fileSystemDock;
    QDockWidget *bookmarksDock;
    QDockWidget *imagePreviewDock;
    QDockWidget *m_thumbViewDock;
    QDockWidget *tagsDock;
    FileSystemTree *fileSystemTree;
    BookMarks *bookmarks;
    QDockWidget *imageInfoDock;
    ThumbsViewer *thumbsViewer;
    ImageViewer *imageViewer;
    QList<QString> pathHistoryList;
    QTimer *SlideShowTimer;
    QFileSystemModel *fileSystemModel;
    QStackedLayout *m_centralLayout;
    bool m_presentationMode;

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

    void createThumbviewDock();

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
    void positionImageToolbar();
    QAction *action(const QString name, bool dropHash = false) const;
    bool focusIsOnBrowsing() const;
};

#endif // PHOTOTONIC_H

