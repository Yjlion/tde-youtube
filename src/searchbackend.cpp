/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "searchbackend.h"
#include "settings.h"

#include <tqregexp.h>
#include <tqdatetime.h>

#include <kprocess.h>
#include <tdelocale.h>
#include <tdeglobal.h>
#include <kdebug.h>

// ASCII "unit separator": never part of a title, safe as a field delimiter.
static const char SEP = '\x1f';

static TQString searchTemplate()
{
    TQString s;
    s += "%(id)s";                         s += SEP;
    s += "%(title)s";                      s += SEP;
    s += "%(channel,uploader|)s";          s += SEP;
    s += "%(duration|0)s";                 s += SEP;
    s += "%(view_count|-1)s";              s += SEP;
    s += "%(upload_date|)s";               s += SEP;
    s += "%(channel_url,uploader_url|)s";
    return s;
}

static TQStringList commonArgs()
{
    TQStringList a;
    a << "--ignore-config" << "--no-warnings" << "--no-colors" << "--encoding" << "utf-8";
    return a;
}

/* ------------------------------------------------------------------ Video */

TQString Video::formatDuration( int s )
{
    if ( s <= 0 )
        return TQString::null;
    int h = s / 3600, m = ( s % 3600 ) / 60, sec = s % 60;
    if ( h > 0 )
        return TQString().sprintf( "%d:%02d:%02d", h, m, sec );
    return TQString().sprintf( "%d:%02d", m, sec );
}

TQString Video::formatViews( long long v )
{
    if ( v < 0 )
        return TQString::null;
    TQString n;
    if ( v >= 1000000000LL )
        n = TDEGlobal::locale()->formatNumber( v / 1e9, v < 10000000000LL ? 1 : 0 ) + i18n( "billion abbreviation", "B" );
    else if ( v >= 1000000LL )
        n = TDEGlobal::locale()->formatNumber( v / 1e6, v < 10000000LL ? 1 : 0 ) + i18n( "million abbreviation", "M" );
    else if ( v >= 1000LL )
        n = TDEGlobal::locale()->formatNumber( v / 1e3, v < 10000LL ? 1 : 0 ) + i18n( "thousand abbreviation", "K" );
    else
        return i18n( "1 view", "%n views", (int) v );
    return i18n( "%1 views" ).arg( n );
}

TQString Video::formatDate( const TQString &d )
{
    if ( d.length() != 8 )
        return TQString::null;
    TQDate date( d.left( 4 ).toInt(), d.mid( 4, 2 ).toInt(), d.mid( 6, 2 ).toInt() );
    if ( !date.isValid() )
        return TQString::null;
    return TDEGlobal::locale()->formatDate( date, true );
}

/* --------------------------------------------------------------- YtDlpJob */

YtDlpJob::YtDlpJob( TQObject *parent, const char *name )
    : TQObject( parent, name ), m_proc( 0 ), m_lineMode( true )
{
}

YtDlpJob::~YtDlpJob()
{
    cancel();
}

TQString YtDlpJob::notFoundMessage()
{
    return i18n( "Could not run yt-dlp (\"%1\"). Please install yt-dlp or set "
                 "its location in the settings." ).arg( Settings::self().ytdlpPath );
}

TQString YtDlpJob::friendlyError( const TQString &error )
{
    TQString e = error.stripWhiteSpace();
    if ( e.startsWith( "ERROR:" ) )
        e = e.mid( 6 ).stripWhiteSpace();
    // "[youtube] dQw4w9WgXcQ: message" -> "message"
    TQRegExp prefix( "^\\[[^\\]]+\\]\\s*([A-Za-z0-9_-]+:)?\\s*" );
    if ( prefix.search( e ) == 0 )
        e = e.mid( prefix.matchedLength() );
    if ( e.contains( "confirm you" ) || e.contains( "Sign in to confirm" ) ) {
        int cut = e.find( " Use --cookies" );
        if ( cut > 0 )
            e.truncate( cut );
        e += " " + i18n( "YouTube wants you to be signed in. Choose a web browser you are signed in "
                         "with under Settings > Configure TDE YouTube > Programs > Cookies." );
    } else if ( e.contains( "JavaScript runtime", false ) || e.contains( "js-runtimes" )
                || e.contains( "challenge solver", false ) ) {
        e += " " + i18n( "yt-dlp needs a JavaScript runtime to play YouTube videos. Install one, "
                         "for example: sudo apt install quickjs" );
    } else if ( e.contains( "page needs to be reloaded", false )
                || e.contains( "Requested format is not available" )
                || e.contains( "nsig extraction failed" )
                || e.contains( "Signature extraction failed" )
                || e.contains( "HTTP Error 403" ) ) {
        // These almost always mean that YouTube changed and yt-dlp did not keep up.
        e += " " + i18n( "Your yt-dlp is probably too old for YouTube. Install a newer one (on "
                         "Debian 13: sudo apt install -t trixie-backports yt-dlp) or choose a newer "
                         "yt-dlp under Settings > Configure TDE YouTube > Programs." );
    }
    return e;
}

bool YtDlpJob::start( const TQStringList &args )
{
    cancel();
    m_out.resize( 0 );
    m_err.resize( 0 );

    m_proc = new TDEProcess( this );
    m_proc->setUseShell( false );
    *m_proc << Settings::self().ytdlpPath;
    *m_proc << commonArgs();
    *m_proc << Settings::self().ytdlpCookieArgs();
    *m_proc << Settings::ytdlpJsRuntimeArgs();
    *m_proc << args;

    connect( m_proc, TQ_SIGNAL( receivedStdout( TDEProcess *, char *, int ) ),
             this,   TQ_SLOT( slotStdout( TDEProcess *, char *, int ) ) );
    connect( m_proc, TQ_SIGNAL( receivedStderr( TDEProcess *, char *, int ) ),
             this,   TQ_SLOT( slotStderr( TDEProcess *, char *, int ) ) );
    connect( m_proc, TQ_SIGNAL( processExited( TDEProcess * ) ),
             this,   TQ_SLOT( slotExited( TDEProcess * ) ) );

    if ( !m_proc->start( TDEProcess::NotifyOnExit, TDEProcess::AllOutput ) ) {
        delete m_proc;
        m_proc = 0;
        return false;
    }
    return true;
}

void YtDlpJob::cancel()
{
    if ( !m_proc )
        return;
    m_proc->disconnect( this );
    if ( m_proc->isRunning() )
        m_proc->kill();
    m_proc->deleteLater();
    m_proc = 0;
}

bool YtDlpJob::isRunning() const
{
    return m_proc && m_proc->isRunning();
}

TQString YtDlpJob::output() const
{
    return TQString::fromUtf8( m_out.data(), m_out.length() );
}

void YtDlpJob::slotStdout( TDEProcess *, char *buffer, int len )
{
    // TQCString is NUL terminated; append the raw bytes.
    uint old = m_out.length();
    m_out.resize( old + len + 1 );
    memcpy( m_out.data() + old, buffer, len );
    m_out.data()[ old + len ] = '\0';
    if ( m_lineMode )
        flushLines( false );
}

void YtDlpJob::slotStderr( TDEProcess *, char *buffer, int len )
{
    uint old = m_err.length();
    m_err.resize( old + len + 1 );
    memcpy( m_err.data() + old, buffer, len );
    m_err.data()[ old + len ] = '\0';
}

void YtDlpJob::flushLines( bool all )
{
    int nl;
    while ( ( nl = m_out.find( '\n' ) ) >= 0 ) {
        TQString line = TQString::fromUtf8( m_out.data(), nl );
        m_out.remove( 0, nl + 1 );
        emit lineReceived( line );
    }
    if ( all && !m_out.isEmpty() ) {
        TQString line = TQString::fromUtf8( m_out.data() );
        m_out.resize( 0 );
        emit lineReceived( line );
    }
}

void YtDlpJob::slotExited( TDEProcess *p )
{
    if ( m_lineMode )
        flushLines( true );

    bool ok = p->normalExit() && p->exitStatus() == 0;
    TQString err;
    if ( !ok ) {
        // Report the last "ERROR:" line, or the last line of stderr.
        TQStringList lines = TQStringList::split( '\n', TQString::fromUtf8( m_err.data() ) );
        for ( TQStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it )
            if ( ( *it ).startsWith( "ERROR:" ) )
                err = ( *it ).mid( 6 ).stripWhiteSpace();
        if ( err.isEmpty() && !lines.isEmpty() )
            err = lines.last().stripWhiteSpace();
        if ( err.isEmpty() )
            err = i18n( "yt-dlp exited with status %1." ).arg( p->exitStatus() );
        else
            err = friendlyError( err );
    }

    m_proc->deleteLater();
    m_proc = 0;
    emit finished( ok, err );
}

/* ---------------------------------------------------------- SearchBackend */

SearchBackend::SearchBackend( TQObject *parent, const char *name )
    : TQObject( parent, name ), m_count( 0 )
{
    m_job = new YtDlpJob( this );
    connect( m_job, TQ_SIGNAL( lineReceived( const TQString & ) ),
             this,  TQ_SLOT( slotLine( const TQString & ) ) );
    connect( m_job, TQ_SIGNAL( finished( bool, const TQString & ) ),
             this,  TQ_SLOT( slotFinished( bool, const TQString & ) ) );
}

bool SearchBackend::looksLikeUrl( const TQString &text )
{
    TQString t = text.stripWhiteSpace();
    return t.startsWith( "http://" ) || t.startsWith( "https://" )
        || t.startsWith( "www.youtube.com/" ) || t.startsWith( "youtube.com/" )
        || t.startsWith( "youtu.be/" ) || t.startsWith( "m.youtube.com/" );
}

TQString SearchBackend::normaliseUrl( const TQString &url )
{
    TQString u = url.stripWhiteSpace();
    if ( !u.startsWith( "http" ) )
        u.prepend( "https://" );
    // A bare channel URL lists the channel's tabs; list its uploads instead.
    TQRegExp channel( "^(https?://(www\\.|m\\.)?youtube\\.com/(@[^/?#]+|channel/[^/?#]+|c/[^/?#]+|user/[^/?#]+))/?$" );
    if ( channel.search( u ) == 0 )
        u = channel.cap( 1 ) + "/videos";
    return u;
}

Video SearchBackend::parseLine( const TQString &line )
{
    Video v;
    TQStringList f = TQStringList::split( TQChar( SEP ), line, true );
    if ( f.count() < 7 )
        return v;
    static TQRegExp idRx( "^[A-Za-z0-9_-]{11}$" );
    if ( idRx.search( f[0] ) != 0 )
        return v;   // channel / playlist entries etc.

    v.id         = f[0];
    v.title      = f[1] == "NA" ? TQString::null : f[1];
    v.channel    = f[2] == "NA" ? TQString::null : f[2];
    v.duration   = (int) f[3].toDouble();
    bool ok;
    v.viewCount  = f[4].toLongLong( &ok );
    if ( !ok ) v.viewCount = -1;
    v.uploadDate = f[5] == "NA" ? TQString::null : f[5];
    v.channelUrl = f[6] == "NA" ? TQString::null : f[6];
    return v;
}

void SearchBackend::search( const TQString &query, int count )
{
    cancel();
    m_count = 0;

    TQString target;
    TQStringList args;
    if ( looksLikeUrl( query ) ) {
        target = normaliseUrl( query );
        args << "--playlist-end" << TQString::number( count );
    } else {
        target = TQString( "ytsearch%1:%2" ).arg( count ).arg( query.stripWhiteSpace() );
    }
    args << "--flat-playlist" << "--print" << searchTemplate() << "--" << target;

    if ( !m_job->start( args ) )
        emit error( YtDlpJob::notFoundMessage() );
}

void SearchBackend::cancel()
{
    m_job->cancel();
}

bool SearchBackend::isRunning() const
{
    return m_job->isRunning();
}

void SearchBackend::slotLine( const TQString &line )
{
    Video v = parseLine( line );
    if ( v.isValid() ) {
        ++m_count;
        emit videoFound( v );
    }
}

void SearchBackend::slotFinished( bool ok, const TQString &err )
{
    if ( !ok && m_count == 0 )
        emit error( err );
    else
        emit finished( m_count );
}

/* --------------------------------------------------------- DetailsFetcher */

DetailsFetcher::DetailsFetcher( TQObject *parent, const char *name )
    : TQObject( parent, name )
{
    m_job = new YtDlpJob( this );
    m_job->setLineMode( false );
    connect( m_job, TQ_SIGNAL( finished( bool, const TQString & ) ),
             this,  TQ_SLOT( slotFinished( bool, const TQString & ) ) );
}

void DetailsFetcher::fetch( const TQString &videoId )
{
    m_id = videoId;
    TQString tmpl;
    tmpl += "%(like_count|-1)s";               tmpl += SEP;
    tmpl += "%(view_count|-1)s";               tmpl += SEP;
    tmpl += "%(upload_date|)s";                tmpl += SEP;
    tmpl += "%(channel_follower_count|-1)s";   tmpl += SEP;
    tmpl += "%(description|)s";

    TQStringList args;
    args << "--skip-download" << "--no-playlist" << "--print" << tmpl
         << "--" << ( "https://www.youtube.com/watch?v=" + videoId );
    if ( !m_job->start( args ) )
        emit error( videoId, YtDlpJob::notFoundMessage() );
}

void DetailsFetcher::cancel()
{
    m_job->cancel();
}

void DetailsFetcher::slotFinished( bool ok, const TQString &err )
{
    if ( !ok ) {
        emit error( m_id, err );
        return;
    }
    TQString out = m_job->output();
    TQStringList f = TQStringList::split( TQChar( SEP ), out, true );
    if ( f.count() < 5 ) {
        emit error( m_id, i18n( "Unexpected output from yt-dlp." ) );
        return;
    }
    // The description itself may (in theory) contain the separator; rejoin it.
    TQString desc = TQStringList( f ).join( TQChar( SEP ) );
    for ( int i = 0; i < 4; ++i )
        desc = desc.mid( desc.find( SEP ) + 1 );
    if ( desc.endsWith( "\n" ) )
        desc.truncate( desc.length() - 1 );
    if ( desc == "NA" )
        desc = TQString::null;

    bool ok1, ok2, ok3;
    long long likes = f[0].toLongLong( &ok1 );
    long long views = f[1].toLongLong( &ok2 );
    long long subs  = f[3].toLongLong( &ok3 );
    emit detailsReady( m_id, ok1 ? likes : -1, ok2 ? views : -1,
                       f[2] == "NA" ? TQString::null : f[2], ok3 ? subs : -1, desc );
}

#include "searchbackend.moc"
