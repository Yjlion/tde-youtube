/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_MPVPLAYER_H
#define TDEYOUTUBE_MPVPLAYER_H

#include <tqwidget.h>
#include <tqcstring.h>
#include <tqstringlist.h>

#include "video.h"

class TQLabel;
class TQSocketNotifier;
class TQTimer;
class TQWidgetStack;
class TDEProcess;

/**
 * Plays videos with mpv. mpv either renders into a child window of this
 * widget (--wid) or opens its own window; in both cases it is remote
 * controlled through its JSON IPC socket.
 */
class MpvPlayer : public TQWidget
{
    TQ_OBJECT

public:
    MpvPlayer( TQWidget *parent = 0, const char *name = 0 );
    ~MpvPlayer();

    bool isActive() const;
    bool isPaused() const { return m_paused; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    const Video &currentVideo() const { return m_video; }

public slots:
    /** Plays @p video. @p external forces a separate mpv window. */
    void play( const Video &video, bool external = false );
    void stop();
    void togglePause();
    void setPaused( bool paused );
    void seek( double seconds );
    void seekRelative( double seconds );
    void setVolume( int volume );

signals:
    void started();
    void stopped();
    void positionChanged( double seconds );
    void durationChanged( double seconds );
    void pausedChanged( bool paused );
    void volumeChanged( int volume );
    void bufferingChanged( bool buffering );
    void playbackError( const TQString &message );
    void fullScreenToggleRequested();
    void fullScreenExitRequested();

private slots:
    void tryConnect();
    void slotReadSocket();
    void slotStderr( TDEProcess *, char *buffer, int len );
    void slotProcessExited( TDEProcess * );

private:
    void sendCommand( const TQString &json );
    void handleMessage( const TQString &line );
    void showPlaceholder( const TQString &richText );
    void showVideo();
    void closeSocket();
    TQString socketPath() const;

    TQWidgetStack    *m_stack;
    TQLabel          *m_placeholder;
    TQWidget         *m_videoWidget;

    TDEProcess       *m_proc;
    int               m_fd;
    TQSocketNotifier *m_notifier;
    TQTimer          *m_connectTimer;
    int               m_connectAttempts;
    TQCString         m_readBuffer;
    TQStringList      m_errors;

    Video   m_video;
    bool    m_external;
    bool    m_playing;
    bool    m_paused;
    bool    m_hasVideo;
    double  m_position;
    double  m_duration;
};

#endif
