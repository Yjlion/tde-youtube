/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ytdlpupdater.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <tqfile.h>
#include <tqregexp.h>
#include <tqstringlist.h>

#include <kprocess.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <tdeio/job.h>
#include <tdelocale.h>
#include <kdebug.h>

// Official release files; "latest/download" redirects to the newest release.
static const char RELEASE_BASE[] = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";
// The platform independent zipimport build. It runs with the system's
// Python 3 and, unlike Debian's package, includes yt-dlp-ejs.
static const char RELEASE_FILE[] = "yt-dlp";

/** SHA-256 of @p path as lower case hex, or null if it can't be computed. */
static TQString sha256OfFile( const TQString &path )
{
    if ( !TQFile::exists( path ) )
        return TQString::null;
    TQCString cmd = "sha256sum -b " + TQFile::encodeName( TDEProcess::quote( path ) ) + " 2>/dev/null";
    TQString sum;
    if ( FILE *f = ::popen( cmd.data(), "r" ) ) {
        char buf[128];
        if ( ::fgets( buf, sizeof( buf ), f ) )
            sum = TQString::fromLatin1( buf ).section( ' ', 0, 0 ).lower();
        ::pclose( f );
    }
    static TQRegExp hex( "^[0-9a-f]{64}$" );
    return hex.search( sum ) == 0 ? sum : TQString::null;
}

YtDlpUpdater *YtDlpUpdater::self()
{
    static YtDlpUpdater *s = 0;
    if ( !s )
        s = new YtDlpUpdater;
    return s;
}

YtDlpUpdater::YtDlpUpdater()
    : TQObject( 0, "YtDlpUpdater" ), m_step( Idle ), m_proc( 0 ), m_result( UpToDate )
{
}

bool YtDlpUpdater::checkDue() const
{
    const Settings &s = Settings::self();
    if ( !s.manageYtdlp )
        return false;
    if ( !TQFile::exists( Settings::managedYtdlpPath() ) || !s.managedYtdlpChecked.isValid() )
        return true;
    return s.managedYtdlpChecked.secsTo( TQDateTime::currentDateTime() ) > 24 * 3600;
}

void YtDlpUpdater::update()
{
    if ( isRunning() )
        return;
    m_step = FetchingSums;
    m_result = UpToDate;
    m_expectedSum = TQString::null;
    emit started();

    if ( TDEStandardDirs::findExe( "python3" ).isEmpty() ) {
        fail( i18n( "yt-dlp needs Python 3, which is not installed." ) );
        return;
    }

    KURL url( TQString( RELEASE_BASE ) + "SHA2-256SUMS" );
    TDEIO::StoredTransferJob *job = TDEIO::storedGet( url, true, false );
    job->addMetaData( "cache", "reload" );
    connect( job, TQ_SIGNAL( result( TDEIO::Job * ) ), this, TQ_SLOT( slotSumsResult( TDEIO::Job * ) ) );
}

void YtDlpUpdater::slotSumsResult( TDEIO::Job *job )
{
    if ( job->error() ) {
        fail( i18n( "Could not check for a new yt-dlp: %1" ).arg( job->errorString() ) );
        return;
    }

    // Lines look like "<sha256>  yt-dlp".
    TQByteArray data = static_cast<TDEIO::StoredTransferJob *>( job )->data();
    TQStringList lines = TQStringList::split( '\n', TQString::fromLatin1( data.data(), data.size() ) );
    for ( TQStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it ) {
        TQStringList f = TQStringList::split( TQRegExp( "\\s+" ), ( *it ).stripWhiteSpace() );
        if ( f.count() != 2 )
            continue;
        TQString name = f[1].startsWith( "*" ) ? f[1].mid( 1 ) : f[1];
        if ( name == RELEASE_FILE ) {
            m_expectedSum = f[0].lower();
            break;
        }
    }
    static TQRegExp hex( "^[0-9a-f]{64}$" );
    if ( hex.search( m_expectedSum ) != 0 ) {
        fail( i18n( "Could not check for a new yt-dlp: the checksum list has an unexpected format." ) );
        return;
    }

    const TQString path = Settings::managedYtdlpPath();
    if ( sha256OfFile( path ) == m_expectedSum ) {
        m_result = UpToDate;
        checkVersion();
        return;
    }

    m_step = FetchingFile;
    KURL url( TQString( RELEASE_BASE ) + RELEASE_FILE );
    TDEIO::StoredTransferJob *fileJob = TDEIO::storedGet( url, true, false );
    fileJob->addMetaData( "cache", "reload" );
    connect( fileJob, TQ_SIGNAL( result( TDEIO::Job * ) ), this, TQ_SLOT( slotFileResult( TDEIO::Job * ) ) );
}

void YtDlpUpdater::slotFileResult( TDEIO::Job *job )
{
    if ( job->error() ) {
        fail( i18n( "Could not download yt-dlp: %1" ).arg( job->errorString() ) );
        return;
    }

    const TQString path = Settings::managedYtdlpPath();
    const TQString tmp = path + ".new";
    TQByteArray data = static_cast<TDEIO::StoredTransferJob *>( job )->data();
    TQFile f( tmp );
    if ( !f.open( IO_WriteOnly | IO_Truncate )
         || f.writeBlock( data.data(), data.size() ) != (int) data.size() ) {
        f.close();
        TQFile::remove( tmp );
        fail( i18n( "Could not save yt-dlp to %1." ).arg( tmp ) );
        return;
    }
    f.close();

    if ( sha256OfFile( tmp ) != m_expectedSum ) {
        TQFile::remove( tmp );
        fail( i18n( "The downloaded yt-dlp is damaged (wrong checksum). It was not installed." ) );
        return;
    }

    // Replace the old copy in one step; a running yt-dlp keeps its old file.
    const TQCString ctmp = TQFile::encodeName( tmp ), cpath = TQFile::encodeName( path );
    if ( ::chmod( ctmp.data(), 0755 ) != 0 || ::rename( ctmp.data(), cpath.data() ) != 0 ) {
        TQFile::remove( tmp );
        fail( i18n( "Could not install yt-dlp to %1." ).arg( path ) );
        return;
    }

    Settings::self().managedYtdlpVersion = TQString::null;
    Settings::forgetYtdlpVersion();
    m_result = Updated;
    checkVersion();
}

void YtDlpUpdater::checkVersion()
{
    // Always run the installed copy once: it proves that it works (it needs
    // Python 3) and tells us its version.
    m_step = CheckingVersion;
    m_versionOut.resize( 0 );
    m_proc = new TDEProcess( this );
    *m_proc << Settings::managedYtdlpPath() << "--ignore-config" << "--version";
    connect( m_proc, TQ_SIGNAL( receivedStdout( TDEProcess *, char *, int ) ),
             this,   TQ_SLOT( slotVersionOutput( TDEProcess *, char *, int ) ) );
    connect( m_proc, TQ_SIGNAL( processExited( TDEProcess * ) ),
             this,   TQ_SLOT( slotVersionExited( TDEProcess * ) ) );
    if ( !m_proc->start( TDEProcess::NotifyOnExit, TDEProcess::Stdout ) ) {
        delete m_proc;
        m_proc = 0;
        fail( i18n( "Could not run the downloaded yt-dlp." ) );
    }
}

void YtDlpUpdater::slotVersionOutput( TDEProcess *, char *buffer, int len )
{
    uint old = m_versionOut.length();
    m_versionOut.resize( old + len + 1 );
    memcpy( m_versionOut.data() + old, buffer, len );
    m_versionOut.data()[ old + len ] = '\0';
}

void YtDlpUpdater::slotVersionExited( TDEProcess *p )
{
    bool ok = p->normalExit() && p->exitStatus() == 0;
    TQString version = TQString::fromLatin1( m_versionOut.data() ).section( '\n', 0, 0 ).stripWhiteSpace();
    m_proc->deleteLater();
    m_proc = 0;

    if ( !ok || version.isEmpty() ) {
        fail( i18n( "The downloaded yt-dlp does not run. Is Python 3 installed?" ) );
        return;
    }

    Settings &s = Settings::self();
    s.managedYtdlpVersion = version;
    s.managedYtdlpChecked = TQDateTime::currentDateTime();
    s.save();
    Settings::forgetYtdlpVersion();
    done( m_result );
}

void YtDlpUpdater::fail( const TQString &message )
{
    kdWarning() << "tde-youtube: " << message << endl;
    m_step = Idle;
    emit finished( Failed, message );
}

void YtDlpUpdater::done( Result result )
{
    m_step = Idle;
    emit finished( result, Settings::self().managedYtdlpVersion );
}

#include "ytdlpupdater.moc"
