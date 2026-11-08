/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mainwindow.h"
#include "detailspane.h"
#include "library.h"
#include "mpvplayer.h"
#include "playercontrols.h"
#include "searchbackend.h"
#include "settings.h"
#include "settingsdialog.h"
#include "videolist.h"

#include <tqclipboard.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqregexp.h>
#include <tqsplitter.h>
#include <tqtimer.h>
#include <tqtooltip.h>
#include <tqvbox.h>

#include <tdeaction.h>
#include <tdeapplication.h>
#include <kcombobox.h>
#include <klineedit.h>
#include <tdeconfig.h>
#include <kiconloader.h>
#include <kinputdialog.h>
#include <tdelocale.h>
#include <tdemenubar.h>
#include <tdemessagebox.h>
#include <tdepopupmenu.h>
#include <kpushbutton.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <ktabwidget.h>
#include <tdetoolbar.h>
#include <tdeglobal.h>

MainWindow::MainWindow()
    : TDEMainWindow( 0, "tde-youtube-mainwindow" ),
      m_requested( 0 ), m_autoPlayFirst( false ), m_fullScreen( false ),
      m_menuWasVisible( true ), m_statusWasVisible( true )
{
    m_backend = new SearchBackend( this );
    connect( m_backend, TQ_SIGNAL( videoFound( const Video & ) ), this, TQ_SLOT( slotVideoFound( const Video & ) ) );
    connect( m_backend, TQ_SIGNAL( finished( int ) ), this, TQ_SLOT( slotSearchFinished( int ) ) );
    connect( m_backend, TQ_SIGNAL( error( const TQString & ) ), this, TQ_SLOT( slotSearchError( const TQString & ) ) );

    setupWidgets();
    setupActions();

    m_statusLabel = new TQLabel( statusBar() );
    m_statusLabel->setSizePolicy( TQSizePolicy( TQSizePolicy::Ignored, TQSizePolicy::Preferred ) );
    m_statusLabel->setMinimumWidth( 1 );
    statusBar()->addWidget( m_statusLabel, 1 );
    setStatus( i18n( "Ready" ) );

    setupGUI( ToolBar | Keys | StatusBar | Save | Create );
    restoreLayout();
    updateActions();

    connect( Library::self(), TQ_SIGNAL( favouritesChanged() ), this, TQ_SLOT( slotFavouritesChanged() ) );
    connect( Library::self(), TQ_SIGNAL( historyChanged() ), this, TQ_SLOT( slotHistoryChanged() ) );
    slotFavouritesChanged();
    slotHistoryChanged();

    m_searchCombo->setFocus();
}

MainWindow::~MainWindow()
{
}

/* ----------------------------------------------------------------- setup */

void MainWindow::setupWidgets()
{
    m_hsplit = new TQSplitter( TQt::Horizontal, this, "main splitter" );

    m_tabs = new KTabWidget( m_hsplit );

    m_searchPage = new TQVBox( m_tabs );
    m_searchPage->setSpacing( 0 );
    m_searchList = new VideoListView( m_searchPage, "search results" );
    m_searchList->setEmptyText( i18n( "Type something into the search box above and press Enter "
                                      "to search YouTube.\n\nYou can also paste the address of a video, "
                                      "playlist or channel." ) );
    m_moreButton = new KPushButton( SmallIconSet( "go-down" ), i18n( "More Results" ), m_searchPage );
    m_moreButton->hide();
    connect( m_moreButton, TQ_SIGNAL( clicked() ), this, TQ_SLOT( slotMoreResults() ) );
    m_tabs->addTab( m_searchPage, SmallIconSet( "edit-find" ), i18n( "Search" ) );

    m_favList = new VideoListView( m_tabs, "favourites" );
    m_favList->setEmptyText( i18n( "You have no favourites yet.\n\nRight-click a video and choose "
                                   "\"Add to Favourites\" (Ctrl+D) to keep it here." ) );
    m_tabs->addTab( m_favList, SmallIconSet( "bookmark" ), i18n( "Favourites" ) );

    m_historyList = new VideoListView( m_tabs, "history" );
    m_historyList->setEmptyText( i18n( "Videos you watch are listed here." ) );
    m_tabs->addTab( m_historyList, SmallIconSet( "history" ), i18n( "History" ) );

    connect( m_tabs, TQ_SIGNAL( currentChanged( TQWidget * ) ), this, TQ_SLOT( slotTabChanged( TQWidget * ) ) );

    VideoListView *lists[] = { m_searchList, m_favList, m_historyList };
    for ( int i = 0; i < 3; ++i ) {
        connect( lists[i], TQ_SIGNAL( videoActivated( const Video & ) ), this, TQ_SLOT( playVideo( const Video & ) ) );
        connect( lists[i], TQ_SIGNAL( currentVideoChanged( const Video & ) ), this, TQ_SLOT( slotCurrentChanged( const Video & ) ) );
        connect( lists[i], TQ_SIGNAL( videoContextMenu( const Video &, const TQPoint & ) ),
                 this, TQ_SLOT( slotContextMenu( const Video &, const TQPoint & ) ) );
    }

    m_vsplit = new TQSplitter( TQt::Vertical, m_hsplit, "player splitter" );

    m_playerBox = new TQWidget( m_vsplit );
    TQVBoxLayout *pl = new TQVBoxLayout( m_playerBox, 0, 0 );
    m_player = new MpvPlayer( m_playerBox );
    m_controls = new PlayerControls( m_playerBox );
    pl->addWidget( m_player, 1 );
    pl->addWidget( m_controls );

    m_details = new DetailsPane( m_vsplit );

    m_hsplit->setResizeMode( m_tabs, TQSplitter::KeepSize );
    m_vsplit->setResizeMode( m_details, TQSplitter::KeepSize );

    connect( m_controls, TQ_SIGNAL( playPauseClicked() ), m_player, TQ_SLOT( togglePause() ) );
    connect( m_controls, TQ_SIGNAL( stopClicked() ), m_player, TQ_SLOT( stop() ) );
    connect( m_controls, TQ_SIGNAL( seekRequested( double ) ), m_player, TQ_SLOT( seek( double ) ) );
    connect( m_controls, TQ_SIGNAL( volumeRequested( int ) ), m_player, TQ_SLOT( setVolume( int ) ) );
    connect( m_controls, TQ_SIGNAL( fullScreenClicked() ), this, TQ_SLOT( slotToggleFullScreen() ) );

    connect( m_player, TQ_SIGNAL( positionChanged( double ) ), m_controls, TQ_SLOT( setPosition( double ) ) );
    connect( m_player, TQ_SIGNAL( durationChanged( double ) ), m_controls, TQ_SLOT( setDuration( double ) ) );
    connect( m_player, TQ_SIGNAL( pausedChanged( bool ) ), m_controls, TQ_SLOT( setPaused( bool ) ) );
    connect( m_player, TQ_SIGNAL( pausedChanged( bool ) ), this, TQ_SLOT( slotPausedChanged( bool ) ) );
    connect( m_player, TQ_SIGNAL( volumeChanged( int ) ), m_controls, TQ_SLOT( setVolume( int ) ) );
    connect( m_player, TQ_SIGNAL( bufferingChanged( bool ) ), m_controls, TQ_SLOT( setBuffering( bool ) ) );
    connect( m_player, TQ_SIGNAL( started() ), this, TQ_SLOT( slotPlayerStarted() ) );
    connect( m_player, TQ_SIGNAL( stopped() ), this, TQ_SLOT( slotPlayerStopped() ) );
    connect( m_player, TQ_SIGNAL( playbackError( const TQString & ) ), this, TQ_SLOT( slotPlayerError( const TQString & ) ) );
    connect( m_player, TQ_SIGNAL( fullScreenToggleRequested() ), this, TQ_SLOT( slotToggleFullScreen() ) );
    connect( m_player, TQ_SIGNAL( fullScreenExitRequested() ), this, TQ_SLOT( slotExitFullScreen() ) );

    m_controls->setVolume( Settings::self().volume );

    setCentralWidget( m_hsplit );
}

void MainWindow::setupActions()
{
    TDEActionCollection *ac = actionCollection();

    // Search box in the tool bar
    m_searchCombo = new KHistoryCombo( true, this, "search combo" );
    m_searchCombo->setMinimumWidth( 280 );
    m_searchCombo->setMaxCount( 30 );
    m_searchCombo->setDuplicatesEnabled( false );
    if ( KLineEdit *le = dynamic_cast<KLineEdit *>( m_searchCombo->lineEdit() ) )
        le->setClickMessage( i18n( "Search YouTube or paste a link" ) );
    connect( m_searchCombo, TQ_SIGNAL( returnPressed( const TQString & ) ), this, TQ_SLOT( slotSearch() ) );

    KWidgetAction *wa = new KWidgetAction( m_searchCombo, i18n( "Search Box" ), 0, 0, 0, ac, "search_box" );
    wa->setAutoSized( true );
    wa->setShortcutConfigurable( false );

    new TDEAction( i18n( "&Search" ), "edit-find", TQt::Key_F6, this, TQ_SLOT( slotSearch() ), ac, "search" );
    new TDEAction( i18n( "Focus Search Box" ), "edit-find", TQt::CTRL + TQt::Key_F, this, TQ_SLOT( slotFocusSearch() ), ac, "search_focus" );
    m_moreAction = new TDEAction( i18n( "&More Results" ), "go-down", TQt::CTRL + TQt::Key_M, this, TQ_SLOT( slotMoreResults() ), ac, "search_more" );

    new TDEAction( i18n( "&Open URL..." ), "fileopen", TQt::CTRL + TQt::Key_O, this, TQ_SLOT( slotOpenUrl() ), ac, "open_url" );
    KStdAction::quit( this, TQ_SLOT( close() ), ac );

    m_playAction = new TDEAction( i18n( "&Play" ), "media-playback-start", TQt::CTRL + TQt::Key_P, this, TQ_SLOT( slotPlay() ), ac, "video_play" );
    m_playExternalAction = new TDEAction( i18n( "Play in &External Player" ), "window_new", TQt::CTRL + TQt::SHIFT + TQt::Key_P,
                                          this, TQ_SLOT( slotPlayExternal() ), ac, "video_play_external" );
    m_pauseAction = new TDEAction( i18n( "P&ause" ), "media-playback-pause", TQt::Key_Space, m_player, TQ_SLOT( togglePause() ), ac, "player_pause" );
    m_stopAction = new TDEAction( i18n( "S&top" ), "media-playback-stop", TQt::CTRL + TQt::Key_Period, m_player, TQ_SLOT( stop() ), ac, "player_stop" );
    new TDEAction( i18n( "Skip &Forward" ), "media-seek-forward", TQt::CTRL + TQt::Key_Right, this, TQ_SLOT( slotSeekForward() ), ac, "player_forward" );
    new TDEAction( i18n( "Skip &Backward" ), "media-seek-backward", TQt::CTRL + TQt::Key_Left, this, TQ_SLOT( slotSeekBackward() ), ac, "player_backward" );
    new TDEAction( i18n( "Volume &Up" ), "player_volume", TQt::CTRL + TQt::Key_Up, this, TQ_SLOT( slotVolumeUp() ), ac, "volume_up" );
    new TDEAction( i18n( "Volume &Down" ), "player_volume", TQt::CTRL + TQt::Key_Down, this, TQ_SLOT( slotVolumeDown() ), ac, "volume_down" );

    m_favAction = new TDEAction( i18n( "Add to &Favourites" ), "bookmark_add", TQt::CTRL + TQt::Key_D, this, TQ_SLOT( slotToggleFavourite() ), ac, "video_favourite" );
    m_copyAction = new TDEAction( i18n( "&Copy Video Address" ), "edit-copy", TQt::CTRL + TQt::SHIFT + TQt::Key_C, this, TQ_SLOT( slotCopyUrl() ), ac, "video_copy_url" );
    m_browserAction = new TDEAction( i18n( "Open in &Web Browser" ), "konqueror", TQt::CTRL + TQt::Key_B, this, TQ_SLOT( slotOpenInBrowser() ), ac, "video_open_browser" );
    m_channelAction = new TDEAction( i18n( "Show &Channel Videos" ), "folder_video", TQt::CTRL + TQt::Key_U, this, TQ_SLOT( slotShowChannel() ), ac, "video_channel" );
    m_removeAction = new TDEAction( i18n( "&Remove from List" ), "edit-delete", TQt::Key_Delete, this, TQ_SLOT( slotRemoveFromList() ), ac, "video_remove" );

    new TDEAction( i18n( "Search &Results" ), "edit-find", TQt::CTRL + TQt::Key_1, this, TQ_SLOT( slotShowSearch() ), ac, "view_search" );
    new TDEAction( i18n( "&Favourites" ), "bookmark", TQt::CTRL + TQt::Key_2, this, TQ_SLOT( slotShowFavourites() ), ac, "view_favourites" );
    new TDEAction( i18n( "&History" ), "history", TQt::CTRL + TQt::Key_3, this, TQ_SLOT( slotShowHistory() ), ac, "view_history" );

    m_fullScreenAction = new TDEToggleAction( i18n( "F&ull Screen Mode" ), "view-fullscreen", TQt::CTRL + TQt::SHIFT + TQt::Key_F,
                                              this, TQ_SLOT( slotToggleFullScreen() ), ac, "fullscreen" );
    m_fullScreenAction->setCheckedState( KGuiItem( i18n( "Exit F&ull Screen Mode" ), "view-restore" ) );
    m_exitFullScreenAction = new TDEAction( i18n( "Exit Full Screen Mode" ), "view-restore", TQt::Key_Escape,
                                            this, TQ_SLOT( slotExitFullScreen() ), ac, "exit_fullscreen" );
    m_exitFullScreenAction->setEnabled( false );

    KStdAction::preferences( this, TQ_SLOT( slotPreferences() ), ac );

    // Restore the search history
    TDEConfig *c = TDEGlobal::config();
    c->setGroup( "Search" );
    m_searchCombo->setHistoryItems( c->readListEntry( "History" ), true );
    m_searchCombo->clearEdit();
}

void MainWindow::restoreLayout()
{
    TDEConfig *c = TDEGlobal::config();
    c->setGroup( "Layout" );
    TQValueList<int> h = c->readIntListEntry( "HorizontalSplitter" );
    TQValueList<int> v = c->readIntListEntry( "VerticalSplitter" );
    if ( h.count() != 2 ) { h.clear(); h << 420 << 780; }
    if ( v.count() != 2 ) { v.clear(); v << 480 << 200; }
    m_hsplit->setSizes( h );
    m_vsplit->setSizes( v );
    if ( !initialGeometrySet() && !c->hasKey( "Initialised" ) )
        resize( 1200, 760 );
}

void MainWindow::saveLayout()
{
    TDEConfig *c = TDEGlobal::config();
    c->setGroup( "Layout" );
    if ( !m_fullScreen ) {
        c->writeEntry( "HorizontalSplitter", m_hsplit->sizes() );
        c->writeEntry( "VerticalSplitter", m_vsplit->sizes() );
    }
    c->writeEntry( "Initialised", true );
    c->setGroup( "Search" );
    c->writeEntry( "History", m_searchCombo->historyItems() );
    Settings::self().save();
    c->sync();
}

bool MainWindow::queryClose()
{
    if ( m_fullScreen )
        setFullScreen( false );
    saveLayout();
    m_player->stop();
    return true;
}

/* ---------------------------------------------------------------- helpers */

void MainWindow::setStatus( const TQString &text )
{
    TQString t = text.simplifyWhiteSpace();
    m_statusLabel->setText( t );
    TQToolTip::remove( m_statusLabel );
    if ( t.length() > 80 )
        TQToolTip::add( m_statusLabel, t );
}

VideoListView *MainWindow::currentList() const
{
    TQWidget *w = m_tabs->currentPage();
    if ( w == m_favList )     return m_favList;
    if ( w == m_historyList ) return m_historyList;
    return m_searchList;
}

Video MainWindow::selectedVideo() const
{
    Video v = currentList()->currentVideo();
    if ( !v.isValid() && m_player->isActive() )
        v = m_player->currentVideo();
    return v;
}

void MainWindow::updateActions()
{
    Video v = selectedVideo();
    bool has = v.isValid();
    bool active = m_player->isActive();

    m_playAction->setEnabled( has );
    m_playExternalAction->setEnabled( has );
    m_favAction->setEnabled( has );
    m_copyAction->setEnabled( has );
    m_browserAction->setEnabled( has );
    m_channelAction->setEnabled( has && !v.channelUrl.isEmpty() );
    m_removeAction->setEnabled( has && currentList() != m_searchList && currentList()->currentVideo().isValid() );
    m_pauseAction->setEnabled( active );
    m_stopAction->setEnabled( active );
    m_moreAction->setEnabled( !m_lastQuery.isEmpty() && !m_backend->isRunning() );

    if ( has && Library::self()->isFavourite( v.id ) ) {
        m_favAction->setText( i18n( "Remove from &Favourites" ) );
        m_favAction->setIcon( "bookmark" );
    } else {
        m_favAction->setText( i18n( "Add to &Favourites" ) );
        m_favAction->setIcon( "bookmark_add" );
    }
}

/* ----------------------------------------------------------------- search */

void MainWindow::slotSearch()
{
    TQString q = m_searchCombo->currentText().stripWhiteSpace();
    if ( q.isEmpty() ) {
        m_searchCombo->setFocus();
        return;
    }
    m_searchCombo->addToHistory( q );
    if ( SearchBackend::looksLikeUrl( q ) )
        openUrl( q );
    else
        search( q );
}

void MainWindow::search( const TQString &query )
{
    m_lastQuery = query;
    m_requested = Settings::self().resultsPerPage;
    if ( m_searchCombo->currentText() != query )
        m_searchCombo->setEditText( query );
    m_searchList->clearVideos();
    m_moreButton->hide();
    m_tabs->showPage( m_searchPage );
    setStatus( SearchBackend::looksLikeUrl( query ) ? i18n( "Loading %1..." ).arg( query )
                                                    : i18n( "Searching for \"%1\"..." ).arg( query ) );
    m_searchList->setEmptyText( i18n( "Searching..." ) );
    m_backend->search( query, m_requested );
    updateActions();
}

void MainWindow::openUrl( const TQString &url )
{
    TQString u = url.stripWhiteSpace();
    TQRegExp single( "(youtube\\.com/(watch\\?|shorts/|live/|embed/)|youtu\\.be/)" );
    m_autoPlayFirst = single.search( u ) >= 0 && !u.contains( "list=" );
    search( u );
}

void MainWindow::slotMoreResults()
{
    if ( m_lastQuery.isEmpty() || m_backend->isRunning() )
        return;
    // yt-dlp cannot page; ask for more and skip the ones already listed.
    m_requested += Settings::self().resultsPerPage;
    m_moreButton->setEnabled( false );
    setStatus( i18n( "Loading more results..." ) );
    m_backend->search( m_lastQuery, m_requested );
    updateActions();
}

void MainWindow::slotVideoFound( const Video &video )
{
    m_searchList->addVideo( video );
    if ( m_searchList->videoCount() == 1 ) {
        m_searchList->setCurrentItem( m_searchList->firstChild() );
        m_searchList->setSelected( m_searchList->firstChild(), true );
        // TQListView may already have made the item current without telling us.
        if ( currentList() == m_searchList )
            slotCurrentChanged( video );
        if ( m_autoPlayFirst ) {
            m_autoPlayFirst = false;
            playVideo( video );
        }
    }
}

void MainWindow::slotSearchFinished( int count )
{
    m_autoPlayFirst = false;
    m_moreButton->setEnabled( true );
    m_moreButton->setShown( count >= m_requested );
    m_searchList->setEmptyText( i18n( "No videos found." ) );
    setStatus( i18n( "One video found.", "%n videos found.", m_searchList->videoCount() ) );
    updateActions();
}

void MainWindow::slotSearchError( const TQString &message )
{
    m_autoPlayFirst = false;
    m_moreButton->setEnabled( true );
    m_searchList->setEmptyText( i18n( "The search failed:\n%1" ).arg( message ) );
    setStatus( i18n( "Search failed: %1" ).arg( message ) );
    updateActions();
}

/* ---------------------------------------------------------------- actions */

void MainWindow::slotOpenUrl()
{
    bool ok = false;
    TQString clip = TQApplication::clipboard()->text().stripWhiteSpace();
    TQString url = KInputDialog::getText( i18n( "Open URL" ),
                                         i18n( "Address of a YouTube video, playlist or channel:" ),
                                         SearchBackend::looksLikeUrl( clip ) ? clip : TQString::null, &ok, this );
    if ( ok && !url.stripWhiteSpace().isEmpty() ) {
        m_searchCombo->addToHistory( url.stripWhiteSpace() );
        openUrl( url );
    }
}

void MainWindow::slotFocusSearch()
{
    if ( m_fullScreen )
        setFullScreen( false );
    m_searchCombo->setFocus();
    m_searchCombo->lineEdit()->selectAll();
}

void MainWindow::playVideo( const Video &video )
{
    if ( !video.isValid() )
        return;
    m_details->setVideo( video );
    Library::self()->addToHistory( video );
    m_player->play( video );
    m_controls->setActive( true );
    setCaption( video.title );
    setStatus( i18n( "Loading \"%1\"..." ).arg( video.title ) );
    updateActions();
}

void MainWindow::slotPlay()
{
    playVideo( selectedVideo() );
}

void MainWindow::slotPlayExternal()
{
    Video v = selectedVideo();
    if ( !v.isValid() )
        return;
    m_details->setVideo( v );
    Library::self()->addToHistory( v );
    m_player->play( v, true );
    m_controls->setActive( true );
    setStatus( i18n( "Playing \"%1\" in an external window." ).arg( v.title ) );
    updateActions();
}

void MainWindow::slotToggleFavourite()
{
    Video v = selectedVideo();
    if ( !v.isValid() )
        return;
    if ( Library::self()->isFavourite( v.id ) ) {
        Library::self()->removeFavourite( v.id );
        setStatus( i18n( "Removed \"%1\" from your favourites." ).arg( v.title ) );
    } else {
        Library::self()->addFavourite( v );
        setStatus( i18n( "Added \"%1\" to your favourites." ).arg( v.title ) );
    }
    updateActions();
}

void MainWindow::slotCopyUrl()
{
    Video v = selectedVideo();
    if ( !v.isValid() )
        return;
    TQApplication::clipboard()->setText( v.watchUrl(), TQClipboard::Clipboard );
    TQApplication::clipboard()->setText( v.watchUrl(), TQClipboard::Selection );
    setStatus( i18n( "Copied %1 to the clipboard." ).arg( v.watchUrl() ) );
}

void MainWindow::slotOpenInBrowser()
{
    Video v = selectedVideo();
    if ( v.isValid() )
        kapp->invokeBrowser( v.watchUrl() );
}

void MainWindow::slotShowChannel()
{
    Video v = selectedVideo();
    if ( !v.isValid() || v.channelUrl.isEmpty() )
        return;
    m_searchCombo->addToHistory( v.channelUrl );
    search( v.channelUrl );
}

void MainWindow::slotRemoveFromList()
{
    VideoListView *l = currentList();
    Video v = l->currentVideo();
    if ( !v.isValid() )
        return;
    if ( l == m_favList )
        Library::self()->removeFavourite( v.id );
    else if ( l == m_historyList )
        Library::self()->removeFromHistory( v.id );
}

void MainWindow::slotShowSearch()     { m_tabs->showPage( m_searchPage ); }
void MainWindow::slotShowFavourites() { m_tabs->showPage( m_favList ); }
void MainWindow::slotShowHistory()    { m_tabs->showPage( m_historyList ); }

void MainWindow::slotSeekForward()  { m_player->seekRelative( 10 ); }
void MainWindow::slotSeekBackward() { m_player->seekRelative( -10 ); }

void MainWindow::slotVolumeUp()
{
    int v = TQMIN( 100, Settings::self().volume + 5 );
    m_player->setVolume( v );
    m_controls->setVolume( v );
}

void MainWindow::slotVolumeDown()
{
    int v = TQMAX( 0, Settings::self().volume - 5 );
    m_player->setVolume( v );
    m_controls->setVolume( v );
}

void MainWindow::slotToggleFullScreen()
{
    // Called from the action (already toggled), the control button or mpv.
    bool want = ( sender() == m_fullScreenAction ) ? m_fullScreenAction->isChecked() : !m_fullScreen;
    setFullScreen( want );
}

void MainWindow::slotExitFullScreen()
{
    setFullScreen( false );
}

void MainWindow::setFullScreen( bool on )
{
    if ( on == m_fullScreen ) {
        m_fullScreenAction->setChecked( on );
        return;
    }
    m_fullScreen = on;
    m_fullScreenAction->setChecked( on );
    m_exitFullScreenAction->setEnabled( on );
    m_controls->setFullScreen( on );

    if ( on ) {
        saveLayout();
        m_menuWasVisible = menuBar()->isVisible();
        m_statusWasVisible = statusBar()->isVisible();
        m_toolbarsVisible.clear();
        TQPtrListIterator<TDEToolBar> it = toolBarIterator();
        for ( ; it.current(); ++it ) {
            m_toolbarsVisible.append( it.current()->isVisible() );
            it.current()->hide();
        }
        menuBar()->hide();
        statusBar()->hide();
        m_tabs->hide();
        m_details->hide();
        showFullScreen();
    } else {
        showNormal();
        TQPtrListIterator<TDEToolBar> it = toolBarIterator();
        TQValueList<bool>::ConstIterator vis = m_toolbarsVisible.begin();
        for ( ; it.current(); ++it ) {
            if ( vis == m_toolbarsVisible.end() || *vis )
                it.current()->show();
            if ( vis != m_toolbarsVisible.end() )
                ++vis;
        }
        if ( m_menuWasVisible )   menuBar()->show();
        if ( m_statusWasVisible ) statusBar()->show();
        m_tabs->show();
        m_details->show();
    }
}

void MainWindow::slotPreferences()
{
    SettingsDialog dlg( this );
    connect( &dlg, TQ_SIGNAL( settingsChanged() ), this, TQ_SLOT( slotSettingsChanged() ) );
    dlg.exec();
}

void MainWindow::slotSettingsChanged()
{
    m_controls->setVolume( Settings::self().volume );
    if ( !Settings::self().keepHistory )
        slotHistoryChanged();
}

/* ------------------------------------------------------------- list slots */

void MainWindow::slotCurrentChanged( const Video &video )
{
    if ( video.isValid() )
        m_details->setVideo( video );
    updateActions();
}

void MainWindow::slotContextMenu( const Video &, const TQPoint &pos )
{
    updateActions();
    TDEPopupMenu menu( this );
    menu.insertTitle( SmallIcon( "video" ), i18n( "Video" ) );
    m_playAction->plug( &menu );
    m_playExternalAction->plug( &menu );
    menu.insertSeparator();
    m_favAction->plug( &menu );
    if ( currentList() != m_searchList )
        m_removeAction->plug( &menu );
    menu.insertSeparator();
    m_channelAction->plug( &menu );
    m_copyAction->plug( &menu );
    m_browserAction->plug( &menu );
    menu.exec( pos );
}

void MainWindow::slotTabChanged( TQWidget * )
{
    Video v = currentList()->currentVideo();
    if ( v.isValid() )
        m_details->setVideo( v );
    updateActions();
}

void MainWindow::slotFavouritesChanged()
{
    m_favList->setVideos( Library::self()->favourites() );
    updateActions();
}

void MainWindow::slotHistoryChanged()
{
    m_historyList->setVideos( Library::self()->history() );
    updateActions();
}

/* ----------------------------------------------------------- player slots */

void MainWindow::slotPlayerStarted()
{
    setStatus( i18n( "Playing \"%1\"" ).arg( m_player->currentVideo().title ) );
    updateActions();
}

void MainWindow::slotPlayerStopped()
{
    m_controls->setActive( false );
    if ( m_fullScreen )
        setFullScreen( false );
    setCaption( TQString::null );
    updateActions();
}

void MainWindow::slotPlayerError( const TQString &message )
{
    setStatus( i18n( "Playback failed: %1" ).arg( message ) );
}

void MainWindow::slotPausedChanged( bool paused )
{
    if ( !m_player->isActive() )
        return;
    m_pauseAction->setText( paused ? i18n( "&Resume" ) : i18n( "P&ause" ) );
    m_pauseAction->setIcon( paused ? "media-playback-start" : "media-playback-pause" );
    setStatus( paused ? i18n( "Paused" ) : i18n( "Playing \"%1\"" ).arg( m_player->currentVideo().title ) );
}

#include "mainwindow.moc"
