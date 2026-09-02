#include "rkhud_infobar.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT( RkHudInfoBar );

const char *RkHudInfoBar::kDocument = "hud_infobar.rml";

// Struct layout for data-binding model.
struct InfoBarData
{
    int hp;
    int armor;
    bool hasHelmet;
    int ammo;
    int ammoReserve;
    Rml::String fireModeString;
    Rml::String primaryString;
    Rml::String secondaryString;
    Rml::String knifeString;
    bool hasGrenade;
    bool hasFlash;
    bool hasFlashPair;
    bool hasDecoy;
    bool hasSmoke;
    bool hasFire;
    bool hasC4;

    bool operator==( const InfoBarData & ) const = default;
} infoBarData;

static void BindInfoBarData( Rml::DataModelConstructor &c )
{
    if ( auto h = c.RegisterStruct<InfoBarData>() )
    {
        h.RegisterMember( "hp", &InfoBarData::hp );
        h.RegisterMember( "armor", &InfoBarData::armor );
        h.RegisterMember( "ammo", &InfoBarData::ammo );
        h.RegisterMember( "ammo_reserve", &InfoBarData::ammoReserve );
        h.RegisterMember( "fire_mode_string", &InfoBarData::fireModeString );
        h.RegisterMember( "has_helmet", &InfoBarData::hasHelmet );
        h.RegisterMember( "primary_string", &InfoBarData::primaryString );
        h.RegisterMember( "secondary_string", &InfoBarData::secondaryString );
        h.RegisterMember( "knife_string", &InfoBarData::knifeString );
        h.RegisterMember( "has_grenade", &InfoBarData::hasGrenade );
        h.RegisterMember( "has_decoy", &InfoBarData::hasDecoy );
        h.RegisterMember( "has_flash", &InfoBarData::hasFlash );
        h.RegisterMember( "has_flash_pair", &InfoBarData::hasFlashPair );
        h.RegisterMember( "has_smoke", &InfoBarData::hasSmoke );
        h.RegisterMember( "has_fire", &InfoBarData::hasFire );
        h.RegisterMember( "has_c4", &InfoBarData::hasC4 );
    }
    c.Bind( "infobar", &infoBarData );
}
RK_HUD_SECTION( BindInfoBarData );

// Fills `infoBarData` from the player. Kept whole-struct so ShowPanel can tell
// whether anything actually changed before waking the data bindings.
static void UpdateInfoFromPlayer( const C_CSPlayer &pPlayer )
{
    infoBarData.hp = pPlayer.GetHealth();
    infoBarData.armor = pPlayer.ArmorValue();
    infoBarData.hasHelmet = false;
    if( pPlayer.HasHelmet() )
        infoBarData.hasHelmet = true;

    infoBarData.fireModeString = " ";
    infoBarData.primaryString = " ";
    infoBarData.secondaryString = " ";
    infoBarData.knifeString = " ";
    infoBarData.hasGrenade = false;
    //infoBarData.hasFlash = false;
    //infoBarData.hasFlashPair = false;
    infoBarData.hasDecoy = false;
    infoBarData.hasSmoke = false;
    infoBarData.hasFire = false;
    infoBarData.hasC4 = false;

    int flashbangAmount = 0;
    for( int i = 0; i < MAX_WEAPONS; i++ )
    {
        CWeaponCSBase *weapon = (CWeaponCSBase*)pPlayer.GetWeapon(i);
        if( !weapon )
            continue;

        int slot = weapon->GetSlot();
        const char *name;

        switch( slot )
        {
            case WEAPON_SLOT_RIFLE:
                name = V_strstr(weapon->GetName(), "_");
                if( name && name[0] )
                    infoBarData.primaryString = name+1;
                break;
            case WEAPON_SLOT_PISTOL:
                name = V_strstr(weapon->GetName(), "_");
                if( name && name[0] )
                    infoBarData.secondaryString = name+1;
                break;
            case WEAPON_SLOT_KNIFE:
                name = V_strstr(weapon->GetName(), "_");
                if( name && name[0] )
                    infoBarData.knifeString = name+1;
                break;
            case WEAPON_SLOT_GRENADES:
            {
                int weaponID = weapon->GetCSWeaponID();
                switch( weaponID )
                {
                    case WEAPON_HEGRENADE:
                        infoBarData.hasGrenade = true;
                        break;
                    case WEAPON_FLASHBANG:
                        flashbangAmount++;
                        break;
                    case WEAPON_SMOKEGRENADE:
                        infoBarData.hasSmoke = true;
                        break;
                    case WEAPON_MOLOTOV:
                        infoBarData.hasFire = true;
                        break;
                    case WEAPON_INCGRENADE:
                        infoBarData.hasFire = true;
                        break;
                    case WEAPON_DECOY:
                        infoBarData.hasDecoy = true;
                    default:
                        break;
                }
                break;
            }
            case WEAPON_SLOT_C4:
                infoBarData.hasC4 = true;
                break;
            default:
                break;
        }
    }
    infoBarData.hasFlash = ( flashbangAmount == 1 );
    infoBarData.hasFlashPair = ( flashbangAmount == 2 );

    CWeaponCSBase *activeWeapon = pPlayer.GetActiveCSWeapon();
    if( activeWeapon )
    {
        infoBarData.ammo = activeWeapon->Clip1();
        infoBarData.ammoReserve = activeWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
        if( activeWeapon->IsFullAuto() )
            infoBarData.fireModeString = "AUTO";
        else if( activeWeapon->IsInBurstMode() )
            infoBarData.fireModeString = "BURST";
        else
            infoBarData.fireModeString = "SINGLE";
    }
}

RkHudInfoBar::RkHudInfoBar(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudInfoBar::~RkHudInfoBar() noexcept
{
    Unload();
}

// Called every frame by the HUD system (CHud::DoElementThink -> SetActive).
void RkHudInfoBar::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( !bShow )
    {
        if( m_bVisible )
            m_pDocument->Hide();
        m_bVisible = false;
        return;
    }

    if( !m_bVisible )
        m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
    m_bVisible = true;

    C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

    // observing someone? switch to that player.
    if( pPlayer && pPlayer->IsObserver() &&
        ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pPlayer->GetObserverMode() == OBS_MODE_CHASE ) )
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );

    if( !pPlayer )
        return;

    const InfoBarData previous = infoBarData;
    UpdateInfoFromPlayer( *pPlayer );

    // The HUD this drives changes a few times a second at most. Dirtying
    // unconditionally made RmlUi re-evaluate every binding and re-shape the text
    // on all of them, every frame.
    if( !( infoBarData == previous ) )
        RkHudDirty( "infobar" );
}

void RkHudInfoBar::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudInfoBar::ShouldDraw()
{
    C_CSPlayer *localPlayer = C_CSPlayer::GetLocalCSPlayer();

    return localPlayer &&
    cl_drawhud.GetBool() &&
    ( localPlayer->IsAlive() || ( localPlayer->IsObserver() && localPlayer->GetObserverMode() == OBS_MODE_IN_EYE || localPlayer->GetObserverMode() == OBS_MODE_CHASE ) ) &&
    CHudElement::ShouldDraw();
}
