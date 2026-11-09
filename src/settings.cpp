/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "settings.h"

#include <tdeapplication.h>
#include <tdeconfig.h>
#include <tdeglobal.h>
#include <tdelocale.h>
#include <kstandarddirs.h>
#include <kprocess.h>

#include <tqfile.h>

#include <stdio.h>
#include <unistd.h>

Settings &Settings::self()
{
    static Settings s;
    return s;
}

Settings::Settings()
{
    load();
}

void Settings::load()
{
    TDEConfig *c = TDEGlobal::config();
    c->setGroup( "General" );
    ytdlpPath      = c->readPathEntry( "YtDlpPath", "yt-dlp" );
    mpvPath        = c->readPathEntry( "MpvPath", "mpv" );
    resultsPerPage = c->readNumEntry( "ResultsPerPage", 20 );
    quality        = c->readNumEntry( "Quality", Quality1080 );
    embedPlayer    = c->readBoolEntry( "EmbedPlayer", true );
    volume         = c->readNumEntry( "Volume", 80 );
    keepHistory    = c->readBoolEntry( "KeepHistory", true );
    maxHistory     = c->readNumEntry( "MaxHistory", 200 );
    extraMpvArgs   = c->readEntry( "ExtraMpvArgs" );
    cookiesBrowser = c->readEntry( "CookiesFromBrowser" );
    cookiesFile    = c->readPathEntry( "CookiesFile" );

    c->setGroup( "yt-dlp" );
    manageYtdlp         = c->readBoolEntry( "Manage", true );
    managedYtdlpVersion = c->readEntry( "Version" );
    managedQjsVersion   = c->readEntry( "QjsVersion" );
    TQDateTime never;
    managedYtdlpChecked = c->readDateTimeEntry( "LastCheck", &never );

    if ( resultsPerPage < 5 )   resultsPerPage = 5;
    if ( resultsPerPage > 100 ) resultsPerPage = 100;
    if ( quality < QualityBest || quality > QualityAudio ) quality = Quality1080;
    if ( volume < 0 )   volume = 0;
    if ( volume > 100 ) volume = 100;
}

void Settings::save()
{
    TDEConfig *c = TDEGlobal::config();
    c->setGroup( "General" );
    c->writePathEntry( "YtDlpPath", ytdlpPath );
    c->writePathEntry( "MpvPath", mpvPath );
    c->writeEntry( "ResultsPerPage", resultsPerPage );
    c->writeEntry( "Quality", quality );
    c->writeEntry( "EmbedPlayer", embedPlayer );
    c->writeEntry( "Volume", volume );
    c->writeEntry( "KeepHistory", keepHistory );
    c->writeEntry( "MaxHistory", maxHistory );
    c->writeEntry( "ExtraMpvArgs", extraMpvArgs );
    c->writeEntry( "CookiesFromBrowser", cookiesBrowser );
    c->writePathEntry( "CookiesFile", cookiesFile );

    c->setGroup( "yt-dlp" );
    c->writeEntry( "Manage", manageYtdlp );
    c->writeEntry( "Version", managedYtdlpVersion );
    c->writeEntry( "QjsVersion", managedQjsVersion );
    if ( managedYtdlpChecked.isValid() )
        c->writeEntry( "LastCheck", managedYtdlpChecked );
    c->sync();
}

TQString Settings::managedYtdlpPath()
{
    return TDEGlobal::dirs()->saveLocation( "data", "tde-youtube/bin/", true ) + "yt-dlp";
}

TQString Settings::managedQjsPath()
{
    return TDEGlobal::dirs()->saveLocation( "data", "tde-youtube/bin/", true ) + "qjs";
}

TQString Settings::ytdlpProgram() const
{
    if ( manageYtdlp ) {
        TQString p = managedYtdlpPath();
        if ( ::access( TQFile::encodeName( p ), X_OK ) == 0 )
            return p;
    }
    return ytdlpPath;
}

TQStringList Settings::qualityNames()
{
    TQStringList l;
    l << i18n( "Best available" )
      << i18n( "Up to 1080p" )
      << i18n( "Up to 720p" )
      << i18n( "Up to 480p" )
      << i18n( "Up to 360p" )
      << i18n( "Audio only" );
    return l;
}

TQStringList Settings::cookieBrowsers()
{
    TQStringList l;
    l << "firefox" << "chromium" << "chrome" << "brave" << "opera" << "vivaldi" << "edge" << "librewolf";
    return l;
}

TQStringList Settings::ytdlpCookieArgs() const
{
    TQStringList a;
    if ( !cookiesBrowser.isEmpty() )
        a << "--cookies-from-browser" << cookiesBrowser;
    if ( !cookiesFile.isEmpty() )
        a << "--cookies" << cookiesFile;
    return a;
}

TQStringList Settings::mpvCookieArgs() const
{
    TQStringList a;
    if ( !cookiesBrowser.isEmpty() )
        a << "--ytdl-raw-options-append=cookies-from-browser=" + cookiesBrowser;
    if ( !cookiesFile.isEmpty() )
        a << "--ytdl-raw-options-append=cookies=" + cookiesFile;
    return a;
}

static TQString s_versionPath, s_version;
static int s_hasDeno = -1;   // -1 unknown

void Settings::forgetHelperInfo()
{
    s_versionPath = s_version = TQString::null;
    s_hasDeno = -1;
}

/** Version of the yt-dlp in use, e.g. "2026.10.26" (cached per path). */
static TQString ytdlpVersion()
{
    const Settings &s = Settings::self();
    const TQString path = s.ytdlpProgram();
    if ( path == s_versionPath )
        return s_version;
    s_versionPath = path;
    TQString &version = s_version;
    version = TQString::null;
    if ( path == Settings::managedYtdlpPath() && !s.managedYtdlpVersion.isEmpty() ) {
        version = s.managedYtdlpVersion;   // known, no need to run it
        return version;
    }
    TQCString cmd = TQFile::encodeName( TDEProcess::quote( path ) ) + " --version 2>/dev/null";
    if ( FILE *f = ::popen( cmd.data(), "r" ) ) {
        char buf[64];
        if ( ::fgets( buf, sizeof( buf ), f ) )
            version = TQString::fromLatin1( buf ).stripWhiteSpace();
        ::pclose( f );
    }
    return version;
}

/** Value for yt-dlp's --js-runtimes, or null to leave yt-dlp's default (deno). */
static TQString jsRuntime()
{
    if ( s_hasDeno < 0 )
        s_hasDeno = TDEStandardDirs::findExe( "deno" ).isEmpty() ? 0 : 1;
    if ( s_hasDeno )
        return TQString::null;   // yt-dlp uses deno by itself

    TQString runtime;
    const TQString qjs = Settings::managedQjsPath();
    if ( Settings::self().manageYtdlp && ::access( TQFile::encodeName( qjs ), X_OK ) == 0 )
        runtime = "quickjs:" + qjs;              // our QuickJS-ng
    else if ( !TDEStandardDirs::findExe( "qjs" ).isEmpty() )
        runtime = "quickjs";                     // QuickJS from the distribution
    else
        return TQString::null;

    // --js-runtimes exists since yt-dlp 2025.11; older versions would refuse to run.
    // Versions are YYYY.MM.DD, so they compare correctly as strings.
    if ( ytdlpVersion() < "2025.11" )
        return TQString::null;
    return runtime;
}

TQStringList Settings::ytdlpJsRuntimeArgs()
{
    TQStringList a;
    if ( !jsRuntime().isEmpty() )
        a << "--js-runtimes" << jsRuntime();
    return a;
}

TQStringList Settings::mpvJsRuntimeArgs()
{
    TQStringList a;
    if ( !jsRuntime().isEmpty() )
        a << "--ytdl-raw-options-append=js-runtimes=" + jsRuntime();
    return a;
}

TQString Settings::formatSelector() const
{
    switch ( quality ) {
    case Quality1080:  return "bestvideo[height<=?1080]+bestaudio/best[height<=?1080]/best";
    case Quality720:   return "bestvideo[height<=?720]+bestaudio/best[height<=?720]/best";
    case Quality480:   return "bestvideo[height<=?480]+bestaudio/best[height<=?480]/best";
    case Quality360:   return "bestvideo[height<=?360]+bestaudio/best[height<=?360]/best";
    case QualityAudio: return "bestaudio/best";
    case QualityBest:
    default:           return "bestvideo+bestaudio/best";
    }
}
