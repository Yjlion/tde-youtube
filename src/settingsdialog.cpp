/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "settingsdialog.h"
#include "settings.h"
#include "library.h"
#include "thumbnailloader.h"
#include "helperupdater.h"

#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqfile.h>
#include <tqhbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>
#include <tqwhatsthis.h>

#include <kcombobox.h>
#include <kiconloader.h>
#include <tdeglobal.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <knuminput.h>
#include <kurlrequester.h>
#include <kstdguiitem.h>

SettingsDialog::SettingsDialog( TQWidget *parent, const char *name )
    : KDialogBase( IconList, i18n( "Configure" ),
                   Ok | Apply | Cancel | Default | Help, Ok, parent, name, true, true )
{
    setHelp( "configuration", "tde-youtube" );

    /* ---- Playback page ---- */
    TQFrame *play = addPage( i18n( "Playback" ), i18n( "Video Playback" ), BarIcon( "tde-youtube", TDEIcon::SizeMedium ) );
    TQVBoxLayout *pl = new TQVBoxLayout( play, 0, spacingHint() );

    TQGroupBox *qg = new TQGroupBox( 2, TQt::Horizontal, i18n( "Player" ), play );
    TQLabel *ql = new TQLabel( i18n( "Preferred &quality:" ), qg );
    m_quality = new KComboBox( qg );
    ql->setBuddy( m_quality );
    m_quality->insertStringList( Settings::qualityNames() );
    TQWhatsThis::add( m_quality, i18n( "The highest resolution mpv should request from YouTube. "
                                       "Lower resolutions use less bandwidth and CPU." ) );

    m_embed = new TQCheckBox( i18n( "&Embed the video in the main window" ), qg );
    new TQWidget( qg );
    TQWhatsThis::add( m_embed, i18n( "When enabled the video plays inside the tde-youtube window. "
                                     "Otherwise mpv opens its own window." ) );

    TQLabel *xl = new TQLabel( i18n( "E&xtra mpv options:" ), qg );
    m_extraArgs = new KLineEdit( qg );
    m_extraArgs->setClickMessage( "--hwdec=auto" );
    xl->setBuddy( m_extraArgs );
    TQWhatsThis::add( m_extraArgs, i18n( "Additional command line options passed to mpv, separated by spaces." ) );
    pl->addWidget( qg );

    TQGroupBox *sg = new TQGroupBox( 2, TQt::Horizontal, i18n( "Search" ), play );
    TQLabel *rl = new TQLabel( i18n( "&Results per search:" ), sg );
    m_results = new KIntNumInput( sg );
    rl->setBuddy( m_results );
    m_results->setRange( 5, 100, 5, true );
    pl->addWidget( sg );
    pl->addStretch( 1 );

    /* ---- Programs page ---- */
    TQFrame *progs = addPage( i18n( "Programs" ), i18n( "Helper Programs" ), BarIcon( "exec", TDEIcon::SizeMedium ) );
    TQGridLayout *gl = new TQGridLayout( progs, 7, 2, 0, spacingHint() );
    TQLabel *intro = new TQLabel( i18n( "tde-youtube uses <b>yt-dlp</b> to search YouTube and <b>mpv</b> to play "
                                        "videos. Enter a command name found in $PATH or a full path." ), progs );
    intro->setAlignment( TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak );
    gl->addMultiCellWidget( intro, 0, 0, 0, 1 );
    TQLabel *l1 = new TQLabel( i18n( "&yt-dlp:" ), progs );
    m_ytdlp = new KURLRequester( progs );
    l1->setBuddy( m_ytdlp );
    gl->addWidget( l1, 1, 0 );
    gl->addWidget( m_ytdlp, 1, 1 );
    TQLabel *l2 = new TQLabel( i18n( "&mpv:" ), progs );
    m_mpv = new KURLRequester( progs );
    l2->setBuddy( m_mpv );
    gl->addWidget( l2, 2, 0 );
    gl->addWidget( m_mpv, 2, 1 );

    TQGroupBox *ug = new TQGroupBox( 1, TQt::Horizontal, i18n( "Downloaded Helper Programs" ), progs );
    m_manageYtdlp = new TQCheckBox( i18n( "&Download yt-dlp and QuickJS from GitHub and keep them up to date" ), ug );
    TQWhatsThis::add( m_manageYtdlp, i18n( "YouTube changes often, and the yt-dlp of your distribution is often "
                                           "too old to play videos. When enabled (the default), TDE YouTube "
                                           "downloads the official portable yt-dlp from github.com/yt-dlp (about "
                                           "3 MB, needs Python 3.10 or newer) and the QuickJS-ng JavaScript engine "
                                           "from github.com/quickjs-ng (about 2.5 MB), which yt-dlp needs to solve "
                                           "YouTube's challenges. Both are kept in your home folder, verified "
                                           "against their published checksums and checked for new versions once "
                                           "a day and whenever YouTube refuses a video. The yt-dlp entered above "
                                           "is only used until the download has finished, or when this is off." ) );
    m_ytdlpStatus = new TQLabel( ug );
    m_ytdlpStatus->setAlignment( TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak );
    TQHBox *ub = new TQHBox( ug );
    m_updateYtdlp = new TQPushButton( SmallIconSet( "reload" ), i18n( "Check for &Updates Now" ), ub );
    new TQWidget( ub );   // keep the button at its natural size
    gl->addMultiCellWidget( ug, 3, 3, 0, 1 );

    connect( m_updateYtdlp, TQ_SIGNAL( clicked() ), this, TQ_SLOT( slotUpdateYtdlp() ) );
    connect( m_manageYtdlp, TQ_SIGNAL( toggled( bool ) ), m_updateYtdlp, TQ_SLOT( setEnabled( bool ) ) );
    connect( HelperUpdater::self(), TQ_SIGNAL( started() ), this, TQ_SLOT( slotYtdlpUpdateStarted() ) );
    connect( HelperUpdater::self(), TQ_SIGNAL( finished( int, const TQString & ) ),
             this, TQ_SLOT( slotYtdlpUpdateFinished( int, const TQString & ) ) );

    TQGroupBox *cg = new TQGroupBox( 2, TQt::Horizontal, i18n( "Cookies" ), progs );
    TQLabel *cintro = new TQLabel( i18n( "YouTube sometimes refuses to work unless you are signed in. "
                                         "yt-dlp can then use the cookies of a web browser in which "
                                         "you are signed in to YouTube, or a cookies.txt file." ), cg );
    cintro->setAlignment( TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak );
    new TQWidget( cg );
    TQLabel *cbl = new TQLabel( i18n( "Use cookies from &browser:" ), cg );
    m_cookiesBrowser = new KComboBox( true, cg );
    m_cookiesBrowser->insertItem( i18n( "None" ) );
    m_cookiesBrowser->insertStringList( Settings::cookieBrowsers() );
    cbl->setBuddy( m_cookiesBrowser );
    TQWhatsThis::add( m_cookiesBrowser, i18n( "Passed to yt-dlp as --cookies-from-browser. You may also type "
                                              "BROWSER:PROFILE, e.g. firefox:default-release." ) );
    TQLabel *cfl = new TQLabel( i18n( "Coo&kies file:" ), cg );
    m_cookiesFile = new KURLRequester( cg );
    m_cookiesFile->setFilter( "*.txt|" + i18n( "Cookie Files" ) + "\n*|" + i18n( "All Files" ) );
    cfl->setBuddy( m_cookiesFile );
    gl->addMultiCellWidget( cg, 4, 4, 0, 1 );
    gl->setRowStretch( 5, 1 );

    /* ---- Privacy page ---- */
    TQFrame *priv = addPage( i18n( "History" ), i18n( "History and Cache" ), BarIcon( "history", TDEIcon::SizeMedium ) );
    TQGridLayout *hl = new TQGridLayout( priv, 5, 2, 0, spacingHint() );
    m_keepHistory = new TQCheckBox( i18n( "&Remember watched videos" ), priv );
    hl->addMultiCellWidget( m_keepHistory, 0, 0, 0, 1 );
    TQLabel *l3 = new TQLabel( i18n( "&Maximum history entries:" ), priv );
    m_maxHistory = new KIntNumInput( priv );
    m_maxHistory->setRange( 10, 1000, 10, false );
    l3->setBuddy( m_maxHistory );
    hl->addWidget( l3, 1, 0 );
    hl->addWidget( m_maxHistory, 1, 1 );
    TQPushButton *clearHist = new TQPushButton( SmallIconSet( "history_clear" ), i18n( "Clear &History" ), priv );
    TQPushButton *clearCache = new TQPushButton( SmallIconSet( "edittrash" ), i18n( "Clear &Thumbnail Cache" ), priv );
    hl->addWidget( clearHist, 2, 0 );
    hl->addWidget( clearCache, 3, 0 );
    hl->setRowStretch( 4, 1 );
    hl->setColStretch( 1, 1 );

    connect( clearHist,  TQ_SIGNAL( clicked() ), this, TQ_SLOT( slotClearHistory() ) );
    connect( clearCache, TQ_SIGNAL( clicked() ), this, TQ_SLOT( slotClearCache() ) );
    connect( m_keepHistory, TQ_SIGNAL( toggled( bool ) ), m_maxHistory, TQ_SLOT( setEnabled( bool ) ) );

    load();

    connect( m_quality,     TQ_SIGNAL( activated( int ) ),  this, TQ_SLOT( slotChanged() ) );
    connect( m_embed,       TQ_SIGNAL( toggled( bool ) ),   this, TQ_SLOT( slotChanged() ) );
    connect( m_extraArgs,   TQ_SIGNAL( textChanged( const TQString & ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_results,     TQ_SIGNAL( valueChanged( int ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_ytdlp,       TQ_SIGNAL( textChanged( const TQString & ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_mpv,         TQ_SIGNAL( textChanged( const TQString & ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_cookiesBrowser, TQ_SIGNAL( textChanged( const TQString & ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_cookiesBrowser, TQ_SIGNAL( activated( int ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_cookiesFile, TQ_SIGNAL( textChanged( const TQString & ) ), this, TQ_SLOT( slotChanged() ) );
    connect( m_manageYtdlp, TQ_SIGNAL( toggled( bool ) ),   this, TQ_SLOT( slotChanged() ) );
    connect( m_keepHistory, TQ_SIGNAL( toggled( bool ) ),   this, TQ_SLOT( slotChanged() ) );
    connect( m_maxHistory,  TQ_SIGNAL( valueChanged( int ) ), this, TQ_SLOT( slotChanged() ) );
    enableButtonApply( false );
    resize( minimumSizeHint().expandedTo( TQSize( 660, 520 ) ) );
}

void SettingsDialog::load()
{
    const Settings &s = Settings::self();
    m_quality->setCurrentItem( s.quality );
    m_embed->setChecked( s.embedPlayer );
    m_extraArgs->setText( s.extraMpvArgs );
    m_results->setValue( s.resultsPerPage );
    m_ytdlp->setURL( s.ytdlpPath );
    m_mpv->setURL( s.mpvPath );
    if ( s.cookiesBrowser.isEmpty() )
        m_cookiesBrowser->setCurrentItem( 0 );
    else
        m_cookiesBrowser->setCurrentText( s.cookiesBrowser );
    m_cookiesFile->setURL( s.cookiesFile );
    m_manageYtdlp->setChecked( s.manageYtdlp );
    m_updateYtdlp->setEnabled( s.manageYtdlp );
    showYtdlpStatus();
    if ( HelperUpdater::self()->isRunning() )
        slotYtdlpUpdateStarted();
    m_keepHistory->setChecked( s.keepHistory );
    m_maxHistory->setValue( s.maxHistory );
    m_maxHistory->setEnabled( s.keepHistory );
}

void SettingsDialog::apply()
{
    Settings &s = Settings::self();
    s.quality        = m_quality->currentItem();
    s.embedPlayer    = m_embed->isChecked();
    s.extraMpvArgs   = m_extraArgs->text().stripWhiteSpace();
    s.resultsPerPage = m_results->value();
    s.ytdlpPath      = m_ytdlp->url().stripWhiteSpace().isEmpty() ? TQString( "yt-dlp" ) : m_ytdlp->url().stripWhiteSpace();
    s.mpvPath        = m_mpv->url().stripWhiteSpace().isEmpty() ? TQString( "mpv" ) : m_mpv->url().stripWhiteSpace();
    s.cookiesBrowser = m_cookiesBrowser->currentItem() == 0 && m_cookiesBrowser->currentText() == i18n( "None" )
                       ? TQString::null : m_cookiesBrowser->currentText().stripWhiteSpace();
    s.cookiesFile    = m_cookiesFile->url().stripWhiteSpace();
    s.manageYtdlp    = m_manageYtdlp->isChecked();
    Settings::forgetHelperInfo();
    s.keepHistory    = m_keepHistory->isChecked();
    s.maxHistory     = m_maxHistory->value();
    s.save();
    enableButtonApply( false );
    emit settingsChanged();

    // Just switched on: fetch the helpers right away.
    if ( HelperUpdater::self()->checkDue() )
        HelperUpdater::self()->update();
}

void SettingsDialog::showYtdlpStatus( const TQString &error )
{
    const Settings &s = Settings::self();
    const TQString none = i18n( "not downloaded yet" );
    TQString ytdlp = TQFile::exists( Settings::managedYtdlpPath() ) && !s.managedYtdlpVersion.isEmpty()
                     ? s.managedYtdlpVersion : none;
    TQString qjs = TQFile::exists( Settings::managedQjsPath() ) && !s.managedQjsVersion.isEmpty()
                   ? s.managedQjsVersion
                   : ( HelperUpdater::qjsAssetName().isEmpty() ? i18n( "not available for this computer" ) : none );
    TQString text = i18n( "yt-dlp: %1, QuickJS-ng: %2." ).arg( ytdlp ).arg( qjs );
    if ( s.managedYtdlpChecked.isValid() )
        text += " " + i18n( "Last checked %1." )
                      .arg( TDEGlobal::locale()->formatDateTime( s.managedYtdlpChecked, true ) );
    if ( !error.isEmpty() )
        text += "\n" + i18n( "Last update failed: %1" ).arg( error );
    m_ytdlpStatus->setText( text );
}

void SettingsDialog::slotUpdateYtdlp()
{
    if ( !Settings::self().manageYtdlp )
        apply();   // checked but not applied yet
    HelperUpdater::self()->update();
}

void SettingsDialog::slotYtdlpUpdateStarted()
{
    m_updateYtdlp->setEnabled( false );
    m_ytdlpStatus->setText( i18n( "Checking for new versions of yt-dlp and QuickJS-ng..." ) );
}

void SettingsDialog::slotYtdlpUpdateFinished( int result, const TQString &message )
{
    m_updateYtdlp->setEnabled( m_manageYtdlp->isChecked() );
    showYtdlpStatus( result == HelperUpdater::Failed ? message : TQString::null );
}

void SettingsDialog::slotOk()
{
    apply();
    accept();
}

void SettingsDialog::slotApply()
{
    apply();
}

void SettingsDialog::slotDefault()
{
    m_quality->setCurrentItem( Settings::Quality1080 );
    m_embed->setChecked( true );
    m_extraArgs->clear();
    m_results->setValue( 20 );
    m_ytdlp->setURL( "yt-dlp" );
    m_mpv->setURL( "mpv" );
    m_cookiesBrowser->setCurrentItem( 0 );
    m_cookiesFile->clear();
    m_manageYtdlp->setChecked( true );
    m_keepHistory->setChecked( true );
    m_maxHistory->setValue( 200 );
    enableButtonApply( true );
}

void SettingsDialog::slotChanged()
{
    enableButtonApply( true );
}

void SettingsDialog::slotClearHistory()
{
    if ( KMessageBox::warningContinueCancel( this, i18n( "Do you really want to clear the watch history?" ),
                                             i18n( "Clear History" ), KStdGuiItem::clear() ) == KMessageBox::Continue )
        Library::self()->clearHistory();
}

void SettingsDialog::slotClearCache()
{
    ThumbnailLoader::self()->clearCache();
    KMessageBox::information( this, i18n( "The thumbnail cache has been cleared." ) );
}

#include "settingsdialog.moc"
