// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#ifndef KISAKSTRIKE_RKPANEL_PLAY_H
#define KISAKSTRIKE_RKPANEL_PLAY_H

#include <rocketui/rocketui.h>

class RocketPlayDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }

    // Driven from panel_play.rml (data-event-click).
    static void StartServer( void );

private:
    static void PopulateMapList( void );

    static bool m_bVisible;
};

#endif //KISAKSTRIKE_RKPANEL_PLAY_H
