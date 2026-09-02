#include "rkhud_killfeed.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"

#include <rocketui/rmlui.h>
#include <deque>

DECLARE_HUDELEMENT( RkHudKillfeed );

const char *RkHudKillfeed::kDocument = "hud_killfeed.rml";

ConVar rocket_hud_killfeed_linger_time( "rocket_hud_killfeed_linger_time", "5", FCVAR_ARCHIVE, "How long in seconds to keep each killfeed entry on screen." );

struct KillfeedEntry
{
    Rml::String attackerName;
    Rml::String gunName;
    Rml::String victimName;
    bool headshot;
    bool wallbang;
    float noticeSpawnTime;
};

// Struct layout for data-binding model.
struct KillFeedData
{
    std::deque<KillfeedEntry> entries;
} killFeedData;

static void BindKillfeed( Rml::DataModelConstructor &c )
{
    if ( auto entry = c.RegisterStruct<KillfeedEntry>() )
    {
        entry.RegisterMember( "attacker_name", &KillfeedEntry::attackerName );
        entry.RegisterMember( "gun_name", &KillfeedEntry::gunName );
        entry.RegisterMember( "victim_name", &KillfeedEntry::victimName );
        entry.RegisterMember( "headshot", &KillfeedEntry::headshot );
        entry.RegisterMember( "wallbang", &KillfeedEntry::wallbang );
    }
    c.RegisterArray<std::deque<KillfeedEntry>>();

    if ( auto h = c.RegisterStruct<KillFeedData>() )
        h.RegisterMember( "entries", &KillFeedData::entries );
    c.Bind( "killfeed", &killFeedData );
}
RK_HUD_SECTION( BindKillfeed );

void RkHudKillfeed::OnPlayerDeath( IGameEvent *event )
{
    KillfeedEntry entry;
    int nAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
    int nVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );

    CCSPlayer* attacker = ToCSPlayer( ClientEntityList().GetBaseEntity( nAttacker ) );
    CCSPlayer* victim = ToCSPlayer( ClientEntityList().GetBaseEntity( nVictim ) );

    if( !attacker || !victim )
        return;

    entry.victimName = victim->GetPlayerName();
    entry.attackerName = attacker->GetPlayerName();
    entry.gunName = event->GetString( "weapon" );
    entry.headshot = ( event->GetInt( "headshot" ) > 0 );
    entry.wallbang = ( event->GetInt( "penetrated" ) > 0 );

    entry.noticeSpawnTime = gpGlobals->curtime;

    killFeedData.entries.push_back( entry );

    RkHudDirty( "killfeed" );
}

// called every frame
void RkHudKillfeed::CheckForOldEntries()
{
    if( killFeedData.entries.empty() )
        return;

    // pop off the first guy, then we're done. This gets called often enough to not matter about the rest.
    if( (gpGlobals->curtime - killFeedData.entries.front().noticeSpawnTime) > rocket_hud_killfeed_linger_time.GetFloat() )
    {
        killFeedData.entries.pop_front();
        RkHudDirty( "killfeed" );
    }
}

// The killfeed has no open/close of its own: it is up whenever the HUD is.
void RkHudKillfeed::OnLoad()
{
    ShowPanel( true, false );
}

RkHudKillfeed::RkHudKillfeed(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudKillfeed::~RkHudKillfeed() noexcept
{
    StopListeningForAllEvents();

    Unload();
}

void RkHudKillfeed::LevelInit()
{
    ListenForGameEvent( "player_death" );

    RkHudDocument::LevelInit();
}

void RkHudKillfeed::LevelShutdown()
{
    killFeedData.entries.clear();

    RkHudDocument::LevelShutdown();
}

void RkHudKillfeed::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }
        CheckForOldEntries();
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

void RkHudKillfeed::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudKillfeed::ShouldDraw()
{
    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

void RkHudKillfeed::FireGameEvent(IGameEvent *event)
{
    // We only listen for "player_death"
    OnPlayerDeath( event );
}
