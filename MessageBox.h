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

#ifndef MESSAGE_BOX_H
#define MESSAGE_BOX_H

#include <QMessageBox>

class MessageBox : public QMessageBox {
Q_OBJECT

public:
    MessageBox(QWidget *parent, StandardButtons buttons = NoButton, StandardButton defaultButton = NoButton);
    void about();
    int ask(const QString &title, const QString &message);
    void critical(const QString &title, const QString &message);
    int warning(const QString &title, const QString &message);
};

#endif // MESSAGE_BOX_H
