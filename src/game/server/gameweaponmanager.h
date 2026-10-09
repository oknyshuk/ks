#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

#include "reflect_annotations.h"

void CreateWeaponManager( const char *pWeaponName, int iMaxPieces );

class CBaseCombatWeapon;

void WeaponManager_AmmoMod( CBaseCombatWeapon *pWeapon );

void WeaponManager_AddManaged( CBaseEntity *pWeapon );
void WeaponManager_RemoveManaged( CBaseEntity *pWeapon );
