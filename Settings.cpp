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

#include <QSettings>
#include "Settings.h"

namespace Settings {

    const char optionThumbsSortFlags[] = "optionThumbsSortFlags";
    const char optionThumbsZoomLevel[] = "optionThumbsZoomLevel";
    const char optionFullScreenMode[] = "optionFullScreenMode";
    const char optionViewerBackgroundColor[] = "optionViewerBackgroundColor";
    const char optionThumbsBackgroundColor[] = "optionThumbsBackgroundColor";
    const char optionThumbsTextColor[] = "optionThumbsTextColor";
    const char optionThumbsPagesReadCount[] = "optionThumbsPagesReadCount";
    const char optionThumbsLayout[] = "optionThumbsLayout";
    const char optionShowImageName[] = "optionShowImageName";
    const char optionEnableAnimations[] = "enableAnimations";
    const char optionWrapImageList[] = "wrapImageList";
    const char optionExifRotationEnabled[] = "exifRotationEnabled";
    const char optionExifThumbRotationEnabled[] = "exifThumbRotationEnabled";
    const char optionReverseMouseBehavior[] = "reverseMouseBehavior";
    const char optionDeleteConfirm[] = "deleteConfirm";
    const char optionShowHiddenFiles[] = "showHiddenFiles";
    const char optionDefaultSaveQuality[] = "defaultSaveQuality";
    const char optionSlideShowDelay[] = "slideShowDelay";
    const char optionSlideShowRandom[] = "slideShowRandom";
    const char optionSlideShowCrossfade[] = "slideShowCrossfade";
    const char optionFileSystemDockVisible[] = "fileSystemDockVisible";
    const char optionBookmarksDockVisible[] = "bookmarksDockVisible";
    const char optionImagePreviewDockVisible[] = "imagePreviewDockVisible";
    const char optionTagsDockVisible[] = "tagsDockVisible";
    const char optionImageInfoDockVisible[] = "imageInfoDockVisible";
    const char optionSmallToolbarIcons[] = "smallToolbarIcons";
    const char optionHideDockTitlebars[] = "hideDockTitlebars";
    const char optionStartupDir[] = "startupDir";
    const char optionSpecifiedStartDir[] = "specifiedStartDir";
    const char optionThumbsBackgroundImage[] = "thumbsBackgroundImage";
    const char optionShowViewerToolbar[] = "showViewerToolbar";
    const char optionLastDir[] = "lastDir";
    const char optionGeometry[] = "Geometry";
    const char optionWindowState[] = "WindowState";
    const char optionShortcuts[] = "Shortcuts";
    const char optionExternalApps[] = "ExternalApps";
    const char optionBangs[] = "Bangs";
    const char optionExifFilters[] = "ExifFilters";
    const char optionWallpaperCommand[] = "WallpaperCommand";
    const char optionCopyMoveToPaths[] = "CopyMoveToPaths";
    const char optionKnownTags[] = "KnownTags";
    const char optionSetWindowIcon[] = "setWindowIcon";
    const char optionUpscalePreview[] = "upscalePreview";
    const char optionScrollZooms[] = "scrollZooms";

    QSettings *appSettings;
    QVariant value(const char *c, const QVariant &defaultValue) { return appSettings->value(QByteArray(c), defaultValue); }
    void setValue(const char *c, const QVariant &value) { appSettings->setValue(QByteArray(c), value); }
    void beginGroup(const char *c) { appSettings->beginGroup(QByteArray(c)); }
    unsigned int layoutMode;
    QColor viewerBackgroundColor;
    QColor thumbsBackgroundColor;
    QColor thumbsTextColor;
    unsigned int thumbsLayout;
    unsigned int thumbsPagesReadCount;
    bool wrapImageList;
    bool enableAnimations;
    qreal rotation;
    bool mouseRotateEnabled = false;
    bool keepTransform;
    int defaultSaveQuality;
    double slideShowDelay;
    bool slideShowRandom;
    bool slideShowActive;
    bool slideShowCrossfade;
    QMap<QString, QAction *> actionKeys;
    int hueVal;
    int saturationVal;
    int lightnessVal;
    int contrastVal;
    int brightVal;
    int redVal;
    int greenVal;
    int blueVal;
    bool colorsActive;
    bool colorizeEnabled;
    bool rNegateEnabled;
    bool gNegateEnabled;
    bool bNegateEnabled;
    bool hueRedChannel;
    bool hueGreenChannel;
    bool hueBlueChannel;
    bool exifRotationEnabled;
    bool exifThumbRotationEnabled;
    bool includeSubDirectories;
    bool showHiddenFiles;
    bool showViewerToolbar;
    QMap<QString, QString> externalApps;
    QMap<QString, QString> bangs;
    QMap<QString, QString> exifFilters;
    QString wallpaperCommand;
    QSet<QString> bookmarkPaths;
    QSet<QString> knownTags;
    bool reverseMouseBehavior;
    bool deleteConfirm;
    QModelIndexList copyCutIndexList;
    bool isCopyOperation;
    QStringList copyCutFileList;
    bool isFullScreen;
    int dialogLastX;
    int dialogLastY;
    StartupDir startupDir;
    QString specifiedStartDir;
    bool showImageName;
    bool smallToolbarIcons;
    bool hideDockTitlebars;
    bool tagsDockVisible;
    bool fileSystemDockVisible;
    bool bookmarksDockVisible;
    bool imagePreviewDockVisible;
    bool imageInfoDockVisible;
    QString currentDirectory;
    QString saveDirectory;
    QString thumbsBackgroundImage;
    QStringList filesList;
    bool isFileListLoaded;
    bool setWindowIcon;
    bool upscalePreview;
    bool scrollZooms;
    int dupeAccuracy;
    QStringList imageToolActions;
}

