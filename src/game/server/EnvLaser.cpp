// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A special kind of beam effect that traces from its start position to
//			its end position and stops if it hits anything.

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "EnvLaser.h"
#include "Sprite.h"

LINK_ENTITY_TO_CLASS( env_laser, CEnvLaser );

IMPLEMENT_REFLECT_DATAMAP( CEnvLaser )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::Spawn_Impl( void )
{
	if ( !GetModelName() )
	{
		SetThink( &CEnvLaser::SUB_Remove );
		return;
	}

	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetThink( &CEnvLaser::StrikeThink );

	SetEndWidth( GetWidth() );				// Note: EndWidth is not scaled

	PointsInit( GetLocalOrigin(), GetLocalOrigin() );

	Precache( );

	if ( !m_pSprite && m_iszSpriteName != NULL_STRING )
	{
		m_pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteName), GetAbsOrigin(), true );
	}
	else
	{
		m_pSprite = nullptr;
	}

	if ( m_pSprite )
	{
		m_pSprite->SetParent( GetMoveParent() );
		m_pSprite->SetTransparency( kRenderGlow, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, m_nRenderFX );
	}

	if ( GetEntityName() != NULL_STRING && !(m_spawnflags & SF_BEAM_STARTON) )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::Precache_Impl( void )
{
	SetModelIndex( PrecacheModel( STRING( GetModelName() ) ) );
	if ( m_iszSpriteName != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteName) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEnvLaser::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "width"))
	{
		SetWidth( std::atof(szValue) );
	}
	else if (FStrEq(szKeyName, "NoiseAmplitude"))
	{
		SetNoise( std::atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "TextureScroll"))
	{
		SetScrollRate( std::atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "texture"))
	{
		SetModelName( AllocPooledString(szValue) );
	}
	else
	{
		BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether the laser is currently active.
//-----------------------------------------------------------------------------
int CEnvLaser::IsOn( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return 0;
	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputTurnOn( inputdata_t &inputdata )
{
	if (!IsOn())
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputTurnOff( inputdata_t &inputdata )
{
	if (IsOn())
	{
		TurnOff();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputToggle( inputdata_t &inputdata )
{
	if ( IsOn() )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::TurnOff( void )
{
	AddEffects( EF_NODRAW );
	if ( m_pSprite )
		m_pSprite->TurnOff();

	SetNextThink( TICK_NEVER_THINK );
	SetThink( nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::TurnOn( void )
{
	RemoveEffects( EF_NODRAW );
	if ( m_pSprite )
		m_pSprite->TurnOn();

	m_flFireTime = gpGlobals->curtime;

	SetThink( &CEnvLaser::StrikeThink );

	//
	// Call StrikeThink here to update the end position, otherwise we will see
	// the beam in the wrong place for one frame since we cleared the nodraw flag.
	//
	StrikeThink();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::FireAtPoint( trace_t &tr )
{
	SetAbsEndPos( tr.endpos );
	if ( m_pSprite )
	{
		UTIL_SetOrigin( m_pSprite, tr.endpos );
	}

	// Apply damage and do sparks every 1/10th of a second.
	if ( gpGlobals->curtime >= m_flFireTime + 0.1 )
	{
		BeamDamage( &tr );
		DoSparks( GetAbsStartPos(), tr.endpos );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::StrikeThink( void )
{
	CBaseEntity *pEnd = RandomTargetname( STRING( m_iszLaserTarget ) );

	Vector vecFireAt = GetAbsEndPos();
	if ( pEnd )
	{
		vecFireAt = pEnd->GetAbsOrigin();
	}

	trace_t tr;

	UTIL_TraceLine( GetAbsOrigin(), vecFireAt, MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );
	FireAtPoint( tr );
	SetNextThink( gpGlobals->curtime );
}
