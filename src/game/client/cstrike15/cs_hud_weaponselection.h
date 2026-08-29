//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_HUD_WEAPONSELECTION_H
#define CS_HUD_WEAPONSELECTION_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_selection.h"
#include "cs_weapon_parse.h"

enum
{
	WEPSELECT_PICKUP = 0,
	WEPSELECT_DROP,
	WEPSELECT_SWITCH,
};

//-----------------------------------------------------------------------------
// Purpose: cstrike weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection
{
	DECLARE_CLASS( CHudWeaponSelection, CBaseHudWeaponSelection );

public:
	explicit CHudWeaponSelection(const char *pElementName );

	virtual bool ShouldDraw();
	virtual void OnWeaponPickup( C_BaseCombatWeapon *pWeapon );
	virtual void OnWeaponDrop( C_BaseCombatWeapon *pWeapon );
	virtual void OnWeaponSwitch( C_BaseCombatWeapon *pWeapon );

	virtual void CycleToNextWeapon( void );
	virtual void CycleToPrevWeapon( void );
	virtual void SwitchToLastWeapon( void );

	typedef enum
	{
		WEAPON_SELECTION_NORMAL = 0,
		WEAPON_SELECTION_MELEE,
		WEAPON_SELECTION_GRENADE_AND_BOMB,
		WEAPON_SELECTION_GRENADE_AND_BOMB_AND_MELEE,
		WEAPON_SELECTION_NO_GRENADE_AND_BOMB,
		WEAPON_SELECTION_GRENADE,
		WEAPON_SELECTION_ITEMSLOT,
	} WEAPON_SELECTION_MODE;

	void CycleToNextWeapon(WEAPON_SELECTION_MODE selectionMode);
	virtual void CycleToNextGrenadeOrBomb( void );
	virtual void CycleToNextGrenadeBombOrMelee( void );
	virtual void CycleToNextNonGrenadeOrBomb( void );
	virtual void UserCmd_Slot3( void );
	virtual void UserCmd_Slot4( void );
	virtual void UserCmd_Slot5( void );
	virtual void UserCmd_Slot6( void );
	virtual void UserCmd_Slot7( void );
	virtual void UserCmd_Slot8( void );
	virtual void UserCmd_Slot9( void );
	virtual void UserCmd_Slot10( void );
	virtual void UserCmd_Slot11( void );
	virtual void UserCmd_GamePadSlot1( void );
	virtual void UserCmd_GamePadSlot2( void );
	virtual void UserCmd_GamePadSlot3( void );
	virtual void UserCmd_GamePadSlot4( void );
	virtual void UserCmd_GamePadSlot5( void );
	virtual void UserCmd_GamePadSlot6( void );

	virtual C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	virtual C_BaseCombatWeapon *GetWeaponInSlotForTarget( C_BasePlayer *player, int iSlot, int iSlotPos );
	virtual void SelectWeaponSlot( int iSlot );
	virtual void SelectWeapon( void );

	virtual C_BaseCombatWeapon	*GetSelectedWeapon( void )
	{ 
		return static_cast< C_BaseCombatWeapon* >( m_hSelectedWeapon.Get() );
	}

	virtual void OpenSelection( void );
	virtual void HideSelection( void );

	virtual void CancelWeaponSelection( void );

	virtual void LevelInit();

	C_BaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition, WEAPON_SELECTION_MODE selectionMode=WEAPON_SELECTION_NORMAL);
	//C_BaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	C_BaseCombatWeapon *FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);

protected:
	virtual bool IsWeaponSelectable()
	{ 
		if (IsInSelectionMode())
			return true;

		return false;
	}

	virtual bool IsHudMenuTakingInput();
	virtual bool IsHudMenuPreventingWeaponSelection();

private:
	void SelectSpecificWeapon( CSWeaponID weaponID );

	virtual	void SetSelectedWeapon( C_BaseCombatWeapon *pWeapon );

	// Weapon-switch behavior settings. Defaults match the former panel-animation
	// defaults ("6" / "1").
	int  m_iMaxSlots;
	bool m_bPlaySelectionSounds;
};

#endif	//CS_HUD_CHAT_H
