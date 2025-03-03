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
#include <QToolButton>
#include <QListWidget>
#include <QListWidgetItem>

#include "MessageBox.h"
#include "MetadataCache.h"
#include "Settings.h"
#include "Tags.h"

#define BLOCK_RECURSION QSignalBlocker blocker(tagsTree);

enum { NewTag = Qt::UserRole + 1, InScope };

ImageTags::ImageTags(QWidget *parent) : QWidget(parent) {

    m_populated = false;
    m_needToSort =false;

    tagsTree = new QListWidget;
    tagsTree->setDragEnabled(false);
    tagsTree->setSortingEnabled(true);
    tabs = new QTabBar(this);
    tabs->addTab(tr("Selection"));
    tabs->addTab(tr("Filter"));
    tabs->setTabIcon(0, QIcon(":/images/tag_yellow.png"));
    tabs->setTabIcon(1, QIcon(":/images/tag_filter_off.png"));
    tabs->setExpanding(false);
    connect(tabs, &QTabBar::currentChanged, this, [=](int idx) { idx ? showTagsFilter() : showSelectedImagesTags(); });

    QToolButton *btn = new QToolButton;
    btn->setToolTip(tr("Show only library tags"));
    m_tagGroup = 3;
    btn->setIcon(QIcon(":/images/tag_multi.png"));
    connect (btn, &QToolButton::clicked, this, [=]() {
        if (m_tagGroup == 3) {
            m_tagGroup = 1;
            btn->setIcon(QIcon(":/images/tag_grey.png"));
            btn->setToolTip(tr("Show only relevant tags"));
        } else if (m_tagGroup == 1) {
            m_tagGroup = 2;
            btn->setIcon(QIcon(":/images/tag_red.png"));
            btn->setToolTip(tr("Show all tags"));
        } else {
            m_tagGroup = 3;
            btn->setIcon(QIcon(":/images/tag_multi.png"));
            btn->setToolTip(tr("Show only library tags"));
        }
        if (tabs->currentIndex() == 0)
            showSelectedImagesTags();
    });
    tabs->setTabButton(0, QTabBar::RightSide, btn);

    btn = new QToolButton;
    btn->setToolTip(tr("Clear Filters"));
    btn->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/images/delete.png")));
    connect (btn, &QToolButton::clicked, this, &ImageTags::clearTagFilters);
    tabs->setTabButton(1, QTabBar::RightSide, btn);
    btn->setEnabled(false);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 3, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(tabs);
    mainLayout->addWidget(tagsTree);
    setLayout(mainLayout);
    currentDisplayMode = SelectionTagsDisplay;

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
    connect(negateAction, &QAction::triggered, this, [=]() {applyTagFiltering();});

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

    QObject::connect(tagsTree, &QListWidget::itemChanged, this, [=](QListWidgetItem *item) {
        BLOCK_RECURSION
        if (currentDisplayMode == DirectoryTagsDisplay)
            applyTagFiltering(item);
        else
            applyUserAction(QList<QListWidgetItem *>() << item);
    });
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
            BLOCK_RECURSION
            tagsTree->sortItems(Qt::AscendingOrder);
            m_needToSort = false;
        });
    }
    bouncer->start();
}

void ImageTags::showMenu(QPoint point) {
    QListWidgetItem *item = tagsTree->itemAt(point);
    addToSelectionAction->setEnabled(item && item->checkState() == Qt::Unchecked);
    removeFromSelectionAction->setEnabled(item && item->checkState() == Qt::Checked);
    removeTagAction->setEnabled(item && !item->data(NewTag).toBool());
    learnTagAction->setEnabled(item && item->data(NewTag).toBool());
    bool noSelection = m_mandatoryFilterTags.isEmpty() && m_sufficientFilterTags.isEmpty();
    negateAction->setText(noSelection ? tr("Show untagged") : tr("Invert filter"));
    actionClearTagsFilter->setEnabled(!noSelection);
    tagsMenu->popup(tagsTree->viewport()->mapToGlobal(point));
}

void ImageTags::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_needToSort)
        sortTags();
}

void ImageTags::setTagIcon(QListWidgetItem *tagItem, TagIcon icon) {
    BLOCK_RECURSION
    static QIcon    grey(":/images/tag_grey.png"),
                    yellow(":/images/tag_yellow.png"),
                    multi(":/images/tag_multi.png"),
                    red(":/images/tag_red.png"),
                    on(":/images/tag_filter_on.png"),
                    off(":/images/tag_filter_off.png"),
                    negate(":/images/tag_filter_negate.png");
    switch (icon) {
        case TagIconDisabled:
            tagItem->setIcon(tagItem->data(NewTag).toBool() ? red : grey);
            break;
        case TagIconEnabled:
            tagItem->setIcon(tagItem->data(NewTag).toBool() ? red : yellow);
            break;
        case TagIconMultiple:
            tagItem->setIcon(multi);
            break;
        case TagIconNew:
            tagItem->setData(NewTag, true);
            tagItem->setIcon(red);
            break;
        case TagIconFilterEnabled:
            tagItem->setIcon(on);
            break;
        case TagIconFilterDisabled:
            tagItem->setIcon(off);
            break;
        case TagIconFilterNegate:
            tagItem->setIcon(negate);
            break;
    }
}

QListWidgetItem* ImageTags::addTag(QString tagName, bool tagChecked, TagIcon icon) {
    BLOCK_RECURSION
    QListWidgetItem *tagItem = new QListWidgetItem();
    tagItem->setText(tagName);
    tagItem->setCheckState(tagChecked ? Qt::Checked : Qt::Unchecked);
    if (currentDisplayMode == DirectoryTagsDisplay) {
        setTagIcon(tagItem, tagChecked ? TagIconFilterEnabled : TagIconFilterDisabled);
        tagItem->setFlags(tagItem->flags() | Qt::ItemIsUserCheckable|Qt::ItemIsUserTristate);
//        setToolTip(tagItem);
    } else {
        setTagIcon(tagItem, icon);
    }
    tagsTree->addItem(tagItem);
    return tagItem;
}

void ImageTags::addTagsFor(const QStringList &files) {
    BLOCK_RECURSION
    bool dunnit = false;
    for (const QString &file : files) {
        size_t hash = qHash(file);
        if (m_trackedFiles.contains(hash))
            continue;
        m_trackedFiles << hash;
        QSet<QString> tags = Metadata::tags(file);
        for (auto tag = tags.cbegin(), end = tags.cend(); tag != end; ++tag) {
            QList <QListWidgetItem*> present = tagsTree->findItems(*tag, Qt::MatchExactly);
            if (present.isEmpty()) {
                QListWidgetItem *item = addTag(*tag, false, TagIconNew);
                item->setData(NewTag, true);
                item->setData(InScope, 1);
                dunnit = true;
            } else {
                for (QListWidgetItem *item : present) {
                    item->setData(InScope, item->data(InScope).toInt() + 1);
                }
            }
        }
    }
    if (dunnit)
        sortTags();
}

void ImageTags::removeTagsFor(const QStringList &files) {
    BLOCK_RECURSION
    for (const QString &file : files) {
        m_trackedFiles.removeOne(qHash(file));
        QSet<QString> tags = Metadata::tags(file);
        for (auto tag = tags.cbegin(), end = tags.cend(); tag != end; ++tag) {
            QList <QListWidgetItem*> present = tagsTree->findItems(*tag, Qt::MatchExactly);
            if (present.isEmpty())
                continue;
            for (QListWidgetItem *item : present) {
                if (!item->data(NewTag).toBool())
                    continue;
                const int scope = item->data(InScope).toInt() - 1;
                if (scope <= 0)
                    delete tagsTree->takeItem(tagsTree->row(item));
                else
                    item->setData(InScope, scope);
            }
        }
    }
}

void ImageTags::setSelectedFiles(const QStringList &files) {
    m_selectedFiles = files;
    if (isVisible() && currentDisplayMode == SelectionTagsDisplay)
        showSelectedImagesTags();
}

void ImageTags::showSelectedImagesTags() {
    BLOCK_RECURSION
    QStringList selectedThumbs = m_selectedFiles;

    setActiveViewMode(SelectionTagsDisplay);
    tagsTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (!m_deathRow.isEmpty()) {
        for (int i = tagsTree->count() - 1; i > -1; --i) {
            QListWidgetItem *item = tagsTree->item(i);
            if (m_deathRow.contains(item->text()))
                delete tagsTree->takeItem(tagsTree->row(item));
        }
        m_deathRow.clear();
    }

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
    for (int i = 0; i < tagsTree->count(); ++i) {
        QListWidgetItem *item = tagsTree->item(i);
        item->setHidden(!(m_tagGroup & (item->data(NewTag).toBool() ? 2 : 1)));
        QString tagName = item->text();
        item->setSelected(m_selectedTags.contains(tagName));
        int tagCountTotal = tagsCount.value(tagName, 0);

        bool newTag = item->data(NewTag).toBool();
        if (selectedThumbsNum == 0) {
            item->setCheckState(Qt::Unchecked);
            item->setFlags(item->flags() & ~(Qt::ItemIsUserCheckable|Qt::ItemIsUserTristate));
            setTagIcon(item, newTag ? TagIconNew : TagIconDisabled);
        } else if (tagCountTotal == selectedThumbsNum) {
            item->setCheckState(Qt::Checked);
            item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsUserTristate);
            setTagIcon(item, newTag ? TagIconNew : TagIconEnabled);
            imagesTagged = true;
        } else if (tagCountTotal) {
            item->setCheckState(Qt::PartiallyChecked);
            item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsUserTristate);
            setTagIcon(item, TagIconMultiple);
            imagesTaggedMixed = true;
        } else {
            item->setCheckState(Qt::Unchecked);
            item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsUserTristate);
            setTagIcon(item, newTag ? TagIconNew : TagIconDisabled);
        }
        updateToolTip(item);
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

//    sortTags(); // see above meek
}

void ImageTags::updateToolTip(QListWidgetItem *item) {
    BLOCK_RECURSION
    static const QString mandatory = tr("Mandatory:");
    static const QString sufficient = tr("Sufficient:");
    static const QString mustnot = tr("The image must not have this tag");
    static const QString must = tr("The image must have this tag");
    static const QString mustany = tr("The image must have any of these tags");

    if (currentDisplayMode == SelectionTagsDisplay || item->checkState() == Qt::Unchecked)
        item->setToolTip(QString());
    else if (item->checkState() == Qt::PartiallyChecked)
        item->setToolTip(sufficient + "\n" + (negateAction->isChecked() ? mustnot : mustany));
    else if (item->checkState() == Qt::Checked)
        item->setToolTip(mandatory + "\n" + (negateAction->isChecked() ? mustnot : must));
}


void ImageTags::showTagsFilter() {
    BLOCK_RECURSION

    setActiveViewMode(DirectoryTagsDisplay);
    m_selectedTags.clear();
    for (int i = 0; i < tagsTree->count(); ++i) {
        QListWidgetItem *item = tagsTree->item(i);
        QString tagName = item->text();
        if (item->isSelected()) {
            m_selectedTags << tagName;
            item->setSelected(false);
        }
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable|Qt::ItemIsUserTristate);
        if (m_mandatoryFilterTags.contains(tagName)) {
            item->setCheckState(Qt::Checked);
            setTagIcon(item, negateAction->isChecked() ? TagIconFilterNegate : TagIconFilterEnabled);
            updateToolTip(item);
        } else if (m_sufficientFilterTags.contains(tagName)) {
            item->setCheckState(Qt::PartiallyChecked);
            setTagIcon(item, negateAction->isChecked() ? TagIconFilterNegate : TagIconFilterEnabled);
            updateToolTip(item);
        } else if (!item->data(InScope).toBool()) {
            item->setHidden(true);
        } else {
            item->setCheckState(Qt::Unchecked);
            setTagIcon(item, TagIconFilterDisabled);
            updateToolTip(item);
        }
    }
    tagsTree->setSelectionMode(QAbstractItemView::SingleSelection);

    sortTags();
}

void ImageTags::populateTagsTree() {
    if (m_populated)
        return;

    BLOCK_RECURSION
    // technically unnecessary now
    tagsTree->clear();

    // tagsTree->sortItems() on many unsorted items is slow AF, so we set the order ahead
    tagsTree->sortItems(Qt::AscendingOrder);
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
    BLOCK_RECURSION
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
    for (int i = 0; i < tagsTree->count(); ++i) {
        QListWidgetItem *item = tagsTree->item(i);
        if (item->checkState() == tagState) {
            checkedTags << item->text();
        }
    }

    return checkedTags;
}

void ImageTags::applyTagFiltering(QListWidgetItem *item) {
    BLOCK_RECURSION
    TagIcon icon = negateAction->isChecked() ? TagIconFilterNegate : TagIconFilterEnabled;
    if (item) {
        if (item->checkState() == Qt::Unchecked &&
                item->data(NewTag).toBool() &&
                item->data(InScope).toInt() < 1) {
            delete tagsTree->takeItem(tagsTree->row(item));
        } else {
            updateToolTip(item);
            setTagIcon(item, item->checkState() == Qt::Unchecked ? TagIconFilterDisabled : icon);
        }
    } else { // inversion
        for (int i = 0; i < tagsTree->count(); ++i) {
            QListWidgetItem *item = tagsTree->item(i);
            updateToolTip(item);
            if (item->checkState() != Qt::Unchecked)
                setTagIcon(item, icon);
        }
    }
    m_mandatoryFilterTags = getCheckedTags(Qt::Checked);
    m_sufficientFilterTags = getCheckedTags(Qt::PartiallyChecked);

    tabs->tabButton(1, QTabBar::RightSide)->setEnabled(true);
    if (m_mandatoryFilterTags.isEmpty() && m_sufficientFilterTags.isEmpty()) {
        tabs->tabButton(1, QTabBar::RightSide)->setEnabled(false);
        tabs->setTabIcon(1, QIcon(":/images/tag_filter_off.png"));
    }
    else if (negateAction->isChecked())
        tabs->setTabIcon(1, QIcon(":/images/tag_filter_negate.png"));
    else
        tabs->setTabIcon(1, QIcon(":/images/tag_filter_on.png"));

    emit filterChanged(m_mandatoryFilterTags, m_sufficientFilterTags, negateAction->isChecked());
}

void ImageTags::applyUserAction(QList<QListWidgetItem *> tagsList) {
    BLOCK_RECURSION
    QStringList tagsAdded, tagsRemoved;
    m_deathRow.clear();
    for (int i = tagsList.size() - 1; i > -1; --i) {
        QListWidgetItem *item = tagsList.at(i);

        Qt::CheckState tagState = item->checkState();
        TagIcon icon = TagIconDisabled;
        if (item->data(NewTag).toBool())
            icon = TagIconNew;
        else if (tagState == Qt::Checked)
            icon = TagIconEnabled;
        setTagIcon(item, icon);

        int scope = item->data(InScope).toInt();
        if (tagState == Qt::Checked) {
            tagsAdded << item->text();
            ++scope;
        } else {
            tagsRemoved << item->text();
            --scope;
        }

        if (scope < 1 && item->data(NewTag).toBool() && // out of scope former transient tag
            !m_mandatoryFilterTags.contains(item->text()) && // not used by either …
            !m_sufficientFilterTags.contains(item->text())) { // … filter
            m_deathRow << item->text();
            item->setData(InScope, 0);
        } else {
            item->setData(InScope, qMax(0, scope));
        }
    }
    emit tagRequest(tagsAdded, tagsRemoved);
}

void ImageTags::removeTagsFromSelection() {
    BLOCK_RECURSION
    for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {
        tagsTree->selectedItems().at(i)->setCheckState(Qt::Unchecked);
    }

    applyUserAction(tagsTree->selectedItems());
}

void ImageTags::addTagsToSelection() {
    BLOCK_RECURSION
    for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {
        tagsTree->selectedItems().at(i)->setCheckState(Qt::Checked);
    }

    applyUserAction(tagsTree->selectedItems());
}

void ImageTags::clearTagFilters() {
    BLOCK_RECURSION
    const bool juggle = currentDisplayMode != DirectoryTagsDisplay;

    if (juggle) {
        setUpdatesEnabled(false);
        showTagsFilter();
    }

    for (int i = 0; i < tagsTree->count(); ++i)
        tagsTree->item(i)->setCheckState(Qt::Unchecked);

    applyTagFiltering();

    if (juggle) {
        showSelectedImagesTags();
        setUpdatesEnabled(true);
    }
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

    BLOCK_RECURSION
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

    BLOCK_RECURSION
    bool removedTagWasChecked = false;
    for (int i = tagsTree->selectedItems().size() - 1; i > -1; --i) {

        QListWidgetItem *item = tagsTree->selectedItems().at(i);
        QString tagName = item->text();
        Settings::knownTags.remove(tagName);

        if (m_mandatoryFilterTags.removeOne(tagName) | m_sufficientFilterTags.removeOne(tagName))
            removedTagWasChecked = true;
        if (item->data(InScope).toBool())
            setTagIcon(item, TagIconNew);
        else
            delete tagsTree->takeItem(tagsTree->row(item));
    }

    if (removedTagWasChecked) {
        applyTagFiltering();
    }
}

void ImageTags::removeTransientTags() {
    BLOCK_RECURSION
    m_trackedFiles.clear();
    for (int i = tagsTree->count() - 1; i > -1; --i) {

        QListWidgetItem *item = tagsTree->item(i);
        if (!item->data(NewTag).toBool() ||
            m_mandatoryFilterTags.contains(item->text()) ||
            m_sufficientFilterTags.contains(item->text())) {
            item->setData(InScope, 0);
            continue;
        }
        delete tagsTree->takeItem(i);
    }
}

void ImageTags::learnTags() {
    if (!tagsTree->selectedItems().size()) {
        return;
    }

    BLOCK_RECURSION
    for (int i = 0; i < tagsTree->selectedItems().size(); ++i) {

        QListWidgetItem *item = tagsTree->selectedItems().at(i);
        QString tagName = item->text();
        item->setData(NewTag, false);
        if (item->checkState() ==  Qt::Unchecked)
            setTagIcon(item, TagIconDisabled);
        else if (item->checkState() ==  Qt::Checked)
            setTagIcon(item, TagIconEnabled);
        // tristate is the multi-icon, we ignore that
        Settings::knownTags.insert(tagName);
    }
}
