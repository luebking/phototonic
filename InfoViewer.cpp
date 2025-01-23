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
#include <QLineEdit>
#include <QMenu>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>

#include "InfoViewer.h"

#include <exiv2/exiv2.hpp>

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
    infoViewerLayout->addWidget(infoViewerTable);
    infoViewerLayout->addWidget(filterLineEdit);

    setLayout(infoViewerLayout);
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

void InfoView::read(QString imageFullPath) {
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if EXIV2_TEST_VERSION(0,28,0)
    Exiv2::Image::UniquePtr exifImage;
#else
    Exiv2::Image::AutoPtr exifImage;
#endif
#pragma clang diagnostic pop

    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
    }
    catch (const Exiv2::Error &error) {
        qWarning() << "EXIV2:" << error.what();
        return;
    }

#define EXIV2_ENTRY QString::fromUtf8(md->tagName().c_str()), QString::fromUtf8(md->print().c_str())
    Exiv2::ExifData &exifData = exifImage->exifData();
    if (!exifData.empty()) {
        Exiv2::ExifData::const_iterator end = exifData.end();
        addTitleEntry("Exif");
        for (Exiv2::ExifData::const_iterator md = exifData.begin(); md != end; ++md) {
            addEntry(EXIV2_ENTRY);
        }
    }

    Exiv2::IptcData &iptcData = exifImage->iptcData();
    if (!iptcData.empty()) {
        Exiv2::IptcData::iterator end = iptcData.end();
        addTitleEntry("IPTC");
        for (Exiv2::IptcData::iterator md = iptcData.begin(); md != end; ++md) {
            addEntry(EXIV2_ENTRY);
        }
    }

    Exiv2::XmpData &xmpData = exifImage->xmpData();
    if (!xmpData.empty()) {
        Exiv2::XmpData::iterator end = xmpData.end();
        addTitleEntry("XMP");
        for (Exiv2::XmpData::iterator md = xmpData.begin(); md != end; ++md) {
            addEntry(EXIV2_ENTRY);
        }
    }
    infoViewerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    infoViewerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    filterItems();
}
