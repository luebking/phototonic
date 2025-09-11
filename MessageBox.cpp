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

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include "MessageBox.h"
#include "Phototonic.h"

MessageBox::MessageBox(QWidget *parent, StandardButtons buttons, StandardButton defaultButton) : QMessageBox(parent) {
    setWindowIcon(QIcon(":/images/phototonic.png"));
    if (buttons != NoButton) {
        setStandardButtons(buttons);
        if (defaultButton != NoButton)
            setDefaultButton(defaultButton);
    }
}

int MessageBox::ask(const QString &title, const QString &message) {
    setWindowTitle(title);
    setText(message);
    setIcon(MessageBox::Question);
    return exec();
}

void MessageBox::critical(const QString &title, const QString &message) {
    setWindowTitle(title);
    setText(message);
    setIcon(MessageBox::Critical);
    exec();
}

int MessageBox::warning(const QString &title, const QString &message) {
    setWindowTitle(title);
    setText(message);
    setIcon(MessageBox::Warning);
    return exec();
}

void MessageBox::about() {
    QStringList contributers;
    contributers    // << "Code: Ofer Kashayov" // oferkv@gmail.com
                    << "Alphatests: Standreas (stefonarch)" // github.com/stefonarch
                    << "Code: Christopher Roy Bratusek" // nano@jpberlin.de
                    << "Code: Krzysztof Pyrkosz" // pyrkosz@o2.pl
                    << "Code: Roman Chistokhodov" // freeslave93@gmail.com
                    << "Code: Tung Le (https://github.com/everbot)" //
                    << "Code: Peter Mattern (https://github.com/pmattern)" //
                    << "Code: Thomas Lübking - some patches in 2015 ;)" // thomas.luebking@gmail.com
                    << "Bosnian: Dino Duratović" // dinomol@mail.com
                    << "Chinese: BigELK176 ≡" // <BigELK176@gmail.com>
                    << "Chinese: wenchao-cui" // <cuzz@qq.com>
                    << "Croatian: Dino Duratović" // dinomol@mail.com
                    << "Croatian: milotype" // <mail@milotype.de>
                    << "Czech: Pavel Fric" // pavelfric@seznam.cz
                    << "Dutch: Heimen Stoffels" // <vistausss@fastmail.com>
                    << "Estonian: Priit Jõerüüt" //<lxqtwlate@joeruut.com>
                    << "French: Adrien Daugabel" // adrien.d@mageialinux-online.org
                    << "French: David Geiger" // david.david@mageialinux-online.org
                    << "French: Nicolas PARLANT" // <nicolas.parlant@parhuet.fr>
                    << "French: Oliver Chiasson" // <olivierchiasson@hotmail.fr>
                    << "French: Rémi Verschelde" // akien@mageia.org
                    << "Georgian: NorwayFun" //  <temuri.doghonadze@gmail.com>
                    << "German: EttoreAtalan" // <atalanttore@googlemail.com>
                    << "German: Jonathan Hooverman" // jonathan.hooverman@gmail.com
                    << "German: Some wordstumbling fool" // thomas.luebking@gmail.com
                    << "Italian: Standreas (stefonarch)" // github.com/stefonarch
                    << "Polish: Jan Rolski" // <wbcwknvstb@proton.me>
                    << "Polish: Krzysztof Pyrkosz" // pyrkosz@o2.pl
                    << "Polish: Robert Wojewódzki" // robwoj44@poczta.onet.pl
                    << "Portuguese: Hugo Carvalho" // <hugokarvalho@hotmail.com>
                    << "Portuguese: Marcos M. Nascimento" // wstlmn@uol.com.br
                    << "Russian: Azasis" // <lionovdmitrij386@gmail.com>
                    << "Russian: Dmitry" // <dmitrydmitry761@gmail.com>
                    << "Russian: Ilya Alexandrovich" // yast4ik@gmail.com
                    << "Russian: Valera" // <ValeraDX@yandex.ru>
                    << "Serbian: Dino Duratović" // dinomol@mail.com
                    << "Spanish: Roberalz" // <lumintnian@outlook.com>
//                    << "Swedish: Luna Jernberg" // <droidbittin@gmail.com>
                    << "Swedish: bittin" // <bittin@reimu.nl>
                    << "Ukrainian: Ihor Hordiichuk" // <igor_ck@outlook.com>
                    << "Standreas and the entire LXQt weblate team"
                    ;
    QString aboutString = "<h1>Phototonic 3.1 α¹</h1>" // + QString(VERSION)
                          "<h4>" + tr("Image Viewer and Organizer") + "</h4>"
                          "<a href=\"https://github.com/luebking/phototonic\">" + tr("Home page and bug reports") + "</a>"
                          "<dl><dt>Copyright</dt><dd>"
                          "&copy;2013-2018 Ofer Kashayov<br>"
                          "&copy;2024-2025 Thomas Lübking</dd></dl>"
                        + "<br><p>Using Qt v" + QT_VERSION_STR
                        + "<hr>Phototonic is licensed under the GNU General Public License v3&nbsp;</p>"
                        + tr("Special thanks to our contributers.");


    setWindowTitle(tr("About"));
    setIconPixmap(QIcon(":/images/phototonic.png").pixmap(128, 128));
    setText(aboutString);
    setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);
    setInformativeText(contributers.at(0));
    QWidget *magicLabel = findChild<QWidget*>("qt_msgbox_informativelabel");
    QPropertyAnimation *magicAnimation = nullptr;
    if (magicLabel) {
        QGraphicsOpacityEffect *magic = new QGraphicsOpacityEffect(magicLabel);
        magicLabel->setGraphicsEffect(magic);
        magic->setOpacity(1);
        magicAnimation = new QPropertyAnimation(magic, "opacity", magic);
        magicAnimation->setDuration(125);
    }
    int idx = 0;
    QTimer *t = new QTimer(this);
    t->setInterval(3000);
    connect (t, &QTimer::timeout, [=,&idx]() {
        idx = (idx + 1) % contributers.size();
        if (magicAnimation) {
            magicAnimation->setStartValue(1);
            magicAnimation->setEndValue(0);
            magicAnimation->setEasingCurve(QEasingCurve::OutQuad);
            magicAnimation->start();
        }
        QTimer::singleShot(125, this, [=]() {
            setInformativeText(contributers.at(idx));
            if (magicAnimation) {
                magicAnimation->setStartValue(0);
                magicAnimation->setEndValue(1);
                magicAnimation->setEasingCurve(QEasingCurve::InQuad);
                magicAnimation->start();
            }
        });
        });
    t->start();
    exec();
}
