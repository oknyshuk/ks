// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
#include "const.h"
#include "string_t.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "ndebugoverlay.h"
#endif
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "reflect_datamap.h"
#include "func_ladder.h"

#if !defined( CLIENT_DLL )
ConVar sv_showladders( "sv_showladders", "0", 0, "Show bbox and dismount points for all ladders (must be set before level load.)\n" );
#endif

std::vector< CFuncLadder * >	CFuncLadder::s_Ladders;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncLadder::CFuncLadder() :
	m_bDisabled( false )
{
	s_Ladders.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncLadder::~CFuncLadder()
{
	ks::erase_first( s_Ladders, this );
}

int CFuncLadder::GetLadderCount()
{
	return std::ssize( s_Ladders );
}

CFuncLadder *CFuncLadder::GetLadder( int index )
{
	if ( index < 0 || index >= std::ssize( s_Ladders ) )
		return nullptr;

	return s_Ladders[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::Spawn()
{
	BaseClass::Spawn();

	// Entity is symbolid
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	
	//AddFlag( FL_WORLDBRUSH );
	SetModelName( NULL_STRING );

	// Make entity invisible
	AddEffects( EF_NODRAW );
	// No model but should still network
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs = VEC_HULL_MAX;

	// This will swap them if they are inverted
	SetEndPoints( m_vecPlayerMountPositionTop, m_vecPlayerMountPositionBottom );

#if !defined( CLIENT_DLL )
	trace_t bottomtrace, toptrace;
	UTIL_TraceHull( m_vecPlayerMountPositionBottom, m_vecPlayerMountPositionBottom, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &bottomtrace );
	UTIL_TraceHull( m_vecPlayerMountPositionTop, m_vecPlayerMountPositionTop, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &toptrace );

	if ( bottomtrace.startsolid || toptrace.startsolid )
	{
		if ( bottomtrace.startsolid )
		{
			DevMsg( 1, "Warning, funcladder with blocked bottom point (%.2f %.2f %.2f) stuck in (%s)\n",
				m_vecPlayerMountPositionBottom.GetX(),
				m_vecPlayerMountPositionBottom.GetY(),
				m_vecPlayerMountPositionBottom.GetZ(),
				bottomtrace.Ent<CBaseEntity>() 
					? 
					UTIL_VarArgs( "%s/%s", bottomtrace.Ent<CBaseEntity>()->GetClassname(), bottomtrace.Ent<CBaseEntity>()->GetEntityName().ToCStr() ) 
					: 
					"NULL" );
		}
		if ( toptrace.startsolid )
		{
			DevMsg( 1, "Warning, funcladder with blocked top point (%.2f %.2f %.2f) stuck in (%s)\n",
				m_vecPlayerMountPositionTop.GetX(),
				m_vecPlayerMountPositionTop.GetY(),
				m_vecPlayerMountPositionTop.GetZ(),
				toptrace.Ent<CBaseEntity>() 
					? 
					UTIL_VarArgs( "%s/%s", toptrace.Ent<CBaseEntity>()->GetClassname(), toptrace.Ent<CBaseEntity>()->GetEntityName().ToCStr() ) 
					: 
					"NULL" );
		}

		// Force geometry overlays on, but only if developer 2 is set...
		if ( developer.GetInt() > 1 )
		{
			m_debugOverlays |= OVERLAY_TEXT_BIT;
		}
	}

	m_vecPlayerMountPositionTop -= GetAbsOrigin();
	m_vecPlayerMountPositionBottom -= GetAbsOrigin();

	// Compute mins, maxs of points
	// 
	Vector mins( MAX_COORD_INTEGER, MAX_COORD_INTEGER, MAX_COORD_INTEGER );
	Vector maxs( -MAX_COORD_INTEGER, -MAX_COORD_INTEGER, -MAX_COORD_INTEGER );
	int i;
	for ( i = 0; i < 3; i++ )
	{
		if ( m_vecPlayerMountPositionBottom.m_Value[ i ] < mins[ i ] )
		{
			mins[ i ] = m_vecPlayerMountPositionBottom.m_Value[ i ];
		}
		if ( m_vecPlayerMountPositionBottom.m_Value[ i ] > maxs[ i ] )
		{
			maxs[ i ] = m_vecPlayerMountPositionBottom.m_Value[ i ];
		}
		if ( m_vecPlayerMountPositionTop.m_Value[ i ] < mins[ i ] )
		{
			mins[ i ] = m_vecPlayerMountPositionTop.m_Value[ i ];
		}
		if ( m_vecPlayerMountPositionTop.m_Value[ i ] > maxs[ i ] )
		{
			maxs[ i ] = m_vecPlayerMountPositionTop.m_Value[ i ];
		}
	}

	// Expand mins/maxs by player hull size
	mins += playerMins;
	maxs += playerMaxs;

	UTIL_SetSize( this, mins, maxs );

	m_bFakeLadder = HasSpawnFlags(SF_LADDER_DONTGETON);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned or after reload from .sav file
//-----------------------------------------------------------------------------
void CFuncLadder::Activate()
{
	// Chain to base class
	BaseClass::Activate();

#if !defined( CLIENT_DLL )
	// Re-hook up ladder dismount points
	SearchForDismountPoints();

	// Show debugging UI if it's active
	if ( sv_showladders.GetBool() )
	{
		m_debugOverlays |= OVERLAY_TEXT_BIT;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::SearchForDismountPoints()
{
#if !defined( CLIENT_DLL )
	std::vector< CInfoLadderDismountHandle > allNodes;

	Vector topPos;
	Vector bottomPos;

	GetTopPosition( topPos );
	GetBottomPosition( bottomPos );

	float dismount_radius = 100.0f;

	Vector vecBottomToTop = topPos - bottomPos;
	float ladderLength = VectorNormalize( vecBottomToTop );

	float recheck = 40.0f;

	// add both sets of nodes
	FindNearbyDismountPoints( topPos, dismount_radius, m_Dismounts );
	FindNearbyDismountPoints( bottomPos, dismount_radius, m_Dismounts );

	while ( 1 )
	{
		ladderLength -= recheck;
		if ( ladderLength <= 0.0f )
			break;
		bottomPos += recheck * vecBottomToTop;
		FindNearbyDismountPoints( bottomPos, dismount_radius, m_Dismounts );
	}
#endif
}

void CFuncLadder::SetEndPoints( const Vector& p1, const Vector& p2 )
{
	m_vecPlayerMountPositionTop = p1;
	m_vecPlayerMountPositionBottom = p2;

	if ( m_vecPlayerMountPositionBottom.GetZ() > m_vecPlayerMountPositionTop.GetZ() )
	{
		Vector temp = m_vecPlayerMountPositionBottom;
		m_vecPlayerMountPositionBottom = m_vecPlayerMountPositionTop;
		m_vecPlayerMountPositionTop = temp;
	}

#if !defined( CLIENT_DLL)
	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs = VEC_HULL_MAX;

	trace_t result;
	UTIL_TraceHull( m_vecPlayerMountPositionTop + Vector( 0, 0, 4 ), m_vecPlayerMountPositionTop, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &result );

	if ( !result.startsolid )
	{
		m_vecPlayerMountPositionTop = result.endpos;
	}

	UTIL_TraceHull( m_vecPlayerMountPositionBottom + Vector( 0, 0, 4 ), m_vecPlayerMountPositionBottom, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &result );

	if ( !result.startsolid )
	{
		m_vecPlayerMountPositionBottom = result.endpos;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::DrawDebugGeometryOverlays()
{
#if !defined( CLIENT_DLL )

	BaseClass::DrawDebugGeometryOverlays();

	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs  = VEC_HULL_MAX;

	Vector topPosition;
	Vector bottomPosition;

	GetTopPosition( topPosition );
	GetBottomPosition( bottomPosition );

	NDebugOverlay::Box( topPosition, playerMins, playerMaxs, 255,0,0,127, 0 );
	NDebugOverlay::Box( bottomPosition, playerMins, playerMaxs, 0,0,255,127, 0 );

	NDebugOverlay::EntityBounds(this, 200, 180, 63, 63, 0);

	trace_t bottomtrace;
	UTIL_TraceHull( m_vecPlayerMountPositionBottom, m_vecPlayerMountPositionBottom, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &bottomtrace );

	int c = std::ssize( m_Dismounts );
	for ( int i = 0 ; i < c ; i++ )
	{
		CInfoLadderDismount *pt = m_Dismounts[ i ];
		if ( !pt )
			continue;

		NDebugOverlay::Box(pt->GetAbsOrigin(),Vector( -16, -16, 0 ), Vector( 16, 16, 8 ), 150,0,0, 63, 0);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CFuncLadder::GetTopPosition( Vector& org )
{
	ComputeAbsPosition( m_vecPlayerMountPositionTop + GetLocalOrigin(), &org );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CFuncLadder::GetBottomPosition( Vector& org )
{
	ComputeAbsPosition( m_vecPlayerMountPositionBottom + GetLocalOrigin(), &org );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bottomToTopVec - 
//-----------------------------------------------------------------------------
void CFuncLadder::ComputeLadderDir( Vector& bottomToTopVec )
{
	Vector top;
	Vector bottom;

	GetTopPosition( top );
	GetBottomPosition( bottom );

	bottomToTopVec = top - bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CFuncLadder::GetDismountCount() const
{
	return std::ssize( m_Dismounts );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : CInfoLadderDismountHandle
//-----------------------------------------------------------------------------
CInfoLadderDismount *CFuncLadder::GetDismount( int index )
{
	if ( index < 0 || index >= std::ssize( m_Dismounts ) )
		return nullptr;
	return m_Dismounts[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			radius - 
//			list - 
//-----------------------------------------------------------------------------
void CFuncLadder::FindNearbyDismountPoints( const Vector& origin, float radius, std::vector< CInfoLadderDismountHandle >& list )
{
#if !defined( CLIENT_DLL )
	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = gEntList.FindEntityByClassnameWithin( pEntity, "info_ladder_dismount", origin, radius)) != nullptr )
	{
		CInfoLadderDismount *landingspot = static_cast< CInfoLadderDismount * >( pEntity );
		contract_assert( landingspot );

		// If spot has a target, then if the target is not this ladder, don't add to our list.
		if ( landingspot->m_target != NULL_STRING )
		{
			if ( landingspot->GetNextTarget() != this )
			{
				continue;
			}
		}

		CInfoLadderDismountHandle handle;
		handle = landingspot;
		if ( ks::find_index( list, handle ) == -1 )
		{
			list.push_back( handle );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncLadder::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncLadder::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CFuncLadder::PlayerGotOn( CBasePlayer *pPlayer )
{
#if !defined( CLIENT_DLL )
	m_OnPlayerGotOnLadder.FireOutput(this, pPlayer);
	pPlayer->EmitSound( "Ladder.StepRight" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CFuncLadder::PlayerGotOff( CBasePlayer *pPlayer )
{
#if !defined( CLIENT_DLL )
	m_OnPlayerGotOffLadder.FireOutput(this, pPlayer);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFuncLadder::DontGetOnLadder( void ) const
{
	return m_bFakeLadder;
}

#if !defined(CLIENT_DLL)
const char *CFuncLadder::GetSurfacePropName()
{
	if ( !m_surfacePropName )
		return nullptr;
	return m_surfacePropName.ToCStr();
}
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( FuncLadder, DT_FuncLadder );

IMPLEMENT_REFLECT_TABLE( CFuncLadder, DT_FuncLadder );

LINK_ENTITY_TO_CLASS_ALIASED( func_useableladder, FuncLadder );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncLadder )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoLadderDismount::DrawDebugGeometryOverlays()
{
#if !defined( CLIENT_DLL )
	BaseClass::DrawDebugGeometryOverlays();

	if ( developer.GetBool() )
	{
		NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, 0 ), Vector( 16, 16, 8 ), 127, 127, 127, 127, 0 );
	}
#endif
}

#if defined( GAME_DLL )
int CFuncLadder::UpdateTransmitState()
{
	// transmit if in PVS for clientside prediction
	return SetTransmitState( FL_EDICT_PVSCHECK );
}
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( InfoLadderDismount, DT_InfoLadderDismount );

IMPLEMENT_REFLECT_TABLE( CInfoLadderDismount, DT_InfoLadderDismount );

LINK_ENTITY_TO_CLASS_ALIASED( info_ladder_dismount, InfoLadderDismount );

#if defined(GAME_DLL)
const char *FuncLadder_GetSurfaceprops(CBaseEntity *pLadderEntity)
{
	CFuncLadder *pLadder = dynamic_cast<CFuncLadder *>(pLadderEntity);
	if ( pLadder )
	{
		if ( pLadder->GetSurfacePropName() )
			return pLadder->GetSurfacePropName();
	}
	return "ladder";
}
#endif
