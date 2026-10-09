#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "shareddefs.h"
import std;
#include "hud.h"
#include "weapon_parse.h"

class C_BaseCombatWeapon;
class CHudTexture;
typedef unsigned short WEAPON_FILE_INFO_HANDLE;

//-----------------------------------------------------------------------------
// Purpose: Stores data about the Weapon Definitions passed to the client when
//			the client first connects to a server. 
//-----------------------------------------------------------------------------
class WeaponsResource
{
public:
	WeaponsResource( void );
	~WeaponsResource( void );

	void Init( void );
	void Reset( void );

	// Sprite handling
	void LoadWeaponSprites( WEAPON_FILE_INFO_HANDLE hWeaponFileInfo );
	void LoadAllWeaponSprites( void );

	// Ammo Handling
	CHudTexture					*GetAmmoIconFromWeapon( int iAmmoId );
	const FileWeaponInfo_t		*GetWeaponFromAmmo( int iAmmoId );
};

extern WeaponsResource gWR;
