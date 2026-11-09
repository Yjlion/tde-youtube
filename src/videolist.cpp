/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "videolist.h"
#include "thumbnailloader.h"

#include <tqheader.h>
#include <tqpainter.h>
#include <tqsimplerichtext.h>

#include <kiconloader.h>
#include <tdelocale.h>
#include <kstringhandler.h>
#include <tdeglobalsettings.h>

static const int MARGIN = 4;

static TQColor mix( const TQColor &a, const TQColor &b, int percentA )
{
    return TQColor( ( a.red()   * percentA + b.red()   * ( 100 - percentA ) ) / 100,
                    ( a.green() * percentA + b.green() * ( 100 - percentA ) ) / 100,
                    ( a.blue()  * percentA + b.blue()  * ( 100 - percentA ) ) / 100 );
}

/* -------------------------------------------------------------- VideoItem */

VideoItem::VideoItem( TQListView *parent, TQListViewItem *after, const Video &video )
    : TDEListViewItem( parent, after ), m_video( video )
{
    setText( 0, video.title );
}

void VideoItem::setup()
{
    TDEListViewItem::setup();
    setHeight( ThumbnailLoader::thumbSize().height() + 2 * MARGIN );
}

int VideoItem::width( const TQFontMetrics &, const TQListView *, int ) const
{
    // Let the column shrink with the view; text is elided when painting.
    return ThumbnailLoader::thumbSize().width() + 3 * MARGIN + 160;
}

TQString VideoItem::key( int, bool ) const
{
    return m_video.title;
}

void VideoItem::paintCell( TQPainter *p, const TQColorGroup &cg, int column, int width, int )
{
    if ( column != 0 )
        return;

    const bool sel = isSelected();
    const TQColor bg   = sel ? cg.highlight() : backgroundColor( column );
    const TQColor fg   = sel ? cg.highlightedText() : cg.text();
    const TQColor dim  = mix( fg, bg, 60 );
    const int h = height();

    p->fillRect( 0, 0, width, h, bg );

    // Thumbnail
    const TQSize ts = ThumbnailLoader::thumbSize();
    TQRect thumb( MARGIN, ( h - ts.height() ) / 2, ts.width(), ts.height() );
    TQPixmap pm = ThumbnailLoader::self()->thumbnail( m_video.id );
    if ( pm.isNull() ) {
        p->fillRect( thumb, TQColor( 40, 40, 40 ) );
        TQPixmap icon = DesktopIcon( "video", 32 );
        p->drawPixmap( thumb.center().x() - icon.width() / 2,
                       thumb.center().y() - icon.height() / 2, icon );
    } else {
        p->drawPixmap( thumb.topLeft(), pm );
    }

    // Duration badge
    TQFont small = listView()->font();
    small.setPointSize( TQMAX( 6, small.pointSize() - 1 ) );
    small.setBold( true );
    TQString dur = m_video.duration > 0 ? Video::formatDuration( m_video.duration ) : TQString::null;
    if ( !dur.isEmpty() ) {
        TQFontMetrics sfm( small );
        int bw = sfm.width( dur ) + 6, bh = sfm.height() + 1;
        TQRect badge( thumb.right() - bw - 3, thumb.bottom() - bh - 3, bw, bh );
        p->fillRect( badge, TQt::black );
        p->setFont( small );
        p->setPen( TQt::white );
        p->drawText( badge, TQt::AlignCenter, dur );
    }

    // Text
    int x = thumb.right() + 2 * MARGIN;
    int avail = width - x - MARGIN;
    if ( avail < 20 )
        return;

    TQFont bold = listView()->font();
    bold.setBold( true );
    TQFontMetrics bfm( bold );
    TQFontMetrics fm( listView()->font() );

    // Title: up to two lines, word wrapped, the second one elided.
    TQString title = m_video.title.isEmpty() ? i18n( "(untitled)" ) : m_video.title;
    TQString line1 = title, line2;
    if ( bfm.width( title ) > avail ) {
        int cut = 0;
        for ( int i = 1; i <= (int) title.length(); ++i ) {
            if ( i == (int) title.length() || title[i].isSpace() ) {
                if ( bfm.width( title.left( i ) ) <= avail )
                    cut = i;
                else
                    break;
            }
        }
        if ( cut == 0 ) {
            line1 = KStringHandler::rPixelSqueeze( title, bfm, avail );
        } else {
            line1 = title.left( cut );
            line2 = KStringHandler::rPixelSqueeze( title.mid( cut ).stripWhiteSpace(), bfm, avail );
        }
    }

    int y = thumb.top();
    p->setPen( fg );
    p->setFont( bold );
    p->drawText( x, y, avail, bfm.lineSpacing(), TQt::AlignLeft | TQt::AlignTop | TQt::SingleLine, line1 );
    y += bfm.lineSpacing();
    if ( !line2.isEmpty() ) {
        p->drawText( x, y, avail, bfm.lineSpacing(), TQt::AlignLeft | TQt::AlignTop | TQt::SingleLine, line2 );
        y += bfm.lineSpacing();
    }

    p->setFont( listView()->font() );
    p->setPen( dim );
    y += 2;
    if ( !m_video.channel.isEmpty() ) {
        p->drawText( x, y, avail, fm.lineSpacing(), TQt::AlignLeft | TQt::AlignTop | TQt::SingleLine,
                     KStringHandler::rPixelSqueeze( m_video.channel, fm, avail ) );
        y += fm.lineSpacing();
    }

    TQStringList stats;
    TQString views = Video::formatViews( m_video.viewCount );
    if ( !views.isEmpty() ) stats << views;
    TQString date = Video::formatDate( m_video.uploadDate );
    if ( !date.isEmpty() ) stats << date;
    if ( m_video.duration <= 0 && m_video.viewCount >= 0 ) stats << i18n( "Live / upcoming" );
    if ( !stats.isEmpty() && y + fm.height() <= thumb.bottom() + 2 )
        p->drawText( x, y, avail, fm.lineSpacing(), TQt::AlignLeft | TQt::AlignTop | TQt::SingleLine,
                     KStringHandler::rPixelSqueeze( stats.join( TQString::fromUtf8( " \xc2\xb7 " ) ), fm, avail ) );
}

/* ---------------------------------------------------------- VideoListView */

VideoListView::VideoListView( TQWidget *parent, const char *name )
    : TDEListView( parent, name )
{
    addColumn( i18n( "Video" ) );
    header()->hide();
    setResizeMode( TQListView::LastColumn );
    setSorting( -1 );
    setAllColumnsShowFocus( true );
    setSelectionMode( TQListView::Single );
    setShowToolTips( true );
    setFullWidth( true );
    setHScrollBarMode( TQScrollView::AlwaysOff );

    connect( ThumbnailLoader::self(), TQ_SIGNAL( thumbnailReady( const TQString &, const TQPixmap & ) ),
             this, TQ_SLOT( slotThumbnail( const TQString &, const TQPixmap & ) ) );
    connect( this, TQ_SIGNAL( doubleClicked( TQListViewItem *, const TQPoint &, int ) ),
             this, TQ_SLOT( slotExecuted( TQListViewItem * ) ) );
    connect( this, TQ_SIGNAL( returnPressed( TQListViewItem * ) ),
             this, TQ_SLOT( slotExecuted( TQListViewItem * ) ) );
    connect( this, TQ_SIGNAL( currentChanged( TQListViewItem * ) ),
             this, TQ_SLOT( slotCurrentChanged( TQListViewItem * ) ) );
    connect( this, TQ_SIGNAL( contextMenu( TDEListView *, TQListViewItem *, const TQPoint & ) ),
             this, TQ_SLOT( slotContextMenu( TDEListView *, TQListViewItem *, const TQPoint & ) ) );
}

VideoItem *VideoListView::addVideo( const Video &video )
{
    if ( !video.isValid() || m_items.contains( video.id ) )
        return 0;
    VideoItem *item = new VideoItem( this, lastItem(), video );
    m_items.insert( video.id, item );
    if ( childCount() == 1 )
        triggerUpdate();
    return item;
}

void VideoListView::setVideos( const VideoList &videos )
{
    // Keep the current video and the scroll position across the rebuild.
    TQListViewItem *cur = currentItem();
    const TQString curId = cur ? static_cast<VideoItem *>( cur )->video().id : TQString::null;
    const bool wasSelected = cur && cur->isSelected();
    const int x = contentsX(), y = contentsY();

    clearVideos();
    for ( VideoList::ConstIterator it = videos.begin(); it != videos.end(); ++it )
        addVideo( *it );

    if ( !curId.isEmpty() ) {
        TQMap<TQString, VideoItem *>::ConstIterator it = m_items.find( curId );
        if ( it != m_items.end() ) {
            setCurrentItem( *it );
            if ( wasSelected )
                setSelected( *it, true );
        }
    }
    setContentsPos( x, y );
}

void VideoListView::removeVideo( const TQString &id )
{
    TQMap<TQString, VideoItem *>::Iterator it = m_items.find( id );
    if ( it == m_items.end() )
        return;
    delete *it;
    m_items.remove( it );
    if ( childCount() == 0 )
        triggerUpdate();
}

void VideoListView::clearVideos()
{
    m_items.clear();
    clear();
    triggerUpdate();
}

bool VideoListView::contains( const TQString &id ) const
{
    return m_items.contains( id );
}

Video VideoListView::currentVideo() const
{
    VideoItem *item = static_cast<VideoItem *>( currentItem() );
    return item ? item->video() : Video();
}

VideoList VideoListView::videos() const
{
    VideoList l;
    for ( TQListViewItem *i = firstChild(); i; i = i->nextSibling() )
        l.append( static_cast<VideoItem *>( i )->video() );
    return l;
}

void VideoListView::setEmptyText( const TQString &text )
{
    m_emptyText = text;
    if ( childCount() == 0 )
        triggerUpdate();
}

void VideoListView::drawContentsOffset( TQPainter *p, int ox, int oy, int cx, int cy, int cw, int ch )
{
    TDEListView::drawContentsOffset( p, ox, oy, cx, cy, cw, ch );
    if ( childCount() == 0 && !m_emptyText.isEmpty() ) {
        p->save();
        p->translate( -ox, -oy );
        TQRect r( contentsX() + 16, contentsY() + 16, visibleWidth() - 32, visibleHeight() - 32 );
        p->setPen( mix( colorGroup().text(), colorGroup().base(), 55 ) );
        p->drawText( r, TQt::AlignCenter | TQt::WordBreak, m_emptyText );
        p->restore();
    }
}

void VideoListView::slotThumbnail( const TQString &id, const TQPixmap & )
{
    TQMap<TQString, VideoItem *>::Iterator it = m_items.find( id );
    if ( it != m_items.end() )
        ( *it )->repaint();
}

void VideoListView::slotExecuted( TQListViewItem *item )
{
    if ( item )
        emit videoActivated( static_cast<VideoItem *>( item )->video() );
}

void VideoListView::slotCurrentChanged( TQListViewItem *item )
{
    emit currentVideoChanged( item ? static_cast<VideoItem *>( item )->video() : Video() );
}

void VideoListView::slotContextMenu( TDEListView *, TQListViewItem *item, const TQPoint &pos )
{
    if ( item )
        emit videoContextMenu( static_cast<VideoItem *>( item )->video(), pos );
}

#include "videolist.moc"
