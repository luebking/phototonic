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

#include <QBuffer>
#include <QDateTime>
#include <QImageReader>
#include <QImageWriter>
#include <QMap>
#include <QSet>
#include <exiv2/exiv2.hpp>
#include "Settings.h"
#include "MetadataCache.h"

namespace Metadata {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#if EXIV2_TEST_VERSION(0,28,0)
    #define Exiv2ImagePtr Exiv2::Image::UniquePtr
    #define Exiv2ValuePtr Exiv2::Value::UniquePtr
#else
    #define Exiv2ImagePtr Exiv2::Image::AutoPtr
    #define Exiv2ValuePtr Exiv2::Value::AutoPtr
#endif
#pragma clang diagnostic pop

class ImageMetadata {
public:
    QSet<QString> tags;
    long orientation;
    qint64 date;
};

static QMap<QString, ImageMetadata> gs_cache;

// updateImageTags
bool updateTags(const QString &imageFileName, QSet<QString> tags) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return false;
    it->tags = tags;
    return true;
}

// removeTagFromImage
bool removeTag(const QString &imageFileName, const QString &tagName) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return false;
    return it->tags.remove(tagName);
}

// removeImage
void forget(const QString &imageFileName) {
    gs_cache.remove(imageFileName);
}

void rename(const QString &oldImageFileName, const QString &newImageFileName) {
    gs_cache.insert(newImageFileName, gs_cache.take(oldImageFileName));
}

// getImageTags
const QSet<QString> &tags(const QString &imageFileName) {
    static QSet<QString> dummy;
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return dummy;
    return it->tags;
}

QImage thumbnail(const QString &imageFullPath) {
    Exiv2ImagePtr exifImage;
    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
    } catch (Exiv2::Error &error) {
        qWarning() << "Error loading image for reading metadata" << error.what();
        return QImage();
    }
    Exiv2::ExifThumbC thumbc(exifImage->exifData());
    Exiv2::DataBuf dbuf = thumbc.copy();
    QBuffer qbuf;
    qbuf.setData(dbuf.c_str(), dbuf.size());
    return QImageReader(&qbuf).read();
}

bool setThumbnail(const QString &imageFullPath, QImage thumbnail) {
    if (thumbnail.width() < 64 && thumbnail.height() < 64) {
        qWarning() << "Not writing tiny thumbnail" << thumbnail.size() << imageFullPath;
        return false;
    }
    Exiv2ImagePtr exifImage;
    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
    } catch (Exiv2::Error &error) {
        qWarning() << "Error loading image for reading metadata" << error.what();
        return false;
    }
    if (thumbnail.width() > 256 || thumbnail.height() > 256)
        thumbnail = thumbnail.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QBuffer qbuf;
    QImageWriter writer(&qbuf, "jpeg");
    writer.setOptimizedWrite(true);
    writer.setProgressiveScanWrite(true);
    writer.setQuality(50);
    writer.write(thumbnail);
    Exiv2::ExifThumb image(exifImage->exifData());
    image.setJpegThumbnail(reinterpret_cast<const Exiv2::byte*>(qbuf.data().constData()), qbuf.size());
    exifImage->writeMetadata();
    return true;
}

// getImageOrientation
long orientation(const QString &imageFileName) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        cache(imageFileName);
    it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return 0;
    return it->orientation;
}

qint64 dateTimeOriginal(const QString &imageFileName) {
    QMap<QString, ImageMetadata>::iterator it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        cache(imageFileName);
    it = gs_cache.find(imageFileName);
    if (it == gs_cache.end())
        return 0;
    return it->date;
}

// setImageTags
void setTags(const QString &imageFileName, QSet<QString> tags) {
    gs_cache[imageFileName].tags = tags;
}

// addTagToImage
bool addTag(const QString &imageFileName, const QString &tagName) {
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

QTransform transformation(const QString &imageFullPath) {
    QTransform trans;
    switch (orientation(imageFullPath)) {
        case 1:
            break;
        case 2:
            trans.scale(-1, 1);
//            image = image.transformed(trans, Qt::SmoothTransformation);
///            image = image.mirrored(true, false);
            break;
        case 3:
            trans.rotate(180);
//            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 4:
            trans.scale(1, -1);
//            image.mirror(false, true);
            break;
        case 5:
            trans.scale(-1, 1);
            trans.rotate(90);
//            image = image.transformed(trans, Qt::SmoothTransformation);
///            image.mirror(true, false);
            break;
        case 6:
            trans.rotate(90);
//            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 7:
            trans.scale(1, -1);
            trans.rotate(90);
//            image = image.transformed(trans, Qt::SmoothTransformation);
///            image.mirror(false, true);
            break;
        case 8:
            trans.rotate(270);
//            image = image.transformed(trans, Qt::SmoothTransformation);
///            image.mirror(true, false);
            break;
        default:
            break;
    }
    return trans;
}

// loadImageMetadata
void cache(const QString &imageFullPath) {
    if (gs_cache.contains(imageFullPath))
        return;

    Exiv2ImagePtr exifImage;
    ImageMetadata imageMetadata;
    imageMetadata.orientation = 0;
    imageMetadata.date = 0;

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
        Exiv2::ExifData &exifData = exifImage->exifData();
        Exiv2::ExifData::const_iterator end = exifData.end();
        for (Exiv2::ExifData::const_iterator md = exifData.begin(); md != end; ++md) {
            if (!strncmp(md->tagName().c_str(), "DateTime", 8)) {
                imageMetadata.date = QDateTime::fromString(QLatin1String(md->print().c_str()),
                                                           QLatin1String("yyyy:MM:dd hh:mm:ss")).toSecsSinceEpoch(); // "2009:06:28 17:06:56"
                break;
            }
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
//                Settings::knownTags.insert(tagName);
            }
        }
    } catch (Exiv2::Error &error) {
        qWarning() << "Failed to read Iptc metadata";
    }

    gs_cache.insert(imageFullPath, imageMetadata);
}

void data(const QString &imageFullPath, Metadata::DataTriple *EXIF, Metadata::DataTriple *IPTC, Metadata::DataTriple *XMP) {

    Exiv2ImagePtr exifImage;
    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
    }
    catch (const Exiv2::Error &error) {
        qWarning() << "EXIV2:" << error.what();
        return;
    }

//#define EXIV2_ENTRY QString::fromUtf8(md->tagName().c_str()), QString::fromUtf8(md->print().c_str())
#define INSERT(_MAP_) const QString p = QString::fromUtf8(md->print().c_str()), r = QString::fromUtf8(md->toString().c_str()); \
    _MAP_->insert(QString::fromUtf8(md->tagName().c_str()), QPair<QString,QString>(p, p == r ? p : r))
    if (EXIF) {
        Exiv2::ExifData &exifData = exifImage->exifData();
        if (!exifData.empty()) {
            Exiv2::ExifData::const_iterator end = exifData.end();
            for (Exiv2::ExifData::const_iterator md = exifData.begin(); md != end; ++md) {
                INSERT(EXIF);
            }
        }
    }

    if (IPTC) {
        Exiv2::IptcData &iptcData = exifImage->iptcData();
        if (!iptcData.empty()) {
            Exiv2::IptcData::iterator end = iptcData.end();
            for (Exiv2::IptcData::iterator md = iptcData.begin(); md != end; ++md) {
                INSERT(IPTC);
            }
        }
    }

    if (XMP) {
        Exiv2::XmpData &xmpData = exifImage->xmpData();
        if (!xmpData.empty()) {
            Exiv2::XmpData::iterator end = xmpData.end();
            for (Exiv2::XmpData::iterator md = xmpData.begin(); md != end; ++md) {
                INSERT(XMP);
            }
        }
    }
}

bool gpsData(const QString &imageFullPath, double &lat, double &lon, double &alt) {
    lat = lon = alt = 0.0;
    Exiv2ImagePtr exifImage;
    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
    }
    catch (const Exiv2::Error &error) {
        qWarning() << "EXIV2:" << error.what();
        return false;
    }
    Exiv2::ExifData &exifData = exifImage->exifData();
    if (exifData.empty())
        return false;

    auto rat2dec = [=](const QString s) {
        QStringList t = s.split("/", Qt::SkipEmptyParts);
        if (t.size() != 2)
            return 0.0; // wtf
        const double c = t.at(0).toDouble();
        const double d = t.at(1).toDouble();
        if (d != 0.0)
            return c/d;
        return 0.0; // nan
    };
    auto ratarc2dec = [=](const QString s) {
        QStringList t = s.split(" ", Qt::SkipEmptyParts);
        double val = 0.0;
        if (t.size() > 0)
            val = rat2dec(t.at(0));
        if (t.size() > 1)
            val += rat2dec(t.at(1))/60.0;
        if (t.size() > 2)
            val += rat2dec(t.at(2))/3600.0;
        return val;
    };

    bool south(false), west(false), nautilus(false);
    int flags = (1|2|4|8|16|32);
    Exiv2::ExifData::const_iterator end = exifData.end();
    for (Exiv2::ExifData::const_iterator md = exifData.begin(); md != end; ++md) {
        const QString key = QString::fromUtf8(md->tagName().c_str());
        if (key == "GPSLatitude") {
            flags &= ~1;
            lat = ratarc2dec(QString::fromUtf8(md->toString().c_str()));
        } else if (key == "GPSLatitudeRef") {
            flags &= ~2;
            south = QString::fromUtf8(md->toString().c_str()).toLower() == "s";
        } else if (key == "GPSLongitude") {
            flags &= ~4;
            lon = ratarc2dec(QString::fromUtf8(md->toString().c_str()));
        } else if (key == "GPSLongitudeRef") {
            flags &= ~8;
            west = QString::fromUtf8(md->toString().c_str()).toLower() == "w";
        } else if (key == "GPSAltitude") {
            flags &= ~16;
            alt = QString::fromUtf8(md->toString().c_str()).toDouble();
        } else if (key == "GPSAltitudeRef") {
            flags &= ~32;
            nautilus = QString::fromUtf8(md->toString().c_str()) == "1";
        }
        if (!flags)
            break;
    }
    if (south)
        lat = -lat;
    if (west)
        lon = -lon;
    if (nautilus)
        alt = -alt;

    return flags != (1|2|4|8|16|32);
}

template <typename Ev2D> static void writeBack(Ev2D &data, Metadata::DataPair newData) {
    if (data.empty())
        return;
    if (newData.isEmpty()) {
        data.clear();
        return;
    }
    DataPair phase2;
    typename Ev2D::iterator md = data.begin();
    while (md != data.end()) {
        const QString tagName = QString::fromUtf8(md->tagName().c_str());
        const QString value = QString::fromUtf8(md->print().c_str());
        DataPair::const_iterator cit = newData.constFind(tagName, value);
        if (cit != newData.constEnd()) {
            newData.erase(cit); // the tag exists in its current form, keep
            ++md;
        } else if (newData.constFind(tagName) == newData.constEnd()) {
            md = data.erase(md); // there's no such tag left, delete
        } else {
            phase2.insert(tagName, value); // edit in second pass
            ++md;
        }
    }
    if (phase2.isEmpty())
        return;
    md = data.begin();
    while (md != data.end()) {
        const QString tagName = QString::fromUtf8(md->tagName().c_str());
        const QString value = QString::fromUtf8(md->print().c_str());
        DataPair::const_iterator cit = phase2.constFind(tagName, value);
        if (cit == phase2.constEnd()) {
            ++md; continue; // not a concern anyway
        }
        phase2.erase(cit); // we're gonna handle this pair now:
        cit = newData.constFind(tagName); // do we still have such tag (w/ different value)
        if (cit == newData.constEnd()) {
            md = data.erase(md); // ultimately stale
        } else {
            if (value == *cit) // this is not supposed to happen
                qWarning() << "errr… WHAT?!";
            if (md->setValue(cit->toStdString()))
                qWarning() << "could not set" << cit.key() << "from" << md->toString() << " to " << *cit;
            newData.erase(cit); // we've stored this pair now - or ultimately failed because of invalid form…
            ++md;
        }
        if (phase2.isEmpty())
            return;
    }
}

bool setData(const QString &imageFullPath, Metadata::DataPair EXIF, Metadata::DataPair IPTC, Metadata::DataPair XMP) {

    Exiv2ImagePtr exifImage;
    try {
        exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
        exifImage->readMetadata();
        writeBack<Exiv2::ExifData>(exifImage->exifData(), EXIF);
        writeBack<Exiv2::IptcData>(exifImage->iptcData(), IPTC);
        writeBack<Exiv2::XmpData>(exifImage->xmpData(), XMP);
        exifImage->writeMetadata();
        Metadata::forget(imageFullPath);
        cache(imageFullPath);
    }
    catch (Exiv2::Error &error) {
        qWarning() << "Failed to write metadata"  << error.what();
        return false;
    }

    return true;
}

bool wipeFrom(const QString &imageFileName) {

    Exiv2ImagePtr image;
    try {
        image = Exiv2::ImageFactory::open(imageFileName.toStdString());
        image->clearMetadata();
        image->writeMetadata();
        Metadata::forget(imageFileName);
    }
    catch (Exiv2::Error &error) {
        return false;
    }
    return true;
}

bool write(const QString &imageFileName) {
    const QSet<QString> &newTags = tags(imageFileName);

    Exiv2ImagePtr exifImage;
    try {
        exifImage = Exiv2::ImageFactory::open(imageFileName.toStdString());
        exifImage->readMetadata();

        Exiv2::IptcData newIptcData;

        /* copy existing data */
        Exiv2::IptcData &iptcData = exifImage->iptcData();
        if (!iptcData.empty()) {
            QString key;
            Exiv2::IptcData::iterator end = iptcData.end();
            for (Exiv2::IptcData::iterator iptcIt = iptcData.begin(); iptcIt != end; ++iptcIt) {
                if (iptcIt->tagName() != "Keywords") {
                    newIptcData.add(*iptcIt);
                }
            }
        }

        /* add new tags */
        QSetIterator<QString> newTagsIt(newTags);
        while (newTagsIt.hasNext()) {
            QString tag = newTagsIt.next();
            Exiv2ValuePtr value = Exiv2::Value::create(Exiv2::string);
            value->read(tag.toStdString());
            Exiv2::IptcKey key("Iptc.Application2.Keywords");
            newIptcData.add(key, value.get());
        }

        exifImage->setIptcData(newIptcData);
        exifImage->writeMetadata();
    }
    catch (Exiv2::Error &error) {
        return false;
    }

    return true;
}

} // namespace Metadata