#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A gib is a chunk of a body, or a piece of wood/metal/rocks/etc.

#include "baseanimating.h"
#include "player_pickup.h"
#include "Sprite.h"

extern CBaseEntity *CreateRagGib( const char *szModel, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecForce, float flFadeTime = 0.0, bool bShouldIgnite = false );

inline constexpr auto GERMAN_GIB_COUNT = 4;
inline constexpr auto HUMAN_GIB_COUNT = 6;
inline constexpr auto ALIEN_GIB_COUNT = 4;

enum GibType_e
{
	GIB_HUMAN,
	GIB_ALIEN,
};

class CGib : public CBaseAnimating,
			 public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CGib, CBaseAnimating );

	virtual ~CGib( void );

	void Spawn( const char *szGibModel, float flLifetime = 25.0f );

	void InitGib( CBaseEntity *pVictim, float fMaxVelocity, float fMinVelocity );
	void BounceGibTouch ( CBaseEntity *pOther );
	void StickyGibTouch ( CBaseEntity *pOther );
	void WaitTillLand( void );
	void DieThink( void );
	void LimitVelocity( void );
	bool SUB_AllowedToFade_Impl( void );

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int	ObjectCaps_Impl( void ) { return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE | FCAP_IMPULSE_USE; }
	static	void SpawnHeadGib( CBaseEntity *pVictim );
	static	void SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, GibType_e eGibType );
	static  void SpawnStickyGibs( CBaseEntity *pVictim, Vector vecOrigin, int cGibs );
	static	void SpawnSpecificGibs( CBaseEntity *pVictim, int nNumGibs, float fMaxVelocity, float fMinVelocity, const char* cModelName, float flLifetime = 25);

	void SetPhysicsAttacker( CBasePlayer *pEntity, float flTime );
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	CBasePlayer *HasPhysicsAttacker_Impl( float dt );

	void SetSprite( CBaseEntity *pSprite )
	{
		m_hSprite = pSprite;	
	}

	CBaseEntity *GetSprite( void )
	{
		return m_hSprite.Get();
	}

	void SetFlame( CBaseEntity *pFlame )
	{
		m_hFlame = pFlame;	
	}

	CBaseEntity *GetFlame( void )
	{
		return m_hFlame.Get();
	}



public:
	void SetBloodColor( int nBloodColor );

	int		m_cBloodDecals;
	int		m_material;
	float	m_lifeTime;
	bool	m_bForceRemove;

	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

private:
	// A little piece of duplicated code
	void AdjustVelocityBasedOnHealth( int nHealth, Vector &vecVelocity );
	int		m_bloodColor;

	EHANDLE m_hSprite;
	EHANDLE m_hFlame;

	// Limit the max number of extant Gibs (mainly a memory issue):
	static const int MAX_CONCURRENT_GIBS = 75;
	static std::vector< EHANDLE > s_ExtantGibs;
};

class CRagGib : public CBaseAnimating
{
public:
	DECLARE_CLASS( CRagGib, CBaseAnimating );

	void Spawn( const char *szModel, const Vector &vecOrigin, const Vector &vecForce, float flFadeTime );
};
