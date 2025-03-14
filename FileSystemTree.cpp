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

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileSystemModel>
#include <QMimeData>
#include "FileSystemTree.h"

FileSystemTree::FileSystemTree(QWidget *parent) : QTreeView(parent) {
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);

    setHeaderHidden(true);

    connect(this, SIGNAL(expanded(const QModelIndex &)), this, SLOT(resizeTreeColumn(const QModelIndex &)));
    connect(this, SIGNAL(collapsed(const QModelIndex &)), this, SLOT(resizeTreeColumn(const QModelIndex &)));
}

QModelIndex FileSystemTree::getCurrentIndex() {
    return selectedIndexes().first();
}

void FileSystemTree::setModel(QAbstractItemModel *m) {
    if (model())
        disconnect(model());
    QTreeView::setModel(m);
    connect(m, &QFileSystemModel::layoutChanged, this, [this]() { scrollTo(currentIndex()); }, Qt::QueuedConnection);
}

void FileSystemTree::resizeTreeColumn(const QModelIndex &index) {
    resizeColumnToContents(0);
    scrollTo(index);
}

void FileSystemTree::dragEnterEvent(QDragEnterEvent *event) {
    QModelIndexList selectedDirs = selectionModel()->selectedRows();
    if (selectedDirs.size() > 0) {
        dndOrigSelection = selectedDirs[0];
        event->acceptProposedAction();
    }
}

void FileSystemTree::dragMoveEvent(QDragMoveEvent *event) {
    setCurrentIndex(indexAt(event->position().toPoint()));
}

void FileSystemTree::dropEvent(QDropEvent *event) {
    if (event->source()) {
        QString fileSystemTreeStr = "FileSystemTree";
        bool dirOp = (event->source()->metaObject()->className() == fileSystemTreeStr);
        emit dropOp(event->modifiers(), dirOp, event->mimeData()->urls().at(0).toLocalFile());
        setCurrentIndex(dndOrigSelection);
    }
}

