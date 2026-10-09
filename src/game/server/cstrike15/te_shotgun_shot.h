#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

void TE_FireBullets( 
	int	iPlayerIndex,
	uint16 nItemDefIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	int iSoundType,
	float flRecoilIndex
	);

void TE_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option );
