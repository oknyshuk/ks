#include "rkhud_buymenu.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT( RkHudBuyMenu );

const char *RkHudBuyMenu::kDocument = "hud_buymenu.rml";

struct ItemListEntry
{
    Rml::String itemName;
    int itemPrice;

    bool operator==( const ItemListEntry & ) const = default;
};

// struct layout for data-binding model.
struct BuyMenuData
{
    int playerCash;
    int playerTeamNum;
    int buyTimeLeft; // seconds
    Rml::Vector<ItemListEntry> gearList;
    Rml::Vector<ItemListEntry> grenadeList;
    Rml::Vector<ItemListEntry> pistolList;
    Rml::Vector<ItemListEntry> rifleList;
    Rml::Vector<ItemListEntry> smgList;
    Rml::Vector<ItemListEntry> heavyList;

    bool operator==( const BuyMenuData & ) const = default;
} buyMenuData;

// Buying and closing are driven from the document (data-event-click / -keyup in
// hud_buymenu.rml). The click used to arrive as a bubbled mousedown on some inner
// span, from which this had to walk up to the .item container, QuerySelector the
// .item_name, read its inner RML back out and re-parse the weapon name -- all to
// recover the string the data model handed the document in the first place. The
// document now passes it straight back.
static void BuyItem( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments )
{
    if ( arguments.empty() )
        return;

    const Rml::String itemName = arguments[0].Get<Rml::String>();

    // Names arrive as WEAPON_AK47 / EQUIPMENT_KEVLAR; `buy` wants the tail.
    const char *underscore = V_strstr( itemName.c_str(), "_" );
    if ( !underscore )
        return;

    char command[512];
    V_snprintf( command, sizeof( command ), "buy %s", underscore + 1 );
    engine->ExecuteClientCmd( command );

    if ( RkHudBuyMenu *pBuyMenu = GET_HUDELEMENT( RkHudBuyMenu ) )
        pBuyMenu->UpdateBuyMenu();
}

// Any key closes the menu, the same clientside event the open path uses.
static void CloseBuyMenu( Rml::DataModelHandle, Rml::Event &, const Rml::VariantList & )
{
    C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
    IGameEvent *pEvent = gameeventmanager->CreateEvent( "buymenu_close" );
    if ( localPlayer && pEvent )
    {
        pEvent->SetInt( "userid", localPlayer->GetUserID() );
        gameeventmanager->FireEventClientSide( pEvent );
    }
}

static void BindBuyMenu( Rml::DataModelConstructor &c )
{
    if ( auto entry = c.RegisterStruct<ItemListEntry>() )
    {
        entry.RegisterMember( "item_name", &ItemListEntry::itemName );
        entry.RegisterMember( "item_price", &ItemListEntry::itemPrice );
    }
    c.RegisterArray<Rml::Vector<ItemListEntry>>();

    if ( auto h = c.RegisterStruct<BuyMenuData>() )
    {
        h.RegisterMember( "player_cash", &BuyMenuData::playerCash );
        h.RegisterMember( "player_teamnum", &BuyMenuData::playerTeamNum );
        h.RegisterMember( "buy_time_left", &BuyMenuData::buyTimeLeft );
        h.RegisterMember( "gear_list", &BuyMenuData::gearList );
        h.RegisterMember( "grenade_list", &BuyMenuData::grenadeList );
        h.RegisterMember( "pistol_list", &BuyMenuData::pistolList );
        h.RegisterMember( "rifle_list", &BuyMenuData::rifleList );
        h.RegisterMember( "smg_list", &BuyMenuData::smgList );
        h.RegisterMember( "heavy_list", &BuyMenuData::heavyList );
    }
    c.Bind( "buymenu", &buyMenuData );

    c.BindEventCallback( "buy_item", &BuyItem );
    c.BindEventCallback( "close_buymenu", &CloseBuyMenu );
}
RK_HUD_SECTION( BindBuyMenu );

// Updates the buy menu options via the game's weapon database.
// Called sparingly when buymenu is opened/interacted.
void RkHudBuyMenu::UpdateBuyMenu()
{
    C_CSPlayer *localPlayer = C_CSPlayer::GetLocalCSPlayer();
    if( !localPlayer )
        return;

    buyMenuData.gearList.clear();
    buyMenuData.grenadeList.clear();
    buyMenuData.pistolList.clear();
    buyMenuData.rifleList.clear();
    buyMenuData.smgList.clear();
    buyMenuData.heavyList.clear();

    for( int i = WEAPON_FIRST ; i < WEAPON_MAX; i++ )
    {
        if( localPlayer->CanAcquire( (CSWeaponID)i, AcquireMethod::Buy ) != AcquireResult::Allowed )
            continue;

        const CCSWeaponInfo* info = GetWeaponInfo( (CSWeaponID)i );
        if( !info )
            continue;

        ItemListEntry entry;
        entry.itemPrice = info->GetWeaponPrice();
        entry.itemName = WeaponIdAsString( (CSWeaponID)i );

        int weaponType = info->GetWeaponType();

        switch( weaponType )
        {
            case WEAPONTYPE_EQUIPMENT:
                buyMenuData.gearList.push_back( entry );
                break;
            case WEAPONTYPE_GRENADE:
                buyMenuData.grenadeList.push_back( entry );
                break;
            case WEAPONTYPE_PISTOL:
                buyMenuData.pistolList.push_back( entry );
                break;
            case WEAPONTYPE_RIFLE:
            case WEAPONTYPE_SNIPER_RIFLE:
                buyMenuData.rifleList.push_back( entry );
                break;
            case WEAPONTYPE_SUBMACHINEGUN:
                buyMenuData.smgList.push_back( entry );
                break;
            case WEAPONTYPE_SHOTGUN:
            case WEAPONTYPE_MACHINEGUN:
                buyMenuData.heavyList.push_back( entry );
                break;
            default:
            {
                continue;
            }
        }
    }

    RkHudDirty( "buymenu" );
}

// called 1x per frame
void RkHudBuyMenu::OnNewFrameBuyMenu()
{
    const BuyMenuData previous = buyMenuData;

    float timeIntoRound = CSGameRules()->GetRoundElapsedTime();
    float buyTime = CSGameRules()->GetBuyTimeLength();

    buyMenuData.buyTimeLeft = int(buyTime - timeIntoRound);

    C_CSPlayer *localPlayer = C_CSPlayer::GetLocalCSPlayer();
    if( localPlayer )
    {
        buyMenuData.playerCash = localPlayer->GetAccount();
        buyMenuData.playerTeamNum = localPlayer->GetTeamNumber();
    }

    // Cash and the buy timer change once a second at most.
    if( !( buyMenuData == previous ) )
        RkHudDirty( "buymenu" );
}

RkHudBuyMenu::RkHudBuyMenu(const char *value) : RkHudDocument( value )
{
    SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
}

RkHudBuyMenu::~RkHudBuyMenu() noexcept
{
    StopListeningForAllEvents();

    Unload();
}

void RkHudBuyMenu::LevelInit()
{
    ListenForGameEvent( "buymenu_open" );
    ListenForGameEvent( "buymenu_close" );

    RkHudDocument::LevelInit();
}

void RkHudBuyMenu::ShowPanel(bool bShow, bool force)
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            UpdateBuyMenu();
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto, Rml::ScrollFlag::None );
        }
        OnNewFrameBuyMenu();
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

void RkHudBuyMenu::SetActive(bool bActive)
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudBuyMenu::ShouldDraw()
{
    // This element is opened/closed by clientside events
    // that we listen for and set m_bVisible manually via showpanel(true)

    return cl_drawhud.GetBool() && CSGameRules() && !CSGameRules()->IsBuyTimeElapsed() && m_bVisible && CHudElement::ShouldDraw();
}

void RkHudBuyMenu::FireGameEvent(IGameEvent *event)
{
    const char *type = event->GetName();

    if( !V_strcmp( "buymenu_open", type ) )
    {
        ShowPanel( true, false );
    }
    else if( !V_strcmp( "buymenu_close", type ) )
    {
        ShowPanel( false, false );
    }
}
