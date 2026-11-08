/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mainwindow.h"

#include <tdeaboutdata.h>
#include <tdeapplication.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>

static const char description[] =
    I18N_NOOP( "Browse, search and play YouTube videos on the Trinity Desktop" );

static TDECmdLineOptions options[] =
{
    { "s", 0, 0 },
    { "search <query>", I18N_NOOP( "Search YouTube for <query> on start-up" ), 0 },
    { "+[URL]", I18N_NOOP( "YouTube video, playlist or channel address to open" ), 0 },
    TDECmdLineLastOption
};

int main( int argc, char **argv )
{
    TDEAboutData about( "tde-youtube", I18N_NOOP( "TDE YouTube" ), VERSION, description,
                        TDEAboutData::License_GPL_V2,
                        I18N_NOOP( "(C) 2026 tde-youtube contributors" ),
                        I18N_NOOP( "This program was largely written with the help of an AI coding "
                                   "assistant (\"vibe coded\"). It is provided as is, without any warranty.\n\n"
                                   "tde-youtube is not affiliated with, endorsed or sponsored by YouTube or Google. "
                                   "It uses yt-dlp and mpv to access and play videos." ),
                        "https://github.com/Yjlion/tde-youtube",
                        "https://github.com/Yjlion/tde-youtube/issues" );
    about.addAuthor( "tde-youtube contributors", I18N_NOOP( "Developers" ), 0,
                     "https://github.com/Yjlion/tde-youtube" );
    about.addCredit( "yt-dlp", I18N_NOOP( "Search and stream extraction" ), 0, "https://github.com/yt-dlp/yt-dlp" );
    about.addCredit( "mpv", I18N_NOOP( "Video playback" ), 0, "https://mpv.io" );

    TDECmdLineArgs::init( argc, argv, &about );
    TDECmdLineArgs::addCmdLineOptions( options );
    TDEApplication app;

    MainWindow *win = 0;
    if ( app.isRestored() ) {
        RESTORE( MainWindow );
    } else {
        win = new MainWindow;
        app.setMainWidget( win );
        win->show();

        TDECmdLineArgs *args = TDECmdLineArgs::parsedArgs();
        if ( args->count() > 0 )
            win->openUrl( TQString::fromLocal8Bit( args->arg( 0 ) ) );
        else if ( args->isSet( "search" ) )
            win->search( TQString::fromLocal8Bit( args->getOption( "search" ) ) );
        args->clear();
    }

    return app.exec();
}
