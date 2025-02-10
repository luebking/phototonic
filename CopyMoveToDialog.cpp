/*
 *  Copyright (C) 2013-2014 Ofer Kashayov - oferkv@live.com
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
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>

#include "CopyMoveToDialog.h"
#include "Settings.h"

void CopyMoveToDialog::savePaths() {
    Settings::bookmarkPaths.clear();
    for (int i = 0; i < pathsTableModel->rowCount(); ++i) {
        if (QStandardItem *bookmark = pathsTableModel->item(i))
            Settings::bookmarkPaths.insert(bookmark->text());
    }
}


void CopyMoveToDialog::add() {
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Choose Directory"), currentPath,
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirName.isEmpty()) {
        return;
    }

    QStandardItem *item = new QStandardItem(QIcon(":/images/bookmarks.png"), dirName);
    pathsTableModel->insertRow(pathsTableModel->rowCount(), item);

    pathsTable->selectionModel()->clearSelection();
    pathsTable->selectionModel()->select(pathsTableModel->index(pathsTableModel->rowCount() - 1, 0),
                                         QItemSelectionModel::Select);
}

void CopyMoveToDialog::closeEvent(QCloseEvent *event) {
    savePaths();
    QDialog::closeEvent(event);
}

CopyMoveToDialog::CopyMoveToDialog(QWidget *parent, QString thumbsPath, bool copyOp) : QDialog(parent) {
    if (copyOp) {
        setWindowTitle(tr("Copy to..."));
        setWindowIcon(QIcon::fromTheme("edit-copy"));
    } else {
        setWindowTitle(tr("Move to..."));
        setWindowIcon(QIcon::fromTheme("go-next"));
    }

    const int w = QFontMetrics(font()).averageCharWidth()*60;
    resize(w, 10*w/16);
    currentPath = thumbsPath;

    pathsTable = new QTableView(this);
    pathsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pathsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pathsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    pathsTableModel = new QStandardItemModel(this);
    pathsTable->setModel(pathsTableModel);
    pathsTable->verticalHeader()->setVisible(false);
    pathsTable->horizontalHeader()->setVisible(false);
    pathsTable->verticalHeader()->setDefaultSectionSize(pathsTable->verticalHeader()->minimumSectionSize());
    pathsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pathsTable->setShowGrid(false);

    connect(pathsTable->selectionModel(), &QItemSelectionModel::selectionChanged, [=](){
        m_destination = QString();
        QModelIndexList indexesList;
        QStandardItem *item;
        if ((indexesList = pathsTable->selectionModel()->selectedIndexes()).size() &&
            (item = pathsTableModel->itemFromIndex(indexesList.first())))
                m_destination = item->text();
        destinationLabel->setText(m_destination);
    });

    connect(pathsTable, &QTableView::doubleClicked, this, [=](){ m_destination.isEmpty() ? reject() : accept(); });

    QVBoxLayout *addRemoveBox = new QVBoxLayout;

    QPushButton *addButton = new QPushButton(tr("Browse..."));
    connect(addButton, SIGNAL(clicked()), this, SLOT(add()));
    addRemoveBox->addWidget(addButton, 0);

    QPushButton *removeButton = new QPushButton(tr("Delete Bookmark"));
    connect(removeButton, &QPushButton::clicked, this, [=]() {
        QModelIndexList indexesList;
        if ((indexesList = pathsTable->selectionModel()->selectedIndexes()).size())
            pathsTableModel->removeRow(indexesList.first().row());
    });
    addRemoveBox->addWidget(removeButton, 0);

    addRemoveBox->addStretch(1);

    QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, &QPushButton::clicked, this, [=](){ reject(); });

    QPushButton *okButton = new QPushButton(copyOp ? tr("Copy") : tr("Move"));
    okButton->setDefault(true);

    connect(okButton, &QPushButton::clicked, this, [=](){ m_destination.isEmpty() ? reject() : accept(); });

    buttonsHbox->addStretch(1);
    buttonsHbox->addWidget(cancelButton, 0);
    buttonsHbox->addWidget(okButton, 0);

    destinationLabel = new QLabel(this);
    destinationLabel->setAlignment(Qt::AlignCenter);
    QFont fnt = destinationLabel->font();
    if (fnt.pointSize() > 0)
        fnt.setPointSize(8*fnt.pointSize()/5);
    else
        fnt.setPixelSize(8*fnt.pixelSize()/5);
    destinationLabel->setFont(fnt);

    QHBoxLayout *pathsBox = new QHBoxLayout;
    pathsBox->addWidget(pathsTable);
    pathsBox->addLayout(addRemoveBox);
    QVBoxLayout *mainVbox = new QVBoxLayout;
    mainVbox->addLayout(pathsBox);
    mainVbox->addWidget(new QLabel(tr("Destination:")));
    mainVbox->addWidget(destinationLabel);
    mainVbox->addLayout(buttonsHbox);
    setLayout(mainVbox);

    // Load paths list
    QSetIterator<QString> it(Settings::bookmarkPaths);
    while (it.hasNext()) {
        QStandardItem *item = new QStandardItem(QIcon(":/images/bookmarks.png"), it.next());
        pathsTableModel->insertRow(pathsTableModel->rowCount(), item);
    }
    pathsTableModel->sort(0);
}
