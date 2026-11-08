/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_SEARCHBACKEND_H
#define TDEYOUTUBE_SEARCHBACKEND_H

#include <tqobject.h>
#include <tqcstring.h>
#include <tqstringlist.h>

#include "video.h"

class TDEProcess;

/**
 * Runs one yt-dlp command line and reports its standard output line by
 * line. Deletes nothing by itself; owners call cancel() / delete it.
 */
class YtDlpJob : public TQObject
{
    TQ_OBJECT

public:
    YtDlpJob( TQObject *parent = 0, const char *name = 0 );
    ~YtDlpJob();

    /** Starts yt-dlp with @p args. Returns false (and emits nothing) if it can't be run. */
    bool start( const TQStringList &args );
    void cancel();
    bool isRunning() const;

    /** Collected standard output (only when line mode is off). */
    TQString output() const;
    /** Emit lineReceived() for each line as it arrives (default: true). */
    void setLineMode( bool on ) { m_lineMode = on; }

    /** Human readable description of why yt-dlp could not be started. */
    static TQString notFoundMessage();
    /** Makes a yt-dlp error line readable and adds hints for common problems. */
    static TQString friendlyError( const TQString &error );

signals:
    void lineReceived( const TQString &line );
    /** @p ok is false if yt-dlp exited with an error; @p error holds its message. */
    void finished( bool ok, const TQString &error );

private slots:
    void slotStdout( TDEProcess *, char *buffer, int len );
    void slotStderr( TDEProcess *, char *buffer, int len );
    void slotExited( TDEProcess * );

private:
    void flushLines( bool all );

    TDEProcess *m_proc;
    TQCString   m_out;
    TQCString   m_err;
    bool        m_lineMode;
};


/** Searches YouTube (or lists a channel / playlist URL) using yt-dlp. */
class SearchBackend : public TQObject
{
    TQ_OBJECT

public:
    SearchBackend( TQObject *parent = 0, const char *name = 0 );

    /**
     * Starts a search for @p query. If @p query is a YouTube URL the videos of
     * that video, playlist or channel are listed instead. Cancels any running search.
     */
    void search( const TQString &query, int count );
    void cancel();
    bool isRunning() const;

    static bool looksLikeUrl( const TQString &text );
    /** Normalises channel URLs so that the "Videos" tab is listed. */
    static TQString normaliseUrl( const TQString &url );
    /** Parses one line produced with searchTemplate(). */
    static Video parseLine( const TQString &line );

signals:
    void videoFound( const Video &video );
    void finished( int count );
    void error( const TQString &message );

private slots:
    void slotLine( const TQString &line );
    void slotFinished( bool ok, const TQString &error );

private:
    YtDlpJob *m_job;
    int       m_count;
};


/** Fetches the full description and statistics of a single video. */
class DetailsFetcher : public TQObject
{
    TQ_OBJECT

public:
    DetailsFetcher( TQObject *parent = 0, const char *name = 0 );

    void fetch( const TQString &videoId );
    void cancel();

signals:
    void detailsReady( const TQString &videoId, long long likes, long long views,
                       const TQString &uploadDate, long long subscribers,
                       const TQString &description );
    void error( const TQString &videoId, const TQString &message );

private slots:
    void slotFinished( bool ok, const TQString &error );

private:
    YtDlpJob *m_job;
    TQString  m_id;
};

#endif
