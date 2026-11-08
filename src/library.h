/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_LIBRARY_H
#define TDEYOUTUBE_LIBRARY_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqstringlist.h>

#include "video.h"

class TDEConfig;

/**
 * Favourites and watch history, stored in tde-youtube-libraryrc.
 */
class Library : public TQObject
{
    TQ_OBJECT

public:
    static Library *self();
    ~Library();

    VideoList favourites() const;
    VideoList history() const;
    bool isFavourite( const TQString &id ) const;

    void addFavourite( const Video &video );
    void removeFavourite( const TQString &id );
    /** Moves @p video to the top of the history (if history is enabled). */
    void addToHistory( const Video &video );
    void removeFromHistory( const TQString &id );
    void clearHistory();

signals:
    void favouritesChanged();
    void historyChanged();

private:
    Library();
    void load();
    void save();
    void storeVideo( const Video &v );
    VideoList videos( const TQStringList &ids ) const;

    TDEConfig              *m_config;
    TQStringList            m_favourites;
    TQStringList            m_history;
    TQMap<TQString, Video>  m_videos;
};

#endif
