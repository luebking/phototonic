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

#ifndef COPY_MOVE_DIALOG_H
#define COPY_MOVE_DIALOG_H

class ThumbsViewer;

namespace CopyOrMove {
    int file(const QString &srcPath, QString &dstPath, bool copy);
    inline int copyFile(const QString &srcPath, QString &dstPath) {
        return CopyOrMove::file(srcPath, dstPath, true);
    }
    inline int moveFile(const QString &srcPath, QString &dstPath) {
        return CopyOrMove::file(srcPath, dstPath, false);
    }
    QDialog::DialogCode resolveConflicts(QMap<QString,QString> &collisions, QWidget *parent = nullptr);

    /** @return latestRow **/
    int list(ThumbsViewer *thumbView, QString &destDir, bool pasteInCurrDir, QWidget *parent = nullptr);
}

#endif // COPY_MOVE_DIALOG_H
