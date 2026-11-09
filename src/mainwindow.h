/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_MAINWINDOW_H
#define TDEYOUTUBE_MAINWINDOW_H

#include <tdemainwindow.h>

#include "video.h"

class TQLabel;
class TQSplitter;
class TQVBox;
class KHistoryCombo;
class KPushButton;
class KTabWidget;
class TDEAction;
class TDEToggleAction;
class SearchBackend;
class VideoListView;
class DetailsPane;
class MpvPlayer;
class PlayerControls;

class MainWindow : public TDEMainWindow
{
    TQ_OBJECT

public:
    MainWindow();
    ~MainWindow();

public slots:
    /** Searches YouTube for @p query (or lists the videos behind a URL). */
    void search( const TQString &query );
    /** Opens a YouTube URL: single videos are played right away. */
    void openUrl( const TQString &url );
    void playVideo( const Video &video );

protected:
    virtual bool queryClose();

private slots:
    void slotSearch();
    void slotMoreResults();
    void slotVideoFound( const Video &video );
    void slotSearchFinished( int count );
    void slotSearchError( const TQString &message );

    void slotOpenUrl();
    void slotFocusSearch();
    void slotPlay();
    void slotPlayExternal();
    void slotToggleFavourite();
    void slotCopyUrl();
    void slotOpenInBrowser();
    void slotShowChannel();
    void slotRemoveFromList();
    void slotShowSearch();
    void slotShowFavourites();
    void slotShowHistory();
    void slotSeekForward();
    void slotSeekBackward();
    void slotVolumeUp();
    void slotVolumeDown();
    void slotToggleFullScreen();
    void slotExitFullScreen();
    void slotPreferences();
    void slotSettingsChanged();

    void slotCurrentChanged( const Video &video );
    void slotContextMenu( const Video &video, const TQPoint &pos );
    void slotTabChanged( TQWidget * );
    /** The library changed: rebuild the list later, never inside a list event. */
    void slotFavouritesChanged();
    void slotHistoryChanged();
    void refreshFavourites();
    void refreshHistory();

    void slotPlayerStarted();
    void slotPlayerStopped();
    void slotPlayerError( const TQString &message );
    void slotPausedChanged( bool paused );

    /** First start: ask about managed yt-dlp; later: daily update check. */
    void slotStartupYtdlp();
    void slotYtdlpUpdateStarted();
    void slotYtdlpUpdateFinished( int result, const TQString &message );

private:
    void setupWidgets();
    void setupActions();
    void updateActions();
    void setFullScreen( bool on );
    void setStatus( const TQString &text );
    VideoListView *currentList() const;
    /** Video the actions apply to: current list item, else the playing video. */
    Video selectedVideo() const;
    void saveLayout();
    void restoreLayout();
    /** yt-dlp failed with @p message; update it if that looks like the cause. */
    void maybeUpdateYtdlp( const TQString &message );

    // widgets
    TQSplitter     *m_hsplit;
    TQSplitter     *m_vsplit;
    KTabWidget     *m_tabs;
    TQVBox         *m_searchPage;
    VideoListView  *m_searchList;
    VideoListView  *m_favList;
    VideoListView  *m_historyList;
    KPushButton    *m_moreButton;
    TQWidget       *m_playerBox;
    MpvPlayer      *m_player;
    PlayerControls *m_controls;
    DetailsPane    *m_details;
    KHistoryCombo  *m_searchCombo;
    TQLabel        *m_statusLabel;

    // actions
    TDEAction       *m_playAction;
    TDEAction       *m_playExternalAction;
    TDEAction       *m_pauseAction;
    TDEAction       *m_stopAction;
    TDEAction       *m_favAction;
    TDEAction       *m_copyAction;
    TDEAction       *m_browserAction;
    TDEAction       *m_channelAction;
    TDEAction       *m_removeAction;
    TDEAction       *m_moreAction;
    TDEAction       *m_exitFullScreenAction;
    TDEToggleAction *m_fullScreenAction;

    SearchBackend *m_backend;
    TQString       m_lastQuery;
    int            m_requested;
    bool           m_autoPlayFirst;
    bool           m_fullScreen;
    bool           m_menuWasVisible, m_statusWasVisible;
    bool           m_favRefreshPending, m_historyRefreshPending;
    Video          m_retryVideo;   ///< play again once yt-dlp has been updated
    TQValueList<bool> m_toolbarsVisible;
};

#endif
