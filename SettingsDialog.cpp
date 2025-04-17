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

#include <QBoxLayout>
#include <QCheckBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>

#include "Settings.h"
#include "SettingsDialog.h"
#include "ShortcutsTable.h"

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Preferences"));
    setWindowIcon(QIcon::fromTheme("preferences-system", QIcon(":/images/phototonic.png")));

    // imageViewer background color
    QLabel *backgroundColorLabel = new QLabel(tr("Background color:"));
    backgroundColorButton = new QToolButton();
    backgroundColorButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QHBoxLayout *backgroundColorHBox = new QHBoxLayout;
    backgroundColorHBox->addWidget(backgroundColorLabel);
    backgroundColorHBox->addWidget(backgroundColorButton);
    backgroundColorHBox->addStretch(1);
    connect(backgroundColorButton, SIGNAL(clicked()), this, SLOT(pickColor()));
    setButtonBgColor(Settings::viewerBackgroundColor, backgroundColorButton);
    imageViewerBackgroundColor = Settings::viewerBackgroundColor;

    // Wrap image list
    wrapListCheckBox = new QCheckBox(tr("Wrap image list when reaching last or first image"), this);
    wrapListCheckBox->setChecked(Settings::wrapImageList);

    // Save quality
    QLabel *saveQualityLabel = new QLabel(tr("Default quality when saving:"));
    saveQualitySpinBox = new QSpinBox;
    saveQualitySpinBox->setRange(0, 100);
    saveQualitySpinBox->setValue(Settings::defaultSaveQuality);
    QHBoxLayout *saveQualityHbox = new QHBoxLayout;
    saveQualityHbox->addWidget(saveQualityLabel);
    saveQualityHbox->addWidget(saveQualitySpinBox);
    saveQualityHbox->addStretch(1);

    // Enable animations
    enableAnimCheckBox = new QCheckBox(tr("Enable GIF animation"), this);
    enableAnimCheckBox->setChecked(Settings::enableAnimations);

    // Enable image Exif rotation
    enableExifCheckBox = new QCheckBox(tr("Rotate image according to Exif orientation value"), this);
    enableExifCheckBox->setChecked(Settings::exifRotationEnabled);

    // Image name
    showImageNameCheckBox = new QCheckBox(tr("Show image file name in viewer"), this);
    showImageNameCheckBox->setChecked(Settings::showImageName);

    // Viewer options
    QVBoxLayout *viewerOptsBox = new QVBoxLayout;
    viewerOptsBox->addLayout(backgroundColorHBox);
    viewerOptsBox->addWidget(enableExifCheckBox);
    viewerOptsBox->addWidget(showImageNameCheckBox);
    viewerOptsBox->addWidget(wrapListCheckBox);
    viewerOptsBox->addWidget(enableAnimCheckBox);
    viewerOptsBox->addLayout(saveQualityHbox);
    viewerOptsBox->addStretch(1);

    // thumbsViewer background color
    QLabel *thumbsBackgroundColorLabel = new QLabel(tr("Thumbnails and Preview Background Color:"));
    thumbsColorPickerButton = new QToolButton();
    thumbsColorPickerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QHBoxLayout *thumbsBackgroundColorLayout = new QHBoxLayout;
    thumbsBackgroundColorLayout->addWidget(thumbsBackgroundColorLabel);
    thumbsBackgroundColorLayout->addWidget(thumbsColorPickerButton);
    thumbsBackgroundColorLayout->addStretch(1);
    connect(thumbsColorPickerButton, SIGNAL(clicked()), this, SLOT(pickThumbsColor()));
    setButtonBgColor(Settings::thumbsBackgroundColor, thumbsColorPickerButton);
    thumbsBackgroundColor = Settings::thumbsBackgroundColor;

    // thumbsViewer text color
    QLabel *thumbLabelColorLabel = new QLabel(tr("Label color:"));
    thumbsLabelColorButton = new QToolButton();
    thumbsLabelColorButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QHBoxLayout *thumbsLabelColorLayout = new QHBoxLayout;
    thumbsLabelColorLayout->addWidget(thumbLabelColorLabel);
    thumbsLabelColorLayout->addWidget(thumbsLabelColorButton);
    thumbsLabelColorLayout->addStretch(1);
    connect(thumbsLabelColorButton, SIGNAL(clicked()), this, SLOT(pickThumbsTextColor()));
    setButtonBgColor(Settings::thumbsTextColor, thumbsLabelColorButton);
    thumbsTextColor = Settings::thumbsTextColor;

    // thumbsViewer background image
    QLabel *thumbsBackgroundImageLabel = new QLabel(tr("Background image:"));
    thumbsBackgroundImageLineEdit = new QLineEdit;
    thumbsBackgroundImageLineEdit->setClearButtonEnabled(true);
    thumbsBackgroundImageLineEdit->setMinimumWidth(200);


    QPushButton *chooseThumbsBackImageButton = new QPushButton();
    chooseThumbsBackImageButton->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
    chooseThumbsBackImageButton->setFixedSize(26, 26);
    chooseThumbsBackImageButton->setIconSize(QSize(16, 16));
    connect(chooseThumbsBackImageButton, SIGNAL(clicked()), this, SLOT(pickBackgroundImage()));


    QHBoxLayout *thumbsBackgroundImageLayout = new QHBoxLayout;
    thumbsBackgroundImageLayout->addWidget(thumbsBackgroundImageLabel);
    thumbsBackgroundImageLayout->addWidget(thumbsBackgroundImageLineEdit);
    thumbsBackgroundImageLayout->addWidget(chooseThumbsBackImageButton);
    thumbsBackgroundImageLayout->addStretch(1);
    thumbsBackgroundImageLineEdit->setText(Settings::thumbsBackgroundImage);

    // Thumbnail pages to read ahead
    QLabel *thumbsPagesReadLabel = new QLabel(tr("Number of thumbnail pages to read ahead:"));
    thumbPagesSpinBox = new QSpinBox;
    thumbPagesSpinBox->setRange(1, 10);
    thumbPagesSpinBox->setValue(Settings::thumbsPagesReadCount);
    QHBoxLayout *thumbPagesReadLayout = new QHBoxLayout;
    thumbPagesReadLayout->addWidget(thumbsPagesReadLabel);
    thumbPagesReadLayout->addWidget(thumbPagesSpinBox);
    thumbPagesReadLayout->addStretch(1);

    enableThumbExifCheckBox = new QCheckBox(tr("Rotate thumbnail according to Exif orientation value"), this);
    enableThumbExifCheckBox->setChecked(Settings::exifThumbRotationEnabled);

    // Upscale preview image
    upscalePreviewCheckBox = new QCheckBox(tr("Scale up small images in preview"), this);
    upscalePreviewCheckBox->setChecked(Settings::upscalePreview);

    // Thumbnail options
    QVBoxLayout *thumbsOptsBox = new QVBoxLayout;
    thumbsOptsBox->addLayout(thumbsBackgroundColorLayout);

    thumbsOptsBox->addLayout(thumbsBackgroundImageLayout);

    thumbsOptsBox->addLayout(thumbsLabelColorLayout);
    thumbsOptsBox->addWidget(enableThumbExifCheckBox);
    thumbsOptsBox->addLayout(thumbPagesReadLayout);
    thumbsOptsBox->addWidget(upscalePreviewCheckBox);
    thumbsOptsBox->addStretch(1);

    // Mouse settings
    reverseMouseCheckBox = new QCheckBox(tr("Swap mouse double-click and middle-click actions"), this);
    reverseMouseCheckBox->setChecked(Settings::reverseMouseBehavior);

    scrollZoomCheckBox = new QCheckBox(tr("Use scroll wheel for zooming in image viewer"), this);
    scrollZoomCheckBox->setChecked(Settings::scrollZooms);

    // Delete confirmation setting
    deleteConfirmCheckBox = new QCheckBox(tr("Delete confirmation"), this);
    deleteConfirmCheckBox->setChecked(Settings::deleteConfirm);

    // Startup directory
    QGroupBox *startupDirGroupBox = new QGroupBox(tr("Startup directory if not specified by command line"));
    startupDirectoryRadioButtons[Settings::RememberLastDir] = new QRadioButton(tr("Remember last"));
    startupDirectoryRadioButtons[Settings::DefaultDir] = new QRadioButton(tr("Default"));
    //: specificy a startup directory
    startupDirectoryRadioButtons[Settings::SpecifiedDir] = new QRadioButton(tr("Specify:"));

    startupDirLineEdit = new QLineEdit;
    startupDirLineEdit->setClearButtonEnabled(true);
    startupDirLineEdit->setMinimumWidth(300);
    startupDirLineEdit->setMaximumWidth(400);

    QPushButton *chooseStartupDirButton = new QPushButton();
    chooseStartupDirButton->setIcon(QIcon::fromTheme("document-open", QIcon(":/images/open.png")));
    chooseStartupDirButton->setFixedSize(26, 26);
    chooseStartupDirButton->setIconSize(QSize(16, 16));
    connect(chooseStartupDirButton, SIGNAL(clicked()), this, SLOT(pickStartupDir()));

    QHBoxLayout *startupDirectoryLayout = new QHBoxLayout;
    startupDirectoryLayout->addWidget(startupDirectoryRadioButtons[2]);
    startupDirectoryLayout->addWidget(startupDirLineEdit);
    startupDirectoryLayout->addWidget(chooseStartupDirButton);
    startupDirectoryLayout->addStretch(1);

    QVBoxLayout *startupDirectoryMainLayout = new QVBoxLayout;
    for (int i = 0; i < 2; ++i) {
        startupDirectoryMainLayout->addWidget(startupDirectoryRadioButtons[i]);
        startupDirectoryRadioButtons[i]->setChecked(false);
    }
    startupDirectoryMainLayout->addLayout(startupDirectoryLayout);
    startupDirectoryMainLayout->addStretch(1);
    startupDirGroupBox->setLayout(startupDirectoryMainLayout);

    if (Settings::startupDir == Settings::SpecifiedDir) {
        startupDirectoryRadioButtons[Settings::SpecifiedDir]->setChecked(true);
    } else if (Settings::startupDir == Settings::RememberLastDir) {
        startupDirectoryRadioButtons[Settings::RememberLastDir]->setChecked(true);
    } else {
        startupDirectoryRadioButtons[Settings::DefaultDir]->setChecked(true);
    }
    startupDirLineEdit->setText(Settings::specifiedStartDir);

    // Keyboard shortcuts
    ShortcutsTable *shortcutsTable = new ShortcutsTable();
    shortcutsTable->refreshShortcuts();
    QGroupBox *keyboardGroupBox = new QGroupBox(tr("Shortcuts"));
    QVBoxLayout *keyboardSettingsLayout = new QVBoxLayout;

    QHBoxLayout *filterShortcutsLayout = new QHBoxLayout;
    QLineEdit *shortcutsFilterLineEdit = new QLineEdit;
    shortcutsFilterLineEdit->setClearButtonEnabled(true);
    shortcutsFilterLineEdit->setPlaceholderText(tr("Filter Items"));
    connect(shortcutsFilterLineEdit, SIGNAL(textChanged(
                                                    const QString&)), shortcutsTable, SLOT(setFilter(
                                                                                                   const QString&)));
    keyboardSettingsLayout->addWidget(new QLabel(tr("Select an entry and press a key to set a new shortcut")));
    keyboardSettingsLayout->addWidget(shortcutsFilterLineEdit);
    keyboardSettingsLayout->addWidget(shortcutsTable);
    keyboardSettingsLayout->addLayout(filterShortcutsLayout);
    keyboardGroupBox->setLayout(keyboardSettingsLayout);

    // Set window icon
    setWindowIconCheckBox = new QCheckBox(tr("Set the application icon according to the current image"), this);
    setWindowIconCheckBox->setChecked(Settings::setWindowIcon);

    QVBoxLayout *generalSettingsLayout = new QVBoxLayout;
    generalSettingsLayout->addWidget(reverseMouseCheckBox);
    generalSettingsLayout->addWidget(deleteConfirmCheckBox);
    generalSettingsLayout->addWidget(startupDirGroupBox);
    generalSettingsLayout->addWidget(scrollZoomCheckBox);

    // Slide show delay
    QLabel *slideDelayLab = new QLabel(tr("Delay between slides in seconds:"));
    slideDelaySpinBox = new QSpinBox;
    slideDelaySpinBox->setRange(1, 3600);
    slideDelaySpinBox->setValue(Settings::slideShowDelay);
    QHBoxLayout *slideDelayLayout = new QHBoxLayout;
    slideDelayLayout->addWidget(slideDelayLab);
    slideDelayLayout->addWidget(slideDelaySpinBox);
    slideDelayLayout->addStretch(1);

    // Slide show random
    slideRandomCheckBox = new QCheckBox(tr("Show random images"), this);
    slideRandomCheckBox->setChecked(Settings::slideShowRandom);

    // Slide show random
    slideCrossfadeCheckBox = new QCheckBox(tr("Crossfade images"), this);
    slideCrossfadeCheckBox->setChecked(Settings::slideShowCrossfade);

    // Slide show options
    QVBoxLayout *slideshowLayout = new QVBoxLayout;
    slideshowLayout->addLayout(slideDelayLayout);
    slideshowLayout->addWidget(slideRandomCheckBox);
    slideshowLayout->addWidget(slideCrossfadeCheckBox);
    slideshowLayout->addStretch(1);

    QGroupBox *slideshowGroupBox = new QGroupBox(tr("Slideshow"));
    slideshowGroupBox->setLayout(slideshowLayout);
    generalSettingsLayout->addWidget(slideshowGroupBox);
    generalSettingsLayout->addWidget(setWindowIconCheckBox);
    generalSettingsLayout->addStretch(1);

    QVBoxLayout *bangLayout = new QVBoxLayout;
    QLabel *rtfm = new QLabel(
    tr("<p>Bangs allow you to use external commands to generate the list of shown images.<br>"
    "The main purpose is to query databases like locate, baloo or tracker, "
    "but anything that can generate a list of image files is suitable</p>"
    "<p>The token <b>%s</b> in the command will be replaced with the parameter.</p>"
    "<p>Eg. for plocate, using the shortcut <i>locate</i> and the command<br>"
    "<i>bash -c \"locate -i '*%s*' | grep --line-buffered -iE '(jpe?g|png)$'\"</i><br>"
    "allows you to enter <i>locate:waldo</i> to display indexed jpg's and png's of waldo.</p>"
    "<p>Phototonic tests the files for existence and will remove duplicates (including file and "
    "directory symlinks.</p>"));
    rtfm->setWordWrap(true);
    bangLayout->addWidget(rtfm);
    bangTable = new QTableWidget(this);
    bangTable->setColumnCount(2);
    bangTable->setSelectionMode(QAbstractItemView::SingleSelection);
    bangTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    bangTable->verticalHeader()->setVisible(false);
    bangTable->verticalHeader()->setDefaultSectionSize(bangTable->verticalHeader()->minimumSectionSize());
    bangTable->setHorizontalHeaderItem(0, new QTableWidgetItem(tr("Shortcut")));
    bangTable->setHorizontalHeaderItem(1, new QTableWidgetItem(tr("Command")));
    bangTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    bangTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    bangTable->setShowGrid(true);
    bangTable->setRowCount(Settings::bangs.size()+1);
    int row = 0;
    for (auto i = Settings::bangs.cbegin(), end = Settings::bangs.cend(); i != end; ++i) {
        bangTable->setItem(row, 0, new QTableWidgetItem(i.key()));
        bangTable->setItem(row++, 1, new QTableWidgetItem(i.value()));
    }
    bangLayout->addWidget(bangTable);
    connect (bangTable, &QTableWidget::cellChanged, this, [=](int row, int col) {
        if (row < bangTable->rowCount() - 1 && bangTable->item(row,0)->text().isEmpty() && bangTable->item(row,1)->text().isEmpty()) {
            bangTable->removeRow(row);
        } else {
            if (col == 0) {
                bangTable->blockSignals(true);
                for (int i = 0; i < bangTable->rowCount(); ++i) {
                    if (QTableWidgetItem *item = bangTable->item(i, 0))
                        item->setForeground(QBrush());
                }
                for (int i = 0; i < bangTable->rowCount() - 1; ++i) {
                    QTableWidgetItem *item_i = bangTable->item(i, 0);
                    if (!item_i)
                        continue;
                for (int j = i + 1; j < bangTable->rowCount(); ++j) {
                    QTableWidgetItem *item_j = bangTable->item(j, 0);
                    if (!item_j || item_j->text() != item_i->text())
                        continue;
                    item_i->setForeground(QColor(208,23,23));
                    item_j->setForeground(QColor(208,23,23));
                }
                }
                bangTable->blockSignals(false);
            }
            if (row == bangTable->rowCount()-1 && !bangTable->item(row,col)->text().isEmpty())
                bangTable->setRowCount(bangTable->rowCount() + 1);
        }
    });
    QAction *act = new QAction;
    act->setShortcut(Qt::Key_Delete);
    act->setShortcutContext(Qt::WidgetShortcut);
    connect(act, &QAction::triggered, [=]() {
        bangTable->currentItem()->setText(QString());
    });
    bangTable->addAction(act);

    /* Confirmation buttons */
    QHBoxLayout *confirmSettingsLayout = new QHBoxLayout;
    QPushButton *okButton = new QPushButton(tr("OK"));
    okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(okButton, SIGNAL(clicked()), this, SLOT(saveSettings()));
    okButton->setDefault(true);
    QPushButton *closeButton = new QPushButton(tr("Cancel"));
    closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(abort()));
    confirmSettingsLayout->addWidget(closeButton, 1, Qt::AlignRight);
    confirmSettingsLayout->addWidget(okButton, 0, Qt::AlignRight);

    /* Tabs */
    QTabWidget *settingsTabs = new QTabWidget;

    QWidget *viewerSettings = new QWidget;
    viewerSettings->setLayout(viewerOptsBox);
    settingsTabs->addTab(viewerSettings, tr("Viewer"));

    QWidget *thumbSettings = new QWidget;
    thumbSettings->setLayout(thumbsOptsBox);
    settingsTabs->addTab(thumbSettings, tr("Thumbnails"));

    QWidget *generalSettings = new QWidget;
    generalSettings->setLayout(generalSettingsLayout);
    settingsTabs->addTab(generalSettings, tr("General"));

    QWidget *keyboardSettings = new QWidget;
    keyboardSettings->setLayout(keyboardSettingsLayout);
    settingsTabs->addTab(keyboardSettings, tr("Shortcuts"));

    QWidget *bangSettings = new QWidget;
    bangSettings->setLayout(bangLayout);
    settingsTabs->addTab(bangSettings, tr("Bangs"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(settingsTabs);
    mainLayout->addLayout(confirmSettingsLayout);
    setLayout(mainLayout);
}

void SettingsDialog::saveSettings() {
    Settings::viewerBackgroundColor = imageViewerBackgroundColor;
    Settings::thumbsBackgroundColor = thumbsBackgroundColor;
    Settings::thumbsTextColor = thumbsTextColor;
    Settings::thumbsBackgroundImage = thumbsBackgroundImageLineEdit->text();
    Settings::thumbsPagesReadCount = (unsigned int) thumbPagesSpinBox->value();
    Settings::wrapImageList = wrapListCheckBox->isChecked();
    Settings::defaultSaveQuality = saveQualitySpinBox->value();
    Settings::slideShowDelay = slideDelaySpinBox->value();
    Settings::slideShowRandom = slideRandomCheckBox->isChecked();
    Settings::slideShowCrossfade = slideCrossfadeCheckBox->isChecked();
    Settings::enableAnimations = enableAnimCheckBox->isChecked();
    Settings::exifRotationEnabled = enableExifCheckBox->isChecked();
    Settings::exifThumbRotationEnabled = enableThumbExifCheckBox->isChecked();
    Settings::showImageName = showImageNameCheckBox->isChecked();
    Settings::reverseMouseBehavior = reverseMouseCheckBox->isChecked();
    Settings::scrollZooms = scrollZoomCheckBox->isChecked();
    Settings::deleteConfirm = deleteConfirmCheckBox->isChecked();
    Settings::setWindowIcon = setWindowIconCheckBox->isChecked();
    Settings::upscalePreview = upscalePreviewCheckBox->isChecked();

    if (startupDirectoryRadioButtons[Settings::RememberLastDir]->isChecked()) {
        Settings::startupDir = Settings::RememberLastDir;
    } else if (startupDirectoryRadioButtons[Settings::DefaultDir]->isChecked()) {
        Settings::startupDir = Settings::DefaultDir;
    } else {
        Settings::startupDir = Settings::SpecifiedDir;
        Settings::specifiedStartDir = startupDirLineEdit->text();
    }

    Settings::bangs.clear();
    for (int i = 0; i < bangTable->rowCount() - 1; ++i) {
        QString key = QString("bang_%1").arg(i);
        if (bangTable->item(i,0) && !bangTable->item(i,0)->text().isEmpty())
            key = bangTable->item(i,0)->text();
        QString value;
        if (bangTable->item(i,1))
            value = bangTable->item(i,1)->text();
        Settings::bangs[key] = value;
    }

    accept();
}

void SettingsDialog::abort() {
    reject();
}

void SettingsDialog::pickColor() {
    QColor userColor = QColorDialog::getColor(Settings::viewerBackgroundColor, this);
    if (userColor.isValid()) {
        setButtonBgColor(userColor, backgroundColorButton);
        imageViewerBackgroundColor = userColor;
    }
}

void SettingsDialog::setButtonBgColor(QColor &color, QAbstractButton *button) {
    const int s = qMin(button->width(),button->height());
    QPixmap pix(s, s);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.drawEllipse(pix.rect());
    p.end();
    button->setIcon(pix);
    button->setText(color.name());
}

void SettingsDialog::pickThumbsColor() {
    QColor userColor = QColorDialog::getColor(Settings::thumbsBackgroundColor, this, tr("Select background color"), QColorDialog::ShowAlphaChannel);
    if (userColor.isValid()) {
        setButtonBgColor(userColor, thumbsColorPickerButton);
        thumbsBackgroundColor = userColor;
    }
}

void SettingsDialog::pickThumbsTextColor() {
    QColor userColor = QColorDialog::getColor(Settings::thumbsTextColor, this);
    if (userColor.isValid()) {
        setButtonBgColor(userColor, thumbsLabelColorButton);
        thumbsTextColor = userColor;
    }
}

void SettingsDialog::pickStartupDir() {
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Choose Startup Directory"), "",
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    startupDirLineEdit->setText(dirName);
}

void SettingsDialog::pickBackgroundImage() {
    QString dirName = QFileDialog::getOpenFileName(this, tr("Open File"), "",
                                                   tr("Images") +
                                                   " (*.jpg *.jpeg *.jpe *.png *.bmp *.tiff *.tif *.ppm *.xbm *.xpm)");
    thumbsBackgroundImageLineEdit->setText(dirName);
}
