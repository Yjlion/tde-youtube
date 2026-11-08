/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_VIDEO_H
#define TDEYOUTUBE_VIDEO_H

#include <tqstring.h>
#include <tqvaluelist.h>

/** Plain description of one YouTube video as reported by yt-dlp. */
struct Video
{
    Video() : duration(0), viewCount(-1) {}

    TQString id;          ///< 11 character video id
    TQString title;
    TQString channel;
    TQString channelUrl;
    TQString uploadDate;  ///< YYYYMMDD, may be empty
    int      duration;    ///< seconds, 0 when unknown / live
    long long viewCount;  ///< -1 when unknown

    bool isValid() const { return !id.isEmpty(); }

    TQString watchUrl() const
    { return TQString::fromLatin1( "https://www.youtube.com/watch?v=" ) + id; }

    TQString thumbnailUrl() const
    { return TQString::fromLatin1( "https://i.ytimg.com/vi/%1/mqdefault.jpg" ).arg( id ); }

    /** "3:07", "1:02:45" or an empty string when unknown. */
    static TQString formatDuration( int seconds );
    /** "1.2M views", "934 views" ... (localised). */
    static TQString formatViews( long long views );
    /** "12 Mar 2024" from YYYYMMDD (localised). */
    static TQString formatDate( const TQString &yyyymmdd );
};

typedef TQValueList<Video> VideoList;

#endif
