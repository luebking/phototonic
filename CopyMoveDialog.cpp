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
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "CopyMoveDialog.h"
#include "MessageBox.h"
#include "MetadataCache.h"
#include "Settings.h"
#include "ThumbsViewer.h"

static QString autoRename(const QString &destDir, const QString &currFile) {
    int extSep = currFile.lastIndexOf(".");
    QString nameOnly = currFile.left(extSep);
    QString extOnly = currFile.right(currFile.size() - extSep - 1);
    QString newFile;

    int idx = 1;
    do {
        newFile = QString(nameOnly + "_copy_%1." + extOnly).arg(idx);
        ++idx;
    } while (idx && (QFile::exists(destDir + QDir::separator() + newFile)));

    return newFile;
}

int CopyOrMove::file(const QString &srcPath, QString &dstPath, bool copy) {
    int res = 0;

    if (copy) {
        res = QFile::copy(srcPath, dstPath);
    } else {
        res = QFile::rename(srcPath, dstPath);
    }

    if (!res && QFile::exists(dstPath)) {
        QFileInfo info(dstPath);
        QString newName = autoRename(info.absolutePath(), info.fileName());
        QString newDestPath = info.absolutePath() + QDir::separator() + newName;

        if (copy) {
            res = QFile::copy(srcPath, newDestPath);
        } else {
            res = QFile::rename(srcPath, newDestPath);
        }
        dstPath = newDestPath;
    }

    if (res && !copy) {
        ThumbsViewer::moveCache(srcPath, dstPath);
        Metadata::rename(srcPath, dstPath);
    }

    return res;
}

static QPixmap loadPreview(QString path, QSize &realSize) {
    QImageReader reader;
    reader.setQuality(50);
    reader.setFileName(path);
    realSize = reader.size();
    QSize thumbSize = realSize;
    thumbSize.scale(QSize(256,256), Qt::KeepAspectRatio);
    reader.setScaledSize(thumbSize);
    reader.setAutoTransform(false);
    return QPixmap::fromImageReader(&reader);
}

struct ImagePrint {
    QPixmap pix;
    QString stats;
};

class EnterGuard : public QObject {
    public:
        EnterGuard(QWidget *guarded) : QObject(guarded), watch(false), m_guarded(guarded) {}
        bool watch;
    protected:
        bool eventFilter(QObject *o, QEvent *e) {
            if (o == m_guarded && (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)) {
                int key = static_cast<QKeyEvent*>(e)->key();
                if (key == Qt::Key_Enter || key == Qt::Key_Return) {
                    e->accept();
                    return watch;
                }
            }
            return false;
        }
    private:
        QWidget *m_guarded;
};

QDialog::DialogCode CopyOrMove::resolveConflicts(QMap<QString,QString> &collisions, QWidget *parent) {
    QDialog *dlg = new QDialog(parent);
    QLabel *srcPreview = new QLabel, *dstPreview = new QLabel, *srcStats = new QLabel, *dstStats = new QLabel;
    QLabel *arrow = new QLabel("</>");
    arrow->setAlignment(Qt::AlignCenter);
    QFont fnt = arrow->font();
    fnt.setBold(true);
    fnt.setPointSize(fnt.pointSize()*4);
    arrow->setFont(fnt);
    srcStats->setAlignment(Qt::AlignRight);
    QStringList headers = QObject::tr("Skip,Replace,Index,Rename,Filename").split(',');
    QTableWidget *table = new QTableWidget(collisions.size(), headers.size()); // skip, overwrite, autoRename, rename, src, dst
    table->setHorizontalHeaderLabels(headers);
    table->setVerticalHeader(nullptr);
    int i = 0;
    for (auto c = collisions.cbegin(), end = collisions.cend(); c != end; ++c, ++i) {
        for (int j = 0; j < table->columnCount() - 1; ++j) {
            QTableWidgetItem *item = new QTableWidgetItem;
            item->setFlags(j ? Qt::ItemIsEnabled|Qt::ItemIsUserCheckable : Qt::ItemIsEnabled);
            item->setCheckState(j ? Qt::Unchecked : Qt::Checked);
            if (!j)
                item->setToolTip(c.key());
            table->setItem(i, j, item);
        }
        QTableWidgetItem *item = new QTableWidgetItem(QFileInfo(*c).fileName());
        item->setToolTip(*c);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsEditable);
        table->setItem(i, table->columnCount() - 1, item);
    }
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->resizeColumnsToContents();
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    EnterGuard *enterGuard = new EnterGuard(table);
    table->installEventFilter(enterGuard);
    auto setArrow = [=](int col) {
    if (col == 0) {
            arrow->setText("><<");
            dstPreview->setGraphicsEffect(nullptr);
        } else if (col == 1) {
            QGraphicsOpacityEffect *magic = new QGraphicsOpacityEffect(dstPreview);
            dstPreview->setGraphicsEffect(magic);
            magic->setOpacity(0.5);
            arrow->setText("<<<");
        } else {
            arrow->setText("^_^");
            dstPreview->setGraphicsEffect(nullptr);
        }
    };
    QObject::connect(table, &QTableWidget::cellChanged, [=](int row, int column) {
        QTableWidgetItem *it = table->item(row, column);
        if (column == table->columnCount() - 1) {
            // name changed, toggle rename
            QFileInfo info(table->item(row, column)->toolTip());
            if (QFileInfo::exists(info.absolutePath() + QDir::separator() + table->item(row, column)->text())) {
                MessageBox(dlg).warning(QObject::tr("New file also exists"),
                                        QObject::tr("The new filename also conflicts with an existing file.\n"
                                                    "The existing file would be overwritten!"));
                table->item(row, 0)->setCheckState(Qt::Checked);
            } else {
                table->item(row, column - 1)->setCheckState(Qt::Checked);
            }
        } else {
            // radio feature
            if (it->checkState() != Qt::Checked)
                return;
            table->blockSignals(true);
            if (column == table->columnCount()-2) {
                int dst = table->columnCount()-1;
                QTableWidgetItem *dit = table->item(row, dst);
                QFileInfo info(dit->toolTip());
                // allow user to fix the destination
                if (QFileInfo::exists(info.absolutePath() + QDir::separator() + dit->text())) {
                    dit->setText("0_0 " + dit->text());
                    table->setCurrentItem(dit);
                    table->setFocus();
                    enterGuard->watch = true;
                    table->editItem(dit);
                }
            }
            it->setFlags((it->flags() & ~Qt::ItemIsUserCheckable));
            for (int j = 0; j < table->columnCount() - 1; ++j) {
                if (j == column)
                    continue;
                it = table->item(row, j);
                it->setCheckState(Qt::Unchecked);
                it->setFlags((it->flags() | Qt::ItemIsUserCheckable));
            }
            if (row == table->currentRow())
                setArrow(column);
            table->blockSignals(false);
        }
    });
    static QMap<QString, ImagePrint> pixMap;
    QObject::connect(table, &QTableWidget::currentCellChanged, [=](int row, int /* column */, int prevR, int /* prevC */) {
        enterGuard->watch = false;
        if (row != prevR) {
            for (int i = 0; i < 4; ++i) {
                if (table->item(row, i)->checkState() == Qt::Checked) {
                    setArrow(i);
                    break;
                }
            }
            for (int i : { 0, 4 }) {
                QString path = table->item(row, i)->toolTip();
                ImagePrint ip = pixMap.value(path);
                if (ip.pix.isNull()) {
                    QSize sz;
                    ip.pix = loadPreview(path, sz);
                    QFileInfo info(path);
                    QFile f(path);
                    f.open(QFile::ReadOnly);
                    QCryptographicHash md5(QCryptographicHash::Md5);
                    md5.addData(&f);
                    ip.stats = QString::number(info.size() / 1024.0, 'f', 2) + "K\n"
                             + info.lastModified().toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat)) + "\n"
                             + QString::number(sz.width()) + "x" + QString::number(sz.height()) + "\n"
                             + QString::fromLatin1(md5.result().toHex());
                    f.close();
                    pixMap[path] = ip;
                }
                i ? dstPreview->setPixmap(ip.pix) : srcPreview->setPixmap(ip.pix);
                i ? dstStats->setText(ip.stats) : srcStats->setText(ip.stats);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
    });
    dlg->setModal(true);
    dlg->setWindowTitle(QObject::tr("File collision resolver"));
    QVBoxLayout *vl = new QVBoxLayout(dlg);
    vl->addWidget(new QLabel(QObject::tr("<h3>Some of the destination files already exist</h3>"
                             "You can resolve the conflicts here or abort the entire copy/move operation.<br>"
                             "<b>By default, the conflicting files will be skipped.</b><br>"
                             "You can also replace or rename the destination, or automatically add a counter for a conflict-free filename."), dlg));
    vl->addWidget(table);
    QGridLayout *glt = new QGridLayout;
    glt->addWidget(dstPreview, 0, 0);
    glt->addWidget(arrow, 0, 1, Qt::AlignCenter);
    glt->addWidget(srcPreview, 0, 2);
    glt->addWidget(dstStats, 1, 0);
    QLabel *legend = new QLabel(QObject::tr("Size") + "\n" + QObject::tr("Modified") + "\n" + QObject::tr("Resolution") + "\n" + QObject::tr("MD5"));
    legend->setAlignment(Qt::AlignHCenter);
    glt->addWidget(legend, 1, 1);
    glt->addWidget(srcStats, 1, 2);
    vl->addLayout(glt);
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Abort|QDialogButtonBox::Ok, dlg);
    vl->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    const QSizeF sz = QFontMetrics(dlg->font()).boundingRect('o').size();
    dlg->resize(sz.width() * 100, sz.width() * 100);
    int ret = dlg->exec();
    pixMap.clear();
    // fix collisions regardless of return, nobody knows what the client code wants with this
    i = 0;
    for (auto c = collisions.begin(), end = collisions.end(); c != end; ++i) {
        if (table->item(i, 0)->checkState() ==  Qt::Checked) { // skip
            c = collisions.erase(c);
            continue;
        } else if (table->item(i, 2)->checkState() ==  Qt::Checked) { // auto-rename
            QFileInfo info(*c);
            *c = info.absolutePath() + QDir::separator() + autoRename(info.absolutePath(), info.fileName());
        } else if (table->item(i, 3)->checkState() ==  Qt::Checked) { // manual rename
            QFileInfo info(*c);
            *c = info.absolutePath() + QDir::separator() + table->item(i, 4)->text();
        }
        ++c;
    }
    dlg->deleteLater();
    return QDialog::DialogCode(ret);
}

int CopyOrMove::list(ThumbsViewer *thumbView, QString &destDir, bool pasteInCurrDir, QWidget *parent) {

    QList<int> rowList; // Only for !pasteInCurrDir

    QElapsedTimer duration;
    int totalTime = 0;
    duration.start();
    int cycle = 1;
    QProgressDialog *pdlg = nullptr;
    QLabel *pdlg_label = nullptr;

    int n = pasteInCurrDir ? Settings::copyCutFileList.size() : Settings::copyCutIndexList.size();

    QMap<QString, QString> collisions;
    for (int i = 0; i < n; ++i) {
        QString sourceFile = pasteInCurrDir ? Settings::copyCutFileList.at(i)
                                            : thumbView->fullPathOf(Settings::copyCutIndexList.at(i).row());
        QString destFile = destDir + QDir::separator() + QFileInfo(sourceFile).fileName();
        if (QFileInfo::exists(destFile))
            collisions[sourceFile] = destFile;
    }
    if (!collisions.isEmpty()) {
        if (resolveConflicts(collisions, parent) == QDialog::Rejected)
            return -1;
    }
    for (int i = pasteInCurrDir ? 0 : n-1; pasteInCurrDir ? i < n : i >= 0; pasteInCurrDir ? ++i : --i) {
        QString sourceFile = pasteInCurrDir ? Settings::copyCutFileList.at(i)
                                            : thumbView->fullPathOf(Settings::copyCutIndexList.at(i).row());
        QString destFile = destDir + QDir::separator() + QFileInfo(sourceFile).fileName();

        if (duration.elapsed() > 30) {
            const int count = pasteInCurrDir ? i : n - i;
            if ((totalTime += duration.elapsed()) > 250) {
                totalTime = 0;
                if (float(count)/n < 1.0f-1.0f/++cycle) {
                    if (!pdlg) {
                        pdlg = new QProgressDialog(parent);
                        pdlg->setMaximum(n);
                        QLabel *l = new QLabel(pdlg);
                        pdlg_label = l;
                        l->setWordWrap(true);
                        l->setFixedWidth(QFontMetrics(l->font()).averageCharWidth()*80);
                        pdlg->setLabel(l);
                    }
                    pdlg->show();
                }
            }
            if (pdlg && pdlg->isVisible()) {
                pdlg->setValue(count);
                QFontMetrics fm(pdlg_label->font());
                QString text =
                    (Settings::isCopyOperation ? QObject::tr("Copying \"%1\" to \"%2\".") : QObject::tr("Moving \"%1\" to \"%2\"."))
                    .arg(fm.elidedText(sourceFile, Qt::ElideMiddle, pdlg_label->width(), Qt::TextWordWrap))
                    .arg(fm.elidedText(destFile, Qt::ElideMiddle, pdlg_label->width(), Qt::TextWordWrap));

                pdlg->setLabelText(text);
            }
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            duration.restart();
        }

        if (QFileInfo::exists(destFile)) {
            destFile = collisions.value(sourceFile);
            if (destFile.isEmpty())
                continue; // skipped
            if (QFileInfo::exists(destFile)) { // overwrite
                if (pasteInCurrDir) {
                    QModelIndexList indexList = thumbView->model()->match(thumbView->model()->index(0, 0), ThumbsViewer::FileNameRole, destFile);
                    qDebug() << "pasteInCurrDir" << indexList.size();
                    if (indexList.size())
                        thumbView->model()->removeRow(indexList.at(0).row());
                }
                QFile::remove(destFile); // prevent subsequent autorename by copyOrMoveFile
            }
        }
        int res = CopyOrMove::file(sourceFile, destFile, Settings::isCopyOperation);
        if (!res || (pdlg && pdlg->wasCanceled()))
            break;

        if (pasteInCurrDir)
            Settings::copyCutFileList[i] = destFile;
        else
            rowList.append(Settings::copyCutIndexList.at(i).row());
    }

    int latestRow = -1;
    if (!pasteInCurrDir) {
        if (!Settings::isCopyOperation) {
            std::sort(rowList.begin(), rowList.end());
            QSignalBlocker scrollbarBlocker(thumbView->verticalScrollBar()); // to not trigger thumb loading
            for (int t = rowList.size() - 1; t >= 0; --t)
                thumbView->model()->removeRow(rowList.at(t));
        }
        latestRow = rowList.size() ? rowList.at(0) : -1;
    }
    if (pdlg)
        pdlg->deleteLater();
    pdlg = nullptr;
    return latestRow;
}
