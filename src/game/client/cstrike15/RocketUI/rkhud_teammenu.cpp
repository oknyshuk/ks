#include "rkhud_teammenu.h"

#include "rkpanel.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "cdll_client_int.h" // extern globals to interfaces like engineclient

#include <rocketui/rmlui.h>

Rml::ElementDocument *RocketTeamMenuDocument::m_pInstance = nullptr;
bool RocketTeamMenuDocument::m_bVisible = false;
RocketTeamMenuEventListener* RocketTeamMenuDocument::m_pEventListener = nullptr;

// One action with the team as an argument, named by the buttons themselves in
// hud_teammenu.rml (data-event-click="jointeam( 3 )").
static void BindTeamMenu( Rml::DataModelConstructor &c )
{
    c.BindEventCallback( "jointeam", []( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments ) {
        if ( arguments.empty() )
            return;

        RocketTeamMenuDocument::ShowPanel( false );

        char command[32];
        V_snprintf( command, sizeof( command ), "jointeam %d", arguments[0].Get<int>() );
        engine->ClientCmd_Unrestricted( command );
    } );
}
RK_HUD_SECTION( BindTeamMenu );

void RocketTeamMenuEventListener::StartAlwaysListenEvents()
{
    ListenForGameEvent( "jointeam_failed" );
    ListenForGameEvent( "player_spawned" );
    ListenForGameEvent( "teamchange_pending" );
}

void RocketTeamMenuEventListener::StopAlwaysListenEvents()
{
    StopListeningForAllEvents();
}

//listen for a few events related to joining teams.
void RocketTeamMenuEventListener::FireGameEvent(IGameEvent *event)
{
    const char *type = event->GetName();

    if( !V_strcmp( type, "jointeam_failed" ) )
    {
        ConMsg("oy vey the jointeam failed\n");
    }
    else if( !V_strcmp( type, "player_spawned" ) )
    {
        C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
        // If this was us.
        if( localPlayer && localPlayer->GetUserID() == event->GetInt( "userid" ) )
        {
            RocketTeamMenuDocument::ShowPanel( false, false );
        }
    }
    else if( !V_strcmp( type, "teamchange_pending" ) )
    {
        RocketTeamMenuDocument::ShowPanel( false, false );
    }
}

void RocketTeamMenuDocument::LoadDialog()
{
    if( m_pInstance || !RkPanelLoad( m_pInstance, ROCKET_CONTEXT_HUD, "hud_teammenu.rml", LoadDialog, UnloadDialog ) )
        return;

    // Joining can fail or be pre-empted; the menu closes itself when it does.
    m_pEventListener = new RocketTeamMenuEventListener;
    m_pEventListener->StartAlwaysListenEvents();
}

void RocketTeamMenuDocument::UnloadDialog()
{
    if( m_pEventListener )
    {
        m_pEventListener->StopListeningForAllEvents();
        delete m_pEventListener;
        m_pEventListener = nullptr;
    }

    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketTeamMenuDocument::ShowPanel(bool bShow, bool immediate)
{
    if( bShow )
        LoadDialog();

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}
