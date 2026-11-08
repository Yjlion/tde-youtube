/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_THUMBNAILLOADER_H
#define TDEYOUTUBE_THUMBNAILLOADER_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqpixmap.h>
#include <tqstringlist.h>

namespace TDEIO { class Job; }

/**
 * Downloads video thumbnails with TDEIO and keeps them in memory and in
 * the on-disk cache ($TDEHOME/share/cache/tde-youtube/thumbs).
 */
class ThumbnailLoader : public TQObject
{
    TQ_OBJECT

public:
    static ThumbnailLoader *self();

    /** Size thumbnails are scaled to for the lists. */
    static TQSize thumbSize() { return TQSize( 120, 68 ); }

    /** Returns the thumbnail if it is available, otherwise requests it and returns a null pixmap. */
    TQPixmap thumbnail( const TQString &videoId );

    /** Deletes the on-disk cache. */
    void clearCache();

signals:
    void thumbnailReady( const TQString &videoId, const TQPixmap &pixmap );

private slots:
    void slotResult( TDEIO::Job *job );
    void startNext();

private:
    ThumbnailLoader();
    TQString cacheDir() const;
    TQPixmap scaled( const TQImage &img ) const;

    TQMap<TQString, TQPixmap>     m_cache;
    TQMap<TDEIO::Job *, TQString> m_jobs;
    TQStringList                  m_queue;
    TQStringList                  m_failed;
};

#endif
