/*
 * tde-youtube - browse and play YouTube videos in the Trinity Desktop
 * Copyright (C) 2026 tde-youtube contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TDEYOUTUBE_SETTINGSDIALOG_H
#define TDEYOUTUBE_SETTINGSDIALOG_H

#include <kdialogbase.h>

class TQCheckBox;
class TQLabel;
class TQPushButton;
class KComboBox;
class KIntNumInput;
class KLineEdit;
class KURLRequester;

/** "Configure tde-youtube" dialog. */
class SettingsDialog : public KDialogBase
{
    TQ_OBJECT

public:
    SettingsDialog( TQWidget *parent = 0, const char *name = 0 );

signals:
    void settingsChanged();

protected slots:
    virtual void slotOk();
    virtual void slotApply();
    virtual void slotDefault();

private slots:
    void slotClearHistory();
    void slotClearCache();
    void slotChanged();
    void slotUpdateYtdlp();
    void slotYtdlpUpdateStarted();
    void slotYtdlpUpdateFinished( int result, const TQString &message );

private:
    void load();
    void apply();
    void showYtdlpStatus( const TQString &error = TQString::null );

    TQCheckBox    *m_manageYtdlp;
    TQLabel       *m_ytdlpStatus;
    TQPushButton  *m_updateYtdlp;

    KURLRequester *m_ytdlp;
    KURLRequester *m_mpv;
    KComboBox     *m_cookiesBrowser;
    KURLRequester *m_cookiesFile;
    KIntNumInput  *m_results;
    KComboBox     *m_quality;
    TQCheckBox    *m_embed;
    KLineEdit     *m_extraArgs;
    TQCheckBox    *m_keepHistory;
    KIntNumInput  *m_maxHistory;
};

#endif
