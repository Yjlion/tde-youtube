/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mpvplayer.h"
#include "settings.h"
#include "searchbackend.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include <tqfile.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqregexp.h>
#include <tqsocketnotifier.h>
#include <tqstylesheet.h>
#include <tqtimer.h>
#include <tqwidgetstack.h>

#include <kprocess.h>
#include <tdelocale.h>
#include <kstandarddirs.h>
#include <tdeglobal.h>
#include <kdebug.h>

enum PropertyId {
    PropTimePos = 1, PropDuration, PropPause, PropVolume,
    PropBuffering, PropVideoFormat, PropFullScreen
};

MpvPlayer::MpvPlayer( TQWidget *parent, const char *name )
    : TQWidget( parent, name ),
      m_proc( 0 ), m_fd( -1 ), m_notifier( 0 ), m_connectAttempts( 0 ),
      m_external( false ), m_playing( false ), m_paused( false ), m_hasVideo( false ),
      m_position( 0 ), m_duration( 0 )
{
    setBackgroundColor( TQt::black );
    setSizePolicy( TQSizePolicy( TQSizePolicy::Expanding, TQSizePolicy::Expanding ) );
    setMinimumSize( 320, 180 );

    TQVBoxLayout *l = new TQVBoxLayout( this );
    m_stack = new TQWidgetStack( this );
    m_stack->setBackgroundColor( TQt::black );
    l->addWidget( m_stack );

    m_placeholder = new TQLabel( m_stack );
    m_placeholder->setAlignment( TQt::AlignCenter | TQt::WordBreak );
    m_placeholder->setPaletteBackgroundColor( TQt::black );
    m_placeholder->setPaletteForegroundColor( TQColor( 200, 200, 200 ) );
    m_placeholder->setMargin( 12 );
    // Never let long (error) texts widen the window.
    m_placeholder->setSizePolicy( TQSizePolicy( TQSizePolicy::Ignored, TQSizePolicy::Ignored ) );
    m_stack->addWidget( m_placeholder, 0 );

    m_videoWidget = new TQWidget( m_stack, "mpv video" );
    m_videoWidget->setBackgroundColor( TQt::black );
    m_stack->addWidget( m_videoWidget, 1 );

    m_connectTimer = new TQTimer( this );
    connect( m_connectTimer, TQ_SIGNAL( timeout() ), this, TQ_SLOT( tryConnect() ) );

    showPlaceholder( i18n( "<p><b>Nothing is playing</b></p>"
                           "<p>Search for videos above and double-click one to play it.</p>" ) );
}

MpvPlayer::~MpvPlayer()
{
    stop();
}

bool MpvPlayer::isActive() const
{
    return m_proc && m_proc->isRunning();
}

TQString MpvPlayer::socketPath() const
{
    return TDEGlobal::dirs()->saveLocation( "tmp" )
         + TQString( "tde-youtube-mpv-%1.sock" ).arg( getpid() );
}

void MpvPlayer::showPlaceholder( const TQString &text )
{
    m_placeholder->setText( "<qt><center>" + text + "</center></qt>" );
    m_stack->raiseWidget( m_placeholder );
}

void MpvPlayer::showVideo()
{
    if ( !m_external && m_hasVideo )
        m_stack->raiseWidget( m_videoWidget );
}

void MpvPlayer::play( const Video &video, bool external )
{
    stop();

    const Settings &s = Settings::self();
    m_video = video;
    m_external = external || !s.embedPlayer;
    m_position = m_duration = 0;
    m_paused = false;
    m_playing = false;
    m_hasVideo = false;
    m_errors.clear();
    emit positionChanged( 0 );
    emit durationChanged( 0 );

    showPlaceholder( i18n( "<p><b>Loading</b></p><p>%1</p>" )
                     .arg( TQStyleSheet::escape( video.title.isEmpty() ? video.watchUrl() : video.title ) ) );

    TQString sock = socketPath();
    TQFile::remove( sock );

    m_proc = new TDEProcess( this );
    m_proc->setUseShell( false );
    *m_proc << s.mpvPath
            << "--input-ipc-server=" + sock
            << "--force-window=yes" << "--idle=no" << "--keep-open=no"
            << "--no-input-terminal" << "--quiet" << "--msg-level=all=error"
            << "--ytdl=yes"
            << "--ytdl-format=" + s.formatSelector()
            << "--script-opts=ytdl_hook-ytdl_path=" + s.ytdlpProgram()
            << TQString( "--volume=%1" ).arg( s.volume );
    *m_proc << s.mpvCookieArgs();
    *m_proc << Settings::mpvJsRuntimeArgs();
    if ( m_external ) {
        *m_proc << "--title=" + ( video.title.isEmpty() ? TQString( "tde-youtube" ) : video.title + " - tde-youtube" );
    } else {
        m_stack->raiseWidget( m_videoWidget );   // make sure the window exists
        m_stack->raiseWidget( m_placeholder );
        *m_proc << TQString( "--wid=%1" ).arg( (unsigned long) m_videoWidget->winId() )
                << "--osc=no" << "--cursor-autohide=1000"
                // Only video outputs that can draw into a foreign window. Without
                // this mpv falls back to SDL on machines without OpenGL (VNC, xrdp)
                // and opens a separate window. Extra options below can override it.
                << "--vo=gpu,gpu-next,xv,x11";
    }
    if ( !s.extraMpvArgs.stripWhiteSpace().isEmpty() )
        *m_proc << TQStringList::split( TQRegExp( "\\s+" ), s.extraMpvArgs );
    *m_proc << "--" << video.watchUrl();

    connect( m_proc, TQ_SIGNAL( receivedStderr( TDEProcess *, char *, int ) ),
             this,   TQ_SLOT( slotStderr( TDEProcess *, char *, int ) ) );
    connect( m_proc, TQ_SIGNAL( receivedStdout( TDEProcess *, char *, int ) ),
             this,   TQ_SLOT( slotStderr( TDEProcess *, char *, int ) ) );
    connect( m_proc, TQ_SIGNAL( processExited( TDEProcess * ) ),
             this,   TQ_SLOT( slotProcessExited( TDEProcess * ) ) );

    if ( !m_proc->start( TDEProcess::NotifyOnExit, TDEProcess::AllOutput ) ) {
        delete m_proc;
        m_proc = 0;
        TQString msg = i18n( "Could not run mpv (\"%1\"). Please install mpv or set its "
                             "location in the settings." ).arg( s.mpvPath );
        showPlaceholder( "<p><b>" + i18n( "Playback failed" ) + "</b></p><p>"
                         + TQStyleSheet::escape( msg ) + "</p>" );
        emit playbackError( msg );
        return;
    }

    if ( m_external )
        showPlaceholder( i18n( "<p><b>Playing in an external mpv window</b></p><p>%1</p>" )
                         .arg( TQStyleSheet::escape( video.title ) ) );

    m_connectAttempts = 0;
    m_connectTimer->start( 100 );
}

void MpvPlayer::stop()
{
    m_connectTimer->stop();
    closeSocket();
    bool wasActive = m_proc != 0;
    if ( m_proc ) {
        m_proc->disconnect( this );
        if ( m_proc->isRunning() )
            m_proc->kill( SIGTERM );
        // Give mpv a moment to exit cleanly before TDEProcess is destroyed.
        TQTimer::singleShot( 2000, m_proc, TQ_SLOT( deleteLater() ) );
        m_proc = 0;
    }
    TQFile::remove( socketPath() );
    m_playing = false;
    m_paused = false;
    if ( wasActive ) {
        showPlaceholder( i18n( "<p><b>Stopped</b></p>" ) );
        emit stopped();
    }
}

void MpvPlayer::closeSocket()
{
    // This can run from inside the notifier's own activated() signal (mpv
    // closed the connection), so never delete the notifier synchronously.
    // Disabling it first also keeps its destructor from touching the fd
    // number, which may already belong to a new socket by then.
    if ( m_notifier ) {
        m_notifier->setEnabled( false );
        m_notifier->disconnect( this );
        m_notifier->deleteLater();
        m_notifier = 0;
    }
    if ( m_fd >= 0 ) {
        ::close( m_fd );
        m_fd = -1;
    }
    m_readBuffer.resize( 0 );
}

void MpvPlayer::tryConnect()
{
    if ( !m_proc ) {
        m_connectTimer->stop();
        return;
    }
    if ( ++m_connectAttempts > 150 ) {   // 15 seconds
        m_connectTimer->stop();
        kdWarning() << "tde-youtube: could not connect to mpv IPC socket" << endl;
        return;
    }

    TQCString path = TQFile::encodeName( socketPath() );
    int fd = ::socket( AF_UNIX, SOCK_STREAM, 0 );
    if ( fd < 0 )
        return;
    struct sockaddr_un addr;
    memset( &addr, 0, sizeof( addr ) );
    addr.sun_family = AF_UNIX;
    strncpy( addr.sun_path, path.data(), sizeof( addr.sun_path ) - 1 );
    if ( ::connect( fd, (struct sockaddr *) &addr, sizeof( addr ) ) < 0 ) {
        ::close( fd );
        return;   // not there yet, retry
    }
    ::fcntl( fd, F_SETFL, ::fcntl( fd, F_GETFL ) | O_NONBLOCK );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );

    m_connectTimer->stop();
    m_fd = fd;
    m_notifier = new TQSocketNotifier( fd, TQSocketNotifier::Read, this );
    connect( m_notifier, TQ_SIGNAL( activated( int ) ), this, TQ_SLOT( slotReadSocket() ) );

    sendCommand( "[\"observe_property\",1,\"time-pos\"]" );
    sendCommand( "[\"observe_property\",2,\"duration\"]" );
    sendCommand( "[\"observe_property\",3,\"pause\"]" );
    sendCommand( "[\"observe_property\",4,\"volume\"]" );
    sendCommand( "[\"observe_property\",5,\"paused-for-cache\"]" );
    sendCommand( "[\"observe_property\",6,\"video-format\"]" );
    sendCommand( "[\"observe_property\",7,\"fullscreen\"]" );
    if ( !m_external ) {
        // Route full screen requests from the embedded video to us (mpv >= 0.37).
        sendCommand( "[\"keybind\",\"f\",\"script-message tde-youtube toggle-fullscreen\"]" );
        sendCommand( "[\"keybind\",\"MBTN_LEFT_DBL\",\"script-message tde-youtube toggle-fullscreen\"]" );
        sendCommand( "[\"keybind\",\"ESC\",\"script-message tde-youtube exit-fullscreen\"]" );
    }
}

void MpvPlayer::sendCommand( const TQString &jsonArray )
{
    if ( m_fd < 0 )
        return;
    TQCString data = ( "{\"command\":" + jsonArray + "}\n" ).utf8();
    const char *p = data.data();
    int left = data.length();
    while ( left > 0 ) {
        ssize_t n = ::write( m_fd, p, left );
        if ( n < 0 ) {
            if ( errno == EINTR || errno == EAGAIN )
                continue;
            return;
        }
        p += n;
        left -= n;
    }
}

void MpvPlayer::slotReadSocket()
{
    char buf[4096];
    for ( ;; ) {
        ssize_t n = ::read( m_fd, buf, sizeof( buf ) );
        if ( n > 0 ) {
            uint old = m_readBuffer.length();
            m_readBuffer.resize( old + n + 1 );
            memcpy( m_readBuffer.data() + old, buf, n );
            m_readBuffer.data()[ old + n ] = '\0';
            continue;
        }
        if ( n == 0 ) {          // mpv closed the connection
            closeSocket();
            return;
        }
        if ( errno == EINTR )
            continue;
        break;                   // EAGAIN
    }

    int nl;
    while ( ( nl = m_readBuffer.find( '\n' ) ) >= 0 ) {
        TQString line = TQString::fromUtf8( m_readBuffer.data(), nl );
        m_readBuffer.remove( 0, nl + 1 );
        handleMessage( line );
    }
}

void MpvPlayer::handleMessage( const TQString &line )
{
    static TQRegExp eventRx( "\"event\":\"([^\"]+)\"" );
    static TQRegExp nameRx( "\"name\":\"([^\"]+)\"" );
    static TQRegExp dataRx( "\"data\":(\"(?:[^\"\\\\]|\\\\.)*\"|[^,}]+)" );
    static TQRegExp reasonRx( "\"reason\":\"([^\"]+)\"" );
    static TQRegExp fileErrorRx( "\"file_error\":\"([^\"]+)\"" );
    static TQRegExp argsRx( "\"args\":\\[([^\\]]*)\\]" );

    if ( eventRx.search( line ) < 0 )
        return;   // command reply
    TQString event = eventRx.cap( 1 );

    if ( event == "property-change" ) {
        if ( nameRx.search( line ) < 0 )
            return;
        TQString name = nameRx.cap( 1 );
        TQString data;
        bool hasData = dataRx.search( line ) >= 0;
        if ( hasData )
            data = dataRx.cap( 1 ).stripWhiteSpace();
        if ( data == "null" )
            hasData = false;

        if ( name == "time-pos" && hasData ) {
            m_position = data.toDouble();
            emit positionChanged( m_position );
        } else if ( name == "duration" && hasData ) {
            m_duration = data.toDouble();
            emit durationChanged( m_duration );
        } else if ( name == "pause" && hasData ) {
            m_paused = ( data == "true" );
            emit pausedChanged( m_paused );
        } else if ( name == "volume" && hasData ) {
            int v = (int) ( data.toDouble() + 0.5 );
            Settings::self().volume = v;
            emit volumeChanged( v );
        } else if ( name == "paused-for-cache" && hasData ) {
            emit bufferingChanged( data == "true" );
        } else if ( name == "video-format" ) {
            m_hasVideo = hasData;
            if ( m_playing ) {
                if ( m_hasVideo )
                    showVideo();
                else if ( !m_external )
                    showPlaceholder( i18n( "<p><b>Playing audio</b></p><p>%1</p>" )
                                     .arg( TQStyleSheet::escape( m_video.title ) ) );
            }
        } else if ( name == "fullscreen" && hasData && data == "true" && !m_external ) {
            sendCommand( "[\"set_property\",\"fullscreen\",false]" );
            emit fullScreenToggleRequested();
        }
    } else if ( event == "playback-restart" ) {
        if ( !m_playing ) {
            m_playing = true;
            if ( m_hasVideo )
                showVideo();
            else if ( !m_external )
                showPlaceholder( i18n( "<p><b>Playing audio</b></p><p>%1</p>" )
                                 .arg( TQStyleSheet::escape( m_video.title ) ) );
            emit started();
        }
    } else if ( event == "end-file" ) {
        if ( reasonRx.search( line ) >= 0 && reasonRx.cap( 1 ) == "error" ) {
            TQString err = fileErrorRx.search( line ) >= 0 ? fileErrorRx.cap( 1 ) : TQString::null;
            if ( !err.isEmpty() )
                m_errors.append( err );
        }
    } else if ( event == "client-message" ) {
        if ( argsRx.search( line ) >= 0 ) {
            TQString args = argsRx.cap( 1 );
            if ( args.contains( "\"tde-youtube\"" ) ) {
                if ( args.contains( "toggle-fullscreen" ) )
                    emit fullScreenToggleRequested();
                else if ( args.contains( "exit-fullscreen" ) )
                    emit fullScreenExitRequested();
            }
        }
    }
}

void MpvPlayer::slotStderr( TDEProcess *, char *buffer, int len )
{
    TQStringList lines = TQStringList::split( '\n', TQString::fromUtf8( buffer, len ) );
    for ( TQStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it ) {
        TQString l = ( *it ).stripWhiteSpace();
        if ( !l.isEmpty() )
            m_errors.append( l );
    }
    while ( m_errors.count() > 20 )
        m_errors.remove( m_errors.begin() );
}

void MpvPlayer::slotProcessExited( TDEProcess *p )
{
    m_connectTimer->stop();
    closeSocket();
    TQFile::remove( socketPath() );

    bool failed = !m_playing || !p->normalExit() || ( p->exitStatus() != 0 && p->exitStatus() != 4 );
    TQString errText;
    if ( failed && !m_errors.isEmpty() ) {
        // Prefer yt-dlp's own error message.
        for ( TQStringList::ConstIterator it = m_errors.begin(); it != m_errors.end(); ++it )
            if ( ( *it ).contains( "ERROR:" ) )
                errText = ( *it ).mid( ( *it ).find( "ERROR:" ) + 6 ).stripWhiteSpace();
        if ( errText.isEmpty() )
            errText = m_errors.last();
        errText = YtDlpJob::friendlyError( errText );
    }

    m_proc->deleteLater();
    m_proc = 0;
    m_playing = false;
    m_paused = false;

    if ( failed && ( !errText.isEmpty() || !p->normalExit() || p->exitStatus() != 0 ) ) {
        if ( errText.isEmpty() )
            errText = i18n( "mpv exited unexpectedly." );
        showPlaceholder( "<p><b>" + i18n( "Playback failed" ) + "</b></p><p>"
                         + TQStyleSheet::escape( errText ) + "</p>" );
        emit playbackError( errText );
    } else {
        showPlaceholder( i18n( "<p><b>Finished</b></p><p>%1</p>" )
                         .arg( TQStyleSheet::escape( m_video.title ) ) );
    }
    emit stopped();
}

void MpvPlayer::togglePause()
{
    sendCommand( "[\"cycle\",\"pause\"]" );
}

void MpvPlayer::setPaused( bool paused )
{
    sendCommand( TQString( "[\"set_property\",\"pause\",%1]" ).arg( paused ? "true" : "false" ) );
}

void MpvPlayer::seek( double seconds )
{
    sendCommand( TQString( "[\"seek\",%1,\"absolute\"]" ).arg( seconds, 0, 'f', 2 ) );
}

void MpvPlayer::seekRelative( double seconds )
{
    sendCommand( TQString( "[\"seek\",%1,\"relative\"]" ).arg( seconds, 0, 'f', 2 ) );
}

void MpvPlayer::setVolume( int volume )
{
    Settings::self().volume = volume;
    sendCommand( TQString( "[\"set_property\",\"volume\",%1]" ).arg( volume ) );
}

#include "mpvplayer.moc"
