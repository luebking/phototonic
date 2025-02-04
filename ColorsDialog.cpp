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

#include <QBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QToolTip>

#include "ImageViewer.h"
#include "ColorsDialog.h"
#include "Settings.h"

ColorsDialog::ColorsDialog(QWidget *parent, ImageViewer *imageViewer) : QDialog(parent), dontApply(false) {
    setWindowTitle(tr("Colors"));
    setWindowIcon(QIcon(":/images/colors.png"));
    resize(350, 300);
    this->imageViewer = imageViewer;

    QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *resetButton = new QPushButton(tr("Reset"));
    resetButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(resetButton, SIGNAL(clicked()), this, SLOT(reset()));
    buttonsHbox->addWidget(resetButton, 0, Qt::AlignLeft);
    QPushButton *okButton = new QPushButton(tr("OK"));
    okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(okButton, SIGNAL(clicked()), this, SLOT(ok()));
    buttonsHbox->addWidget(okButton, 0, Qt::AlignRight);
    okButton->setDefault(true);

    auto makeSlider = [=](QString label, QSlider **slider) {
        *slider = new QSlider(Qt::Horizontal);
        (*slider)->setTickPosition(QSlider::TicksAbove);
        (*slider)->setTickInterval(25);
        (*slider)->setRange(-100, 100);
        (*slider)->setTracking(false);
        (*slider)->setToolTip("0");
        connect(*slider, &QSlider::valueChanged, [=](int v) {
            (*slider)->setToolTip(QString::number(v));
            applyColors();
        });
        connect(*slider, &QSlider::sliderMoved, [=](int v) {
            const QString n = QString::number(v);
            (*slider)->setToolTip(n);
            QToolTip::showText(QCursor::pos(), n, nullptr, QRect(), 0);
        });
        QAction *zero = new QAction(*slider);
        zero->setShortcut(Qt::Key_0);
        zero->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(zero, &QAction::triggered, [=]() { (*slider)->setValue(0); });
        (*slider)->addAction(zero);
        return new QLabel(label);
    };

    /* hue saturation */
    QLabel *hueLab = makeSlider(tr("Hue"), &hueSlider);
    QLabel *satLab = makeSlider(tr("Saturation"), &saturationSlider);
    QLabel *lightLab = makeSlider(tr("Lightness"), &lightnessSlider);


    colorizeCheckBox = new QCheckBox(tr("Colorize"), this);
    colorizeCheckBox->setCheckState(Settings::colorizeEnabled ? Qt::Checked : Qt::Unchecked);
    connect(colorizeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(applyColors()));

    rNegateCheckBox = new QCheckBox(tr("Negative"), this);
    rNegateCheckBox->setCheckState(Settings::rNegateEnabled ? Qt::Checked : Qt::Unchecked);
    connect(rNegateCheckBox, SIGNAL(stateChanged(int)), this, SLOT(applyColors()));

    gNegateCheckBox = new QCheckBox(tr("Negative"), this);
    gNegateCheckBox->setCheckState(Settings::gNegateEnabled ? Qt::Checked : Qt::Unchecked);
    connect(gNegateCheckBox, SIGNAL(stateChanged(int)), this, SLOT(applyColors()));

    bNegateCheckBox = new QCheckBox(tr("Negative"), this);
    bNegateCheckBox->setCheckState(Settings::bNegateEnabled ? Qt::Checked : Qt::Unchecked);
    connect(bNegateCheckBox, SIGNAL(stateChanged(int)), this, SLOT(applyColors()));


    QHBoxLayout *channelsHbox = new QHBoxLayout;
    redCheckBox = new QCheckBox(tr("Red"));
    redCheckBox->setCheckable(true);
    redCheckBox->setChecked(Settings::hueRedChannel);
    connect(redCheckBox, SIGNAL(clicked()), this, SLOT(applyColors()));
    channelsHbox->addWidget(redCheckBox, 0, Qt::AlignLeft);
    greenCheckBox = new QCheckBox(tr("Green"));
    greenCheckBox->setCheckable(true);
    greenCheckBox->setChecked(Settings::hueGreenChannel);
    connect(greenCheckBox, SIGNAL(clicked()), this, SLOT(applyColors()));
    channelsHbox->addWidget(greenCheckBox, 0, Qt::AlignLeft);
    blueCheckBox = new QCheckBox(tr("Blue"));
    blueCheckBox->setCheckable(true);
    blueCheckBox->setChecked(Settings::hueBlueChannel);
    connect(blueCheckBox, SIGNAL(clicked()), this, SLOT(applyColors()));
    channelsHbox->addWidget(blueCheckBox, 0, Qt::AlignLeft);
    channelsHbox->addStretch(1);

    QGridLayout *hueSatLay = new QGridLayout;
    hueSatLay->addWidget(hueLab, 1, 0, 1, 1);
    hueSatLay->addWidget(hueSlider, 1, 1, 1, 1);
    hueSatLay->addWidget(colorizeCheckBox, 2, 1, 1, 1);
    hueSatLay->addWidget(satLab, 3, 0, 1, 1);
    hueSatLay->addWidget(saturationSlider, 3, 1, 1, 1);
    hueSatLay->addWidget(lightLab, 4, 0, 1, 1);
    hueSatLay->addWidget(lightnessSlider, 4, 1, 1, 1);
    hueSatLay->setColumnMinimumWidth(0, 70);

    QGroupBox *hueSatGroup = new QGroupBox(tr("Hue and Saturation"));
    hueSatGroup->setLayout(hueSatLay);

    QGridLayout *channelsLay = new QGridLayout;
    channelsLay->addLayout(channelsHbox, 5, 1, 1, 1);
    channelsLay->setColumnMinimumWidth(0, 70);
    QGroupBox *channelsGroup = new QGroupBox(tr("Affected Channels"));
    channelsGroup->setLayout(channelsLay);

    /* brightness contrast */
    QLabel *brightLab = makeSlider(tr("Brightness"), &brightSlider);
    QLabel *contrastLab = makeSlider(tr("Contrast"), &contrastSlider);
    contrastSlider->setInvertedAppearance(true);


    QGridLayout *brightContrastbox = new QGridLayout;
    brightContrastbox->addWidget(brightLab, 1, 0, 1, 1);
    brightContrastbox->addWidget(brightSlider, 1, 1, 1, 1);
    brightContrastbox->addWidget(contrastLab, 2, 0, 1, 1);
    brightContrastbox->addWidget(contrastSlider, 2, 1, 1, 1);
    brightContrastbox->setColumnMinimumWidth(0, 70);

    QGroupBox *brightContrastGroup = new QGroupBox(tr("Brightness and Contrast"));
    brightContrastGroup->setLayout(brightContrastbox);

    /* Channel mixer */
    QLabel *redLab = makeSlider(tr("Red"), &redSlider);
    QLabel *greenLab = makeSlider(tr("Green"), &greenSlider);
    QLabel *blueLab = makeSlider(tr("Blue"), &blueSlider);

    QGridLayout *channelMixbox = new QGridLayout;
    channelMixbox->addWidget(redLab, 1, 0, 1, 1);
    channelMixbox->addWidget(redSlider, 1, 1, 1, 1);
    channelMixbox->addWidget(rNegateCheckBox, 1, 2, 1, 1);
    channelMixbox->addWidget(greenLab, 2, 0, 1, 1);
    channelMixbox->addWidget(greenSlider, 2, 1, 1, 1);
    channelMixbox->addWidget(gNegateCheckBox, 2, 2, 1, 1);
    channelMixbox->addWidget(blueLab, 3, 0, 1, 1);
    channelMixbox->addWidget(blueSlider, 3, 1, 1, 1);
    channelMixbox->addWidget(bNegateCheckBox, 3, 2, 1, 1);
    channelMixbox->setColumnMinimumWidth(0, 70);

    QGroupBox *channelMixGroup = new QGroupBox(tr("Color Balance"));
    channelMixGroup->setLayout(channelMixbox);

    QVBoxLayout *mainVbox = new QVBoxLayout;
    mainVbox->addWidget(brightContrastGroup);
    mainVbox->addWidget(channelMixGroup);
    mainVbox->addWidget(hueSatGroup);
    mainVbox->addWidget(channelsGroup);
    mainVbox->addStretch(1);
    mainVbox->addLayout(buttonsHbox);
    setLayout(mainVbox);
}

void ColorsDialog::applyColors() {
    if (dontApply)
        return;

    if (brightSlider->value() >= 0) {
        Settings::brightVal = (brightSlider->value() * 500 / 100) + 100;
    } else {
        Settings::brightVal = brightSlider->value() + 100;
    }

    if (contrastSlider->value() >= 0) {
        Settings::contrastVal = (contrastSlider->value() * 79 / 100) + 78;
    } else {
        Settings::contrastVal = contrastSlider->value() + 79;
    }

    Settings::hueVal = hueSlider->value() * 127 / 100;

    if (saturationSlider->value() >= 0) {
        Settings::saturationVal = (saturationSlider->value() * 500 / 100) + 100;
    } else {
        Settings::saturationVal = saturationSlider->value() + 100;
    }

    if (lightnessSlider->value() >= 0) {
        Settings::lightnessVal = (lightnessSlider->value() * 200 / 100) + 100;
    } else {
        Settings::lightnessVal = lightnessSlider->value() + 100;
    }

    Settings::redVal = redSlider->value();
    Settings::greenVal = greenSlider->value();
    Settings::blueVal = blueSlider->value();

    Settings::colorizeEnabled = colorizeCheckBox->isChecked();
    Settings::rNegateEnabled = rNegateCheckBox->isChecked();
    Settings::gNegateEnabled = gNegateCheckBox->isChecked();
    Settings::bNegateEnabled = bNegateCheckBox->isChecked();
    Settings::hueRedChannel = redCheckBox->isChecked();
    Settings::hueGreenChannel = greenCheckBox->isChecked();
    Settings::hueBlueChannel = blueCheckBox->isChecked();

    imageViewer->refresh();
}

void ColorsDialog::ok() {
    Settings::dialogLastX = pos().x();
    Settings::dialogLastY = pos().y();
    accept();
}

void ColorsDialog::reset() {
    dontApply = true;
    hueSlider->setValue(0);
    colorizeCheckBox->setChecked(false);
    rNegateCheckBox->setChecked(false);
    gNegateCheckBox->setChecked(false);
    bNegateCheckBox->setChecked(false);
    saturationSlider->setValue(0);
    lightnessSlider->setValue(0);
    redCheckBox->setChecked(true);
    greenCheckBox->setChecked(true);
    blueCheckBox->setChecked(true);
    Settings::hueRedChannel = true;
    Settings::hueGreenChannel = true;
    Settings::hueBlueChannel = true;

    contrastSlider->setValue(0);
    brightSlider->setValue(0);

    redSlider->setValue(0);
    greenSlider->setValue(0);
    blueSlider->setValue(0);
    dontApply = false;
    if (isVisible())
        applyColors();
}
