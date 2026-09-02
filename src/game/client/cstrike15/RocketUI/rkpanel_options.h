#ifndef KISAKSTRIKE_RKPANEL_OPTIONS_H
#define KISAKSTRIKE_RKPANEL_OPTIONS_H

#include <rocketui/rocketui.h>

class RocketOptionsDocument
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

    // Driven from panel_options.rml (data-event-click / data-event-change).
    static void SwitchTab( const char *tabId );
    static void PopulateResolution( void );

    // True while PopulateControls syncs the controls to the current values, so
    // the change actions can tell that apart from a user edit.
    static bool m_bPopulating;

private:
    static void PopulateControls( void );
    static void PopulateDisplayMode( void );

    static bool m_bVisible;
};

#endif //KISAKSTRIKE_RKPANEL_OPTIONS_H
