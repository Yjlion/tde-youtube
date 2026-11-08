/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "thumbnailloader.h"

#include <tqdir.h>
#include <tqfile.h>
#include <tqimage.h>
#include <tqtimer.h>

#include <kstandarddirs.h>
#include <tdeglobal.h>
#include <kurl.h>
#include <tdeio/job.h>

// Number of simultaneous downloads.
static const int MAX_JOBS = 6;

ThumbnailLoader *ThumbnailLoader::self()
{
    static ThumbnailLoader *s = 0;
    if ( !s )
        s = new ThumbnailLoader;
    return s;
}

ThumbnailLoader::ThumbnailLoader()
    : TQObject( 0, "ThumbnailLoader" )
{
}

TQString ThumbnailLoader::cacheDir() const
{
    return TDEGlobal::dirs()->saveLocation( "cache", "tde-youtube/thumbs/", true );
}

TQPixmap ThumbnailLoader::scaled( const TQImage &img ) const
{
    TQSize s = thumbSize();
    // Crop to 16:9 (some thumbnails are letter-boxed 4:3) and scale smoothly.
    TQImage i = img;
    int wantH = i.width() * 9 / 16;
    if ( wantH > 0 && wantH < i.height() )
        i = i.copy( 0, ( i.height() - wantH ) / 2, i.width(), wantH );
    return TQPixmap( i.smoothScale( s.width(), s.height() ) );
}

TQPixmap ThumbnailLoader::thumbnail( const TQString &id )
{
    if ( id.isEmpty() )
        return TQPixmap();

    TQMap<TQString, TQPixmap>::ConstIterator it = m_cache.find( id );
    if ( it != m_cache.end() )
        return *it;

    TQString file = cacheDir() + id + ".jpg";
    if ( TQFile::exists( file ) ) {
        TQImage img( file );
        if ( !img.isNull() ) {
            TQPixmap pm = scaled( img );
            m_cache.insert( id, pm );
            return pm;
        }
    }

    if ( !m_failed.contains( id ) && !m_queue.contains( id ) ) {
        bool running = false;
        for ( TQMap<TDEIO::Job *, TQString>::ConstIterator j = m_jobs.begin(); j != m_jobs.end(); ++j )
            if ( *j == id ) { running = true; break; }
        if ( !running ) {
            m_queue.append( id );
            TQTimer::singleShot( 0, this, TQ_SLOT( startNext() ) );
        }
    }
    return TQPixmap();
}

void ThumbnailLoader::startNext()
{
    while ( (int) m_jobs.count() < MAX_JOBS && !m_queue.isEmpty() ) {
        TQString id = m_queue.first();
        m_queue.remove( m_queue.begin() );
        KURL url( TQString( "https://i.ytimg.com/vi/%1/mqdefault.jpg" ).arg( id ) );
        TDEIO::StoredTransferJob *job = TDEIO::storedGet( url, false, false );
        job->addMetaData( "cache", "refresh" );
        m_jobs.insert( job, id );
        connect( job, TQ_SIGNAL( result( TDEIO::Job * ) ), this, TQ_SLOT( slotResult( TDEIO::Job * ) ) );
    }
}

void ThumbnailLoader::slotResult( TDEIO::Job *job )
{
    TQString id = m_jobs[ job ];
    m_jobs.remove( job );

    TDEIO::StoredTransferJob *sj = static_cast<TDEIO::StoredTransferJob *>( job );
    TQImage img;
    if ( job->error() || !img.loadFromData( sj->data() ) ) {
        m_failed.append( id );
    } else {
        TQFile f( cacheDir() + id + ".jpg" );
        if ( f.open( IO_WriteOnly ) ) {
            f.writeBlock( sj->data() );
            f.close();
        }
        TQPixmap pm = scaled( img );
        m_cache.insert( id, pm );
        emit thumbnailReady( id, pm );
    }
    startNext();
}

void ThumbnailLoader::clearCache()
{
    TQDir d( cacheDir() );
    TQStringList files = d.entryList( "*.jpg", TQDir::Files );
    for ( TQStringList::ConstIterator it = files.begin(); it != files.end(); ++it )
        d.remove( *it );
    m_cache.clear();
    m_failed.clear();
}

#include "thumbnailloader.moc"
