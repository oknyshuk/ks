#include "rkhud_scoreboard.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "in_buttons.h"
#include "c_playerresource.h"

#include <c_team.h>

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT( RkHudScoreboard );

const char *RkHudScoreboard::kDocument = "hud_scoreboard.rml";

// Struct layout for data-binding model.
struct PlayerEntry
{
    int entid;
    Rml::String name;
    int teamnum;

    int cash;
    int kills;
    int deaths;
    int assists;
    int ping;
    bool alive;

    bool operator==( const PlayerEntry & ) const = default;
};
struct ScoreboardData
{
    int tScore;
    int ctScore;
    int numSpecs;
    Rml::Vector<PlayerEntry> ctPlayers;
    Rml::Vector<PlayerEntry> tPlayers;

    bool operator==( const ScoreboardData & ) const = default;
} scoreboardData;

static void BindScoreboard( Rml::DataModelConstructor &c )
{
    if ( auto entry = c.RegisterStruct<PlayerEntry>() )
    {
        entry.RegisterMember( "entid", &PlayerEntry::entid );
        entry.RegisterMember( "name", &PlayerEntry::name );
        entry.RegisterMember( "teamnum", &PlayerEntry::teamnum );
        entry.RegisterMember( "cash", &PlayerEntry::cash );
        entry.RegisterMember( "kills", &PlayerEntry::kills );
        entry.RegisterMember( "deaths", &PlayerEntry::deaths );
        entry.RegisterMember( "assists", &PlayerEntry::assists );
        entry.RegisterMember( "ping", &PlayerEntry::ping );
        entry.RegisterMember( "alive", &PlayerEntry::alive );
    }
    c.RegisterArray<Rml::Vector<PlayerEntry>>();

    if ( auto h = c.RegisterStruct<ScoreboardData>() )
    {
        h.RegisterMember( "t_score", &ScoreboardData::tScore );
        h.RegisterMember( "ct_score", &ScoreboardData::ctScore );
        h.RegisterMember( "num_specs", &ScoreboardData::numSpecs );
        h.RegisterMember( "ctplayers", &ScoreboardData::ctPlayers );
        h.RegisterMember( "tplayers", &ScoreboardData::tPlayers );
    }
    c.Bind( "scoreboard", &scoreboardData );
}
RK_HUD_SECTION( BindScoreboard );

// Rows are data-driven; these two are the containers the update needs to reach.
void RkHudScoreboard::OnLoad()
{
    m_elemCtSection = m_pDocument->GetElementById( "ct" );
    m_elemTSection = m_pDocument->GetElementById( "t" );
    if ( !m_elemCtSection || !m_elemTSection )
        Warning( "hud_scoreboard.rml is missing the 'ct'/'t' sections\n" );
}

RkHudScoreboard::RkHudScoreboard(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudScoreboard::~RkHudScoreboard() noexcept
{
    Unload();
}

void RkHudScoreboard::Update()
{
    IGameResources *gr = GameResources();
    if( !gr )
        return;

    const ScoreboardData previous = scoreboardData;

    // Rebuilt from scratch rather than reconciled in place. The old version
    // searched both team arrays for an existing row, erased on team change while
    // iterating, and patched the loop counter to compensate -- for at most 64
    // rows that are only looked at while the board is held open. It also stopped
    // at `i < maxClients`, so the last slot never appeared.
    scoreboardData.ctPlayers.clear();
    scoreboardData.tPlayers.clear();

    for( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        if( !gr->IsConnected( i ) )
            continue;

        const int team = gr->GetTeam( i );
        if( team != TEAM_CT && team != TEAM_TERRORIST )
            continue;   // spectators don't get a row

        C_CSPlayer *player = ToCSPlayer( UTIL_PlayerByIndex( i ) );
        if( !player )
            continue;

        PlayerEntry entry = {};
        entry.entid = i;
        entry.teamnum = team;
        entry.name = player->IsBot() ? ( Rml::String( "BOT " ) + gr->GetPlayerName( i ) )
                                     : Rml::String( gr->GetPlayerName( i ) );
        entry.cash = player->GetAccount();
        entry.kills = gr->GetKills( i );
        entry.deaths = gr->GetDeaths( i );
        entry.assists = g_PR->GetAssists( i );
        entry.ping = gr->GetPing( i );
        entry.alive = player->IsAlive();

        ( team == TEAM_CT ? scoreboardData.ctPlayers : scoreboardData.tPlayers )
            .push_back( entry );
    }

    scoreboardData.numSpecs = GetGlobalTeam( TEAM_SPECTATOR )->GetNumPlayers();
    scoreboardData.ctScore = gr->GetTeamScore( TEAM_CT );
    scoreboardData.tScore = gr->GetTeamScore( TEAM_TERRORIST );

    // Re-scanned every frame the board is held open; the numbers on it change on
    // kills and buys. Comparing the rows is far cheaper than rebuilding them.
    if( !( scoreboardData == previous ) )
        RkHudDirty( "scoreboard" );
}

// this is called every frame, keep that in mind.
void RkHudScoreboard::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        // Update the information every frame while the scoreboard is open.
        Update();

        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }
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

void RkHudScoreboard::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudScoreboard::ShouldDraw()
{
    int buttons = input->GetButtonBits( false );

    if( !(buttons & IN_SCORE) )
        return false;

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}
