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
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }

private:
    static void PopulateMapList( void );
    static void StartServer( void );

    static bool m_bVisible;
    static bool m_bGrabbingInput;
};

#endif //KISAKSTRIKE_RKPANEL_PLAY_H
