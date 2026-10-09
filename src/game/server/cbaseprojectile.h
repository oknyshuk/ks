#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

#include "baseanimating.h"
#include "const.h"

// Purpose: Base class for simple projectiles

enum MoveType_t;
enum MoveCollide_t;

//=============================================================================
//=============================================================================
class CBaseProjectile : public CBaseAnimating
{
	DECLARE_CLASS( CBaseProjectile, CBaseAnimating );

public:
	void Touch( CBaseEntity *pOther );
	virtual void HandleTouch( CBaseEntity *pOther );

	void Think();
	virtual void HandleThink();

	void Spawn(	char *pszModel,
		const Vector &vecOrigin,
		const Vector &vecVelocity,
		edict_t *pOwner,
		MoveType_t	iMovetype,
		MoveCollide_t nMoveCollide,
		int	iDamage,
		int iDamageType,
		CBaseEntity *pIntendedTarget = nullptr );

	virtual void Precache( void ) {};

	int	m_iDmg;
	int m_iDmgType;
	EHANDLE m_hIntendedTarget;
};
