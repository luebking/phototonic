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
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include "CropDialog.h"
#include "Settings.h"

CropDialog::CropDialog(QWidget *parent) : QDialog(parent) {
    //: Like a bad dvd where you get a black frame around the image
    setWindowTitle(tr("Letterbox"));
    setWindowIcon(QIcon(":/images/crop.png"));
    resize(350, 100);
    if (Settings::dialogLastX)
        move(Settings::dialogLastX, Settings::dialogLastY);

    QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *resetButton = new QPushButton(tr("Reset"));
    connect(resetButton, SIGNAL(clicked()), this, SLOT(reset()));
    QPushButton *okButton = new QPushButton(tr("OK"));
    connect(okButton, SIGNAL(clicked()), this, SLOT(ok()));
    okButton->setDefault(true);
    buttonsHbox->addWidget(resetButton, 0, Qt::AlignLeft);
    buttonsHbox->addWidget(okButton, 0, Qt::AlignRight);

    QSlider *topSlide = nullptr, *bottomSlide = nullptr,  *leftSlide = nullptr, *rightSlide = nullptr;
    auto setupSliders = [=](QSlider **sliderp, QSpinBox **spinboxp) {
        QSlider *slider = new QSlider(Qt::Horizontal, this);
        slider->setTickPosition(QSlider::TicksAbove);
        slider->setTickInterval(10);
        slider->setTracking(true);
        slider->setRange(0, 100);
        QSpinBox *spinbox = new QSpinBox(this);
        spinbox->setPrefix("% ");
        spinbox->setRange(0, 100);
        connect(slider, &QSlider::valueChanged, spinbox, &QSpinBox::setValue);
        connect(spinbox, &QSpinBox::valueChanged, slider, &QSlider::setValue);
        connect(spinbox, &QSpinBox::valueChanged, this, &CropDialog::emitValues);
        *sliderp = slider;
        *spinboxp = spinbox;
    };

    setupSliders(&leftSlide, &leftSpinBox);
    setupSliders(&topSlide, &topSpinBox);
    setupSliders(&rightSlide, &rightSpinBox);
    setupSliders(&bottomSlide, &bottomSpinBox);

    QGridLayout *mainGbox = new QGridLayout;
    mainGbox->addWidget(new QLabel(tr("Left"), this), 0, 0, 1, 1);
    mainGbox->addWidget(leftSlide, 0, 1, 1, 1);
    mainGbox->addWidget(leftSpinBox, 0, 2, 1, 1);

    mainGbox->addWidget(new QLabel(tr("Right"), this), 1, 0, 1, 1);
    mainGbox->addWidget(rightSlide, 1, 1, 1, 1);
    mainGbox->addWidget(rightSpinBox, 1, 2, 1, 1);

    mainGbox->addWidget(new QLabel(tr("Top"), this), 2, 0, 1, 1);
    mainGbox->addWidget(topSlide, 2, 1, 1, 1);
    mainGbox->addWidget(topSpinBox, 2, 2, 1, 1);

    mainGbox->addWidget(new QLabel(tr("Bottom"), this), 3, 0, 1, 1);
    mainGbox->addWidget(bottomSlide, 3, 1, 1, 1);
    mainGbox->addWidget(bottomSpinBox, 3, 2, 1, 1);


    QVBoxLayout *mainVbox = new QVBoxLayout;
    mainVbox->addLayout(mainGbox);
    mainVbox->addLayout(buttonsHbox);
    setLayout(mainVbox);
}

void CropDialog::emitValues()
{
    if (sender() == leftSpinBox && leftSpinBox->value() + rightSpinBox->value() > 100)
        rightSpinBox->setValue(100-leftSpinBox->value());
    if (sender() == rightSpinBox && leftSpinBox->value() + rightSpinBox->value() > 100)
        leftSpinBox->setValue(100-rightSpinBox->value());
    if (sender() == topSpinBox && topSpinBox->value() + bottomSpinBox->value() > 100)
        bottomSpinBox->setValue(100-topSpinBox->value());
    if (sender() == bottomSpinBox && topSpinBox->value() + bottomSpinBox->value() > 100)
        topSpinBox->setValue(100-bottomSpinBox->value());
    emit valuesChanged(leftSpinBox->value(), topSpinBox->value(), rightSpinBox->value(), bottomSpinBox->value());
}

void CropDialog::ok() {
    Settings::dialogLastX = pos().x();
    Settings::dialogLastY = pos().y();
    accept();
}

void CropDialog::reset() {
    leftSpinBox->setValue(0);
    rightSpinBox->setValue(0);
    topSpinBox->setValue(0);
    bottomSpinBox->setValue(0);
}