/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_DETAILSPANE_H
#define TDEYOUTUBE_DETAILSPANE_H

#include <tqwidget.h>
#include <tqmap.h>

#include "video.h"

class TQLabel;
class KTextBrowser;
class DetailsFetcher;

/** Shows title, channel, statistics and description of the selected video. */
class DetailsPane : public TQWidget
{
    TQ_OBJECT

public:
    DetailsPane( TQWidget *parent = 0, const char *name = 0 );

    const Video &video() const { return m_video; }

public slots:
    void setVideo( const Video &video );

private slots:
    void slotDetails( const TQString &id, long long likes, long long views,
                      const TQString &uploadDate, long long subscribers, const TQString &description );
    void slotError( const TQString &id, const TQString &message );

private:
    struct Details {
        long long likes, views, subscribers;
        TQString uploadDate, description;
    };
    void showDetails( const Details &d );
    void updateHeader( const Details *d );
    static TQString linkify( const TQString &text );

    Video           m_video;
    TQLabel        *m_title;
    TQLabel        *m_meta;
    KTextBrowser   *m_description;
    DetailsFetcher *m_fetcher;
    TQMap<TQString, Details> m_cache;
};

#endif
