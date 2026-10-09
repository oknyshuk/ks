#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

struct WeaponProficiencyInfo_t
{
	float	spreadscale;
	float	bias;
};

enum WeaponProficiency_t
{
	WEAPON_PROFICIENCY_POOR = 0,
	WEAPON_PROFICIENCY_AVERAGE,
	WEAPON_PROFICIENCY_GOOD,
	WEAPON_PROFICIENCY_VERY_GOOD,
	WEAPON_PROFICIENCY_PERFECT,
};

const char *GetWeaponProficiencyName( WeaponProficiency_t proficiency );
