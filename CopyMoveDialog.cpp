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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QElapsedTimer>

#include "CopyMoveDialog.h"
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

int CopyMoveDialog::copyOrMoveFile(const QString &srcPath, QString &dstPath, bool copy) {
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

CopyMoveDialog::CopyMoveDialog(QWidget *parent) : QProgressDialog(parent) {
    m_label = new QLabel("");
    m_label->setWordWrap(true);
    m_label->setFixedWidth(QFontMetrics(m_label->font()).averageCharWidth()*80);
    setLabel(m_label);
}

void CopyMoveDialog::execute(ThumbsViewer *thumbView, QString &destDir, bool pasteInCurrDir) {

    QList<int> rowList; // Only for !pasteInCurrDir
    QFontMetrics fm(m_label->font());

    QElapsedTimer duration;
    int totalTime = 0;
    duration.start();
    int cycle = 1;

    int n = pasteInCurrDir ? Settings::copyCutFileList.size() : Settings::copyCutIndexList.size();
    setMaximum(n);

    for (int i = pasteInCurrDir ? 0 : n-1; pasteInCurrDir ? i < n : i >= 0; pasteInCurrDir ? ++i : --i) {
        QString sourceFile = pasteInCurrDir ? Settings::copyCutFileList.at(i)
                                            : thumbView->fullPathOf(Settings::copyCutIndexList.at(i).row());
        QString destFile = destDir + QDir::separator() + QFileInfo(sourceFile).fileName();
        if (duration.elapsed() > 30) {
            const int count = pasteInCurrDir ? i : n - i;
            if ((totalTime += duration.elapsed()) > 250) {
                totalTime = 0;
                if (float(count)/n < 1.0f-1.0f/++cycle)
                    show();
            }
            if (isVisible()) {
                setValue(count);
                QString text =
                    (Settings::isCopyOperation ? tr("Copying \"%1\" to \"%2\".") : tr("Moving \"%1\" to \"%2\"."))
                    .arg(fm.elidedText(sourceFile, Qt::ElideMiddle, m_label->width(), Qt::TextWordWrap))
                    .arg(fm.elidedText(destFile, Qt::ElideMiddle, m_label->width(), Qt::TextWordWrap));

                setLabelText(text);
            }
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            duration.restart();
        }

        int res = copyOrMoveFile(sourceFile, destFile, Settings::isCopyOperation);
        if (!res || wasCanceled())
            break;

        if (pasteInCurrDir)
            Settings::copyCutFileList[i] = destFile;
        else
            rowList.append(Settings::copyCutIndexList.at(i).row());
    }

    if (!pasteInCurrDir) {
        if (!Settings::isCopyOperation) {
            std::sort(rowList.begin(), rowList.end());
            QSignalBlocker scrollbarBlocker(thumbView->verticalScrollBar()); // to not trigger thumb loading
            for (int t = rowList.size() - 1; t >= 0; --t)
                thumbView->model()->removeRow(rowList.at(t));
        }
        latestRow = rowList.size() ? rowList.at(0) : -1;
    }
    close();
}
