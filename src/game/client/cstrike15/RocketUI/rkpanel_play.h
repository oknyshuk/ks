// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKPANEL_PLAY_H
#define KISAKSTRIKE_RKPANEL_PLAY_H

#include <rocketui/rocketui.h>

class RkPlayClickListener;

class RocketPlayDocument
{
    friend class RkPlayClickListener;
protected:
    static Rml::ElementDocument *m_pInstance;

    RocketPlayDocument( );
public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    // True while this panel legitimately owns mouse/keyboard input (polled).
    static bool OwnsInput();
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }

private:
    static void PopulateMapList( void );
    static void StartServer( void );

    static bool m_bVisible;
};

#endif //KISAKSTRIKE_RKPANEL_PLAY_H
