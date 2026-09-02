#ifndef KISAKSTRIKE_RKHUD_PAUSEMENU_H
#define KISAKSTRIKE_RKHUD_PAUSEMENU_H

#include <rocketui/rocketui.h>

class RkPauseMenuButtons;

// Dismiss the in-game UI and hand the mouse back to the game. See the definition:
// hiding our own documents is not enough.
void RocketUI_ReturnToGame();

class RocketPauseMenuDocument
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
private:
    static bool m_bVisible;
};

#endif
