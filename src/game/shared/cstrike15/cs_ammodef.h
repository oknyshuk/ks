#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Holds defintion for game ammo types

#include "ammodef.h"


struct CSAmmoCost
{
	int	buySize;
	int	cost;
};

//=============================================================================
//	>> CCSAmmoDef
//=============================================================================
class CCSAmmoDef : public CAmmoDef
{

public:

	void AddAmmoCost( char const* name, int cost, int buySize );

	CCSAmmoDef(void);
	~CCSAmmoDef( void );

	int GetBuySize( int nAmmoIndex ) const;
	int GetCost( int nAmmoIndex ) const;

private:
	CSAmmoCost	m_csAmmo[MAX_AMMO_TYPES];
};


// Get the global ammodef object. This is usually implemented in each mod's game rules file somewhere,
// so the mod can setup custom ammo types.
CCSAmmoDef* GetCSAmmoDef();
