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
class QListWidget;
class QListWidgetItem;

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
    ImageTags(QWidget *parent);

    QListWidgetItem* addTag(QString tagName, bool tagChecked, TagIcon icon);
    void addTagsFor(const QStringList &files);
    void removeTagsFor(const QStringList &files);
    void populateTagsTree();
    void removeTransientTags();
    void showTagsFilter();

    /// @todo - detangle this
    TagsDisplayMode currentDisplayMode; // phototonic.cpp

public slots:
    void setSelectedFiles(const QStringList &files);
    void showSelectedImagesTags();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void applyUserAction(QList<QListWidgetItem *> tagsList);
    QStringList getCheckedTags(Qt::CheckState tagState);
    void setActiveViewMode(TagsDisplayMode mode);
    void setTagIcon(QListWidgetItem *tagItem, TagIcon icon);
    void updateToolTip(QListWidgetItem *item);
    void sortTags();

    QStringList m_selectedFiles;
    QStringList m_mandatoryFilterTags;
    QStringList m_sufficientFilterTags;
    QStringList m_deathRow;
    QAction *actionAddTag;
    QAction *addToSelectionAction;
    QAction *removeFromSelectionAction;
    QAction *actionClearTagsFilter;
    QAction *negateAction;
    QAction *learnTagAction;
    QAction *removeTagAction;
    QTabBar *tabs;
    QMenu *tagsMenu;
    QListWidget *tagsTree;
    bool m_populated;
    bool m_needToSort;
    QList<size_t> m_trackedFiles;
    QStringList m_selectedTags;

private slots:
    void addNewTag();
    void addTagsToSelection();
    void applyTagFiltering(QListWidgetItem *item = nullptr);
    void clearTagFilters();
    void learnTags();
    void removeTags();
    void removeTagsFromSelection();
    void showMenu(QPoint point);

signals:
    void filterChanged(const QStringList &mandatory, const QStringList &sufficient, bool invert);
    void tagRequest(const QStringList &tagsAdded, const QStringList &tagsRemoved);

};

#endif // TAGS_H

