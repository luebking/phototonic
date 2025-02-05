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

#ifndef TAGS_H
#define TAGS_H

class QTabBar;
class ThumbsViewer;
class QTreeWidget;
class QTreeWidgetItem;

#include <QWidget>


enum TagsDisplayMode {
    DirectoryTagsDisplay,
    SelectionTagsDisplay
};

enum TagIcon {
    TagIconDisabled,
    TagIconEnabled,
    TagIconMultiple,
    TagIconNew,
    TagIconFilterDisabled,
    TagIconFilterEnabled,
    TagIconFilterNegate
};

class ImageTags : public QWidget {
Q_OBJECT

public:
    ImageTags(QWidget *parent, ThumbsViewer *thumbsViewer);

    void addTag(QString tagName, bool tagChecked, TagIcon icon);
    void addTagsFor(const QStringList &files);
    bool isImageFilteredOut(QString imagePath);
    void populateTagsTree();
    void removeTransientTags();
    void showTagsFilter();

    /// @todo - detangle this
    QTreeWidget *tagsTree; // phototonic.cpp
    bool dirFilteringActive; // thumbsview.cpp
    TagsDisplayMode currentDisplayMode; // thumbsview.cpp

public slots:
    void showSelectedImagesTags();

protected:
    void showEvent(QShowEvent *event) override;

private:
    bool writeTagsToImage(QString &imageFileName, const QSet<QString> &tags);

    QSet<QString> getCheckedTags(Qt::CheckState tagState);

    void setTagIcon(QTreeWidgetItem *tagItem, TagIcon icon);

    void setActiveViewMode(TagsDisplayMode mode);

    void applyUserAction(QTreeWidgetItem *item);

    void applyUserAction(QList<QTreeWidgetItem *> tagsList);

    void sortTags();

    QSet<QString> imageFilteringTags;
    QAction *actionAddTag;
    QAction *addToSelectionAction;
    QAction *removeFromSelectionAction;
    QAction *actionClearTagsFilter;
    QAction *negateAction;
    QAction *learnTagAction;
    QAction *removeTagAction;
    QTreeWidgetItem *lastChangedTagItem;
    ThumbsViewer *thumbView;
    QTabBar *tabs;
    bool negateFilterEnabled;
    QMenu *tagsMenu;
    bool m_populated;
    bool m_needToSort;

private slots:

    void addNewTag();
    void addTagsToSelection();
    void applyTagFiltering();
    void clearTagFilters();
    void learnTags();
    void negateFilter();
    void removeTags();
    void removeTagsFromSelection();
    void saveLastChangedTag(QTreeWidgetItem *item, int column);
    void showMenu(QPoint point);
    void tagClicked(QTreeWidgetItem *item, int column);

signals:
    void reloadThumbs();

};

#endif // TAGS_H

