/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_HELPERUPDATER_H
#define TDEYOUTUBE_HELPERUPDATER_H

#include <tqobject.h>
#include <tqcstring.h>
#include <tqstring.h>
#include <tqstringlist.h>

class TDEProcess;
namespace TDEIO { class Job; }

/**
 * Downloads the helper programs from their official GitHub releases into
 * ~/.trinity/share/apps/tde-youtube/bin/ and keeps them up to date:
 *
 * - yt-dlp: the platform independent zipimport "yt-dlp" file. It runs with
 *   the system's Python 3 and, unlike distribution packages, includes the
 *   yt-dlp-ejs challenge solver. Verified against the release's SHA2-256SUMS.
 * - QuickJS-ng: the static "qjs-linux-<arch>" binary, which yt-dlp uses to
 *   solve YouTube's JavaScript challenges. Verified against the SHA-256
 *   digest GitHub lists for the release asset.
 *
 * A file is only downloaded if the local copy's checksum differs from the
 * latest release's. New files are verified, then swapped in atomically, so a
 * running yt-dlp is never disturbed. A failure with one program does not
 * stop the other from being updated.
 */
class HelperUpdater : public TQObject
{
    TQ_OBJECT

public:
    enum Result { UpToDate, Updated, Failed };

    static HelperUpdater *self();

    bool isRunning() const { return m_step != Idle; }

    /**
     * True if managed helpers are enabled and one of them is missing or the
     * last check is older than a day.
     */
    bool checkDue() const;

    /** Release asset name of QuickJS-ng for this machine, or null if there is none. */
    static TQString qjsAssetName();

public slots:
    /** Checks for new versions and installs them. Does nothing if already running. */
    void update();

signals:
    void started();
    /**
     * @p message describes the installed versions for UpToDate / Updated,
     * and the problems for Failed.
     */
    void finished( int result, const TQString &message );

private slots:
    void slotYtdlpSums( TDEIO::Job *job );
    void slotYtdlpFile( TDEIO::Job *job );
    void slotQjsRelease( TDEIO::Job *job );
    void slotQjsFile( TDEIO::Job *job );
    void slotVersionOutput( TDEProcess *, char *buffer, int len );
    void slotVersionExited( TDEProcess * );

private:
    HelperUpdater();

    void startQjs();
    void checkYtdlpVersion();
    void finish();
    /** Verifies @p data against @p sum and installs it as @p path; returns an error or null. */
    static TQString installFile( const TQByteArray &data, const TQString &path, const TQString &sum );
    void get( const TQString &url, const char *slot, bool githubApi = false );

    enum Step { Idle, YtdlpSums, YtdlpFile, QjsRelease, QjsFile, YtdlpVersion };
    Step        m_step;
    TQString    m_ytdlpSum;
    TQString    m_qjsSum;
    TQString    m_qjsTag;
    TQStringList m_errors;
    bool        m_changed;
    TDEProcess *m_proc;
    TQCString   m_versionOut;
};

#endif
