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

#include <QCheckBox>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include "ResizeDialog.h"


ResizeDialog::ResizeDialog(QSize originalSize, QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Scale Image"));
    setWindowIcon(QIcon::fromTheme("transform-scale", QIcon(":/images/phototonic.png")));

    QHBoxLayout *buttonsHbox = new QHBoxLayout;
    QPushButton *okButton = new QPushButton(tr("Scale"));
    connect(okButton, &QPushButton::clicked, [=]() { accept(); });
    okButton->setDefault(true);
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, &QPushButton::clicked, [=]() { reject(); });
    buttonsHbox->addWidget(cancelButton, 1, Qt::AlignRight);
    buttonsHbox->addWidget(okButton, 0, Qt::AlignRight);

    widthSpinBox = new QSpinBox;
    widthSpinBox->setRange(0, originalSize.width() * 10);
    widthSpinBox->setValue(originalSize.width());
    connect(widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(adjustSizes()));
    heightSpinBox = new QSpinBox;
    heightSpinBox->setRange(0, originalSize.height() * 10);
    heightSpinBox->setValue(originalSize.height());
    connect(heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(adjustSizes()));

    QGridLayout *mainGbox = new QGridLayout;
    QLabel *origSizeLab = new QLabel(tr("Current size:"));
    QString imageSizeStr = QString("%1 x %2").arg(originalSize.width()).arg(originalSize.height());
    QLabel *origSizePixelsLab = new QLabel(imageSizeStr);
    QLabel *widthLab = new QLabel(tr("New Width:"));
    QLabel *heightLab = new QLabel(tr("New Height:"));
    QLabel *unitsLab = new QLabel(tr("Units:"));

    QLabel *newSizeLab = new QLabel(tr("New size:"));
    newSizePixelsLabel = new QLabel(imageSizeStr);

    pixelsRadioButton = new QRadioButton(tr("Pixels"));
    pixelsRadioButton->setChecked(true);
    connect(pixelsRadioButton, SIGNAL(toggled(bool)), this, SLOT(setUnits()));

    keepAspect = new QCheckBox(tr("Keep aspect ratio"), this);
    keepAspect->setChecked(true);
    connect(keepAspect, SIGNAL(toggled(bool)), this, SLOT(adjustSizes()));

    QHBoxLayout *radiosHbox = new QHBoxLayout;
    radiosHbox->addStretch(1);
    radiosHbox->addWidget(pixelsRadioButton);
    radiosHbox->addWidget(new QRadioButton(tr("Percent")));

    mainGbox->addWidget(origSizeLab, 2, 2, 1, 1);
    mainGbox->addWidget(origSizePixelsLab, 2, 4, 1, 1);
    mainGbox->addWidget(widthLab, 6, 2, 1, 1);
    mainGbox->addWidget(heightLab, 7, 2, 1, 1);
    mainGbox->addWidget(unitsLab, 3, 2, 1, 1);
    mainGbox->addWidget(widthSpinBox, 6, 4, 1, 2);
    mainGbox->addWidget(heightSpinBox, 7, 4, 1, 2);
    mainGbox->addLayout(radiosHbox, 3, 4, 1, 3);
    mainGbox->addWidget(keepAspect, 5, 2, 1, 3);
    mainGbox->addWidget(newSizeLab, 8, 2, 1, 1);
    mainGbox->addWidget(newSizePixelsLabel, 8, 4, 1, 1);
    mainGbox->setRowStretch(9, 1);
    mainGbox->setColumnStretch(3, 1);

    QVBoxLayout *mainVbox = new QVBoxLayout;
    mainVbox->addLayout(mainGbox);
    mainVbox->addLayout(buttonsHbox);
    setLayout(mainVbox);
    widthSpinBox->setFocus(Qt::OtherFocusReason);

    m_last = m_originalSize = originalSize;
}

QSize ResizeDialog::newSize() {
    return m_last;
}


void ResizeDialog::setUnits() {
    int w = widthSpinBox->value(), h = heightSpinBox->value(); // preserve against range changes
    widthSpinBox->blockSignals(true);
    heightSpinBox->blockSignals(true);
    if (pixelsRadioButton->isChecked()) { // percent -> pixels
        widthSpinBox->setRange(0, m_originalSize.width() * 10);
        heightSpinBox->setRange(0, m_originalSize.height() * 10);
        widthSpinBox->setValue(qRound((m_originalSize.width() * w) / 100.));
        heightSpinBox->setValue(qRound((m_originalSize.height() * h) / 100.0));
        m_last = QSize(widthSpinBox->value(), heightSpinBox->value());
    } else { // pixels -> percent
        widthSpinBox->setRange(0, 100 * 10);
        heightSpinBox->setRange(0, 100 * 10);
        widthSpinBox->setValue(qRound((100.0 * w) / m_originalSize.width()));
        heightSpinBox->setValue(qRound((100.0 * h) / m_originalSize.height()));
        m_last = QSize(qRound((widthSpinBox->value() * m_originalSize.width()) / 100.0),
                       qRound((heightSpinBox->value() * m_originalSize.height()) / 100.0));
    }
    widthSpinBox->blockSignals(false);
    heightSpinBox->blockSignals(false);
}

void ResizeDialog::adjustSizes() {
    widthSpinBox->blockSignals(true);
    heightSpinBox->blockSignals(true);

    if (keepAspect->isChecked()) {
        if (pixelsRadioButton->isChecked()) {
            QSize imageSize = m_originalSize;

            Qt::AspectRatioMode aspect = Qt::KeepAspectRatio;
            if (widthSpinBox->value() > m_last.width() || heightSpinBox->value() > m_last.height())
                aspect = Qt::KeepAspectRatioByExpanding;

            imageSize.scale(widthSpinBox->value(), heightSpinBox->value(), aspect);
            widthSpinBox->setValue(imageSize.width());
            heightSpinBox->setValue(imageSize.height());
        } else {
            if (sender() == heightSpinBox) {
                widthSpinBox->setValue(heightSpinBox->value());
            } else {
                heightSpinBox->setValue(widthSpinBox->value());
            }
        }
    }

    int width = widthSpinBox->value(), height = heightSpinBox->value();
    if (!pixelsRadioButton->isChecked()) {
        width = qRound((width * m_originalSize.width()) / 100.0);
        height = qRound((height * m_originalSize.height()) / 100.0);
    }
    m_last = QSize(width, height);
    newSizePixelsLabel->setText(QString("%1 x %2").arg(width).arg(height));

    widthSpinBox->blockSignals(false);
    heightSpinBox->blockSignals(false);
}
