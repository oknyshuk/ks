// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "weapon_proficiency.h"

static const char *g_ProficiencyNames[] = 
{
	"Poor",
	"Average",
	"Good",
	"Very Good",
	"Perfect"
};

const char *GetWeaponProficiencyName( WeaponProficiency_t proficiency )
{
	static_assert( std::size( g_ProficiencyNames ) == WEAPON_PROFICIENCY_PERFECT + 1 ); // Hey, update that there table!

	if ( proficiency < 0 || proficiency > WEAPON_PROFICIENCY_PERFECT )
		return "<<Invalid>>";
	return g_ProficiencyNames[proficiency];
}
