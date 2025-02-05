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

#ifndef THUMBS_VIEWER_H
#define THUMBS_VIEWER_H

class QStandardItem;
class QStandardItemModel;

#include <QBitArray>
#include <QDir>
#include <QFileInfoList>
#include <QListView>
#include <QTimer>

struct Histogram
{
    float red[256]{};
    float green[256]{};
    float blue[256]{};

    inline float compareChannel(const float hist1[256], const float hist2[256]) const
    {
        float len1 = 0.f, len2 = 0.f, corr = 0.f;

        for (uint16_t i=0; i<256; i++) {
            len1 += hist1[i];
            len2 += hist2[i];
            corr += std::sqrt(hist1[i] * hist2[i]);
        }

        const float part1 = 1.f / std::sqrt(len1 * len2);

        return std::sqrt(1.f - part1 * corr);
    }

    inline float compare(const Histogram &other) const
    {
        return compareChannel(red, other.red) +
            compareChannel(green, other.green) +
            compareChannel(blue, other.blue);
    }
};
Q_DECLARE_METATYPE(Histogram);

struct Constraint
{
    qint64 younger = 0;
    qint64 older = 0;
    qint64 bigger = 0;
    qint64 smaller = 0;
    QSize minRes;
    QSize maxRes;
    qint64 minPix = 0;
    qint64 maxPix = 0;
};

class ThumbsViewer : public QListView {
Q_OBJECT

public:
    enum UserRoles {
        FileNameRole = Qt::UserRole + 1,
        SortRole,
        LoadedRole,
        BrightnessRole,
        TypeRole,
        SizeRole,
        TimeRole,
        HistogramRole,
        DateTimeOriginal
    };
    enum ThumbnailLayouts {
        Classic,
        Squares,
        Compact
    };

    ThumbsViewer(QWidget *parent);

    void loadPrepare();

    void applyFilter();

    void reLoad();
    void loadDuplicates();

    void loadFileList();

    void loadSubDirectories();

    void setThumbColors();

    using QListView::setCurrentIndex;
    bool setCurrentIndex(const QString &fileName);
    bool setCurrentIndex(int row);

    void setNeedToScroll(bool needToScroll);
    void setResizeEnabled(bool resize) { m_resize = resize; }

    void setTagFilters(const QStringList &mandatory, const QStringList &sufficient, bool invert) {
        m_mandatoryFilterTags = mandatory;
        m_sufficientFilterTags = sufficient;
        m_invertTagFilter = invert;
    }

    void selectCurrentIndex();

    QStandardItem *addThumb(const QString &imageFullPath);

    void abort(bool permanent = false);

    void selectByBrightness(qreal min, qreal max);

    int getNextRow();

    int getPrevRow();

    QStringList selectedFiles() const;

    QString getSingleSelectionFilename();
    QString fullPathOf(int idx);
    QIcon icon(int idx);

    int dynamicGridWidth();
    void refreshThumbs();
    bool setFilter(const QString &filter, QString *error = nullptr);
    void scanForSort(UserRoles role);
    int firstVisibleThumb();
    int lastVisibleThumb();
    QImage renderHistogram(const QString &imagePath, bool logarithmic = false);
    QString locateThumbnail(const QString &path) const;
    bool isBusy() { return m_busy; }
    QDir thumbsDir;
    QDir::SortFlags thumbsSortFlags;
    int thumbSize;

signals:
    void currentIndexChanged(const QModelIndex &current);
    void filesAdded(const QStringList &files);
    void progress(unsigned int current, unsigned int total);
    void status(QString s);
    void selectionChanged(int count);

protected:
    void startDrag(Qt::DropActions) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool isConstrained(const QFileInfo &fileInfo) const;
    bool matchesTagFilter(const QString &path) const;
    void initThumbs();

    bool loadThumb(int row, bool fastOnly = false);

    void findDupes(bool resetCounters);

    void updateThumbsCount();

    void updateImageInfoViewer(int row);

    QSize itemSizeHint() const;

    QString thumbnailFileName(const QString &path) const;
    void storeThumbnail(const QString &originalPath, QImage thumbnail, const QSize &originalSize) const;

    QFileInfoList thumbFileInfoList;
    QList<Histogram> histograms;
    QList<QString> histFiles;
    bool m_histSorted;
    QPixmap emptyImg;

    bool isAbortThumbsLoading = false;
    bool isClosing = false;
    bool isNeedToScroll = false;
    bool scrolledForward = false;
    int thumbsRangeFirst;
    int thumbsRangeLast;

    QTimer m_selectionChangedTimer;
    QTimer m_loadThumbTimer;
    QString m_filter;
    QList<Constraint> m_constraints;
    QStringList m_mandatoryFilterTags;
    QStringList m_sufficientFilterTags;
    bool m_invertTagFilter;
    bool m_busy;
    bool m_resize;
    QStandardItemModel *m_model;
    QString m_desiredThumbPath;

public slots:
    void invertSelection();
    void loadVisibleThumbs(int scrollBarValue = 0);
    void promoteSelectionChange();
    void tagSelected(const QStringList &tagsAdded, const QStringList &tagsRemoved) const;

protected slots:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private slots:
    void loadThumbsRange();
};

#endif // THUMBS_VIEWER_H

