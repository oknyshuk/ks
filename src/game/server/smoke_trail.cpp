//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "smoke_trail.h"
#include "dt_send.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SMOKETRAIL_ENTITYNAME		"env_smoketrail"
#define SPORETRAIL_ENTITYNAME		"env_sporetrail"
#define SPOREEXPLOSION_ENTITYNAME	"env_sporeexplosion"
#define DUSTTRAIL_ENTITYNAME		"env_dusttrail"

//-----------------------------------------------------------------------------
//Data table
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( SmokeTrail, DT_SmokeTrail )

LINK_ENTITY_TO_CLASS(env_smoketrail, SmokeTrail);

IMPLEMENT_REFLECT_DATAMAP( SmokeTrail )


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
SmokeTrail::SmokeTrail()
{
	m_SpawnRate = 10;
	m_StartColor.GetForModify().Init(0.5, 0.5, 0.5);
	m_EndColor.GetForModify().Init(0,0,0);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_bEmit = true;
	m_nAttachment	= 0;
	m_Opacity = 0.5f;
}


//-----------------------------------------------------------------------------
// Parse data from a map file
//-----------------------------------------------------------------------------
bool SmokeTrail::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "startcolor" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_StartColor.GetForModify().Init( tmp.r / 255.0f, tmp.g / 255.0f, tmp.b / 255.0f );
		return true;
	}

	if ( FStrEq( szKeyName, "endcolor" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_EndColor.GetForModify().Init( tmp.r / 255.0f, tmp.g / 255.0f, tmp.b / 255.0f );
		return true;
	}

	if ( FStrEq( szKeyName, "emittime" ) )
	{
		m_StopEmitTime = gpGlobals->curtime + atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//-----------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//-----------------------------------------------------------------------------
void SmokeTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : SmokeTrail*
//-----------------------------------------------------------------------------
SmokeTrail* SmokeTrail::CreateSmokeTrail()
{
	CBaseEntity *pEnt = CreateEntityByName(SMOKETRAIL_ENTITYNAME);
	if(pEnt)
	{
		SmokeTrail *pSmoke = dynamic_cast<SmokeTrail*>(pEnt);
		if(pSmoke)
		{
			pSmoke->Activate();
			return pSmoke;
		}
		else
		{
			UTIL_Remove(pEnt);
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void SmokeTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}


//==================================================
// RocketTrail
//==================================================

//Data table
IMPLEMENT_REFLECT_SERVERCLASS( RocketTrail, DT_RocketTrail )

LINK_ENTITY_TO_CLASS( env_rockettrail, RocketTrail );


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
RocketTrail::RocketTrail()
{
	m_SpawnRate = 10;
	m_StartColor.GetForModify().Init(0.5, 0.5, 0.5);
	m_EndColor.GetForModify().Init(0,0,0);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_bEmit = true;
	m_nAttachment	= 0;
	m_Opacity = 0.5f;
	m_flFlareScale = 1.5;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void RocketTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SmokeTrail*
//-----------------------------------------------------------------------------
RocketTrail* RocketTrail::CreateRocketTrail()
{
	CBaseEntity *pEnt = CreateEntityByName( "env_rockettrail" );
	
	if( pEnt != nullptr )
	{
		RocketTrail *pTrail = dynamic_cast<RocketTrail*>(pEnt);
		
		if( pTrail != nullptr )
		{
			pTrail->Activate();
			return pTrail;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void RocketTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}

//==================================================
// SporeTrail
//==================================================

IMPLEMENT_REFLECT_SERVERCLASS( SporeTrail, DT_SporeTrail )

LINK_ENTITY_TO_CLASS(env_sporetrail, SporeTrail);


SporeTrail::SporeTrail( void )
{
	m_vecEndColor.GetForModify().Init();

	m_flSpawnRate			= 100.0f;
	m_flParticleLifetime	= 1.0f;
	m_flStartSize			= 1.0f;
	m_flEndSize				= 0.0f;
	m_flSpawnRadius			= 16.0f;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SporeTrail*
//-----------------------------------------------------------------------------
SporeTrail* SporeTrail::CreateSporeTrail()
{
	CBaseEntity *pEnt = CreateEntityByName( SPORETRAIL_ENTITYNAME );
	
	if(pEnt)
	{
		SporeTrail *pSpore = dynamic_cast<SporeTrail*>(pEnt);
		
		if ( pSpore )
		{
			pSpore->Activate();
			return pSpore;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;
}

//==================================================
// SporeExplosion
//==================================================

IMPLEMENT_REFLECT_SERVERCLASS( SporeExplosion, DT_SporeExplosion )

LINK_ENTITY_TO_CLASS( env_sporeexplosion, SporeExplosion );

IMPLEMENT_REFLECT_DATAMAP( SporeExplosion )

SporeExplosion::SporeExplosion( void )
{
	m_flSpawnRate			= 100.0f;
	m_flParticleLifetime	= 1.0f;
	m_flStartSize			= 1.0f;
	m_flEndSize				= 0.0f;
	m_flSpawnRadius			= 16.0f;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	m_bEmit = true;
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SporeExplosion::Spawn( void )
{
	BaseClass::Spawn();

	m_bEmit = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SporeExplosion*
//-----------------------------------------------------------------------------
SporeExplosion *SporeExplosion::CreateSporeExplosion()
{
	CBaseEntity *pEnt = CreateEntityByName( SPOREEXPLOSION_ENTITYNAME );
	
	if ( pEnt )
	{
		SporeExplosion *pSpore = dynamic_cast<SporeExplosion*>(pEnt);
		
		if ( pSpore )
		{
			pSpore->Activate();
			return pSpore;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;
}

void SporeExplosion::InputEnable( inputdata_t &inputdata )
{
	m_bDontRemove = true;
	m_bDisabled = false;
	m_bEmit = true;
}

void SporeExplosion::InputDisable( inputdata_t &inputdata )
{
	m_bDontRemove = true;
	m_bDisabled = true;
	m_bEmit = false;
}


IMPLEMENT_REFLECT_SERVERCLASS( CFireTrail, DT_FireTrail )

LINK_ENTITY_TO_CLASS( env_fire_trail, CFireTrail );

void CFireTrail::Precache( void )
{
	PrecacheMaterial( "sprites/flamelet1" );
	PrecacheMaterial( "sprites/flamelet2" );
	PrecacheMaterial( "sprites/flamelet3" );
	PrecacheMaterial( "sprites/flamelet4" );
	PrecacheMaterial( "sprites/flamelet5" );
	PrecacheMaterial( "particle/particle_smokegrenade" );
	PrecacheMaterial( "particle/particle_noisesphere" );
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void CFireTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Create and return a new fire trail entity
//-----------------------------------------------------------------------------
CFireTrail *CFireTrail::CreateFireTrail( void )
{
	CBaseEntity *pEnt = CreateEntityByName( "env_fire_trail" );
	
	if ( pEnt )
	{
		CFireTrail *pTrail = dynamic_cast<CFireTrail*>(pEnt);
		
		if ( pTrail )
		{
			pTrail->Activate();
			return pTrail;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;	
}


//-----------------------------------------------------------------------------
//Data table
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( DustTrail, DT_DustTrail )

LINK_ENTITY_TO_CLASS( env_dusttrail, DustTrail);

IMPLEMENT_REFLECT_DATAMAP( DustTrail )


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
DustTrail::DustTrail()
{
	m_SpawnRate = 10;
	m_Color.GetForModify().Init(0.5, 0.5, 0.5);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_bEmit = true;
	m_Opacity = 0.5f;
}


//-----------------------------------------------------------------------------
// Parse data from a map file
//-----------------------------------------------------------------------------
bool DustTrail::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_Color.GetForModify().Init( tmp.r / 255.0f, tmp.g / 255.0f, tmp.b / 255.0f );
		return true;
	}

	if ( FStrEq( szKeyName, "emittime" ) )
	{
		m_StopEmitTime = gpGlobals->curtime + atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//-----------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//-----------------------------------------------------------------------------
void DustTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : DustTrail*
//-----------------------------------------------------------------------------
DustTrail* DustTrail::CreateDustTrail()
{
	CBaseEntity *pEnt = CreateEntityByName(DUSTTRAIL_ENTITYNAME);
	if(pEnt)
	{
		DustTrail *pDust = dynamic_cast<DustTrail*>(pEnt);
		if(pDust)
		{
			pDust->Activate();
			return pDust;
		}
		else
		{
			UTIL_Remove(pEnt);
		}
	}

	return nullptr;
}
