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
#include <QLabel>
#include <QPushButton>

#include "CopyMoveDialog.h"
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
    int res;

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

    return res;
}

CopyMoveDialog::CopyMoveDialog(QWidget *parent) : QDialog(parent) {
    abortOp = false;

    opLabel = new QLabel("");

    cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(abort()));

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addWidget(opLabel);

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(buttonsLayout, Qt::AlignRight);
    setLayout(mainLayout);
}

void CopyMoveDialog::execute(ThumbsViewer *thumbView, QString &destDir, bool pasteInCurrDir) {
    show();

    if (pasteInCurrDir) {
        for (int tn = 0; tn < Settings::copyCutFileList.size(); ++tn) {
            QString sourceFile = Settings::copyCutFileList.at(tn);
            QString destFile = destDir + QDir::separator() + QFileInfo(sourceFile).fileName();

            opLabel->setText((Settings::isCopyOperation ? tr("Copying \"%1\" to \"%2\".") : tr("Moving \"%1\" to \"%2\"."))
                                     .arg(sourceFile).arg(destFile));
            QApplication::processEvents();

            int res = copyOrMoveFile(sourceFile, destFile, Settings::isCopyOperation);

            if (!res || abortOp) {
                break;
            } else {
                Settings::copyCutFileList[tn] = destFile;
            }
        }
    } else {
        QList<int> rowList;
        for (int tn = Settings::copyCutIndexList.size() - 1; tn >= 0; --tn) {
            QString sourceFile = thumbView->fullPathOf(Settings::copyCutIndexList.at(tn).row());
            QString destFile = destDir + QDir::separator() + QFileInfo(sourceFile).fileName();

            opLabel->setText((Settings::isCopyOperation ? tr("Copying \"%1\" to \"%2\".") : tr("Moving \"%1\" to \"%2\"."))
                                     .arg(sourceFile).arg(destFile));
            QApplication::processEvents();

            int res = copyOrMoveFile(sourceFile, destFile, Settings::isCopyOperation);

            if (!res || abortOp) {
                break;
            }

            rowList.append(Settings::copyCutIndexList.at(tn).row());
        }

        if (!Settings::isCopyOperation) {
            std::sort(rowList.begin(), rowList.end());
            for (int t = rowList.size() - 1; t >= 0; --t)
                thumbView->model()->removeRow(rowList.at(t));
        }
        latestRow = rowList.size() ? rowList.at(0) : -1;
    }

    nFiles = Settings::copyCutIndexList.size();
    close();
}

void CopyMoveDialog::abort() {
    abortOp = true;
}
