// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <time.h>
#include <math.h>
#include "stringregistry.h"
#include "eventlist.h"
#include "client_class.h"
#include "const.h"
#include "ispatialpartition.h"
#include "tier2/resourceprecacher.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "fire_smoke.h"
#include "edict.h"
#include "engine/IEngineTrace.h"
#include "iclientrenderable.h"
#include "materialsystem/itexture.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "materialsystem/MaterialSystemUtil.h"
module ks.client;

import std;
import ks.core.dbg;
import ks.core.threadtools;
import ks.platform;
import ks.simd;
import :sharedvar;
import ks.core.basetypes;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :util;
import :particlemgr;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :baseentity;
import ks.game.gamemovement;
import ks.game.sun_shared;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vplane;
import ks.tier1.tracked_ref;
import ks.core.random;
import ks.core.types;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.core.vprof;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.tier1.interfaces;
import ks.tier1.strtools;
import ks.game.shareddefs;
import :cstrike15.cs_player;
import ks.game.mapentities_shared;

// NOTE: If CStringRegistry allowed storing arbitrary data, we could just use that.
// in this case we have the "isPrivate" member and the replacement rules 
// (eventIndex can be reused by private activities), so a custom table is necessary
struct eventlist_t
{
	int					eventIndex;
	int					iType;
	CStringRegistry::Key	stringKey;
	short				isPrivate;
};

std::vector< eventlist_t > g_EventList;

// This stores the actual event names.  Also, the string ID in the registry is simply an index 
// into the g_EventList array.
CStringRegistry	g_EventStrings;

// this is just here to accelerate adds
static int g_HighestEvent = 0;

int g_nEventListVersion = 1;

void EventList_Init( void )
{
	g_HighestEvent = 0;
}

void EventList_Free( void )
{
	g_EventStrings.ClearStrings();
	g_EventList.clear(); g_EventList.shrink_to_fit();

	// So studiohdrs can reindex event indices
	++g_nEventListVersion;
}

// add a new event to the database
eventlist_t *EventList_AddEventEntry( const char *pName, int iEventIndex, bool isPrivate, int iType )
{
	int index = std::ssize( g_EventList ); g_EventList.emplace_back();
	eventlist_t *pList = &g_EventList[index];
	pList->eventIndex = iEventIndex;
	pList->stringKey = g_EventStrings.AddString( pName, index );
	pList->isPrivate = isPrivate;
	pList->iType = iType;	

	// UNDONE: This implies that ALL shared activities are added before ANY custom activities
	// UNDONE: Segment these instead?  It's a 32-bit int, how many activities do we need?
	if ( iEventIndex > g_HighestEvent )
	{
		g_HighestEvent = iEventIndex;
	}

	return pList;
}

// get the database entry from a string
static eventlist_t *ListFromString( const char *pString )
{
	// just use the string registry to do this search/map
	int stringID = g_EventStrings.GetStringID( pString );
	if ( stringID < 0 )
		return nullptr;

	return &g_EventList[stringID];
}

// Get the database entry for an index
static eventlist_t *ListFromEvent( int eventIndex )
{
	// ugly linear search
	for ( int i = 0; i < std::ssize( g_EventList ); i++ )
	{
		if ( g_EventList[i].eventIndex == eventIndex )
		{
			return &g_EventList[i];
		}
	}

	return nullptr;
}

int EventList_GetEventType( int eventIndex )
{
	eventlist_t *pEvent = ListFromEvent( eventIndex );

	if ( pEvent )
	{
		return pEvent->iType;
	}

	return -1;
}

bool EventList_RegisterSharedEvent( const char *pszEventName, int iEventIndex, int iType )
{
	// UNDONE: Do we want to do these checks when not in developer mode? or maybe DEBUG only?
	// They really only matter when you change the list of code controlled activities.  IDs
	// for content controlled activities never collide because they are generated.

	// first, check to make sure the slot we're asking for is free. It must be for 
	// a shared event.
	eventlist_t *pList = ListFromString( pszEventName );
	if ( !pList )
	{
		pList = ListFromEvent( iEventIndex );
	}

	//Already in list.
	if ( pList )
	{
		return false;
	}
	// ----------------------------------------------------------------

	EventList_AddEventEntry( pszEventName, iEventIndex, false, iType );
	return true;
}

Animevent EventList_RegisterPrivateEvent( const char *pszEventName )
{
	eventlist_t *pList = ListFromString( pszEventName );
	if ( pList )
	{
		// this activity is already in the list. If the activity we collided with is also private, 
		// then the collision is OK. Otherwise, it's a bug.
		if ( pList->isPrivate )
		{
			return (Animevent)pList->eventIndex;
		}
		else
		{
			// this private activity collides with a shared activity. That is not allowed.
			Warning( "***\nShared<->Private Event collision!\n***\n" );
			contract_assert( 0 );
			return AE_INVALID;
		}
	}

	int iType = AE_TYPE_SERVER;

	iType = AE_TYPE_CLIENT;

	pList = EventList_AddEventEntry( pszEventName, g_HighestEvent+1, true, iType );
	return (Animevent)pList->eventIndex;
}

// Get the index for a given Event name
// Done at load time for all models
int EventList_IndexForName( const char *pszEventName )
{
	// this is a fast O(lgn) search (actually does 2 O(lgn) searches)
	eventlist_t *pList = ListFromString( pszEventName );

	if ( pList )
	{
		return pList->eventIndex;
	}

	return -1;
}

// Get the name for a given index
// This should only be used in debug code, it does a linear search
// But at least it only compares integers
const char *EventList_NameForIndex( int eventIndex )
{
	eventlist_t *pList = ListFromEvent( eventIndex );
	if ( pList )
	{
		return g_EventStrings.GetStringForKey( pList->stringKey );
	}
	return nullptr;
}

void EventList_RegisterSharedEvents( void )
{
	REGISTER_SHARED_ANIMEVENT( AE_EMPTY, AE_TYPE_SERVER );
	
	REGISTER_SHARED_ANIMEVENT( AE_NPC_LEFTFOOT, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_RIGHTFOOT, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_BODYDROP_LIGHT, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_BODYDROP_HEAVY, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_SWISHSOUND, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_180TURN, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_ITEM_PICKUP, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_DROP, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_SET_SEQUENCE_NAME, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_SET_SEQUENCE_NUMBER, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_SET_ACTIVITY, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_HOLSTER, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_DRAW, AE_TYPE_SERVER  );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_WEAPON_FIRE, AE_TYPE_SERVER | AE_TYPE_WEAPON );

	REGISTER_SHARED_ANIMEVENT( AE_CL_PLAYSOUND, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_SV_PLAYSOUND, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_STOPSOUND, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_START_SCRIPTED_EFFECT, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_STOP_SCRIPTED_EFFECT, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CLIENT_EFFECT_ATTACH, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_MUZZLEFLASH, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_MUZZLEFLASH, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_THUMPER_THUMP, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_AMMOCRATE_PICKUP_AMMO, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_RAGDOLL, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_ADDGESTURE, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_RESTARTGESTURE, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_ATTACK_BROADCAST, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_NPC_HURT_INTERACTION_PARTNER, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_NPC_SET_INTERACTION_CANTDIE, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_SV_DUSTTRAIL, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_CREATE_PARTICLE_EFFECT, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_STOP_PARTICLE_EFFECT, AE_TYPE_CLIENT );	
	REGISTER_SHARED_ANIMEVENT( AE_CL_ADD_PARTICLE_EFFECT_CP, AE_TYPE_CLIENT );	
	REGISTER_SHARED_ANIMEVENT( AE_CL_CREATE_PARTICLE_BRASS, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_RAGDOLL, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_ENABLE_BODYGROUP, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_DISABLE_BODYGROUP, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_VALUE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_VALUE_CMODEL_WPN, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_TO_CLIP, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_BODYGROUP_SET_TO_NEXTCLIP, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_EJECT_MAG, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_EJECT_MAG_UNHIDE, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CL_SHOW_SILENCER, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_ATTACH_SILENCER_COMPLETE, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_HIDE_SILENCER, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_CL_DETACH_SILENCER_COMPLETE, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_PRIMARYATTACK, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_INCREMENTAMMO, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_HIDE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_UNHIDE, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_ASW_FOOTSTEP, AE_TYPE_CLIENT  );
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_FOOTSTEP, AE_TYPE_CLIENT  );
	
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_RELOAD_SOUND_A, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_RELOAD_SOUND_B, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_MARINE_RELOAD_SOUND_C, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_REMOVE_CLIENT_AIM, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_PLAYWPNSOUND, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_COMPLETE_RELOAD, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_MELEE_DAMAGE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_MELEE_START_COLLISION_DAMAGE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_MELEE_STOP_COLLISION_DAMAGE, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_SCREEN_SHAKE, AE_TYPE_CLIENT );
	REGISTER_SHARED_ANIMEVENT( AE_START_DETECTING_COMBO, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_STOP_DETECTING_COMBO, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_COMBO_TRANSITION, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_ALLOW_MOVEMENT, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_SKILL_EVENT, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_TUG_INCAP, AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_BEGIN_TAUNT_LOOP, AE_TYPE_CLIENT | AE_TYPE_SERVER );

	REGISTER_SHARED_ANIMEVENT( AE_CLIENT_EJECT_BRASS, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_CL_SET_STATTRAK_GLOW, AE_TYPE_CLIENT );

	REGISTER_SHARED_ANIMEVENT( AE_WPN_CZ_DUMP_CURRENT_MAG, AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_CZ_UPDATE_BODYGROUP, AE_TYPE_SERVER );
	
	REGISTER_SHARED_ANIMEVENT( AE_WPN_CLIP_TO_POSEPARAM, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_NEXTCLIP_TO_POSEPARAM, AE_TYPE_CLIENT | AE_TYPE_SERVER );
	REGISTER_SHARED_ANIMEVENT( AE_WPN_EMPTYSHOTS_TO_POSEPARAM, AE_TYPE_CLIENT | AE_TYPE_SERVER );

}

using namespace ::ks::reflect;

PRECACHE_REGISTER_BEGIN( GLOBAL, SmokeStackMaterials )
	PRECACHE( MATERIAL, "particle/SmokeStack" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecvProp - 
//			*pStruct - 
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_Scale( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CFireSmoke	*pFireSmoke	= (CFireSmoke	*) pStruct;
	float scale				= pData->m_Value.m_Float;

	//If changed, update our internal information
	if ( ( pFireSmoke->m_flScale != scale ) && ( pFireSmoke->m_flScaleEnd != scale ) )
	{
		pFireSmoke->m_flScaleStart		= pFireSmoke->m_flScaleRegister;
		pFireSmoke->m_flScaleEnd		= scale;			
	}

	pFireSmoke->m_flScale = scale;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecvProp - 
//			*pStruct - 
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_ScaleTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CFireSmoke	*pFireSmoke	= (CFireSmoke	*) pStruct;
	float time				= pData->m_Value.m_Float;

	//If changed, update our internal information
	//if ( pFireSmoke->m_flScaleTime != time )
	{
		if ( time == -1.0f )
		{
			pFireSmoke->m_flScaleTimeStart	= Helper_GetTime()-1.0f;
			pFireSmoke->m_flScaleTimeEnd	= pFireSmoke->m_flScaleTimeStart;
		}
		else
		{
			pFireSmoke->m_flScaleTimeStart	= Helper_GetTime();
			pFireSmoke->m_flScaleTimeEnd	= Helper_GetTime() + time;
		}
	}

	pFireSmoke->m_flScaleTime = time;
}

//Receive datatable
IMPLEMENT_REFLECT_CLIENTCLASS( CFireSmoke, DT_FireSmoke, CFireSmoke )

//==================================================
// CFireSmoke
//==================================================

CFireSmoke::CFireSmoke()
{
}

CFireSmoke::~CFireSmoke()
{

	// Shut down our effect if we have it
	if ( m_hEffect )
	{
		m_hEffect->StopEmission(false, false , true);
		m_hEffect = nullptr;
	}

}

static constexpr auto FLAME_ALPHA_START = 0.9f;
static constexpr auto FLAME_ALPHA_END = 1.0f;

static constexpr auto FLAME_TRANS_START = 0.75f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::AddFlames( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CFireSmoke::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::UpdateEffects( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFireSmoke::ShouldDraw()
{

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::Start( void )
{
	const char *lpszEffectName;
	int nSize = (int) std::floor( m_flStartScale / 36.0f );

	switch ( nSize )
	{
	case 0:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_tiny_smoke" : "env_fire_tiny";
		break;

	case 1:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_small_smoke" : "env_fire_small";
		break;

	case 2:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_medium_smoke" : "env_fire_medium";
		break;

	case 3:
	default:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_large_smoke" : "env_fire_large";
		break;
	}

	// Create the effect of the correct size
	m_hEffect = ParticleProp()->Create( lpszEffectName, PATTACH_ABSORIGIN );

}

//-----------------------------------------------------------------------------
// Purpose: FIXME: what's the right way to do this?
//-----------------------------------------------------------------------------
void CFireSmoke::StartClientOnly( void )
{
	Start();

	ClientEntityList().AddNonNetworkableEntity(	this );
	CollisionProp()->CreatePartitionHandle();
	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW );
	AddToLeafSystem();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::RemoveClientOnly(void)
{
	ClientThinkList()->RemoveThinkable( GetClientHandle() );

	// Remove from the client entity list.
	ClientEntityList().RemoveEntity( GetClientHandle() );

	::partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );

	RemoveFromLeafSystem();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::UpdateAnimation( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::UpdateFlames( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::UpdateScale( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::Update( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFireSmoke::FindClipPlane( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Spawn smoke (...duh)
//-----------------------------------------------------------------------------

void CFireSmoke::SpawnSmoke( void )
{
}

int CFireSprite::DrawModel( int flags, const RenderableInstance_t &instance)
{
	if ( m_bFadeFromAbove )
	{
		// The sprites become less visible the more you look down or up at them
		Vector vToPos = GetLocalOrigin() - CurrentViewOrigin();
		VectorNormalize( vToPos );

		float fUpAmount = vToPos.z;

		int iAlpha = 255;

		if ( fUpAmount < -0.75f )
			iAlpha = 0;
		else if ( fUpAmount < -0.65f )
			iAlpha = 255 - (int)( ( fUpAmount + 0.65f ) * 10.0f * -255.0f );
		else if ( fUpAmount > 0.85f )
			iAlpha = 0;
		else if ( fUpAmount > 0.75f )
			iAlpha = 255 - (int)( ( fUpAmount - 0.75f ) * 10.0f * 255.0f );

		SetColor( iAlpha, iAlpha, iAlpha );
	}

	return BaseClass::DrawModel( flags, instance );
}

int CFireFromAboveSprite::DrawModel( int flags, const RenderableInstance_t &instance)
{
	// The sprites become more visible the more you look down or up at them
	Vector vToPos = GetLocalOrigin() - CurrentViewOrigin();
	VectorNormalize( vToPos );

	float fUpAmount = vToPos.z;

	int iAlpha = 0;

	if ( fUpAmount < -0.85f )
		iAlpha = 255;
	else if ( fUpAmount < -0.65f )
		iAlpha = (int)( ( fUpAmount + 0.65f ) * 5.0f * -255.0f );
	else if ( fUpAmount > 0.75f )
		iAlpha = 255;
	else if ( fUpAmount > 0.55f )
		iAlpha = (int)( ( fUpAmount - 0.55f ) * 5.0f * 255.0f );

	SetColor( iAlpha, iAlpha, iAlpha );

	return BaseClass::DrawModel( flags, instance );
}

CFireOverlay::CFireOverlay( CFireSmoke *owner)
{
	m_pOwner	= owner;
	m_flScale	= 0.0f;
	m_nGUID		= random_valve->RandomInt( -999999, 999999 );
}

class CBaseAnimating;

using namespace ::ks::reflect;

// c_fish.cpp
// Simple fish client-side logic
// Author: Michael S. Booth, April 2005

extern float UTIL_WaterLevel( const Vector &position, float minz, float maxz );

ConVar FishDebug( side_registry(), "fish_debug", "0", FCVAR_CHEAT, "Show debug info for fish" );

//-----------------------------------------------------------------------------
/**
 * Client-side fish entity
 */
void RecvProxy_FishOriginX( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_FishOriginY( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CFish : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_CFish", .base = false } ]]
	[[= ks::reflect::From<"m_actualPos.x", ks::reflect::Net{ .wire = "m_x" }, RecvProxy_FishOriginX>{} ]]
	[[= ks::reflect::From<"m_actualPos.y", ks::reflect::Net{ .wire = "m_y" }, RecvProxy_FishOriginY>{} ]]
	[[= ks::reflect::From<"m_actualPos.z", ks::reflect::Net{ .wire = "m_z" }>{} ]]
	[[= ks::reflect::From<"m_actualAngles.y", ks::reflect::Net{ .wire = "m_angle" }>{} ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_lifeState", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFish, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	virtual void Spawn( void );
	virtual void ClientThink();

	virtual void OnDataChanged( DataUpdateType_t type );

private:
	friend void RecvProxy_FishOriginX( const CRecvProxyData *pData, void *pStruct, void *pOut );
	friend void RecvProxy_FishOriginY( const CRecvProxyData *pData, void *pStruct, void *pOut );

	Vector m_pos;						///< local position
	Vector m_vel;						///< local velocity
	QAngle m_angles;					///< local angles

	int m_localLifeState;				///< our version of m_lifeState

	float m_deathDepth;					///< water depth when we died
	float m_deathAngle;					///< angle to float at when dead
	float m_buoyancy;					///< so each fish floats at a different rate when dead

	CountdownTimer m_wiggleTimer;		///< for simulating swimming motions
	float m_wigglePhase;				///< where in the wiggle sinusoid we are
	float m_wiggleRate;					///< the speed of our wiggling

	Vector m_actualPos;					///< position from server
	QAngle m_actualAngles;				///< angles from server

	[[= ks::reflect::Net{} ]] Vector m_poolOrigin;
	[[= ks::reflect::Net{} ]] float m_waterLevel;					///< Z coordinate of water surface

	bool m_gotUpdate;					///< true after we have received a network update

	enum { MAX_ERROR_HISTORY = 20 };
	float m_errorHistory[ MAX_ERROR_HISTORY ];	///< error history samples
	int m_errorHistoryIndex;
	int m_errorHistoryCount;
	float m_averageError;
};

//-----------------------------------------------------------------------------
void RecvProxy_FishOriginX( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CFish *fish = (CFish *)pStruct;
	float *out = (float *)pOut;

	*out = pData->m_Value.m_Float + fish->m_poolOrigin.x;
}

void RecvProxy_FishOriginY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CFish *fish = (CFish *)pStruct;
	float *out = (float *)pOut;

	*out = pData->m_Value.m_Float + fish->m_poolOrigin.y;
}

IMPLEMENT_REFLECT_CLIENTCLASS( CFish, DT_CFish, CFish )

//-----------------------------------------------------------------------------
void CFish::Spawn( void )
{
	BaseClass::Spawn();

	m_angles = QAngle( 0, 0, 0 );
	m_actualAngles = m_angles;

	m_vel = Vector( 0, 0, 0 );
	m_gotUpdate = false;
	m_localLifeState = LIFE_ALIVE;
	m_buoyancy = RandomFloat( 0.4f, 1.0f );

	m_errorHistoryIndex = 0;
	m_errorHistoryCount = 0;
	m_averageError = 0.0f;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
void CFish::ClientThink()
{
	if (FishDebug.GetBool())
	{
		debugoverlay->AddLineOverlay( m_pos, m_actualPos, 255, 0, 0, true, 0.1f );
		switch( m_localLifeState )
		{
			case LIFE_DYING:
				debugoverlay->AddTextOverlay( m_pos, 0.1f, "DYING" );
				break;

			case LIFE_DEAD:
				debugoverlay->AddTextOverlay( m_pos, 0.1f, "DEAD" );
				break;
		}
	}

	float deltaT = gpGlobals->frametime;

	// check if we just died
	if (m_localLifeState == LIFE_ALIVE && m_lifeState != LIFE_ALIVE)
	{
		// we have died
		m_localLifeState = LIFE_DYING;

		m_deathDepth = m_pos.z;

		// determine surface float angle
		m_deathAngle = RandomFloat( 87.0f, 93.0f ) * ((RandomInt( 0, 100 ) < 50) ? 1.0f : -1.0f);
	}

	switch( m_localLifeState )
	{
		case LIFE_DYING:
		{
			// depth parameter
			float t = (m_pos.z - m_deathDepth) / (m_waterLevel - m_deathDepth);
			t *= t;

			// roll onto side
			m_angles.z = m_deathAngle * t;

			// float to surface
			const float fudge = 2.0f;
			if (m_pos.z < m_waterLevel - fudge)
			{
				m_vel.z += (1.0f - t) * m_buoyancy * deltaT;
			}
			else
			{
				m_localLifeState = LIFE_DEAD;
			}

			break;
		}

		case LIFE_DEAD:
		{
			// depth parameter
			float t = (m_pos.z - m_deathDepth) / (m_waterLevel - m_deathDepth);
			t *= t;

			// roll onto side
			m_angles.z = m_deathAngle * t;

			// keep near water surface
			const float sub = 0.5f;
			m_vel.z += 10.0f * (m_waterLevel - m_pos.z - sub) * deltaT;

			// bob on surface
			const float rollAmp = 5.0f;
			const float rollFreq = 2.33f;
			m_angles.z += rollAmp * std::sin( rollFreq * (gpGlobals->curtime + 10.0f * entindex()) ) * deltaT;

			const float rollAmp2 = 7.0f;
			const float rollFreq2 = 4.0f;
			m_angles.x += rollAmp2 * sin( rollFreq2 * (gpGlobals->curtime + 10.0f * entindex()) ) * deltaT;

			const float bobAmp = 0.75f;
			const float bobFreq = 4.0f;
			m_vel.z += bobAmp * sin( bobFreq * (gpGlobals->curtime + 10.0f * entindex()) ) * deltaT;

			const float bobAmp2 = 0.75f;
			const float bobFreq2 = 3.333f;
			m_vel.z += bobAmp2 * sin( bobFreq2 * (gpGlobals->curtime + 10.0f * entindex()) ) * deltaT;

			// decay movement speed to zero
			const float drag = 1.0f;
			m_vel.z -= drag * m_vel.z * deltaT;

			break;
		}

		case LIFE_ALIVE:
		{
			// use server-side Z coordinate directly
			m_pos.z = m_actualPos.z;

			// use server-side angles
			m_angles = m_actualAngles;

			// fishy wiggle based on movement
			if (!m_wiggleTimer.IsElapsed())
			{
				float swimPower = 1.0f - (m_wiggleTimer.GetElapsedTime() / m_wiggleTimer.GetCountdownDuration());
				const float amp = 6.0f * swimPower;
				float wiggle = amp * std::sin( m_wigglePhase );

				m_wigglePhase += m_wiggleRate * deltaT;

				// wiggle decay
				const float wiggleDecay = 5.0f;
				m_wiggleRate -= wiggleDecay * deltaT;

				m_angles.y += wiggle;
			}

			break;
		}
	}

	// compute error between our local position and actual server position
	Vector error = m_actualPos - m_pos;
	error.z = 0.0f;
	float errorLen = error.Length();

	if (m_localLifeState == LIFE_ALIVE)
	{
		// if error is far above average, start swimming
		const float wiggleThreshold = 2.0f;
		if (errorLen - m_averageError > wiggleThreshold)
		{
			// if error is large, we must have started swimming
			const float swimTime = 5.0f;
			m_wiggleTimer.Start( swimTime );

			m_wiggleRate = 2.0f * errorLen;

			const float maxWiggleRate = 30.0f;
			if (m_wiggleRate > maxWiggleRate)
			{
				m_wiggleRate = maxWiggleRate;
			}
		}

		// update average error
		m_errorHistory[ m_errorHistoryIndex++ ] = errorLen;
		if (m_errorHistoryIndex >= MAX_ERROR_HISTORY)
		{
			m_errorHistoryIndex = 0;
			m_errorHistoryCount = MAX_ERROR_HISTORY;
		}
		else if (m_errorHistoryCount < MAX_ERROR_HISTORY)
		{
			++m_errorHistoryCount;
		}

		m_averageError = 0.0f;
		if (m_errorHistoryCount)
		{
			for( int r=0; r<m_errorHistoryCount; ++r )
			{
				m_averageError += m_errorHistory[r];
			}
			m_averageError /= (float)m_errorHistoryCount;
		}
	}

	// keep fish motion smooth by correcting towards actual server position
	// NOTE: This only tracks XY motion
	const float maxError = 20.0f;
	float errorT = errorLen / maxError;
	if (errorT > 1.0f)
	{
		errorT = 1.0f;
	}

	// we want a nonlinear spring force for tracking
	errorT *= errorT;

	// as fish move faster, their error increases - use a stiffer spring when fast, and a weak one when slow
	const float trackRate = 0.0f + errorT * 115.0f;
	m_vel.x += trackRate * error.x * deltaT;
	m_vel.y += trackRate * error.y * deltaT;

	const float trackDrag = 2.0f + errorT * 6.0f;
	m_vel.x -= trackDrag * m_vel.x * deltaT;
	m_vel.y -= trackDrag * m_vel.y * deltaT;

	// euler integration
	m_pos += m_vel * deltaT;

	SetNetworkOrigin( m_pos );
	SetAbsOrigin( m_pos );

	SetNetworkAngles( m_angles );
	SetAbsAngles( m_angles );
}

//-----------------------------------------------------------------------------
void CFish::OnDataChanged( DataUpdateType_t type )
{
	//if (!m_gotUpdate)

	if (type == DATA_UPDATE_CREATED)
	{
		// initial update
		m_gotUpdate = true;

		m_pos = m_actualPos;
		m_vel = Vector( 0, 0, 0 );

		return;
	}
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

extern ConVar r_flashlightdepthres;

extern ConVar r_flashlightdepthtexture;

static ConVar r_swingflashlight( side_registry(), "r_swingflashlight", "1", FCVAR_CHEAT );
static ConVar r_flashlightlockposition( side_registry(), "r_flashlightlockposition", "0", FCVAR_CHEAT );
static ConVar r_flashlightfov( side_registry(), "r_flashlightfov", "53.0", FCVAR_CHEAT );
ConVar r_flashlightoffsetx( side_registry(), "r_flashlightoffsetright", "5.0", FCVAR_CHEAT );
ConVar r_flashlightoffsety( side_registry(), "r_flashlightoffsetup", "-5.0", FCVAR_CHEAT );
ConVar r_flashlightoffsetz( side_registry(), "r_flashlightoffsetforward", "0.0", FCVAR_CHEAT );
static ConVar r_flashlightnear( side_registry(), "r_flashlightnear", "4.0", FCVAR_CHEAT );
static ConVar r_flashlightfar( side_registry(), "r_flashlightfar", "750.0", FCVAR_CHEAT );
static ConVar r_flashlightconstant( side_registry(), "r_flashlightconstant", "0.0", FCVAR_CHEAT );
static ConVar r_flashlightlinear( side_registry(), "r_flashlightlinear", "100.0", FCVAR_CHEAT );
static ConVar r_flashlightquadratic( side_registry(), "r_flashlightquadratic", "0.0", FCVAR_CHEAT );
static ConVar r_flashlightvisualizetrace( side_registry(), "r_flashlightvisualizetrace", "0", FCVAR_CHEAT );
static ConVar r_flashlightambient( side_registry(), "r_flashlightambient", "0.0", FCVAR_CHEAT );
static ConVar r_flashlightshadowatten( side_registry(), "r_flashlightshadowatten", "0.35", FCVAR_CHEAT );
static ConVar r_flashlightladderdist( side_registry(), "r_flashlightladderdist", "40.0", FCVAR_CHEAT );
static ConVar r_flashlight_topdown( side_registry(), "r_flashlight_topdown", "0" );

static ConVar r_flashlightnearoffsetscale( side_registry(), "r_flashlightnearoffsetscale", "1.0", FCVAR_CHEAT );
static ConVar r_flashlighttracedistcutoff( side_registry(), "r_flashlighttracedistcutoff", "128" );
static ConVar r_flashlightbacktraceoffset( side_registry(), "r_flashlightbacktraceoffset", "0.4", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
CFlashlightEffectManager & FlashlightEffectManager( int32 nSplitscreenPlayerOverride )
{
	static CFlashlightEffectManager s_flashlightEffectManagerArray[ MAX_SPLITSCREEN_PLAYERS ];

	if ( nSplitscreenPlayerOverride != -1 )
	{
		return s_flashlightEffectManagerArray[ nSplitscreenPlayerOverride ];
	}

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return s_flashlightEffectManagerArray[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nEntIndex - The m_nEntIndex of the client entity that is creating us.
//			vecPos - The position of the light emitter.
//			vecDir - The direction of the light emission.
//-----------------------------------------------------------------------------
CFlashlightEffect::CFlashlightEffect(int nEntIndex, const char *pszTextureName, float flFov, float flFarZ, float flLinearAtten )
{
	m_FlashlightHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_nEntIndex = nEntIndex;

	m_flCurrentPullBackDist = 1.0f;

	m_bMuzzleFlashEnabled = false;
	m_flMuzzleFlashBrightness = 1.0f;

	m_flFov = flFov;
	m_flFarZ = flFarZ;
	m_flLinearAtten = flLinearAtten;
	m_bCastsShadows = true;

	m_bIsOn = false;

	UpdateFlashlightTexture( pszTextureName );
	m_MuzzleFlashTexture.Init( "effects/flashlight_freezecam", TEXTURE_GROUP_OTHER, true, TEXTUREFLAGS_SRGB );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFlashlightEffect::~CFlashlightEffect()
{
	LightOff();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFlashlightEffect::TurnOn()
{
	m_bIsOn = true;
	m_flCurrentPullBackDist = 1.0f;
}

//-----------------------------------------------------------------------------
void CFlashlightEffect::SetMuzzleFlashEnabled( bool bEnabled, float flBrightness )
{
	m_bMuzzleFlashEnabled = bEnabled;
	m_flMuzzleFlashBrightness = flBrightness;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFlashlightEffect::TurnOff()
{
	if (m_bIsOn)
	{
		m_bIsOn = false;
		LightOff();
	}
}

// Custom trace filter that skips the player and the view model.
// If we don't do this, we'll end up having the light right in front of us all
// the time.
class CTraceFilterSkipPlayerAndViewModel : public CTraceFilter
{
public:
	CTraceFilterSkipPlayerAndViewModel( CBasePlayer *pPlayer, bool bTracePlayers )
	{
		m_pPlayer = pPlayer;
		m_bSkipPlayers = !bTracePlayers;

		{
			m_pLowerBody = nullptr;
		}
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		// Test against the vehicle too?
		// FLASHLIGHTFIXME: how do you know that you are actually inside of the vehicle?
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
		if ( !pEntity )
			return true;

		if ( ( ToBaseViewModel( pEntity ) != nullptr ) ||
			 pEntity == m_pPlayer ||
			 pEntity == m_pLowerBody ||
			 ( m_bSkipPlayers && pEntity->IsPlayer() ) ||
			 pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS ||
			 pEntity->GetCollisionGroup() == COLLISION_GROUP_INTERACTIVE_DEBRIS )
		{
			return false;
		}

		return true;
	}

private:
	CBaseEntity *m_pPlayer;
	CBaseEntity *m_pLowerBody;
	bool m_bSkipPlayers;
};

void CBasePlayer::GetFlashlightOffset( const Vector &vecForward, const Vector &vecRight, const Vector &vecUp, Vector *pVecOffset ) const
{
	*pVecOffset = r_flashlightoffsety.GetFloat() * vecUp + r_flashlightoffsetx.GetFloat() * vecRight + r_flashlightoffsetz.GetFloat() * vecForward;
}

ConVar r_flashlightmuzzleflashfov( side_registry(), "r_flashlightmuzzleflashfov", "120", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Update the flashlight for top down camera view
//  (flashlight origin doesn't move around when you get near things)
//-----------------------------------------------------------------------------
void CFlashlightEffect::UpdateLightTopDown(const Vector &vecPos, const Vector &vecForward, const Vector &vecRight, const Vector &vecUp )
{
	CVProfScope _( "CFlashlightEffect::UpdateLightTopDown", 0, VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING, false, BUDGETFLAG_OTHER );

	FlashlightState_t state;

	state.m_vecLightOrigin = vecPos;

	Vector vTarget = vecPos + vecForward * r_flashlightfar.GetFloat();

	// Work with these local copies of the basis for the rest of the function
	Vector vDir   = vTarget - vecPos;
	Vector vRight = vecRight;
	Vector vUp    = vecUp;
	VectorNormalize( vDir   );
	VectorNormalize( vRight );
	VectorNormalize( vUp    );

	// Orthonormalize the basis, since the flashlight texture projection will require this later...
	vUp -= DotProduct( vDir, vUp ) * vDir;
	VectorNormalize( vUp );
	vRight -= DotProduct( vDir, vRight ) * vDir;
	VectorNormalize( vRight );
	vRight -= DotProduct( vUp, vRight ) * vUp;
	VectorNormalize( vRight );

	contract_assert( fabs( DotProduct( vDir, vRight ) - 0.0f ) <= 1e-3 );
	contract_assert( fabs( DotProduct( vDir, vUp    ) - 0.0f ) <= 1e-3 );
	contract_assert( fabs( DotProduct( vRight, vUp  ) - 0.0f ) <= 1e-3 );

	BasisToQuaternion( vDir, vRight, vUp, state.m_quatOrientation );

	state.m_fConstantAtten = r_flashlightconstant.GetFloat();
	state.m_fQuadraticAtten = r_flashlightquadratic.GetFloat();
	state.m_fLinearAtten = r_flashlightlinear.GetFloat();
	state.m_fHorizontalFOVDegrees = r_flashlightfov.GetFloat();
	state.m_fVerticalFOVDegrees = r_flashlightfov.GetFloat();

	state.m_Color[0] = 1.0f;
	state.m_Color[1] = 1.0f;
	state.m_Color[2] = 1.0f;
	state.m_Color[3] = r_flashlightambient.GetFloat();
	state.m_NearZ = r_flashlightnear.GetFloat() + m_flCurrentPullBackDist;		// Push near plane out so that we don't clip the world when the flashlight pulls back 
	state.m_FarZ = state.m_FarZAtten = r_flashlightfar.GetFloat();	// Strictly speaking, these are different, but the game can treat them the same
	state.m_bEnableShadows = state.m_bEnableShadows && r_flashlightdepthtexture.GetBool();
	state.m_flShadowMapResolution = r_flashlightdepthres.GetInt();

	state.m_pSpotlightTexture = m_FlashlightTexture;
	state.m_nSpotlightTextureFrame = 0;

	state.m_flShadowAtten = r_flashlightshadowatten.GetFloat();
	state.m_flShadowSlopeScaleDepthBias = g_pMaterialSystemHardwareConfig->GetShadowSlopeScaleDepthBias();
	state.m_flShadowDepthBias = g_pMaterialSystemHardwareConfig->GetShadowDepthBias();

	if( m_FlashlightHandle == CLIENTSHADOW_INVALID_HANDLE )
	{
		m_FlashlightHandle = g_pClientShadowMgr->CreateFlashlight( state );
	}
	else
	{
		if( !r_flashlightlockposition.GetBool() )
		{
			g_pClientShadowMgr->UpdateFlashlightState( m_FlashlightHandle, state );
		}
	}

	g_pClientShadowMgr->UpdateProjectedTexture( m_FlashlightHandle, true );

	// Kill the old flashlight method if we have one.
	// FIXME: This doesn't compile
//	LightOffOld();

}

//-----------------------------------------------------------------------------
// Purpose: Do the headlight
//-----------------------------------------------------------------------------
void CFlashlightEffect::UpdateLight(	int nEntIdx, const Vector &vecPos, const Vector &vecForward, const Vector &vecRight,
										const Vector &vecUp, float flFov, float flFarZ, float flLinearAtten, bool castsShadows, const char* pTextureName )
{
	CVProfScope _( __FUNCTION__, 0, VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING, false, BUDGETFLAG_OTHER );

	if ( r_flashlight_topdown.GetBool() )
	{
		UpdateLightTopDown( vecPos, vecForward, vecRight, vecUp );
		return;
	}

	m_nEntIndex = nEntIdx;
	m_flFov = flFov;
	m_flFarZ = flFarZ;
	m_flLinearAtten = flLinearAtten;

	if ( m_bCastsShadows != castsShadows )
	{
		// requires recreation of the flashlight
		LightOff();
	}
	m_bCastsShadows = castsShadows;

	UpdateFlashlightTexture( pTextureName );

	FlashlightState_t state;

	if ( UpdateDefaultFlashlightState( state, vecPos, vecForward, vecRight, vecUp, castsShadows ) == false )
	{
		return;
	}

	if( m_FlashlightHandle == CLIENTSHADOW_INVALID_HANDLE )
	{
		m_FlashlightHandle = g_pClientShadowMgr->CreateFlashlight( state );
	}
	else
	{
		if( !r_flashlightlockposition.GetBool() )
		{
			g_pClientShadowMgr->UpdateFlashlightState( m_FlashlightHandle, state );
		}
	}
	
	g_pClientShadowMgr->UpdateProjectedTexture( m_FlashlightHandle, true );
	
}

void CFlashlightEffect::UpdateLight(	int nEntIdx, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, 
										float flFov, bool castsShadows, ITexture *pFlashlightTexture, const Vector &vecBrightness,
										bool bTracePlayers )
{
	CVProfScope _( __FUNCTION__, 0, VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING, false, BUDGETFLAG_OTHER );

	m_nEntIndex = nEntIdx;
	if ( m_bCastsShadows != castsShadows )
	{
		// requires recreation of the flashlight
		LightOff();
	}
	m_bCastsShadows = castsShadows;

	FlashlightState_t state;

	if ( UpdateDefaultFlashlightState( state, vecPos, vecDir, vecRight, vecUp, castsShadows, bTracePlayers ) == false )
	{
		return;
	}

	state.m_fHorizontalFOVDegrees = flFov;
	state.m_fVerticalFOVDegrees = flFov;

	state.m_Color[0] = vecBrightness.x;
	state.m_Color[1] = vecBrightness.y;
	state.m_Color[2] = vecBrightness.z;

	if ( pFlashlightTexture )
	{
		state.m_pSpotlightTexture = pFlashlightTexture;
		state.m_pProjectedMaterial = nullptr;
	}

	if( m_FlashlightHandle == CLIENTSHADOW_INVALID_HANDLE )
	{
		m_FlashlightHandle = g_pClientShadowMgr->CreateFlashlight( state );
	}
	else
	{
		if( !r_flashlightlockposition.GetBool() )
		{
			g_pClientShadowMgr->UpdateFlashlightState( m_FlashlightHandle, state );
		}
	}

	g_pClientShadowMgr->UpdateProjectedTexture( m_FlashlightHandle, true );

}

bool CFlashlightEffect::UpdateDefaultFlashlightState( FlashlightState_t& state, const Vector &vecPos, const Vector &vecForward,
														const Vector &vecRight, const Vector &vecUp, bool castsShadows, bool bTracePlayers )
{
	CVProfScope _( __FUNCTION__, 0, VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING, false, BUDGETFLAG_OTHER );

	if ( !m_bIsOn )
	{
		//		return;
	}

	if ( ComputeLightPosAndOrientation( vecPos, vecForward, vecRight, vecUp, state.m_vecLightOrigin, state.m_quatOrientation, bTracePlayers ) == false )
	{
		return false;
	}

	state.m_fQuadraticAtten = r_flashlightquadratic.GetFloat();

	bool bFlicker = false;

	if ( bFlicker == false )
	{
		if ( m_flLinearAtten > 0.0f )
		{
			state.m_fLinearAtten = m_flLinearAtten;
		}
		else
		{
			state.m_fLinearAtten = r_flashlightlinear.GetFloat();
		}

		if ( m_flFov > 0.0f )
		{
			state.m_fHorizontalFOVDegrees = m_flFov;
			state.m_fVerticalFOVDegrees = m_flFov;
		}
		else
		{
			state.m_fHorizontalFOVDegrees = r_flashlightfov.GetFloat();
			state.m_fVerticalFOVDegrees = r_flashlightfov.GetFloat();
		}

		if ( m_bMuzzleFlashEnabled )
		{
			state.m_fHorizontalFOVDegrees = state.m_fVerticalFOVDegrees = r_flashlightmuzzleflashfov.GetFloat();
		}
	}

	state.m_fConstantAtten = r_flashlightconstant.GetFloat();
	state.m_Color[0] = 1.0f;
	state.m_Color[1] = 1.0f;
	state.m_Color[2] = 1.0f;
	state.m_Color[3] = r_flashlightambient.GetFloat();

	state.m_NearZ = r_flashlightnear.GetFloat() + r_flashlightnearoffsetscale.GetFloat() * m_flCurrentPullBackDist;		// Optionally push near plane out so that we don't clip the world when the flashlight pulls back 
	if ( m_flFarZ > 0.0f )
	{
		state.m_FarZ = state.m_FarZAtten = m_flFarZ;	// Strictly speaking, these are different, but the game can treat them the same
	}
	else
	{
		state.m_FarZ = state.m_FarZAtten = r_flashlightfar.GetFloat();	// Strictly speaking, these are different, but the game can treat them the same
	}
	state.m_bEnableShadows = castsShadows && r_flashlightdepthtexture.GetBool();
	state.m_flShadowMapResolution = r_flashlightdepthres.GetInt();

	if ( m_bMuzzleFlashEnabled )
	{
		state.m_pSpotlightTexture = m_MuzzleFlashTexture;
		state.m_pProjectedMaterial = nullptr;
		state.m_Color[0] = m_flMuzzleFlashBrightness;
		state.m_Color[1] = m_flMuzzleFlashBrightness;
		state.m_Color[2] = m_flMuzzleFlashBrightness;
	}
	else
	{
		state.m_pSpotlightTexture = m_FlashlightTexture;
		state.m_pProjectedMaterial = nullptr;
	}

	state.m_nSpotlightTextureFrame = 0;

	state.m_flShadowAtten = r_flashlightshadowatten.GetFloat();
	state.m_flShadowSlopeScaleDepthBias = g_pMaterialSystemHardwareConfig->GetShadowSlopeScaleDepthBias();
	state.m_flShadowDepthBias = g_pMaterialSystemHardwareConfig->GetShadowDepthBias();

	return true;
}

void CFlashlightEffect::UpdateFlashlightTexture( const char* pTextureName )
{
	static const char *pEmptyString = "";

	if ( pTextureName == nullptr )
	{
		pTextureName = pEmptyString;
	}

	if ( !m_FlashlightTexture.IsValid() ||
		V_stricmp( m_textureName, pTextureName ) != 0 )
	{
		if ( pTextureName == pEmptyString )
		{
			m_FlashlightTexture.Init( "effects/flashlight001", TEXTURE_GROUP_OTHER, true );
		}
		else
		{
			m_FlashlightTexture.Init( pTextureName, TEXTURE_GROUP_OTHER, true );
		}
		V_strncpy( m_textureName, pTextureName, sizeof( m_textureName ) );
	}
}

bool CFlashlightEffect::ComputeLightPosAndOrientation( const Vector &vecPos, const Vector &vecForward, const Vector &vecRight, const Vector &vecUp,
														Vector& vecFinalPos, Quaternion& quatOrientation, bool bTracePlayers )
{
	const float flEpsilon = 0.1f;			// Offset flashlight position along vecUp
	float flDistCutoff = r_flashlighttracedistcutoff.GetFloat();
	const float flDistDrag = 0.2;
	bool bDebugVis = r_flashlightvisualizetrace.GetBool();

	CBasePlayer *pPlayer = UTIL_PlayerByIndex( m_nEntIndex );
	if ( !pPlayer )
	{
		pPlayer = CBasePlayer::GetLocalPlayer();
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}
	}

	// We will lock some of the flashlight params if player is on a ladder, to prevent oscillations due to the trace-rays
	bool bPlayerOnLadder = ( pPlayer->GetMoveType() == MOVETYPE_LADDER );

	CTraceFilterSkipPlayerAndViewModel traceFilter( pPlayer, bTracePlayers );

	//	Vector vOrigin = vecPos + r_flashlightoffsety.GetFloat() * vecUp;
	Vector vecOffset;
	pPlayer->GetFlashlightOffset( vecForward, vecRight, vecUp, &vecOffset );
	Vector vOrigin = vecPos + vecOffset;

	// Not on ladder...trace a hull
	if ( !bPlayerOnLadder ) 
	{
		Vector vecPlayerEyePos = pPlayer->GetRenderOrigin() + pPlayer->GetViewOffset();

		trace_t pmOriginTrace;
		UTIL_TraceHull( vecPlayerEyePos, vOrigin, Vector(-2, -2, -2), Vector(2, 2, 2), ( MASK_SOLID & ~(CONTENTS_HITBOX) ) | CONTENTS_WINDOW | CONTENTS_GRATE, &traceFilter, &pmOriginTrace );//1

		if ( bDebugVis )
		{
			debugoverlay->AddBoxOverlay( pmOriginTrace.endpos, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), QAngle( 0, 0, 0 ), 0, 255, 0, 16, 0 );
			if ( pmOriginTrace.DidHit() || pmOriginTrace.startsolid )
			{
				debugoverlay->AddLineOverlay( pmOriginTrace.startpos, pmOriginTrace.endpos, 255, 128, 128, true, 0 );
			}
			else
			{
				debugoverlay->AddLineOverlay( pmOriginTrace.startpos, pmOriginTrace.endpos, 255, 0, 0, true, 0 );
			}
		}

		if ( pmOriginTrace.DidHit() || pmOriginTrace.startsolid )
		{
			vOrigin = pmOriginTrace.endpos;
		}
		else
		{
			if ( pPlayer->m_vecFlashlightOrigin != vecPlayerEyePos )
			{
				vOrigin = vecPos;
			}
		}
	}
	else // on ladder...skip the above hull trace
	{
		vOrigin = vecPos;
	}

	// Now do a trace along the flashlight direction to ensure there is nothing within range to pull back from
	int iMask = MASK_OPAQUE_AND_NPCS;
	iMask &= ~CONTENTS_HITBOX;
	iMask |= CONTENTS_WINDOW | CONTENTS_GRATE | CONTENTS_IGNORE_NODRAW_OPAQUE;

	Vector vTarget = vOrigin + vecForward * r_flashlightfar.GetFloat();

	// Work with these local copies of the basis for the rest of the function
	Vector vDir   = vTarget - vOrigin;
	Vector vRight = vecRight;
	Vector vUp    = vecUp;
	VectorNormalize( vDir   );
	VectorNormalize( vRight );
	VectorNormalize( vUp    );

	// Orthonormalize the basis, since the flashlight texture projection will require this later...
	vUp -= DotProduct( vDir, vUp ) * vDir;
	VectorNormalize( vUp );
	vRight -= DotProduct( vDir, vRight ) * vDir;
	VectorNormalize( vRight );
	vRight -= DotProduct( vUp, vRight ) * vUp;
	VectorNormalize( vRight );

	contract_assert( fabs( DotProduct( vDir, vRight ) - 0.0f ) <= 1e-3 );
	contract_assert( fabs( DotProduct( vDir, vUp    ) - 0.0f ) <= 1e-3 );
	contract_assert( fabs( DotProduct( vRight, vUp  ) - 0.0f ) <= 1e-3 );

	trace_t pmDirectionTrace;
	UTIL_TraceHull( vOrigin, vTarget, Vector( -1.5, -1.5, -1.5 ), Vector( 1.5, 1.5, 1.5 ), iMask, &traceFilter, &pmDirectionTrace );//.5

	if ( bDebugVis )
	{
		debugoverlay->AddBoxOverlay( pmDirectionTrace.endpos, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ), QAngle( 0, 0, 0 ), 0, 0, 255, 16, 0 );
		debugoverlay->AddLineOverlay( vOrigin, pmDirectionTrace.endpos, 255, 0, 0, false, 0 );
	}

	float flTargetPullBackDist = 0.0f;
	float flDist = (pmDirectionTrace.endpos - vOrigin).Length();

	if ( flDist < flDistCutoff )
	{
		// We have an intersection with our cutoff range
		// Determine how far to pull back, then trace to see if we are clear
		float flPullBackDist = bPlayerOnLadder ? r_flashlightladderdist.GetFloat() : flDistCutoff - flDist;	// Fixed pull-back distance if on ladder

		flTargetPullBackDist = flPullBackDist;

		if ( !bPlayerOnLadder )
		{
			trace_t pmBackTrace;
			// start the trace away from the actual trace origin a bit, to avoid getting stuck on small, close "lips"
			UTIL_TraceHull( vOrigin - vDir * ( flDistCutoff * r_flashlightbacktraceoffset.GetFloat() ), vOrigin - vDir * ( flPullBackDist - flEpsilon ),
				Vector( -1.5f, -1.5f, -1.5f ), Vector( 1.5f, 1.5f, 1.5f ), iMask, &traceFilter, &pmBackTrace );

			if ( bDebugVis )
			{
				debugoverlay->AddLineOverlay( pmBackTrace.startpos, pmBackTrace.endpos, 255, 0, 255, true, 0 );
			}

			if( pmBackTrace.DidHit() )
			{
				// We have an intersection behind us as well, so limit our flTargetPullBackDist
				float flMaxDist = (pmBackTrace.endpos - vOrigin).Length() - flEpsilon;
				flTargetPullBackDist = std::min( flMaxDist, flTargetPullBackDist );
				//m_flCurrentPullBackDist = std::min( flMaxDist, m_flCurrentPullBackDist );	// possible pop
			}
		}
	}

	if ( bDebugVis )
	{
		// visualize pullback
		debugoverlay->AddBoxOverlay( vOrigin - vDir * m_flCurrentPullBackDist, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), QAngle( 0, 0, 0 ), 255, 255, 0, 16, 0 );
		debugoverlay->AddBoxOverlay( vOrigin - vDir * flTargetPullBackDist, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), QAngle( 0, 0, 0 ), 128, 128, 0, 16, 0 );
	}

	m_flCurrentPullBackDist = Lerp( flDistDrag, m_flCurrentPullBackDist, flTargetPullBackDist );
	m_flCurrentPullBackDist = std::min( m_flCurrentPullBackDist, flDistCutoff );	// clamp to max pullback dist
	vOrigin = vOrigin - vDir * m_flCurrentPullBackDist;

	vecFinalPos = vOrigin;
	BasisToQuaternion( vDir, vRight, vUp, quatOrientation );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFlashlightEffect::LightOff()
{

	// Clear out the light
	if( m_FlashlightHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( m_FlashlightHandle );
		m_FlashlightHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
}

CHeadlightEffect::CHeadlightEffect() 
{

}

CHeadlightEffect::~CHeadlightEffect()
{
	
}

void CHeadlightEffect::UpdateLight( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance )
{
	if ( IsOn() == false )
		 return;

	FlashlightState_t state;
	Vector basisX, basisY, basisZ;
	basisX = vecDir;
	basisY = vecRight;
	basisZ = vecUp;
	VectorNormalize(basisX);
	VectorNormalize(basisY);
	VectorNormalize(basisZ);

	BasisToQuaternion( basisX, basisY, basisZ, state.m_quatOrientation );
		
	state.m_vecLightOrigin = vecPos;

	state.m_fHorizontalFOVDegrees = 45.0f;
	state.m_fVerticalFOVDegrees = 30.0f;
	state.m_fQuadraticAtten = r_flashlightquadratic.GetFloat();
	state.m_fLinearAtten = r_flashlightlinear.GetFloat();
	state.m_fConstantAtten = r_flashlightconstant.GetFloat();
	state.m_Color[0] = 1.0f;
	state.m_Color[1] = 1.0f;
	state.m_Color[2] = 1.0f;
	state.m_Color[3] = r_flashlightambient.GetFloat();
	state.m_NearZ = r_flashlightnear.GetFloat();
	state.m_FarZ = r_flashlightfar.GetFloat();
	state.m_bEnableShadows = true;
	state.m_pSpotlightTexture = m_FlashlightTexture;
	state.m_pProjectedMaterial = nullptr;
	state.m_nSpotlightTextureFrame = 0;
	
	if( GetFlashlightHandle() == CLIENTSHADOW_INVALID_HANDLE )
	{
		SetFlashlightHandle( g_pClientShadowMgr->CreateFlashlight( state ) );
	}
	else
	{
		g_pClientShadowMgr->UpdateFlashlightState( GetFlashlightHandle(), state );
	}
	
	g_pClientShadowMgr->UpdateProjectedTexture( GetFlashlightHandle(), true );
}

void CFlashlightEffectManager::TurnOnFlashlight( int nEntIndex, const char *pszTextureName, float flFov, float flFarZ, float flLinearAtten)
{
	m_pFlashlightTextureName = pszTextureName;
	m_nFlashlightEntIndex = nEntIndex;
	m_flFov = flFov;
	m_flFarZ = flFarZ;
	m_flLinearAtten = flLinearAtten;
	m_bFlashlightOn = true;

	if ( m_bFlashlightOverride )
	{
		// somebody is overriding the flashlight. We're keeping around the params to restore it later.
		return;
	}

	if ( !m_pFlashlightEffect )
	{
		if( pszTextureName )
		{
			m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex, pszTextureName, flFov, flFarZ, flLinearAtten );
		}
		else
		{
			m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex );
		}

		if( !m_pFlashlightEffect )
		{
			return;
		}
	}

	m_pFlashlightEffect->TurnOn();
}

void CFlashlightEffectManager::TurnOffFlashlight( bool bForce)
{
	m_pFlashlightTextureName = nullptr;
	m_bFlashlightOn = false;

	if ( bForce )
	{
		m_bFlashlightOverride = false;
		m_nMuzzleFlashFrameCountdown = 0;
		m_muzzleFlashTimer.Invalidate();
		delete m_pFlashlightEffect;
		m_pFlashlightEffect = nullptr;
		return;
	}

	if ( m_bFlashlightOverride )
	{
		// don't mess with it while it's overridden
		return;
	}

	if( m_nMuzzleFlashFrameCountdown == 0 && m_muzzleFlashTimer.IsElapsed() )
	{
		delete m_pFlashlightEffect;
		m_pFlashlightEffect = nullptr;
	}
}

void CFlashlightEffectManager::UpdateFlashlight( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov, bool castsShadows,
		float flFarZ, float flLinearAtten, const char* pTextureName)
{
	if ( m_bFlashlightOverride )
	{
		// don't mess with it while it's overridden
		return;
	}

	bool bMuzzleFlashActive = ( m_nMuzzleFlashFrameCountdown > 0 ) || !m_muzzleFlashTimer.IsElapsed();

	if ( m_pFlashlightEffect )
	{
		m_flFov = flFov;
		m_flFarZ = flFarZ;
		m_flLinearAtten = flLinearAtten;
		m_pFlashlightEffect->UpdateLight( m_nFlashlightEntIndex, vecPos, vecDir, vecRight, vecUp, flFov, flFarZ, flLinearAtten, castsShadows, pTextureName );
		m_pFlashlightEffect->SetMuzzleFlashEnabled( bMuzzleFlashActive, m_flMuzzleFlashBrightness );
	}

	if ( !bMuzzleFlashActive && !m_bFlashlightOn && m_pFlashlightEffect )
	{
		delete m_pFlashlightEffect;
		m_pFlashlightEffect = nullptr;
	}

	if ( bMuzzleFlashActive && !m_bFlashlightOn && !m_pFlashlightEffect )
	{
		m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex );
		m_pFlashlightEffect->SetMuzzleFlashEnabled( bMuzzleFlashActive, m_flMuzzleFlashBrightness );
	}

	if ( bMuzzleFlashActive && m_nFXComputeFrame != gpGlobals->framecount )
	{
		m_nFXComputeFrame = gpGlobals->framecount;
		m_nMuzzleFlashFrameCountdown--;
	}
}

void CFlashlightEffectManager::TriggerMuzzleFlash()
{
	m_nMuzzleFlashFrameCountdown = 2;
	m_muzzleFlashTimer.Start( 0.066f );		// show muzzleflash for 2 frames or 66ms, whichever is longer
	m_flMuzzleFlashBrightness = random_valve->RandomFloat( 0.4f, 2.0f );
}

void CFlashlightEffectManager::EnableFlashlightOverride( bool bEnable)
{
	m_bFlashlightOverride = bEnable;

	if ( !m_bFlashlightOverride )
	{
		// make sure flashlight is in its original state
		if ( m_bFlashlightOn && m_pFlashlightEffect == nullptr )
		{
			TurnOnFlashlight( m_nFlashlightEntIndex, m_pFlashlightTextureName, m_flFov, m_flFarZ, m_flLinearAtten );
		}
		else if ( !m_bFlashlightOn && m_pFlashlightEffect )
		{
			delete m_pFlashlightEffect;
			m_pFlashlightEffect = nullptr;
		}
	}
}

void CFlashlightEffectManager::UpdateFlashlightOverride(	bool bFlashlightOn, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp,
									float flFov, bool castsShadows, ITexture *pFlashlightTexture, const Vector &vecBrightness)
{
	contract_assert( m_bFlashlightOverride );
	if ( !m_bFlashlightOverride )
	{
		return;
	}

	if ( bFlashlightOn && !m_pFlashlightEffect )
	{
		m_pFlashlightEffect = new CFlashlightEffect( m_nFlashlightEntIndex );			
	}
	else if ( !bFlashlightOn && m_pFlashlightEffect )
	{
		delete m_pFlashlightEffect;
		m_pFlashlightEffect = nullptr;
	}

	if( m_pFlashlightEffect )
	{
		m_pFlashlightEffect->UpdateLight( m_nFlashlightEntIndex, vecPos, vecDir, vecRight, vecUp, flFov, castsShadows, pFlashlightTexture, vecBrightness, false );
	}
}
