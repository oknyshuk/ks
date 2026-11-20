// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "collisionutils.h"
#include "eiface.h"
#include "engine/IEngineTrace.h"
#include "inetchannelinfo.h"
#include "irecipientfilter.h"
#include "networkvar.h"
#include "shake.h"
#include "studio.h"
#include "usermessages_schema.h"
#include "worldsize.h"
#include "shareddefs.h"
#include "declare_class.h"
#include "dt_send.h"
#include "mathlib/IceKey.H"
#include "model_types.h"
#include "vphysics/object_hash.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.platform;
import ks.core.tslist;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.game.entity_cast;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.playerid;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :player;
import :baseentity;
import :vscript;
import :fish;
import :sharedvar;
import :nav_4;
import :entity_kind;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :entity_2;
import ks.core.cmdline;
import ks.core.random;
import ks.core.scratcharena;
import ks.math.vplane;
import ks.tier1.checksum_crc;
import ks.math.ssequaternion;

class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CRecipientFilter;
class IEntityFactory;

using namespace ::ks::reflect;

// Purpose: Utility code.

extern int		g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud
extern int		g_sModelIndexBloodDrop;		// (in combatweapon.cpp) holds the sprite index for the initial blood
extern int		g_sModelIndexBloodSpray;	// (in combatweapon.cpp) holds the sprite index for splattered blood

// this is true if the engine should be sent log output.  Once per frame it is rechecked to see if logging has been enabled.
bool g_bIsLogging = true;

#ifdef	DEBUG
void DBG_AssertFunction( bool fExpr, const char *szExpr, const char *szFile, int szLine, const char *szMessage )
{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != nullptr)
		V_snprintf(szOut,sizeof(szOut), "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		V_snprintf(szOut,sizeof(szOut), "ASSERT FAILED:\n %s \n(%s@%d)\n", szExpr, szFile, szLine);
	Warning( "%s", szOut);
}
#endif	// DEBUG

//-----------------------------------------------------------------------------
// Entity creation factory
//-----------------------------------------------------------------------------
class CEntityFactoryDictionary : public IEntityFactoryDictionary
{
public:
	CEntityFactoryDictionary();

	virtual void InstallFactory( IEntityFactory *pFactory, const char *pClassName );
	virtual IServerNetworkable *Create( const char *pClassName );
	virtual void Destroy( const char *pClassName, IServerNetworkable *pNetworkable );
	virtual const char *GetCannonicalName( const char *pClassName );
	void ReportEntitySizes();

private:
	IEntityFactory *FindFactory( const char *pClassName );
public:
	ks::idict< IEntityFactory * > m_Factories;
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
IEntityFactoryDictionary *EntityFactoryDictionary()
{
	static CEntityFactoryDictionary s_EntityFactory;
	return &s_EntityFactory;
}

void DumpEntityFactories_f()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CEntityFactoryDictionary *dict = ( CEntityFactoryDictionary * )EntityFactoryDictionary();
	if ( dict )
	{
		for ( const auto &[name, pFactory] : dict->m_Factories )
		{
			Warning( "%s\n", name.c_str() );
		}
	}
}

static ConCommand dumpentityfactories( side_registry(), "dumpentityfactories", DumpEntityFactories_f, "Lists all entity factory names.", FCVAR_GAMEDLL );

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static void dump_entity_sizes( const CCommand &args );
static ConCommand dump_entity_sizes_command( side_registry(), "dump_entity_sizes", dump_entity_sizes, "Print sizeof(entclass)" );
static void dump_entity_sizes( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	((CEntityFactoryDictionary*)EntityFactoryDictionary())->ReportEntitySizes();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEntityFactoryDictionary::CEntityFactoryDictionary()
{
}

//-----------------------------------------------------------------------------
// Finds a new factory
//-----------------------------------------------------------------------------
IEntityFactory *CEntityFactoryDictionary::FindFactory( const char *pClassName )
{
	IEntityFactory **ppFactory = ks::find_ptr( m_Factories, pClassName );
	return ppFactory ? *ppFactory : nullptr;
}

//-----------------------------------------------------------------------------
// Install a new factory
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::InstallFactory( IEntityFactory *pFactory, const char *pClassName )
{
	m_Factories.emplace( pClassName, pFactory );
}

//-----------------------------------------------------------------------------
// Instantiate something using a factory
//-----------------------------------------------------------------------------
IServerNetworkable *CEntityFactoryDictionary::Create( const char *pClassName )
{
	IEntityFactory *pFactory = FindFactory( pClassName );
	if ( !pFactory )
	{
		Warning("Attempted to create unknown entity type %s!\n", pClassName );
		return nullptr;
	}
	return pFactory->Create( pClassName );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
const char *CEntityFactoryDictionary::GetCannonicalName( const char *pClassName )
{
	auto it = m_Factories.find( pClassName );
	return it != m_Factories.end() ? it->first.c_str() : nullptr;
}

//-----------------------------------------------------------------------------
// Destroy a networkable
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::Destroy( const char *pClassName, IServerNetworkable *pNetworkable )
{
	IEntityFactory *pFactory = FindFactory( pClassName );
	if ( !pFactory )
	{
		Warning("Attempted to destroy unknown entity type %s!\n", pClassName );
		return;
	}

	pFactory->Destroy( pNetworkable );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::ReportEntitySizes()
{
	for ( const auto &[name, pFactory] : m_Factories )
	{
		Msg( " %s: %d", name.c_str(), (int)pFactory->GetEntitySize() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int UTIL_PrecacheDecal( const char *name, bool preload )
{
	// If this is out of order, make sure to warn.
	if ( !CBaseEntity::IsPrecacheAllowed() )
	{
		if ( !engine->IsDecalPrecached( name ) )
		{
			contract_assert( !"UTIL_PrecacheDecal:  too late" );

			Warning( "Late precache of %s\n", name );
		}
	}

	return engine->PrecacheDecal( name, preload );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float UTIL_GetSimulationInterval()
{
	if ( CBaseEntity::IsSimulatingOnAlternateTicks() )
		return ( TICK_INTERVAL * 2.0 );
	return TICK_INTERVAL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int UTIL_EntitiesInBox( const Vector &mins, const Vector &maxs, CFlaggedEntitiesEnum *pEnum )
{
	::partition->EnumerateElementsInBox( PARTITION_ENGINE_NON_STATIC_EDICTS, mins, maxs, false, pEnum );
	return pEnum->GetCount();
}

int UTIL_EntitiesInSphere( const Vector &center, float radius, CFlaggedEntitiesEnum *pEnum )
{
	::partition->EnumerateElementsInSphere( PARTITION_ENGINE_NON_STATIC_EDICTS, center, radius, false, pEnum );
	return pEnum->GetCount();
}

CEntitySphereQuery::CEntitySphereQuery( const Vector &center, float radius, int flagMask )
{
	m_listIndex = 0;
	m_listCount = UTIL_EntitiesInSphere( m_pList, std::size( m_pList ), center, radius, flagMask );
}

CBaseEntity *CEntitySphereQuery::GetCurrentEntity()
{
	if ( m_listIndex < m_listCount )
		return m_pList[m_listIndex];
	return nullptr;
}

//-----------------------------------------------------------------------------
// Simple trace filter
//-----------------------------------------------------------------------------
class CTracePassFilter : public CTraceFilter
{
public:
	CTracePassFilter( IHandleEntity *pPassEnt ) : m_pPassEnt( pPassEnt ) {}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
			return false;

		if (!PassServerEntityFilter( pHandleEntity, m_pPassEnt ))
			return false;

		return true;
	}

private:
	IHandleEntity *m_pPassEnt;
};

//-----------------------------------------------------------------------------
// Drops an entity onto the floor
//-----------------------------------------------------------------------------
int UTIL_DropToFloor( CBaseEntity *pEntity, unsigned int mask, CBaseEntity *pIgnore)
{
	// Assume no ground
	pEntity->SetGroundEntity( nullptr );

	contract_assert( pEntity );

	trace_t	trace;
	// HACK: is this really the only sure way to detect crossing a terrain boundry?
	UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), mask, pIgnore, pEntity->GetCollisionGroup(), &trace );
	if (trace.fraction == 0.0)
		return -1;

	UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin() - Vector(0,0,256), mask, pIgnore, pEntity->GetCollisionGroup(), &trace );

	if (trace.allsolid)
		return -1;

	if (trace.fraction == 1)
		return 0;

	pEntity->SetAbsOrigin( trace.endpos );
	pEntity->SetGroundEntity( trace.Ent<CBaseEntity>() );

	return 1;
}

//-----------------------------------------------------------------------------
// Returns false if any part of the bottom of the entity is off an edge that
// is not a staircase.
//-----------------------------------------------------------------------------
bool UTIL_CheckBottom( CBaseEntity *pEntity, ITraceFilter *pTraceFilter, float flStepSize )
{
	Vector	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;

	contract_assert( pEntity );

	CTracePassFilter traceFilter(pEntity);
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilter;
	}

	unsigned int mask = pEntity->PhysicsSolidMaskForEntity();

	VectorAdd (pEntity->GetAbsOrigin(), pEntity->WorldAlignMins(), mins);
	VectorAdd (pEntity->GetAbsOrigin(), pEntity->WorldAlignMaxs(), maxs);

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
	{
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (enginetrace->GetPointContents(start) != CONTENTS_SOLID)
				goto realcheck;
		}
	}
	return true;		// we got out easy

realcheck:
	// check it for real...
	start[2] = mins[2] + flStepSize; // seems to help going up/down slopes.
	
	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*flStepSize;
	
	UTIL_TraceLine( start, stop, mask, pTraceFilter, &trace );

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

	// the corners must be within 16 of the midpoint	
	for	(x=0 ; x<=1 ; x++)
	{
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];
			
			UTIL_TraceLine( start, stop, mask, pTraceFilter, &trace );
			
			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > flStepSize)
				return false;
		}
	}
	return true;
}

bool g_bDisableEhandleAccess = false;
bool g_bReceivedChainedUpdateOnRemove = false;
//-----------------------------------------------------------------------------
// Purpose: Sets the entity up for deletion.  Entity will not actually be deleted
//			until the next frame, so there can be no pointer errors.
// Input  : *oldObj - object to delete
//-----------------------------------------------------------------------------
void UTIL_Remove( IServerNetworkable *oldObj )
{
	CServerNetworkProperty* pProp = static_cast<CServerNetworkProperty*>( oldObj );
	if ( !pProp || pProp->IsMarkedForDeletion() )
		return;

	if ( PhysIsInCallback() )
	{
		// This assert means that someone is deleting an entity inside a callback.  That isn't supported so
		// this code will defer the deletion of that object until the end of the current physics simulation frame
		// Since this is hidden from the calling code it's preferred to call PhysCallbackRemove() directly from the caller
		// in case the deferred delete will have unwanted results (like continuing to receive callbacks).  That will make it 
		// obvious why the unwanted results are happening so the caller can handle them appropriately. (some callbacks can be masked 
		// or the calling entity can be flagged to filter them in most cases)
		contract_assert( 0 );
		PhysCallbackRemove(oldObj);
		return;
	}

	// mark it for deletion	
	pProp->MarkForDeletion( );

	CBaseEntity *pBaseEnt = EntityCast<CBaseEntity>( oldObj->GetBaseEntity() );
	if ( pBaseEnt )
	{
		g_bReceivedChainedUpdateOnRemove = false;
		pBaseEnt->UpdateOnRemove();

		contract_assert( g_bReceivedChainedUpdateOnRemove );

		// clear oldObj targetname / other flags now
		pBaseEnt->SetName( NULL_STRING );

	}

	gEntList.AddToDeleteList( oldObj );
}

void UTIL_Remove( CBaseEntity *oldObj )
{
	if ( !oldObj )
		return;
	UTIL_Remove( oldObj->NetworkProp() );
}

static int s_RemoveImmediateSemaphore = 0;
void UTIL_DisableRemoveImmediate()
{
	s_RemoveImmediateSemaphore++;
}
void UTIL_EnableRemoveImmediate()
{
	s_RemoveImmediateSemaphore--;
	contract_assert( s_RemoveImmediateSemaphore>=0 );
}
//-----------------------------------------------------------------------------
// Purpose: deletes an entity, without any delay.  WARNING! Only use this when sure
//			no pointers rely on this entity.
// Input  : *oldObj - the entity to delete
//-----------------------------------------------------------------------------
void UTIL_RemoveImmediate( CBaseEntity *oldObj )
{
	// valid pointer or already removed?
	if ( !oldObj || oldObj->IsEFlagSet(EFL_KILLME) )
		return;

	if ( s_RemoveImmediateSemaphore )
	{
		UTIL_Remove(oldObj);
		return;
	}

	oldObj->AddEFlags( EFL_KILLME );	// Make sure to ignore further calls into here or UTIL_Remove.

	g_bReceivedChainedUpdateOnRemove = false;
	oldObj->UpdateOnRemove();
	contract_assert( g_bReceivedChainedUpdateOnRemove );

	// Entities shouldn't reference other entities in their destructors
	//  that type of code should only occur in an UpdateOnRemove call
	g_bDisableEhandleAccess = true;
	delete oldObj;
	g_bDisableEhandleAccess = false;

}

// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBasePlayer	*UTIL_PlayerByIndex( int playerIndex )
{
	CBasePlayer *pPlayer = nullptr;

	if ( playerIndex > 0 && playerIndex <= gpGlobals->maxClients )
	{
		edict_t *pPlayerEdict = INDEXENT( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->IsFree() )
		{
			pPlayer = (CBasePlayer*)GetContainingEntity( pPlayerEdict );
		}
	}
	
	return pPlayer;
}

CBasePlayer* UTIL_PlayerByName( const char *name )
{
	if ( !name || !name[0] )
		return nullptr;

	for (int i = 1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		
		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		if ( V_stricmp( pPlayer->GetPlayerName(), name ) == 0 )
		{
			return pPlayer;
		}
	}
	
	return nullptr;
}

CBasePlayer* UTIL_PlayerByUserId( int userID )
{
	for (int i = 1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		
		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		if ( engine->GetPlayerUserId(pPlayer->edict()) == userID )
		{
			return pPlayer;
		}
	}
	
	return nullptr;
}

CBasePlayer* UTIL_PlayerByAccountID( AccountID_t accountID )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected( ) )
			continue;

		if ( engine->GetClientPlayerID( pPlayer->edict( ) ) && 
			( engine->GetClientPlayerID( pPlayer->edict( ) )->GetAccountID( ) == accountID ) )
		{
			return pPlayer;
		}
	}

	return nullptr;
}

//
// Return the local player.
// If this is a multiplayer game, return NULL.
// 
CBasePlayer *UTIL_GetLocalPlayer( void )
{
	if ( gpGlobals->maxClients > 1 )
	{
		if ( developer.GetBool() )
		{
			contract_assert( !"UTIL_GetLocalPlayer" );
			
#ifdef	DEBUG
			Warning( "UTIL_GetLocalPlayer() called in multiplayer game.\n" );
#endif
		}

		return nullptr;
	}

	return UTIL_PlayerByIndex( 1 );
}

//
// Get the local player on a listen server - this is for multiplayer use only
// 
CBasePlayer *UTIL_GetListenServerHost( void )
{
	// no "local player" if this is a dedicated server or a single player game
	if (engine->IsDedicatedServer())
	{
		contract_assert( !"UTIL_GetListenServerHost" );
		Warning( "UTIL_GetListenServerHost() called from a dedicated server or single-player game.\n" );
		return nullptr;
	}

	return UTIL_PlayerByIndex( 1 );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if the command was issued by the listenserver host, or by the dedicated server, via rcon or the server console.
 * This is valid during ConCommand execution.
 */
bool UTIL_IsCommandIssuedByServerAdmin( void )
{
	int issuingPlayerIndex = UTIL_GetCommandClientIndex();

	if ( engine->IsDedicatedServer() )
	{
		if ( issuingPlayerIndex > 0 )
			return false;
	}
	else
	{
		if ( issuingPlayerIndex != engine->GetLocalClientIndex() ) // comparing 1-based client index, also called Player Index in some places
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns a CBaseEntity pointer by entindex.  Index is 1 based.
 */
CBaseEntity	*UTIL_EntityByIndex( int entityIndex )
{
	CBaseEntity *entity = nullptr;

	if ( entityIndex > 0 )
	{
		edict_t *edict = INDEXENT( entityIndex );
		if ( edict && !edict->IsFree() )
		{
			entity = GetContainingEntity( edict );
		}
	}
	
	return entity;
}

int ENTINDEX( CBaseEntity *pEnt )
{
	// This works just like ENTINDEX for edicts.
	if ( pEnt )
		return pEnt->entindex();
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerIndex - 
//			ping - 
//			packetloss - 
//-----------------------------------------------------------------------------
void UTIL_GetPlayerConnectionInfo( int playerIndex, int& ping, int &packetloss )
{
	CBasePlayer *player =  UTIL_PlayerByIndex( playerIndex );
	if ( player->IsSplitScreenPlayer() && 
		 player->GetSplitScreenPlayerOwner() )
	{
		player = player->GetSplitScreenPlayerOwner();
		playerIndex = player->entindex();
	}

	INetChannelInfo *nci = engine->GetPlayerNetInfo(playerIndex);

	if ( nci && player && !player->IsBot() )
	{
		float latency = nci->GetAvgLatency( FLOW_OUTGOING ); // in seconds
		
		// that should be the correct latency, we assume that cmdrate is higher 
		// then updaterate, what is the case for default settings
		const char * szCmdRate = engine->GetClientConVarValue( playerIndex, "cl_cmdrate" );
		const char * szUpdateRate = engine->GetClientConVarValue( playerIndex, "cl_updaterate" );

		static const ConVar *pSvClientCmdrateDifference = g_pCVar->FindVar( "sv_client_cmdrate_difference" );
		static const ConVar *pMinUpdateRate = g_pCVar->FindVar( "sv_minupdaterate" );
		static const ConVar *pMaxUpdateRate = g_pCVar->FindVar( "sv_maxupdaterate" );

		float flUpdateRateValue = V_atof( szUpdateRate );
		if ( !player->IsHLTV() )
		{
			if ( pMinUpdateRate && pMaxUpdateRate )
				flUpdateRateValue = clamp( flUpdateRateValue, pMinUpdateRate->GetFloat(), pMaxUpdateRate->GetFloat() );
		}
		
		float flCmdRateValue = V_atof( szCmdRate );
		if ( player->IsHLTV() )
		{
			flCmdRateValue = flUpdateRateValue;
		}
		else
		{
			// First, we make it stay within range of cl_updaterate.
			if ( pSvClientCmdrateDifference )
			{
				float diff = flCmdRateValue - flUpdateRateValue;
				float diffMaxCap = pSvClientCmdrateDifference->GetFloat();
				if ( fabs( diff ) > diffMaxCap )
				{
					if ( diff > 0 )
						flCmdRateValue = flUpdateRateValue + diffMaxCap;
					else
						flCmdRateValue = flUpdateRateValue - diffMaxCap;
				}
			}
			static const ConVar *pMinCmdRate = g_pCVar->FindVar( "sv_mincmdrate" );
			static const ConVar *pMaxCmdRate = g_pCVar->FindVar( "sv_maxcmdrate" );
			if ( pMinUpdateRate && pMaxUpdateRate )
				flCmdRateValue = clamp( flCmdRateValue, pMinCmdRate->GetFloat(), pMaxCmdRate->GetFloat() );
		}

		latency -= (0.5f/flCmdRateValue) + TICKS_TO_TIME( 1.0f ); // correct latency

		// in GoldSrc we had a different, not fixed tickrate. so we have to adjust
		// Source pings by half a tick to match the old GoldSrc pings.
		latency -= TICKS_TO_TIME( 0.5f );

		ping = latency * 1000.0f; // as msecs
		ping = clamp( ping, 5, 1000 ); // set bounds, dont show pings under 5 msecs
		
		packetloss = 100.0f * nci->GetAvgLoss( FLOW_INCOMING ); // loss in percentage
		packetloss = clamp( packetloss, 0, 100 );
	}
	else
	{
		ping = 0;
		packetloss = 0;
	}
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}

//-----------------------------------------------------------------------------
// Compute shake amplitude
//-----------------------------------------------------------------------------
inline float ComputeShakeAmplitude( const Vector &center, const Vector &shakePt, float amplitude, float radius ) 
{
	if ( radius <= 0 )
		return amplitude;

	float localAmplitude = -1;
	Vector delta = center - shakePt;
	float distance = delta.Length();

	if ( distance <= radius )
	{
		// Make the amplitude fall off over distance
		float flPerc = 1.0 - (distance / radius);
		localAmplitude = amplitude * flPerc;
	}

	return localAmplitude;
}

//-----------------------------------------------------------------------------
// Transmits the actual shake event
//-----------------------------------------------------------------------------
inline void TransmitShakeEvent( CBasePlayer *pPlayer, float localAmplitude, float frequency, float duration, ShakeCommand_t eCommand )
{
	if (( localAmplitude > 0 ) || ( eCommand == SHAKE_STOP ))
	{
		if ( eCommand == SHAKE_STOP )
			localAmplitude = 0;

		CSingleUserRecipientFilter user( pPlayer );
		user.MakeReliable();
		ks::net::CCSUsrMsg_Shake msg;
		msg.command = eCommand;					// shake command (SHAKE_START, STOP, FREQUENCY, AMPLITUDE)
		msg.local_amplitude = localAmplitude;		// shake magnitude/amplitude
		msg.frequency = frequency;				// shake noise frequency
		msg.duration = duration;					// shake lasts this long

		SendUserMessage( user, ks::net::CS_UM_Shake, msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shake the screen of all clients within radius.
//			radius == 0, shake all clients
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
// Input  : center - Center of screen shake, radius is measured from here.
//			amplitude - Amplitude of shake
//			frequency - 
//			duration - duration of shake in seconds.
//			radius - Radius of effect, 0 shakes all clients.
//			command - One of the following values:
//				SHAKE_START - starts the screen shake for all players within the radius
//				SHAKE_STOP - stops the screen shake for all players within the radius
//				SHAKE_AMPLITUDE - modifies the amplitude of the screen shake
//									for all players within the radius
//				SHAKE_FREQUENCY - modifies the frequency of the screen shake
//									for all players within the radius
//			bAirShake - if this is false, then it will only shake players standing on the ground.
//-----------------------------------------------------------------------------
static const float MAX_SHAKE_AMPLITUDE = 16.0f;
void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake, std::vector< CBasePlayer * > *ignore )
{
	int			i;
	float		localAmplitude;

	if ( amplitude > MAX_SHAKE_AMPLITUDE )
	{
		amplitude = MAX_SHAKE_AMPLITUDE;
	}
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only start shakes for players that are on the ground unless doing an air shake.
		//
		if ( !pPlayer || (!bAirShake && (eCommand == SHAKE_START) && !(pPlayer->GetFlags() & FL_ONGROUND)) )
		{
			continue;
		}

		if ( ignore && ks::contains( *ignore, pPlayer ) )
		{
			continue;
		}

		localAmplitude = ComputeShakeAmplitude( center, pPlayer->WorldSpaceCenter(), amplitude, radius );

		// This happens if the player is outside the radius, in which case we should ignore 
		// all commands
		if (localAmplitude < 0)
			continue;

		TransmitShakeEvent( pPlayer, localAmplitude, frequency, duration, eCommand );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shake an object and all players on or near it
//-----------------------------------------------------------------------------
void UTIL_ScreenShakeObject( CBaseEntity *pEnt, const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake )
{
	int			i;
	float		localAmplitude;

	CBaseEntity *pHighestParent = pEnt->GetRootMoveParent();
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if (!pPlayer)
			continue;

		// Shake the object, or anything hierarchically attached to it at maximum amplitude
		localAmplitude = 0;
		if (pHighestParent == pPlayer->GetRootMoveParent())
		{
			localAmplitude = amplitude;
		}
		else if ((pPlayer->GetFlags() & FL_ONGROUND) && pPlayer->GetGroundEntity() && (pPlayer->GetGroundEntity()->GetRootMoveParent() == pHighestParent))
		{
			// If the player is standing on the object, use maximum amplitude
			localAmplitude = amplitude;
		}
		else
		{
			// Only shake players that are on the ground.
			if ( !bAirShake && !(pPlayer->GetFlags() & FL_ONGROUND) )
			{
				continue;
			}

			localAmplitude = ComputeShakeAmplitude( center, pPlayer->WorldSpaceCenter(), amplitude, radius );

			// This happens if the player is outside the radius, 
			// in which case we should ignore all commands
			if (localAmplitude < 0)
				continue;
		}

		TransmitShakeEvent( (CBasePlayer *)pPlayer, localAmplitude, frequency, duration, eCommand );
	}
}

//-----------------------------------------------------------------------------
// Transmits the actual tilt event
//-----------------------------------------------------------------------------
inline void TransmitTiltEvent( CBasePlayer *pPlayer, QAngle tiltAngle, float duration, float tiltTime, ShakeCommand_t eCommand, bool bEaseInOut )
{
	// Gurjeets - Not used in csgo
	//CSingleUserRecipientFilter user( pPlayer );
	//user.MakeReliable();
	//UserMessageBegin( user, "Tilt" );
	//WRITE_BYTE( eCommand );				// tilt command (SHAKE_START, STOP, FREQUENCY, AMPLITUDE)
	//WRITE_BYTE( bEaseInOut );			// tilt ease in/out
	//WRITE_FLOAT( tiltAngle.x );			// tilt angle
	//WRITE_FLOAT( tiltAngle.y );
	//WRITE_FLOAT( tiltAngle.z );
	//WRITE_FLOAT( duration );			// tilt lasts this long
	//WRITE_FLOAT( tiltTime );			// tilt time
	//MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: Tilt the screen of all clients within radius.
//			radius == 0, shake all clients
// Input  : center - Center of screen tilt, radius is measured from here.
//			tiltAngle - Angle that the world is pretending to tilt
//			duration - duration of tilt in seconds.
//			radius - Radius of effect, 0 shakes all clients.
//			tiltTime - how long it takes to reach full tilt
//			command - One of the following values:
//				SHAKE_START - starts the screen tilt for all players within the radius
//				SHAKE_STOP - stops the screen tilt for all players within the radius
//				SHAKE_AMPLITUDE - modifies the amplitude of the screen tilt
//									for all players within the radius
//				SHAKE_FREQUENCY - modifies the frequency of the screen tilt
//									for all players within the radius
//			bAirShake - if this is false, then it will only tilt players standing on the ground.
//-----------------------------------------------------------------------------
void UTIL_ScreenTilt( const Vector &center, const QAngle &tiltAngle, float duration, float radius, float tiltTime, ShakeCommand_t eCommand, bool bEaseInOut )
{
	int			i;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only start shakes for players that are on the ground unless doing an air shake.
		//
		if ( !pPlayer )
		{
			continue;
		}

		// This happens if the player is outside the radius, in which case we should ignore 
		// all commands
		if ( radius != 0.0f && pPlayer->WorldSpaceCenter().DistTo( center ) > radius )
			continue;

		TransmitTiltEvent( (CBasePlayer *)pPlayer, tiltAngle, duration, tiltTime, eCommand, bEaseInOut );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Punches the view of all clients within radius.
//			If radius is 0, punches all clients.
// Input  : center - Center of punch, radius is measured from here.
//			radius - Radius of effect, 0 punches all clients.
//			bInAir - if this is false, then it will only punch players standing on the ground.
//-----------------------------------------------------------------------------
void UTIL_ViewPunch( const Vector &center, QAngle angPunch, float radius, bool bInAir )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only apply the punch to players that are on the ground unless doing an air punch.
		//
		if ( !pPlayer || (!bInAir && !(pPlayer->GetFlags() & FL_ONGROUND)) )
		{
			continue;
		}

		QAngle angTemp = angPunch;

		if ( radius > 0 )
		{
			Vector delta = center - pPlayer->GetAbsOrigin();
			float distance = delta.Length();

			if ( distance <= radius )
			{
				// Make the punch amplitude fall off over distance.
				float flPerc = 1.0 - (distance / radius);
				angTemp *= flPerc;
			}
			else
			{
				continue;
			}
		}
		
		pPlayer->ViewPunch( angTemp );
	}
}

void UTIL_ScreenFadeBuild( ScreenFade_t &fade, const color32 &color, float fadeTime, float fadeHold, int flags )
{
	fade.duration = FixedUnsigned16( fadeTime, 1<<SCREENFADE_FRACBITS );		// 7.9 fixed
	fade.holdTime = FixedUnsigned16( fadeHold, 1<<SCREENFADE_FRACBITS );		// 7.9 fixed
	fade.r = color.r;
	fade.g = color.g;
	fade.b = color.b;
	fade.a = color.a;
	fade.fadeFlags = flags;
}

void UTIL_ScreenFadeWrite( const ScreenFade_t &fade, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	CBasePlayer *pRecipient = static_cast< CBasePlayer * >( pEntity );

	if ( pRecipient->ShouldThrottleUserMessage( "Fade" ) )
	{
		return;
	}

	CSingleUserRecipientFilter user( pRecipient );
	user.MakeReliable();

	ks::net::CCSUsrMsg_Fade msg;

	msg.duration = fade.duration;		// fade lasts this long
	msg.hold_time = fade.holdTime;		// fade lasts this long
	msg.flags = fade.fadeFlags;		// fade type (in / out)
	msg.clr.mut().r = fade.r;				// fade red
	msg.clr.mut().g = fade.g;				// fade green
	msg.clr.mut().b = fade.b;				// fade blue
	msg.clr.mut().a = fade.a;				// fade blue

	SendUserMessage( user, ks::net::CS_UM_Fade, msg );		
}

void UTIL_ScreenFadeAll( const color32 &color, float fadeTime, float fadeHold, int flags )
{
	int			i;
	ScreenFade_t	fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, flags );

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
	
		UTIL_ScreenFadeWrite( fade, pPlayer );
	}
}

void UTIL_ScreenFade( CBaseEntity *pEntity, const color32 &color, float fadeTime, float fadeHold, int flags )
{
	ScreenFade_t	fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, flags );
	UTIL_ScreenFadeWrite( fade, pEntity );
}

void UTIL_HudMessage( CBasePlayer *pToPlayer, const hudtextparms_t &textparms, const char *pMessage )
{
	CRecipientFilter filter;
	
	if( pToPlayer )
	{
		filter.AddRecipient( pToPlayer );
	}
	else
	{
		filter.AddAllPlayers();
	}

	filter.MakeReliable();

	ks::net::CCSUsrMsg_HudMsg msg;
	msg.channel = textparms.channel & 0xFF;
	msg.pos.mut().x = textparms.x;
	msg.pos.mut().y = textparms.y;
	msg.clr1.mut().r = textparms.r1;
	msg.clr1.mut().g = textparms.g1;
	msg.clr1.mut().b = textparms.b1;
	msg.clr1.mut().a = textparms.a1;
	msg.clr2.mut().r = textparms.r2;
	msg.clr2.mut().g = textparms.g2;
	msg.clr2.mut().b = textparms.b2;
	msg.clr2.mut().a = textparms.a2;
	msg.effect = textparms.effect;
	msg.fade_in_time = textparms.fadeinTime;
	msg.fade_out_time = textparms.fadeoutTime;
	msg.hold_time = textparms.holdTime;
	msg.fx_time = textparms.fxTime;
	msg.text = pMessage;
	SendUserMessage( filter, ks::net::CS_UM_HudMsg, msg );
}

void UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage )
{
	UTIL_HudMessage( nullptr, textparms, pMessage );
}

void UTIL_HudHintText( CBaseEntity *pEntity, const char *pMessage )
{
	if ( !pEntity )
		return;

	CSingleUserRecipientFilter user( (CBasePlayer *)pEntity );
	user.MakeReliable();

	ks::net::CCSUsrMsg_KeyHintText msg;
	msg.hints.emplace_back( pMessage );
	SendUserMessage( user, ks::net::CS_UM_KeyHintText, msg );
}

void UTIL_ClientPrintFilter( IRecipientFilter& filter, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	ks::net::CCSUsrMsg_TextMsg msg;

	msg.msg_dst = msg_dest;
	msg.params.emplace_back( msg_name );

	if ( param1 )
		msg.params.emplace_back( param1 );
	else
		msg.params.emplace_back( "" );

	if ( param2 )
		msg.params.emplace_back( param2 );
	else
		msg.params.emplace_back( "" );

	if ( param3 )
		msg.params.emplace_back( param3 );
	else
		msg.params.emplace_back( "" );

	if ( param4 )
		msg.params.emplace_back( param4 );
	else
		msg.params.emplace_back( "" );

	SendUserMessage( filter, ks::net::CS_UM_TextMsg, msg );
}
					 
void UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	CReliableBroadcastRecipientFilter filter;

	UTIL_ClientPrintFilter( filter, msg_dest, msg_name, param1, param2, param3, param4 );
}

void ClientPrint( CBasePlayer *player, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	if ( !player )
		return;

	CSingleUserRecipientFilter user( player );
	user.MakeReliable();

	UTIL_ClientPrintFilter( user, msg_dest, msg_name, param1, param2, param3, param4 );
}

void UTIL_SayTextFilter( IRecipientFilter& filter, const char *pText, CBasePlayer *pPlayer, EUtilSayTextMessageType_t eMessageType )
{
	ks::net::CCSUsrMsg_SayText msg;
	
	if ( pPlayer ) 
	{
		msg.ent_idx = pPlayer->entindex();
	}
	else
	{
		msg.ent_idx = 0; // world, dedicated server says
	}
	msg.text = pText;
	msg.chat = ( eMessageType == kEUtilSayTextMessageType_TeamonlyChat ) || ( eMessageType == kEUtilSayTextMessageType_AllChat );
	msg.textallchat = eMessageType == kEUtilSayTextMessageType_AllChat;

	SendUserMessage( filter, ks::net::CS_UM_SayText, msg );
}

void UTIL_SayText2Filter( IRecipientFilter& filter, CBasePlayer *pEntity, EUtilSayTextMessageType_t eMessageType, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	ks::net::CCSUsrMsg_SayText2 msg;

	if ( pEntity )
	{
		msg.ent_idx = pEntity->entindex();
	}
	else
	{
		msg.ent_idx = 0; // world, dedicated server says
	}

	msg.chat = ( eMessageType == kEUtilSayTextMessageType_TeamonlyChat ) || ( eMessageType == kEUtilSayTextMessageType_AllChat );
	msg.textallchat = eMessageType == kEUtilSayTextMessageType_AllChat;
	msg.msg_name = msg_name;

	if ( param1 )
		msg.params.emplace_back( param1 );
	else
		msg.params.emplace_back( "" );

	if ( param2 )
		msg.params.emplace_back( param2 );
	else
		msg.params.emplace_back( "" );

	if ( param3 )
		msg.params.emplace_back( param3 );
	else
		msg.params.emplace_back( "" );

	if ( param4 )
		msg.params.emplace_back( param4 );
	else
		msg.params.emplace_back( "" );

	SendUserMessage( filter, ks::net::CS_UM_SayText2, msg );
}

void UTIL_SayText( const char *pText, CBasePlayer *pToPlayer )
{
	if ( !pToPlayer->IsNetClient() )
		return;

	CSingleUserRecipientFilter user( pToPlayer );
	user.MakeReliable();

	UTIL_SayTextFilter( user, pText, pToPlayer, kEUtilSayTextMessageType_Default );
}

void UTIL_SayTextAll( const char *pText, CBasePlayer *pPlayer, EUtilSayTextMessageType_t eMessageType )
{
	CReliableBroadcastRecipientFilter filter;
	UTIL_SayTextFilter( filter, pText, pPlayer, eMessageType );
}

void UTIL_ShowMessage( const char *pString, CBasePlayer *pPlayer )
{
	CRecipientFilter filter;

	if ( pPlayer )
	{
		filter.AddRecipient( pPlayer );
	}
	else
	{
		filter.AddAllPlayers();
	}
	
	filter.MakeReliable();

	ks::net::CCSUsrMsg_HudText msg;
	msg.text = pString;
	SendUserMessage( filter, ks::net::CS_UM_HudText, msg );
}

void UTIL_ShowMessageAll( const char *pString )
{
	UTIL_ShowMessage( pString, nullptr );
}

//-------------------------------------------------------------------------------------------------
// HudMessagePanel helper
void UTIL_MessageTextAll( const char *text, Color color )
{
	// Gurjeets - Not used in CSGO
	//CReliableBroadcastRecipientFilter filter;
	//UserMessageBegin( filter, "MessageText" );
	//	WRITE_BYTE( color.r );
	//	WRITE_BYTE( color.g );
	//	WRITE_BYTE( color.b );
	//	WRITE_STRING( text );
	//MessageEnd();
}

//-------------------------------------------------------------------------------------------------
// HudMessagePanel helper
void UTIL_MessageText( CBasePlayer *player, const char *text, Color color )
{
	// Gurjeets - Not used in CSGO
	//CSingleUserRecipientFilter filter( player );
	//filter.MakeReliable();
	//UserMessageBegin( filter, "MessageText" );
	//	WRITE_BYTE( color.r );
	//	WRITE_BYTE( color.g );
	//	WRITE_BYTE( color.b );
	//	WRITE_STRING( text );
	//MessageEnd();
}

//-------------------------------------------------------------------------------------------------
// HudMessagePanel helper
void UTIL_ResetMessageTextAll( void )
{
	// Gurjeets - Not used in CSGO
	//CReliableBroadcastRecipientFilter filter;
	//UserMessageBegin( filter, "MessageText" );
	//MessageEnd();
}

//-------------------------------------------------------------------------------------------------
// HudMessagePanel helper
void UTIL_ResetMessageText( CBasePlayer *player )
{
	// Gurjeets - Not used in CSGO
	//CSingleUserRecipientFilter filter( player );
	//filter.MakeReliable();
	//UserMessageBegin( filter, "MessageText" );
	//MessageEnd();
}

// So we always return a valid surface
static csurface_t	g_NullSurface = { "**empty**", 0 };

void UTIL_SetTrace(trace_t& trace, const Ray_t &ray, edict_t *ent, float fraction, 
				   int hitgroup, unsigned int contents, const Vector& normal, float intercept )
{
	trace.startsolid = (fraction == 0.0f);
	trace.fraction = fraction;
	VectorCopy( ray.m_Start, trace.startpos );
	VectorMA( ray.m_Start, fraction, ray.m_Delta, trace.endpos );
	VectorCopy( normal, trace.plane.normal );
	trace.plane.dist = intercept;
	trace.m_pEnt = CBaseEntity::Instance( ent );
	trace.hitgroup = hitgroup;
	trace.surface =	g_NullSurface;
	trace.contents = contents;
}

void UTIL_ClearTrace( trace_t &trace )
{
	std::memset( &trace, 0, sizeof(trace));
	trace.fraction = 1.f;
	trace.fractionleftsolid = 0;
	trace.surface = g_NullSurface;
}

//-----------------------------------------------------------------------------
// Sets the entity size
//-----------------------------------------------------------------------------
static void SetMinMaxSize (CBaseEntity *pEnt, const Vector& mins, const Vector& maxs )
{
	for ( int i=0 ; i<3 ; i++ )
	{
		if ( mins[i] > maxs[i] )
		{
			Error( "%s: backwards mins/maxs", ( pEnt ) ? pEnt->GetDebugName() : "<NULL>" );
		}
	}

	contract_assert( pEnt );

	pEnt->SetCollisionBounds( mins, maxs );
}

//-----------------------------------------------------------------------------
// Sets the model size
//-----------------------------------------------------------------------------
void UTIL_SetSize( CBaseEntity *pEnt, const Vector &vecMin, const Vector &vecMax )
{
	SetMinMaxSize (pEnt, vecMin, vecMax);
}

//-----------------------------------------------------------------------------
// Sets the model to be associated with an entity
//-----------------------------------------------------------------------------
void UTIL_SetModel( CBaseEntity *pEntity, const char *pModelName )
{
	// check to see if model was properly precached
	int i = modelinfo->GetModelIndex( pModelName );
	if ( i < 0 )
	{
		Error("%i/%s - %s:  UTIL_SetModel:  not precached: %s\n", pEntity->entindex(),
			STRING( pEntity->GetEntityName() ),
			pEntity->GetClassname(), pModelName);
	}

	pEntity->SetModelIndex( i ) ;
	pEntity->SetModelName( AllocPooledString( pModelName ) );

	// brush model
	const model_t *mod = modelinfo->GetModel( i );
	if ( mod )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( mod, mins, maxs );
		SetMinMaxSize (pEntity, mins, maxs);
	}
	else
	{
		SetMinMaxSize (pEntity, vec3_origin, vec3_origin);
	}

	CBaseAnimating *pAnimating = pEntity->GetBaseAnimating();
	if ( pAnimating )
	{
		pAnimating->m_nForceBone = 0;
	}
}

void UTIL_SetOrigin( CBaseEntity *entity, const Vector &vecOrigin, bool bFireTriggers )
{
	entity->SetLocalOrigin( vecOrigin );
	if ( bFireTriggers )
	{
		entity->PhysicsTouchTriggers();
	}
}

void UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, uint32 ulColor, int ulCount )
{
	Msg( "UTIL_ParticleEffect:  Disabled\n" );
}

void UTIL_Smoke( const Vector &origin, const float scale, const float framerate )
{
	g_pEffects->Smoke( origin, g_sModelIndexSmoke, scale, framerate );
}

// snaps a vector to the nearest axis vector (if within epsilon)
void UTIL_SnapDirectionToAxis( Vector &direction, float epsilon )
{
	float proj = 1 - epsilon;
	for ( int i = 0; i < 3; i ++ )
	{
		if ( fabs(direction[i]) > proj )
		{
			// snap to axis unit vector
			if ( direction[i] < 0 )
				direction[i] = -1.0f;
			else
				direction[i] = 1.0f;
			direction[(i+1)%3] = 0;
			direction[(i+2)%3] = 0;
			return;
		}
	}
}

const char *UTIL_VarArgs( const char *format, ... )
{
	std::va_list		argptr;
	static char		string[1024];
	
	__builtin_va_start (argptr, format);
	V_vsnprintf(string, sizeof(string), format,argptr);
	__builtin_va_end (argptr);

	return string;	
}

bool UTIL_IsMasterTriggered(string_t sMaster, CBaseEntity *pActivator)
{
	if (sMaster != NULL_STRING)
	{
		CBaseEntity *pMaster = gEntList.FindEntityByName( nullptr, sMaster, nullptr, pActivator );
	
		if ( pMaster && (pMaster->ObjectCaps() & FCAP_MASTER) )
		{
			return pMaster->IsTriggered( pActivator );
		}

		Warning( "Master was null or not a master!\n");
	}

	// if this isn't a master entity, just say yes.
	return true;
}

void UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	CPVSFilter filter( origin );
	te->BloodStream( filter, 0.0, &origin, &direction, 247, 63, 14, 255, std::min( amount, 255 ) );
}				

Vector UTIL_RandomBloodVector( void )
{
	Vector direction;

	direction.x = random_valve->RandomFloat ( -1, 1 );
	direction.y = random_valve->RandomFloat ( -1, 1 );
	direction.z = random_valve->RandomFloat ( 0, 1 );

	return direction;
}

//------------------------------------------------------------------------------
// Purpose : Creates both an decal and any associated impact effects (such
//			 as flecks) for the given iDamageType and the trace's end position
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	CBaseEntity *pEntity = pTrace->Ent<CBaseEntity>();

	// Is the entity valid, is the surface sky?
	if ( !pEntity || !UTIL_IsValidEntity( pEntity ) || (pTrace->surface.flags & SURF_SKY) )
		return;

	if ( pTrace->fraction == 1.0 )
		return;

	pEntity->ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace( trace_t *pTrace, Vector const &right, int playernum )
{
	if (pTrace->fraction == 1.0)
		return;

	CBroadcastRecipientFilter filter;
	filter.SetIgnorePredictionCull( true );	// need to ignore prediction cull for the person who is spraying

	te->PlayerDecal( filter, 0.0,
		&pTrace->endpos, &pTrace->startpos, &right, playernum, pTrace->Ent<CBaseEntity>()->entindex(), pTrace->hitbox );
}

bool UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return true;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !stricmp( pTeamName1, pTeamName2 ) )	// Same Team?
			return true;
	}

	return false;
}

void UTIL_AxisStringToPointPoint( Vector &start, Vector &end, const char *pString )
{
	char tmpstr[256];

	V_strncpy( tmpstr, pString, sizeof(tmpstr) );
	char *pVec = std::strtok( tmpstr, "," );
	int i = 0;
	while ( pVec != nullptr && *pVec )
	{
		if ( i == 0 )
		{
			UTIL_StringToVector( start.Base(), pVec );
			i++;
		}
		else
		{
			UTIL_StringToVector( end.Base(), pVec );
		}
		pVec = strtok( nullptr, "," );
	}
}

void UTIL_AxisStringToPointDir( Vector &start, Vector &dir, const char *pString )
{
	Vector end;
	UTIL_AxisStringToPointPoint( start, end, pString );
	dir = end - start;
	VectorNormalize(dir);
}

void UTIL_AxisStringToUnitDir( Vector &dir, const char *pString )
{
	Vector start;
	UTIL_AxisStringToPointDir( start, dir, pString );
}

/*
==================================================
UTIL_ClipPunchAngleOffset
==================================================
*/

void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip )
{
	QAngle	final = in + punch;

	//Clip each component
	for ( int i = 0; i < 3; i++ )
	{
		if ( final[i] > clip[i] )
		{
			final[i] = clip[i];
		}
		else if ( final[i] < -clip[i] )
		{
			final[i] = -clip[i];
		}

		//Return the result
		in[i] = final[i] - punch[i];
	}
}

extern int	g_sModelIndexBubbles;// holds the index for the bubbles model

void UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count )
{
	Vector mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	CPASFilter filter( mid );

	te->Bubbles( filter, 0.0,
		&mins, &maxs, flHeight, g_sModelIndexBubbles, count, 8.0 );
}

void UTIL_BubbleTrail( const Vector& from, const Vector& to, int count )
{
	// Find water surface will return from.z if the from point is above water
	float flStartHeight = UTIL_FindWaterSurface( from, from.z, from.z + 256 );
	flStartHeight = flStartHeight - from.z;

	float flEndHeight = UTIL_FindWaterSurface( to, to.z, to.z + 256 );
	flEndHeight = flEndHeight - to.z;

	if ( ( flStartHeight == 0 ) && ( flEndHeight == 0 ) )
		return;

	float flWaterZ = flStartHeight + from.z;

	const Vector *pFrom = &from;
	const Vector *pTo = &to;
	Vector vecWaterPoint;
	if ( ( flStartHeight == 0 ) || ( flEndHeight == 0 ) )
	{
		if ( flStartHeight == 0 )
		{
			flWaterZ = flEndHeight + to.z;
		}

		float t = IntersectRayWithAAPlane( from, to, 2, 1.0f, flWaterZ );
		contract_assert( (t >= -1e-3f) && ( t <= 1.0f ) );
		VectorLerp( from, to, t, vecWaterPoint );
		if ( flStartHeight == 0 )
		{
			pFrom = &vecWaterPoint;

			// Reduce the count by the actual length
			count = (int)( count * ( 1.0f - t ) );
		}
		else
		{
			pTo = &vecWaterPoint;

			// Reduce the count by the actual length
			count = (int)( count * t );
		}
	}

	CBroadcastRecipientFilter filter;
	te->BubbleTrail( filter, 0.0, pFrom, pTo, flWaterZ, g_sModelIndexBubbles, count, 8.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Start - 
//			End - 
//			ModelIndex - 
//			FrameStart - 
//			FrameRate - 
//			Life - 
//			Width - 
//			Noise - 
//			Red - 
//			Green - 
//			Brightness - 
//			Speed - 
//-----------------------------------------------------------------------------
void UTIL_Beam( Vector &Start, Vector &End, int nModelIndex, int nHaloIndex, unsigned char FrameStart, unsigned char FrameRate,
				float Life, unsigned char Width, unsigned char EndWidth, unsigned char FadeLength, unsigned char Noise, unsigned char Red, unsigned char Green,
				unsigned char Blue, unsigned char Brightness, unsigned char Speed)
{
	CBroadcastRecipientFilter filter;

	te->BeamPoints( filter, 0.0,
		&Start, 
		&End, 
		nModelIndex, 
		nHaloIndex, 
		FrameStart,
		FrameRate, 
		Life,
		Width,
		EndWidth,
		FadeLength,
		Noise,
		Red,
		Green,
		Blue,
		Brightness,
		Speed );
}

bool UTIL_IsValidEntity( CBaseEntity *pEnt )
{
	edict_t *pEdict = pEnt->edict();
	if ( !pEdict || pEdict->IsFree() )
		return false;
	return true;
}

// UNDONE: Do we need this to avoid doing too much of this?  Measure startup times and see

class CPrecacheOtherList : public CAutoGameSystem
{
public:
	CPrecacheOtherList( char const *name ) : CAutoGameSystem( name )
	{
	}
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPostEntity();

	bool AddOrMarkPrecached( const char *pClassname );

private:
	ks::intern_pool		m_list;
};

void CPrecacheOtherList::LevelInitPreEntity()
{
	m_list.clear();
}

void CPrecacheOtherList::LevelShutdownPostEntity()
{
	m_list.clear();
}

//-----------------------------------------------------------------------------
// Purpose: mark or add
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrecacheOtherList::AddOrMarkPrecached( const char *pClassname )
{
	if ( m_list.contains( pClassname ) )
		return false;

	m_list.intern( pClassname );
	return true;
}

CPrecacheOtherList g_PrecacheOtherList( "CPrecacheOtherList" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szClassname - 
//			*modelName - 
//-----------------------------------------------------------------------------
void UTIL_PrecacheOther( const char *szClassname, const char *modelName )
{
	// already done this one?, if not, mark as done
	if ( !g_PrecacheOtherList.AddOrMarkPrecached( szClassname ) )
		return;

	CBaseEntity	*pEntity = CreateEntityByName( szClassname );
	if ( !pEntity )
	{
		Warning( "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}

	// If we have a specified model, set it before calling precache
	if ( modelName && modelName[0] )
	{
		pEntity->SetModelName( AllocPooledString( modelName ) );
	}
	
	if (pEntity)
		pEntity->Precache( );

	UTIL_RemoveImmediate( pEntity );
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( const char *fmt, ... )
{
	if ( !g_bIsLogging )
		return;

	std::va_list		argptr;
	char		tempString[1024];
	
	__builtin_va_start ( argptr, fmt );
	V_vsnprintf( tempString, sizeof(tempString), fmt, argptr );
	__builtin_va_end   ( argptr );

	// Print to server console
	engine->LogPrint( tempString );
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir )
{
	Vector2D	vec2LOS;

	vec2LOS = ( vecCheck - vecSrc ).AsVector2D();
	Vector2DNormalize( vec2LOS );

	return DotProduct2D(vec2LOS, vecDir.AsVector2D());
}

//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest )
{
	int i = 0;

	while ( pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}

// computes gravity scale for an absolute gravity.  Pass the result into CBaseEntity::SetGravity()
float UTIL_ScaleForGravity( float desiredGravity )
{
	float worldGravity = sv_gravity.GetFloat();
	return worldGravity > 0 ? desiredGravity / worldGravity : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Spawns an entity into the game, initializing it with the map ent data block
// Input  : *pEntity - the newly created entity
//			*mapData - pointer a block of entity map data
// Output : -1 if the entity was not successfully created; 0 on success
//-----------------------------------------------------------------------------
int DispatchSpawn( CBaseEntity *pEntity, bool bRunVScripts )
{
	if ( pEntity )
	{
		MDLCACHE_CRITICAL_SECTION();

		// keep a smart pointer that will now if the object gets deleted
		EHANDLE pEntSafe;
		pEntSafe = pEntity;

		// Initialize these or entities who don't link to the world won't have anything in here
		// is this necessary?
		//pEntity->SetAbsMins( pEntity->GetOrigin() - Vector(1,1,1) );
		//pEntity->SetAbsMaxs( pEntity->GetOrigin() + Vector(1,1,1) );

		if( bRunVScripts )
		{
			pEntity->RunVScripts();
			pEntity->RunPrecacheScripts();
		}

		bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
		CBaseAnimating *pAnimating = pEntity->GetBaseAnimating();
		if (!pAnimating)
		{
			pEntity->Spawn();
		}
		else
		{
			// Don't allow the PVS check to skip animation setup during spawning
			pAnimating->SetBoneCacheFlags( BCF_IS_IN_SPAWN );
			pEntity->Spawn();
			pAnimating->ClearBoneCacheFlags( BCF_IS_IN_SPAWN );
		}
		mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );

		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.

		if ( pEntSafe == nullptr || pEntity->IsMarkedForDeletion() )
			return -1;

		if ( pEntity->m_iGlobalname != NULL_STRING ) 
		{
			// Handle global stuff here
			int globalIndex = GlobalEntity_GetIndex( pEntity->m_iGlobalname );
			if ( globalIndex >= 0 )
			{
				// Already dead? delete
				if ( GlobalEntity_GetState(globalIndex) == GLOBAL_DEAD )
				{
					pEntity->Remove();
					return -1;
				}
				else if ( !FStrEq(STRING(gpGlobals->mapname), GlobalEntity_GetMap(globalIndex)) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				GlobalEntity_Add( pEntity->m_iGlobalname, gpGlobals->mapname, GLOBAL_ON );
//					Msg( "Added global entity %s (%s)\n", pEntity->GetClassname(), STRING(pEntity->m_iGlobalname) );
			}
		}

		gEntList.NotifySpawn( pEntity );

		if( bRunVScripts )
		{
			pEntity->RunOnPostSpawnScripts();
		}

	}

	return 0;
}

// UNDONE: This could be a better test - can we run the absbox through the bsp and see
// if it contains any solid space?  or would that eliminate some entities we want to keep?
int UTIL_EntityInSolid( CBaseEntity *ent )
{
	Vector	point;

	CBaseEntity *pParent = ent->GetMoveParent();
	// HACKHACK -- If you're attached to a client, always go through
	if ( pParent )
	{
		if ( pParent->IsPlayer() )
			return 0;

		ent = ent->GetRootMoveParent();
	}

	point = ent->WorldSpaceCenter();
	return ( enginetrace->GetPointContents( point ) & MASK_SOLID );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the matrix from an entity
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void EntityMatrix::InitFromEntity( CBaseEntity *pEntity, int iAttachment )
{
	if ( !pEntity )
	{
		Identity();
		return;
	}

	// Get an attachment's matrix?
	if ( iAttachment != 0 )
	{
		CBaseAnimating *pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating && pAnimating->GetModelPtr() )
		{
			Vector vOrigin;
			QAngle vAngles;
			if ( pAnimating->GetAttachment( iAttachment, vOrigin, vAngles ) )
			{
				((VMatrix *)this)->SetupMatrixOrgAngles( vOrigin, vAngles );
				return;
			}
		}
	}

	((VMatrix *)this)->SetupMatrixOrgAngles( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles() );
}

void EntityMatrix::InitFromEntityLocal( CBaseEntity *entity )
{
	if ( !entity || !entity->edict() )
	{
		Identity();
		return;
	}
	((VMatrix *)this)->SetupMatrixOrgAngles( entity->GetLocalOrigin(), entity->GetLocalAngles() );
}

//==================================================
// Purpose: 
// Input: 
// Output: 
//==================================================

void UTIL_ValidateSoundName( string_t &name, const char *defaultStr )
{
	if ( ( !name || 
		   std::strlen( (char*) STRING( name ) ) < 1 ) || 
		   !V_stricmp( (char *)STRING(name), "0" ) )
	{
		name = AllocPooledString( defaultStr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper for UTIL_FindClientInPVS
// Input  : check - last checked client
// Output : static int UTIL_GetNewCheckClient
//-----------------------------------------------------------------------------
// FIXME:  include bspfile.h here?
class CCheckClient : public CAutoGameSystem
{
public:
	CCheckClient( char const *name ) : CAutoGameSystem( name )
	{
	}

	void LevelInitPreEntity()
	{
		m_checkCluster = -1;
		m_lastcheck = 1;
		m_lastchecktime = -1;
		m_bClientPVSIsExpanded = false;
	}

	byte	m_checkPVS[MAX_MAP_LEAFS/8];
	byte	m_checkVisibilityPVS[MAX_MAP_LEAFS/8];
	int		m_checkCluster;
	int		m_lastcheck;
	float	m_lastchecktime;
	bool	m_bClientPVSIsExpanded;
};

CCheckClient g_CheckClient( "CCheckClient" );

static int UTIL_GetNewCheckClient( int check )
{
	int		i;
	edict_t	*ent;
	Vector	org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > gpGlobals->maxClients)
		check = gpGlobals->maxClients;

	if (check == gpGlobals->maxClients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if ( i > gpGlobals->maxClients )
		{
			i = 1;
		}

		ent = INDEXENT( i );
		if ( !ent )
			continue;

		// Looped but didn't find anything else
		if ( i == check )
			break;	

		if ( !ent->GetUnknown() )
			continue;

		CBaseEntity *entity = GetContainingEntity( ent );
		if ( !entity )
			continue;

		if ( entity->GetFlags() & FL_NOTARGET )
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

	if ( i != check )
	{
		std::memset( g_CheckClient.m_checkVisibilityPVS, 0, sizeof(g_CheckClient.m_checkVisibilityPVS) );
		g_CheckClient.m_bClientPVSIsExpanded = false;
	}

	if ( ent )
	{
		// get the PVS for the entity
		CBaseEntity *pce = GetContainingEntity( ent );
		if ( !pce )
			return i;

		org = pce->EyePosition();

		int clusterIndex = engine->GetClusterForOrigin( org );
		if ( clusterIndex != g_CheckClient.m_checkCluster )
		{
			g_CheckClient.m_checkCluster = clusterIndex;
			engine->GetPVSForCluster( clusterIndex, sizeof(g_CheckClient.m_checkPVS), g_CheckClient.m_checkPVS );

		}
	}
	
	return i;
}

//-----------------------------------------------------------------------------
// Gets the current check client....
//-----------------------------------------------------------------------------
static edict_t *UTIL_GetCurrentCheckClient()
{
	edict_t	*ent;

	// find a new check if on a new frame
	float delta = gpGlobals->curtime - g_CheckClient.m_lastchecktime;
	if ( delta >= 0.1 || delta < 0 )
	{
		g_CheckClient.m_lastcheck = UTIL_GetNewCheckClient( g_CheckClient.m_lastcheck );
		g_CheckClient.m_lastchecktime = gpGlobals->curtime;
	}

	// return check if it might be visible	
	ent = INDEXENT( g_CheckClient.m_lastcheck );

	// Allow dead clients -- JAY
	// Our monsters know the difference, and this function gates alot of behavior
	// It's annoying to die and see monsters stop thinking because you're no longer
	// "in" their PVS
	if ( !ent || ent->IsFree() || !ent->GetUnknown())
	{
		return nullptr;
	}

	return ent;
}

void UTIL_SetClientVisibilityPVS( edict_t *pClient, const unsigned char *pvs, int pvssize )
{
	if ( pClient == UTIL_GetCurrentCheckClient() )
	{
		contract_assert( pvssize <= sizeof(g_CheckClient.m_checkVisibilityPVS) );

		g_CheckClient.m_bClientPVSIsExpanded = false;

		unsigned *pFrom = (unsigned *)pvs;
		unsigned *pMask = (unsigned *)g_CheckClient.m_checkPVS;
		unsigned *pTo = (unsigned *)g_CheckClient.m_checkVisibilityPVS;

		int limit = pvssize / 4;
		int i;

		for ( i = 0; i < limit; i++ )
		{
			pTo[i] = pFrom[i] & ~pMask[i];

			if ( pFrom[i] )
			{
				g_CheckClient.m_bClientPVSIsExpanded = true;
			}
		}

		int remainder = pvssize % 4;
		for ( i = 0; i < remainder; i++ )
		{
			((unsigned char *)&pTo[limit])[i] = ((unsigned char *)&pFrom[limit])[i] & !((unsigned char *)&pMask[limit])[i];

			if ( ((unsigned char *)&pFrom[limit])[i] != 0)
			{
				g_CheckClient.m_bClientPVSIsExpanded = true;
			}
		}
	}
}

bool UTIL_ClientPVSIsExpanded()
{
	return g_CheckClient.m_bClientPVSIsExpanded;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client (or object that has a client enemy) that would be a valid target.
//  If there are more than one valid options, they are cycled each frame
//  If (self.origin + self.viewofs) is not in the PVS of the current target, it is not returned at all.
// Input  : *pEdict - 
// Output : edict_t*
//-----------------------------------------------------------------------------
CBaseEntity *UTIL_FindClientInPVS( const Vector &vecBoxMins, const Vector &vecBoxMaxs )
{
	edict_t	*ent = UTIL_GetCurrentCheckClient();
	if ( !ent )
	{
		return nullptr;
	}

	if ( !engine->CheckBoxInPVS( vecBoxMins, vecBoxMaxs, g_CheckClient.m_checkPVS, sizeof( g_CheckClient.m_checkPVS ) ) )
	{
		return nullptr;
	}

	// might be able to see it
	return GetContainingEntity( ent );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client (or object that has a client enemy) that would be a valid target.
//  If there are more than one valid options, they are cycled each frame
//  If (self.origin + self.viewofs) is not in the PVS of the current target, it is not returned at all.
// Input  : *pEdict - 
// Output : edict_t*
//-----------------------------------------------------------------------------
ConVar sv_strict_notarget( side_registry(), "sv_strict_notarget", "0", 0, "If set, notarget will cause entities to never think they are in the pvs" );

edict_t *UTIL_FindClientInPVSGuts(edict_t *pEdict, unsigned char *pvs, unsigned pvssize )
{
	Vector	view;

	edict_t	*ent = UTIL_GetCurrentCheckClient();
	if ( !ent )
	{
		return nullptr;
	}

	CBaseEntity *pPlayerEntity = GetContainingEntity( ent );
	if( (!pPlayerEntity || (pPlayerEntity->GetFlags() & FL_NOTARGET)) && sv_strict_notarget.GetBool() )
	{
		return nullptr;
	}
	// if current entity can't possibly see the check entity, return 0
	// UNDONE: Build a box for this and do it over that box
	// UNDONE: Use CM_BoxLeafnums()
	CBaseEntity *pe = GetContainingEntity( pEdict );
	if ( pe )
	{
		view = pe->EyePosition();
		
		if ( !engine->CheckOriginInPVS( view, pvs, pvssize ) )
		{
			return nullptr;
		}
	}

	// might be able to see it
	return ent;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client that could see the entity directly
//-----------------------------------------------------------------------------

edict_t *UTIL_FindClientInPVS(edict_t *pEdict)
{
	return g_pGameRules->DoFindClientInPVS( pEdict, g_CheckClient.m_checkPVS, sizeof( g_CheckClient.m_checkPVS ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client that could see the entity, including through a camera
//-----------------------------------------------------------------------------
edict_t *UTIL_FindClientInVisibilityPVS( edict_t *pEdict )
{
	return g_pGameRules->DoFindClientInPVS( pEdict, g_CheckClient.m_checkVisibilityPVS, sizeof( g_CheckClient.m_checkVisibilityPVS ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a chain of entities within the PVS of another entity (client)
//  starting_ent is the ent currently at in the list
//  a starting_ent of NULL signifies the beginning of a search
// Input  : *pplayer - 
//			*starting_ent - 
// Output : edict_t
//-----------------------------------------------------------------------------
CBaseEntity *UTIL_EntitiesInPVS( CBaseEntity *pPVSEntity, CBaseEntity *pStartingEntity )
{
	Vector			org;
	static byte		pvs[ MAX_MAP_CLUSTERS/8 ];
	static Vector	lastOrg( 0, 0, 0 );
	static int		lastCluster = -1;

	if ( !pPVSEntity )
		return nullptr;

	// NOTE: These used to be caching code here to prevent this from
	// being called over+over which breaks when you go back + forth
	// across level transitions
	// So, we'll always get the PVS each time we start a new EntitiesInPVS iteration.
	// Given that weapon_binocs + leveltransition code is the only current clients
	// of this, this seems safe.
	if ( !pStartingEntity )
	{
		org = pPVSEntity->EyePosition();
		int clusterIndex = engine->GetClusterForOrigin( org );
		contract_assert( clusterIndex >= 0 );
		engine->GetPVSForCluster( clusterIndex, sizeof(pvs), pvs );
	}

	for ( CBaseEntity *pEntity = gEntList.NextEnt(pStartingEntity); pEntity; pEntity = gEntList.NextEnt(pEntity) )
	{
		// Only return attached ents.
		if ( !pEntity->edict() )
			continue;

		CBaseEntity *pParent = pEntity->GetRootMoveParent();

		Vector vecSurroundMins, vecSurroundMaxs;
		pParent->CollisionProp()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
		if ( !engine->CheckBoxInPVS( vecSurroundMins, vecSurroundMaxs, pvs, sizeof( pvs ) ) )
			continue;

		return pEntity;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Get the predicted postion of an entity of a certain number of seconds
//			Use this function with caution, it has great potential for annoying the player, especially
//			if used for target firing predition
// Input  : *pTarget - target entity to predict
//			timeDelta - amount of time to predict ahead (in seconds)
//			&vecPredictedPosition - output
//-----------------------------------------------------------------------------
void UTIL_PredictedPosition( CBaseEntity *pTarget, float flTimeDelta, Vector *vecPredictedPosition )
{
	if ( ( pTarget == nullptr ) || ( vecPredictedPosition == nullptr ) )
		return;

	Vector	vecPredictedVel;

	//FIXME: Should we look at groundspeed or velocity for non-clients??

	//Get the proper velocity to predict with
	CBasePlayer	*pPlayer = ToBasePlayer( pTarget );

	//Player works differently than other entities
	if ( pPlayer != nullptr )
	{
		{
			//Get the player's stored velocity
			vecPredictedVel = pPlayer->GetSmoothedVelocity();
		}
	}
	else
	{
		// See if we're a combat character in a vehicle
		CBaseCombatCharacter *pCCTarget = pTarget->MyCombatCharacterPointer();
		{
			// See if we're an animating entity
			CBaseAnimating *pAnimating = entity_cast<CBaseAnimating>(pTarget);
			if ( pAnimating != nullptr )
			{
				vecPredictedVel = pAnimating->GetGroundSpeedVelocity();
			}
			else
			{
				// Otherwise we're a vanilla entity
				vecPredictedVel = pTarget->GetSmoothedVelocity();				
			}
		}
	}

	//Get the result
	(*vecPredictedPosition) = pTarget->GetAbsOrigin() + ( vecPredictedVel * flTimeDelta );
}

//-----------------------------------------------------------------------------
// Purpose: Points the destination entity at the target entity
// Input  : *pDest - entity to be pointed at the target
//			*pTarget - target to point at
//-----------------------------------------------------------------------------
bool UTIL_PointAtEntity( CBaseEntity *pDest, CBaseEntity *pTarget )
{
	if ( ( pDest == nullptr ) || ( pTarget == nullptr ) )
	{
		return false;
	}

	Vector dir = (pTarget->GetAbsOrigin() - pDest->GetAbsOrigin());

	VectorNormalize( dir );

	//Store off as angles
	QAngle angles;
	VectorAngles( dir, angles );
	pDest->SetLocalAngles( angles );
	pDest->SetAbsAngles( angles );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Points the destination entity at the target entity by name
// Input  : *pDest - entity to be pointed at the target
//			strTarget - name of entity to target (will only choose the first!)
//-----------------------------------------------------------------------------
void UTIL_PointAtNamedEntity( CBaseEntity *pDest, string_t strTarget )
{
	//Attempt to find the entity
	if ( !UTIL_PointAtEntity( pDest, gEntList.FindEntityByName( nullptr, strTarget ) ) )
	{
		DevMsg( 1, "%s (%s) was unable to point at an entity named: %s\n", pDest->GetClassname(), pDest->GetDebugName(), STRING( strTarget ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy the pose parameter values from one entity to the other
// Input  : *pSourceEntity - entity to copy from
//			*pDestEntity - entity to copy to
//-----------------------------------------------------------------------------
bool UTIL_TransferPoseParameters( CBaseEntity *pSourceEntity, CBaseEntity *pDestEntity )
{
	CBaseAnimating *pSourceBaseAnimating = entity_cast<CBaseAnimating>( pSourceEntity );
	CBaseAnimating *pDestBaseAnimating = entity_cast<CBaseAnimating>( pDestEntity );

	if ( !pSourceBaseAnimating || !pDestBaseAnimating )
		return false;

	for ( int iPose = 0; iPose < MAXSTUDIOPOSEPARAM; ++iPose )
	{
		pDestBaseAnimating->SetPoseParameter( iPose, pSourceBaseAnimating->GetPoseParameter( iPose ) );
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Make a muzzle flash appear
// Input  : &origin - position of the muzzle flash
//			&angles - angles of the fire direction
//			scale - scale of the muzzle flash
//			type - type of muzzle flash
//-----------------------------------------------------------------------------
void UTIL_MuzzleFlash( const Vector &origin, const QAngle &angles, int scale, int type )
{
	CPASFilter filter( origin );

	te->MuzzleFlash( filter, 0.0f, origin, angles, scale, type );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vStartPos - start of the line
//			vEndPos - end of the line
//			vPoint - point to find nearest point to on specified line
//			clampEnds - clamps returned points to being on the line segment specified
// Output : Vector - nearest point on the specified line
//-----------------------------------------------------------------------------
Vector UTIL_PointOnLineNearestPoint(const Vector& vStartPos, const Vector& vEndPos, const Vector& vPoint, bool clampEnds )
{
	Vector	vEndToStart		= (vEndPos - vStartPos);
	Vector	vOrgToStart		= (vPoint  - vStartPos);
	float	fNumerator		= DotProduct(vEndToStart,vOrgToStart);
	float	fDenominator	= vEndToStart.Length() * vOrgToStart.Length();
	float	fIntersectDist	= vOrgToStart.Length()*(fNumerator/fDenominator);
	float	flLineLength	= VectorNormalize( vEndToStart ); 
	
	if ( clampEnds )
	{
		fIntersectDist = clamp( fIntersectDist, 0.0f, flLineLength );
	}
	
	Vector	vIntersectPos	= vStartPos + vEndToStart * fIntersectDist;

	return vIntersectPos;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
AngularImpulse WorldToLocalRotation( const VMatrix &localToWorld, const Vector &worldAxis, float rotation )
{
	// fix axes of rotation to match axes of vector
	Vector rot = worldAxis * rotation;
	// since the matrix maps local to world, do a transpose rotation to get world to local
	AngularImpulse ang = localToWorld.VMul3x3Transpose( rot );

	return ang;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*pLength - 
// Output : byte
//-----------------------------------------------------------------------------
byte *UTIL_LoadFileForMe( const char *filename, int *pLength )
{
	void *buffer = nullptr;

	int length = filesystem->ReadFileEx( filename, "GAME", &buffer, true, true );

	if ( pLength )
	{
		*pLength = length;
	}

	return (byte *)buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void UTIL_FreeFile( byte *buffer )
{
	filesystem->FreeOptimalReadBuffer( buffer );
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether an entity is within a certain angular tolerance to viewer
// Input  : *pEntity - entity which is the "viewer"
//			vecPosition - position to test against
//			flTolerance - tolerance (as dot-product)
//			*pflDot - if not NULL, holds the 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, const Vector &vecPosition, float flDotTolerance, float *pflDot /*= NULL*/ )
{
	if ( pflDot )
	{
		*pflDot = 0.0f;
	}

	// Required elements
	if ( pViewer == nullptr )
		return false;

	Vector forward;
	pViewer->GetVectors( &forward, nullptr, nullptr );

	Vector dir = vecPosition - pViewer->GetAbsOrigin();
	VectorNormalize( dir );

	// Larger dot product corresponds to a smaller angle
	float flDot = dir.Dot( forward );

	// Return the result
	if ( pflDot )
	{
		*pflDot = flDot;
	}

	// Within the goal tolerance
	if ( flDot >= flDotTolerance )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether an entity is within a certain angular tolerance to viewer
// Input  : *pEntity - entity which is the "viewer"
//			*pTarget - entity to test against
//			flTolerance - tolerance (as dot-product)
//			*pflDot - if not NULL, holds the 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, CBaseEntity *pTarget, float flDotTolerance, float *pflDot /*= NULL*/ )
{
	if ( pViewer == nullptr || pTarget == nullptr )
		return false;

	return UTIL_IsFacingWithinTolerance( pViewer, pTarget->GetAbsOrigin(), flDotTolerance, pflDot );
}

//-----------------------------------------------------------------------------
// Purpose: Fills in color for debug purposes based on a relationship
// Input  : nRelationship - relationship to test
//			*pR, *pG, *pB - colors to fill
//-----------------------------------------------------------------------------
void UTIL_GetDebugColorForRelationship( int nRelationship, int &r, int &g, int &b )
{
	switch ( nRelationship )
	{
	case D_LI:
		r = 0;
		g = 255;
		b = 0;
		break;
	case D_NU:
		r = 0;
		g = 0;
		b = 255;
		break;
	case D_HT:
		r = 255;
		g = 0;
		b = 0;
		break;
	case D_FR:
		r = 255;
		g = 255;
		b = 0;
		break;
	default:
		r = 255;
		g = 255;
		b = 255;
		break;
	}
}

void LoadAndSpawnEntities_ParseEntKVBlockHelper( CBaseEntity *pNode, KeyValues *pkvNode )
{
	KeyValues *pkvNodeData = pkvNode->GetFirstSubKey();
	while ( pkvNodeData )
	{
		// Handle the connections block
		if ( !V_strcmp(pkvNodeData->GetName(), "connections") )
		{
			LoadAndSpawnEntities_ParseEntKVBlockHelper( pNode, pkvNodeData );
		}
		else
		{
			pNode->KeyValue( pkvNodeData->GetName(), pkvNodeData->GetString() );
		}

		pkvNodeData = pkvNodeData->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads and parses a file and spawns entities defined in it.
//-----------------------------------------------------------------------------
bool UTIL_LoadAndSpawnEntitiesFromScript( std::vector< CBaseEntity* > &entities, const char *pScriptFile, const char *pBlock, bool bActivate )
{
	KeyValues *pkvFile = new KeyValues( pBlock );

	if ( pkvFile->LoadFromFile( filesystem, pScriptFile, "GAME" ) )
	{	
		// Load each block, and spawn the entities
		KeyValues *pkvNode = pkvFile->GetFirstSubKey();
		while ( pkvNode )
		{
			// Get name
			const char *pNodeName = pkvNode->GetName();

			if ( stricmp( pNodeName, "entity" ) )
			{
				pkvNode = pkvNode->GetNextKey();
				continue;
			}

			KeyValues *pClassname = pkvNode->FindKey( "classname" );

			if ( pClassname )
			{
				// Use the classname instead
				pNodeName = pClassname->GetString();
			}

			// Spawn the entity
			CBaseEntity *pNode = CreateEntityByName( pNodeName );

			if ( pNode )
			{
				LoadAndSpawnEntities_ParseEntKVBlockHelper( pNode, pkvNode );
				DispatchSpawn( pNode );
				entities.push_back( pNode );
			}
			else
			{
				Warning( "UTIL_LoadAndSpawnEntitiesFromScript: Failed to spawn entity, type: '%s'\n", pNodeName );
			}

			// Move to next entity
			pkvNode = pkvNode->GetNextKey();
		}

		if ( bActivate == true )
		{
			bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
			// Then activate all the entities
			for ( int i = 0; i < std::ssize( entities ); i++ )
			{
				entities[i]->Activate();
			}
			mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );
		}
	}
	else
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector an angle from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles )
{
	if ( pEntity == nullptr )
		return;

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	matrix3x4_t matEntityToParent;
	AngleMatrix( vecAngles, matEntityToParent );
	MatrixSetColumn( vecPosition, 3, matEntityToParent );

	// concatenate with our parent's transform
	matrix3x4_t matScratch, matResult;
	matrix3x4_t matParentToWorld;
	
	if ( pEntity->GetParent() != nullptr )
	{
		matParentToWorld = pEntity->GetParentToWorldTransform( matScratch );
	}
	else
	{
		matParentToWorld = pEntity->EntityToWorldTransform();
	}

	ConcatTransforms( matParentToWorld, matEntityToParent, matResult );

	// pull our absolute position out of the matrix
	MatrixGetColumn( matResult, 3, vecPosition );
	MatrixAngles( matResult, vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector and quaternion from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat )
{
	if ( pEntity == nullptr )
		return;

	QAngle vecAngles;
	QuaternionAngles( quat, vecAngles );
	UTIL_ParentToWorldSpace( pEntity, vecPosition, vecAngles );
	AngleQuaternion( vecAngles, quat );
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector an angle from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles )
{
	if ( pEntity == nullptr )
		return;

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	matrix3x4_t matEntityToParent;
	AngleMatrix( vecAngles, matEntityToParent );
	MatrixSetColumn( vecPosition, 3, matEntityToParent );

	// concatenate with our parent's transform
	matrix3x4_t matScratch, matResult;
	matrix3x4_t matWorldToParent;
	
	if ( pEntity->GetParent() != nullptr )
	{
		matScratch = pEntity->GetParentToWorldTransform( matScratch );
	}
	else
	{
		matScratch = pEntity->EntityToWorldTransform();
	}

	MatrixInvert( matScratch, matWorldToParent );
	ConcatTransforms( matWorldToParent, matEntityToParent, matResult );

	// pull our absolute position out of the matrix
	MatrixGetColumn( matResult, 3, vecPosition );
	MatrixAngles( matResult, vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector and quaternion from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat )
{
	if ( pEntity == nullptr )
		return;

	QAngle vecAngles;
	QuaternionAngles( quat, vecAngles );
	UTIL_WorldToParentSpace( pEntity, vecPosition, vecAngles );
	AngleQuaternion( vecAngles, quat );
}

//-----------------------------------------------------------------------------
// Purpose: Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
// Input  : *pVecPos - 
//-----------------------------------------------------------------------------
void UTIL_BoundToWorldSize( Vector *pVecPos )
{
	contract_assert( pVecPos );
	for ( int i = 0; i < 3; ++i )
	{
		(*pVecPos)[ i ] = clamp( (*pVecPos)[ i ], MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	}
}

//--------------------------------------------------------------------------------------------------------
/**
 * Return true if ground is fairly level within the given radius around an entity
 * Trace 4 vertical hull-quadrants and test their collisions and ground heights and normals
 */
bool UTIL_IsGroundLevel( float radius, const Vector &position, float hullHeight, int mask, const CBaseEntity *ignore, bool debugTraces )
{
	const int subdivisions = 3;
	const int samples = subdivisions * subdivisions;
	
	// trace down below floor level to detect ledges and overhangs
	trace_t result[ samples ];
	float size = 2.0f * radius / subdivisions;

	int i, j, s = 0;
	float x, y = -radius;
	for( j=0; j<subdivisions; ++j )
	{
		x = -radius;
		for( int i=0; i<subdivisions; ++i )
		{
			UTIL_TraceHull( position + Vector( 0, 0, hullHeight ), 
							position + Vector( 0, 0, -100 ), 
							Vector( x, y, 0 ), 
							Vector( x + size, y + size, 10.0f ), 
							mask, ignore, COLLISION_GROUP_PLAYER_MOVEMENT, 
							&result[ s ] );

			if ( debugTraces )
			{
				NDebugOverlay::SweptBox( position + Vector( 0, 0, hullHeight ), 
										 result[ s ].endpos, 
										 Vector( x, y, 0 ), 
										 Vector( x + size, y + size, 10.0f ), 
										 vec3_angle, 255, 255, 0, 255, 5.0f );
			}

			++s;
			x += size;
		}

		y += size;
	}

// 	UTIL_TraceHull( GetPosition() + Vector( 0, 0, body->GetCrouchHullHeight() ), GetPosition() + Vector( 0, 0, -100 ), Vector( -radius, -radius, 0 ), Vector(      0,      0, 10.0f ), MASK_ZOMBIESOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &result[0] );
// 	UTIL_TraceHull( GetPosition() + Vector( 0, 0, body->GetCrouchHullHeight() ), GetPosition() + Vector( 0, 0, -100 ), Vector( -radius,       0, 0 ), Vector(      0, radius, 10.0f ), MASK_ZOMBIESOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &result[1] );
// 	UTIL_TraceHull( GetPosition() + Vector( 0, 0, body->GetCrouchHullHeight() ), GetPosition() + Vector( 0, 0, -100 ), Vector(       0,       0, 0 ), Vector( radius, radius, 10.0f ), MASK_ZOMBIESOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &result[2] );
// 	UTIL_TraceHull( GetPosition() + Vector( 0, 0, body->GetCrouchHullHeight() ), GetPosition() + Vector( 0, 0, -100 ), Vector(       0, -radius, 0 ), Vector( radius,      0, 10.0f ), MASK_ZOMBIESOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &result[3] );

	for( i=0; i<samples; ++i )
	{
		if ( result[i].startsolid )
		{
			return false;
		}

		if ( result[i].DidHit() )
		{
			const float flat = 0.9f;
			if ( result[i].plane.normal.z < flat )
			{
				// too steep to sit here
				return false;
			}

			const float maxZDelta = 10.0f;
			for( j = 0; j<samples; ++j )
			{
				if ( i != j && abs( result[i].endpos.z - result[j].endpos.z ) > maxZDelta )
				{
					return false;
				}
			}
		}
	}

	return true;
}

void UTIL_SendClientCommandKVToPlayer( KeyValues *pKV, CBasePlayer *pPlayer )
{
	KeyValues::AutoDelete autodelete( pKV );
	if ( pPlayer )
	{
		engine->ClientCommandKeyValues( pPlayer->edict(), pKV->MakeCopy() );
	}
	else
	{
		// On server, send to all players
		for (int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				engine->ClientCommandKeyValues( pPlayer->edict(), pKV->MakeCopy() );
			}
		}
	}
}

void UTIL_RecordAchievementEvent( const char *pszAchievementname, CBasePlayer *pPlayer /*= NULL*/ )
{
	bool bIsWriteStat = ( std::strchr( pszAchievementname, '@' ) || strchr( pszAchievementname, '[' ) || strchr( pszAchievementname, '(' ) ); // Achievements cannot have @[( symbols in names - it's a stat
	KeyValues *pKV; 
	if ( bIsWriteStat )
		pKV = new KeyValues( "write_stats", pszAchievementname, 1 );
	else
		pKV = new KeyValues( "write_awards", pszAchievementname, 1 );

	UTIL_SendClientCommandKVToPlayer( pKV, pPlayer );
}

//=============================================================================
//
// Tests!
//

static constexpr auto NUM_KDTREE_TESTS = 2500;
static constexpr auto NUM_KDTREE_ENTITY_SIZE = 256;

void CC_KDTreeTest( const CCommand &args )
{
	Msg( "Testing kd-tree entity queries." );

	// Get the testing spot.
//	CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_start" );
//	Vector vecStart = pSpot->GetAbsOrigin();

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( UTIL_GetLocalPlayer() );
	Vector vecStart = pPlayer->GetAbsOrigin();

	static Vector *vecTargets = nullptr;
	static bool bFirst = true;

	// Generate the targets - rays (1K long).
	if ( bFirst )
	{
		vecTargets = new Vector [NUM_KDTREE_TESTS];
		double flRadius = 0;
		double flTheta = 0;
		double flPhi = 0;
		for ( int i = 0; i < NUM_KDTREE_TESTS; ++i )
		{
			flRadius += NUM_KDTREE_TESTS * 123.123;
			flRadius =  std::fmod( flRadius, 128.0 );
			flRadius =  fabs( flRadius );

			flTheta  += NUM_KDTREE_TESTS * 76.76;
			flTheta  =  fmod( flTheta, (double) DEG2RAD( 360 ) );
			flTheta  =  fabs( flTheta );

			flPhi    += NUM_KDTREE_TESTS * 1997.99;
			flPhi    =  fmod( flPhi, (double) DEG2RAD( 180 ) );
			flPhi    =  fabs( flPhi );

			float st, ct, sp, cp;
			SinCos( flTheta, &st, &ct );
			SinCos( flPhi, &sp, &cp );

			vecTargets[i].x = flRadius * ct * sp;
			vecTargets[i].y = flRadius * st * sp;
			vecTargets[i].z = flRadius * cp;
			
			// Make the trace 1024 units long.
			Vector vecDir = vecTargets[i] - vecStart;
			VectorNormalize( vecDir );
			vecTargets[i] = vecStart + vecDir * 1024;
		}

		bFirst = false;
	}

	int nTestType = 0;
	if ( args.ArgC() >= 2 )
	{
		nTestType = std::atoi( args[ 1 ] );
	}

	vtune( true );

	g_VProfCurrentProfile.Resume();
	g_VProfCurrentProfile.Start();
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.MarkFrame();

	switch ( nTestType )
	{
	case 0:
		{
			CVProfScope _( "TraceTotal", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );			

			trace_t trace;
			for ( int iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				UTIL_TraceLine( vecStart, vecTargets[iTest], MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &trace );
			}
			break;
		}
	case 1:
		{
			CVProfScope _( "TraceTotal", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

			trace_t trace;
			for ( int iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				UTIL_TraceHull( vecStart, vecTargets[iTest], VEC_HULL_MIN, VEC_HULL_MAX, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );
			}
			break;
		}
	case 2:
		{
			Vector vecMins[NUM_KDTREE_TESTS];
			Vector vecMaxs[NUM_KDTREE_TESTS];
			int iTest;
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				vecMins[iTest] = vecStart;
				vecMaxs[iTest] = vecStart;
				for ( int iAxis = 0; iAxis < 3; ++iAxis )
				{
					if ( vecTargets[iTest].x < vecMins[iTest].x ) { vecMins[iTest].x = vecTargets[iTest].x; }
					if ( vecTargets[iTest].y < vecMins[iTest].y ) { vecMins[iTest].y = vecTargets[iTest].y; }
					if ( vecTargets[iTest].z < vecMins[iTest].z ) { vecMins[iTest].z = vecTargets[iTest].z; }

					if ( vecTargets[iTest].x > vecMaxs[iTest].x ) { vecMaxs[iTest].x = vecTargets[iTest].x; }
					if ( vecTargets[iTest].y > vecMaxs[iTest].y ) { vecMaxs[iTest].y = vecTargets[iTest].y; }
					if ( vecTargets[iTest].z > vecMaxs[iTest].z ) { vecMaxs[iTest].z = vecTargets[iTest].z; }
				}
			}

			CVProfScope _( "TraceTotal", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

			int nCount = 0;

			Vector vecDelta;
			trace_t trace;
			CBaseEntity *pList[1024];
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				nCount += UTIL_EntitiesInBox( pList, 1024, vecMins[iTest], vecMaxs[iTest], 0 );
			}

			Msg( "Count = %d\n", nCount );
			break;
		}
	case 3:
		{
			Vector vecDelta;
			float flRadius[NUM_KDTREE_TESTS];
			int iTest;
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				VectorSubtract( vecTargets[iTest], vecStart, vecDelta );
				flRadius[iTest] = vecDelta.Length() * 0.5f;
			}

			CVProfScope _( "TraceTotal", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

			int nCount = 0;

			trace_t trace;
			CBaseEntity *pList[1024];
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				nCount += UTIL_EntitiesInSphere( pList, 1024, vecStart, flRadius[iTest], 0 );
			}

			Msg( "Count = %d\n", nCount );
			break;
		}
	default:
		{
			break;
		}
	}

	g_VProfCurrentProfile.MarkFrame();
	g_VProfCurrentProfile.Pause();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL );
	
	vtune( false );
}

static ConCommand kdtree_test( side_registry(), "kdtree_test", CC_KDTreeTest, "Tests spatial partition for entities queries.", FCVAR_CHEAT );

void CC_VoxelTreeView( void )
{
	Msg( "VoxelTreeView\n" );
	::partition->RenderAllObjectsInTree( 10.0f );
}

static ConCommand voxeltree_view( side_registry(), "voxeltree_view", CC_VoxelTreeView, "View entities in the voxel-tree.", FCVAR_CHEAT );

void CC_VoxelTreePlayerView( void )
{
	Msg( "VoxelTreePlayerView\n" );

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( UTIL_GetLocalPlayer() );
	Vector vecStart = pPlayer->GetAbsOrigin();
	::partition->RenderObjectsInPlayerLeafs( vecStart - VEC_HULL_MIN, vecStart + VEC_HULL_MAX, 3.0f  );
}

static ConCommand voxeltree_playerview( side_registry(), "voxeltree_playerview", CC_VoxelTreePlayerView, "View entities in the voxel-tree at the player position.", FCVAR_CHEAT );

void CC_VoxelTreeBox( const CCommand &args )
{
	Vector vecMin, vecMax;
	if ( args.ArgC() >= 6 )
	{
		vecMin.x = std::atof( args[ 1 ] );
		vecMin.y = atof( args[ 2 ] );
		vecMin.z = atof( args[ 3 ] );

		vecMax.x = atof( args[ 4 ] );
		vecMax.y = atof( args[ 5 ] );
		vecMax.z = atof( args[ 6 ] );
	}
	else
	{
		return;
	}

	float flTime = 10.0f;

	Vector vecPoints[8];
	vecPoints[0].Init( vecMin.x, vecMin.y, vecMin.z );
	vecPoints[1].Init( vecMin.x, vecMax.y, vecMin.z );
	vecPoints[2].Init( vecMax.x, vecMax.y, vecMin.z );
	vecPoints[3].Init( vecMax.x, vecMin.y, vecMin.z );
	vecPoints[4].Init( vecMin.x, vecMin.y, vecMax.z );
	vecPoints[5].Init( vecMin.x, vecMax.y, vecMax.z );
	vecPoints[6].Init( vecMax.x, vecMax.y, vecMax.z );
	vecPoints[7].Init( vecMax.x, vecMin.y, vecMax.z );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[1], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[2], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[3], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[0], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[4], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[5], vecPoints[6], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[6], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[7], vecPoints[4], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[4], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[6], 255, 0, 0, true, flTime );

	Msg( "VoxelTreeBox - (%f %f %f) to (%f %f %f)\n", vecMin.x, vecMin.y, vecMin.z, vecMax.x, vecMax.y, vecMax.z );
	::partition->RenderObjectsInBox( vecMin, vecMax, flTime );
}

static ConCommand voxeltree_box( side_registry(), "voxeltree_box", CC_VoxelTreeBox, "View entities in the voxel-tree inside box <Vector(min), Vector(max)>.", FCVAR_CHEAT );

void CC_VoxelTreeSphere( const CCommand &args )
{
	Vector vecCenter;
	float flRadius;
	if ( args.ArgC() >= 4 )
	{
		vecCenter.x = std::atof( args[ 1 ] );
		vecCenter.y = atof( args[ 2 ] );
		vecCenter.z = atof( args[ 3 ] );

		flRadius = atof( args[ 3 ] );
	}
	else
	{
		return;
	}

	float flTime = 3.0f;

	Vector vecMin, vecMax;
	vecMin.Init( vecCenter.x - flRadius, vecCenter.y - flRadius, vecCenter.z - flRadius );
	vecMax.Init( vecCenter.x + flRadius, vecCenter.y + flRadius, vecCenter.z + flRadius );

	Vector vecPoints[8];
	vecPoints[0].Init( vecMin.x, vecMin.y, vecMin.z );
	vecPoints[1].Init( vecMin.x, vecMax.y, vecMin.z );
	vecPoints[2].Init( vecMax.x, vecMax.y, vecMin.z );
	vecPoints[3].Init( vecMax.x, vecMin.y, vecMin.z );
	vecPoints[4].Init( vecMin.x, vecMin.y, vecMax.z );
	vecPoints[5].Init( vecMin.x, vecMax.y, vecMax.z );
	vecPoints[6].Init( vecMax.x, vecMax.y, vecMax.z );
	vecPoints[7].Init( vecMax.x, vecMin.y, vecMax.z );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[1], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[2], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[3], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[0], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[4], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[5], vecPoints[6], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[6], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[7], vecPoints[4], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[4], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[6], 255, 0, 0, true, flTime );

	Msg( "VoxelTreeSphere - (%f %f %f), %f\n", vecCenter.x, vecCenter.y, vecCenter.z, flRadius );
	::partition->RenderObjectsInSphere( vecCenter, flRadius, flTime );
}

static ConCommand voxeltree_sphere( side_registry(), "voxeltree_sphere", CC_VoxelTreeSphere, "View entities in the voxel-tree inside sphere <Vector(center), float(radius)>.", FCVAR_CHEAT );

static constexpr auto NUM_COLLISION_TESTS = 2500;
void CC_CollisionTest( const CCommand &args )
{
	if ( !physenv )
		return;

	Msg( "Testing collision system\n" );
	::partition->ReportStats( "" );
	int i;
	CBaseEntity *pSpot = gEntList.FindEntityByClassname( nullptr, "info_player_start");
	Vector start = pSpot->GetAbsOrigin();
	static Vector *targets = nullptr;
	static bool first = true;
	static float test[2] = {1,1};
	if ( first )
	{
		targets = new Vector[NUM_COLLISION_TESTS];
		float radius = 0;
		float theta = 0;
		float phi = 0;
		for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
		{
			radius += NUM_COLLISION_TESTS * 123.123;
			radius = fabs(std::fmod(radius, 128));
			theta += NUM_COLLISION_TESTS * 76.76;
			theta = fabs(fmod(theta, DEG2RAD(360)));
			phi += NUM_COLLISION_TESTS * 1997.99;
			phi = fabs(fmod(phi, DEG2RAD(180)));
			
			float st, ct, sp, cp;
			SinCos( theta, &st, &ct );
			SinCos( phi, &sp, &cp );

			targets[i].x = radius * ct * sp;
			targets[i].y = radius * st * sp;
			targets[i].z = radius * cp;
			
			// make the trace 1024 units long
			Vector dir = targets[i] - start;
			VectorNormalize(dir);
			targets[i] = start + dir * 1024;
		}
		first = false;
	}

	//Vector results[NUM_COLLISION_TESTS];

	int testType = 0;
	if ( args.ArgC() >= 2 )
	{
		testType = std::atoi(args[1]);
	}
	float duration = 0;
	Vector size[2];
	size[0].Init(0,0,0);
	size[1].Init(16,16,16);
	unsigned int dots = 0;
	int nMask = MASK_ALL & ~(CONTENTS_MONSTER | CONTENTS_HITBOX );
	for ( int j = 0; j < 2; j++ )
	{
		float startTime = Plat_FloatTime();
		if ( testType == 1 )
		{
			trace_t tr;
			for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
			{
				UTIL_TraceHull( start, targets[i], -size[1], size[1], nMask, nullptr, COLLISION_GROUP_NONE, &tr );
			}
		}
		else
		{
			testType = 0;
			trace_t tr;

			for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
			{
				if ( i == 0 )
				{
					::partition->RenderLeafsForRayTraceStart( 10.0f );
				}

				UTIL_TraceLine( start, targets[i], nMask, nullptr, COLLISION_GROUP_NONE, &tr );

				if ( i == 0 )
				{
					::partition->RenderLeafsForRayTraceEnd( );
				}
			}
		}

		duration += Plat_FloatTime() - startTime;
	}
	test[testType] = duration;
	Msg("%d collisions in %.2f ms (%u dots)\n", NUM_COLLISION_TESTS, duration*1000, dots );
	::partition->ReportStats( "" );
	int red = 255, green = 0, blue = 0;
	for ( i = 0; i < 1 /*NUM_COLLISION_TESTS*/; i++ )
	{
		NDebugOverlay::Line( start, targets[i], red, green, blue, false, 2 );
	}
}

void UTIL_EnsureInstanceBaseline( const char *szClassname )
{
	CBaseEntity *pEnt = CreateEntityByName( szClassname );
	if ( pEnt )
	{
		engine->EnsureInstanceBaseline( pEnt->entindex() );
		UTIL_RemoveImmediate( pEnt );
	}
}

static ConCommand collision_test(side_registry(), "collision_test", CC_CollisionTest, "Tests collision system", FCVAR_CHEAT );

class CBaseEntity;
class CBasePlayer;
class CEffectData;
struct FindClosestPassableSpace_TraceAdapter_t;

using namespace ::ks::reflect;

bool NPC_CheckBrushExclude( CBaseEntity *pEntity, CBaseEntity *pBrush );

ConVar r_visualizetraces( side_registry(), "r_visualizetraces", "0", FCVAR_CHEAT );
ConVar developer(side_registry(), "developer", "0", FCVAR_RELEASE, "Set developer message level" ); // developer mode

float UTIL_VecToYaw( const Vector &vec )
{
	if (vec.y == 0 && vec.x == 0)
		return 0;
	
	float yaw = std::atan2( vec.y, vec.x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

float UTIL_VecToPitch( const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		pitch = RAD2DEG( std::asin( -tmp.z ) );
	}
	return pitch;
}

float UTIL_VecToYaw( const matrix3x4_t &matrix, const Vector &vec )
{
	Vector tmp = vec;
	VectorNormalize( tmp );

	float x = matrix[0][0] * tmp.x + matrix[1][0] * tmp.y + matrix[2][0] * tmp.z;
	float y = matrix[0][1] * tmp.x + matrix[1][1] * tmp.y + matrix[2][1] * tmp.z;

	if (x == 0.0f && y == 0.0f)
		return 0.0f;
	
	float yaw = std::atan2( -y, x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

float UTIL_VecToPitch( const matrix3x4_t &matrix, const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		float z = matrix[0][2] * tmp.x + matrix[1][2] * tmp.y + matrix[2][2] * tmp.z;
		pitch = RAD2DEG( std::asin( -z ) );
		if (pitch < 0)
			pitch += 360;
	}
	return pitch;
}

Vector UTIL_YawToVector( float yaw )
{
	Vector ret;
	
	ret.z = 0;
	float angle = DEG2RAD( yaw );
	SinCos( angle, &ret.y, &ret.x );

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function get get determinisitc random values for shared/prediction code
// Input  : seedvalue - 
//			*module - 
//			line - 
// Output : static int
//-----------------------------------------------------------------------------
static int SeedFileLineHash( int seedvalue, const char *sharedname, int additionalSeed )
{
	CRC32_t retval;

	CRC32_Init( &retval );

	//ensure cross-platform agreement
	seedvalue = ( seedvalue );
	additionalSeed = ( additionalSeed );

	CRC32_ProcessBuffer( &retval, (void *)&seedvalue, sizeof( int ) );
	CRC32_ProcessBuffer( &retval, (void *)&additionalSeed, sizeof( int ) );
	CRC32_ProcessBuffer( &retval, (void *)sharedname, V_strlen( sharedname ) );
	
	CRC32_Final( &retval );

	return (int)( retval );
}

float SharedRandomFloat( const char *sharedname, float flMinVal, float flMaxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	return RandomFloat( flMinVal, flMaxVal );
}

int SharedRandomInt( const char *sharedname, int iMinVal, int iMaxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	return RandomInt( iMinVal, iMaxVal );
}

Vector SharedRandomVector( const char *sharedname, float minVal, float maxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	// HACK:  Can't call RandomVector/Angle because it uses rand() not vstlib Random*() functions!
	// Get a random vector.
	Vector random_valve;
	random_valve.x = RandomFloat( minVal, maxVal );
	random_valve.y = RandomFloat( minVal, maxVal );
	random_valve.z = RandomFloat( minVal, maxVal );
	return random_valve;
}

QAngle SharedRandomAngle( const char *sharedname, float minVal, float maxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );

	// HACK:  Can't call RandomVector/Angle because it uses rand() not vstlib Random*() functions!
	// Get a random vector.
	Vector random_valve;
	random_valve.x = RandomFloat( minVal, maxVal );
	random_valve.y = RandomFloat( minVal, maxVal );
	random_valve.z = RandomFloat( minVal, maxVal );
	return QAngle( random_valve.x, random_valve.y, random_valve.z );
}

//-----------------------------------------------------------------------------
//
// Shared client/server trace filter code
//
//-----------------------------------------------------------------------------
bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass ) 
{
	if ( !pPass )
		return true;

	if ( pTouch == pPass )
		return false;

	const CBaseEntity *pEntTouch = EntityFromEntityHandle( pTouch );
	const CBaseEntity *pEntPass = EntityFromEntityHandle( pPass );
	if ( !pEntTouch || !pEntPass )
		return true;

	// don't clip against own missiles
	if ( pEntTouch->GetOwnerEntity() == pEntPass )
		return false;
	
	// don't clip against owner
	if ( pEntPass->GetOwnerEntity() == pEntTouch )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// A standard filter to be applied to just about everything.
//-----------------------------------------------------------------------------
bool StandardFilterRules( IHandleEntity *pHandleEntity, int fContentsMask )
{
	CBaseEntity *pCollide = EntityFromEntityHandle( pHandleEntity );

	// Static prop case...
	if ( !pCollide )
		return true;

	SolidType_t solid = pCollide->GetSolid();
	const model_t *pModel = pCollide->GetModel();

	if ( ( modelinfo->GetModelType( pModel ) != mod_brush ) || (solid != SOLID_BSP && solid != SOLID_VPHYSICS) )
	{
		if ( (fContentsMask & CONTENTS_MONSTER) == 0 )
			return false;
	}

	// This code is used to cull out tests against see-thru entities
	if ( !(fContentsMask & CONTENTS_WINDOW) )
	{
		bool bIsTranslucent = modelinfo->IsTranslucent( pModel );
		bool bIsTwoPass = modelinfo->IsTranslucentTwoPass( pModel );
		if ( bIsTranslucent && !bIsTwoPass )
			return false;
	}

	// FIXME: this is to skip BSP models that are entities that can be 
	// potentially moved/deleted, similar to a monster but doors don't seem to 
	// be flagged as monsters
	// FIXME: the FL_WORLDBRUSH looked promising, but it needs to be set on 
	// everything that's actually a worldbrush and it currently isn't
	if ( !(fContentsMask & CONTENTS_MOVEABLE) && (pCollide->GetMoveType() == MOVETYPE_PUSH))// !(touch->flags & FL_WORLDBRUSH) )
		return false;

	return true;
}

bool IsWeaponClassname( const char *pszClassName )
{
	// Look at the name of the class to determine if this is a weapon.
	return ( !V_strnicmp( "weapon_", pszClassName, WEAPON_CLASSNAME_PREFIX_LENGTH ) );
}

//-----------------------------------------------------------------------------
// Simple trace filter
//-----------------------------------------------------------------------------
CTraceFilterSimple::CTraceFilterSimple( const IHandleEntity *passedict, int collisionGroup,
										ShouldHitFunc_t pExtraShouldHitFunc )
{
	m_pPassEnt = passedict;
	m_collisionGroup = collisionGroup;
	m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;
}

//-----------------------------------------------------------------------------
// The trace filter!
//-----------------------------------------------------------------------------
bool CTraceFilterSimple::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( m_pPassEnt )
	{
		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		{
			return false;
		}
	}

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( !pEntity )
		return false;
	if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
		return false;
	if ( pEntity && !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
		return false;
	if ( m_pExtraShouldHitCheckFunction &&
		 (! ( m_pExtraShouldHitCheckFunction( pHandleEntity, contentsMask ) ) ) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter that doesn't hit NPCs or players
//-----------------------------------------------------------------------------
bool CTraceFilterNoNPCsOrPlayer::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;
}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter that doesn't hit players
//-----------------------------------------------------------------------------
bool CTraceFilterNoPlayers::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		return !pEntity->IsPlayer();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Trace filter that skips two entities
//-----------------------------------------------------------------------------
CTraceFilterSkipTwoEntities::CTraceFilterSkipTwoEntities( const IHandleEntity *passentity, const IHandleEntity *passentity2, int collisionGroup ) :
	BaseClass( passentity, collisionGroup ), m_pPassEnt2(passentity2)
{
}

bool CTraceFilterSkipTwoEntities::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	contract_assert( pHandleEntity );
	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt2 ) )
		return false;

	return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
CTraceFilterSimpleList::CTraceFilterSimpleList( int collisionGroup ) :
	CTraceFilterSimple( nullptr, collisionGroup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterSimpleList::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( ks::find_index( m_PassEntities, pHandleEntity ) != -1 )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Purpose: Add an entity to my list of entities to ignore in the trace
//-----------------------------------------------------------------------------
void CTraceFilterSimpleList::AddEntityToIgnore( IHandleEntity *pEntity )
{
	m_PassEntities.push_back( pEntity );
}

void CTraceFilterSimpleList::AddEntitiesToIgnore( int nCount, IHandleEntity **ppEntities )
{
	int nIndex = std::ssize( m_PassEntities ); m_PassEntities.resize( m_PassEntities.size() + nCount );
	std::memcpy( &m_PassEntities[nIndex], ppEntities, nCount * sizeof( IHandleEntity* ) );
}

//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for NPC LOS traces
//-----------------------------------------------------------------------------
CTraceFilterLOS::CTraceFilterLOS( IHandleEntity *pHandleEntity, int collisionGroup, IHandleEntity *pHandleEntity2 ) :
		CTraceFilterSkipTwoEntities( pHandleEntity, pHandleEntity2, collisionGroup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterLOS::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

	if ( !pEntity->BlocksLOS() )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Sweeps against a particular model, using collision rules 
//-----------------------------------------------------------------------------
void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
					  const Vector &hullMax, CBaseEntity *pentModel, int collisionGroup, trace_t *ptr )
{
	// Cull it....
	if ( pentModel && pentModel->ShouldCollide( collisionGroup, MASK_ALL ) )
	{
		Ray_t ray;
		ray.Init( vecStart, vecEnd, hullMin, hullMax );
		enginetrace->ClipRayToEntity( ray, MASK_ALL, pentModel, ptr ); 
	}
	else
	{
		std::memset( ptr, 0, sizeof(trace_t) );
		ptr->fraction = 1.0f;
	}
}

bool UTIL_EntityHasMatchingRootParent( CBaseEntity *pRootParent, CBaseEntity *pEntity )
{
	if ( pRootParent )
	{
		// NOTE: Don't let siblings/parents collide.
		if ( pRootParent == pEntity->GetRootMoveParent() )
			return true;
		if ( pEntity->GetOwnerEntity() && pRootParent == pEntity->GetOwnerEntity()->GetRootMoveParent() )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Sweep an entity from the starting to the ending position 
//-----------------------------------------------------------------------------
class CTraceFilterEntity : public CTraceFilterSimple
{
	DECLARE_CLASS( CTraceFilterEntity, CTraceFilterSimple );

public:
	CTraceFilterEntity( CBaseEntity *pEntity, int nCollisionGroup ) 
		: CTraceFilterSimple( pEntity, nCollisionGroup )
	{
		m_pRootParent = pEntity->GetRootMoveParent();
		m_pEntity = pEntity;
		m_checkHash = g_EntityCollisionHash->IsObjectInHash(pEntity);
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		// Check parents against each other
		// NOTE: Don't let siblings/parents collide.
		if ( UTIL_EntityHasMatchingRootParent( m_pRootParent, pEntity ) )
			return false;

		if ( m_checkHash )
		{
			if ( g_EntityCollisionHash->IsObjectPairInHash( m_pEntity, pEntity ) )
				return false;
		}

		

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:

	CBaseEntity *m_pRootParent;
	CBaseEntity *m_pEntity;
	bool		m_checkHash;
};

class CTraceFilterEntityIgnoreOther : public CTraceFilterEntity
{
	DECLARE_CLASS( CTraceFilterEntityIgnoreOther, CTraceFilterEntity );
public:
	CTraceFilterEntityIgnoreOther( CBaseEntity *pEntity, const IHandleEntity *pIgnore, int nCollisionGroup ) : 
		CTraceFilterEntity( pEntity, nCollisionGroup ), m_pIgnoreOther( pIgnore )
	{
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( pHandleEntity == m_pIgnoreOther )
			return false;

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:
	const IHandleEntity *m_pIgnoreOther;
};

//-----------------------------------------------------------------------------
// Sweeps a particular entity through the world 
//-----------------------------------------------------------------------------
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr )
{
	ICollideable *pCollision = pEntity->GetCollideable();
	//Vector vec = pCollision->OBBMaxs();
	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	contract_assert( pCollision->GetCollisionAngles() == vec3_angle );

	CTraceFilterEntity traceFilter( pEntity, pCollision->GetCollisionGroup() );

	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, &traceFilter, ptr );
}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, const IHandleEntity *pIgnore, int nCollisionGroup, trace_t *ptr )
{
	ICollideable *pCollision;
	pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	contract_assert( pCollision->GetCollisionAngles() == vec3_angle );

	CTraceFilterEntityIgnoreOther traceFilter( pEntity, pIgnore, nCollisionGroup );

	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, &traceFilter, ptr );
}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, ITraceFilter *pFilter, trace_t *ptr )
{
	ICollideable *pCollision;
	pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	contract_assert( pCollision->GetCollisionAngles() == vec3_angle );

	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, pFilter, ptr );
}

// ----
// This is basically a regular TraceLine that uses the FilterEntity filter.
void UTIL_TraceLineFilterEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					   unsigned int mask, int nCollisionGroup, trace_t *ptr )
{
	CTraceFilterEntity traceFilter( pEntity, nCollisionGroup );
	UTIL_TraceLine( vecAbsStart, vecAbsEnd, mask, &traceFilter, ptr );
}

void UTIL_ClipTraceToPlayers( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter *filter, trace_t *tr )
{
	trace_t playerTrace;
	Ray_t ray;
	float smallestFraction = tr->fraction;
	const float maxRange = 60.0f;

	ray.Init( vecAbsStart, vecAbsEnd );

	for ( int k = 1; k <= gpGlobals->maxClients; ++k )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( k );

		if ( !player || !player->IsAlive() )
			continue;

		if ( filter && filter->ShouldHitEntity( player, mask ) == false )
			continue;

		float range = DistanceToRay( player->WorldSpaceCenter(), vecAbsStart, vecAbsEnd );
		if ( range < 0.0f || range > maxRange )
			continue;

		enginetrace->ClipRayToEntity( ray, mask|CONTENTS_HITBOX, player, &playerTrace );
		if ( playerTrace.fraction < smallestFraction )
		{
			// we shortened the ray - save off the trace
			*tr = playerTrace;
			smallestFraction = playerTrace.fraction;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer using a particle effect
//-----------------------------------------------------------------------------
void UTIL_ParticleTracer( const char *pszTracerEffectName, const Vector &vecStart, const Vector &vecEnd, 
				 int iEntIndex, int iAttachment, bool bWhiz )
{
	int iParticleIndex = GetParticleSystemIndex( pszTracerEffectName );
	UTIL_Tracer( vecStart, vecEnd, iEntIndex, iAttachment, 0, bWhiz, "ParticleTracer", iParticleIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer effect using the old, non-particle system, tracer effects.
//-----------------------------------------------------------------------------
void UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex, 
				 int iAttachment, float flVelocity, bool bWhiz, const char *pCustomTracerName, int iParticleID )
{
	CEffectData data;
	data.m_vStart = vecStart;
	data.m_vOrigin = vecEnd;
	data.m_nEntIndex = iEntIndex;
	data.m_flScale = flVelocity;
	data.m_nHitBox = iParticleID;

	// Flags
	if ( bWhiz )
	{
		data.m_fFlags |= TRACER_FLAG_WHIZ;
	}

	if ( iAttachment != TRACER_DONT_USE_ATTACHMENT )
	{
		data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
		// Stomp the start, since it's not going to be used anyway
		data.m_nAttachmentIndex = iAttachment;
	}

	// Fire it off
	if ( pCustomTracerName )
	{
		DispatchEffect( pCustomTracerName, data );
	}
	else
	{
		DispatchEffect( "Tracer", data );
	}
}

void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 5;
	}

	if ( amount > 255 )
		amount = 255;

	if (color == BLOOD_COLOR_MECH)
	{
		g_pEffects->Sparks(origin);
		if (random_valve->RandomFloat(0, 2) >= 1)
		{
			UTIL_Smoke(origin, random_valve->RandomInt(10, 15), 10);
		}
	}
	else
	{
		// Normal blood impact
		UTIL_BloodImpact( origin, direction, color, amount );
	}
}	

//-----------------------------------------------------------------------------
// Purpose: Returns low violence settings
//-----------------------------------------------------------------------------
static ConVar	violence_hblood( side_registry(), "violence_hblood","1", 0, "Draw human blood" );
static ConVar	violence_hgibs( side_registry(), "violence_hgibs","1", 0, "Show human gib entities" );
static ConVar	violence_ablood( side_registry(), "violence_ablood","1", 0, "Draw alien blood" );
static ConVar	violence_agibs( side_registry(), "violence_agibs","1", 0, "Show alien gib entities" );

bool UTIL_IsLowViolence( void )
{
	// These convars are no longer necessary -- the engine is the final arbiter of
	// violence settings -- but they're here for legacy support and for testing low
	// violence when the engine is in normal violence mode.
	if ( !violence_hblood.GetBool() || !violence_ablood.GetBool() || !violence_hgibs.GetBool() || !violence_agibs.GetBool() )
		return true;

	return engine->IsLowViolence();
}

bool UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			return violence_hblood.GetBool();
		}
		else
		{
			return violence_ablood.GetBool();
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Purpose : Use trace to pass a specific decal type to the entity being decaled
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_DecalTrace( trace_t *pTrace, char const *decalName )
{
	if (pTrace->fraction == 1.0)
		return;

	CBaseEntity *pEntity = pTrace->Ent<CBaseEntity>();
	if ( !pEntity )
		return;
	pEntity->DecalTrace( pTrace, decalName );
}

void UTIL_BloodDecalTrace( trace_t *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
		{
			UTIL_DecalTrace( pTrace, "Blood" );
		}
		else if (bloodColor == BLOOD_COLOR_BRIGHTGREEN)
		{				
			UTIL_DecalTrace( pTrace, "GreenBlood" );
		}
		else
		{
			UTIL_DecalTrace( pTrace, "YellowBlood" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&dir - 
//			color - 
//			amount - 
//-----------------------------------------------------------------------------
void UTIL_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount )
{
	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_nColor = (unsigned char)color;

	DispatchEffect( "bloodimpact", data );
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	int mask = (pMainEnt) ? pMainEnt->PhysicsSolidMaskForEntity() : MASK_SOLID;
	UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, mask, pMainEnt, COLLISION_GROUP_NONE, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax, unsigned int mask, ITraceFilter *pFilter )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, mask, pFilter, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z = minz;

	if ( !(UTIL_PointContents(midUp, MASK_WATER) & MASK_WATER) )
		return minz;

	midUp.z = maxz;
	if ( UTIL_PointContents(midUp, MASK_WATER) & MASK_WATER )
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff/2.0;
		if ( UTIL_PointContents(midUp, MASK_WATER) & MASK_WATER )
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}

//-----------------------------------------------------------------------------
// Like UTIL_WaterLevel, but *way* less expensive.
// I didn't replace UTIL_WaterLevel everywhere to avoid breaking anything.
//-----------------------------------------------------------------------------
class CWaterTraceFilter : public CTraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pCollide = EntityFromEntityHandle( pHandleEntity );

		// Static prop case...
		if ( !pCollide )
			return false;

		// Only impact water stuff...
		if ( pCollide->GetSolidFlags() & FSOLID_VOLUME_CONTENTS )
			return true;

		return false;
	}
};

float UTIL_FindWaterSurface( const Vector &position, float minz, float maxz )
{
	Vector vecStart, vecEnd;
	vecStart.Init( position.x, position.y, maxz );
	vecEnd.Init( position.x, position.y, minz );

	Ray_t ray;
	trace_t tr;
	CWaterTraceFilter waterTraceFilter;
	ray.Init( vecStart, vecEnd );
	enginetrace->TraceRay( ray, MASK_WATER, &waterTraceFilter, &tr );

	return tr.endpos.z;
}

void UTIL_StringToFloatArray( float *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	V_strncpy( tempString, pString, sizeof(tempString) );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = std::atof( pfront );

		// skip any leading whitespace
		while ( *pstr && *pstr <= ' ' )
			pstr++;

		// skip to next whitespace
		while ( *pstr && *pstr > ' ' )
			pstr++;

		if (!*pstr)
			break;

		pstr++;
		pfront = pstr;
	}
	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	UTIL_StringToFloatArray( pVector, 3, pString );
}

void UTIL_DecodeICE( unsigned char * buffer, int size, const unsigned char *key )
{
	ks::ScratchArena<> scratch;
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	int blockSize = ice.blockSize();

	unsigned char *temp = (unsigned char *) scratch.AllocBytes( PAD_NUMBER( size, blockSize ) );
	unsigned char *p1 = buffer;
	unsigned char *p2 = temp;
				
	// encrypt data in 8 byte blocks
	int bytesLeft = size;
	while ( bytesLeft >= blockSize )
	{
		ice.decrypt( p1, p2 );
		bytesLeft -= blockSize;
		p1+=blockSize;
		p2+=blockSize;
	}

	// copy encrypted data back to original buffer
	V_memcpy( buffer, temp, size-bytesLeft );
}

void UTIL_EncodeICE( unsigned char * buffer, unsigned int size, const unsigned char *key )
{
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	unsigned char *cipherText = buffer;
	unsigned char *plainText = buffer;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < size)
	{
		ice.encrypt( plainText, cipherText );
		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
}

// work-around since client header doesn't like inlined gpGlobals->curtime
float IntervalTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

// work-around since client header doesn't like inlined gpGlobals->curtime
float CountdownTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

IMPLEMENT_REFLECT_TABLE( IntervalTimer, DT_IntervalTimer );

IMPLEMENT_REFLECT_TABLE( CountdownTimer, DT_CountdownTimer );

IMPLEMENT_REFLECT_TABLE( CTimeline, DT_Timeline );

void CTimeline::ClearValues( void )
{
	Invalidate();

	std::memset( m_flValues.m_Value, 0, sizeof( m_flValues.m_Value ) );
	memset( m_nValueCounts.m_Value, 0, sizeof( m_nValueCounts.m_Value ) );
	m_nBucketCount = 0;
	m_flInterval = TIMELINE_INTERVAL_START;
	m_flFinalValue = 0.0f;
	m_bStopped = false;
}

void CTimeline::RecordValue( float flValue )
{
	if ( !HasStarted() || m_bStopped )
		return;

	int iBucket = GetCurrentBucket();

	contract_assert( iBucket >= 0 );

	while ( iBucket >= TIMELINE_ARRAY_SIZE )
	{
		Compress();
		iBucket = GetCurrentBucket();
	}

	if ( iBucket >= m_nBucketCount )
	{
		m_nBucketCount = iBucket + 1;
	}

	m_flValues.GetForModify( iBucket ) += flValue;
	m_nValueCounts.GetForModify( iBucket )++;

	if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM || 
		 m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE || 
		 m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE_BLEND )
	{
		// Fill in blank preceding entries
		float flCurrentValue = m_flValues[ iBucket ] / m_nValueCounts[ iBucket ];

		int iPrecedingBucket = iBucket - 1;

		while ( iPrecedingBucket >= 0 && m_nValueCounts[ iPrecedingBucket ] <= 0 )
		{
			iPrecedingBucket--;
		}

		// Get the last logged value (or the current if this is the first)
		float flPrecedingValue = flCurrentValue;

		if ( iPrecedingBucket >= 0 )
		{
			// Last logged value
			if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM )
			{
				flPrecedingValue = m_flValues[ iPrecedingBucket ];

				if ( m_nValueCounts[ iBucket ] == 1 )
				{
					// Sum in the previous bucket if this is the first value in this bucket
					m_flValues.GetForModify( iBucket ) += flPrecedingValue;
				}
			}
			else
			{
				flPrecedingValue = m_flFinalValue;
			}
		}

		// Number of buckets for blending from old to new value
		float flNumBuckets = ( iBucket - iPrecedingBucket ) + 1;

		if ( flNumBuckets >= 3.0f )
		{
			if ( m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE_BLEND )
			{
				// Blend empty values between preceding and current
				float flInterpBucket = 1.0f;

				for ( int i = iPrecedingBucket + 1; i < iBucket; ++i, flInterpBucket += 1.0f )
				{
					float flInterp = flInterpBucket / flNumBuckets;
					m_flValues.Set( i, flPrecedingValue * ( 1.0f - flInterp ) + flCurrentValue * flInterp );
					m_nValueCounts.Set( i, 1 );
				}
			}
			else
			{
				// Set empty values to preceding value
				for ( int i = iPrecedingBucket + 1; i < iBucket; ++i )
				{
					m_flValues.Set( i, flPrecedingValue );
					m_nValueCounts.Set( i, 1 );
				}
			}
		}
	}

	m_flFinalValue = flValue;
}

float CTimeline::GetValue( int i ) const
{
	contract_assert( i >= 0 && i < m_nBucketCount );

	if ( i < 0 || i >= m_nBucketCount )
	{
		return 0.0f;
	}

	if ( m_nValueCounts[ i ] <= 0 )
	{
		return 0.0f;
	}
	else if ( i == 0 && m_nCompressionType == TIMELINE_COMPRESSION_SUM && m_nBucketCount > 1 )
	{
		// Aways start at 0 for sums!
		return 0.0;
	}
	else
	{
		switch ( m_nCompressionType )
		{
		case TIMELINE_COMPRESSION_AVERAGE:
		case TIMELINE_COMPRESSION_AVERAGE_BLEND:
			return m_flValues[ i ] / m_nValueCounts[ i ];

		case TIMELINE_COMPRESSION_SUM:
		case TIMELINE_COMPRESSION_COUNT_PER_INTERVAL:
		default:
			return m_flValues[ i ];
		}
	}
}

float CTimeline::GetValueAtInterp( float fInterp ) const
{
	if ( fInterp <= 0.0f )
	{
		return GetValue( 0 );
	}

	if ( fInterp >= 1.0f )
	{
		if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM || 
			 m_nCompressionType == TIMELINE_COMPRESSION_COUNT_PER_INTERVAL )
		{
			return GetValue( Count() - 1 );
		}
		else
		{
			return m_flFinalValue;
		}
	}

	float fBucket = fInterp * ( Count() - 1 );
	int nBucket = fBucket;
	fBucket -= nBucket;

	float fValue = GetValue( nBucket );
	float fNextValue = GetValue( nBucket + 1 );

	return fValue * ( 1.0f - fBucket ) + fNextValue * fBucket;
}

void CTimeline::Compress( void )
{
	int i, j;

	switch ( m_nCompressionType )
	{
	case TIMELINE_COMPRESSION_SUM:
		for ( i = 0, j = 0; i < TIMELINE_ARRAY_SIZE; i += 2, ++j )
		{
			m_flValues.GetForModify( j ) = std::max( m_flValues[ i ], m_flValues[ i + 1 ] );
			m_nValueCounts.GetForModify( j ) = m_nValueCounts[ i ] + m_nValueCounts[ i + 1 ];
		}
		break;

	default:
		for ( i = 0, j = 0; i < TIMELINE_ARRAY_SIZE; i += 2, ++j )
		{
			m_flValues.GetForModify( j ) = m_flValues[ i ] + m_flValues[ i + 1 ];
			m_nValueCounts.GetForModify( j ) = m_nValueCounts[ i ] + m_nValueCounts[ i + 1 ];
		}
		break;
	}

	int nRemainingBytes = ( TIMELINE_ARRAY_SIZE - j ) * sizeof( m_flValues[0] );

	std::memset( &( m_flValues.GetForModify( j ) ), 0, nRemainingBytes );
	memset( &( m_nValueCounts.GetForModify( j ) ), 0, nRemainingBytes );

	m_flInterval *= 2.0f;

	m_nBucketCount = j;
}

unsigned short UTIL_GetAchievementEventMask( void )
{
	CRC32_t mapCRC;
	CRC32_Init( &mapCRC );

	char lowercase[ 256 ];
	V_strncpy( lowercase, STRING( gpGlobals->mapname ), sizeof( lowercase ) );
	V_strlower( lowercase );

	CRC32_ProcessBuffer( &mapCRC, lowercase, V_strlen( lowercase ) );
	CRC32_Final( &mapCRC );

	return ( mapCRC & 0xFFFF );
}

const char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename )
{
	const char *pValue = pSub->GetString( pName, nullptr );
	if ( !pValue )
	{
		if ( pFilename )
		{
			DevWarning( "Can't get key value	'%s' from file '%s'.\n", pName, pFilename );
		}
		return "";
	}

	int len = V_strlen( pValue ) + 1;
	char *pAlloced = new char[ len ];
	contract_assert( pAlloced );
	V_strncpy( pAlloced, pValue, len );
	return pAlloced;
}

int UTIL_StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings )
{
	if ( !szValue || !szValue[0] )
		return -1;

	for ( int i = 0; i < iNumStrings; i++ )
	{
		if ( FStrEq(szValue, pValueStrings[i]) )
			return i;
	}

	contract_assert( 0 );
	return -1;
}

static char s_NumBitsInNibble[ 16 ] = 
{
	0, // 0000 = 0
	1, // 0001 = 1
	1, // 0010 = 2
	2, // 0011 = 3
	1, // 0100 = 4
	2, // 0101 = 5
	2, // 0110 = 6
	3, // 0111 = 7
	1, // 1000 = 8
	2, // 1001 = 9
	2, // 1010 = 10
	3, // 1011 = 11
	2, // 1100 = 12
	3, // 1101 = 13
	3, // 1110 = 14
	4, // 1111 = 15
};

int UTIL_CountNumBitsSet( unsigned int nVar )
{
	int nNumBits = 0;

	while ( nVar > 0 )
	{
		// Look up and add in bits in the bottom nibble
		nNumBits += s_NumBitsInNibble[ nVar & 0x0f ];

		// Shift one nibble to the right
		nVar >>= 4;
	}

	return nNumBits;
}

int UTIL_CountNumBitsSet( uint64 nVar )
{
	int nNumBits = 0;

	while ( nVar > 0 )
	{
		// Look up and add in bits in the bottom nibble
		nNumBits += s_NumBitsInNibble[ nVar & 0x0f ];

		// Shift one nibble to the right
		nVar >>= 4;
	}

	return nNumBits;
}

bool UTIL_FindClosestPassableSpace( const Vector &vOriginalCenter, const Vector &vExtents, const Vector &vIndecisivePush, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	contract_assert( vExtents != vec3_origin );

	trace_t traces[2];
	Ray_t entRay;
	entRay.m_Extents = vExtents;
	entRay.m_IsRay = false;
	entRay.m_IsSwept = true;
	entRay.m_StartOffset = vec3_origin;

	Vector vOriginalExtents = vExtents;
	Vector vCenter = vOriginalCenter;
	Vector vGrowSize = vExtents * (1.0f / (float)(iIterations + 1));
	Vector vCurrentExtents = vExtents - vGrowSize;

	int iLargestExtent = 0;
	{
		float fLargestExtent = vOriginalExtents[0];
		for( int i = 1; i != 3; ++i )
		{
			if( vOriginalExtents[i] > fLargestExtent )
			{
				iLargestExtent = i;
				fLargestExtent = vOriginalExtents[i];
			}
		}
	}

	Ray_t testRay;
	testRay.m_Extents = vGrowSize;
	testRay.m_IsRay = false;
	testRay.m_IsSwept = true;
	testRay.m_StartOffset = vec3_origin;

	float fOriginalExtentDists[8]; //distance between extents
	//generate distance lookup. We reference this by XOR'ing the indices of two extents to find the axis of difference
	{
		//Since the ratios of lengths never change, we're going to normalize these distances to a value so we can simply scale on each iteration
		//We've picked the largest extent as the basis simply because it's nonzero
		float fNormalizer = 1.0f / vOriginalExtents[iLargestExtent];
				
		float fXDiff = vOriginalExtents.x * 2.0f * fNormalizer;
		float fXSqr = fXDiff * fXDiff;

		float fYDiff = vOriginalExtents.y * 2.0f * fNormalizer;
		float fYSqr = fYDiff * fYDiff;

		float fZDiff = vOriginalExtents.z * 2.0f * fNormalizer;
		float fZSqr = fZDiff * fZDiff;

		fOriginalExtentDists[0] = 0.0f; //should never get hit
		fOriginalExtentDists[1] = fXDiff; //line along x axis		
		fOriginalExtentDists[2] = fYDiff; //line along y axis
		fOriginalExtentDists[3] = std::sqrt( fXSqr + fYSqr ); //diagonal perpendicular to z-axis
		fOriginalExtentDists[4] = fZDiff; //line along z axis
		fOriginalExtentDists[5] = sqrt( fXSqr + fZSqr ); //diagonal perpendicular to y-axis
		fOriginalExtentDists[6] = sqrt( fYSqr + fZSqr ); //diagonal perpendicular to x-axis
		fOriginalExtentDists[7] = sqrt( fXSqr + fYSqr + fZSqr ); //diagonal on all axes
	}

	Vector ptExtents[8]; //ordering is going to be like 3 bits, where 0 is a min on the related axis, and 1 is a max on the same axis, axis order x y z
	float fExtentsValidation[8]; //some points are more valid than others, and this is our measure

	vCenter.z += 0.001f; //to satisfy m_IsSwept on first pass
	
	unsigned int iFailCount;
	for( iFailCount = 0; iFailCount != iIterations; ++iFailCount )
	{
		//float fXDistribution[2] = { -vCurrentExtents.x, vCurrentExtents.x };
		//float fYDistribution[3] = { -vCurrentExtents.y, 0.0f, vCurrentExtents.y };
		//float fZDistribution[5] = { -vCurrentExtents.z, 0.0f, 0.0f, 0.0f, vCurrentExtents.z };

		//hey look, they can overlap
		float fExtentDistribution[6];
		fExtentDistribution[ 0 ] = vCenter.z + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NZ ) == 0 ) ? ( -vCurrentExtents.z ) : ( 0.0f ) );	// Z-
		fExtentDistribution[ 1 ] = vCenter.x + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NX ) == 0 ) ? ( -vCurrentExtents.x ) : ( 0.0f ) );	// X-
		fExtentDistribution[ 2 ] = vCenter.x + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_X ) == 0 ) ? ( vCurrentExtents.x ) : ( 0.0f ) );		// X+
		fExtentDistribution[ 3 ] = vCenter.y + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NY ) == 0 ) ? ( -vCurrentExtents.y ) : ( 0.0f ) );	// Y-
		fExtentDistribution[ 4 ] = vCenter.z + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_Z ) == 0 ) ? ( vCurrentExtents.z ) : ( 0.0f ) );		// Z+
		fExtentDistribution[ 5 ] = vCenter.y + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_Y ) == 0 ) ? ( vCurrentExtents.y ) : ( 0.0f ) );		// Y+

		float *pXDistribution = &fExtentDistribution[1];
		float *pYDistribution = &fExtentDistribution[3];

		bool bExtentInvalid[8];
		float fExtentDists[8];
		bool bAnyInvalid = false;
		for( int i = 0; i != 8; ++i )
		{
			ptExtents[i].x = pXDistribution[i & (1<<0)]; //fExtentDistribution[(0 or 1) + 1]
			ptExtents[i].y = pYDistribution[i & (1<<1)]; //fExtentDistribution[(0 or 2) + 3]
			ptExtents[i].z = fExtentDistribution[i & (1<<2)]; //fExtentDistribution[(0 or 4)]

			fExtentsValidation[i] = 0.0f;
			bExtentInvalid[i] = pTraceAdapter->pPointOutsideWorldFunc( ptExtents[i], pTraceAdapter );
			bAnyInvalid |= bExtentInvalid[i];
			fExtentDists[i] = fOriginalExtentDists[i] * vExtents[iLargestExtent];
		}

		//trace from all extents to all other extents and rate the validity
		{
			unsigned int counters[2]; //I know it's weird, get over it
			for( counters[0] = 0; counters[0] != 7; ++counters[0] )
			{
				for( counters[1] = counters[0] + 1; counters[1] != 8; ++counters[1] )
				{
					for( int i = 0; i != 2; ++i )
					{
						if( bExtentInvalid[counters[i]] )
						{
							traces[i].startsolid = true;
							traces[i].fraction = 0.0f;
						}
						else
						{
							testRay.m_Start = ptExtents[counters[i]];
							testRay.m_Delta = ptExtents[counters[1-i]] - ptExtents[counters[i]];
							pTraceAdapter->pTraceFunc( testRay, &traces[i], pTraceAdapter );
						}
					}

					float fDistance = fExtentDists[counters[0] ^ counters[1]];

					for( int i = 0; i != 2; ++i )
					{
						if( (traces[i].fraction == 1.0f) && (traces[1-i].fraction != 1.0f) )
						{
							//One sided collision >_<
							traces[i].startsolid = true;
							traces[i].fraction = 0.0f;
							break;
						}
					}

					for( int i = 0; i != 2; ++i )
					{
						if( traces[i].startsolid )
						{
							bExtentInvalid[counters[i]] = true;
							bAnyInvalid = true;
						}
						else
						{
							fExtentsValidation[counters[i]] += traces[i].fraction * fDistance;
						}
					}
				}
			}
		}

		//optimally we should do this check before tracing extents. But one sided collision is a bitch
		if( !bAnyInvalid )
		{
			//try to trace back to the starting position (if we start in valid, the endpoint will be closer to the original center)
			entRay.m_Start = vCenter;
			entRay.m_Delta = vOriginalCenter - vCenter;

			pTraceAdapter->pTraceFunc( entRay, &traces[0], pTraceAdapter );
			if( traces[0].startsolid == false )
			{
				//damned one sided collision
				vCenterOut = traces[0].endpos;
				return !pTraceAdapter->pPointOutsideWorldFunc( vCenterOut, pTraceAdapter );
			}
		}

		//find the direction to move based on the extent validity
		{
			Vector vNewOriginDirection( 0.0f, 0.0f, 0.0f );
			float fTotalValidation = 0.0f;
			for( int i = 0; i != 8; ++i )
			{
				if( !bExtentInvalid[i] )
				{
					vNewOriginDirection += (ptExtents[i] - vCenter) * fExtentsValidation[i];
					fTotalValidation += fExtentsValidation[i];
				}
			}

			if( fTotalValidation != 0.0f )
			{
				vCenter += (vNewOriginDirection / fTotalValidation);

				//increase sizing
				testRay.m_Extents += vGrowSize; //increase the ray size
				vCurrentExtents -= vGrowSize; //while reducing the overall test region size (so outermost ray extents are the same)
			}
			else
			{
				//no point was valid, apply the indecisive vector
				vCenter += vIndecisivePush;

				//reset sizing
				testRay.m_Extents = vGrowSize;
				vCurrentExtents = vOriginalExtents - vGrowSize;
			}
		}
	}

	//Warning( "FindClosestPassableSpace() failure.\n" );

	// X360TBD: Hits in portal devtest
	//AssertMsg( IsGameConsole() || iFailCount != iIterations, "FindClosestPassableSpace() failure." );
	vCenterOut = vOriginalCenter;
	return false;
}

static void EngineTraceFunc( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	enginetrace->TraceRay( ray, pTraceAdapter->fMask, pTraceAdapter->pTraceFilter, pResult );
}

static bool EnginePointOutsideWorldFunc( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	return enginetrace->PointOutsideWorld( vTest );
}

bool UTIL_FindClosestPassableSpace( const Vector &vOriginalCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags )
{
	FindClosestPassableSpace_TraceAdapter_t adapter;
	adapter.pTraceFunc = EngineTraceFunc;
	adapter.pPointOutsideWorldFunc = EnginePointOutsideWorldFunc;
	adapter.pTraceFilter = pTraceFilter;
	adapter.fMask = fMask;

	return UTIL_FindClosestPassableSpace( vOriginalCenter, vExtents, vIndecisivePush, iIterations, vCenterOut, nAxisRestrictionFlags, &adapter );
}

bool UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, unsigned int iIterations, Vector &vOriginOut, Vector *pStartingPosition, int nAxisRestrictionFlags ) //assumes the object is already in a mostly passable space
{
	// Don't ever do this to entities with a move parent
	if ( pEntity->GetMoveParent() )
	{
		vOriginOut = pEntity->GetAbsOrigin();
		return false;
	}

	Vector vEntityMaxs;
	Vector vEntityMins;
	pEntity->CollisionProp()->WorldSpaceAABB( &vEntityMins, &vEntityMaxs );

	Vector ptEntityCenter = ((vEntityMins + vEntityMaxs) / 2.0f);
	//vEntityMins -= ptEntityCenter;
	vEntityMaxs -= ptEntityCenter;
	
	Vector vCenterToOrigin = pEntity->GetAbsOrigin() - ptEntityCenter;
	if( pStartingPosition != nullptr )
	{
		Vector vOriginOffset = (*pStartingPosition) - pEntity->GetAbsOrigin();
		ptEntityCenter += vOriginOffset;
	}

	CTraceFilterSimple traceFilter( pEntity, pEntity->GetCollisionGroup() );

	Vector vResult;
	bool bSuccess = UTIL_FindClosestPassableSpace( ptEntityCenter, vEntityMaxs, vIndecisivePush, &traceFilter, fMask, iIterations, vResult );
	vResult += vCenterToOrigin;
	vOriginOut = vResult;

	return bSuccess;
}

bool UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, Vector *pStartingPosition, int nAxisRestrictionFlags )
{
	Vector vNewPos;
	bool bWorked = UTIL_FindClosestPassableSpace( pEntity, vIndecisivePush, fMask, 100, vNewPos, pStartingPosition, nAxisRestrictionFlags );
	if( bWorked )
	{
		pEntity->Teleport( &vNewPos, nullptr, nullptr );
	}
	return bWorked;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves the MOD directory for the active game (ie. "hl2")
//-----------------------------------------------------------------------------

bool UTIL_GetModDir( char *lpszTextOut, unsigned int nSize )
{
	// Must pass in a buffer at least large enough to hold the desired string
	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );
	contract_assert( std::strlen(pGameDir) <= nSize );
	if ( std::strlen(pGameDir) > nSize )
		return false;

	V_strncpy( lpszTextOut, pGameDir, nSize );
	if ( V_strnchr( lpszTextOut, '/', nSize ) || V_strnchr( lpszTextOut, '\\', nSize ) )
	{
		// Strip the last directory off (which will be our game dir)
		V_StripLastDir( lpszTextOut, nSize );

		// Find the difference in string lengths and take that difference from the original string as the mod dir
		int dirlen = V_strlen( lpszTextOut );
		V_strncpy( lpszTextOut, pGameDir + dirlen, V_strlen( pGameDir ) - dirlen + 1 );
	}

	return true;
}

//#define FRUSTUM_DEBUGGING //for dumping some clipping information in UTIL_CalcFrustumThroughConvexPolygon

int UTIL_CalcFrustumThroughConvexPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &vFrustumOrigin, const VPlane *pInputFrustumPlanes, int iInputFrustumPlanes, VPlane *pOutputFrustumPlanes, int iMaxOutputPlanes, int iPreserveCount )
{
	ks::ScratchArena<> scratch;
	contract_assert( iPreserveCount <= iMaxOutputPlanes );
	contract_assert( iPreserveCount <= iInputFrustumPlanes );
	if( iPolyVertCount < 3 )
		return 0;

	int iMaxComplexity = iMaxOutputPlanes - iPreserveCount;
	
	Vector *pClippedVerts;
	int iClippedVertCount;
	if( iInputFrustumPlanes > 0 )
	{
		//clip the polygon by the input frustum
		int iAllocSize = iPolyVertCount + iInputFrustumPlanes;

		Vector *pWorkVerts[2];
		pWorkVerts[0] = scratch.Alloc<Vector>( iAllocSize * 2 ); //possible to add 1 point per cut, iPolyVertCount starting points, iInputFrustumPlaneCount cuts
		pWorkVerts[1] = pWorkVerts[0] + iAllocSize;

		//clip by first plane and put output into pInVerts
		iClippedVertCount = ClipPolyToPlane( (Vector *)pPolyVertices, iPolyVertCount, pWorkVerts[0], pInputFrustumPlanes[0].m_Normal, pInputFrustumPlanes[0].m_Dist, 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 1; i != iInputFrustumPlanes; ++i )
		{
			if( iClippedVertCount < 3 )
				return 0; //nothing left in the frustum

			iClippedVertCount = ClipPolyToPlane( pWorkVerts[(i & 1) ^ 1], iClippedVertCount, pWorkVerts[i & 1], pInputFrustumPlanes[i].m_Normal, pInputFrustumPlanes[i].m_Dist, 0.01f );
		}

		if( iClippedVertCount < 3 )
			return false; //nothing left in the frustum

		pClippedVerts = pWorkVerts[(iInputFrustumPlanes & 1) ^ 1];
	}
	else
	{
		//no input frustum
		if( iPolyVertCount > iMaxComplexity )
		{
			//we'll need to reduce our output frustum, copy the input polygon
			pClippedVerts = scratch.Alloc<Vector>( iPolyVertCount );
			std::memcpy( pClippedVerts, pPolyVertices, sizeof( Vector ) * iPolyVertCount );
		}
		else
		{
			//we won't need to simplify the polygon to reduce output planes, just point at the input polygon
			pClippedVerts = (Vector *)pPolyVertices;
		}
		iClippedVertCount = iPolyVertCount;
	}

	contract_assert( iClippedVertCount <= (iPolyVertCount + iInputFrustumPlanes) );

	if( iClippedVertCount > iMaxComplexity )
	{
		float *fLineLengthSqr = scratch.Alloc<float>( iClippedVertCount );

		for( int i = 0; i != (iClippedVertCount - 1); ++i )
		{
			fLineLengthSqr[i] = (pClippedVerts[i + 1] - pClippedVerts[i]).LengthSqr();
		}
		fLineLengthSqr[(iClippedVertCount - 1)] = (pClippedVerts[0] - pClippedVerts[(iClippedVertCount - 1)]).LengthSqr(); //wrap around

		while( iClippedVertCount > iMaxComplexity ) //vert count == number of planes we need to bound the polygon
		{
			//we have too many verts to represent this accurately in the output frustum plane count
			//so, we're going to eliminate the smallest sides one at a time and bridge the surrounding sides until we're down to iMaxComplexity
			float fMinSide = fLineLengthSqr[0];
			int iMinSideFirstPoint = 0;
			int iOldVertCount = iClippedVertCount;
			--iClippedVertCount; //we're going to decrement this sometime in this block, it makes math easier to do it now

			for( int i = 1; i != iOldVertCount; ++i )
			{
				if( fLineLengthSqr[i] < fMinSide )
				{
					fMinSide = fLineLengthSqr[i];
					iMinSideFirstPoint = i;
				}
			}

			int i1, i2, i3, i4;
			i1 = (iMinSideFirstPoint + iClippedVertCount)%(iOldVertCount); //-1 with a wrap
			i2 = iMinSideFirstPoint;
			i3 = (iMinSideFirstPoint + 1)%(iOldVertCount);
			i4 = (iMinSideFirstPoint + 2)%(iOldVertCount);

			Vector *p1, *p2, *p3, *p4;
			p1 = &pClippedVerts[i1];
			p2 = &pClippedVerts[i2];
			p3 = &pClippedVerts[i3]; //this is the one we'll actually be dropping in the merge
			p4 = &pClippedVerts[i4];

			//now we know the two points that we have to merge to one, project and make a merged point from the surrounding lines
			//if( fMinSide >= 0.1f ) //only worth doing the math if it's actually going to be accurate and make a difference
			{
				//http://mathworld.wolfram.com/Line-LineIntersection.html (20)
				Vector vA = *p2 - *p1;
				Vector vB = *p4 - *p3;
				Vector vC = *p3 - *p1;
				Vector vCxB = vC.Cross( vB );
				Vector vAxB = vA.Cross( vB );
				float fS = vCxB.Dot(vAxB)/vAxB.LengthSqr();

				*p2 = *p1 + (vA * fS);

				fLineLengthSqr[i1] = (*p2 - *p1).LengthSqr();
			}
			
			fLineLengthSqr[i2] = (*p4 - *p2).LengthSqr(); //must do this BEFORE possibly shifting points p4+ left

			if( i3 < i4 ) //not the last point in the array
			{
				int iElementShift = (iOldVertCount - i4);

				//eliminate p3, we merged p2+p3 and already stored the result in p2
				std::memmove( p3, p4, sizeof( Vector ) * iElementShift );
				memmove( &fLineLengthSqr[i3], &fLineLengthSqr[i4], sizeof( float ) * iElementShift );
			}
		}

	}

	//generate planes defined by each line around the convex and the frustum origin
	{
		int iFlipNormalsXOR = 0; //this algorithm was written assuming polygon vertices would be in a clockwise order from the perspective of vFrustumOrigin, some logic needs to flip if the inverse is true
		{
			Vector vLine1 = pPolyVertices[1] - pPolyVertices[0];
			Vector vLine2 = pPolyVertices[2] - pPolyVertices[1];
			Vector vFrontFace = vLine2.Cross( vLine1 );

			iFlipNormalsXOR = (vFrontFace.Dot( vFrustumOrigin - pPolyVertices[0] ) < 0.0f) ? 1 : 0; //this will assist in reversing the normal by flipping the cross product
		}		

		Vector vTemp[2];
		vTemp[0] = pClippedVerts[iClippedVertCount - 1] - vFrustumOrigin;
		for( int i = 0; i != iClippedVertCount; ++i )
		{
			int iIndexing = i & 1; //we can carry over the line computation from one iteration to the next, flip which order we look at the temps with
			vTemp[iIndexing ^ 1] = pClippedVerts[i] - vFrustumOrigin; 

			Vector vNormal = vTemp[iIndexing ^ iFlipNormalsXOR].Cross( vTemp[(iIndexing ^ iFlipNormalsXOR) ^ 1] ); //vLine1.Cross( vLine2 );
			vNormal.NormalizeInPlace();

			pOutputFrustumPlanes[i].Init( vNormal, vNormal.Dot( vFrustumOrigin ) );
		}
	}

	//preserve input planes on request
	if( iPreserveCount > 0 )
	{
		std::memcpy( &pOutputFrustumPlanes[iClippedVertCount], &pInputFrustumPlanes[iInputFrustumPlanes - iPreserveCount], sizeof( VPlane ) * iPreserveCount );
	}

	return (iClippedVertCount + iPreserveCount);
}

//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------

CFlaggedEntitiesEnum::CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask )
{
	m_pList = pList;
	m_listMax = listMax;
	m_flagMask = flagMask;
	m_count = 0;
}

bool CFlaggedEntitiesEnum::AddToList( CBaseEntity *pEntity )
{
	if ( m_count >= m_listMax )
	{
		contract_assert( false && "reached enumerated list limit.  Increase limit, decrease radius, or make it so entity flags will work for you" );
		return false;
	}
	m_pList[m_count] = pEntity;
	m_count++;
	return true;
}

IterationRetval_t CFlaggedEntitiesEnum::EnumElement( IHandleEntity *pHandleEntity )
{
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
	if ( pEntity )
	{
		if ( m_flagMask && !(pEntity->GetFlags() & m_flagMask) )	// Does it meet the criteria?
			return ITERATION_CONTINUE;

		if ( !AddToList( pEntity ) )
			return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}

int UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum )
{
	::partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, pEnum );
	return pEnum->GetCount();
}

//-----------------------------------------------------------------------------
// Returns the length of the next command
//-----------------------------------------------------------------------------
void UTIL_GetNextCommandLength( const char *pText, int nMaxLen, int *pCommandLength, int *pNextCommandOffset )
{
	int nCommandLength = 0;
	int nNextCommandOffset;
	bool bIsQuoted = false;
	bool bIsCommented = false;
	for ( nNextCommandOffset=0; nNextCommandOffset < nMaxLen; ++nNextCommandOffset, nCommandLength += bIsCommented ? 0 : 1 )
	{
		char c = pText[nNextCommandOffset];
		if ( !bIsCommented )
		{
			if ( c == '"' )
			{
				bIsQuoted = !bIsQuoted;
				continue;
			}

			// don't break if inside a C++ style comment
			if ( !bIsQuoted && c == '/' )
			{
				bIsCommented = ( nNextCommandOffset < nMaxLen-1 ) && pText[nNextCommandOffset+1] == '/';
				if ( bIsCommented )
				{
					++nNextCommandOffset;
					continue;
				}
			}

			// don't break if inside a quoted string
			if ( !bIsQuoted && c == ';' )
				break;	
		}

		// FIXME: This is legacy behavior; should we not break if a \n is inside a quoted string?
		if ( c == '\n' )
			break;
	}

	*pCommandLength = nCommandLength;
	*pNextCommandOffset = nNextCommandOffset;
}

/**
 *  remove double spaces and empty bold/italic HTML tags from a string
 */
void UTIL_TrimEmptyWhitespaceFromHTML( wchar_t* pszDescWriter, std::size_t descWriterByteSize, const wchar_t* pszDescReader )
{
	std::size_t writerArraySize = descWriterByteSize / sizeof( wchar_t );
	wchar_t* pszDescWriterEnd = pszDescWriter + writerArraySize - 1; // leave 1 character for '\0' terminator

	while ( *pszDescReader != 0 && pszDescWriter < pszDescWriterEnd )
	{
		if ( !std::wcsncmp( pszDescReader, L" <b></b> ", 9 ) ||
			 !wcsncmp( pszDescReader, L" <i></i> ", 9 ) )
		{
			pszDescReader += 8;
		}
		else if ( !wcsncmp( pszDescReader, L"  ", 2 ) )
		{
			pszDescReader += 1;
		}
		else
		{
			*pszDescWriter = *pszDescReader;
			pszDescWriter++;
			pszDescReader++;
		}
	}

	// terminate buffer
	*pszDescWriter = 0;
}

void UTIL_TrimEmptyWhitespaceFromHTML( char* pszDescWriter, std::size_t descWriterByteSize, const char* pszDescReader )
{
	std::size_t writerArraySize = descWriterByteSize / sizeof( char );
	char* pszDescWriterEnd = pszDescWriter + writerArraySize - 1; // leave 1 character for '\0' terminator

	while ( *pszDescReader != 0 && pszDescWriter < pszDescWriterEnd )
	{
		if ( !std::strncmp( pszDescReader, " <b></b> ", 9 ) ||
			 !strncmp( pszDescReader, " <i></i> ", 9 ) )
		{
			pszDescReader += 8;
		}
		else if ( !strncmp( pszDescReader, "  ", 2 ) )
		{
			pszDescReader += 1;
		}
		else
		{
			*pszDescWriter = *pszDescReader;
			pszDescWriter++;
			pszDescReader++;
		}
	}

	// terminate buffer
	*pszDescWriter = 0;
}

//-----------------------------------------------------------------------------
// Converts an IHandleEntity to an CBaseEntity
//-----------------------------------------------------------------------------
const CBaseEntity *EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity )
{
	IHandleEntity *pHandleEntity = const_cast<IHandleEntity*>(pConstHandleEntity);

	if ( staticpropmgr->IsStaticProp( pHandleEntity ) )
		return nullptr;

	IServerUnknown *pUnk = (IServerUnknown*)pHandleEntity;
	return EntityCast<const CBaseEntity>( pUnk->GetBaseEntity() );
}

CBaseEntity *EntityFromEntityHandle( IHandleEntity *pHandleEntity )
{
	if ( staticpropmgr->IsStaticProp( pHandleEntity ) )
		return nullptr;

	IServerUnknown *pUnk = (IServerUnknown*)pHandleEntity;
	contract_assert( !pUnk || pUnk->GetBaseEntity() );
	return EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
}

void CountdownTimer::Start( float duration)
{
	m_timestamp = Now() + duration;
	m_duration = duration;
}

void CountdownTimer::StartFromTime( float startTime, float duration)
{
	m_timestamp = startTime + duration;
	m_duration = duration;
}

float CountdownTimer::GetElapsedRatio() const
{
	if ( HasStarted() )
	{
		float elapsed = GetElapsedTime() / m_duration;
		if ( elapsed < 0.0f )
			return 0.0f;
		if ( elapsed > 1.0f )
			return 1.0f;
		return elapsed;
	}

	return 1.0f;
}

float CTimeline::GetValueTime( int i) const
{
	contract_assert( i >= 0 && i < m_nBucketCount );
	return static_cast<float>( i ) * m_flInterval;
}
