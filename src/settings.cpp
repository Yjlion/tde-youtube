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
    c->sync();
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
