/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_PLAYERCONTROLS_H
#define TDEYOUTUBE_PLAYERCONTROLS_H

#include <tqwidget.h>

class TQLabel;
class TQSlider;
class TQToolButton;

/** Transport bar below the video: play/pause, stop, seek, time, volume and full screen. */
class PlayerControls : public TQWidget
{
    TQ_OBJECT

public:
    PlayerControls( TQWidget *parent = 0, const char *name = 0 );

public slots:
    void setActive( bool active );
    void setPaused( bool paused );
    void setPosition( double seconds );
    void setDuration( double seconds );
    void setVolume( int volume );
    void setBuffering( bool buffering );
    void setFullScreen( bool fullScreen );

signals:
    void playPauseClicked();
    void stopClicked();
    void seekRequested( double seconds );
    void volumeRequested( int volume );
    void fullScreenClicked();

private slots:
    void slotSliderPressed();
    void slotSliderReleased();
    void slotSliderMoved( int value );
    void slotVolume( int value );

private:
    void updateTime( double pos );

    TQToolButton *m_playPause;
    TQToolButton *m_stop;
    TQToolButton *m_fullScreen;
    TQSlider     *m_seek;
    TQSlider     *m_volume;
    TQLabel      *m_time;
    TQLabel      *m_volumeIcon;
    double        m_duration;
    bool          m_dragging;
    bool          m_paused;
    bool          m_buffering;
    bool          m_updatingVolume;
};

#endif
