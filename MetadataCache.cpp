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

#include <QMap>
#include <QSet>
#include <exiv2/exiv2.hpp>
#include "Settings.h"
#include "MetadataCache.h"

namespace Metadata {

class ImageMetadata {
public:
    QSet<QString> tags;
    long orientation;
};

static QMap<QString, ImageMetadata> gs_cache;

// updateImageTags
bool updateTags(QString &imageFileName, QSet<QString> tags) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return false;
    it->tags = tags;
    return true;
}

// removeTagFromImage
bool removeTag(QString &imageFileName, const QString &tagName) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return false;
    return it->tags.remove(tagName);
}

// removeImage
void forget(QString &imageFileName) {
    gs_cache.remove(imageFileName);
}

// getImageTags
const QSet<QString> &tags(QString &imageFileName) {
    static QSet<QString> dummy;
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return dummy;
    return it->tags;
}

// getImageOrientation
long orientation(QString &imageFileName) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        cache(imageFileName);
    it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return 0;
    return it->orientation;
}

// setImageTags
void setTags(const QString &imageFileName, QSet<QString> tags) {
    gs_cache[imageFileName].tags = tags;
}

// addTagToImage
bool addTag(QString &imageFileName, QString &tagName) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return false; // no such image
    if (it->tags.contains(tagName))
        return false; // no overwrite

    it->tags.insert(tagName);
    return true;
}

// clear
void dropCache() {
    gs_cache.clear();
}

// loadImageMetadata
void cache(const QString &imageFullPath) {
    if (gs_cache.contains(imageFullPath))
        return;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if EXIV2_TEST_VERSION(0,28,0)
    Exiv2::Image::UniquePtr exifImage;
#else
    Exiv2::Image::AutoPtr exifImage;
#endif
#pragma clang diagnostic pop

    ImageMetadata imageMetadata;
    imageMetadata.orientation = 0;

    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
    } catch (Exiv2::Error &error) {
        qWarning() << "Error loading image for reading metadata" << error.what();
        gs_cache.insert(imageFullPath, imageMetadata);
        return;
    }

    if (!exifImage->good()) {
        gs_cache.insert(imageFullPath, imageMetadata);
        return;
    }

    if (exifImage->supportsMetadata(Exiv2::mdExif)) try {
        Exiv2::ExifData::const_iterator it = Exiv2::orientation(exifImage->exifData());
        if (it != exifImage->exifData().end()) {
#if EXIV2_TEST_VERSION(0,28,0)
            imageMetadata.orientation = it->toUint32();
#else
            imageMetadata.orientation = it->toLong();
#endif
        }
    } catch (Exiv2::Error &error) {
        qWarning() << "Failed to read Exif metadata" << error.what();
    }

    if (exifImage->supportsMetadata(Exiv2::mdIptc)) try {
        Exiv2::IptcData &iptcData = exifImage->iptcData();
        if (!iptcData.empty()) {
            QString key;
            Exiv2::IptcData::iterator end = iptcData.end();

            // Finds the first ID, but we need to loop over the rest in case there are more
            Exiv2::IptcData::iterator iptcIt = iptcData.findId(Exiv2::IptcDataSets::Keywords);
            for (; iptcIt != end; ++iptcIt) {
                if (iptcIt->tag() != Exiv2::IptcDataSets::Keywords) {
                    continue;
                }

                QString tagName = QString::fromUtf8(iptcIt->toString().c_str());
                imageMetadata.tags.insert(tagName);
                Settings::knownTags.insert(tagName);
            }
        }
    } catch (Exiv2::Error &error) {
        qWarning() << "Failed to read Iptc metadata";
    }

    gs_cache.insert(imageFullPath, imageMetadata);
}

} // namespace Metadata