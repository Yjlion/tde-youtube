/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_SETTINGS_H
#define TDEYOUTUBE_SETTINGS_H

#include <tqdatetime.h>
#include <tqstring.h>
#include <tqstringlist.h>

/** Application settings, stored in tde-youtuberc. */
class Settings
{
public:
    enum Quality { QualityBest = 0, Quality1080, Quality720, Quality480, Quality360, QualityAudio };

    static Settings &self();

    void load();
    void save();

    /** yt-dlp format selector for the configured quality. */
    TQString formatSelector() const;
    static TQStringList qualityNames();

    TQString ytdlpPath;
    TQString mpvPath;
    int      resultsPerPage;
    int      quality;
    bool     embedPlayer;
    int      volume;
    bool     keepHistory;
    int      maxHistory;
    TQString extraMpvArgs;
    TQString cookiesBrowser;   ///< e.g. "firefox"; empty = none
    TQString cookiesFile;      ///< Netscape cookies.txt; empty = none

    /** Use our own copy of yt-dlp, downloaded from GitHub and kept up to date. */
    bool      manageYtdlp;
    /** The user was asked whether to use managed yt-dlp. */
    bool      manageYtdlpAsked;
    TQString  managedYtdlpVersion;   ///< version of our copy, e.g. "2026.10.26"
    TQDateTime managedYtdlpChecked;  ///< last successful update check

    /** Where our own copy of yt-dlp lives (~/.trinity/share/apps/tde-youtube/bin/yt-dlp). */
    static TQString managedYtdlpPath();
    /** The yt-dlp to run: our own copy if enabled and installed, else ytdlpPath. */
    TQString ytdlpProgram() const;
    /** Call after the yt-dlp program changed so its version is looked up again. */
    static void forgetYtdlpVersion();

    /** Cookie options for yt-dlp itself. */
    TQStringList ytdlpCookieArgs() const;
    /** The same options in the form mpv passes them on to yt-dlp. */
    TQStringList mpvCookieArgs() const;
    static TQStringList cookieBrowsers();

    /**
     * yt-dlp needs a JavaScript runtime for YouTube and only looks for deno
     * by default. If deno is missing but QuickJS (qjs) is installed, these
     * options make yt-dlp use QuickJS.
     */
    static TQStringList ytdlpJsRuntimeArgs();
    /** The same options in the form mpv passes them on to yt-dlp. */
    static TQStringList mpvJsRuntimeArgs();

private:
    Settings();
};

#endif
