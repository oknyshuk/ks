#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "baseentity.h"

//-----------------------------------------------------------------------------
// Set this spawnflag before calling Spawn to get electrical effects
//-----------------------------------------------------------------------------
inline constexpr auto SF_RAGDOLL_BOOGIE_ELECTRICAL = 0x10000;
inline constexpr auto SF_RAGDOLL_BOOGIE_ELECTRICAL_NARROW_BEAM = 0x20000;


//-----------------------------------------------------------------------------
// Makes ragdolls DANCE!
//-----------------------------------------------------------------------------
class CRagdollBoogie : public CBaseEntity 
{
	DECLARE_CLASS( CRagdollBoogie, CBaseEntity );

public:
	static CRagdollBoogie	*Create( CBaseEntity *pTarget, float flMagnitude, float flStartTime, float flLengthTime = 0.0f, int nSpawnFlags = 0 );
	static void IncrementSuppressionCount( CBaseEntity *pTarget );
	static void DecrementSuppressionCount( CBaseEntity *pTarget );

	virtual void Precache();
	void Spawn();

private:
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetBoogieTime( float flStartTime, float flLengthTime );
	void	SetMagnitude( float flMagnitude );
	void	BoogieThink( void );
	void	ZapThink();
	
	float m_flStartTime;
	float m_flBoogieLength;
	float m_flMagnitude;
	int	m_nSuppressionCount;
};
