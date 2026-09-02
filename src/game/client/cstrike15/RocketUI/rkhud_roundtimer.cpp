#include "rkhud_roundtimer.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "c_playerresource.h"

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT( RkHudRoundTimer );

const char *RkHudRoundTimer::kDocument = "hud_roundtimer.rml";

// Struct layout for data-binding model.
struct RoundTimerData
{
    int MinutesLeft;
    int SecondsLeft;
    bool bombPlanted;
    int ctScore;
    int tScore;

    bool operator==( const RoundTimerData & ) const = default;
} roundTimerData;

static void BindRoundTimerData( Rml::DataModelConstructor &c )
{
    if ( auto h = c.RegisterStruct<RoundTimerData>() )
    {
        h.RegisterMember( "minutes_left", &RoundTimerData::MinutesLeft );
        h.RegisterMember( "seconds_left", &RoundTimerData::SecondsLeft );
        h.RegisterMember( "bomb_planted", &RoundTimerData::bombPlanted );
        h.RegisterMember( "ct_score", &RoundTimerData::ctScore );
        h.RegisterMember( "t_score", &RoundTimerData::tScore );
    }
    c.Bind( "roundtimer", &roundTimerData );
}
RK_HUD_SECTION( BindRoundTimerData );

RkHudRoundTimer::RkHudRoundTimer(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudRoundTimer::~RkHudRoundTimer() noexcept
{
    Unload();
}

void RkHudRoundTimer::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }

        const RoundTimerData previous = roundTimerData;

        int remainingTime;
        if ( CSGameRules()->IsFreezePeriod() )
        {
            // countdown to the start of the round while we're in freeze period
            remainingTime = (int)ceil( CSGameRules()->GetRoundStartTime() - gpGlobals->curtime );
        }
        else
        {
            remainingTime = (int)ceil( CSGameRules()->GetRoundRemainingTime() );
        }
        roundTimerData.SecondsLeft = remainingTime % 60;
        roundTimerData.MinutesLeft = remainingTime / 60;
        roundTimerData.ctScore = g_PR->GetTeamScore( TEAM_CT );
        roundTimerData.tScore = g_PR->GetTeamScore( TEAM_TERRORIST );
        roundTimerData.bombPlanted = CSGameRules()->m_bBombPlanted;

        // A clock that ticks once a second, polled every frame.
        if( !( roundTimerData == previous ) )
            RkHudDirty( "roundtimer" );
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudRoundTimer::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudRoundTimer::ShouldDraw()
{
    C_CSPlayer *localPlayer = C_CSPlayer::GetLocalCSPlayer();

    if( !localPlayer || localPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
        return false;

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}
