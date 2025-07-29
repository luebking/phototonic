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
#include <QComboBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>

#include "MetadataCache.h"
#include "InfoViewer.h"
#include "Settings.h"

InfoView::InfoView(QWidget *parent) : QWidget(parent) {

    infoViewerTable = new QTableView(this);
    infoViewerTable->setSelectionMode(QAbstractItemView::SingleSelection/* ExtendedSelection makes no sense wrt the copy feature*/);
    infoViewerTable->setSelectionBehavior(QAbstractItemView::SelectRows/* SelectItems dto*/);
    infoViewerTable->verticalHeader()->setVisible(false);
    infoViewerTable->verticalHeader()->setDefaultSectionSize(infoViewerTable->verticalHeader()->minimumSectionSize());
    infoViewerTable->horizontalHeader()->setVisible(false);
    infoViewerTable->setEditTriggers(QAbstractItemView::DoubleClicked /* QAbstractItemView::NoEditTriggers */);
    infoViewerTable->setTabKeyNavigation(false);
    infoViewerTable->setShowGrid(false);

    imageInfoModel = new QStandardItemModel(this);
    infoViewerTable->setModel(imageInfoModel);

    // Menu
    QAction *copyAction = new QAction(tr("Copy"), this);
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copyEntry()));
    m_removeAction = new QAction(tr("Remove"), this);
    connect(m_removeAction, SIGNAL(triggered()), this, SLOT(removeEntry()));
    infoMenu = new QMenu("");
    infoMenu->addAction(copyAction);
    infoMenu->addAction(m_removeAction);
    infoViewerTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(infoViewerTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showInfoViewMenu(QPoint)));

    // Filter items
    m_filter = new QComboBox(this);
    m_filter->setEditable(true);
    m_filter->lineEdit()->setPlaceholderText(tr("Filter Items"));
    m_filter->addItems(Settings::exifFilters.keys());
    m_filter->setEditText(QString()); // start w/ empty filter

    m_manageFiltersButton = new QPushButton("+", this);
    connect(m_manageFiltersButton, &QPushButton::clicked, [=]() {
        if (Settings::exifFilters.remove(m_filter->currentText())) {
            m_filter->removeItem(m_filter->currentIndex());
            m_filter->setEditText(QString());
        } else {
            bool ok;
            QString n = QInputDialog::getText(this, tr("Enter filter name"),
                                                tr("Enter a name (without leading \"$\") for this filter.\nThe special name\"preview\" is used for the filter in the viewer."),
                                                QLineEdit::Normal, QString(), &ok);
            if (ok && !n.isEmpty()) {
                Settings::exifFilters.insert("$"+n, m_filter->currentText());
                m_filter->addItem("$"+n);
                m_filter->setEditText("$"+n);
            } else {
                return;
            }
        }
    });
    connect(m_filter, SIGNAL(currentTextChanged(const QString&)), this, SLOT(filterItems()));

    m_saveExifButton = new QPushButton(tr("Save EXIF changes"), this);
    connect(m_saveExifButton, SIGNAL(clicked()), this, SLOT(saveExifChanges()));

    QVBoxLayout *infoViewerLayout = new QVBoxLayout(this);
    QHBoxLayout *histLayout = new QHBoxLayout;
    histLayout->addStretch();
    histLayout->addWidget(m_histogram = new QLabel(this));
    m_histogram->setToolTip(tr("Left click: toggle logarithmic scale\nRight click: toggle size\nHide with filter: |nohistogram|"));
    histLayout->addStretch();
    infoViewerLayout->addLayout(histLayout);
    infoViewerLayout->addWidget(infoViewerTable);
    infoViewerLayout->addWidget(m_saveExifButton);
    QHBoxLayout *filterLayout = new QHBoxLayout;
    filterLayout->addWidget(m_filter, 100);
    filterLayout->addWidget(m_manageFiltersButton);
    infoViewerLayout->addLayout(filterLayout);

    setLayout(infoViewerLayout);
    m_histogram->installEventFilter(this);
    infoViewerTable->installEventFilter(this);
}

bool InfoView::eventFilter(QObject *o, QEvent *e) {
    if (o == m_histogram && e->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton) {
            emit histogramClicked();
        } else if (me->button() == Qt::RightButton) {
            m_histogram->setScaledContents(!m_histogram->hasScaledContents());
            if (m_histogram->hasScaledContents()) {
                m_histogram->setFixedSize(32,20);
            } else {
                m_histogram->setFixedSize(m_histogram->pixmap().size());
            }
        }
    } else if (o == infoViewerTable && e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Delete) {
        const QModelIndexList &selection = infoViewerTable->selectionModel()->selectedIndexes();
        if (!selection.isEmpty()) {
            selectedEntry = selection.first();
            removeEntry();
        }
    }
    return QWidget::eventFilter(o, e);
}

void InfoView::showInfoViewMenu(QPoint pt) {
    selectedEntry = infoViewerTable->indexAt(pt);
    if (selectedEntry.column() == 0)
        selectedEntry = selectedEntry.siblingAtColumn(1);
    if (selectedEntry.isValid() && infoViewerTable->columnSpan(selectedEntry.row(), 0) == 1) {
        QStandardItem *item = imageInfoModel->itemFromIndex(selectedEntry);
        m_removeAction->setVisible(item && item->isEditable());
        infoMenu->popup(infoViewerTable->viewport()->mapToGlobal(pt));
    }
    selectedEntry = QModelIndex();
}

void InfoView::clear() {
    m_currentFile.clear();
    imageInfoModel->clear();
}

void ExifItem::setData(const QVariant &value, int role) {
    if (role == Qt::EditRole)
        QStandardItem::setData(value, Qt::UserRole + 1);
    else
        QStandardItem::setData(value, role);
}

QVariant ExifItem::data(int role) const {
    if (role == Qt::EditRole)
        return QStandardItem::data(Qt::UserRole + 1);
    else
        return QStandardItem::data(role);
}

void InfoView::addEntry(QString key, QString value, bool editable, QString editValue) {
    int atRow = imageInfoModel->rowCount();
    QStandardItem *itemKey = new QStandardItem(key);
    itemKey->setEditable(false);
    imageInfoModel->insertRow(atRow, itemKey);
    if (!value.isEmpty()) {
        ExifItem *itemVal = new ExifItem(value);
        itemVal->setToolTip(value);
        itemVal->setEditable(editable);
        itemVal->setData(editValue, Qt::EditRole);
        imageInfoModel->setItem(atRow, 1, itemVal);
    }
}

void InfoView::addTitleEntry(QString title) {
    int atRow = imageInfoModel->rowCount();
    QStandardItem *itemKey = new QStandardItem(title);
    itemKey->setEditable(false);
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

void InfoView::removeEntry() {
    if (selectedEntry.isValid()) {
        imageInfoModel->removeRows(selectedEntry.row(), 1);
        m_saveExifButton->show();
        selectedEntry = QModelIndex();
    }
}

void InfoView::showSaveButton(QStandardItem *changedItem) {
    if (changedItem) {
        imageInfoModel->blockSignals(true);
        changedItem->setData(changedItem->data(Qt::EditRole), Qt::DisplayRole);
        imageInfoModel->blockSignals(false);
    }
    m_saveExifButton->show();
}

void InfoView::saveExifChanges() {
    Metadata::DataPair EXIF, IPTC, XMP;
    Metadata::DataPair *data = nullptr;
    for (int i = 0; i < imageInfoModel->rowCount(); ++i) {
        if (infoViewerTable->columnSpan(i, 0) > 1) { // title
            if (imageInfoModel->item(i)->text() == "Exif")
                data = &EXIF;
            else if (imageInfoModel->item(i)->text() == "IPTC")
                data = &IPTC;
            else if (imageInfoModel->item(i)->text() == "XMP")
                data = &XMP;
            continue; // title
        }
        if (!imageInfoModel->item(i, 1) || imageInfoModel->item(i, 1)->text().isEmpty())
            continue; // empty field
        if (!imageInfoModel->item(i, 1)->isEditable())
            continue; // static data, brightness, file stats - not metadata
        if (!data) {
            qDebug() << "WAHHHHHH! (headless metadata!)" << imageInfoModel->item(i)->text();
            continue;
        }
        data->insert(imageInfoModel->item(i)->text(), imageInfoModel->item(i, 1)->text());
    }
    Metadata::setData(m_currentFile, EXIF, IPTC, XMP);
    m_saveExifButton->hide();
    reloadExifData();
    emit exifChanged(m_currentFile);
}

void InfoView::reloadExifData() {
    const QString currentFile = m_currentFile;
    m_currentFile = QString();
    const QPixmap histogram = m_histogram->pixmap();
    read(currentFile, QImage());
    m_histogram->setPixmap(histogram);
}

void InfoView::filterItems() {
    QString filter = Settings::exifFilters.value(m_filter->currentText());
    if (filter.isEmpty()) {
        m_manageFiltersButton->setText("+");
        filter = m_filter->currentText();
    } else {
        m_manageFiltersButton->setText("-");
    }

    const bool nohistogram = filter.contains("|nohistogram|");
    if (nohistogram)
        filter.remove("|nohistogram|");
    m_histogram->setVisible(!nohistogram);

    QRegularExpression re(filter, QRegularExpression::CaseInsensitiveOption);
    if (!re.isValid()) {
        m_manageFiltersButton->setEnabled(false);
        QPalette pal = m_filter->palette();
        pal.setColor(QPalette::Text, 0xd01717); // force my idea of Qt::red on everyone
        m_filter->setPalette(pal);
        return;
    }
    const bool hot = !filter.isEmpty();
    m_manageFiltersButton->setEnabled(hot);
    m_filter->setPalette(QPalette());
    int lastHeader = -1;
    bool allHidden = true;
    for (int i = 0; i < imageInfoModel->rowCount(); ++i) {
        if (infoViewerTable->columnSpan(i, 0) > 1) { // title
            infoViewerTable->setRowHidden(i, false);
            if (allHidden && lastHeader > -1)
                infoViewerTable->setRowHidden(lastHeader, true);
            lastHeader = i;
            allHidden = true;
            infoViewerTable->setRowHidden(i, false);
            continue;
        }
        const bool hidden = hot && !imageInfoModel->item(i)->text().contains(re);
        infoViewerTable->setRowHidden(i, hidden);
        allHidden = allHidden && hidden;
    }
    if (allHidden && lastHeader > -1)
        infoViewerTable->setRowHidden(lastHeader, true);
}

void InfoView::hint(QString key, QString value) {
    m_hints[key] = value;
}

QString InfoView::html() const {
    QRegularExpression re;
    bool showFileStats = false;
    QString filter = Settings::exifFilters.value("$preview");
    if (!filter.isEmpty()) {
        re = QRegularExpression(filter, QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) {
            qWarning() << "invalid custom preview filter" << filter;
            filter = QString();
        }
    }
    if (filter.isEmpty()) {
        showFileStats = true;
        filter = "^(make|model|isospeedratings|fnumber|exposuretime|focallength|datetime|lensmodel|isospeedratings|flash|exposureprogram|artist|copyright|software|keywords)$";
        re = QRegularExpression(filter, QRegularExpression::CaseInsensitiveOption);
    }

    QString text = "<html><table>";
    QString lastHeader, lastTag;
    for (int i = 0; i < imageInfoModel->rowCount(); ++i) {
        if (infoViewerTable->columnSpan(i, 0) > 1) {
            showFileStats = showFileStats && !i; // file stats header is on 0
            lastHeader = imageInfoModel->item(i)->text();
            continue;
        }
        QString tag = imageInfoModel->item(i)->text();
        if ((showFileStats || tag.contains(re)) && imageInfoModel->item(i, 1)) {
            if (!lastHeader.isNull()) {
                text += "<tr><th>" + lastHeader + "</th></tr>";
                lastHeader = QString();
            }
            static const QRegularExpression newline("[\r\n]+");
            QString value = imageInfoModel->item(i, 1)->text();
            value.replace(newline, "<br>");
            if (lastTag == tag)
                tag = QString();
            else
                lastTag = tag;
            text += "<tr><td>" + tag + "</td><td style='word-wrap: break-word; max-width: 48em;'>" + value + "</td></tr>";
        }
    }
    text += "</table></html>";
    return text;
}

void InfoView::read(QString imageFullPath, const QImage &histogram) {
    m_histogram->setPixmap(QPixmap::fromImage(histogram));

    if (m_currentFile == imageFullPath)
        return;

    m_saveExifButton->hide();
    disconnect(imageInfoModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(showSaveButton(QStandardItem*)));
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

    Metadata::DataTriple EXIF, IPTC, XMP;
    Metadata::data(imageFullPath, &EXIF, &IPTC, &XMP);

    if (!EXIF.isEmpty()) {
        addTitleEntry("Exif");
        for (auto i = EXIF.cbegin(), end = EXIF.cend(); i != end; ++i)
            addEntry(i.key(), i.value().first, true, i.value().second);
    }
    if (!IPTC.isEmpty()) {
        addTitleEntry("IPTC");
        for (auto i = IPTC.cbegin(), end = IPTC.cend(); i != end; ++i)
            addEntry(i.key(), i.value().first, true, i.value().second);
    }
    if (!XMP.isEmpty()) {
        addTitleEntry("XMP");
        for (auto i = XMP.cbegin(), end = XMP.cend(); i != end; ++i)
            addEntry(i.key(), i.value().first, true, i.value().second);
    }

    infoViewerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    infoViewerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    filterItems();
    connect(imageInfoModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(showSaveButton(QStandardItem*)));
}
