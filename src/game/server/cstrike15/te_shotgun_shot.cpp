//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"
#include "fx_cs_shared.h"


#define NUM_BULLET_SEED_BITS 8


//-----------------------------------------------------------------------------
// Purpose: Display's a blood sprite
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEFireBullets", .base = false } ]]
      CTEFireBullets : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEFireBullets, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEFireBullets( const char *name );
	virtual			~CTEFireBullets( void );

public:
	CNetworkVar( int, m_iPlayer, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint16, m_nItemDefIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkQAngle( m_vecAngles, [[= ks::reflect::Net{ .bits = 13, .index = 1 } ]]  [[= ks::reflect::Net{ .bits = 13, .index = 0 } ]] );
	CNetworkVar( int, m_iWeaponID, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_iMode, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_iSeed, [[= ks::reflect::Net{ .bits = NUM_BULLET_SEED_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fInaccuracy, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 1 } ]] );
	CNetworkVar( float, m_flRecoilIndex, [[= ks::reflect::Net{ .bits = 10, .low = 0, .high = 1000 } ]] );
	CNetworkVar( float, m_fSpread, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 0.1f } ]] );
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	CNetworkVar( float, m_fAccuracyFishtail );
#endif
	CNetworkVar( int, m_iSoundType, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEFireBullets::CTEFireBullets( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEFireBullets::~CTEFireBullets( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEFireBullets, DT_TEFireBullets )


// Singleton
static CTEFireBullets g_TEFireBullets( "Shotgun Shot" );


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
	)
{
	// Just always send gunshots to clients.
	CBroadcastRecipientFilter filter;
	filter.UsePredictionRules();

	g_TEFireBullets.m_iPlayer = iPlayerIndex-1;
	g_TEFireBullets.m_nItemDefIndex = nItemDefIndex;
	g_TEFireBullets.m_vecOrigin = vOrigin;
	g_TEFireBullets.m_vecAngles = vAngles;
	g_TEFireBullets.m_iSeed = iSeed;
	g_TEFireBullets.m_fInaccuracy = fInaccuracy;
	g_TEFireBullets.m_flRecoilIndex = flRecoilIndex;
	g_TEFireBullets.m_fSpread = fSpread;
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	g_TEFireBullets.m_fAccuracyFishtail = fAccuracyFishtail;
#endif
	g_TEFireBullets.m_iMode = iMode;
	g_TEFireBullets.m_iWeaponID = iWeaponID;
	g_TEFireBullets.m_iSoundType = iSoundType;

	Assert( iSeed < (1 << NUM_BULLET_SEED_BITS) );
	
	g_TEFireBullets.Create( filter, 0 );
}




//-----------------------------------------------------------------------------
// Purpose: Displays a bomb plant animation
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEPlantBomb", .base = false } ]]
      CTEPlantBomb : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlantBomb, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEPlantBomb( const char *name );
	virtual			~CTEPlantBomb( void );

public:
	CNetworkVar( int, m_iPlayer, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( PlantBombOption_t, m_option, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPlantBomb::CTEPlantBomb( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPlantBomb::~CTEPlantBomb( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEPlantBomb, DT_TEPlantBomb )


// Singleton
static CTEPlantBomb g_TEPlantBomb( "Bomb Plant" );


void TE_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option )
{
	CPASFilter filter( vOrigin );
	filter.UsePredictionRules();

	g_TEPlantBomb.m_iPlayer = iPlayerIndex-1;
	g_TEPlantBomb.m_option = option;
	g_TEPlantBomb.Create( filter, 0 );
}
