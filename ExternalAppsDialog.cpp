/*
 *  Copyright (C) 2013-2018 Ofer Kashayov <oferkv@live.com>
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

#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>

#include "ExternalAppsDialog.h"
#include "Settings.h"

ExternalAppsDialog::ExternalAppsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Manage External Applications"));
    setWindowIcon(QIcon::fromTheme("preferences-other", QIcon(":/images/phototonic.png")));
    resize(512, 256);

    appsTable = new QTableView(this);
    appsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    appsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    appsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    appsTableModel = new QStandardItemModel(this);
    appsTable->setModel(appsTableModel);
    appsTable->verticalHeader()->setVisible(false);
    appsTable->verticalHeader()->setDefaultSectionSize(appsTable->verticalHeader()->minimumSectionSize());
    appsTableModel->setHorizontalHeaderItem(0, new QStandardItem(QString(tr("Name"))));
    appsTableModel->setHorizontalHeaderItem(1,
                                            new QStandardItem(QString(tr("Application path and arguments"))));
    appsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    appsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    appsTable->setShowGrid(false);

    QVBoxLayout *externalAppsLayout = new QVBoxLayout;
    //: open an existing executable
    QPushButton *addButton = new QPushButton(tr("Pick Executable..."));
    connect(addButton, SIGNAL(clicked()), this, SLOT(add()));
    externalAppsLayout->addWidget(addButton, 0, Qt::AlignTop);
    QPushButton *entryButton = new QPushButton(tr("Add manually"));
    connect(entryButton, SIGNAL(clicked()), this, SLOT(entry()));
    externalAppsLayout->addWidget(entryButton, 0, Qt::AlignTop);
    QPushButton *removeButton = new QPushButton(tr("Delete"));
    connect(removeButton, SIGNAL(clicked()), this, SLOT(remove()));
    externalAppsLayout->addWidget(removeButton, 0, Qt::AlignTop);
    externalAppsLayout->addStretch(1);

    QHBoxLayout *wrapperLayout = new QHBoxLayout;
    wrapperLayout->addWidget(appsTable);
    wrapperLayout->addLayout(externalAppsLayout);

    QHBoxLayout *wallpaperLayout = new QHBoxLayout;
    wallpaperLayout->addWidget(new QLabel(tr("Wallpaper command")));
    wallpaperLayout->addWidget(m_wallpaperCommand = new QComboBox);

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    QPushButton *okButton = new QPushButton(tr("OK"));
    okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(okButton, SIGNAL(clicked()), this, SLOT(ok()));
    buttonsLayout->addWidget(okButton, 0, Qt::AlignRight);

    QVBoxLayout *externalAppsMainLayout = new QVBoxLayout;
    externalAppsMainLayout->addWidget(new QLabel(
            tr("· %f and %tf will be substituted with the single image and thumbnail paths\n"
               "· %u and %tu with their respective URIs (file:///path/to/file)\n"
               "· %lat %lon and %alt with the GPS location in signed decimals (\"altitude\"…)\n"
               "· by default the selected images are appended to the command"), this));
    externalAppsMainLayout->addLayout(wrapperLayout);
    externalAppsMainLayout->addLayout(wallpaperLayout);
    externalAppsMainLayout->addLayout(buttonsLayout);
    setLayout(externalAppsMainLayout);

    // Load external apps list
    m_wallpaperCommand->setEditable(true);
    QStringList wpcomm;
    wpcomm  << "feh --bg-fill"
            << "pcmanfm-qt --set-wallpaper"
            << "pcmanfm --set-wallpaper"
            << "gsettings set org.gnome.desktop.background picture-uri \'%u\'"
            << "gsettings set org.gnome.desktop.background picture-uri-dark \'%u\'";
    if (Settings::wallpaperCommand.isEmpty() && !wpcomm.contains(Settings::wallpaperCommand))
        m_wallpaperCommand->addItem(Settings::wallpaperCommand);
    m_wallpaperCommand->addItems(wpcomm);
    m_wallpaperCommand->setCurrentText(Settings::wallpaperCommand);

    QString key, val;
    QMapIterator<QString, QString> it(Settings::externalApps);
    while (it.hasNext()) {
        it.next();
        key = it.key();
        val = it.value();
        addTableModelItem(appsTableModel, key, val);
    }
}

void ExternalAppsDialog::ok() {
    Settings::wallpaperCommand = m_wallpaperCommand->currentText();
    int row = appsTableModel->rowCount();
    Settings::externalApps.clear();
    for (int i = 0; i < row; ++i) {
        if (!appsTableModel->itemFromIndex(appsTableModel->index(i, 1))->text().isEmpty()) {
            Settings::externalApps[appsTableModel->itemFromIndex(appsTableModel->index(i, 0))->text()] =
                    appsTableModel->itemFromIndex(appsTableModel->index(i, 1))->text();
        }
    }
    accept();
}

void ExternalAppsDialog::add() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose Application"), "", "");
    if (fileName.isEmpty())
        return;

    QFileInfo fileInfo = QFileInfo(fileName);
    QString appName = fileInfo.fileName();
    addTableModelItem(appsTableModel, appName, fileName);
}

void ExternalAppsDialog::entry() {
    int atRow = appsTableModel->rowCount();
    QStandardItem *itemKey = new QStandardItem(QString(tr("New Application")));
    appsTableModel->insertRow(atRow, itemKey);
}

void ExternalAppsDialog::remove() {
    QModelIndexList indexesList;
    while ((indexesList = appsTable->selectionModel()->selectedIndexes()).size()) {
        appsTableModel->removeRow(indexesList.first().row());
    }
}

void ExternalAppsDialog::addTableModelItem(QStandardItemModel *model, QString &key, QString &val) {
    int atRow = model->rowCount();
    QStandardItem *itemKey = new QStandardItem(key);
    QStandardItem *itemKey2 = new QStandardItem(val);
    model->insertRow(atRow, itemKey);
    model->setItem(atRow, 1, itemKey2);
}
