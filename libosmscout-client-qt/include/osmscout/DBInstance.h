#ifndef OSMSCOUT_CLIENT_QT_DBINSTANCE_H
#define OSMSCOUT_CLIENT_QT_DBINSTANCE_H

/*
 OSMScout - a Qt backend for libosmscout and libosmscout-map
 Copyright (C) 2017 Lukáš Karas

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <osmscout/Database.h>
#include <osmscout/LocationService.h>
#include <osmscout/LocationDescriptionService.h>
#include <osmscout/MapService.h>
#include <osmscout/MapPainterQt.h>

#include <osmscout/util/Breaker.h>

#include <osmscout/ClientQtImportExport.h>

#include <QObject>
#include <QMutex>
#include <QDebug>
#include <QMap>
#include <QThread>
#include <QElapsedTimer>
#include <QMutexLocker>

namespace osmscout {

/**
 * \ingroup QtAPI
 */
class OSMSCOUT_CLIENT_QT_API QBreaker : public osmscout::Breaker
{
private:
  mutable QMutex mutex;
  bool           aborted;

public:
  QBreaker();

  void Break();
  bool IsAborted() const;
  void Reset();
};

/**
 * \ingroup QtAPI
 */
class OSMSCOUT_CLIENT_QT_API StyleError
{
    enum StyleErrorType {
        Symbol, Error, Warning, Exception
    };

public:
    StyleError(StyleErrorType type, int line, int column, const QString &text) :
        type(type), line(line), column(column), text(text){}
    StyleError(QString msg);

    StyleErrorType GetType(){ return type; }
    QString GetTypeName() const;
    int GetLine(){ return line; }
    int GetColumn(){ return column; }
    const QString &GetText(){ return text; }
    QString GetDescription(){return GetTypeName()+": "+GetText();}

private:
    StyleErrorType  type;
    int             line;
    int             column;
    QString         text;
};


/**
 * \ingroup QtAPI
 *
 * Instance of one osmscout database and database specific objects.
 *
 * It is thread safe
 */
class OSMSCOUT_CLIENT_QT_API DBInstance : public QObject
{
  Q_OBJECT

public:
  const QString                           path;

private:
  mutable QMutex                          mutex;
  QMap<QThread*,osmscout::MapPainterQt*>  painterHolder; ///< thread-local cache of map painters, guarded by mutex
  QElapsedTimer                           lastUsage; ///< last time when database was used, guarded by mutex

  osmscout::GeoBox                        dbBox; ///< cached database GeoBox, may be accessed without lock and lastUsage update
  osmscout::DatabaseRef                   database;

  osmscout::LocationServiceRef            locationService;
  osmscout::LocationDescriptionServiceRef locationDescriptionService;
  osmscout::MapServiceRef                 mapService;

  osmscout::StyleConfigRef                styleConfig;

public slots:
  void onThreadFinished();

public:
  inline DBInstance(QString path,
                    osmscout::DatabaseRef database,
                    osmscout::LocationServiceRef locationService,
                    osmscout::LocationDescriptionServiceRef locationDescriptionService,
                    osmscout::MapServiceRef mapService,
                    osmscout::StyleConfigRef styleConfig):
    path(path),
    database(database),
    locationService(locationService),
    locationDescriptionService(locationDescriptionService),
    mapService(mapService),
    styleConfig(styleConfig)
  {
    if (!database->GetBoundingBox(dbBox)){
      osmscout::log.Error() << "Failed to get database GeoBox: " << path.toStdString();
    }
    lastUsage.start();
  };

  inline ~DBInstance()
  {
    close();
  };

  inline osmscout::GeoBox GetDBGeoBox() const
  {
    return dbBox;
  }

  /**
   * return true if database is open
   * lastUsage is not udpated
   */
  inline bool IsOpen() const
  {
    return database->IsOpen();
  }

  inline osmscout::DatabaseRef GetDatabase()
  {
    QMutexLocker locker(&mutex);
    lastUsage.restart();
    return database;
  }

  inline osmscout::MapServiceRef GetMapService()
  {
    QMutexLocker locker(&mutex);
    lastUsage.restart();
    return mapService;
  }

  inline osmscout::LocationDescriptionServiceRef GetLocationDescriptionService()
  {
    QMutexLocker locker(&mutex);
    lastUsage.restart();
    return locationDescriptionService;
  }

  inline osmscout::LocationServiceRef GetLocationService()
  {
    QMutexLocker locker(&mutex);
    lastUsage.restart();
    return locationService;
  }

  inline osmscout::StyleConfigRef GetStyleConfig()
  {
    return styleConfig;
  }

  /**
   * Returns the number of milliseconds since last database usage
   */
  inline qint64 LastUsageMs() const
  {
    QMutexLocker locker(&mutex);
    return lastUsage.elapsed();
  }

  bool LoadStyle(QString stylesheetFilename,
                 std::unordered_map<std::string,bool> stylesheetFlags,
                 QList<StyleError> &errors);

  /**
   * Get or create thread local MapPainter instance for this map
   * \note To make sure that painter will not be destroyed during usage,
   * read-lock for databases should be held.
   * \warning It may be null when styleConfig is not loaded!
   * @return pointer to thread-local painter
   */
  osmscout::MapPainterQt* GetPainter();

  void close();
};

typedef std::shared_ptr<DBInstance> DBInstanceRef;

}

#endif /* OSMSCOUT_CLIENT_QT_DBINSTANCE_H */
