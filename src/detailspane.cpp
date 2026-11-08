/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "detailspane.h"
#include "searchbackend.h"

#include <tqlabel.h>
#include <tqlayout.h>
#include <tqregexp.h>
#include <tqstylesheet.h>

#include <ktextbrowser.h>
#include <tdelocale.h>
#include <tdeglobal.h>
#include <tdeglobalsettings.h>

DetailsPane::DetailsPane( TQWidget *parent, const char *name )
    : TQWidget( parent, name )
{
    TQVBoxLayout *l = new TQVBoxLayout( this, 4, 4 );

    m_title = new TQLabel( this );
    m_title->setTextFormat( TQt::RichText );
    m_title->setAlignment( TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak );
    TQFont f = m_title->font();
    f.setPointSize( f.pointSize() + 2 );
    f.setBold( true );
    m_title->setFont( f );
    m_title->setSizePolicy( TQSizePolicy( TQSizePolicy::Ignored, TQSizePolicy::Preferred ) );
    l->addWidget( m_title );

    m_meta = new TQLabel( this );
    m_meta->setTextFormat( TQt::RichText );
    m_meta->setAlignment( TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak );
    m_meta->setSizePolicy( TQSizePolicy( TQSizePolicy::Ignored, TQSizePolicy::Preferred ) );
    l->addWidget( m_meta );

    m_description = new KTextBrowser( this );  // links open in the web browser;
    m_description->setTextFormat( TQt::RichText );
    m_description->setFrameStyle( TQFrame::StyledPanel | TQFrame::Sunken );
    l->addWidget( m_description, 1 );

    m_fetcher = new DetailsFetcher( this );
    connect( m_fetcher, TQ_SIGNAL( detailsReady( const TQString &, long long, long long, const TQString &, long long, const TQString & ) ),
             this, TQ_SLOT( slotDetails( const TQString &, long long, long long, const TQString &, long long, const TQString & ) ) );
    connect( m_fetcher, TQ_SIGNAL( error( const TQString &, const TQString & ) ),
             this, TQ_SLOT( slotError( const TQString &, const TQString & ) ) );

    setVideo( Video() );
}

void DetailsPane::setVideo( const Video &video )
{
    if ( video.isValid() && video.id == m_video.id )
        return;
    m_fetcher->cancel();
    m_video = video;

    if ( !video.isValid() ) {
        m_title->setText( TQString::null );
        m_meta->setText( TQString::null );
        m_description->setText( "<qt><i>" + i18n( "Select a video to see its description." ) + "</i></qt>" );
        return;
    }

    if ( m_cache.contains( video.id ) ) {
        showDetails( m_cache[ video.id ] );
        return;
    }
    updateHeader( 0 );
    m_description->setText( "<qt><i>" + i18n( "Loading description..." ) + "</i></qt>" );
    m_fetcher->fetch( video.id );
}

void DetailsPane::updateHeader( const Details *d )
{
    m_title->setText( TQStyleSheet::escape( m_video.title ) );

    TQStringList parts;
    if ( !m_video.channel.isEmpty() ) {
        TQString ch = "<b>" + TQStyleSheet::escape( m_video.channel ) + "</b>";
        if ( d && d->subscribers >= 0 )
            ch += " (" + i18n( "%1 subscribers" ).arg( TDEGlobal::locale()->formatNumber( (double) d->subscribers, 0 ) ) + ")";
        parts << ch;
    }
    TQString dur = Video::formatDuration( m_video.duration );
    if ( !dur.isEmpty() )
        parts << dur;
    long long views = d && d->views >= 0 ? d->views : m_video.viewCount;
    if ( views >= 0 )
        parts << i18n( "%1 views" ).arg( TDEGlobal::locale()->formatNumber( (double) views, 0 ) );
    if ( d && d->likes >= 0 )
        parts << i18n( "%1 likes" ).arg( TDEGlobal::locale()->formatNumber( (double) d->likes, 0 ) );
    TQString date = Video::formatDate( d ? d->uploadDate : m_video.uploadDate );
    if ( !date.isEmpty() )
        parts << date;

    m_meta->setText( "<qt>" + parts.join( TQString::fromUtf8( " &nbsp;\xc2\xb7&nbsp; " ) ) + "</qt>" );
}

void DetailsPane::showDetails( const Details &d )
{
    updateHeader( &d );
    if ( d.description.stripWhiteSpace().isEmpty() )
        m_description->setText( "<qt><i>" + i18n( "This video has no description." ) + "</i></qt>" );
    else
        m_description->setText( "<qt>" + linkify( d.description ) + "</qt>" );
}

TQString DetailsPane::linkify( const TQString &text )
{
    TQString html = TQStyleSheet::escape( text );
    TQRegExp url( "(https?://[^\\s<>\"]+)" );
    int pos = 0;
    while ( ( pos = url.search( html, pos ) ) >= 0 ) {
        TQString u = url.cap( 1 );
        TQString a = "<a href=\"" + u + "\">" + u + "</a>";
        html.replace( pos, u.length(), a );
        pos += a.length();
    }
    html.replace( "\n", "<br>" );
    return html;
}

void DetailsPane::slotDetails( const TQString &id, long long likes, long long views,
                               const TQString &uploadDate, long long subscribers, const TQString &description )
{
    Details d;
    d.likes = likes;
    d.views = views;
    d.subscribers = subscribers;
    d.uploadDate = uploadDate;
    d.description = description;
    m_cache.insert( id, d );
    if ( id == m_video.id )
        showDetails( d );
}

void DetailsPane::slotError( const TQString &id, const TQString &message )
{
    if ( id != m_video.id )
        return;
    m_description->setText( "<qt><i>" + i18n( "Could not load the description: %1" )
                            .arg( TQStyleSheet::escape( message ) ) + "</i></qt>" );
}

#include "detailspane.moc"
