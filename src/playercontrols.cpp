/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "playercontrols.h"
#include "video.h"

#include <tqlabel.h>
#include <tqlayout.h>
#include <tqslider.h>
#include <tqtoolbutton.h>
#include <tqtooltip.h>

#include <kiconloader.h>
#include <tdelocale.h>

static TQString timeText( double s )
{
    int t = (int) s;
    if ( t < 0 ) t = 0;
    int h = t / 3600, m = ( t % 3600 ) / 60, sec = t % 60;
    if ( h > 0 )
        return TQString().sprintf( "%d:%02d:%02d", h, m, sec );
    return TQString().sprintf( "%d:%02d", m, sec );
}

PlayerControls::PlayerControls( TQWidget *parent, const char *name )
    : TQWidget( parent, name ),
      m_duration( 0 ), m_dragging( false ), m_paused( false ),
      m_buffering( false ), m_updatingVolume( false )
{
    TQHBoxLayout *l = new TQHBoxLayout( this, 2, 4 );

    m_playPause = new TQToolButton( this );
    m_playPause->setAutoRaise( true );
    m_playPause->setIconSet( SmallIconSet( "media-playback-pause" ) );
    TQToolTip::add( m_playPause, i18n( "Play / pause (Space)" ) );
    l->addWidget( m_playPause );

    m_stop = new TQToolButton( this );
    m_stop->setAutoRaise( true );
    m_stop->setIconSet( SmallIconSet( "media-playback-stop" ) );
    TQToolTip::add( m_stop, i18n( "Stop" ) );
    l->addWidget( m_stop );

    m_seek = new TQSlider( 0, 0, 5, 0, TQt::Horizontal, this );
    m_seek->setTracking( false );
    TQToolTip::add( m_seek, i18n( "Position" ) );
    l->addWidget( m_seek, 1 );

    m_time = new TQLabel( this );
    m_time->setMinimumWidth( fontMetrics().width( "00:00:00 / 00:00:00" ) + 4 );
    m_time->setAlignment( TQt::AlignCenter );
    l->addWidget( m_time );

    m_volumeIcon = new TQLabel( this );
    m_volumeIcon->setPixmap( SmallIcon( "player_volume" ) );
    l->addWidget( m_volumeIcon );

    m_volume = new TQSlider( 0, 100, 5, 80, TQt::Horizontal, this );
    m_volume->setFixedWidth( 90 );
    TQToolTip::add( m_volume, i18n( "Volume" ) );
    l->addWidget( m_volume );

    m_fullScreen = new TQToolButton( this );
    m_fullScreen->setAutoRaise( true );
    m_fullScreen->setIconSet( SmallIconSet( "view-fullscreen" ) );
    TQToolTip::add( m_fullScreen, i18n( "Full screen (F)" ) );
    l->addWidget( m_fullScreen );

    connect( m_playPause,  TQ_SIGNAL( clicked() ), this, TQ_SIGNAL( playPauseClicked() ) );
    connect( m_stop,       TQ_SIGNAL( clicked() ), this, TQ_SIGNAL( stopClicked() ) );
    connect( m_fullScreen, TQ_SIGNAL( clicked() ), this, TQ_SIGNAL( fullScreenClicked() ) );
    connect( m_seek,   TQ_SIGNAL( sliderPressed() ),  this, TQ_SLOT( slotSliderPressed() ) );
    connect( m_seek,   TQ_SIGNAL( sliderReleased() ), this, TQ_SLOT( slotSliderReleased() ) );
    connect( m_seek,   TQ_SIGNAL( sliderMoved( int ) ), this, TQ_SLOT( slotSliderMoved( int ) ) );
    connect( m_seek,   TQ_SIGNAL( valueChanged( int ) ), this, TQ_SLOT( slotSliderMoved( int ) ) );
    connect( m_volume, TQ_SIGNAL( valueChanged( int ) ), this, TQ_SLOT( slotVolume( int ) ) );

    setActive( false );
}

void PlayerControls::setActive( bool active )
{
    m_playPause->setEnabled( active );
    m_stop->setEnabled( active );
    m_seek->setEnabled( active && m_duration > 0 );
    if ( !active ) {
        m_duration = 0;
        m_seek->setRange( 0, 0 );
        m_buffering = false;
        setPaused( false );
        updateTime( 0 );
    }
}

void PlayerControls::setPaused( bool paused )
{
    m_paused = paused;
    m_playPause->setIconSet( SmallIconSet( paused ? "media-playback-start" : "media-playback-pause" ) );
}

void PlayerControls::setPosition( double s )
{
    if ( m_dragging )
        return;
    m_seek->blockSignals( true );
    m_seek->setValue( (int) s );
    m_seek->blockSignals( false );
    updateTime( s );
}

void PlayerControls::setDuration( double s )
{
    m_duration = s;
    m_seek->setRange( 0, (int) s );
    m_seek->setPageStep( TQMAX( 5, (int) s / 20 ) );
    m_seek->setEnabled( s > 0 && m_stop->isEnabled() );
    updateTime( m_seek->value() );
}

void PlayerControls::setVolume( int v )
{
    m_updatingVolume = true;
    m_volume->setValue( v );
    m_updatingVolume = false;
}

void PlayerControls::setBuffering( bool b )
{
    m_buffering = b;
    updateTime( m_seek->value() );
}

void PlayerControls::setFullScreen( bool fs )
{
    m_fullScreen->setIconSet( SmallIconSet( fs ? "view-restore" : "view-fullscreen" ) );
}

void PlayerControls::updateTime( double pos )
{
    TQString t = timeText( pos ) + " / " + ( m_duration > 0 ? timeText( m_duration ) : TQString( "--:--" ) );
    if ( m_buffering )
        t = i18n( "Buffering..." );
    m_time->setText( t );
}

void PlayerControls::slotSliderPressed()
{
    m_dragging = true;
}

void PlayerControls::slotSliderReleased()
{
    m_dragging = false;
    emit seekRequested( m_seek->value() );
}

void PlayerControls::slotSliderMoved( int v )
{
    updateTime( v );
    // Clicks on the groove / keyboard changes (not drags) seek immediately.
    if ( !m_dragging && sender() == m_seek && !m_seek->signalsBlocked() )
        emit seekRequested( v );
}

void PlayerControls::slotVolume( int v )
{
    if ( !m_updatingVolume )
        emit volumeRequested( v );
}

#include "playercontrols.moc"
