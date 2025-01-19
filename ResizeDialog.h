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

#ifndef RESIZE_DIALOG_H
#define RESIZE_DIALOG_H

class QCheckBox;
class QLabel;
class QRadioButton;
class QSpinBox;
#include <QDialog>

class ResizeDialog : public QDialog {
Q_OBJECT

public:
    ResizeDialog(QSize originalSize, QWidget *parent = nullptr);
    QSize newSize();

private slots:
    void setUnits();
    void adjustSizes();

private:
    QSize m_originalSize;
    QSize m_last;

    QCheckBox *keepAspect;
    QSpinBox *widthSpinBox;
    QSpinBox *heightSpinBox;
    QRadioButton *pixelsRadioButton;
    QLabel *newSizePixelsLabel;
};

#endif // RESIZE_DIALOG_H