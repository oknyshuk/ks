//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "cs_hud_weaponselection.h"
#include "iclientmode.h"
#include "iinput.h"
#include "cs_gamerules.h"
#include "cs_weapon_parse.h"
#include "c_cs_player.h"

#include <keyvalues.h>

#include "localize/ilocalize.h"

#include <string.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudWeaponSelection );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSelection::CHudWeaponSelection( const char *pElementName ) : CBaseHudWeaponSelection( pElementName )
{
	SetHiddenBits( HIDEHUD_WEAPONSELECTION | HIDEHUD_PLAYERDEAD );

	// Formerly CPanelAnimationVars; initialize to their old defaults now that this
	// element is no longer a panel. (CHudElement::operator new zeroes members,
	// so m_bPlaySelectionSounds MUST be set true here or weapon-switch sounds die.)
	m_iMaxSlots = 6;
	m_bPlaySelectionSounds = true;
}

//-----------------------------------------------------------------------------
// Purpose: sets up display for showing weapon pickup
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnWeaponPickup( C_BaseCombatWeapon *pWeapon )
{
	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( pWeapon );
	C_CSPlayer *pPlayer = GetHudPlayer();

	if ( pCSWeapon && !pCSWeapon->IsMarkedForDeletion() && pPlayer )
	{
		//const CCSWeaponInfo *pCSWeaponInfo = GetWeaponInfo( pCSWeapon->GetCSWeaponID() );
		if ( pPlayer->State_Get() == STATE_ACTIVE )
		{
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up display for showing weapon drop
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnWeaponDrop( C_BaseCombatWeapon *pWeapon )
{
	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( pWeapon );
	C_CSPlayer *pPlayer = GetHudPlayer();

	if ( pCSWeapon && pPlayer )
	{
		if ( pPlayer->State_Get() == STATE_ACTIVE )
		{
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up display for showing weapon pickup
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnWeaponSwitch( C_BaseCombatWeapon *pWeapon )
{
	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( pWeapon );
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pCSWeapon && pPlayer )
	{
		if ( pPlayer->State_Get() == STATE_ACTIVE )
		{
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the CHudMenu should take slot1, etc commands
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::IsHudMenuTakingInput()
{
	return CBaseHudWeaponSelection::IsHudMenuTakingInput();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the weapon selection hud should be hidden because
//          the CHudMenu is open
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::IsHudMenuPreventingWeaponSelection()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the panel should draw
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::ShouldDraw()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		if ( IsInSelectionMode() )
		{
			HideSelection();
		}
		return false;
	}

	bool bret = CBaseHudWeaponSelection::ShouldDraw();
	if ( !bret )
		return false;

	return ( m_bSelectionVisible ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::LevelInit()
{
	CHudElement::LevelInit();
	
	m_iMaxSlots = clamp( m_iMaxSlots, 0, MAX_WEAPON_SLOTS );
}

// NOTE: The render-only methods Paint(), DrawBox() and ApplySchemeSettings() were
// removed here when CHudWeaponSelection stopped deriving from a panel. On
// CSTRIKE15 the Paint() body was already compiled out (#if !defined( CSTRIKE15 )),
// so no visible behavior changed; all weapon-switch logic lives in the methods below.

//-----------------------------------------------------------------------------
// Purpose: Opens weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OpenSelection( void )
{
	Assert(!IsInSelectionMode());

	CBaseHudWeaponSelection::OpenSelection();
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("OpenWeaponSelectionMenu");
}

//-----------------------------------------------------------------------------
// Purpose: Closes weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::HideSelection( void )
{
	CBaseHudWeaponSelection::HideSelection();
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("CloseWeaponSelectionMenu");
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition, WEAPON_SELECTION_MODE selectionMode /*WEAPON_SELECTION_NORMAL*/)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestNextSlot = MAX_WEAPON_SLOTS;
	int iLowestNextPosition = MAX_WEAPON_POSITIONS;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if (selectionMode != WEAPON_SELECTION_NORMAL)
		{
			// skip over weapon types we arent including in this list
			CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >( pWeapon );
			if (!pCSWeapon)
				continue;
			bool isGrenade = pCSWeapon->IsKindOf( WEAPONTYPE_GRENADE );
			bool isBomb = pCSWeapon->IsKindOf( WEAPONTYPE_C4 );
			bool isStackable = pCSWeapon->IsKindOf( WEAPONTYPE_STACKABLEITEM );
			bool isMelee = pCSWeapon->IsKindOf( WEAPONTYPE_KNIFE );
			if ( ( selectionMode == WEAPON_SELECTION_GRENADE_AND_BOMB && !isGrenade && !isBomb ) ||
				 ( selectionMode == WEAPON_SELECTION_GRENADE_AND_BOMB_AND_MELEE && !isGrenade && !isBomb && !isMelee ) ||
				 ( selectionMode == WEAPON_SELECTION_NO_GRENADE_AND_BOMB && ( isGrenade || isBomb || isMelee ) ) ||	 
				 ( selectionMode == WEAPON_SELECTION_GRENADE && !isGrenade ) ||
				 ( selectionMode == WEAPON_SELECTION_MELEE && !isMelee ) ||
				 ( selectionMode == WEAPON_SELECTION_ITEMSLOT && !isBomb && !isStackable ) )
				continue;
		}

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot > iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition > iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot < iLowestNextSlot || (weaponSlot == iLowestNextSlot && weaponPosition < iLowestNextPosition) )
				{
					iLowestNextSlot = weaponSlot;
					iLowestNextPosition = weaponPosition;
					pNextWeapon = pWeapon;
				}
			}
		}
	}

	return pNextWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the prior available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pPrevWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestPrevSlot = -1;
	int iLowestPrevPosition = -1;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot < iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition < iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot > iLowestPrevSlot || (weaponSlot == iLowestPrevSlot && weaponPosition > iLowestPrevPosition) )
				{
					iLowestPrevSlot = weaponSlot;
					iLowestPrevPosition = weaponPosition;
					pPrevWeapon = pWeapon;
				}
			}
		}
	}

	return pPrevWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the next item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextWeapon( void )
{
	CycleToNextWeapon(WEAPON_SELECTION_NORMAL);
}

//-----------------------------------------------------------------------------
// Purpose: Select the next grenade or bomb
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextGrenadeOrBomb( void )
{
	CycleToNextWeapon(WEAPON_SELECTION_GRENADE_AND_BOMB);
}

//-----------------------------------------------------------------------------
// Purpose: Select the next grenade or bomb
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextGrenadeBombOrMelee( void )
{
	CycleToNextWeapon(WEAPON_SELECTION_GRENADE_AND_BOMB_AND_MELEE);
}

//-----------------------------------------------------------------------------
// Purpose: Select the next pistol/gun/knife
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextNonGrenadeOrBomb( void )
{
	CycleToNextWeapon(WEAPON_SELECTION_NO_GRENADE_AND_BOMB);
}

	
//-----------------------------------------------------------------------------
// Purpose: Select a weapon from nBucketID at the nSlotPos
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectSpecificWeapon( CSWeaponID weaponID )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
	{
		return;
	}

	// Note[pfreese] This could probably just be replaced by a call to CBasePlayer::SelectItem().
	// It's interesting, and perhaps a bit worrying, that SelectItem() does additional
	// tests that the code in this file does not, such as verifying that the current
	// weapon can be holstered.
	//
	// Players who bind keys to the use command, e.g. "bind 6 use weapon_hegrenade" will
	// get weapons selected via the CBasePlayer::SelectItem() code path, which means they will
	// see slightly different behavior than players who switch weapons using the redirected
	// slotN commands above (the default configured through the Options UI.

	// get the weapon by ID if the player owns it
	CWeaponCSBase* pWeapon = pPlayer->GetCSWeapon(weaponID);
	if ( pWeapon != NULL )
	{
		// Make sure the player's allowed to switch weapons
		if ( pPlayer->IsAllowedToSwitchWeapons() == false )
			return;

		// Mark the change
		SetSelectedWeapon( pWeapon );
		SelectWeapon();

		if( m_bPlaySelectionSounds )
			pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

void CHudWeaponSelection::SetSelectedWeapon( C_BaseCombatWeapon *pWeapon ) 
{ 
	m_hSelectedWeapon = pWeapon;
}

// Switch to knife
void CHudWeaponSelection::UserCmd_Slot3( void )
{
	//SelectSpecificWeapon( WEAPON_KNIFE );
	CycleToNextWeapon( WEAPON_SELECTION_MELEE );
}

// Switch to breach charges
void CHudWeaponSelection::UserCmd_Slot5( void )
{
	CycleToNextWeapon( WEAPON_SELECTION_ITEMSLOT );
}

// Switch to hegrenade
void CHudWeaponSelection::UserCmd_Slot6( void )
{
	SelectSpecificWeapon( WEAPON_HEGRENADE );
}

// Switch to flashbang
void CHudWeaponSelection::UserCmd_Slot7( void )
{
	SelectSpecificWeapon( WEAPON_FLASHBANG );
}

// Switch to smoke grenade
void CHudWeaponSelection::UserCmd_Slot8( void )
{
	SelectSpecificWeapon( WEAPON_SMOKEGRENADE );
}

// Switch to decoy
void CHudWeaponSelection::UserCmd_Slot9( void )
{
	SelectSpecificWeapon( WEAPON_DECOY );
}


// Switch to molotov
void CHudWeaponSelection::UserCmd_Slot10( void )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return;

	if ( pPlayer->HasWeaponOfType( WEAPON_MOLOTOV ) )
		SelectSpecificWeapon( WEAPON_MOLOTOV );
	else
		SelectSpecificWeapon( WEAPON_INCGRENADE );
}

// Switch to taser
void CHudWeaponSelection::UserCmd_Slot11( void )
{
	SelectSpecificWeapon( WEAPON_TASER );
}


// Cycle grenades
void CHudWeaponSelection::UserCmd_Slot4( void )
{
	CycleToNextWeapon( WEAPON_SELECTION_GRENADE );
}

void CHudWeaponSelection::UserCmd_GamePadSlot1( void )
{
	SelectSpecificWeapon( WEAPON_TASER );
}

void CHudWeaponSelection::UserCmd_GamePadSlot2( void )
{
	SelectSpecificWeapon( WEAPON_HEGRENADE );
}

void CHudWeaponSelection::UserCmd_GamePadSlot3( void )
{
	SelectSpecificWeapon( WEAPON_FLASHBANG );
}

void CHudWeaponSelection::UserCmd_GamePadSlot4( void )
{
	SelectSpecificWeapon( WEAPON_SMOKEGRENADE );
}

void CHudWeaponSelection::UserCmd_GamePadSlot5( void )
{
	SelectSpecificWeapon( WEAPON_DECOY );
}

void CHudWeaponSelection::UserCmd_GamePadSlot6( void )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return;

	if ( pPlayer->HasWeaponOfType( WEAPON_MOLOTOV ) )
		SelectSpecificWeapon( WEAPON_MOLOTOV );
	else
		SelectSpecificWeapon( WEAPON_INCGRENADE );
}


//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the next item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextWeapon(WEAPON_SELECTION_MODE selectionMode)
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

		pNextWeapon = FindNextWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition(), selectionMode );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindNextWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition(), selectionMode );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to start
		pNextWeapon = FindNextWeaponInWeaponSelection(-1, -1, selectionMode);
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );

#if defined ( CSTRIKE15 )
		SelectWeapon();
#else
		if( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}
#endif

		// Play the "cycle to next weapon" sound
		if( m_bPlaySelectionSounds )
			pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the previous item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToPrevWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->IsPlayerDead() )
		return;

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

		pNextWeapon = FindPrevWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindPrevWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to end of weapon list
		pNextWeapon = FindPrevWeaponInWeaponSelection(MAX_WEAPON_SLOTS, MAX_WEAPON_POSITIONS);
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );

#if defined ( CSTRIKE15 )
		SelectWeapon();
#else
		if( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}
#endif

		// Play the "cycle to next weapon" sound
		if( m_bPlaySelectionSounds )
			pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Switches the last weapon the player was using
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SwitchToLastWeapon( void )
{
	// Get the player's last weapon
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	if ( player->IsPlayerDead() )
		return;

	C_BaseCombatWeapon *lastWeapon = player->GetLastWeapon();
	C_BaseCombatWeapon *activeWeapon = player->GetActiveWeapon();

	if ( lastWeapon == activeWeapon )
		lastWeapon = NULL;

	// make sure our last weapon is still with us and valid (has ammo etc)
	if ( lastWeapon )
	{
		int i;
		for ( i = 0; i < MAX_WEAPONS; i++ )
		{
			C_BaseCombatWeapon *weapon = player->GetWeapon(i);
			
			if ( !weapon  || !weapon->CanBeSelected() )
				continue;

			if (weapon == lastWeapon )
				break;
		}

		if ( i == MAX_WEAPONS )
			lastWeapon = NULL; // weapon not found/valid
	}

	// if we don't have a 'last weapon' choose best weapon
	if ( !lastWeapon )
	{
		lastWeapon = GameRules()->GetNextBestWeapon( player, activeWeapon );
	}

	::input->MakeWeaponSelection( lastWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlot( int iSlot, int iSlotPos )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return NULL;

	if ( player->IsPlayerDead() )
		return NULL;

	for ( int i = 0; i < player->WeaponCount(); i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);
		
		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlotForTarget( C_BasePlayer *player, int iSlot, int iSlotPos )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && pLocalPlayer == player )
		return GetWeaponInSlot( iSlot, iSlotPos );

	if ( !player )
		return NULL;

	if ( player->IsPlayerDead() )
		return NULL;

	for ( int i = 0; i < player->WeaponCount(); i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);

		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Player has chosen to draw the currently selected weapon
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeapon( void )
{
	if ( !GetSelectedWeapon() )
	{
		engine->ClientCmd( "cancelselect\n" );
		return;
	}

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	C_BaseCombatWeapon *activeWeapon = player->GetActiveWeapon();

	// Don't allow selections of weapons that can't be selected (out of ammo, etc)
	if ( !GetSelectedWeapon()->CanBeSelected() )
	{
		player->EmitSound( "Player.DenyWeaponSelection" );
	}
	else
	{
		// Only play the "weapon selected" sound if they are selecting
		// a weapon different than the one that is already active.
		if (GetSelectedWeapon() != activeWeapon)
		{
			if (player->GetTeamNumber() == TEAM_CT)
			{
				// Play the "weapon selected" sound
				player->EmitSound("Player.WeaponSelected_CT");

			}
			else
			{
				// Play the "weapon selected" sound
				player->EmitSound("Player.WeaponSelected_T");
			}
		}

		SetWeaponSelected();
	
		m_hSelectedWeapon = NULL;
	
		engine->ClientCmd( "cancelselect\n" );

	}
}

//-----------------------------------------------------------------------------
// Purpose: Abort selecting a weapon
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CancelWeaponSelection()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	// Fastswitches happen in a single frame, so the Weapon Selection HUD Element isn't visible
	// yet, but it's going to be next frame. We need to ask it if it thinks it's going to draw,
	// instead of checking it's IsActive flag.
	if ( ShouldDraw() )
	{
		HideSelection();

		m_hSelectedWeapon = NULL;
	}
	else
	{
		// [pmf] The escape command causes the UI to be closed, which in turn causes an "unpause" command, which is blocked on multi-player games, 
		// causing a warning. It seems like this escape command is only necessary when fast-switching is disabled. According to one of our designers,
		// we don't need non-fast-switching, so I'm removing this command injection to prevent the subsequent warning.
		// 	engine->ClientCmd("escape");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves selection to the specified slot
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeaponSlot( int iSlot )
{
	// iSlot is one higher than it should be, since it's the number key, not the 0-based index into the weapons
	--iSlot;

	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Don't try and read past our possible number of slots
	if ( iSlot > MAX_WEAPON_SLOTS )
		return;
	
	// Make sure the player's allowed to switch weapons
	if ( pPlayer->IsAllowedToSwitchWeapons() == false )
		return;

	int slotPos = 0;
	C_BaseCombatWeapon *pActiveWeapon = GetSelectedWeapon();

	// start later in the list
	if ( IsInSelectionMode() && pActiveWeapon && pActiveWeapon->GetSlot() == iSlot )
	{
		slotPos = pActiveWeapon->GetPosition() + 1;
	}

	// find the weapon in this slot
	pActiveWeapon = GetNextActivePos( iSlot, slotPos );
	if ( !pActiveWeapon )
	{
		pActiveWeapon = GetNextActivePos( iSlot, 0 );
	}
	
	if ( pActiveWeapon != NULL )
	{
		// Mark the change
		SetSelectedWeapon( pActiveWeapon );

		bool bMultipleWeaponsInSlot = false;

		for( int i=0;i<MAX_WEAPON_POSITIONS;i++ )
		{
			C_BaseCombatWeapon *pSlotWpn = GetWeaponInSlot( pActiveWeapon->GetSlot(), i );

			if( pSlotWpn != NULL && pSlotWpn != pActiveWeapon )
			{
				bMultipleWeaponsInSlot = true;
				break;
			}
		}

#if defined ( CSTRIKE15 )
		// only select if only one item in the bucket
		if( bMultipleWeaponsInSlot == false )
		{
			// only one active item in bucket, so change directly to weapon
			SelectWeapon();
		}
#else

		// if fast weapon switch is on, then weapons can be selected in a single keypress
		// but only if there is only one item in the bucket
		if( hud_fastswitch.GetInt() > 0 && bMultipleWeaponsInSlot == false )
		{
			// only one active item in bucket, so change directly to weapon
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			// open the weapon selection
			OpenSelection();
		}
#endif
	}

	if( m_bPlaySelectionSounds )
			pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
}
