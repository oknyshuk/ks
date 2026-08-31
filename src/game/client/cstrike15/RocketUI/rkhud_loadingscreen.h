#ifndef KISAKSTRIKE_RKHUD_LOADINGSCREEN_H
#define KISAKSTRIKE_RKHUD_LOADINGSCREEN_H

#include <rocketui/rocketui.h>

class RocketLoadingScreenDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

    RocketLoadingScreenDocument( );
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
    static bool m_bVisible;
};


#endif //KISAKSTRIKE_RKHUD_LOADINGSCREEN_H
