//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Volumetric dust motes.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "func_dust_shared.h"
#include "te_particlesystem.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "reflect_annotations.h"
#include "reflect_sendtable.h"

#define DUST_LIFETIME_NETWORK_BITS 4

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class [[= ks::reflect::NetTable{ .name = "DT_Func_Dust", .base = false } ]]
      [[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } >{} ]]
      [[= ks::reflect::From<"m_Collision", ks::reflect::Net{}, nullptr, &REFERENCE_SEND_TABLE( DT_CollisionProperty )>{} ]]
      CFunc_Dust : public CBaseEntity
{
public:
	DECLARE_CLASS( CFunc_Dust, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CFunc_Dust();
	virtual 		~CFunc_Dust();


// CBaseEntity overrides.
public:

	virtual void	Spawn();
	virtual void	Activate();
	virtual void	Precache();
	virtual bool	KeyValue( const char *szKeyName, const char *szValue );


// Input handles.
public:
	
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );


// FGD properties.
public:

	CNetworkVar( color32, m_Color, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]]
	                              [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] [[= ks::reflect::Key{ .name = "Color" } ]] );
	CNetworkVar( int, m_SpawnRate, [[= ks::reflect::Net{ .bits = 12, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "SpawnRate" } ]] );
	
	CNetworkVar( float, m_flSizeMin, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "SizeMin" } ]] );
	CNetworkVar( float, m_flSizeMax, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "SizeMax" } ]] );

	CNetworkVar( int, m_SpeedMax, [[= ks::reflect::Net{ .bits = 12, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "SpeedMax" } ]] );

	CNetworkVar( int, m_LifetimeMin, [[= ks::reflect::Net{ .bits = DUST_LIFETIME_NETWORK_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "LifetimeMin" } ]] );
	CNetworkVar( int, m_LifetimeMax, [[= ks::reflect::Net{ .bits = DUST_LIFETIME_NETWORK_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "LifetimeMax" } ]] );

	CNetworkVar( int, m_DistMax, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "DistMax" } ]] );

	CNetworkVar( float, m_FallSpeed, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FallSpeed" } ]] );

	CNetworkVar( bool, m_bAffectedByWind, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "AffectedByWind" } ]] );

public:

	CNetworkVar( int, m_DustFlags, [[= ks::reflect::Net{ .bits = DUST_NUMFLAGS, .flags = SPROP_UNSIGNED } ]] );	// Combination of DUSTFLAGS_

private:	
	int			m_iAlpha;

};


class CFunc_DustMotes : public CFunc_Dust
{
	DECLARE_CLASS( CFunc_DustMotes, CFunc_Dust );
public:
					CFunc_DustMotes();
};


class CFunc_DustCloud : public CFunc_Dust
{
	DECLARE_CLASS( CFunc_DustCloud, CFunc_Dust );
public:
};

// Changing this will break demos. Peeling it out to clamp post creation to fix some shipped maps that specify out of range lifetimes. 
IMPLEMENT_REFLECT_SERVERCLASS( CFunc_Dust, DT_Func_Dust )


IMPLEMENT_REFLECT_DATAMAP( CFunc_Dust )

LINK_ENTITY_TO_CLASS( func_dustmotes, CFunc_DustMotes );
LINK_ENTITY_TO_CLASS( func_dustcloud, CFunc_DustCloud );


// ------------------------------------------------------------------------------------- //
// CFunc_DustMotes implementation.
// ------------------------------------------------------------------------------------- //

CFunc_DustMotes::CFunc_DustMotes()
{
	m_DustFlags |= DUSTFLAGS_SCALEMOTES;
}



// ------------------------------------------------------------------------------------- //
// CFunc_Dust implementation.
// ------------------------------------------------------------------------------------- //

CFunc_Dust::CFunc_Dust()
{
	m_DustFlags = DUSTFLAGS_ON;
	m_FallSpeed = 0.0f;
}


CFunc_Dust::~CFunc_Dust()
{
}


void CFunc_Dust::Spawn()
{
	Precache();

	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );
	//AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_VOLUME_CONTENTS );

	// Clamp to values in a networkable range... can't up the networked bits without breaking demos. 
	const int unMaxLifetimeVal = (1 << (DUST_LIFETIME_NETWORK_BITS) ) - 1;
	m_LifetimeMin = Clamp( m_LifetimeMin.Get(), 0, unMaxLifetimeVal );
	m_LifetimeMax = Clamp( m_LifetimeMax.Get(), 0, unMaxLifetimeVal );

	//Since keyvalues can arrive in any order, and UTIL_StringToColor32 stomps alpha,
	//install the alpha value here.
	color32 clr = { m_Color.m_Value.r, m_Color.m_Value.g, m_Color.m_Value.b, (byte)m_iAlpha };
	m_Color.Set( clr );

	BaseClass::Spawn();
}


void CFunc_Dust::Precache()
{
	PrecacheMaterial( "particle/sparkles" );
}

void CFunc_Dust::Activate()
{
	BaseClass::Activate();
}


bool CFunc_Dust::KeyValue( const char *szKeyName, const char *szValue )
{
	if( stricmp( szKeyName, "StartDisabled" ) == 0 )
	{
		if( szValue[0] == '1' )
			m_DustFlags &= ~DUSTFLAGS_ON;
		else
			m_DustFlags |= DUSTFLAGS_ON;
	
		return true;
	}
	else if( stricmp( szKeyName, "Alpha" ) == 0 )
	{
		m_iAlpha = atoi( szValue );
		return true;
	}
	else if( stricmp( szKeyName, "Frozen" ) == 0 )
	{
		if( szValue[0] == '1' )
			m_DustFlags |= DUSTFLAGS_FROZEN;
		else
			m_DustFlags &= ~DUSTFLAGS_FROZEN;
	
		return true;
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}
}


void CFunc_Dust::InputTurnOn( inputdata_t &inputdata )
{
	if( !(m_DustFlags & DUSTFLAGS_ON) )
	{
		m_DustFlags |= DUSTFLAGS_ON;
	}
}


void CFunc_Dust::InputTurnOff( inputdata_t &inputdata )
{
	if( m_DustFlags & DUSTFLAGS_ON )
	{
		m_DustFlags &= ~DUSTFLAGS_ON;
	}
}

//
// Dust
//

class [[= ks::reflect::NetTable{ .name = "DT_TEDust" } ]]
      CTEDust : public CTEParticleSystem
{
public:
	DECLARE_CLASS( CTEDust, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEDust( const char *name );
	virtual			~CTEDust( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles ) { };
	
	CNetworkVar( float, m_flSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkVar( float, m_flSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = 4, .low = -1.0f, .high = 1.0f, .enc = ks::reflect::ENC_VECTOR } ]] );
};

CTEDust::CTEDust( const char *name ) : BaseClass( name )
{
	m_flSize = 1.0f;
	m_flSpeed = 1.0f;
	m_vecDirection.Init();
}

CTEDust::~CTEDust( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEDust, DT_TEDust )

static CTEDust g_TEDust( "Dust" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&angles - 
//-----------------------------------------------------------------------------
void TE_Dust( IRecipientFilter& filter, float delay,
	const Vector &pos, const Vector &dir, float size, float speed )
{
	g_TEDust.m_vecOrigin	= pos;
	g_TEDust.m_vecDirection	= dir;
	g_TEDust.m_flSize		= size;
	g_TEDust.m_flSpeed		= speed;

	Assert( dir.Length() < 1.01 );	// make sure it's a normal

	//Send it
	g_TEDust.Create( filter, delay );
}

class CEnvDustPuff : public CPointEntity
{
	DECLARE_CLASS( CEnvDustPuff, CPointEntity );

public:
	
	DECLARE_DATADESC();

protected:

	// Input handlers
	[[= ks::reflect::Input{ .name = "SpawnDust", .type = FIELD_VOID } ]] void InputSpawnDust( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "scale" } ]] float		m_flScale;
	[[= ks::reflect::Key{ .name = "color" } ]] color32		m_rgbaColor;
};

LINK_ENTITY_TO_CLASS( env_dustpuff, CEnvDustPuff );

IMPLEMENT_REFLECT_DATAMAP( CEnvDustPuff )


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvDustPuff::InputSpawnDust( inputdata_t &inputdata )
{
	Vector dir;
	AngleVectors( GetAbsAngles(), &dir );

	VectorNormalize( dir );

	g_pEffects->Dust( GetAbsOrigin(), dir, m_flScale, m_flSpeed );
}
