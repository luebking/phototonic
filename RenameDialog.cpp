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
#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "RenameDialog.h"

RenameDialog::RenameDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Rename Image"));

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    QPushButton *okButton = new QPushButton(tr("Rename"));
    connect(okButton, SIGNAL(clicked()), this, SLOT(ok()));
    okButton->setDefault(true);

    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(abort()));
    buttonsLayout->addWidget(cancelButton, 1, Qt::AlignRight);
    buttonsLayout->addWidget(okButton, 0, Qt::AlignRight);

    QHBoxLayout *renameLayout = new QHBoxLayout;
    QLabel *label = new QLabel(tr("New name: "));
    m_fileName = new QLineEdit();
    m_fileName->setMinimumWidth(200);
    renameLayout->addWidget(label);
    renameLayout->addWidget(m_fileName);

    m_patternHint = new QLabel(tr("<h3>Rename files according to pattern</h3>"
    "Supported placeholders:<hr>"
    "<table><tr><th align=right>%index :</th><td>0-padded index, based on selection order</td></tr>"
    "<tr><th align=right>%date :</th><td>Date and time of last modification, ISO8601 format</td></tr>"
    "<tr><th align=right>%exifdate :</th><td>Date and time of EXIF timestamp or file creation, ISO8601 format</td></tr>"
    "<tr><th align=right>%size :</th><td>Image size, WxH</td></tr></table>"), this);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_patternHint);
    mainLayout->addLayout(renameLayout);
    mainLayout->addLayout(buttonsLayout);
    setLayout(mainLayout);
    setWindowIcon(QIcon(":/images/phototonic.png"));
    setMinimumWidth(480);
}

void RenameDialog::ok() {
    accept();
}

void RenameDialog::abort() {
    reject();
}

void RenameDialog::setFileName(QString name) {
    m_patternHint->setVisible(name.isEmpty());
    m_fileName->setText(name);
    m_fileName->setSelection(0, name.lastIndexOf("."));
}

QString RenameDialog::fileName() const {
    return m_fileName->text();
}
