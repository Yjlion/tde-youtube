/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_YTDLPUPDATER_H
#define TDEYOUTUBE_YTDLPUPDATER_H

#include <tqobject.h>
#include <tqcstring.h>
#include <tqstring.h>

class TDEProcess;
namespace TDEIO { class Job; }

/**
 * Downloads the portable yt-dlp (the zipimport "yt-dlp" release file, which
 * includes the yt-dlp-ejs challenge solver scripts) from the official GitHub
 * releases into Settings::managedYtdlpPath() and keeps it up to date.
 *
 * Every check first fetches SHA2-256SUMS of the latest release. If our copy
 * already has the listed checksum nothing is downloaded. Otherwise the new
 * file is downloaded, verified against that checksum and swapped in
 * atomically, so a running yt-dlp is never disturbed.
 */
class YtDlpUpdater : public TQObject
{
    TQ_OBJECT

public:
    enum Result { UpToDate, Updated, Failed };

    static YtDlpUpdater *self();

    bool isRunning() const { return m_step != Idle; }

    /** True if managed yt-dlp is enabled and the last check is older than a day. */
    bool checkDue() const;

public slots:
    /** Checks for a new yt-dlp and installs it. Does nothing if already running. */
    void update();

signals:
    void started();
    /** @p message is the installed version for UpToDate / Updated, an error for Failed. */
    void finished( int result, const TQString &message );

private slots:
    void slotSumsResult( TDEIO::Job *job );
    void slotFileResult( TDEIO::Job *job );
    void slotVersionOutput( TDEProcess *, char *buffer, int len );
    void slotVersionExited( TDEProcess * );

private:
    YtDlpUpdater();
    void fail( const TQString &message );
    void done( Result result );
    void checkVersion();

    enum Step { Idle, FetchingSums, FetchingFile, CheckingVersion };
    Step      m_step;
    TQString  m_expectedSum;
    TDEProcess *m_proc;
    TQCString   m_versionOut;
    Result    m_result;
};

#endif
