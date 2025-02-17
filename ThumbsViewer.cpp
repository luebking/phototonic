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

#include <QApplication>
#include <QCollator>
#include <QColorSpace>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QDrag>
#include <QFileSystemWatcher>
#include <QImageReader>
#include <QLabel>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QProgressDialog>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>
#include <cmath>

#include "MessageBox.h"
#include "MetadataCache.h"
#include "Settings.h"
#include "SmartCrop.h"
#include "Tags.h"
#include "ThumbsViewer.h"

static int gs_fontHeight = 0;

ThumbsViewer::ThumbsViewer(QWidget *parent) : QListView(parent) {
    m_busy = false;
    m_resize = true;
    m_invertTagFilter = false;
    m_filterDirty = false;
    gs_fontHeight = QFontMetrics(font()).height();

    Settings::thumbsBackgroundColor = Settings::value(Settings::optionThumbsBackgroundColor, QColor(39,39,39)).value<QColor>();
    Settings::thumbsTextColor = Settings::value(Settings::optionThumbsTextColor, QColor(250,250,250)).value<QColor>();
    setThumbColors();
    Settings::thumbsPagesReadCount = Settings::value(Settings::optionThumbsPagesReadCount, 2).toUInt();
    thumbSize = Settings::value(Settings::optionThumbsZoomLevel, 200).toInt();

    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setResizeMode(QListView::Adjust);
    setWordWrap(true);
    setWrapping(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);

    // This is the default but set for clarity. Could make it configurable to use
    // QAbstractItemView::ScrollPerPixel instead.
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_model = new QStandardItemModel(this);
//    m_model->setSortRole(Qt::DisplayRole);
    setModel(m_model);

    m_selectionChangedTimer.setInterval(100);
    m_selectionChangedTimer.setSingleShot(true);
    connect(&m_selectionChangedTimer, &QTimer::timeout, this, &ThumbsViewer::promoteSelectionChange);
    connect(this->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]() {
        if (!m_selectionChangedTimer.isActive()) {
            m_selectionChangedTimer.start();
        }
    });

    m_loadThumbTimer.setInterval(250);
    m_loadThumbTimer.setSingleShot(true);
    connect(&m_loadThumbTimer, &QTimer::timeout, [=](){ loadVisibleThumbs(verticalScrollBar()->value()); });

    QTimer *fsUpdateDelay = new QTimer(this);
    fsUpdateDelay->setSingleShot(true);
    fsUpdateDelay->setInterval(500);
    connect(fsUpdateDelay, &QTimer::timeout, this, [=]() {reload(true);});
    m_fsWatcher = new QFileSystemWatcher(this);
    connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, fsUpdateDelay, qOverload<>(&QTimer::start));

    emptyImg.load(":/images/no_image.png");

    refreshThumbs(); // apply settings to icon layout
}

void ThumbsViewer::setThumbColors() {
    QColor background = Settings::thumbsLayout == Squares ? Qt::transparent : Settings::thumbsBackgroundColor;
    QPalette pal = palette();
    pal.setColor(QPalette::Base, background);
    pal.setColor(QPalette::Text, Settings::thumbsTextColor);
    if (!Settings::thumbsBackgroundImage.isEmpty()) {
        QImage bgImg(Settings::thumbsBackgroundImage);
        pal.setBrush(QPalette::Base, bgImg);
    }
    setPalette(pal);
}

QString ThumbsViewer::getSingleSelectionFilename() {
    if (selectionModel()->selectedIndexes().size() == 1)
        return m_model->item(selectionModel()->selectedIndexes().first().row())->data(FileNameRole).toString();

    return QString();
}

QString ThumbsViewer::fullPathOf(int idx)
{
    QStandardItem *item = m_model->item(idx);
    if (!item) {
        qDebug() << "meek!" << idx << m_model->rowCount();
        return QString();
    }
    return item->data(FileNameRole).toString();
}

QIcon ThumbsViewer::icon(int idx)
{
    QStandardItem *item = m_model->item(idx);
    if (!item) {
        qDebug() << "meek!" << idx << m_model->rowCount();
        return QIcon();
    }
    QIcon icon = item->icon();
    if (icon.isNull() && loadThumb(idx, true))
        icon = m_model->item(idx)->icon();
    return icon;
}

int ThumbsViewer::nextRow() {
    int next = currentIndex().row() + 1;
    while (next < m_model->rowCount() && isRowHidden(next))
        ++next;
    return next < m_model->rowCount() ? next : -1;
}

int ThumbsViewer::previousRow() {
    int prev = currentIndex().row() - 1;
    while (prev > -1 && isRowHidden(prev))
        --prev;
    return prev > -1 ? prev : -1;
}

bool ThumbsViewer::setCurrentIndex(const QString &fileName) {
    if (!m_model->rowCount()) {
        m_desiredThumbPath = fileName;
        return true;
    }
    QModelIndexList indexList = m_model->match(m_model->index(0, 0), FileNameRole, fileName);
    if (indexList.size()) {
        setCurrentIndex(indexList.at(0));
        return true;
    }
    return false;
}

void ThumbsViewer::updateThumbnail(const QString &fileName) {
    if (!m_model->rowCount())
        return;
    QModelIndexList indexList = m_model->match(m_model->index(0, 0), FileNameRole, fileName);
    if (!indexList.size())
        return;
    QModelIndex idx = indexList.at(0);
    m_model->setData(idx, false, LoadedRole);
    if (viewport()->rect().intersects(visualRect(idx)))
        loadThumb(idx.row());
}

bool ThumbsViewer::setCurrentIndex(int row) {
    QModelIndex idx = m_model->indexFromItem(m_model->item(row));
    if (idx.isValid()) {
        setCurrentIndex(idx);
        return true;
    }
    return false;
}

void ThumbsViewer::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    QListView::currentChanged(current, previous);
    emit currentIndexChanged(current);
}

void ThumbsViewer::promoteSelectionChange() {
    const QModelIndexList indexesList = selectionModel()->selectedIndexes();
    int selectedThumbs = indexesList.size();

    if (Settings::setWindowIcon) {
        if (selectedThumbs > 0)
            window()->setWindowIcon(m_model->item(indexesList.first().row())->icon().pixmap(WINDOW_ICON_SIZE));
        else
            window()->setWindowIcon(QApplication::windowIcon());
    }

    emit selectionChanged(indexesList.size());

    if (!selectedThumbs)
        promoteThumbsCount();
    else
        emit status(tr("Selected %1 of %n image(s)", "", m_model->rowCount()).arg(QString::number(selectedThumbs)));
}

QStringList ThumbsViewer::selectedFiles() const {
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    QStringList paths;
    for (int i = 0; i < indexesList.size(); ++i) {
        paths << m_model->item(indexesList.at(i).row())->data(FileNameRole).toString();
    }
    return paths;
}

void ThumbsViewer::tagSelected(const QStringList &tagsAdded, const QStringList &tagsRemoved) const {
    QProgressDialog progress(window());
    QStringList files = selectedFiles();
    progress.setMaximum(files.size());
    QElapsedTimer timer;
    int totalTime = 0;
    timer.start();
    int cycle = 1;
    for (int i = 0; i < files.size(); ++i) {

        QString imageName = files.at(i);
        progress.setLabelText(tr("Tagging %1").arg(imageName));

        for (const QString &tag : tagsAdded)
            Metadata::addTag(imageName, tag);
        for (const QString &tag : tagsRemoved)
            Metadata::removeTag(imageName, tag);

        if (!Metadata::write(imageName)) {
            MessageBox(window()).critical(tr("Error"), tr("Failed to save tags to %1").arg(imageName));
            Metadata::forget(imageName);
        }
        if (timer.elapsed() > 30) {
            if ((totalTime += timer.elapsed()) > 250) {
                totalTime = 0;
                if (float(i)/files.size() < 1.0f-1.0f/++cycle)
                    progress.show();
            }
            QApplication::processEvents();
        }

        if (progress.wasCanceled()) {
            break;
        }
    }
}

void ThumbsViewer::startDrag(Qt::DropActions) {
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    if (indexesList.isEmpty()) {
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (QModelIndexList::const_iterator it = indexesList.constBegin(),
                 end = indexesList.constEnd(); it != end; ++it) {
        urls << QUrl::fromLocalFile(m_model->item(it->row())->data(FileNameRole).toString());
    }
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);
    QPixmap pix;
    if (indexesList.count() > 1) {
        pix = QPixmap(128, 112);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 2));
        int x = 0, y = 0, xMax = 0, yMax = 0;
        for (int i = 0; i < qMin(5, indexesList.count()); ++i) {
            QPixmap pix = m_model->item(indexesList.at(i).row())->icon().pixmap(72);
            if (i == 4) {
                x = (xMax - pix.width()) / 2;
                y = (yMax - pix.height()) / 2;
            }
            painter.drawPixmap(x, y, pix);
            xMax = qMax(xMax, qMin(128, x + pix.width()));
            yMax = qMax(yMax, qMin(112, y + pix.height()));
            painter.drawRect(x + 1, y + 1, qMin(126, pix.width() - 2), qMin(110, pix.height() - 2));
            x = !(x == y) * 56;
            y = !y * 40;
        }
        painter.end();
        pix = pix.copy(0, 0, xMax, yMax);
        drag->setPixmap(pix);
    } else {
        pix = m_model->item(indexesList.at(0).row())->icon().pixmap(128);
        drag->setPixmap(pix);
    }
    drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::IgnoreAction);
}

void ThumbsViewer::abort(bool permanent) {
    if (!m_busy)
        return;

    isAbortThumbsLoading = true;

    if (!isClosing && permanent) {
        isClosing = true;
    }
}

void ThumbsViewer::loadVisibleThumbs(int scrollBarValue) {

    // Hack:
    // when a paint even is requested Qt first calls updateGeometry() on
    // everything.
    // qscrollbar emits valueChanged() in its updateGeometry(), leading to us
    // possibly recursing when calling processEvents.
    static bool processing = false;
    if (processing) {
        return;
    }
    processing = true;

    static int lastScrollBarValue = 0;

    if (scrollBarValue >= 0) {
        scrolledForward = (scrollBarValue >= lastScrollBarValue);
        lastScrollBarValue = scrollBarValue;
    } else {
        loadThumbsRange();
        processing = false;
        return;
    }

    for (;;) {
        int firstVisible = firstVisibleThumb();
        int lastVisible = lastVisibleThumb();
        if (isAbortThumbsLoading || firstVisible < 0 || lastVisible < 0) {
            processing = false;
            return;
        }

        if (scrolledForward) {
            lastVisible += ((lastVisible - firstVisible) * (Settings::thumbsPagesReadCount + 1));
            if (lastVisible >= m_model->rowCount()) {
                lastVisible = m_model->rowCount() - 1;
            }
        } else {
            firstVisible -= (lastVisible - firstVisible) * (Settings::thumbsPagesReadCount + 1);
            if (firstVisible < 0) {
                firstVisible = 0;
            }

            lastVisible += 10;
            if (lastVisible >= m_model->rowCount()) {
                lastVisible = m_model->rowCount() - 1;
            }
        }

        if (thumbsRangeFirst == firstVisible && thumbsRangeLast == lastVisible) {
            processing = false;
            return;
        }

        thumbsRangeFirst = firstVisible;
        thumbsRangeLast = lastVisible;

        loadThumbsRange();
        if (isAbortThumbsLoading) {
            processing = false;
            break;
        }
    }
    processing = false;
}

int ThumbsViewer::firstVisibleThumb() {
    for (int currThumb = 0; currThumb < m_model->rowCount(); ++currThumb) {
        if (isRowHidden(currThumb))
            continue;
        const QModelIndex idx = m_model->indexFromItem(m_model->item(currThumb));
        if (viewport()->rect().contains(QPoint(0, visualRect(idx).bottom() + 1))) {
            return idx.row();
        }
    }
    return -1;
}

int ThumbsViewer::lastVisibleThumb() {
    for (int currThumb = m_model->rowCount() - 1; currThumb >= 0; --currThumb) {
        if (isRowHidden(currThumb))
            continue;
        const QModelIndex idx = m_model->indexFromItem(m_model->item(currThumb));
        if (viewport()->rect().contains(QPoint(0, visualRect(idx).y() + 1))) {
            return idx.row();
        }
    }
    return -1;
}

void ThumbsViewer::loadFileList() {
    int j = 0;
    for (int i = 0; i < Settings::filesList.size(); ++i) {
        if (addThumb(QFileInfo(Settings::filesList.at(i))))
            ++j;
    }

    if (j) {
        m_filterDirty = true;
        filterRows(j);
    }
    promoteThumbsCount();

    if (!m_desiredThumbPath.isEmpty()) {
        setCurrentIndex(m_desiredThumbPath);
        m_desiredThumbPath.clear();
        scrollTo(currentIndex());
    } else if (m_model->rowCount() && selectionModel()->selectedIndexes().size() == 0) {
        setCurrentIndex(0);
    }
    loadVisibleThumbs();
}

void ThumbsViewer::reload(bool iterative) {
    if (m_busy) {
        abort();
        QTimer::singleShot(50, this, [=]() { reload(iterative); });
        return;
    }
    static QTimer *scrollDelay = nullptr;
    if (!scrollDelay) {
        scrollDelay = new QTimer(this);
        scrollDelay->setInterval(150);
        scrollDelay->setSingleShot(true);
        connect(scrollDelay, &QTimer::timeout, [=]() { loadVisibleThumbs(verticalScrollBar()->value()); });
    }
    scrollDelay->stop();
    disconnect(verticalScrollBar(), SIGNAL(valueChanged(int)), scrollDelay, SLOT(start()));
    m_busy = true;

    if (!iterative)
        loadPrepare();

    if (Settings::isFileListLoaded) {
        loadFileList();
        connect(verticalScrollBar(), SIGNAL(valueChanged(int)), scrollDelay, SLOT(start()));
        m_busy = false;
        return;
    }

    // Get all patterns supported by QImageReader
    static QStringList imageTypeGlobs;
    if (imageTypeGlobs.isEmpty()) {
        QMimeDatabase db;
        for (const QByteArray &type : QImageReader::supportedMimeTypes()) {
            imageTypeGlobs.append(db.mimeTypeForName(type).globPatterns());
        }
    }

    thumbsDir.setNameFilters(imageTypeGlobs);
    thumbsDir.setFilter(QDir::Files);
    if (Settings::showHiddenFiles) {
        thumbsDir.setFilter(thumbsDir.filter() | QDir::Hidden);
    }

    thumbsDir.setPath(Settings::currentDirectory);
    QDir::SortFlags tempThumbsSortFlags = thumbsSortFlags;
    if (tempThumbsSortFlags & QDir::Size || tempThumbsSortFlags & QDir::Time) {
        tempThumbsSortFlags ^= QDir::Reversed;
    }

    if (thumbsSortFlags & QDir::Time || thumbsSortFlags & QDir::Size || thumbsSortFlags & QDir::Type) {
        thumbsDir.setSorting(tempThumbsSortFlags);
    } else { // by name
        thumbsDir.setSorting(QDir::NoSort);
    }

    QStringList selection;
    if (iterative) { // remove file selection since it might be affected by the change
        setUpdatesEnabled(false);
        selection = selectedFiles();
        selectionModel()->clearSelection();
    }

    initThumbs(iterative);

    if (Settings::includeSubDirectories) {
        loadSubDirectories(iterative);
        thumbsDir.setPath(Settings::currentDirectory);
    }

    if (iterative && selection.size()) { // restore selection
        for (int i = 0; i < m_model->rowCount(); ++i) {
            for (int j = 0; j < selection.size(); ++j) {
                if (fullPathOf(i) == selection.at(j)) {
                    selectionModel()->select(m_model->index(i,0), QItemSelectionModel::Select);
                    selection.remove(j);
                    break;
                }
            }
            if (!selection.size())
                break;
        }
        setUpdatesEnabled(true);
    }

    m_busy = false;
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), scrollDelay, SLOT(start()));
}

void ThumbsViewer::loadSubDirectories(bool iterative) {
    QDirIterator dirIterator(Settings::currentDirectory, QDirIterator::Subdirectories);

    while (dirIterator.hasNext()) {
        dirIterator.next();
        if (dirIterator.fileInfo().isDir() && dirIterator.fileName() != "." && dirIterator.fileName() != "..") {
            thumbsDir.setPath(dirIterator.filePath());

            initThumbs(iterative);

            if (isAbortThumbsLoading) {
                return;
            }
        }
    }

    promoteSelectionChange();
}

void ThumbsViewer::setTagFilters(const QStringList &mandatory, const QStringList &sufficient, bool invert) {
    m_mandatoryFilterTags = mandatory;
    m_sufficientFilterTags = sufficient;
    m_invertTagFilter = invert;
    m_filterDirty = true;
    QMetaObject::invokeMethod(this, "filterRows", Qt::QueuedConnection);
}

bool ThumbsViewer::setFilter(const QString &filter, QString *error) {
    QStringList tokens = filter.split('/');
    m_filter = tokens.first().trimmed();
    m_constraints.clear();
    bool sane = true;
    for (int i = 1; i < tokens.size(); ++i) {
        m_constraints.append(Constraint());
        QStringList subtokens = tokens.at(i).split(' ', Qt::SkipEmptyParts);
        char side = 0;
        for (QString t : subtokens) {
            if (t.startsWith("<")) {
                side = side ? -1 : 1; t.remove(0,1);
            } else if (t.startsWith("=")) {
                side = side ? -1 : 3; t.remove(0,1);
            } else if (t.startsWith(">")) {
                side = side ? -1 : 2; t.remove(0,1);
            }
            if (side < 0) {
                sane = false;
                if (error)
                    *error += "Invalid sequence: " + tokens.at(i) + "\n";
                break;
            }
            if (side == 0)
                side = 3;
            if (t.isEmpty())
                continue;
            auto setSizeConstraint = [=](int multiplier) {
                bool ok;
                qint64 v = t.chopped(2).toFloat(&ok) * multiplier;
                if (!ok) { if (error) *error += "Invalid value: " + t + "\n"; return false; }
                if (side & 1) m_constraints.last().smaller = v;
                if (side & 2) m_constraints.last().bigger = v;
                if ((side & 3) == 3) {
                    m_constraints.last().smaller =  v * 101 / 100;
                    m_constraints.last().bigger  =  v *  99 / 100;
                }
                return true;
            };
            auto setAgeConstraint = [=](int multiplier) {
                bool ok;
                qint64 v = t.chopped(1).toFloat(&ok) * multiplier;
                if (!ok) { if (error) *error += "Invalid value: " + t + "\n"; return false; }
                if (side & 1) m_constraints.last().younger = v;
                if (side & 2) m_constraints.last().older = v;
                if ((side & 3) == 3) {
                    m_constraints.last().younger =  v * 101 / 100;
                    m_constraints.last().older   =  v *  99 / 100;
                }
                return true;
            };

            if (t.endsWith("kb", Qt::CaseInsensitive)) {
                if (!setSizeConstraint(1024)) { sane = false; break; }
            } else if (t.endsWith("mb", Qt::CaseInsensitive)) {
                if (!setSizeConstraint(1024*1024)) { sane = false; break; }
            } else if (t.endsWith("gb", Qt::CaseInsensitive)) {
                if (!setSizeConstraint(1024*1024*1024)) { sane = false; break; }
            } else if (t.endsWith("m", Qt::CaseSensitive)) {
                if (!setAgeConstraint(60)) { sane = false; break; }
            } else if (t.endsWith("h", Qt::CaseInsensitive)) {
                if (!setAgeConstraint(60*60)) { sane = false; break; }
            } else if (t.endsWith("d", Qt::CaseInsensitive)) {
                if (!setAgeConstraint(24*60*60)) { sane = false; break; }
            } else if (t.endsWith("w", Qt::CaseInsensitive)) {
                if (!setAgeConstraint(7*24*60*60)) { sane = false; break; }
            } else if (t.endsWith("M", Qt::CaseInsensitive)) {
                if (!setAgeConstraint(30*24*60*60)) { sane = false; break; }
            } else if (t.endsWith("y", Qt::CaseInsensitive)) {
                if (!setAgeConstraint(365*24*60*60)) { sane = false; break; }
            } else if (t.endsWith("mp", Qt::CaseInsensitive)) {
                bool ok;
                qint64 v = t.chopped(2).toFloat(&ok) * 1000*1000;
                if (!ok) {
                    if (error) { *error += "Invalid value: " + t + "\n"; } sane = false;  break;
                }
                if (side & 1) m_constraints.last().maxPix = v;
                if (side & 2) m_constraints.last().minPix = v;
                if ((side & 3) == 3) {
                    m_constraints.last().maxPix =  v * 101 / 100;
                    m_constraints.last().minPix =  v *  99 / 100;
                }
            } else if (t.contains("x", Qt::CaseInsensitive)) {
                QStringList st = t.split('x', Qt::KeepEmptyParts, Qt::CaseInsensitive);
                if (st.size() != 2) {
                    if (error) { *error += "Invalid value: " + t + "\n"; } sane = false;  break;
                }
                QSize sz(0,0); bool ok; int v;
                v = st.at(0).toInt(&ok);
                if (ok) sz.setWidth(v);
                v = st.at(1).toInt(&ok);
                if (ok) sz.setHeight(v);
                if (sz.isNull()) {
                    if (error) { *error += "Invalid value: " + t + "\n"; } sane = false;  break;
                }
                if (side & 1) m_constraints.last().maxRes = sz;
                if (side & 2) m_constraints.last().minRes = sz;
            } else {
                QDateTime date = QDateTime::fromString(t, "yyyy-MM-dd");
                if (!date.isValid()) {
                    if (error) { *error += "Invalid value: " + t + "\n"; } sane = false; break;
                }
                qint64 secs = date.secsTo(QDateTime::currentDateTime());
                if (secs < 0) {
                    qWarning() << "warning, issued future date constraint";
                    secs = 0;
                }
                if (side & 1) m_constraints.last().younger = secs;
                if (side & 2) m_constraints.last().older = secs;
                if ((side & 3) == 3) {
                    m_constraints.last().younger += 24*60*60;
                    m_constraints.last().older   -= 24*60*60;
                }
            }
            side = 0;
        }
        side = 0;
    }
    if (sane) {
        m_filterDirty = true;
        QMetaObject::invokeMethod(this, "filterRows", Qt::QueuedConnection);
    }
    return sane;
}

void ThumbsViewer::filterRows(int first, int last) {
    if (!m_filterDirty)
        return;

    if (first < 0)
        first = 0;
    if (last < 0)
        last = m_model->rowCount() - 1;

    QStringList hidden;
    QStringList shown;

    QList<QRegularExpression> globs;
    for (const QString &g : m_filter.split(" ", Qt::SkipEmptyParts))
        globs << QRegularExpression::fromWildcard(g, Qt::CaseInsensitive, QRegularExpression::UnanchoredWildcardConversion);

    for (int i = first; i < last + 1; ++i) {
        bool wasHidden = isRowHidden(i);
        QFileInfo fileInfo(fullPathOf(i));

        bool globFailure = false;
        for (const QRegularExpression &g : globs) {
            if (fileInfo.fileName().contains(g))
                continue;
            if (!wasHidden)
                hidden << fileInfo.filePath();
            setRowHidden(i, true);
            globFailure = true;
            break;
        }
        if (globFailure)
            continue;

        if (isConstrained(fileInfo)) {
            if (!wasHidden)
                hidden << fileInfo.filePath();
            setRowHidden(i, true);
            continue;
        }
        if (!matchesTagFilter(fileInfo.filePath())) {
            // deliberately do NOT emit those as hidden to keep their tags in the tagviewer
//            if (!wasHidden)
//                hidden << fileInfo.filePath();
            setRowHidden(i, true);
            continue;
        }
        if (wasHidden)
            shown << fileInfo.filePath();
        setRowHidden(i, false);
    }

    m_visibleThumbs += shown.size() - hidden.size();

    if (!hidden.isEmpty())
        emit filesHidden(hidden);
    if (!shown.isEmpty())
        emit filesShown(shown);
    loadVisibleThumbs(); // slow last
    promoteThumbsCount();
    m_filterDirty = false;
}

QSize ThumbsViewer::itemSizeHint() const
{
    switch(Settings::thumbsLayout) {
    case Squares:
        return QSize(thumbSize, thumbSize);
    case Compact:
        return QSize(thumbSize, thumbSize + int(2.5*gs_fontHeight));
    case Classic:
        return QSize(thumbSize, thumbSize + int(1.5*gs_fontHeight));
    default:
        qWarning() << "Invalid thumbs layout" << Settings::thumbsLayout;
        return QSize(thumbSize, thumbSize);
    }

}


void ThumbsViewer::loadPrepare() {

    m_model->clear();

    static QString lastPath; // do we need to drop the metadata cache
    m_histSorted = false;
    m_visibleThumbs = 0;
    if (Settings::isFileListLoaded || lastPath != Settings::currentDirectory) {
        m_fsWatcher->removePaths(m_fsWatcher->directories());
        lastPath = Settings::isFileListLoaded ? QString() : Settings::currentDirectory;
        Metadata::dropCache();
        histFiles.clear(); // these can grow out of control and currently sort O(n^2)
        histograms.clear();
    }

    if (isNeedToScroll) {
        scrollToTop();
    }

    if (!isClosing) {
        isAbortThumbsLoading = false;
    }

    thumbsRangeFirst = -1;
    thumbsRangeLast = -1;
}

void ThumbsViewer::refreshThumbs() {
    for (int row = 0; row < m_model->rowCount(); ++row)
        m_model->setData(m_model->index(row, 0), false, LoadedRole);
    setIconSize(QSize(thumbSize, thumbSize));
    setViewportMargins(0, gs_fontHeight, 0, 0);
    if (Settings::thumbsLayout == Squares) {
        setSpacing(0);
        setUniformItemSizes(true);
        setGridSize(itemSizeHint());
    } else if (Settings::thumbsLayout == Compact) {
        setSpacing(0);
        setUniformItemSizes(false);
        setGridSize(itemSizeHint());
    } else {
        setUniformItemSizes(false);
        setGridSize(QSize(dynamicGridWidth(), itemSizeHint().height() + gs_fontHeight));
    }
    thumbsRangeFirst = -1;
    thumbsRangeLast = -1;
    loadVisibleThumbs();
}

void ThumbsViewer::loadDuplicates()
{
    if (m_busy) {
        abort();
        QTimer::singleShot(50, this, [=]() { loadDuplicates(); });
        return;
    }
    m_busy = true;
    loadPrepare();

    emit status(tr("Searching duplicate images..."));

    findDupes(true);
    m_model->setSortRole(SortRole);

    if (Settings::includeSubDirectories) {
        QDirIterator iterator(Settings::currentDirectory, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
                thumbsDir.setPath(iterator.filePath());

                findDupes(false);
                if (isAbortThumbsLoading) {
                    goto finish;
                }
            }
        }
    }

finish:
    thumbsDir.setPath(Settings::currentDirectory);
    m_model->sort(0);
    m_busy = false;
    return;
}

bool ThumbsViewer::isConstrained(const QFileInfo &fileInfo) const {
    bool constrained = false;
    for (const Constraint &c : m_constraints) {
        constrained = false;
        if ((constrained = (c.smaller && fileInfo.size() > c.smaller))) continue;
        if ((constrained = (c.bigger  && fileInfo.size() < c.bigger ))) continue;
        qint64 age = fileInfo.lastModified().secsTo(QDateTime::currentDateTime());
        if ((constrained = (c.older   && age < c.older  ))) continue;
        if ((constrained = (c.younger && age > c.younger))) continue;

        if (!(c.minPix || c.maxPix || c.minRes.isValid() || c.maxRes.isValid()))
            break;
        // we gotta inspect the image for this
        QSize res = QImageReader(fileInfo.filePath()).size();
        if (!res.isValid())
            break; // if we can't check the image we give it a pass
        if ((constrained = (c.minPix && res.width()*res.height() < c.minPix))) continue;
        if ((constrained = (c.maxPix && res.width()*res.height() > c.maxPix))) continue;
        if ((constrained = (c.minRes.width() > 0 && res.width() < c.minRes.width()))) continue;
        if ((constrained = (c.minRes.height() > 0 && res.height() < c.minRes.height()))) continue;
        if ((constrained = (c.maxRes.width() > 0 && res.width() > c.maxRes.width()))) continue;
        if ((constrained = (c.maxRes.height() > 0 && res.height() > c.maxRes.height()))) continue;

        break; // this constraint is sufficient
    }
    return constrained;
}

bool ThumbsViewer::matchesTagFilter(const QString &path) const {
    bool emptyFilter = true;
    QSet<QString> itags;
    if (!m_mandatoryFilterTags.isEmpty()) {
        emptyFilter = false;
        itags = Metadata::tags(path);
        for (const QString &s : m_mandatoryFilterTags) {
            if (itags.contains(s) == m_invertTagFilter)
                return false;
        }
    }
    if (!m_sufficientFilterTags.isEmpty()) {
        if (emptyFilter)
            itags = Metadata::tags(path);
        emptyFilter = true;
        for (const QString &s : m_sufficientFilterTags) {
            if (itags.contains(s))
                return !m_invertTagFilter;
        }
        return m_invertTagFilter;
    }
    if (m_invertTagFilter && emptyFilter && !Metadata::tags(path).isEmpty())
        return false;

    return true;
}

void ThumbsViewer::initThumbs(bool iterative) {
    m_fsWatcher->addPath(thumbsDir.path());
    QFileInfoList thumbFileInfoList = thumbsDir.entryInfoList();

    if (iterative) {
        int doing = 0;
        for (int i = m_model->rowCount() - 1; i >=0 ; --i) {
            QStandardItem *item = m_model->item(i);
            if (!item) {
                qDebug() << "meek, why's there no item?!";
                continue;
            }
            QFileInfo file(item->data(FileNameRole).toString());
            if (!file.exists()) {
                if (!isRowHidden(i))
                    --m_visibleThumbs;
                m_model->removeRow(i); // file was deleted, drop thumb
                continue;
            }
            if (item->data(TimeRole).toDateTime() != file.lastModified()) { // outdated
                m_model->item(i)->setData(false, LoadedRole); // reload
                int idx = histFiles.indexOf(file.filePath());
                if (idx > -1) {
                    histFiles.remove(idx);
                    histograms.remove(idx);
                }
            }
            doing += thumbFileInfoList.removeAll(file); // we already have this file
        }
    }

    if (!(thumbsSortFlags & QDir::Time) && !(thumbsSortFlags & QDir::Size) && !(thumbsSortFlags & QDir::Type)) {
        QCollator collator;
        if (thumbsSortFlags & QDir::IgnoreCase) {
            collator.setCaseSensitivity(Qt::CaseInsensitive);
        }

        collator.setNumericMode(true);

        if (thumbsSortFlags & QDir::Reversed) {
            std::sort(thumbFileInfoList.begin(), thumbFileInfoList.end(), [&](const QFileInfo &a, const QFileInfo &b) {
                    return collator.compare(a.fileName(), b.fileName()) > 0;
                    });
        } else {
            std::sort(thumbFileInfoList.begin(), thumbFileInfoList.end(), [&](const QFileInfo &a, const QFileInfo &b) {
                    return collator.compare(a.fileName(), b.fileName()) < 0;
                    });
        }
    }

    addThumbs(thumbFileInfoList);

    if (!iterative) {
        if (!m_desiredThumbPath.isEmpty()) {
            setCurrentIndex(m_desiredThumbPath);
            m_desiredThumbPath.clear();
            scrollTo(currentIndex());
        } else if (m_model->rowCount() && selectionModel()->selectedIndexes().size() == 0) {
            setCurrentIndex(0);
        }
    }
    promoteThumbsCount();
    loadVisibleThumbs();
}

void ThumbsViewer::promoteThumbsCount() {
    if (m_visibleThumbs < 1)
        emit status(tr("No images"));
    else if (m_visibleThumbs != m_model->rowCount())
        emit status(tr("%n of %1 image(s)", "", m_visibleThumbs).arg(m_model->rowCount()));
    else
        emit status(tr("%n image(s)", "", m_visibleThumbs));
}

struct DuplicateImage
{
    QString filePath;
    unsigned int duplicates;
    unsigned int id = 0;
};

void ThumbsViewer::findDupes(bool resetCounters)
{
    const QFileInfoList thumbFileInfoList = thumbsDir.entryInfoList();
    static unsigned int duplicateFiles, scannedFiles, totalFiles;
    static QHash<QBitArray, DuplicateImage> imageHashes;
    if (resetCounters) {
        imageHashes.clear();
        duplicateFiles = scannedFiles = totalFiles = 0;
    }
    totalFiles += thumbFileInfoList.size();

    QStringList filterTokens = m_filter.split(" ", Qt::SkipEmptyParts);

    QElapsedTimer timer;
    timer.start();

    for (int currThumb = 0; currThumb < thumbFileInfoList.size(); ++currThumb) {
        if (timer.elapsed() > 30) {
            emit progress(scannedFiles, totalFiles);
            emit status(tr("Found %n duplicate(s) among %1 files", "", duplicateFiles).arg(totalFiles));
            m_model->sort(0);
            QApplication::processEvents();
            timer.restart();
            if (isAbortThumbsLoading)
                break;
        }

        QFileInfo thumbFileInfo = thumbFileInfoList.at(currThumb);

        bool nameMatch = filterTokens.isEmpty();
        const QString &fn = thumbFileInfo.fileName();
        if (!nameMatch) {
            for (const QString &t : filterTokens) {
                if (fn.contains(t, Qt::CaseInsensitive)) {
                    nameMatch = true;
                    break;
                }
            }
        }
        if (!nameMatch || isConstrained(thumbFileInfo))
            continue;

        QImageReader imageReader;
        QString imageFileName = thumbFileInfo.absoluteFilePath();
        QImage image;
        imageReader.setFileName(imageFileName);
        imageReader.setQuality(50); // 50 is the threshold where Qt does fast decoding, but still good scaling
        const QSize targetSize = imageReader.size();
        QSize realSize;
        QString thumbnailPath = locateThumbnail(imageFileName);
        if (!thumbnailPath.isEmpty() && QImageReader(thumbnailPath).canRead()) {
            imageReader.setFileName(thumbnailPath);
            imageReader.read(&image);
            realSize = QSize(image.text("Thumb::Image::Width").toInt(), image.text("Thumb::Image::Height").toInt());
        }
        if (targetSize != realSize) {
            imageReader.setFileName(imageFileName);
            imageReader.read(&image);
        }

        ++scannedFiles;

        if (image.isNull()) {
            qWarning() << "invalid image" << thumbFileInfo.fileName();
            continue;
        }

        QBitArray imageHash(64);
        image = image.convertToFormat(QImage::Format_Grayscale8).scaled(9, 9, Qt::KeepAspectRatioByExpanding /*, Qt::SmoothTransformation*/);
        for (int y=0; y<8; ++y) {
            const uchar *line = image.scanLine(y);
            //const uchar *nextLine = image.scanLine(y+1);
            for (int x=0; x<8; ++x) {
                imageHash.setBit(y * 8 + x, line[x] > line[x+1]);
                //imageHash.setBit(y * 8 + x + 64, line[x] > nextLine[x]);
            }
        }

        QString currentFilePath = thumbFileInfo.filePath();

        QHash<QBitArray, DuplicateImage>::iterator match = imageHashes.find(imageHash);
        if (match == imageHashes.end()) {
            imageHashes.insert(imageHash, {currentFilePath, 0, (unsigned int)imageHashes.count()});
        } else {
            ++duplicateFiles;
            // display sibling
            int newFiles = 0;
            if (match.value().duplicates < 1) {
                if (QStandardItem *item = addThumb(QFileInfo(match.value().filePath))) {
                    ++newFiles;
                    item->setData(match.value().id, SortRole);
                }
            }
            // ... and this one
            match.value().duplicates++;
            if (QStandardItem *item = addThumb(QFileInfo(currentFilePath))) {
                ++newFiles;
                item->setData(match.value().id, SortRole);
            }
            if (newFiles) {
                m_filterDirty = true;
                filterRows(m_model->rowCount() - newFiles);
            }
        }

        if (isAbortThumbsLoading) {
            break;
        }
    }

    emit progress(scannedFiles, totalFiles);
    emit status(tr("Found %n duplicate(s) among %1 files", "", duplicateFiles).arg(totalFiles));
    QApplication::processEvents();
}

static Histogram calcHist(const QImage &img)
{
    Histogram hist;
    if (img.isNull()) {
        qWarning() << "Histogram calculation: Invalid file";
        return hist;
    }
    const QImage image = img.scaled(256, 256).convertToFormat(QImage::Format_RGB888);
    for (int y=0; y<image.height(); y++) {
        const uchar *line = image.scanLine(y);
        for (int x=0; x<image.width(); x++) {
            const int index = x * 3;
            hist.red[line[index + 0]] += 1.f;
            hist.green[line[index + 1]] += 1.f;
            hist.blue[line[index + 2]] += 1.f;
        }
    }
    return hist;
}
/*
static Histogram calcHist(const QString &filePath) {
    QImageReader reader(filePath);
    reader.setScaledSize(QSize(256, 256));
    reader.setAutoTransform(false);
    QImage image = reader.read();
    if (image.isNull()) {
        qWarning() << "Invalid file" << filePath << reader.errorString();
        return {};
    }
    return calcHist(image);
}
*/

void ThumbsViewer::scanForSort(UserRoles role) {
    if (role != HistogramRole && role != BrightnessRole)
        return;

    QProgressDialog progress(tr("Loading..."), tr("Abort"), 0, m_model->rowCount(), this);
    progress.setMaximum(m_model->rowCount());

    QElapsedTimer timer;
    timer.start();
    qint64 totalTime = 0;

    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem *item = m_model->item(i);

        Q_ASSERT(item);
        if (!item) {
            continue;
        }

        const QString filename = item->data(FileNameRole).toString();
        if (item->data(BrightnessRole).isValid() && histFiles.contains(filename)) {
            continue;
        }

        // try to use thumbnail (they're used when storing the histogram in ::loadThumb as well)
        bool haveThumbogram = false;
        QString thumbname = locateThumbnail(filename);
        if (!thumbname.isEmpty()) {
            QImageReader thumbReader(thumbname);
            QImage image;
            thumbReader.read(&image);
            haveThumbogram = QImageReader(filename).size() == QSize(image.text("Thumb::Image::Width").toInt(), 
                                                                    image.text("Thumb::Image::Height").toInt());
            if (haveThumbogram) {
                histograms.append(calcHist(image));
                item->setData(qGray(image.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).pixel(0, 0)) / 255.0, BrightnessRole);
            }
        }
        if (!haveThumbogram) {
            QImageReader reader(filename);
            reader.setScaledSize(QSize(256, 256));
            reader.setAutoTransform(false);
            QImage image = reader.read();
            if (image.isNull()) {
                qWarning() << "Invalid file" << filename << reader.errorString();
                continue;
            }
            histograms.append(calcHist(image));
            item->setData(qGray(image.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).pixel(0, 0)) / 255.0, BrightnessRole);
        }
        histFiles.append(filename);
        m_histSorted = false;

        if (timer.elapsed() > 30) {
            if ((totalTime += timer.elapsed()) > 500) {
                progress.show();
            }
            progress.setValue(i);
            QApplication::processEvents();
            if (progress.wasCanceled())
                return;
            timer.restart();
        }
    }

    if (role == BrightnessRole)
        return; // we're done, brightness is an absolute measure

    if (m_histSorted)
        return; // this should be sorted already, less files doesn't change the similarity of the remaining - we've some holes in the list

    progress.setLabelText(tr("Comparing..."));
    progress.setMaximum(histFiles.count());
    progress.setValue(0);
    if ((totalTime += timer.elapsed()) > 600)
        progress.show();
    timer.restart();

    for (int i=0; i<histFiles.size() - 1; i++) {
        float minScore = std::numeric_limits<float>::max();
        int minIndex = i+1;

        for (int j=i+1; j<histFiles.size(); j++) {
            const float score = histograms.at(i).compare(histograms.at(j));
            if (score > minScore) {
                continue;
            }
            minIndex = j;
            minScore = score;
        }
        histFiles.swapItemsAt(i+1, minIndex);
        histograms.swapItemsAt(i+1, minIndex);

        if (timer.elapsed() > 30) {
            if ((totalTime += timer.elapsed()) > 700)
                progress.show();
            progress.setValue(i);
            QApplication::processEvents();
            if (progress.wasCanceled())
                return;
            timer.restart();
        }
    }

    progress.setLabelText(tr("Sorting..."));
    progress.setMaximum(m_model->rowCount());
    progress.setValue(0);
    if ((totalTime += timer.elapsed()) > 800)
        progress.show();
    timer.restart();

    QHash<QString, int> indices;
    for (int i=0; i<histFiles.size(); i++) {
        indices[histFiles.at(i)] = i;
    }
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem *item = m_model->item(i);
        if (!item) {
            qWarning() << "Invalid item" << i;
            continue;
        }
        const QString filename = item->data(FileNameRole).toString();
        QHash<QString, int>::const_iterator cit = indices.find(filename);
        if (cit == indices.end()) {
            qWarning() << "Invalid file" << filename;
            continue;
        }
        item->setData(indices.size() - *cit, HistogramRole);

        if (timer.elapsed() > 30) {
            if ((totalTime += timer.elapsed()) > 900)
                progress.show();
            progress.setValue(i);
            QApplication::processEvents();
            if (progress.wasCanceled())
                return;
            timer.restart();
        }
    }
    m_histSorted = true;
}

void ThumbsViewer::loadThumbsRange() {
    static bool isInProgress = false;

    if (isInProgress) {
        isAbortThumbsLoading = true;
        QTimer::singleShot(0, this, SLOT(loadThumbsRange()));
        return;
    }

    int currentRowCount = m_model->rowCount();
    if (!currentRowCount)
        return;

    isInProgress = true;

    QElapsedTimer timer;
    timer.start();

    for (bool fastOnly : { true, false }) {
    int currThumb;
    for (scrolledForward ? currThumb = thumbsRangeFirst : currThumb = thumbsRangeLast;
         (scrolledForward ? currThumb <= thumbsRangeLast : currThumb >= thumbsRangeFirst);
         scrolledForward ? ++currThumb : --currThumb) {

        if (isAbortThumbsLoading || m_model->rowCount() != currentRowCount || currThumb < 0)
            break;

        if (isRowHidden(currThumb))
            continue;

        QStandardItem *item = m_model->item(currThumb);
        if (!item) {
            qDebug() << "meek" << m_model->rowCount() << currentRowCount << currThumb << thumbsRangeFirst << thumbsRangeLast << scrolledForward;
            continue;
        }
        if (item->data(LoadedRole).toBool())
            continue;

        loadThumb(currThumb, fastOnly);

        if (timer.elapsed() > 30) {
            QApplication::processEvents();
            timer.restart();
        }
    }
    }

    isInProgress = false;

    if (!isClosing) {
        isAbortThumbsLoading = false;
    }
}

QString ThumbsViewer::thumbnailFileName(const QString &originalPath)
{
    QFileInfo info(originalPath);
    QString canonicalPath = info.canonicalFilePath();
    if (canonicalPath.isEmpty()) {
//        qWarning() << originalPath << "does not exist!";
        canonicalPath = info.absoluteFilePath();
    }
    QUrl url = QUrl::fromLocalFile(canonicalPath);
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QFile::encodeName(url.adjusted(QUrl::RemovePassword).url()));
    return QString::fromLatin1(md5.result().toHex()) + QStringLiteral(".png");
}

int ThumbsViewer::removeFromCache(const QString &path) {
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    return 0;
#else
    const QString filename = thumbnailFileName(path);
    if (filename.isEmpty()) {
        qDebug() << "meek, remove thumb" << filename;
        return 0;
    }
    QStringList folders = {
        QStringLiteral("xx-large/"), // max 1024px
        QStringLiteral("x-large/"), // max 512px
        QStringLiteral("large/"), // max 256px, doesn't look too bad when upscaled to max
        QStringLiteral("normal/")
    };
    int count = 0;
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
                                                                        QLatin1String("/thumbnails/");
    for (const QString &folder : folders) {
        if (QFile::remove(basePath + folder + filename))
            ++count;
    }
    return count;
#endif
}

int ThumbsViewer::moveCache(const QString &oldpath, const QString &newpath) {
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    return 0;
#else
    const QString oldthumb = thumbnailFileName(oldpath);
    const QString newthumb = thumbnailFileName(newpath);
    if (oldthumb.isEmpty() || newthumb.isEmpty()) {
        qDebug() << "meek, move thumb" << oldthumb << newthumb;
        return 0;
    }
    QStringList folders = {
        QStringLiteral("xx-large/"), // max 1024px
        QStringLiteral("x-large/"), // max 512px
        QStringLiteral("large/"), // max 256px, doesn't look too bad when upscaled to max
        QStringLiteral("normal/")
    };
    int count = 0;
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
                                                                        QLatin1String("/thumbnails/");
    for (const QString &folder : folders) {
        if (QFile::rename(basePath + folder + oldthumb, basePath + folder + newthumb) ||
            QFile::remove(basePath + folder + oldthumb)) // at least delete the source - the dst likely already exists
            ++count;
    }
    return count;
#endif
}

QString ThumbsViewer::locateThumbnail(const QString &originalPath) const
{
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    return "";
#endif
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
                                                                        QLatin1String("/thumbnails/");
    if (originalPath.startsWith(basePath))
        return QString(); // we're in the thumbnail cache, no point in checking stuff

    QStringList folders = {
        QStringLiteral("xx-large/"), // max 1024px
        QStringLiteral("x-large/"), // max 512px
        QStringLiteral("large/"), // max 256px, doesn't look too bad when upscaled to max
    };

    if (thumbSize <= 200) {
        folders.append(QStringLiteral("normal/")); // 128px max
    }
    const QString filename = thumbnailFileName(originalPath);
    const QFileInfo originalInfo(originalPath);
    for (const QString &folder : folders) {
        QFileInfo info(basePath + folder + filename);
        if (!info.exists()) {
            continue;
        }
        if (originalInfo.metadataChangeTime() > info.lastModified()) {
            continue;
        }
        if (originalInfo.lastModified() > info.lastModified()) {
            continue;
        }
        return info.absoluteFilePath();
    }
    return QString();
}

void ThumbsViewer::storeThumbnail(const QString &originalPath, QImage thumbnail, const QSize &originalSize) const {
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    return;
#endif
    const QString canonicalPath = QFileInfo(originalPath).canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        qWarning() << "Asked to store thumbnail for non-existent path" << originalPath;
        return;
    }

    QString folder = QStringLiteral("normal/");
    const int maxSize = qMax(thumbnail.width(), thumbnail.height());
    if (maxSize < 64) {
        qDebug() << "Refusing to store tiny thumbnail" << thumbnail.size();
        return;
    }
    if (maxSize >= 1024) {
        folder = QStringLiteral("xx-large/");
        thumbnail = thumbnail.scaled(1024, 1024, Qt::KeepAspectRatio);
    } else if (maxSize >= 384) {
        folder = QStringLiteral("x-large/");
        thumbnail = thumbnail.scaled(512, 512, Qt::KeepAspectRatio);
    } else if (maxSize > 200) {
        folder = QStringLiteral("large/");
        thumbnail = thumbnail.scaled(256, 256, Qt::KeepAspectRatio);
    } else if (maxSize >= 100) {
        folder = QStringLiteral("normal/");
        thumbnail = thumbnail.scaled(128, 128, Qt::KeepAspectRatio);
    } else {
        qWarning() << "Thumbnail too small" << thumbnail.size();
        return;
    }

    const QString filename = thumbnailFileName(originalPath);
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
        QLatin1String("/thumbnails/");

    if (!QFileInfo::exists(basePath + folder)) {
        QDir().mkpath(basePath + folder);
    }

    const QString fullPath = basePath + folder + filename;
    QFileInfo info(fullPath);

    QDateTime lastModified = info.lastModified();
    if (info.metadataChangeTime() > info.lastModified()) {
        lastModified = info.metadataChangeTime();
    }
    thumbnail.setText(QStringLiteral("Thumb::MTime"), QString::number(lastModified.toSecsSinceEpoch()));

    QUrl url = QUrl::fromLocalFile(canonicalPath).adjusted(QUrl::RemovePassword);
    thumbnail.setText(QStringLiteral("Thumb::URI"), url.url());

    thumbnail.setText(QStringLiteral("Thumb::Image::Width"), QString::number(originalSize.width()));
    thumbnail.setText(QStringLiteral("Thumb::Image::Height"), QString::number(originalSize.height()));
    thumbnail.setText("Software", "Phototonic");
    thumbnail.convertToColorSpace(QColorSpace::SRgb);

    thumbnail.save(fullPath);
}

bool ThumbsViewer::loadThumb(int currThumb, bool fastOnly) {
    if (!m_model->item(currThumb)) {
        qDebug() << "meeek: loadThumb for invalid row" << currThumb;
        return false;
    }
    if (m_model->item(currThumb)->data(LoadedRole).toBool())
        return true;

    QImageReader thumbReader;
    QString imageFileName = m_model->item(currThumb)->data(FileNameRole).toString();
    QImage thumb;
    bool imageReadOk = false;
    bool shouldStoreThumbnail = false;

    thumbReader.setFileName(imageFileName);
    thumbReader.setQuality(50); // 50 is the threshold where Qt does fast decoding, but still good scaling
    const QSize origThumbSize = thumbReader.size();
    QSize currentThumbSize = origThumbSize;

    QString thumbnailPath = locateThumbnail(imageFileName);
    if (!thumbnailPath.isEmpty()) {
        if (QImageReader(thumbnailPath).canRead()) {
            thumbReader.setFileName(thumbnailPath);
        } else {
            qWarning() << "Invalid thumbnail" << thumbnailPath;
            shouldStoreThumbnail = true;
        }
    } else {
        shouldStoreThumbnail = true;
    }
    if (fastOnly && shouldStoreThumbnail)
        return false;

    auto readThreaded = [&]() {
        bool wentOk = false;
        QThread *thread = QThread::create([&](){wentOk = thumbReader.read(&thumb);});
        thread->start();
        while (!thread->wait(30)) {
            QApplication::processEvents();
        }
        thread->deleteLater();
        return wentOk;
    };

    QSize thumbSizeQ(thumbSize,thumbSize);
    if (currentThumbSize.isValid()) {
        bool scaleMe =  Settings::upscalePreview ||
                        currentThumbSize.width() > thumbSize ||
                        currentThumbSize.height() > thumbSize;
        if (scaleMe && currentThumbSize != thumbSizeQ) {
            currentThumbSize.scale(thumbSizeQ, Settings::thumbsLayout != Classic ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
        }

        thumbReader.setScaledSize(currentThumbSize);
        imageReadOk = shouldStoreThumbnail ? readThreaded() : thumbReader.read(&thumb);

        if (imageReadOk && !shouldStoreThumbnail) {
            int w = thumb.text("Thumb::Image::Width").toInt();
            int h = thumb.text("Thumb::Image::Height").toInt();
            if (origThumbSize != QSize(w, h)) {
                if (fastOnly)
                    return false;
                qWarning() << "Invalid size in stored thumbnail" << w << h << "vs" << origThumbSize;
                imageReadOk = false;
            }
        }
        if (!imageReadOk && !shouldStoreThumbnail) { // tried thumbnail but somehow failed, sanitize it
            shouldStoreThumbnail = true;
            thumbReader.setFileName(imageFileName);
            imageReadOk = readThreaded();
        }
    }

    if (imageReadOk) {
        if (shouldStoreThumbnail) {
            if (!origThumbSize.isValid() || qMax(origThumbSize.width(), origThumbSize.height()) > 1024)
                storeThumbnail(imageFileName, thumb, origThumbSize);
//            else
//                qDebug() << "not storing thumb for pathetically small image" << origThumbSize;
        }
        if (Settings::exifThumbRotationEnabled) {
            thumb = thumb.transformed(Metadata::transformation(imageFileName), Qt::SmoothTransformation);
            currentThumbSize = thumb.size();
            currentThumbSize.scale(thumbSizeQ, Settings::thumbsLayout != Classic ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
        }

        m_model->item(currThumb)->setData(qGray(thumb.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).pixel(0, 0)) / 255.0, BrightnessRole);

        if (Settings::thumbsLayout != Classic) {
            thumb = SmartCrop::crop(thumb, thumbSizeQ);
        }

        m_model->item(currThumb)->setIcon(QPixmap::fromImage(thumb));
        m_model->item(currThumb)->setData(true, LoadedRole);
        if (!histFiles.contains(imageFileName)) {
            histograms.append(calcHist(thumb));
            histFiles.append(imageFileName);
        }
        m_model->item(currThumb)->setSizeHint(itemSizeHint());
    } else {
        m_model->item(currThumb)->setIcon(QIcon(":/images/error_image.png"));
        currentThumbSize.setHeight(BAD_IMAGE_SIZE);
        currentThumbSize.setWidth(BAD_IMAGE_SIZE);
        return false;
    }
    return true;
}

int ThumbsViewer::addThumbs(const QFileInfoList &fileInfos) {
    QElapsedTimer timer;
    timer.start();
//    int totalTime = 0;

    int batchStart = 0, batchEnd = -1;
    int added = 0;
    for (int i = 0; i < fileInfos.size(); ++i) {
        if (addThumb(fileInfos.at(i))) {
            ++added;
            ++batchEnd;
        }

        if (timer.elapsed() > 30) {
            m_filterDirty = true;
            filterRows(batchStart, batchEnd);
            batchStart = batchEnd;
            QApplication::processEvents();
            /** @todo: nice idea, but doesn't work
            totalTime += timer.elapsed();
            if (totalTime > 500) { // if this takes too long
                loadVisibleThumbs(); // make it look fast
                totalTime = INT_MIN; // so don't time out again.
            }
            */
            timer.restart();
        }
    }
    m_filterDirty = true;
    filterRows(batchStart, -1);
    return added;
}

QStandardItem * ThumbsViewer::addThumb(const QFileInfo &thumbFileInfo) {

    Metadata::cache(thumbFileInfo.filePath());

    QStandardItem *thumbItem = new QStandardItem();
    thumbItem->setData(false, LoadedRole);
    thumbItem->setData(thumbFileInfo.size(), SizeRole);
    thumbItem->setData(thumbFileInfo.suffix(), TypeRole);
    thumbItem->setData(thumbFileInfo.lastModified(), TimeRole);
    thumbItem->setData(thumbFileInfo.filePath(), FileNameRole);
    qint64 exifTime = Metadata::dateTimeOriginal(thumbFileInfo.filePath());
    if (!exifTime)
        exifTime = thumbFileInfo.birthTime().toSecsSinceEpoch();
    thumbItem->setData(exifTime, DateTimeOriginal);
    thumbItem->setSizeHint(itemSizeHint());

    if (Settings::thumbsLayout != Squares) {
        thumbItem->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
        thumbItem->setText(thumbFileInfo.fileName());
    }

    m_model->appendRow(thumbItem);
    setRowHidden(m_model->rowCount() - 1, true);
    return thumbItem;
}

void ThumbsViewer::mousePressEvent(QMouseEvent *event) {
    QListView::mousePressEvent(event);

    if (Settings::reverseMouseBehavior && event->button() == Qt::MiddleButton) {
        if (selectionModel()->selectedIndexes().size() == 1)
                emit(doubleClicked(selectionModel()->selectedIndexes().first()));
    }
}

int ThumbsViewer::dynamicGridWidth() {
    int sbd = verticalScrollBar()->sizeHint().width();
    if (!verticalScrollBar()->isVisible())
        sbd += sbd + 1;
    const int w = viewport()->width() - sbd;
    int pad = 0;
    if (int hcount = w/thumbSize) {
        pad = (w % thumbSize) / hcount;
        if (pad < 5 && --hcount > 0)
            pad = ((w % thumbSize) + thumbSize) / hcount;
    }
    return thumbSize + pad;
}

void ThumbsViewer::resizeEvent(QResizeEvent *event) {
    if (!m_resize)
        return;
    QListView::resizeEvent(event);
    if (Settings::thumbsLayout == Classic)
        setGridSize(QSize(dynamicGridWidth(), gridSize().height()));
    scrollTo(currentIndex());
    m_loadThumbTimer.start();
}

void ThumbsViewer::invertSelection() {
    QItemSelection toggleSelection;
    QModelIndex firstIndex = m_model->index(0, 0);
    QModelIndex lastIndex = m_model->index(m_model->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    selectionModel()->select(toggleSelection, QItemSelectionModel::Toggle);
}

void ThumbsViewer::setNeedToScroll(bool needToScroll) {
    this->isNeedToScroll = needToScroll;
}

QImage ThumbsViewer::renderHistogram(const QString &filename, bool logarithmic) {
    QImage image(256,160,QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    Histogram histogram;
    int idx = histFiles.indexOf(filename);
    if (idx > -1) {
        histogram = histograms.at(idx);
    } else {
        QImage thumb;
        // try to use thumbnail (they're used when storing the histogram in ::loadThumb as well)
        QString thumbname = locateThumbnail(filename);
        if (!thumbname.isEmpty()) {
            QImageReader reader(thumbname);
            reader.read(&thumb);
            if (QImageReader(filename).size() != QSize(thumb.text("Thumb::Image::Width").toInt(), 
                                                       thumb.text("Thumb::Image::Height").toInt())) {
                reader.setFileName(filename);
                reader.setScaledSize(QSize(256, 256));
                reader.setAutoTransform(false);
                reader.read(&thumb);
                if (thumb.isNull()) {
                    qWarning() << "Invalid file" << filename << reader.errorString();
                    return image;
                }
            }
        }
        qDebug() << "emergency";
        histogram = calcHist(thumb);
    }
    QRgb red = 0xffa06464/* d01717 */, green = 0xff8ca064/* 8cc716 */, blue = 0xff648ca0/* 1793d0 */;
    float factor = 0.0;
    float average = 0.0;
    for (uint16_t i=0; i<256; ++i) {
        if (logarithmic) {
            histogram.red[i] = log(histogram.red[i]);
            histogram.green[i] = log(histogram.green[i]);
            histogram.blue[i] = log(histogram.blue[i]);
        }
        factor = qMax(factor, qMax(histogram.red[i], qMax(histogram.green[i], histogram.blue[i])));
        average += histogram.red[i] + histogram.green[i] + histogram.blue[i];
    }
    if (!logarithmic)
        factor = 160.0f/qMin(factor, average / 128.0f); // cap at 200% of mean value

    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float minValue = 65536.0f;
            float value;
            value = qRound(factor*histogram.red[x]);
            if (value >= 160-y /* && value < minValue */) {  line[x] = red; minValue = value;  }
            value = qRound(factor*histogram.green[x]);
            if (value >= 160-y    && value < minValue   ) {  line[x] = green; minValue = value;  }
            value = qRound(factor*histogram.blue[x]);
            if (value >= 160-y    && value < minValue   ) {  line[x] = blue; /* minValue = value; */  }
        }
    }
    return image;
}
