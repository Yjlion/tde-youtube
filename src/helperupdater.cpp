/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "helperupdater.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <tqfile.h>
#include <tqregexp.h>

#include <kprocess.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <tdeio/job.h>
#include <tdelocale.h>
#include <kdebug.h>

// yt-dlp: "latest/download" redirects to the newest release.
static const char YTDLP_BASE[] = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";
static const char YTDLP_FILE[] = "yt-dlp";
// QuickJS-ng: no checksum file, but the API lists a SHA-256 digest per asset.
static const char QJS_API[] = "https://api.github.com/repos/quickjs-ng/quickjs/releases/latest";
static const char QJS_DOWNLOAD_PREFIX[] = "https://github.com/quickjs-ng/quickjs/releases/download/";

static bool isSha256( const TQString &s )
{
    static TQRegExp hex( "^[0-9a-f]{64}$" );
    return hex.search( s ) == 0;
}

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
    return isSha256( sum ) ? sum : TQString::null;
}

static bool isExecutable( const TQString &path )
{
    return ::access( TQFile::encodeName( path ), X_OK ) == 0;
}

HelperUpdater *HelperUpdater::self()
{
    static HelperUpdater *s = 0;
    if ( !s )
        s = new HelperUpdater;
    return s;
}

HelperUpdater::HelperUpdater()
    : TQObject( 0, "HelperUpdater" ), m_step( Idle ), m_changed( false ), m_proc( 0 )
{
}

TQString HelperUpdater::qjsAssetName()
{
    struct utsname u;
    if ( ::uname( &u ) != 0 )
        return TQString::null;
    const TQString m = TQString::fromLatin1( u.machine );
    TQString arch;
    if ( m == "x86_64" || m == "amd64" )
        arch = "x86_64";
    else if ( m == "aarch64" || m == "arm64" )
        arch = "aarch64";
    else if ( m == "armv7l" || m == "armv8l" )
        arch = "armv7";
    else if ( TQRegExp( "^i[3-6]86$" ).search( m ) == 0 )
        arch = "x86";
    else if ( m == "riscv64" )
        arch = "riscv64";
    return arch.isEmpty() ? TQString::null : "qjs-linux-" + arch;
}

bool HelperUpdater::checkDue() const
{
    const Settings &s = Settings::self();
    if ( !s.manageYtdlp )
        return false;
    if ( !isExecutable( Settings::managedYtdlpPath() ) )
        return true;
    if ( !qjsAssetName().isEmpty() && !isExecutable( Settings::managedQjsPath() ) )
        return true;
    if ( !s.managedYtdlpChecked.isValid() )
        return true;
    return s.managedYtdlpChecked.secsTo( TQDateTime::currentDateTime() ) > 24 * 3600;
}

void HelperUpdater::get( const TQString &url, const char *slot, bool githubApi )
{
    TDEIO::StoredTransferJob *job = TDEIO::storedGet( KURL( url ), true, false );
    job->addMetaData( "cache", "reload" );
    if ( githubApi )
        job->addMetaData( "customHTTPHeader", "Accept: application/vnd.github+json" );
    connect( job, TQ_SIGNAL( result( TDEIO::Job * ) ), this, slot );
}

void HelperUpdater::update()
{
    if ( isRunning() )
        return;
    m_errors.clear();
    m_changed = false;
    m_ytdlpSum = m_qjsSum = m_qjsTag = TQString::null;
    m_step = YtdlpSums;
    emit started();

    if ( TDEStandardDirs::findExe( "python3" ).isEmpty() ) {
        m_errors << i18n( "yt-dlp needs Python 3, which is not installed." );
        startQjs();
        return;
    }
    get( TQString( YTDLP_BASE ) + "SHA2-256SUMS", TQ_SLOT( slotYtdlpSums( TDEIO::Job * ) ) );
}

/* ----------------------------------------------------------------- yt-dlp */

void HelperUpdater::slotYtdlpSums( TDEIO::Job *job )
{
    if ( job->error() ) {
        m_errors << i18n( "Could not check for a new yt-dlp: %1" ).arg( job->errorString() );
        startQjs();
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
        if ( name == YTDLP_FILE ) {
            m_ytdlpSum = f[0].lower();
            break;
        }
    }
    if ( !isSha256( m_ytdlpSum ) ) {
        m_errors << i18n( "Could not check for a new yt-dlp: the checksum list has an unexpected format." );
        startQjs();
        return;
    }

    if ( sha256OfFile( Settings::managedYtdlpPath() ) == m_ytdlpSum ) {
        startQjs();
        return;
    }
    m_step = YtdlpFile;
    get( TQString( YTDLP_BASE ) + YTDLP_FILE, TQ_SLOT( slotYtdlpFile( TDEIO::Job * ) ) );
}

void HelperUpdater::slotYtdlpFile( TDEIO::Job *job )
{
    TQString err;
    if ( job->error() )
        err = i18n( "Could not download yt-dlp: %1" ).arg( job->errorString() );
    else
        err = installFile( static_cast<TDEIO::StoredTransferJob *>( job )->data(),
                           Settings::managedYtdlpPath(), m_ytdlpSum );
    if ( err.isEmpty() ) {
        m_changed = true;
        Settings::self().managedYtdlpVersion = TQString::null;   // looked up below
        Settings::forgetHelperInfo();
    } else {
        m_errors << i18n( "yt-dlp: %1" ).arg( err );
    }
    startQjs();
}

/* ------------------------------------------------------------- QuickJS-ng */

void HelperUpdater::startQjs()
{
    if ( qjsAssetName().isEmpty() ) {
        // No ready-made QuickJS-ng for this CPU; an installed deno, qjs or
        // node is used instead (see Settings::ytdlpJsRuntimeArgs()).
        checkYtdlpVersion();
        return;
    }
    m_step = QjsRelease;
    get( QJS_API, TQ_SLOT( slotQjsRelease( TDEIO::Job * ) ), true );
}

void HelperUpdater::slotQjsRelease( TDEIO::Job *job )
{
    if ( job->error() ) {
        m_errors << i18n( "Could not check for a new QuickJS-ng: %1" ).arg( job->errorString() );
        checkYtdlpVersion();
        return;
    }

    // Small, fixed JSON layout: find our asset by name, then the first digest
    // and download URL after it (GitHub lists them later in the same object).
    TQByteArray data = static_cast<TDEIO::StoredTransferJob *>( job )->data();
    const TQString json = TQString::fromUtf8( data.data(), data.size() );
    const TQString asset = qjsAssetName();

    TQRegExp tagRx( "\"tag_name\"\\s*:\\s*\"([^\"]+)\"" );
    TQRegExp nameRx( "\"name\"\\s*:\\s*\"" + TQRegExp::escape( asset ) + "\"" );
    TQRegExp digestRx( "\"digest\"\\s*:\\s*\"sha256:([0-9a-fA-F]{64})\"" );
    TQRegExp urlRx( "\"browser_download_url\"\\s*:\\s*\"([^\"]+)\"" );

    int pos = nameRx.search( json );
    TQString url;
    if ( tagRx.search( json ) >= 0 && pos >= 0
         && digestRx.search( json, pos ) >= 0 && urlRx.search( json, pos ) >= 0 ) {
        m_qjsTag = tagRx.cap( 1 );
        m_qjsSum = digestRx.cap( 1 ).lower();
        url = urlRx.cap( 1 );
    }
    if ( m_qjsTag.isEmpty() || !isSha256( m_qjsSum )
         || !url.startsWith( QJS_DOWNLOAD_PREFIX ) || !url.endsWith( "/" + asset ) ) {
        m_errors << i18n( "Could not check for a new QuickJS-ng: the release information has an "
                          "unexpected format." );
        checkYtdlpVersion();
        return;
    }

    if ( sha256OfFile( Settings::managedQjsPath() ) == m_qjsSum ) {
        Settings::self().managedQjsVersion = m_qjsTag;
        checkYtdlpVersion();
        return;
    }
    m_step = QjsFile;
    get( url, TQ_SLOT( slotQjsFile( TDEIO::Job * ) ) );
}

void HelperUpdater::slotQjsFile( TDEIO::Job *job )
{
    TQString err;
    if ( job->error() )
        err = i18n( "Could not download QuickJS-ng: %1" ).arg( job->errorString() );
    else
        err = installFile( static_cast<TDEIO::StoredTransferJob *>( job )->data(),
                           Settings::managedQjsPath(), m_qjsSum );
    if ( err.isEmpty() ) {
        m_changed = true;
        Settings::self().managedQjsVersion = m_qjsTag;
        Settings::forgetHelperInfo();
    } else {
        m_errors << i18n( "QuickJS-ng: %1" ).arg( err );
    }
    checkYtdlpVersion();
}

/* ------------------------------------------------------- install / verify */

TQString HelperUpdater::installFile( const TQByteArray &data, const TQString &path, const TQString &sum )
{
    const TQString tmp = path + ".new";
    TQFile f( tmp );
    if ( !f.open( IO_WriteOnly | IO_Truncate )
         || f.writeBlock( data.data(), data.size() ) != (int) data.size() ) {
        f.close();
        TQFile::remove( tmp );
        return i18n( "could not save it to %1." ).arg( tmp );
    }
    f.close();

    if ( sha256OfFile( tmp ) != sum ) {
        TQFile::remove( tmp );
        return i18n( "the download is damaged (wrong checksum), so it was not installed." );
    }

    // Replace the old copy in one step; a running program keeps its old file.
    const TQCString ctmp = TQFile::encodeName( tmp ), cpath = TQFile::encodeName( path );
    if ( ::chmod( ctmp.data(), 0755 ) != 0 || ::rename( ctmp.data(), cpath.data() ) != 0 ) {
        TQFile::remove( tmp );
        return i18n( "could not install it to %1." ).arg( path );
    }
    return TQString::null;
}

/* ---------------------------------------------------------------- version */

void HelperUpdater::checkYtdlpVersion()
{
    // Run our yt-dlp once: it proves that it works (it needs Python 3) and
    // tells us its version.
    const TQString path = Settings::managedYtdlpPath();
    if ( !isExecutable( path ) ) {
        finish();
        return;
    }
    m_step = YtdlpVersion;
    m_versionOut.resize( 0 );
    m_proc = new TDEProcess( this );
    *m_proc << path << "--ignore-config" << "--version";
    connect( m_proc, TQ_SIGNAL( receivedStdout( TDEProcess *, char *, int ) ),
             this,   TQ_SLOT( slotVersionOutput( TDEProcess *, char *, int ) ) );
    connect( m_proc, TQ_SIGNAL( processExited( TDEProcess * ) ),
             this,   TQ_SLOT( slotVersionExited( TDEProcess * ) ) );
    if ( !m_proc->start( TDEProcess::NotifyOnExit, TDEProcess::Stdout ) ) {
        delete m_proc;
        m_proc = 0;
        m_errors << i18n( "Could not run the downloaded yt-dlp." );
        finish();
    }
}

void HelperUpdater::slotVersionOutput( TDEProcess *, char *buffer, int len )
{
    uint old = m_versionOut.length();
    m_versionOut.resize( old + len + 1 );
    memcpy( m_versionOut.data() + old, buffer, len );
    m_versionOut.data()[ old + len ] = '\0';
}

void HelperUpdater::slotVersionExited( TDEProcess *p )
{
    bool ok = p->normalExit() && p->exitStatus() == 0;
    TQString version = TQString::fromLatin1( m_versionOut.data() ).section( '\n', 0, 0 ).stripWhiteSpace();
    m_proc->deleteLater();
    m_proc = 0;

    if ( ok && !version.isEmpty() )
        Settings::self().managedYtdlpVersion = version;
    else
        m_errors << i18n( "The downloaded yt-dlp does not run. Is Python 3 (3.10 or newer) installed?" );
    finish();
}

void HelperUpdater::finish()
{
    Settings &s = Settings::self();
    if ( m_errors.isEmpty() )
        s.managedYtdlpChecked = TQDateTime::currentDateTime();
    s.save();
    Settings::forgetHelperInfo();
    m_step = Idle;

    if ( !m_errors.isEmpty() ) {
        for ( TQStringList::ConstIterator it = m_errors.begin(); it != m_errors.end(); ++it )
            kdWarning() << "tde-youtube: " << *it << endl;
        emit finished( Failed, m_errors.join( " " ) );
        return;
    }
    TQString versions = i18n( "yt-dlp %1, QuickJS-ng %2" )
                        .arg( s.managedYtdlpVersion ).arg( s.managedQjsVersion );
    emit finished( m_changed ? Updated : UpToDate, versions );
}

#include "helperupdater.moc"
