#include "rkhud_radar.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "c_playerresource.h"

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT( RkHudRadar );

const char *RkHudRadar::kDocument = "hud_radar.rml";
DECLARE_HUD_MESSAGE( RkHudRadar, ProcessSpottedEntityUpdate );

ConVar rocket_hud_radar_info_linger_time( "rocket_hud_radar_info_linger_time", "3", FCVAR_ARCHIVE, "How long in seconds does the data stay visible after an update" );
ConVar rocket_hud_radar_scale( "rocket_hud_radar_scale", "0.15", FCVAR_ARCHIVE, "scale for radar" );

// What hud_radar.rml binds to. One blip per player slot; the document positions
// and colours them, so this file only does the projection maths.
struct RadarBlip
{
    int x = 0, y = 0;
    bool ct = false;    // false = T; picks the .player-ct / .player-t colour
    bool shown = false;

    bool operator==( const RadarBlip & ) const = default;
};
struct RadarData
{
    int width = 0, height = 0;
    Rml::Vector<RadarBlip> blips;

    bool operator==( const RadarData & ) const = default;
} radarData;

static void BindRadar( Rml::DataModelConstructor &c )
{
    if ( auto blip = c.RegisterStruct<RadarBlip>() )
    {
        blip.RegisterMember( "x", &RadarBlip::x );
        blip.RegisterMember( "y", &RadarBlip::y );
        blip.RegisterMember( "ct", &RadarBlip::ct );
        blip.RegisterMember( "shown", &RadarBlip::shown );
    }
    c.RegisterArray<Rml::Vector<RadarBlip>>();

    if ( auto handle = c.RegisterStruct<RadarData>() )
    {
        handle.RegisterMember( "width", &RadarData::width );
        handle.RegisterMember( "height", &RadarData::height );
        handle.RegisterMember( "blips", &RadarData::blips );
    }
    // One blip per player slot for the lifetime of the model: data-for only
    // instances elements when the array *size* changes, so a fixed size means the
    // blips are created once and only ever moved afterwards.
    radarData.blips.assign( MAX_PLAYERS, RadarBlip{} );
    c.Bind( "radar", &radarData );
}
RK_HUD_SECTION( BindRadar );

static void RadarSizeChanged( IConVar *pConvar, const char *szOldValue, float fOldValue )
{
    RkHudRadar *pRadar = GET_HUDELEMENT( RkHudRadar );
    if( !pRadar )
    {
        Warning( "Couldn't grab hud radar to update size!\n" );
        return;
    }
    pRadar->UpdateRadarSize();
}
ConVar rocket_hud_radar_height( "rocket_hud_radar_height", "400", FCVAR_ARCHIVE, "height in pixels for the radar", RadarSizeChanged );
ConVar rocket_hud_radar_width( "rocket_hud_radar_width", "400", FCVAR_ARCHIVE, "width in pixels for the radar", RadarSizeChanged );

bool RkHudRadar::MsgFunc_ProcessSpottedEntityUpdate(const CCSUsrMsg_ProcessSpottedEntityUpdate &msg)
{
    if( msg.new_update() )
    {
        // Clear everything.
        //for( int i = 0; i < MAX_PLAYERS; i++ )
        //    m_spottedPlayers[i].timelastSpotted = 0.0f;
    }

    for( int i = 0; i < msg.entity_updates_size(); i++ )
    {
        const auto &update = msg.entity_updates(i);

        int entID = update.entity_idx();
        // make sure this is a valid id for any type of entity.
        if( entID < 1 || entID >= MAX_EDICTS )
            continue;

        // these are sent from the server as /4
        int x = update.origin_x() * 4;
        int y = update.origin_y() * 4;
        int z = update.origin_z() * 4;
        int yaw = update.angle_y();

        const char *szEntClassName;
        int classID = update.class_id();
        for ( ClientClass *pCur = g_pClientClassHead; pCur; pCur = pCur->m_pNext )
        {
            if( pCur->m_ClassID == classID )
            {
                szEntClassName = pCur->GetName();
                break;
            }
        }

        if( !szEntClassName )
        {
            Warning( "Unknown entity class received in ProcessSpottedEntityUpdate.\n" );
        }

        // Clients are unaware of the defuser class type, so we need to flag defuse entities manually
        if( update.defuser() )
        {
            // TODO: this is the defuser!
        }
        else if( !V_strcmp( "CCSPlayer", szEntClassName ) )
        {
            // out of bounds!
            if( entID < 1 || entID > MAX_PLAYERS )
                return true;

            // subtract 1 to convert 1-64 to 0-63
            SpottedInfo &playerInfo = m_spottedPlayers[ (entID-1) ];
            playerInfo.entId = update.entity_idx();
            playerInfo.originX = x;
            playerInfo.originY = y;
            playerInfo.originZ = z;
            playerInfo.angleYaw = yaw;
            if( update.has_player_has_defuser() )
            {
                playerInfo.playerWithDefuser = true;
                // TODO: set defuser pos
            }
            if( update.player_has_c4() )
            {
                playerInfo.playerWithC4 = true;
                // TODO: set bomb pos
            }
            playerInfo.timelastSpotted = gpGlobals->curtime;
        }
        else if( !V_strcmp( "CC4", szEntClassName ) || !V_strcmp( "CPlantedC4", szEntClassName ) )
        {
            // TODO: set bomb pos
        }
        else if( !V_strcmp( "CHostage", szEntClassName ) )
        {
            // TODO: set hostage pos
        }
    }

    return true;
}

// https://www.unknowncheats.me/forum/general-programming-and-reversing/135529-implement-simple-radar.html
static inline void RotatePoint( float x, float y, float centerX, float centerY, float angle, float *outX, float *outY )
{
    angle = DEG2RAD( angle );

    float cosTheta = cosf( angle );
    float sinTheta = sinf( angle );

    *outX = (cosTheta * ( x - centerX )) - (sinTheta * ( y - centerY ));
    *outY = (sinTheta * ( x - centerX )) + (cosTheta * ( y - centerY ));
    *outX += centerX;
    *outY += centerY;
}

void RkHudRadar::UpdateRadarSize()
{
    if( !m_pDocument )
        return;

    m_radarWidth = rocket_hud_radar_width.GetFloat();
    m_radarHeight = rocket_hud_radar_height.GetFloat();
    m_radarCenterX = ( m_radarWidth / 2.0f );
    m_radarCenterY = ( m_radarHeight / 2.0f );

    // hud_radar.rml sizes itself off these; blips are children, so their
    // coordinates stay relative to the radar box.
    radarData.width = (int)m_radarWidth;
    radarData.height = (int)m_radarHeight;
    RkHudDirty( "radar" );
}

void RkHudRadar::UpdateRadarFrame()
{
    C_CSPlayer *activePlayer = C_CSPlayer::GetLocalCSPlayer();
    if( !activePlayer )
        return;

    // observing someone? switch to that player.
    if( activePlayer->IsObserver() && (activePlayer->GetObserverMode() == OBS_MODE_IN_EYE || activePlayer->GetObserverMode() == OBS_MODE_CHASE) )
        activePlayer = ToCSPlayer(activePlayer->GetObserverTarget());

    if( !activePlayer )
        return;

    IGameResources *gr = GameResources();
    float currTime = gpGlobals->curtime;
    const RadarData previous = radarData;

    // The Radar packets are designed to supplement an existing regular radar.
    // We will do a regular radar, but with the dormant check, see if we have some recent maphack data from server.
    for( int i = 1; i <= MAX_PLAYERS; i++ )
    {
        // array index for the blip
        int index = i - 1;
        RadarBlip &blip = radarData.blips[index];

        if( i == engine->GetLocalPlayer() )
        {
            blip.shown = false;
            continue;
        }

        CBasePlayer *player = UTIL_PlayerByIndex( i );
        const SpottedInfo &playerInfo = m_spottedPlayers[index];

        if( !player || !player->IsAlive() )
        {
            blip.shown = false;
            continue;
        }

        // if dormant and we dont have any recent information from the server
        if( player->IsDormant() && ( ( currTime - playerInfo.timelastSpotted ) > rocket_hud_radar_info_linger_time.GetFloat() ) )
        {
            blip.shown = false;
            continue;
        }

        // At this point we either have a visible player or recent server radar data.
        // GameResources knows the team even while the entity is dormant.
        blip.ct = gr && gr->GetTeam( i ) == TEAM_CT;

        float originDiffX;
        float originDiffY;

        if( player->IsDormant() )
        {
            originDiffX = activePlayer->GetAbsOrigin().x - playerInfo.originX;
            originDiffY = activePlayer->GetAbsOrigin().y - playerInfo.originY;
        }
        else
        {
           originDiffX = activePlayer->GetAbsOrigin().x - player->GetAbsOrigin().x;
           originDiffY = activePlayer->GetAbsOrigin().y - player->GetAbsOrigin().y;
        }

        originDiffX *= rocket_hud_radar_scale.GetFloat();
        originDiffY *= rocket_hud_radar_scale.GetFloat();
        originDiffX *= -1; // x goes other way

        // add the center of the radar.
        originDiffX += m_radarCenterX;
        originDiffY += m_radarCenterY;

        float rotatedX;
        float rotatedY;
        RotatePoint( originDiffX, originDiffY, m_radarCenterX, m_radarCenterY, activePlayer->EyeAngles().y - 90.0f, &rotatedX, &rotatedY );

        // these guys are off the radar. Go ahead and hide them.
        if( rotatedX > m_radarWidth || rotatedX < 0 || rotatedY > m_radarHeight || rotatedY < 0 )
        {
            blip.shown = false;
            continue;
        }

        blip.x = int( rotatedX );
        blip.y = int( rotatedY );
        blip.shown = true;
    }

    // Everyone moves at once or not at all, so one compare covers the lot.
    if( !( radarData == previous ) )
        RkHudDirty( "radar" );
}

void RkHudRadar::OnLoad()
{
    UpdateRadarSize();
}

RkHudRadar::RkHudRadar(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudRadar::~RkHudRadar() noexcept
{
    Unload();
}

void RkHudRadar::LevelInit()
{
    RkHudDocument::LevelInit();

    HOOK_HUD_MESSAGE( RkHudRadar, ProcessSpottedEntityUpdate );
}

void RkHudRadar::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }
        UpdateRadarFrame();
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

void RkHudRadar::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudRadar::ShouldDraw()
{
    C_CSPlayer *localPlayer = C_CSPlayer::GetLocalCSPlayer();

    return localPlayer &&
           cl_drawhud.GetBool() &&
           ( localPlayer->IsAlive() || ( localPlayer->IsObserver() && localPlayer->GetObserverMode() == OBS_MODE_IN_EYE || localPlayer->GetObserverMode() == OBS_MODE_CHASE ) ) &&
           CHudElement::ShouldDraw();
}
