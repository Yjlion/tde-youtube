/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "library.h"
#include "settings.h"

#include <tdeconfig.h>

Library *Library::self()
{
    static Library *s = 0;
    if ( !s )
        s = new Library;
    return s;
}

Library::Library()
    : TQObject( 0, "Library" )
{
    m_config = new TDEConfig( "tde-youtube-libraryrc", false, false );
    load();
}

Library::~Library()
{
    delete m_config;
}

void Library::load()
{
    m_config->setGroup( "Library" );
    m_favourites = m_config->readListEntry( "Favourites" );
    m_history    = m_config->readListEntry( "History" );

    TQStringList all = m_favourites + m_history;
    for ( TQStringList::ConstIterator it = all.begin(); it != all.end(); ++it ) {
        if ( m_videos.contains( *it ) )
            continue;
        TQString group = "Video " + *it;
        if ( !m_config->hasGroup( group ) )
            continue;
        m_config->setGroup( group );
        Video v;
        v.id         = *it;
        v.title      = m_config->readEntry( "Title" );
        v.channel    = m_config->readEntry( "Channel" );
        v.channelUrl = m_config->readEntry( "ChannelUrl" );
        v.uploadDate = m_config->readEntry( "UploadDate" );
        v.duration   = m_config->readNumEntry( "Duration", 0 );
        v.viewCount  = m_config->readEntry( "Views", "-1" ).toLongLong();
        m_videos.insert( v.id, v );
    }
}

void Library::save()
{
    m_config->setGroup( "Library" );
    m_config->writeEntry( "Favourites", m_favourites );
    m_config->writeEntry( "History", m_history );

    // Drop the data of videos that are no longer referenced.
    TQStringList groups = m_config->groupList();
    for ( TQStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it ) {
        if ( !( *it ).startsWith( "Video " ) )
            continue;
        TQString id = ( *it ).mid( 6 );
        if ( !m_favourites.contains( id ) && !m_history.contains( id ) ) {
            m_config->deleteGroup( *it, true );
            m_videos.remove( id );
        }
    }
    m_config->sync();
}

void Library::storeVideo( const Video &v )
{
    m_videos.insert( v.id, v );
    m_config->setGroup( "Video " + v.id );
    m_config->writeEntry( "Title", v.title );
    m_config->writeEntry( "Channel", v.channel );
    m_config->writeEntry( "ChannelUrl", v.channelUrl );
    m_config->writeEntry( "UploadDate", v.uploadDate );
    m_config->writeEntry( "Duration", v.duration );
    m_config->writeEntry( "Views", TQString::number( v.viewCount ) );
}

VideoList Library::videos( const TQStringList &ids ) const
{
    VideoList l;
    for ( TQStringList::ConstIterator it = ids.begin(); it != ids.end(); ++it ) {
        TQMap<TQString, Video>::ConstIterator v = m_videos.find( *it );
        if ( v != m_videos.end() )
            l.append( *v );
    }
    return l;
}

VideoList Library::favourites() const { return videos( m_favourites ); }
VideoList Library::history() const    { return videos( m_history ); }

bool Library::isFavourite( const TQString &id ) const
{
    return m_favourites.contains( id );
}

void Library::addFavourite( const Video &v )
{
    if ( !v.isValid() || isFavourite( v.id ) )
        return;
    storeVideo( v );
    m_favourites.prepend( v.id );
    save();
    emit favouritesChanged();
}

void Library::removeFavourite( const TQString &id )
{
    if ( !m_favourites.contains( id ) )
        return;
    m_favourites.remove( id );
    save();
    emit favouritesChanged();
}

void Library::addToHistory( const Video &v )
{
    if ( !v.isValid() || !Settings::self().keepHistory )
        return;
    storeVideo( v );
    m_history.remove( v.id );
    m_history.prepend( v.id );
    int max = TQMAX( 1, Settings::self().maxHistory );
    while ( (int) m_history.count() > max )
        m_history.remove( m_history.fromLast() );
    save();
    emit historyChanged();
}

void Library::removeFromHistory( const TQString &id )
{
    if ( !m_history.contains( id ) )
        return;
    m_history.remove( id );
    save();
    emit historyChanged();
}

void Library::clearHistory()
{
    m_history.clear();
    save();
    emit historyChanged();
}

#include "library.moc"
