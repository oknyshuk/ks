#include "rkhud_pausemenu.h"

#include "rkpanel.h"

#include "rkhud_model.h"

#include <rocketui/rmlui.h>

#include "cdll_client_int.h" // extern globals to interfaces like engineclient

#include "rkhud_chat.h"
#include "rkhud_teammenu.h"
#include "rkhud_buymenu.h"
#include "rkpanel_options.h"
#include "iengineui.h"

Rml::ElementDocument *RocketPauseMenuDocument::m_pInstance = nullptr;
bool RocketPauseMenuDocument::m_bVisible = false;

// The engine keeps its own GameUI-visible flag, and while it is set UI_ActivateMouse()
// deactivates the mouse every frame -- no mouse look, even with nothing on screen. So a
// panel that means "back to the game" must not just hide itself: it asks the engine to
// dismiss the GameUI, and CGameUI::OnGameUIHidden takes our documents down. One-way
// flow (engine -> documents) instead of two half-wired state machines.
void RocketUI_ReturnToGame()
{
    if ( engine->IsInGame() && engineui && engineui->IsGameUIVisible() )
        engine->ClientCmd_Unrestricted( "gameui_hide\n" );
}

// The buttons name their own action in hud_pausemenu.rml (data-event-click), so
// there is no listener walking every <button> and no chain of id comparisons.
static void BindPauseMenu( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "pm_resume", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketUI_ReturnToGame();
    } );
    c.BindEventCallback( "pm_choose_team", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketTeamMenuDocument::ShowPanel( true );
        RocketPauseMenuDocument::ShowPanel( false );
    } );
    c.BindEventCallback( "pm_options", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        RocketOptionsDocument::ShowPanel( true );
        RocketPauseMenuDocument::ShowPanel( false );
    } );
    c.BindEventCallback( "pm_disconnect", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & ) {
        engine->ClientCmd_Unrestricted( "disconnect" );
    } );
}
RK_HUD_SECTION( BindPauseMenu );

void RocketPauseMenuDocument::LoadDialog()
{
    RkPanelLoad( m_pInstance, ROCKET_CONTEXT_HUD, "hud_pausemenu.rml", LoadDialog, UnloadDialog );
}

void RocketPauseMenuDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketPauseMenuDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
    {
        LoadDialog();

        // The chat and the buy menu are modal in their own right: ESC belongs to
        // them while either is up.
        RkHudChat *pChat = GET_HUDELEMENT( RkHudChat );
        RkHudBuyMenu *pBuyMenu = GET_HUDELEMENT( RkHudBuyMenu );
        if( !pChat || !pBuyMenu )
            return;
        if( pChat->ChatRaised() || pBuyMenu->m_bVisible || !pBuyMenu->m_pDocument )
            return;
    }

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}
