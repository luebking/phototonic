/*
 *  Copyright (C) 2013-2015 Ofer Kashayov <oferkv@live.com>
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

#ifndef META_DATA_CACHE_H
#define META_DATA_CACHE_H

#include <QTransform>

namespace Metadata {
    bool addTag(QString &imageFileName, QString &tagName);
    void cache(const QString &imageFullPath);
    void dropCache();
    QTransform transformation(QString &imageFullPath);
    void forget(QString &imageFileName);
    long orientation(QString &imageFileName);
    bool removeTag(QString &imageFileName, const QString &tagName);
    void setTags(const QString &imageFileName, QSet<QString> tags);
    const QSet<QString> &tags(QString &imageFileName);
    bool updateTags(QString &imageFileName, QSet<QString> tags);
};

#endif // META_DATA_CACHE_H

