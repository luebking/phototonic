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
#include <QBoxLayout>
#include <QClipboard>
#include <QFileInfo>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>

#include "MetadataCache.h"
#include "InfoViewer.h"

InfoView::InfoView(QWidget *parent) : QWidget(parent) {

    infoViewerTable = new QTableView(this);
    infoViewerTable->setSelectionMode(QAbstractItemView::SingleSelection/* ExtendedSelection makes no sense wrt the copy feature*/);
    infoViewerTable->setSelectionBehavior(QAbstractItemView::SelectRows/* SelectItems dto*/);
    infoViewerTable->verticalHeader()->setVisible(false);
    infoViewerTable->verticalHeader()->setDefaultSectionSize(infoViewerTable->verticalHeader()->minimumSectionSize());
    infoViewerTable->horizontalHeader()->setVisible(false);
    infoViewerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    infoViewerTable->setTabKeyNavigation(false);
    infoViewerTable->setShowGrid(false);

    imageInfoModel = new QStandardItemModel(this);
    infoViewerTable->setModel(imageInfoModel);

    // Menu
    QAction *copyAction = new QAction(tr("Copy"), this);
    infoViewerTable->connect(copyAction, SIGNAL(triggered()), this, SLOT(copyEntry()));
    infoMenu = new QMenu("");
    infoMenu->addAction(copyAction);
    infoViewerTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(infoViewerTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showInfoViewMenu(QPoint)));

    // Filter items
    filterLineEdit = new QLineEdit(this);
    connect(filterLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(filterItems()));
    filterLineEdit->setClearButtonEnabled(true);
    filterLineEdit->setPlaceholderText(tr("Filter Items"));

    QVBoxLayout *infoViewerLayout = new QVBoxLayout(this);
    QHBoxLayout *histLayout = new QHBoxLayout;
    histLayout->addStretch();
    histLayout->addWidget(m_histogram = new QLabel(this));
    histLayout->addStretch();
    infoViewerLayout->addLayout(histLayout);
    infoViewerLayout->addWidget(infoViewerTable);
    infoViewerLayout->addWidget(filterLineEdit);

    setLayout(infoViewerLayout);
    m_histogram->installEventFilter(this);
}

bool InfoView::eventFilter(QObject *o, QEvent *e) {
    if (o == m_histogram && e->type() == QEvent::MouseButtonPress)
        emit histogramClicked();
    return QWidget::eventFilter(o, e);
}

void InfoView::showInfoViewMenu(QPoint pt) {
    selectedEntry = infoViewerTable->indexAt(pt);
    if (selectedEntry.column() == 0)
        selectedEntry = selectedEntry.siblingAtColumn(1);
    if (selectedEntry.isValid() && infoViewerTable->columnSpan(selectedEntry.row(), 0) == 1)
        infoMenu->popup(infoViewerTable->viewport()->mapToGlobal(pt));
}

void InfoView::clear() {
    m_currentFile.clear();
    imageInfoModel->clear();
}

void InfoView::addEntry(QString key, QString value) {
    int atRow = imageInfoModel->rowCount();
    QStandardItem *itemKey = new QStandardItem(key);
    imageInfoModel->insertRow(atRow, itemKey);
    if (!value.isEmpty()) {
        QStandardItem *itemVal = new QStandardItem(value);
        itemVal->setToolTip(value);
        imageInfoModel->setItem(atRow, 1, itemVal);
    }
}

void InfoView::addTitleEntry(QString title) {
    int atRow = imageInfoModel->rowCount();
    QStandardItem *itemKey = new QStandardItem(title);
    imageInfoModel->insertRow(atRow, itemKey);

    QFont boldFont;
    boldFont.setBold(true);
    itemKey->setData(boldFont, Qt::FontRole);
    infoViewerTable->setSpan(atRow, 0, 1, 2);
}

void InfoView::copyEntry() {
    if (selectedEntry.isValid()) {
        QApplication::clipboard()->setText(imageInfoModel->itemFromIndex(selectedEntry)->toolTip());
    }
}

void InfoView::filterItems() {
    const QString filter = filterLineEdit->text().toLower();
    for (int i = 0; i < imageInfoModel->rowCount(); ++i) {
        if (infoViewerTable->columnSpan(i, 0) > 1) { // title
            continue;
        }
        infoViewerTable->setRowHidden(i, !filter.isEmpty() && !imageInfoModel->item(i)->text().toLower().contains(filter));
    }
}

void InfoView::hint(QString key, QString value) {
    m_hints[key] = value;
}

QString InfoView::html() const {
    QString text = "<html><table>";
    for (int i = 0; i < imageInfoModel->rowCount(); ++i) {
        text += "<tr>";
        if (infoViewerTable->columnSpan(i, 0) > 1)
            text += "<th>" + imageInfoModel->item(i)->text() + "</th>";
        else if (imageInfoModel->item(i, 1) &&  // empty field
                imageInfoModel->item(i, 1)->text().length() < 65) { // some fields contain binaries, useless for human perception
            text += "<td>" + imageInfoModel->item(i)->text() + "</td><td>" + imageInfoModel->item(i, 1)->text() + "</td>";
        }
        text += "</tr>";
    }
    text += "</table></html>";
    return text;
}

void InfoView::read(QString imageFullPath, const QImage &histogram) {
    m_histogram->setPixmap(QPixmap::fromImage(histogram));

    if (m_currentFile == imageFullPath)
        return;

    clear();
    QFileInfo imageInfo = QFileInfo(imageFullPath);
    if (!imageInfo.exists())
        return;

    m_currentFile = imageFullPath;
    QString key;
    QString val;

    addTitleEntry(tr("Image"));
    addEntry(tr("File name"), imageInfo.fileName());
    addEntry(tr("Location"), imageInfo.path());
    addEntry(key = tr("Size"), QString::number(imageInfo.size() / 1024.0, 'f', 2) + "K");
    addEntry(tr("Modified"), imageInfo.lastModified().toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat)));

    QImageReader imageInfoReader(imageFullPath);
    if (imageInfoReader.size().isValid()) {
        addEntry(tr("Format"), imageInfoReader.format().toUpper());

        const QSize sz = imageInfoReader.size();
        addEntry(tr("Resolution"), QString::number(sz.width()) + "x" + QString::number(sz.height()));
        addEntry(tr("Megapixel"), QString::number((sz.width() * sz.height()) / 1000000.0, 'f', 2));

        for (auto i = m_hints.cbegin(), end = m_hints.cend(); i != end; ++i)
            addEntry(i.key(), i.value());
    } else {
        imageInfoReader.read();
        key = tr("Error");
        val = imageInfoReader.errorString();
        addEntry(key, val);
    }

    QMap<QString, QString> EXIF, IPTC, XMP;
    Metadata::data(imageFullPath, &EXIF, &IPTC, &XMP);

    if (!EXIF.isEmpty()) {
        addTitleEntry("Exif");
        for (auto i = EXIF.cbegin(), end = EXIF.cend(); i != end; ++i)
            addEntry(i.key(), i.value());
    }
    if (!IPTC.isEmpty()) {
        addTitleEntry("IPTC");
        for (auto i = IPTC.cbegin(), end = IPTC.cend(); i != end; ++i)
            addEntry(i.key(), i.value());
    }
    if (!XMP.isEmpty()) {
        addTitleEntry("XMP");
        for (auto i = XMP.cbegin(), end = XMP.cend(); i != end; ++i)
            addEntry(i.key(), i.value());
    }

    infoViewerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    infoViewerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    filterItems();
}
