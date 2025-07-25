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

#ifndef INFO_VIEWER_H
#define INFO_VIEWER_H

class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QStandardItemModel;
class QTableView;

#include <QMap>
#include <QModelIndex>
#include <QWidget>

class InfoView : public QWidget {
Q_OBJECT

public:
    InfoView(QWidget *parent);

    void clear();
    void hint(QString key, QString value);
    QString html() const;
    void read(QString imageFullPath, const QImage &histogram = QImage());
    void reloadExifData();

signals:
    void histogramClicked();
    void exifChanged(const QString &fileName);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private slots:
    void copyEntry();
    void filterItems();
    void removeEntry();
    void saveExifChanges();
    void showInfoViewMenu(QPoint pt);
    void showSaveButton();

private:
    void addEntry(QString key, QString value, bool editable = false);
    void addTitleEntry(QString title);

    QTableView *infoViewerTable;
    QStandardItemModel *imageInfoModel;
    QModelIndex selectedEntry;
    QMenu *infoMenu;
    QComboBox *m_filter;
    QPushButton *m_manageFiltersButton;
    QString m_currentFile;
    QMap<QString, QString> m_hints;
    QLabel *m_histogram;
    QAction *m_removeAction;
    QPushButton *m_saveExifButton;
};

#endif // INFO_VIEWER_H
