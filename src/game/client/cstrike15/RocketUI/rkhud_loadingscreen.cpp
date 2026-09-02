#include "rkhud_loadingscreen.h"

#include "rkpanel.h"

#include "rkhud_model.h"

#include <rocketui/rmlui.h>

#include "cbase.h"
#include "cdll_client_int.h" // extern globals to interfaces like engineclient
#include "rkhud_teammenu.h"
#include "c_cs_player.h"

Rml::ElementDocument *RocketLoadingScreenDocument::m_pInstance = nullptr;
bool RocketLoadingScreenDocument::m_bVisible = false;

// hud_loadingscreen.rml dismisses itself on any click (data-event-mousedown).
static void BindLoadingScreen( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "dismiss_loadingscreen", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketLoadingScreenDocument::ShowPanel( false );
    } );
}
RK_HUD_SECTION( BindLoadingScreen );

void RocketLoadingScreenDocument::LoadDialog()
{
    RkPanelLoad( m_pInstance, ROCKET_CONTEXT_HUD, "hud_loadingscreen.rml", LoadDialog, UnloadDialog );
}

void RocketLoadingScreenDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketLoadingScreenDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
        LoadDialog();

    // Dismissing the loading screen is what joins the game and raises team select.
    const bool bWasVisible = m_bVisible;

    RkPanelShow( m_pInstance, m_bVisible, bShow );

    if( !bShow && bWasVisible )
    {
        engine->ClientCmd_Unrestricted( "joingame" );
        RocketTeamMenuDocument::ShowPanel( true );
    }
}
