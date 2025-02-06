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

#include <QApplication>
#include <QBoxLayout>
#include <QInputDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTabBar>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>

#include "MessageBox.h"
#include "MetadataCache.h"
#include "Settings.h"
#include "Tags.h"

enum { NewTag = Qt::UserRole + 1, InScope };

ImageTags::ImageTags(QWidget *parent) : QWidget(parent) {
    m_populated = false;
    m_needToSort =false;

    tagsTree = new QTreeWidget;
    tagsTree->setColumnCount(2);
    tagsTree->setDragEnabled(false);
    tagsTree->setSortingEnabled(true);
    tagsTree->header()->close();
    tagsTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    tabs = new QTabBar(this);
    tabs->addTab(tr("Selection"));
    tabs->addTab(tr("Filter"));
    tabs->setTabIcon(0, QIcon(":/images/tag_yellow.png"));
    tabs->setTabIcon(1, QIcon(":/images/tag_filter_off.png"));
    tabs->setExpanding(false);
    connect(tabs, &QTabBar::currentChanged, this, [=](int idx) { idx ? showTagsFilter() : showSelectedImagesTags(); });

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 3, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(tabs);
    mainLayout->addWidget(tagsTree);
    setLayout(mainLayout);
    currentDisplayMode = SelectionTagsDisplay;

    connect(tagsTree, &QTreeWidget::itemChanged, this, [=](QTreeWidgetItem *item, int) {
        lastChangedTagItem = item;
    });
    connect(tagsTree, &QTreeWidget::itemClicked, this, [=](QTreeWidgetItem *item, int) {
        if (item != lastChangedTagItem)
            return;
        if (currentDisplayMode == DirectoryTagsDisplay)
            applyTagFiltering();
        else
            applyUserAction(QList<QTreeWidgetItem *>() << item);
        lastChangedTagItem = 0;
    });

    tagsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tagsTree, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showMenu(QPoint)));

    addToSelectionAction = new QAction(tr("Tag"), this);
    addToSelectionAction->setIcon(QIcon(":/images/tag_yellow.png"));
    connect(addToSelectionAction, SIGNAL(triggered()), this, SLOT(addTagsToSelection()));

    removeFromSelectionAction = new QAction(tr("Untag"), this);
    connect(removeFromSelectionAction, SIGNAL(triggered()), this, SLOT(removeTagsFromSelection()));

    actionAddTag = new QAction(tr("New Tag"), this);
    actionAddTag->setIcon(QIcon(":/images/new_tag.png"));
    connect(actionAddTag, SIGNAL(triggered()), this, SLOT(addNewTag()));

    learnTagAction = new QAction(tr("Add to library"), this);
    connect(learnTagAction, SIGNAL(triggered()), this, SLOT(learnTags()));

    removeTagAction = new QAction(tr("Remove from library"), this);
    removeTagAction->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));
    connect(removeTagAction, SIGNAL(triggered()), this, SLOT(removeTags()));

    actionClearTagsFilter = new QAction(tr("Clear Filters"), this);
    actionClearTagsFilter->setIcon(QIcon(":/images/tag_filter_off.png"));
    connect(actionClearTagsFilter, SIGNAL(triggered()), this, SLOT(clearTagFilters()));

    negateAction = new QAction(this);
    negateAction->setCheckable(true);
    connect(negateAction, SIGNAL(triggered()), this, SLOT(applyTagFiltering()));

    tagsMenu = new QMenu("");
    tagsMenu->addAction(addToSelectionAction);
    tagsMenu->addAction(removeFromSelectionAction);
    tagsMenu->addSeparator();
    tagsMenu->addAction(learnTagAction);
    tagsMenu->addAction(actionAddTag);
    tagsMenu->addAction(removeTagAction);
    tagsMenu->addSeparator();
    tagsMenu->addAction(actionClearTagsFilter);
    tagsMenu->addAction(negateAction);
}

void ImageTags::sortTags() {
    if (!isVisible()) {
        m_needToSort = true;
        return; // the below is slow AF for large lists
    }
    static QTimer *bouncer = nullptr;
    if (!bouncer) {
        bouncer = new QTimer(this);
        bouncer->setSingleShot(true);
        bouncer->setInterval(250);
        connect (bouncer, &QTimer::timeout, this, [=]() {
            tagsTree->resizeColumnToContents(0);
            tagsTree->sortItems(0, Qt::AscendingOrder);
            m_needToSort = false;
        });
    }
    bouncer->start();
}

void ImageTags::showMenu(QPoint point) {
    QTreeWidgetItem *item = tagsTree->itemAt(point);
    addToSelectionAction->setEnabled(bool(item));
    removeFromSelectionAction->setEnabled(bool(item));
    removeTagAction->setEnabled(item && !item->data(0, NewTag).toBool());
    learnTagAction->setEnabled(item && item->data(0, NewTag).toBool());
    negateAction->setText((m_mandatoryFilterTags.isEmpty() &&
                           m_sufficientFilterTags.isEmpty()) ? tr("Show untagged") : tr("Invert filter"));
    tagsMenu->popup(tagsTree->viewport()->mapToGlobal(point));
}

void ImageTags::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_needToSort)
        sortTags();
}

void ImageTags::setTagIcon(QTreeWidgetItem *tagItem, TagIcon icon) {
    static QIcon    grey(":/images/tag_grey.png"),
                    yellow(":/images/tag_yellow.png"),
                    multi(":/images/tag_multi.png"),
                    red(":/images/tag_red.png"),
                    on(":/images/tag_filter_on.png"),
                    off(":/images/tag_filter_off.png"),
                    negate(":/images/tag_filter_negate.png");
    switch (icon) {
        case TagIconDisabled:
            tagItem->setIcon(0, tagItem->data(0, NewTag).toBool() ? red : grey);
            break;
        case TagIconEnabled:
            tagItem->setIcon(0, tagItem->data(0, NewTag).toBool() ? red : yellow);
            break;
        case TagIconMultiple:
            tagItem->setIcon(0, multi);
            break;
        case TagIconNew:
            tagItem->setData(0, NewTag, true);
            tagItem->setIcon(0, red);
            break;
        case TagIconFilterEnabled:
            tagItem->setIcon(0, on);
            break;
        case TagIconFilterDisabled:
            tagItem->setIcon(0, off);
            break;
        case TagIconFilterNegate:
            tagItem->setIcon(0, negate);
            break;
    }
}

QTreeWidgetItem* ImageTags::addTag(QString tagName, bool tagChecked, TagIcon icon) {
    QTreeWidgetItem *tagItem = new QTreeWidgetItem();
    tagItem->setText(0, tagName);
    tagItem->setCheckState(0, tagChecked ? Qt::Checked : Qt::Unchecked);
    if (currentDisplayMode == DirectoryTagsDisplay) {
        setTagIcon(tagItem, tagChecked ? TagIconFilterEnabled : TagIconFilterDisabled);
        tagItem->setFlags(tagItem->flags() | Qt::ItemIsUserCheckable|Qt::ItemIsUserTristate);
    } else {
        setTagIcon(tagItem, icon);
    }
    tagsTree->addTopLevelItem(tagItem);
    return tagItem;
}

void ImageTags::addTagsFor(const QStringList &files) {
    bool dunnit = false;
    for (const QString &file : files) {
        QSet<QString> tags = Metadata::tags(file);
        for (auto tag = tags.cbegin(), end = tags.cend(); tag != end; ++tag) {
            QList <QTreeWidgetItem*> present = tagsTree->findItems(*tag, Qt::MatchExactly);
            if (present.isEmpty()) {
                QTreeWidgetItem *item = addTag(*tag, false, TagIconNew);
                item->setData(0, NewTag, true);
                item->setData(0, InScope, true);
                dunnit = true;
            } else {
                for (QTreeWidgetItem *item : present)
                    item->setData(0, InScope, true);
            }
        }
    }
    if (dunnit)
        sortTags();
}

void ImageTags::setSelectedFiles(const QStringList &files) {
    m_selectedFiles = files;
    if (isVisible() && currentDisplayMode == SelectionTagsDisplay)
        showSelectedImagesTags();
}

void ImageTags::showSelectedImagesTags() {
    static bool busy = false;
    if (busy) {
        qDebug() << "meek: showSelectedImagesTags recursion";
        return;
    }
    busy = true;
    QStringList selectedThumbs = m_selectedFiles;

    setActiveViewMode(SelectionTagsDisplay);

    int selectedThumbsNum = selectedThumbs.size();
    QMap<QString, int> tagsCount;
    for (int i = 0; i < selectedThumbsNum; ++i) {
        QSetIterator<QString> imageTagsIter(Metadata::tags(selectedThumbs.at(i)));
        while (imageTagsIter.hasNext()) {
            QString imageTag = imageTagsIter.next();
            tagsCount[imageTag]++;

            if (tagsTree->findItems(imageTag, Qt::MatchExactly).isEmpty()) {
                qDebug() << "meek: why is there an unknown tag during the selection?";
                addTag(imageTag, true, TagIconNew);
            }
        }
    }

    bool imagesTagged = false, imagesTaggedMixed = false;
    QTreeWidgetItemIterator it(tagsTree);
    bool needToResize = false;
    while (*it) {
        if ((*it)->isHidden())
            needToResize = true;
        (*it)->setHidden(false);
        QString tagName = (*it)->text(0);
        int tagCountTotal = tagsCount.value(tagName, 0);

        bool newTag = (*it)->data(0, NewTag).toBool();
        if (selectedThumbsNum == 0) {
            (*it)->setCheckState(0, Qt::Unchecked);
            (*it)->setFlags((*it)->flags() & ~(Qt::ItemIsUserCheckable|Qt::ItemIsUserTristate));
            setTagIcon(*it, newTag ? TagIconNew : TagIconDisabled);
        } else if (tagCountTotal == selectedThumbsNum) {
            (*it)->setCheckState(0, Qt::Checked);
            (*it)->setFlags(((*it)->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsUserTristate);
            setTagIcon(*it, newTag ? TagIconNew : TagIconEnabled);
            imagesTagged = true;
        } else if (tagCountTotal) {
            (*it)->setCheckState(0, Qt::PartiallyChecked);
            (*it)->setFlags(((*it)->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsUserTristate);
            setTagIcon(*it, TagIconMultiple);
            imagesTaggedMixed = true;
        } else {
            (*it)->setCheckState(0, Qt::Unchecked);
            (*it)->setFlags(((*it)->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsUserTristate);
            setTagIcon(*it, newTag ? TagIconNew : TagIconDisabled);
        }
        ++it;
    }

    if (imagesTagged) {
        tabs->setTabIcon(0, QIcon(":/images/tag_yellow.png"));
    } else if (imagesTaggedMixed) {
        tabs->setTabIcon(0, QIcon(":/images/tag_multi.png"));
    } else {
        tabs->setTabIcon(0, QIcon(":/images/tag_grey.png"));
    }

    addToSelectionAction->setEnabled(selectedThumbsNum ? true : false);
    removeFromSelectionAction->setEnabled(selectedThumbsNum ? true : false);

    if (needToResize)
        tagsTree->resizeColumnToContents(0);
//    sortTags(); // see above meek
    busy = false;
}

void ImageTags::showTagsFilter() {
    static bool busy = false;
    if (busy) {
        qDebug() << "showTagsFilter recursion";
        return;
    }
    busy = true;

    setActiveViewMode(DirectoryTagsDisplay);

    QTreeWidgetItemIterator it(tagsTree);
    while (*it) {
        if (!(*it)->data(0, InScope).toBool()) {
            (*it)->setHidden(true);
            ++it;
            continue;
        }
        QString tagName = (*it)->text(0);

        (*it)->setFlags((*it)->flags() | Qt::ItemIsUserCheckable|Qt::ItemIsUserTristate);
        if (m_mandatoryFilterTags.contains(tagName)) {
            (*it)->setCheckState(0, Qt::Checked);
            setTagIcon(*it, negateAction->isChecked() ? TagIconFilterNegate : TagIconFilterEnabled);
        } else if (m_sufficientFilterTags.contains(tagName)) {
            (*it)->setCheckState(0, Qt::PartiallyChecked);
            setTagIcon(*it, negateAction->isChecked() ? TagIconFilterNegate : TagIconFilterEnabled);
        } else {
            (*it)->setCheckState(0, Qt::Unchecked);
            setTagIcon(*it, TagIconFilterDisabled);
        }
        ++it;
    }

    sortTags();
    busy = false;
}

void ImageTags::populateTagsTree() {
    if (m_populated)
        return;

    // technically unnecessary now
    tagsTree->clear();

    // tagsTree->sortItems() on many unsorted items is slow AF, so we set the order ahead
    tagsTree->sortItems(0, Qt::AscendingOrder);
    // and pre-sort a list to speed that up *A LOT*
    QList<QString> list(Settings::knownTags.cbegin(), Settings::knownTags.cend());
    std::sort(list.begin(), list.end());
    for (const QString &tag : list)
        addTag(tag, false, TagIconDisabled);

    if (currentDisplayMode == SelectionTagsDisplay) {
        showSelectedImagesTags();
    } else {
        showTagsFilter();
    }

    sortTags();
    m_populated = true;
}

void ImageTags::setActiveViewMode(TagsDisplayMode mode) {
    currentDisplayMode = mode;
    actionAddTag->setVisible(currentDisplayMode == SelectionTagsDisplay);
    removeTagAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
    learnTagAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
    addToSelectionAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
    removeFromSelectionAction->setVisible(currentDisplayMode == SelectionTagsDisplay);
    actionClearTagsFilter->setVisible(currentDisplayMode == DirectoryTagsDisplay);
    negateAction->setVisible(currentDisplayMode == DirectoryTagsDisplay);
}

QStringList ImageTags::getCheckedTags(Qt::CheckState tagState) {
    QStringList checkedTags;
    QTreeWidgetItemIterator it(tagsTree);

    while (*it) {
        if ((*it)->checkState(0) == tagState) {
            checkedTags << (*it)->text(0);
        }
        ++it;
    }

    return checkedTags;
}

void ImageTags::applyTagFiltering() {
    m_mandatoryFilterTags = getCheckedTags(Qt::Checked);
    m_sufficientFilterTags = getCheckedTags(Qt::PartiallyChecked);

    if (m_mandatoryFilterTags.isEmpty() && m_sufficientFilterTags.isEmpty())
        tabs->setTabIcon(1, QIcon(":/images/tag_filter_off.png"));
    else if (negateAction->isChecked())
        tabs->setTabIcon(1, QIcon(":/images/tag_filter_negate.png"));
    else
        tabs->setTabIcon(1, QIcon(":/images/tag_filter_on.png"));

    emit filterChanged(m_mandatoryFilterTags, m_sufficientFilterTags, negateAction->isChecked());
}

void ImageTags::applyUserAction(QList<QTreeWidgetItem *> tagsList) {

    QStringList tagsAdded, tagsRemoved;
    for (int i = tagsList.size() - 1; i > -1; --i) {
        QTreeWidgetItem *item = tagsList.at(i);

        Qt::CheckState tagState = item->checkState(0);
        TagIcon icon = TagIconDisabled;
        if (item->data(0, NewTag).toBool())
            icon = TagIconNew;
        else if (tagState == Qt::Checked)
            icon = TagIconEnabled;
        setTagIcon(item, icon);

        (tagState == Qt::Checked ? tagsAdded : tagsRemoved) << item->text(0);
    }
    emit tagRequest(tagsAdded, tagsRemoved);
}

void ImageTags::removeTagsFromSelection() {
    for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {
        tagsTree->selectedItems().at(i)->setCheckState(0, Qt::Unchecked);
    }

    applyUserAction(tagsTree->selectedItems());
}

void ImageTags::addTagsToSelection() {
    for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {
        tagsTree->selectedItems().at(i)->setCheckState(0, Qt::Checked);
    }

    applyUserAction(tagsTree->selectedItems());
}

void ImageTags::clearTagFilters() {
    QTreeWidgetItemIterator it(tagsTree);
    while (*it) {
        (*it)->setCheckState(0, Qt::Unchecked);
        ++it;
    }
    applyTagFiltering();
}

void ImageTags::addNewTag() {
    bool ok;
    QString title = tr("Add a new tag");
    QString newTagName = QInputDialog::getText(this, title, tr("Enter new tag name"),
                                               QLineEdit::Normal, "", &ok);
    if (!ok) {
        return;
    }

    if (newTagName.isEmpty()) {
        MessageBox msgBox(this);
        msgBox.critical(tr("Error"), tr("No name entered"));
        return;
    }

    QSetIterator<QString> knownTagsIt(Settings::knownTags);
    while (knownTagsIt.hasNext()) {
        QString tag = knownTagsIt.next();
        if (newTagName == tag) {
            MessageBox msgBox(this);
            msgBox.critical(tr("Error"), tr("Tag %1 already exists").arg(newTagName));
            return;
        }
    }

    addTag(newTagName, false, TagIconDisabled);
    Settings::knownTags.insert(newTagName);
    sortTags();
}

void ImageTags::removeTags() {
    if (!tagsTree->selectedItems().size()) {
        return;
    }

    MessageBox msgBox(this);
    msgBox.setText(tr("Delete %n selected tags(s)?", "", tagsTree->selectedItems().size()));
    msgBox.setWindowTitle(tr("Delete tag"));
    msgBox.setIcon(MessageBox::Warning);
    msgBox.setStandardButtons(MessageBox::Yes | MessageBox::Cancel);
    msgBox.setDefaultButton(MessageBox::Cancel);

    if (msgBox.exec() != MessageBox::Yes) {
        return;
    }

    bool removedTagWasChecked = false;
    for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {

        QTreeWidgetItem *item = tagsTree->selectedItems().at(i);
        QString tagName = item->text(0);
        Settings::knownTags.remove(tagName);

        if (m_mandatoryFilterTags.removeOne(tagName) | m_sufficientFilterTags.removeOne(tagName))
            removedTagWasChecked = true;
        if (item->data(0, InScope).toBool())
            setTagIcon(item, TagIconNew);
        else
            tagsTree->takeTopLevelItem(tagsTree->indexOfTopLevelItem(item));
    }

    if (removedTagWasChecked) {
        applyTagFiltering();
    }
}

void ImageTags::removeTransientTags() {
    for (int i = tagsTree->topLevelItemCount() - 1; i > -1; --i) {

        QTreeWidgetItem *item = tagsTree->topLevelItem(i);
        if (!item->data(0, NewTag).toBool() ||
            m_mandatoryFilterTags.contains(item->text(0)) ||
            m_sufficientFilterTags.contains(item->text(0))) {
            item->setData(0, InScope, false);
            continue;
        }
        delete tagsTree->takeTopLevelItem(i);
    }
}

void ImageTags::learnTags() {
    if (!tagsTree->selectedItems().size()) {
        return;
    }

    for (int i = 0; i < tagsTree->selectedItems().size(); ++i) {

        QTreeWidgetItem *item = tagsTree->selectedItems().at(i);
        QString tagName = item->text(0);
        item->setData(0, NewTag, false);
        if (item->checkState(0) ==  Qt::Unchecked)
            setTagIcon(item, TagIconDisabled);
        else if (item->checkState(0) ==  Qt::Checked)
            setTagIcon(item, TagIconEnabled);
        // tristate is the multi-icon, we ignore that
        Settings::knownTags.insert(tagName);
    }
}
