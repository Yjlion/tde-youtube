/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_VIDEOLIST_H
#define TDEYOUTUBE_VIDEOLIST_H

#include <tdelistview.h>
#include <tqmap.h>

#include "video.h"

/** One row of a VideoListView: thumbnail, title, channel and statistics. */
class VideoItem : public TDEListViewItem
{
public:
    VideoItem( TQListView *parent, TQListViewItem *after, const Video &video );

    const Video &video() const { return m_video; }

    virtual void paintCell( TQPainter *p, const TQColorGroup &cg, int column, int width, int align );
    virtual void setup();
    virtual int width( const TQFontMetrics &fm, const TQListView *lv, int c ) const;
    virtual TQString key( int column, bool ascending ) const;

private:
    Video m_video;
};


/** List of videos with thumbnails, used for search results, favourites and history. */
class VideoListView : public TDEListView
{
    TQ_OBJECT

public:
    VideoListView( TQWidget *parent = 0, const char *name = 0 );

    /** Appends @p video unless a video with the same id is already listed. */
    VideoItem *addVideo( const Video &video );
    void setVideos( const VideoList &videos );
    void removeVideo( const TQString &id );
    void clearVideos();
    bool contains( const TQString &id ) const;
    int  videoCount() const { return childCount(); }

    /** The current (focused) video, or an invalid Video. */
    Video currentVideo() const;
    /** All listed videos in order. */
    VideoList videos() const;

    /** Text shown when the list is empty. */
    void setEmptyText( const TQString &text );

signals:
    void videoActivated( const Video &video );
    void currentVideoChanged( const Video &video );
    void videoContextMenu( const Video &video, const TQPoint &globalPos );

protected:
    virtual void drawContentsOffset( TQPainter *p, int ox, int oy, int cx, int cy, int cw, int ch );

private slots:
    void slotThumbnail( const TQString &id, const TQPixmap &pm );
    void slotExecuted( TQListViewItem *item );
    void slotCurrentChanged( TQListViewItem *item );
    void slotContextMenu( TDEListView *, TQListViewItem *item, const TQPoint &pos );

private:
    TQMap<TQString, VideoItem *> m_items;
    TQString m_emptyText;
};

#endif
