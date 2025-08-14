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

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

class QAbstractButton;
class QCheckBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QToolButton;
class QDoubleSpinBox;
#include <QDialog>

class SettingsDialog : public QDialog {
Q_OBJECT

public:
    SettingsDialog(QWidget *parent);

private slots:

    void pickColor();

    void pickThumbsColor();

    void pickThumbsTextColor();

    void pickStartupDir();

    void pickBackgroundImage();

public slots:

    void abort();

    void saveSettings();

private:
    QToolButton *backgroundColorButton;
    QToolButton *thumbsColorPickerButton;
    QToolButton *thumbsLabelColorButton;
    QSpinBox *thumbPagesSpinBox;
    QSpinBox *saveQualitySpinBox;
    QColor imageViewerBackgroundColor;
    QColor thumbsBackgroundColor;
    QColor thumbsTextColor;
    QCheckBox *wrapListCheckBox;
    QCheckBox *enableAnimCheckBox;
    QCheckBox *enableExifCheckBox;
    QCheckBox *enableThumbExifCheckBox;
    QCheckBox *showImageNameCheckBox;
    QCheckBox *reverseMouseCheckBox;
    QCheckBox *scrollZoomCheckBox;
    QCheckBox *deleteConfirmCheckBox;
    QDoubleSpinBox *slideDelaySpinBox;
    QCheckBox *slideRandomCheckBox;
    QCheckBox *slideCrossfadeCheckBox;
    QRadioButton *startupDirectoryRadioButtons[3];
    QLineEdit *startupDirLineEdit;
    QLineEdit *thumbsBackgroundImageLineEdit;
    QCheckBox *thumbsRepeatBackgroundImageCheckBox;
    QCheckBox *setWindowIconCheckBox;
    QCheckBox *upscalePreviewCheckBox;
    QTableWidget *bangTable;

    void setButtonBgColor(QColor &color, QAbstractButton *button);
};

#endif // SETTINGS_DIALOG_H

