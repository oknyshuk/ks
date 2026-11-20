// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_class.h"
#include "dlight.h"
#include "iclientalphaproperty.h"
#include "icliententity.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "mouthinfo.h"
#include "reflect.h"
#include "soundinfo.h"
#include "toolframework/itoolentity.h"
#include "videocfg/videocfg.h"
#include "vphysics_interface.h"
#include "vscript/ivscript.h"
#include "cdll_client_int.h"
#include "client_thinklist.h"
#include "cliententitylist.h"
#include "predictioncopy.h"
#include "reflect_datamap.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "shareddefs.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "engine/IEngineTrace.h"
#include "irecipientfilter.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "utils/common/scriplib.h"
#include "vcollide_parse.h"
#include "baseentity.h"
#include "predictable_entity.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.numeric;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.time;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.game.cellcoord;
import ks.game.predictableid;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.transform;
import ks.math.vector;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.interpolatedvar;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.slot_pool;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import ks.vstdlib.callqueue;
import :baseentity;
import :props;
import :iinput;
import :baseanimatedtextureproxy;
import :sharedvar;
import :util;
import :particlemgr;
import :cstrike15.cs_player;
import :precache;
import :noopanimcontroller;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import :cstrike15.cs_shareddefs;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.platform;
import ks.game.entity_cast;
import ks.tier1.exprevaluator;
import ks.game.entitycore;

class CClientAlphaProperty;
class CNewParticleEffect;
class CPredictionCopy;
class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CRecipientFilter;
class CTeam;

using namespace ::ks::reflect;

static bool g_bWasSkipping = (bool)-1;
static bool g_bWasThreaded =(bool)-1;

void cc_cl_interp_all_changed( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.GetInt() )
	{
		CBaseEntityIterator iterator;
		CBaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != nullptr )	
		{
			if ( pEnt->ShouldInterpolate() )
			{
				pEnt->AddToEntityList(ENTITY_LIST_INTERPOLATE);
			}
		}
	}
}

static ConVar  report_cliententitysim( side_registry(), "report_cliententitysim", "0", FCVAR_CHEAT, "List all clientside simulations and time - will report and turn itself off." );
static ConVar  cl_extrapolate( side_registry(), "cl_extrapolate", "1", FCVAR_CHEAT, "Enable/disable extrapolation if interpolation history runs out." );
static ConVar  cl_interp_npcs( side_registry(), "cl_interp_npcs", "0.0", 0, "Interpolate NPC positions starting this many seconds in past (or cl_interp, if greater)" );  
static ConVar  cl_interp_all( side_registry(), "cl_interp_all", "0", 0, "Disable interpolation list optimizations.", 0, 0, 0, 0, cc_cl_interp_all_changed );
ConVar  r_drawmodeldecals( side_registry(), "r_drawmodeldecals", "1" );
extern ConVar	cl_showerror;
int CBaseEntity::m_nPredictionRandomSeed = -1;
CBasePlayer *CBaseEntity::m_pPredictionPlayer = nullptr;
bool CBaseEntity::s_bAbsQueriesValid = true;
bool CBaseEntity::s_bAbsRecomputationEnabled = true;
bool CBaseEntity::s_bInterpolate = true;
int CBaseEntity::s_nIncomingPacketCommandsAcknowledged = -1;

bool CBaseEntity::sm_bDisableTouchFuncs = false;	// Disables PhysicsTouch and PhysicsStartTouch function calls

static ConVar  r_drawrenderboxes( side_registry(), "r_drawrenderboxes", "0", FCVAR_CHEAT, "(0 - off) (1 - Draws the bounding box of entities) (2 - Draws the axis aligned bounding box used for culling) (3 - draws both bounding boxes)" );  

static bool g_bAbsRecomputationStack[8];
static unsigned short g_iAbsRecomputationStackPos = 0;

// All the entities that want Interpolate() called on them.
// Linked: list order is the Simulate/Interpolate/PreRender order; handles live in CBaseEntity::m_ListEntry
static ks::linked_slot_pool<CBaseEntity*, unsigned short> g_EntityLists[NUM_ENTITY_LISTS];
static bool s_bImmediateRemovesAllowed = true;
bool SetImmediateEntityRemovesAllowed( bool bAllowed )
{
	bool bOldValue = s_bImmediateRemovesAllowed;
	s_bImmediateRemovesAllowed = bAllowed;
	return bOldValue;
}

// Create singleton
static CPredictableList g_Predictables[ MAX_SPLITSCREEN_PLAYERS ];
CPredictableList *GetPredictables( int nSlot )
{
	contract_assert( ( nSlot >= 0 ) && "Tried to get prediction slot for player %d. This probably means you are predicting something that isn't local. Crash ensues." );
	return &g_Predictables[ nSlot ];
}

//-----------------------------------------------------------------------------
// Purpose: Add entity to list
// Input  : add - 
// Output : int
//-----------------------------------------------------------------------------
void CPredictableList::AddToPredictableList( CBaseEntity *add )
{
	// Add to general sorted list, unless an entity with this entindex is already there
	m_Predictables.insert( add );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : remove - 
//-----------------------------------------------------------------------------
void CPredictableList::RemoveFromPredictablesList( CBaseEntity *remove )
{
	ks::erase_first( m_Predictables, remove );
}

//-----------------------------------------------------------------------------
// Purpose: Searc predictables for previously created entity (by testId)
// Input  : testId - 
// Output : static CBaseEntity
//-----------------------------------------------------------------------------
static CBaseEntity *FindPreviouslyCreatedEntity( CPredictableId& testId )
{
	return nullptr;
}

class IRecordingList
{
public:
	virtual ~IRecordingList() {};
	virtual void	AddToList( ClientEntityHandle_t add ) = 0;
	virtual void	RemoveFromList( ClientEntityHandle_t remove ) = 0;

	virtual int		Count() = 0;
	virtual IClientRenderable *Get( int index ) = 0;
};

class CRecordingList : public IRecordingList
{
public:
	virtual void	AddToList( ClientEntityHandle_t add );
	virtual void	RemoveFromList( ClientEntityHandle_t remove );

	virtual int		Count();
	IClientRenderable *Get( int index );
private:
	std::vector< ClientEntityHandle_t > m_Recording;
};

static CRecordingList g_RecordingList;
IRecordingList *recordinglist = &g_RecordingList;

//-----------------------------------------------------------------------------
// Purpose: Add entity to list
// Input  : add - 
// Output : int
//-----------------------------------------------------------------------------
void CRecordingList::AddToList( ClientEntityHandle_t add )
{
	// This is a hack to remap slot to index
	if ( ks::find_index( m_Recording, add ) != -1 )
	{
		return;
	}

	// Add to general list
	m_Recording.push_back( add );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : remove - 
//-----------------------------------------------------------------------------
void CRecordingList::RemoveFromList( ClientEntityHandle_t remove )
{
	ks::erase_first( m_Recording, remove );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slot - 
// Output : IClientRenderable
//-----------------------------------------------------------------------------
IClientRenderable *CRecordingList::Get( int index )
{
	return cl_entitylist->GetClientRenderableFromHandle( m_Recording[ index ] );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CRecordingList::Count()
{
	return std::ssize( m_Recording );
}

// Helper object implementation
CCurTimeScopeGuard::CCurTimeScopeGuard( float flNewCurTime, bool bOptionalCondition /*= true*/ )
{
	m_bActive = bOptionalCondition;
	if ( m_bActive )
	{
		m_flSavedTime = gpGlobals->curtime;
		gpGlobals->curtime = flNewCurTime;
	}
	else
	{
		m_flSavedTime = 0.0f;
	}
}

CCurTimeScopeGuard::~CCurTimeScopeGuard()
{
	if ( m_bActive )
	{
		gpGlobals->curtime = m_flSavedTime;
	}
}

// Should these be somewhere else?
#define PITCH 0

//-----------------------------------------------------------------------------
// Purpose: Decodes animtime and notes when it changes
// Input  : *pStruct - ( CBaseEntity * ) used to flag animtime is changine
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_AnimTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEntity = ( CBaseEntity * )pStruct;
	contract_assert( pOut == &pEntity->m_flAnimTime );

	int t;
	int tickbase;
	int addt;

	// Unpack the data.
	addt	= pData->m_Value.m_Int;

	// Note, this needs to be encoded relative to packet timestamp, not raw client clock
	tickbase = gpGlobals->GetNetworkBase( gpGlobals->tickcount, pEntity->entindex() );

	t = tickbase;
											//  and then go back to floating point time.
	t += addt;				// Add in an additional up to 256 100ths from the server

	// center m_flAnimTime around current time.
	while (t < gpGlobals->tickcount - 127)
		t += 256;
	while (t > gpGlobals->tickcount + 127)
		t -= 256;
	
	pEntity->m_flAnimTime = ( t * TICK_INTERVAL );
}

void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEntity = ( CBaseEntity * )pStruct;
	contract_assert( pOut == &pEntity->m_flSimulationTime );

	int t;
	int tickbase;
	int addt;

	// Unpack the data.
	addt	= pData->m_Value.m_Int;

	// Note, this needs to be encoded relative to packet timestamp, not raw client clock
	tickbase = gpGlobals->GetNetworkBase( gpGlobals->tickcount, pEntity->entindex() );

	t = tickbase;
											//  and then go back to floating point time.
	t += addt;				// Add in an additional up to 256 100ths from the server

	// center m_flSimulationTime around current time.
	while (t < gpGlobals->tickcount - 127)
		t += 256;
	while (t > gpGlobals->tickcount + 127)
		t -= 256;
	
	float simtime = ( t * TICK_INTERVAL );

	if ( simtime != pEntity->m_flSimulationTime )
	{
		pEntity->OnSimulationTimeChanging( pEntity->m_flSimulationTime, simtime );
		pEntity->m_flSimulationTime = simtime;
	}
}

void CBaseEntity::RecvProxy_CellBits( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	if ( pEnt->SetCellBits( pData->m_Value.m_Int ) )
	{
		if ( pEnt->ShouldRegenerateOriginFromCellBits() )
		{
			pEnt->m_vecNetworkOrigin.x = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellX, pEnt->m_vecCellOrigin.x );
			pEnt->m_vecNetworkOrigin.y = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellY, pEnt->m_vecCellOrigin.y );
			pEnt->m_vecNetworkOrigin.z = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellZ, pEnt->m_vecCellOrigin.z );
		}
	}
}

void CBaseEntity::RecvProxy_CellX( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	int *cellX = (int *)pOut;
	contract_assert( cellX == &pEnt->m_cellX );

	*cellX = pData->m_Value.m_Int;

	// Cell changed, update world position
	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		pEnt->m_vecNetworkOrigin.x = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellX, pEnt->m_vecCellOrigin.x );
	}
}

void CBaseEntity::RecvProxy_CellY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	int *cellY = (int *)pOut;
	contract_assert( cellY == &pEnt->m_cellY );

	*cellY = pData->m_Value.m_Int;

	// Cell changed, update world position
	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		pEnt->m_vecNetworkOrigin.y = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellY, pEnt->m_vecCellOrigin.y );
	}
}

void CBaseEntity::RecvProxy_CellZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	int *cellZ = (int *)pOut;
	contract_assert( cellZ == &pEnt->m_cellZ );

	*cellZ = pData->m_Value.m_Int;

	// Cell changed, update world position
	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		pEnt->m_vecNetworkOrigin.z = CoordFromCell( pEnt->m_cellwidth, pEnt->m_cellZ, pEnt->m_vecCellOrigin.z );
	}
}

void CBaseEntity::RecvProxy_CellOrigin( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	Vector *vecNetworkOrigin = (Vector *)pOut;

	contract_assert( vecNetworkOrigin == &pEnt->m_vecNetworkOrigin );

	pEnt->m_vecCellOrigin.x = pData->m_Value.m_Vector[0];
	pEnt->m_vecCellOrigin.y = pData->m_Value.m_Vector[1];
	pEnt->m_vecCellOrigin.z = pData->m_Value.m_Vector[2];

	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		int const cellwidth = pEnt->m_cellwidth; // Load it into a register
		vecNetworkOrigin->x = CoordFromCell( cellwidth, pEnt->m_cellX, pData->m_Value.m_Vector[0] );
		vecNetworkOrigin->y = CoordFromCell( cellwidth, pEnt->m_cellY, pData->m_Value.m_Vector[1] );
		vecNetworkOrigin->z = CoordFromCell( cellwidth, pEnt->m_cellZ, pData->m_Value.m_Vector[2] );
	}
}

void CBaseEntity::RecvProxy_CellOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	Vector *vecNetworkOrigin = (Vector *)pOut;

	contract_assert( vecNetworkOrigin == &pEnt->m_vecNetworkOrigin );

	pEnt->m_vecCellOrigin.x = pData->m_Value.m_Vector[0];
	pEnt->m_vecCellOrigin.y = pData->m_Value.m_Vector[1];

	int const cellwidth = pEnt->m_cellwidth; // Load it into a register

	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		vecNetworkOrigin->x = CoordFromCell( cellwidth, pEnt->m_cellX, pData->m_Value.m_Vector[0] );
		vecNetworkOrigin->y = CoordFromCell( cellwidth, pEnt->m_cellY, pData->m_Value.m_Vector[1] );
	}
}

void CBaseEntity::RecvProxy_CellOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	float *vecNetworkOriginZ = (float *)pOut;

	contract_assert( vecNetworkOriginZ == &pEnt->m_vecNetworkOrigin[2] );

	pEnt->m_vecCellOrigin.z = pData->m_Value.m_Float;

	int const cellwidth = pEnt->m_cellwidth; // Load it into a register

	if ( pEnt->ShouldRegenerateOriginFromCellBits() )
	{
		*vecNetworkOriginZ = CoordFromCell( cellwidth, pEnt->m_cellZ, pData->m_Value.m_Float );
	}
}

void RecvProxy_LocalVelocity( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseEntity *pEnt = (CBaseEntity *)pStruct;

	Vector vecVelocity;
	
	vecVelocity.x = pData->m_Value.m_Vector[0];
	vecVelocity.y = pData->m_Value.m_Vector[1];
	vecVelocity.z = pData->m_Value.m_Vector[2];

	// SetLocalVelocity checks to see if the value has changed
	pEnt->SetLocalVelocity( vecVelocity );
}
void RecvProxy_ToolRecording( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	if ( !ToolsEnabled() )
		return;

	CBaseEntity *pEnt = (CBaseEntity *)pStruct;
	pEnt->SetToolRecording( pData->m_Value.m_Int != 0 );
}

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(CBaseEntity, DT_BaseEntity, CBaseEntity);

// Not static: named by CBaseEntity's Bare<> annotation, which is parsed wherever the class
// is included. Declared in baseentity.h.
void RecvProxy_MoveType( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CBaseEntity*)pStruct)->SetMoveType( (MoveType_t)(pData->m_Value.m_Int) );
}

void RecvProxy_MoveCollide( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CBaseEntity*)pStruct)->SetMoveCollide( (MoveCollide_t)(pData->m_Value.m_Int) );
}

 static void RecvProxy_Solid_Entity( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CBaseEntity*)pStruct)->SetSolid( (SolidType_t)pData->m_Value.m_Int );
}

 static void RecvProxy_SolidFlags_Entity( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CBaseEntity*)pStruct)->SetSolidFlags( pData->m_Value.m_Int );
}

void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CBaseEntity*)pStruct)->SetEffects( pData->m_Value.m_Int );
}

void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// This proxy will cause the alpha modulation to get updated correctly
	CBaseEntity *pEnt = (CBaseEntity*)pStruct;
	uint32 color = ((uint32)pData->m_Value.m_Int);
	color32 c = *(color32*)( &color );
	pEnt->SetRenderColor( c.r, c.g, c.b );
	pEnt->SetRenderAlpha( c.a );
}

void CBaseEntity::RecvProxyOldSpottedByMask( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Well this is fun.  pStruct points to the start of the array, so we have to work back to get base entity
	CBaseEntity *pEnt = (CBaseEntity*) ( (byte*)pStruct - __builtin_offsetof( CBaseEntity, m_bSpottedBy ) );
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );
	int nPlayerIndex = (bool*)pOut - pEnt->m_bSpottedBy;
	pEnt->SetIsSpottedBy( nPlayerIndex );
}

IMPLEMENT_REFLECT_TABLE_IN( CBaseEntity, DT_AnimTimeMustBeFirst );

BEGIN_ENT_SCRIPTDESC_ROOT( CBaseEntity, "Root class of all client-side entities" )
	DEFINE_SCRIPTFUNC_NAMED( GetAbsOrigin, "GetOrigin", ""  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetForward, "GetForwardVector", "Get the forward vector of the entity"  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetLeft, "GetLeftVector", "Get the left vector of the entity"  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetUp, "GetUpVector", "Get the up vector of the entity"  )
	DEFINE_SCRIPTFUNC( GetTeamNumber, "Gets this entity's team" )
END_SCRIPTDESC();

IMPLEMENT_REFLECT_TABLE( CBaseEntity, DT_BaseEntity );

IMPLEMENT_REFLECT_PREDMAP_NO_BASE( CBaseEntity );

//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------

void SpewInterpolatedVar( CInterpolatedVar< Vector > *pVar )
{
	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	float prevtime = 0.0f;
	while ( 1 )
	{
		float changetime, vel;
		Vector *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		vel = apparent.AddSample( changetime, *pVal );
		Msg( "%6.6f: (%.2f %.2f %.2f), vel: %.2f [dt %.1f]\n", changetime, (*pVal).x, (*pVal).y, (*pVal).z, vel, prevtime == 0.0f ? 0.0f : 1000.0f * ( changetime - prevtime ) );
		i = pVar->GetNext( i );
		prevtime = changetime;
	}
	Msg( "--------------------------------------------------\n" );
}

void SpewInterpolatedVar( CInterpolatedVar< Vector > *pVar, float flNow, float flInterpAmount, bool bSpewAllEntries = true )
{
	float target = flNow - flInterpAmount;

	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	float newtime = 999999.0f;
	Vector newVec( 0, 0, 0 );
	bool bSpew = true;

	while ( 1 )
	{
		float changetime, vel;
		Vector *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		if ( bSpew && target >= changetime )
		{
			Vector o;
			pVar->DebugInterpolate( &o, flNow );
			bool bInterp = newtime != 999999.0f;
			float frac = 0.0f;
			char desc[ 32 ];

			if ( bInterp )
			{
				frac = ( target - changetime ) / ( newtime - changetime );
				V_snprintf( desc, sizeof( desc ), "interpolated [%.2f]", frac );
			}
			else
			{
				bSpew = true;
				int savei = i;
				i = pVar->GetNext( i );
				float oldtertime = 0.0f;
				pVar->GetHistoryValue( i, oldtertime );

				if ( changetime != oldtertime )
				{
					frac = ( target - changetime ) / ( changetime - oldtertime );
				}

				V_snprintf( desc, sizeof( desc ), "extrapolated [%.2f]", frac );
				i = savei;
			}

			if ( bSpew )
			{
				Msg( "  > %6.6f: (%.2f %.2f %.2f) %s for %.1f msec\n", 
					target, 
					o.x, o.y, o.z, 
					desc,
					1000.0f * ( target - changetime ) );
				bSpew = false;
			}
		}

		vel = apparent.AddSample( changetime, *pVal );
		if ( bSpewAllEntries )
		{
			Msg( "    %6.6f: (%.2f %.2f %.2f), vel: %.2f [dt %.1f]\n", changetime, (*pVal).x, (*pVal).y, (*pVal).z, vel, newtime == 999999.0f ? 0.0f : 1000.0f * ( newtime - changetime ) );
		}
		i = pVar->GetNext( i );
		newtime = changetime;
		newVec = *pVal;
	}
	Msg( "--------------------------------------------------\n" );
}
template<class T>
void GetInterpolatedVarTimeRange( CInterpolatedVar<T> *pVar, float &flMin, float &flMax )
{
	flMin = 1e23;
	flMax = -1e23;

	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	while ( 1 )
	{
		float changetime;
		if ( !pVar->GetHistoryValue( i, changetime ) )
			return;

		flMin = std::min( flMin, changetime );
		flMax = std::max( flMax, changetime );
		i = pVar->GetNext( i );
	}
}

//-----------------------------------------------------------------------------
// Global methods related to when abs data is correct
//-----------------------------------------------------------------------------
void CBaseEntity::SetAbsQueriesValid( bool bValid )
{
	// @MULTICORE: Always allow in worker threads, assume higher level code is handling correctly
	if ( !ThreadInMainThread() )
		return;

	if ( !bValid )
	{
		s_bAbsQueriesValid = false;
	}
	else
	{
		s_bAbsQueriesValid = true;
	}
}

bool CBaseEntity::IsAbsQueriesValid( void )
{
	if ( !ThreadInMainThread() )
		return true;
	return s_bAbsQueriesValid;
}

void CBaseEntity::PushEnableAbsRecomputations( bool bEnable )
{
	if ( !ThreadInMainThread() )
		return;
	if ( g_iAbsRecomputationStackPos < std::size( g_bAbsRecomputationStack ) )
	{
		g_bAbsRecomputationStack[g_iAbsRecomputationStackPos] = s_bAbsRecomputationEnabled;
		++g_iAbsRecomputationStackPos;
		s_bAbsRecomputationEnabled = bEnable;
	}
	else
	{
		contract_assert( false );
	}
}

void CBaseEntity::PopEnableAbsRecomputations()
{
	if ( !ThreadInMainThread() )
		return;
	if ( g_iAbsRecomputationStackPos > 0 )
	{
		--g_iAbsRecomputationStackPos;
		s_bAbsRecomputationEnabled = g_bAbsRecomputationStack[g_iAbsRecomputationStackPos];
	}
	else
	{
		contract_assert( false );
	}
}

void CBaseEntity::EnableAbsRecomputations( bool bEnable )
{
	if ( !ThreadInMainThread() )
		return;
	// This should only be called at the frame level. Use PushEnableAbsRecomputations
	// if you're blocking out a section of code.
	contract_assert( g_iAbsRecomputationStackPos == 0 );

	s_bAbsRecomputationEnabled = bEnable;
}

bool CBaseEntity::IsAbsRecomputationsEnabled()
{
	if ( !ThreadInMainThread() )
		return true;
	return s_bAbsRecomputationEnabled;
}

int	CBaseEntity::GetTextureFrameIndex( void )
{
	return m_iTextureFrameIndex;
}

void CBaseEntity::SetTextureFrameIndex( int iIndex )
{
	m_iTextureFrameIndex = iIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *map - 
//-----------------------------------------------------------------------------
void CBaseEntity::Interp_SetupMappings( VarMapping_t *map )
{
	if( !map )
		return;

	int c = std::ssize( map->m_Entries );
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;
		void *data = e->data;
		int type = e->type;

		watcher->Setup( data, type );
		watcher->SetInterpolationAmount( GetInterpolationAmount( watcher->GetType() ) ); 
	}
}

void CBaseEntity::Interp_RestoreToLastNetworked( VarMapping_t *map, int flags )
{
	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "restoretolastnetworked" );

	Vector oldOrigin = GetLocalOrigin();
	QAngle oldAngles = GetLocalAngles();

	int c = std::ssize( map->m_Entries );
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();
		if ( flags && !(type & flags) )
			continue;

		watcher->RestoreToLastNetworked();
	}

	BaseInterpolatePart2( oldOrigin, oldAngles, 0 );
}

void CBaseEntity::Interp_UpdateInterpolationAmounts( VarMapping_t *map )
{
	if( !map )
		return;

	int c = std::ssize( map->m_Entries );
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;
		watcher->SetInterpolationAmount( GetInterpolationAmount( watcher->GetType() ) ); 
	}
}

void CBaseEntity::Interp_HierarchyUpdateInterpolationAmounts()
{
	Interp_UpdateInterpolationAmounts( GetVarMapping() );

	for ( CBaseEntity *pChild = FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
	{
		pChild->Interp_HierarchyUpdateInterpolationAmounts();
	}
}

int CBaseEntity::Interp_Interpolate( VarMapping_t *map, float currentTime )
{
	int bNoMoreChanges = 1;
	if ( currentTime < map->m_lastInterpolationTime )
	{
		for ( int i = 0; i < map->m_nInterpolatedEntries; i++ )
		{
			VarMapEntry_t *e = &map->m_Entries[ i ];

			e->m_bNeedsToInterpolate = true;
		}
	}
	map->m_lastInterpolationTime = currentTime;

	for ( int i = 0; i < map->m_nInterpolatedEntries; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];

		if ( !e->m_bNeedsToInterpolate )
			continue;
			
		IInterpolatedVar *watcher = e->watcher;
		contract_assert( !( watcher->GetType() & EXCLUDE_AUTO_INTERPOLATE ) );

		if ( watcher->Interpolate( currentTime ) )
			e->m_bNeedsToInterpolate = false;
		else
			bNoMoreChanges = 0;
	}

	return bNoMoreChanges;
}

//-----------------------------------------------------------------------------
// Functions.
//-----------------------------------------------------------------------------
// The core asks this side about an entity through these. The core's queries are const; the
// entity's answers are not all marked so.
static CBaseEntity *SideEntity( const CEntityCore *p )
{
	return const_cast<CBaseEntity *>( static_cast<const CBaseEntity *>( p ) );
}

// Client network vars mark nothing.
static constexpr CoreServices s_ClientCoreServices{
	nullptr,
	nullptr,
	[]( const CEntityCore *p ) -> const Vector & { return SideEntity( p )->GetAbsOrigin(); },
	[]( const CEntityCore *p ) -> const Vector & { return SideEntity( p )->GetAbsVelocity(); },
	[]( const CEntityCore *p ) -> const QAngle & { return SideEntity( p )->GetAbsAngles(); },
	[]( const CEntityCore *p ) { return SideEntity( p )->entindex(); },
	[]( const CEntityCore *p ) { return SideEntity( p )->IsPlayer(); },
	[]( const CEntityCore *p ) { return SideEntity( p )->IsFloating(); },
	[]( const CEntityCore *p ) { return SideEntity( p )->GetCollisionGroup(); },
	[]( const CEntityCore *p ) { return SideEntity( p )->GetTeamNumber(); },
	[]( const CEntityCore *p ) { return SideEntity( p )->PhysicsSolidMaskForEntity(); },
};

CBaseEntity::CBaseEntity() : 
	m_iv_vecOrigin( "CBaseEntity::m_iv_vecOrigin" ),
	m_iv_angRotation( "CBaseEntity::m_iv_angRotation" )
{
	m_pServices = &s_ClientCoreServices;
	AddVar( &m_vecOrigin, &m_iv_vecOrigin, LATCH_SIMULATION_VAR );
	AddVar( &m_angRotation, &m_iv_angRotation, LATCH_SIMULATION_VAR );

	// made noise 'a long time ago'
	m_flLastMadeNoiseTime = -99999.0f;

	m_iTeamNum = TEAM_UNASSIGNED;
	m_nMinCPULevel = m_nMaxCPULevel = 0;
	m_nMinGPULevel = m_nMaxGPULevel = 0;

	m_flFadeScale = 0.0f;
	m_fadeMinDist = m_fadeMaxDist = 0.0f;
	m_pClientAlphaProperty = nullptr;
	m_nSplitUserPlayerPredictionSlot = 0;
	m_DataChangeEventRef = -1;
	m_EntClientFlags = 0;
	m_bRenderWithViewModels = false;
	m_bDisableCachedRenderBounds = false;
	m_iParentAttachment = 0;
	m_bIsValidIKAttachment = false;

	SetPredictionEligible( false );
	m_bPredictable = false;

	m_bSimulatedEveryTick = false;
	m_bAnimatedEveryTick = false;
	m_pPhysicsObject = nullptr;
	m_bDisableSimulationFix = false;
	m_bSpotted = false;
	for ( int i = 0; i < kNumSpottedByMask; i++ )
		m_bSpottedByMask.Set( i, 0 );

	SetIdentityMatrix( m_rgflCoordinateFrame );
	m_nSimulationTick = -1;

	// Assume drawing everything
	m_bReadyToDraw = true;
	m_bClientSideRagdoll = false;
	m_flProxyRandomValue = 0.0f;

	m_fBBoxVisFlags = 0;
	
	for ( int i = 0; i < NUM_ENTITY_LISTS; i++ )
	{
		m_ListEntry[i] = 0xFFFF;
	}
	AddToEntityList( ENTITY_LIST_PRERENDER );

	Clear();

	m_bEnabledInToolView = true;
	m_bToolRecording = false;
	m_ToolHandle = 0;
	m_nLastRecordedFrame = -1;
	m_bRecordInTools = true;

	ParticleProp()->Init( this );

	m_spawnflags = 0;

	m_flUseLookAtAngle = DEFAULT_LOOK_AT_USE_ANGLE;
}

//-----------------------------------------------------------------------------
// Deallocates the alpha property
//-----------------------------------------------------------------------------
void CBaseEntity::CleanUpAlphaProperty()
{
	if ( m_pClientAlphaProperty )
	{
		ClientAlphaPropertyMgr()->DestroyClientAlphaProperty( m_pClientAlphaProperty );
		m_pClientAlphaProperty = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CBaseEntity::~CBaseEntity()
{
	Term();
	CleanUpAlphaProperty();
	ClearDataChangedEvent( m_DataChangeEventRef );
	for ( int i = 0; i < NUM_ENTITY_LISTS; i++ )
	{
		RemoveFromEntityList(entity_list_ids_t(i));
	}
}

void CBaseEntity::Clear( void )
{
	m_bDormant = true;
	m_bCanUseBrushModelFastPath = false;
	m_nCreationTick = -1;
	m_RefEHandle.Term();
	m_ModelInstance = MODEL_INSTANCE_INVALID;
	m_ShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_hRender = INVALID_CLIENT_RENDER_HANDLE;
	m_hThink = INVALID_THINK_HANDLE;
	m_AimEntsListHandle = INVALID_AIMENTS_LIST_HANDLE;

	index = -1;
	m_Collision.Init( this );
	CleanUpAlphaProperty();
	m_pClientAlphaProperty = static_cast< CClientAlphaProperty * >( ClientAlphaPropertyMgr()->CreateClientAlphaProperty( this ) );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
	model = nullptr;
	m_vecAbsOrigin.Init();
	m_angAbsRotation.Init();
	m_vecVelocity.Init();
	ClearFlags();
	m_vecViewOffset.Init();
	m_vecBaseVelocity.Init();
	m_nModelIndex = 0;
	m_flAnimTime = 0;
	m_flSimulationTime = 0;
	SetSolid( SOLID_NONE );
	SetSolidFlags( 0 );
	SetMoveCollide( MOVECOLLIDE_DEFAULT );
	SetMoveType( MOVETYPE_NONE );

	ClearEffects();
	m_iEFlags = 0;
	m_nRenderMode = 0;
	m_nOldRenderMode = 0;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	SetRenderFX( kRenderFxNone );
	m_flFriction = 0.0f;       
	m_flGravity = 0.0f;
	SetCheckUntouch( false );
	m_ShadowDirUseOtherEntity = nullptr;

	m_nLastThinkTick = gpGlobals->tickcount;

	// Remove prediction context if it exists
	// Do not enable this on all entities. It forces bone setup for entities that
	// don't need it.
	//AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// IClientUnknown 
//-----------------------------------------------------------------------------
IClientAlphaProperty* CBaseEntity::GetClientAlphaProperty()
{
	return m_pClientAlphaProperty;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::Activate()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::SpawnClientEntity( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Attach to entity
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::Init( int entnum, int iSerialNum )
{
	contract_assert( entnum >= 0 && entnum < NUM_ENT_ENTRIES );

	index = entnum;
	m_pClientAlphaProperty->SetDesyncOffset( index );

	cl_entitylist->AddNetworkableEntity( GetIClientUnknown(), entnum, iSerialNum );

	CollisionProp()->CreatePartitionHandle();

	InitSharedVars();

	Interp_SetupMappings( GetVarMapping() );

	m_nCreationTick = gpGlobals->tickcount;

	m_hScriptInstance = nullptr;
	
	return true;
}
					  
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseEntity::InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels )
{
	int nModelIndex;

	if ( pszModelName != nullptr )
	{
		nModelIndex = modelinfo->GetModelIndex( pszModelName );
		
		if ( nModelIndex == -1 )
		{
			// Model could not be found
			contract_assert( !"Model could not be found, index is -1" );
			return false;
		}
	}
	else
	{
		nModelIndex = -1;
	}

	Interp_SetupMappings( GetVarMapping() );

	return InitializeAsClientEntityByIndex( nModelIndex, bRenderWithViewModels );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseEntity::InitializeAsClientEntityByIndex( int iIndex, bool bRenderWithViewModels )
{
	// Setup model data.
	RenderWithViewModels( bRenderWithViewModels );

	// NOTE: This will add the client entity to the renderable "leaf system" (Renderable)
	SetModelByIndex( iIndex );

	// Add the client entity to the master entity list.
	cl_entitylist->AddNonNetworkableEntity( GetIClientUnknown() );
	contract_assert( GetClientHandle() != ClientEntityList().InvalidHandle() );

	// Add the client entity to the spatial partition. (Collidable)
	CollisionProp()->CreatePartitionHandle();

	index = -1;
	m_pClientAlphaProperty->SetDesyncOffset( rand() % 1024 );

	SpawnClientEntity();

	return true;
}

void CBaseEntity::Term()
{
	CBaseEntity::PhysicsRemoveTouchedList( this );
	CBaseEntity::PhysicsRemoveGroundList( this );
	DestroyAllDataObjects();

	// Remove from the predictables list
	if ( GetPredictable() || IsClientCreated() )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			GetPredictables( i )->RemoveFromPredictablesList( this );
		}
	}

	// If it's play simulated, remove from simulation list if the player still exists...
	if ( IsPlayerSimulated() )
	{
		UnsetPlayerSimulated();
	}

	if ( GetClientHandle() != INVALID_CLIENTENTITY_HANDLE )
	{
		if ( GetThinkHandle() != INVALID_THINK_HANDLE )
		{
			ClientThinkList()->RemoveThinkable( GetClientHandle() );
		}

		// Remove from the client entity list.
		ClientEntityList().RemoveEntity( GetClientHandle() );

		m_RefEHandle = INVALID_CLIENTENTITY_HANDLE;
	}
	
	// Are we in the partition?
	CollisionProp()->DestroyPartitionHandle();

	// If Client side only entity index will be -1
	if ( index != -1 )
	{
		beams->KillDeadBeams( this );
	}

	// Clean up the model instance
	DestroyModelInstance();

	// Clean up drawing
	RemoveFromLeafSystem();

	RemoveFromAimEntsList();

	if ( m_hScriptInstance )
	{
		g_pScriptVM->RemoveInstance( m_hScriptInstance );
		m_hScriptInstance = nullptr;
	}
}

void CBaseEntity::SetRefEHandle( const CBaseHandle &handle )
{
	m_RefEHandle = handle;
}

const CBaseHandle& CBaseEntity::GetRefEHandle() const
{
	return m_RefEHandle;
}

//-----------------------------------------------------------------------------
// Purpose: Free beams and destroy object
//-----------------------------------------------------------------------------
void CBaseEntity::Release()
{
	{
		CBaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
		UnlinkFromHierarchy();
	}

	// Note that this must be called from here, not the destructor, because otherwise the
	//  vtable is hosed and the derived classes function is not going to get called!!!
	if ( IsIntermediateDataAllocated() )
	{
		DestroyIntermediateData();
	}

	UpdateOnRemove();

	delete this;
}

//-----------------------------------------------------------------------------
// Only meant to be called from subclasses.
// Returns true if instance valid, false otherwise
//-----------------------------------------------------------------------------
void CBaseEntity::CreateModelInstance()
{
	if ( m_ModelInstance == MODEL_INSTANCE_INVALID )
	{
		m_ModelInstance = modelrender->CreateInstance( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::DestroyModelInstance()
{
	if (m_ModelInstance != MODEL_INSTANCE_INVALID)
	{
		modelrender->DestroyInstance( m_ModelInstance );
		m_ModelInstance = MODEL_INSTANCE_INVALID;
	}
}

void CBaseEntity::SetRemovalFlag( bool bRemove ) 
{ 
	if (bRemove) 
		m_iEFlags |= EFL_KILLME; 
	else 
		m_iEFlags &= ~EFL_KILLME; 
}

//-----------------------------------------------------------------------------
// Alpha
//-----------------------------------------------------------------------------
void CBaseEntity::SetRenderAlpha( byte a )
{
	if ( m_clrRender.GetA() != a )
	{
		m_clrRender.SetA( a );
		m_pClientAlphaProperty->SetAlphaModulation( a );
	}
}

byte CBaseEntity::GetRenderAlpha() const
{
	return m_pClientAlphaProperty->GetAlphaModulation( );
}

//-----------------------------------------------------------------------------
// Methods related to fade scale
//-----------------------------------------------------------------------------
float CBaseEntity::GetMinFadeDist( ) const
{
	return m_fadeMinDist;
}

float CBaseEntity::GetMaxFadeDist( ) const
{
	return m_fadeMaxDist;
}

void CBaseEntity::SetDistanceFade( float flMinDist, float flMaxDist )
{
	m_fadeMinDist = flMinDist;
	m_fadeMaxDist = flMaxDist;

	// NOTE: Setting the fade will not necessarily produce the same values
	// as what was passed in. I'm deliberately choosing not to grab them back out
	// because I'm not sure what client logic depends on them being negative, for example

	// Specifically, I'm certain the loading logic in CPhysPropClientside,
	// as well as code inside of CPhysPropClientside::Initialize
	// will definitely not work unless I'm doing it the way I'm currently doing it.
	AlphaProp()->SetFade( m_flFadeScale, m_fadeMinDist, m_fadeMaxDist );
}

void CBaseEntity::SetGlobalFadeScale( float flFadeScale )
{
	m_flFadeScale = flFadeScale;
	int modelType = modelinfo->GetModelType( model );
	if ( modelType == mod_studio )
	{
		MDLCACHE_CRITICAL_SECTION();
		MDLHandle_t hStudioHdr = modelinfo->GetCacheHandle( model );
		if ( hStudioHdr != MDLHANDLE_INVALID )
		{
			const studiohdr_t *pStudioHdr = mdlcache->LockStudioHdr( hStudioHdr );
			if ( pStudioHdr->flags & STUDIOHDR_FLAGS_NO_FORCED_FADE )
			{
				flFadeScale = 0.0f;
			}
			mdlcache->UnlockStudioHdr( hStudioHdr );
		}
	}
	AlphaProp()->SetFade( flFadeScale, m_fadeMinDist, m_fadeMaxDist );
}

float CBaseEntity::GetGlobalFadeScale( ) const
{
	return m_flFadeScale;
}

//-----------------------------------------------------------------------------
// VPhysics objects..
//-----------------------------------------------------------------------------
int CBaseEntity::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		// multi-object entities must implement this function
		contract_assert( !(pPhys->GetGameFlags() & FVPHYSICS_MULTIOBJECT_ENTITY) );
		if ( listMax > 0 )
		{
			pList[0] = pPhys;
			return 1;
		}
	}
	return 0;
}

bool CBaseEntity::VPhysicsIsFlesh( void )
{
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = VPhysicsGetObjectList( pList, std::size( pList ) );
	for ( int i = 0; i < count; i++ )
	{
		int material = pList[i]->GetMaterialIndex();
		const surfacedata_t *pSurfaceData = physprops->GetSurfaceData( material );
		// Is flesh ?, don't allow pickup
		if ( pSurfaceData->game.material == CHAR_TEX_ANTLION || pSurfaceData->game.material == CHAR_TEX_FLESH || pSurfaceData->game.material == CHAR_TEX_BLOODYFLESH || pSurfaceData->game.material == CHAR_TEX_ALIENFLESH )
			return true;
	}
	return false;
}

void CBaseEntity::VPhysicsCompensateForPredictionErrors( const byte *predicted_state_data )
{
	contract_assert( GetPredictable() );

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	IPredictedPhysicsObject *pPredictedObject = pPhysicsObject ? pPhysicsObject->GetPredictedInterface() : nullptr;
	if( pPredictedObject )
	{
		Vector vPredictedOrigin;
		
		{
			const typedescription_t *tdOrigin = CPredictionCopy::FindFlatFieldByName( "m_vecNetworkOrigin", GetPredDescMap() );
			contract_assert( tdOrigin );
			V_memcpy( &vPredictedOrigin, predicted_state_data + tdOrigin->flatOffset[ TD_OFFSET_PACKED ], sizeof( Vector ) );

		}

		Vector vOriginDelta = m_vecNetworkOrigin - vPredictedOrigin;

		Vector vPredictedVelocity;
		{
			const typedescription_t *tdVelocity = CPredictionCopy::FindFlatFieldByName( "m_vecAbsVelocity", GetPredDescMap() );			
			contract_assert( tdVelocity );
			V_memcpy( &vPredictedVelocity, predicted_state_data + tdVelocity->flatOffset[ TD_OFFSET_PACKED ], sizeof( Vector ) );
		}

		Vector vVelocityDelta = m_vecAbsVelocity - vPredictedVelocity;

		pPredictedObject->SetErrorDelta_Position( vOriginDelta );
		pPredictedObject->SetErrorDelta_Velocity( vVelocityDelta );
	}	
}

//-----------------------------------------------------------------------------
// Returns the health fraction
//-----------------------------------------------------------------------------
float CBaseEntity::HealthFraction() const
{
	if (GetMaxHealth() == 0)
		return 1.0f;

	float flFraction = (float)GetHealth() / (float)GetMaxHealth();
	flFraction = clamp( flFraction, 0.0f, 1.0f );
	return flFraction;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves the coordinate frame for this entity.
// Input  : forward - Receives the entity's forward vector.
//			right - Receives the entity's right vector.
//			up - Receives the entity's up vector.
//-----------------------------------------------------------------------------
void CBaseEntity::GetVectors(Vector* pForward, Vector* pRight, Vector* pUp) const
{
	// This call is necessary to cause m_rgflCoordinateFrame to be recomputed
	const matrix3x4_t &entityToWorld = EntityToWorldTransform();

	if (pForward != nullptr)
	{
		MatrixGetColumn( entityToWorld, 0, *pForward ); 
	}

	if (pRight != nullptr)
	{
		MatrixGetColumn( entityToWorld, 1, *pRight ); 
		*pRight *= -1.0f;
	}

	if (pUp != nullptr)
	{
		MatrixGetColumn( entityToWorld, 2, *pUp ); 
	}
}

void CBaseEntity::UpdateVisibilityAllEntities()
{
	CBaseEntityIterator iterator;
	CBaseEntity *pEnt;
	while ( (pEnt = iterator.Next()) != nullptr )	
	{
		pEnt->UpdateVisibility();	
	}
}

// (static function)
static void cl_updatevisibility( const CCommand &args );
static ConCommand cl_updatevisibility_command( side_registry(), "cl_updatevisibility", cl_updatevisibility, "Updates visibility bits." );
static void cl_updatevisibility( const CCommand &args )
{
	CBaseEntity::UpdateVisibilityAllEntities();
}
			    
void CBaseEntity::RenderWithViewModels( bool bEnable )
{
	m_bRenderWithViewModels = bEnable;
	g_pClientLeafSystem->RenderWithViewModels( m_hRender, bEnable );
}

void CBaseEntity::RenderForceOpaquePass( bool bEnable )
{
	g_pClientLeafSystem->EnableForceOpaquePass( m_hRender, bEnable );
}

bool CBaseEntity::IsRenderForceOpaquePass() const
{
	return (m_hRender != INVALID_CLIENT_RENDER_HANDLE && g_pClientLeafSystem->IsEnableForceOpaquePass( m_hRender ));
}

bool CBaseEntity::IsRenderingWithViewModels() const
{
	contract_assert( ( m_hRender == INVALID_CLIENT_RENDER_HANDLE ) || 
		( m_bRenderWithViewModels == g_pClientLeafSystem->IsRenderingWithViewModels( m_hRender ) ) );
	return m_bRenderWithViewModels;
}

void CBaseEntity::DisableCachedRenderBounds( bool bDisabled )
{
	m_bDisableCachedRenderBounds = bDisabled;
	g_pClientLeafSystem->DisableCachedRenderBounds( m_hRender, bDisabled );
}

bool CBaseEntity::IsCachedRenderBoundsDisabled() const
{
	return m_bDisableCachedRenderBounds;
}

void CBaseEntity::UpdateVisibility()
{

	m_VisibilityBits.reset();
	
	// NOTE: Do not reactivate this. We need to use ShouldSuppressForSplitScreenPlayer
	// for make portals work. Make sure ShouldDraw always returns true
	bool bDraw;
	{
		CSetActiveSplitScreenPlayerGuard g( __FILE__, __LINE__ );
		bDraw = ( ShouldDraw() && !IsDormant() && ( !ToolsEnabled() || IsEnabledInToolView() ) );
	}
	if ( bDraw )
	{
		bool bIsSplitScreenActive = engine->IsSplitScreenActive();
		if ( !bIsSplitScreenActive )
		{
			CBasePlayer::SetRemoteSplitScreenPlayerViewsAreLocalPlayer( true );
			IterateRemoteSplitScreenViewSlots_Push( true );
		}

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			bool bShouldSkip = ShouldSuppressForSplitScreenPlayer( hh );
			if ( !bShouldSkip )
			{
				m_VisibilityBits.set( hh );
			}
		}

		if ( !bIsSplitScreenActive )
		{
			IterateRemoteSplitScreenViewSlots_Pop();
			CBasePlayer::SetRemoteSplitScreenPlayerViewsAreLocalPlayer( false );
		}
	}

	if ( bDraw )
	{
		// add/update leafsystem
		AddToLeafSystem();
	}
	else
	{
		// remove from leaf system
		RemoveFromLeafSystem();
	}
}

bool CBaseEntity::ShouldDrawForSplitScreenUser( int nSlot )
{ 
	return m_VisibilityBits[nSlot];
}

//-----------------------------------------------------------------------------
// Hooks into the fast path render system
//-----------------------------------------------------------------------------
IClientModelRenderable*	CBaseEntity::GetClientModelRenderable()
{
	if ( !m_bReadyToDraw || !m_bCanUseBrushModelFastPath )
		return nullptr;

	return this; 
}

//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
bool CBaseEntity::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
	switch ( nCategory )
	{
	case MODEL_DATA_STENCIL:
		return false; //ComputeStencilState( (ShaderStencilState_t*)pData );

	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether object should render.
//-----------------------------------------------------------------------------
bool CBaseEntity::ShouldDraw()
{
// Only test this in tf2

	// Some rendermodes prevent rendering
	if ( m_nRenderMode == kRenderNone )
		return false;

	CPULevel_t nCPULevel = GetCPULevel();
	bool bNoDraw = ( m_nMinCPULevel && m_nMinCPULevel-1 > nCPULevel );
	bNoDraw = bNoDraw || ( m_nMaxCPULevel && m_nMaxCPULevel-1 < nCPULevel );
	if ( bNoDraw )
		return false;

	GPULevel_t nGPULevel = GetGPULevel();
	bNoDraw = ( m_nMinGPULevel && m_nMinGPULevel-1 > nGPULevel );
	bNoDraw = bNoDraw || ( m_nMaxGPULevel && m_nMaxGPULevel-1 < nGPULevel );
	if ( bNoDraw )
		return false;

	return (model != 0) && !IsEffectActive(EF_NODRAW) && (index != 0);
}

bool CBaseEntity::TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace )
{
	return false;
}

bool CBaseEntity::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return false;
}

//-----------------------------------------------------------------------------
// Used when the collision prop is told to ask game code for the world-space surrounding box
//-----------------------------------------------------------------------------
void CBaseEntity::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	// This should only be called if you're using USE_GAME_CODE on the server
	// and you forgot to implement the client-side version of this method.
	contract_assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Derived classes will have to write their own message cracking routines!!!
// Input  : length - 
//			*data - 
//-----------------------------------------------------------------------------
void CBaseEntity::ReceiveMessage( int classID, bf_read &msg )
{
	// BaseEntity doesn't have a base class we could relay this message to
	
	int messageType = msg.ReadByte();
	switch( messageType )
	{
		case BASEENTITY_MSG_REMOVE_DECALS:	RemoveAllDecals();
											break;
	}
}

void* CBaseEntity::GetDataTableBasePtr()
{
	return this;
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t CBaseEntity::ShadowCastType()
{
	if (IsEffectActive(EF_NODRAW | EF_NOSHADOW))
		return SHADOWS_NONE;

	int modelType = modelinfo->GetModelType( model );
	return (modelType == mod_studio) ? SHADOWS_RENDER_TO_TEXTURE : SHADOWS_NONE;
}

//-----------------------------------------------------------------------------
// Fast reflections
//-----------------------------------------------------------------------------
bool CBaseEntity::ComputeIsRenderingInFastReflections() const
{
	if ( IsEffectActive( EF_MARKED_FOR_FAST_REFLECTION ) )
		return true;

	CBaseEntity *pParent = GetMoveParent();
	if ( pParent )
		return pParent->ComputeIsRenderingInFastReflections();
	return false;
}

bool CBaseEntity::IsRenderingInFastReflections() const
{
	contract_assert( ( m_hRender == INVALID_CLIENT_RENDER_HANDLE ) || 
		( ComputeIsRenderingInFastReflections() == g_pClientLeafSystem->IsRenderingInFastReflections( m_hRender ) ) );
	return ComputeIsRenderingInFastReflections();
}

void CBaseEntity::OnFastReflectionRenderingChanged()
{
	bool bIsReflecting = ComputeIsRenderingInFastReflections();

	bool bChanged = ( m_hRender == INVALID_CLIENT_RENDER_HANDLE ) ||
		( bIsReflecting != g_pClientLeafSystem->IsRenderingInFastReflections( m_hRender ) );
	if ( bChanged )
	{
		g_pClientLeafSystem->RenderInFastReflections( m_hRender, bIsReflecting );

		// Children must also update
		for ( CBaseEntity *pChild = FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
		{
			pChild->OnFastReflectionRenderingChanged();
		}
	}
}

void CBaseEntity::OnDisableShadowDepthRenderingChanged()
{
	bool bIsShadowDepthRenderingDisabled = IsEffectActive( EF_NOSHADOWDEPTH );
	g_pClientLeafSystem->DisableShadowDepthRendering( m_hRender, bIsShadowDepthRenderingDisabled );
}

void CBaseEntity::OnDisableCSMRenderingChanged()
{
	bool bIsCSMRenderingDisabled = IsEffectActive( EF_NOCSM );
	g_pClientLeafSystem->DisableCSMRendering( m_hRender, bIsCSMRenderingDisabled );
}

void CBaseEntity::OnShadowDepthRenderingCacheableStateChanged()
{
	bool bIsShadowDepthRenderingCacheDisabled = IsEffectActive( EF_SHADOWDEPTH_NOCACHE );
	g_pClientLeafSystem->DisableShadowDepthCaching( m_hRender, bIsShadowDepthRenderingCacheDisabled );
}

//-----------------------------------------------------------------------------
// Per-entity shadow cast distance + direction
//-----------------------------------------------------------------------------
bool CBaseEntity::GetShadowCastDistance( float *pDistance, ShadowType_t shadowType ) const			
{ 
	if ( m_flShadowCastDistance != 0.0f )
	{
		*pDistance = m_flShadowCastDistance; 
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CBaseEntity::GetShadowUseOtherEntity( void ) const
{
	return m_ShadowDirUseOtherEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::SetShadowUseOtherEntity( CBaseEntity *pEntity )
{
	m_ShadowDirUseOtherEntity = pEntity;
}

CDiscontinuousInterpolatedVar< QAngle >& CBaseEntity::GetRotationInterpolator()
{
	return m_iv_angRotation;
}

CDiscontinuousInterpolatedVar< Vector >& CBaseEntity::GetOriginInterpolator()
{
	return m_iv_vecOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: Return a per-entity shadow cast direction
//-----------------------------------------------------------------------------
bool CBaseEntity::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const			
{ 
	if ( m_ShadowDirUseOtherEntity )
		return m_ShadowDirUseOtherEntity->GetShadowCastDirection( pDirection, shadowType );

	return false;
}

//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool CBaseEntity::ShouldReceiveProjectedTextures( int flags )
{
	contract_assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		 return false;

 	if ( IsEffectActive( EF_NOFLASHLIGHT ) )
 		 return false;

	if( ( flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 )
	{
		if ( GetRenderMode() > kRenderNormal && GetRenderAlpha() == 0 )
			 return false;

		return true;
	}

	contract_assert( flags & SHADOW_FLAGS_SHADOW );

	if ( IsEffectActive( EF_NORECEIVESHADOW ) )
		 return false;

	if (modelinfo->GetModelType( model ) == mod_studio)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Shadow-related methods
//-----------------------------------------------------------------------------
bool CBaseEntity::IsShadowDirty( )
{
	return IsEFlagSet( EFL_DIRTY_SHADOWUPDATE );
}

void CBaseEntity::MarkShadowDirty( bool bDirty )
{
	if ( bDirty )
	{
		AddEFlags( EFL_DIRTY_SHADOWUPDATE );
	}
	else
	{
		RemoveEFlags( EFL_DIRTY_SHADOWUPDATE );
	}
}

IClientRenderable *CBaseEntity::GetShadowParent()
{
	CBaseEntity *pParent = GetMoveParent();
	return pParent ? pParent->GetClientRenderable() : nullptr;
}

IClientRenderable *CBaseEntity::FirstShadowChild()
{
	CBaseEntity *pChild = FirstMoveChild();
	return pChild ? pChild->GetClientRenderable() : nullptr;
}

IClientRenderable *CBaseEntity::NextShadowPeer()
{
	CBaseEntity *pPeer = NextMovePeer();
	return pPeer ? pPeer->GetClientRenderable() : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns index into entities list for this entity
// Output : Index
//-----------------------------------------------------------------------------
int	CBaseEntity::entindex( void ) const
{
	return index;
}

int CBaseEntity::GetSoundSourceIndex() const
{
	return GetRefEHandle().GetEntryIndex();
}

//-----------------------------------------------------------------------------
// Get render origin and angles
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::GetRenderOrigin( void )
{
	return GetAbsOrigin();
}

const QAngle& CBaseEntity::GetRenderAngles( void )
{
	return GetAbsAngles();
}

const matrix3x4_t &CBaseEntity::RenderableToWorldTransform()
{
	return EntityToWorldTransform();
}

IPVSNotify* CBaseEntity::GetPVSNotifyInterface()
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : theMins - 
//			theMaxs - 
//-----------------------------------------------------------------------------
void CBaseEntity::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	int nModelType = modelinfo->GetModelType( model );
	if (nModelType == mod_studio || nModelType == mod_brush)
	{
		modelinfo->GetModelRenderBounds( GetModel(), theMins, theMaxs );
	}
	else
	{
		// By default, we'll just snack on the collision bounds, transform
		// them into entity-space, and call it a day.
		if ( GetRenderAngles() == CollisionProp()->GetCollisionAngles() )
		{
			theMins = CollisionProp()->OBBMins();
			theMaxs = CollisionProp()->OBBMaxs();
		}
		else
		{
			contract_assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
			if ( IsPointSized() )
			{
				//theMins = CollisionProp()->GetCollisionOrigin();
				//theMaxs	= theMins;
				theMins = theMaxs = vec3_origin;
			}
			else
			{
				// NOTE: This shouldn't happen! Or at least, I haven't run
				// into a valid case where it should yet.
//				Assert(0);
				IRotateAABB( EntityToWorldTransform(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), theMins, theMaxs );
			}
		}
	}
}

void CBaseEntity::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	DefaultRenderBoundsWorldspace( this, mins, maxs );
}

void CBaseEntity::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	m_EntClientFlags |= ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS;
	GetRenderBounds( mins, maxs );
	m_EntClientFlags &= ~ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS;
}

//-----------------------------------------------------------------------------
// Purpose: Last received origin
// Output : const float
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::GetAbsOrigin( void ) const
{
	contract_assert( s_bAbsQueriesValid );
	const_cast<CBaseEntity*>(this)->CalcAbsolutePosition();
	return m_vecAbsOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: Last received angles
// Output : const
//-----------------------------------------------------------------------------
const QAngle& CBaseEntity::GetAbsAngles( void ) const
{
	contract_assert( s_bAbsQueriesValid );
	const_cast<CBaseEntity*>(this)->CalcAbsolutePosition();
	return m_angAbsRotation;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetNetworkOrigin( const Vector& org )
{
	m_vecNetworkOrigin = org;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetNetworkAngles( const QAngle& ang )
{
	m_angNetworkAngles = ang;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const Vector&
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::GetNetworkOrigin() const
{
	return m_vecNetworkOrigin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const QAngle&
//-----------------------------------------------------------------------------
const QAngle& CBaseEntity::GetNetworkAngles() const
{
	return m_angNetworkAngles;
}

//-----------------------------------------------------------------------------
// Purpose: Get current model pointer for this entity
// Output : const struct model_s
//-----------------------------------------------------------------------------
const model_t *CBaseEntity::GetModel( void ) const
{
	return model;
}

//-----------------------------------------------------------------------------
// Purpose: Get model index for this entity
// Output : int - model index
//-----------------------------------------------------------------------------
int CBaseEntity::GetModelIndex( void ) const
{
	return m_nModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetModelIndex( int index )
{
	m_nModelIndex = index;
	const model_t *pModel = modelinfo->GetModel( m_nModelIndex );
	SetModelPointer( pModel );
}

void CBaseEntity::SetModelPointer( const model_t *pModel )
{
	if ( pModel != model )
	{
		DestroyModelInstance();
		model = pModel;
		OnNewModel();
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : val - 
//			moveCollide - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetMoveType( MoveType_t val, MoveCollide_t moveCollide /*= MOVECOLLIDE_DEFAULT*/ )
{
	// Make sure the move type + move collide are compatible...

 	m_MoveType = val;
	SetMoveCollide( moveCollide );
}

//-----------------------------------------------------------------------------
// Purpose: Get rendermode
// Output : int - the render mode
//-----------------------------------------------------------------------------
bool CBaseEntity::IsTransparent( void )
{
	bool modelIsTransparent = modelinfo->IsTranslucent(model);
	return modelIsTransparent || (m_nRenderMode != kRenderNormal);
}

//-----------------------------------------------------------------------------
// Default implementation of compute translucency type
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t CBaseEntity::ComputeTranslucencyType()
{
	if ( m_bIsBlurred )
		return RENDERABLE_IS_TRANSLUCENT;
	return modelinfo->ComputeTranslucencyType( model, GetSkin(), GetBody() );
}

//-----------------------------------------------------------------------------
// Client code should call this under any circumstances where translucency type may change
//-----------------------------------------------------------------------------
void CBaseEntity::OnTranslucencyTypeChanged()
{
	if ( m_hRender != INVALID_CLIENT_RENDER_HANDLE )
	{
		g_pClientLeafSystem->SetTranslucencyType( m_hRender, ComputeTranslucencyType() );
	}
}

//-----------------------------------------------------------------------------
// Client code should call this under any circumstances where splitscreen rendering may change
//-----------------------------------------------------------------------------
void CBaseEntity::OnSplitscreenRenderingChanged()
{
	if ( IsSplitScreenSupported() && ( m_hRender != INVALID_CLIENT_RENDER_HANDLE ) )
	{
		g_pClientLeafSystem->EnableSplitscreenRendering( m_hRender, ComputeSplitscreenRenderingFlags( this ) );
	}
}

int CBaseEntity::GetRenderFlags( void )
{
	const model_t *pModel = GetModel();
	if ( pModel && render->DoesBrushModelNeedPowerOf2Framebuffer( pModel ) )
	{
		return ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB;
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to CMouthInfo data
// Output : CMouthInfo
//-----------------------------------------------------------------------------
CMouthInfo *CBaseEntity::GetMouth( void )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve sound spatialization info for the specified sound on this entity
// Input  : info - 
// Output : Return false to indicate sound is not audible
//-----------------------------------------------------------------------------
bool CBaseEntity::GetSoundSpatialization( SpatializationInfo_t& info )
{
	// World is always audible
	if ( entindex() == 0 )
	{
		return true;
	}

	// Out of PVS
	if ( IsDormant() )
	{
		return false;
	}

	// pModel might be NULL, but modelinfo can handle that
	const model_t *pModel = GetModel();
	
	if ( info.pflRadius )
	{
		*info.pflRadius = modelinfo->GetModelRadius( pModel );
	}
	
	if ( info.pOrigin )
	{
		*info.pOrigin = GetAbsOrigin();

		// move origin to middle of brush
		if ( modelinfo->GetModelType( pModel ) == mod_brush )
		{
			Vector mins, maxs, center;

			modelinfo->GetModelBounds( pModel, mins, maxs );
			VectorAdd( mins, maxs, center );
			VectorScale( center, 0.5f, center );

			(*info.pOrigin) += center;
		}
	}

	if ( info.pAngles )
	{
		VectorCopy( GetAbsAngles(), *info.pAngles );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get attachment point by index
// Input  : number - which point
// Output : float * - the attachment point
//-----------------------------------------------------------------------------
bool CBaseEntity::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	origin = GetAbsOrigin();
	angles = GetAbsAngles();
	return true;
}

bool CBaseEntity::GetAttachment( int number, Vector &origin )
{
	origin = GetAbsOrigin();
	return true;
}

bool CBaseEntity::GetAttachment( int number, matrix3x4_t &matrix )
{
	MatrixCopy( EntityToWorldTransform(), matrix );
	return true;
}

bool CBaseEntity::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	originVel = GetAbsVelocity();
	angleVel.Init();
	return true;
}

bool CBaseEntity::ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter )
{
	if ( nAttachmentIndex <= 0 )
	{
		VectorTransform( modelLightingCenter, matrix, transformedLightingCenter );
	}
	else
	{
		matrix3x4_t attachmentTransform;
		GetAttachment( nAttachmentIndex, attachmentTransform );
		VectorTransform( modelLightingCenter, attachmentTransform, transformedLightingCenter );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get this entity's rendering clip plane if one is defined
// Output : float * - The clip plane to use, or NULL if no clip plane is defined
//-----------------------------------------------------------------------------
float *CBaseEntity::GetRenderClipPlane( void )
{
	if( m_bEnableRenderingClipPlane )
		return m_fRenderingClipPlane;
	else
		return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseEntity::DrawBrushModel( bool bDrawingTranslucency, int nFlags, bool bTwoPass )
{
	CVProfScope _( "CBaseEntity::DrawBrushModel", 0, VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING, false, BUDGETFLAG_OTHER );
	// Identity brushes are drawn in view->DrawWorld as an optimization
	contract_assert( modelinfo->GetModelType( model ) == mod_brush );

	ERenderDepthMode_t DepthMode = DEPTH_MODE_NORMAL;
	if ( ( nFlags & STUDIO_SSAODEPTHTEXTURE ) != 0 )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}
	else if ( ( nFlags & STUDIO_SHADOWDEPTHTEXTURE ) != 0 )
	{
		DepthMode = DEPTH_MODE_SHADOW;
	}

	if ( DepthMode != DEPTH_MODE_NORMAL )
	{
		render->DrawBrushModelShadowDepth( this, (model_t *)model, GetAbsOrigin(), GetAbsAngles(), DepthMode );
	}
	else
	{
		DrawBrushModelMode_t mode = DBM_DRAW_ALL;
		if ( bTwoPass )
		{
			mode = bDrawingTranslucency ? DBM_DRAW_TRANSLUCENT_ONLY : DBM_DRAW_OPAQUE_ONLY;
		}
		render->DrawBrushModelEx( this, (model_t *)model, GetAbsOrigin(), GetAbsAngles(), mode );
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
int CBaseEntity::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	int drawn = 0;
	if ( !model )
	{
		return drawn;
	}

	int modelType = modelinfo->GetModelType( model );
	switch ( modelType )
	{
	case mod_brush:
		render->SetBlend( ( flags & STUDIO_SHADOWDEPTHTEXTURE ) ? 1.0f : (float)instance.m_nAlpha * ( 1.0f / 255.0f ) );
		drawn = DrawBrushModel( ( flags & STUDIO_TRANSPARENCY ) ? true : false, ( flags & STUDIO_SHADOWDEPTHTEXTURE ) ? true : false, ( flags & STUDIO_TWOPASS ) ? true : false );
		break;
	case mod_studio:
		// All studio models must be derived from CBaseAnimating.  Issue warning.
		Warning( "ERROR:  Can't draw studio model %s because %s is not derived from CBaseAnimating\n",
			modelinfo->GetModelName( model ), GetClientClass()->m_pNetworkName ? GetClientClass()->m_pNetworkName : "unknown" );
		break;
	case mod_sprite:
		//drawn = DrawSprite();
		Warning( "ERROR:  Sprite model's not supported any more except in legacy temp ents\n" );
		break;
	default:
		break;
	}

	// If we're visualizing our bboxes, draw them
	DrawBBoxVisualizations();

	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: Setup the bones for drawing
//-----------------------------------------------------------------------------
bool CBaseEntity::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void CBaseEntity::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
}

//-----------------------------------------------------------------------------
// Purpose: Process any local client-side animation events
//-----------------------------------------------------------------------------
void CBaseEntity::DoAnimationEvents( )
{
}

void CBaseEntity::UpdatePartitionListEntry()
{
	// Don't add the world entity
	CollideType_t shouldCollide = GetCollideType();

	// Choose the list based on what kind of collisions we want
	int list = PARTITION_CLIENT_NON_STATIC_EDICTS;
	if (shouldCollide == ENTITY_SHOULD_COLLIDE)
		list |= PARTITION_CLIENT_SOLID_EDICTS;
	else if (shouldCollide == ENTITY_SHOULD_RESPOND)
		list |= PARTITION_CLIENT_RESPONSIVE_EDICTS;

	if ( m_bIsValidIKAttachment )
	{
		list |= PARTITION_CLIENT_IK_ATTACHMENT;
	}

	// add the entity to the KD tree so we will collide against it
	::partition->RemoveAndInsert( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS | PARTITION_CLIENT_IK_ATTACHMENT, list, CollisionProp()->GetPartitionHandle() );
}

void CBaseEntity::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Init should have been called before we get in here.
	contract_assert( CollisionProp()->GetPartitionHandle() != PARTITION_INVALID_HANDLE );
	if ( entindex() < 0 )
		return;
	
	switch( state )
	{
	case SHOULDTRANSMIT_START:
		{
			// We've just been sent by the server. Become active.
			SetDormant( false );
			
			UpdatePartitionListEntry();

		}
		break;

	case SHOULDTRANSMIT_END:
		{
			// Clear out links if we're out of the picture...
			UnlinkFromHierarchy();

			// We're no longer being sent by the server. Become dormant.
			SetDormant( true );
			
			// remove the entity from the KD tree so we won't collide against it
			::partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );
		
		}
		break;

	default:
		contract_assert( 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Call this in PostDataUpdate if you don't chain it down!
//-----------------------------------------------------------------------------
void CBaseEntity::MarkMessageReceived()
{
	m_flLastMessageTime = engine->GetLastTimeStamp();
}

//-----------------------------------------------------------------------------
// Purpose: Entity is about to be decoded from the network stream
// Input  : bnewentity - is this a new entity this update?
//-----------------------------------------------------------------------------
void CBaseEntity::PreDataUpdate( DataUpdateType_t updateType )
{
	// Register for an OnDataChanged call and call OnPreDataChanged().
	if ( AddDataChangeEvent( this, updateType, &m_DataChangeEventRef ) )
	{
		OnPreDataChanged( updateType );
	}

	// Need to spawn on client before receiving original network data 
	// in case it overrides any values set up in spawn ( e.g., m_iState )
	bool bnewentity = (updateType == DATA_UPDATE_CREATED);

	if ( !bnewentity )
	{
		Interp_RestoreToLastNetworked( GetVarMapping(), 0 /*both simulation and animation vars*/ );
	}

	if ( bnewentity && !IsClientCreated() )
	{
		m_flSpawnTime = engine->GetLastTimeStamp();
		MDLCACHE_CRITICAL_SECTION();
		Spawn();
	}

	m_vecOldOrigin = GetNetworkOrigin();
	m_vecOldAngRotation = GetNetworkAngles();

	m_flOldAnimTime = m_flAnimTime;
	m_flOldSimulationTime = m_flSimulationTime;

	m_nOldRenderMode = m_nRenderMode;

	if ( m_hRender != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->EnableAlternateSorting( m_hRender, m_bAlternateSorting );
	}
}

const Vector& CBaseEntity::GetOldOrigin()
{
	return m_vecOldOrigin;
}

void CBaseEntity::UnlinkChild( CBaseEntity *pParent, CBaseEntity *pChild )
{
	contract_assert( pChild );
	contract_assert( pParent != pChild );
	contract_assert( pChild->GetMoveParent() == pParent );

	// Unlink from parent
	// NOTE: pParent *may well be NULL*! This occurs
	// when a child has unlinked from a parent, and the child
	// remains in the PVS but the parent has not
	if (pParent && (pParent->m_pMoveChild == pChild))
	{
		contract_assert( !(pChild->m_pMovePrevPeer.IsValid()) );
		pParent->m_pMoveChild = pChild->m_pMovePeer;
	}

	// Unlink from siblings...
	if (pChild->m_pMovePrevPeer)
	{
		pChild->m_pMovePrevPeer->m_pMovePeer = pChild->m_pMovePeer;
	}
	if (pChild->m_pMovePeer)
	{
		pChild->m_pMovePeer->m_pMovePrevPeer = pChild->m_pMovePrevPeer;
	}

	pChild->m_pMovePeer = nullptr;
	pChild->m_pMovePrevPeer = nullptr;
	pChild->m_pMoveParent = nullptr;
	pChild->RemoveFromAimEntsList();
	pChild->OnFastReflectionRenderingChanged();

	Interp_HierarchyUpdateInterpolationAmounts();
}

void CBaseEntity::LinkChild( CBaseEntity *pParent, CBaseEntity *pChild )
{
	contract_assert( !pChild->m_pMovePeer.IsValid() );
	contract_assert( !pChild->m_pMovePrevPeer.IsValid() );
	contract_assert( !pChild->m_pMoveParent.IsValid() );
	contract_assert( pParent != pChild );

	pChild->m_pMovePrevPeer = nullptr;
	pChild->m_pMovePeer = pParent->m_pMoveChild;
	if (pChild->m_pMovePeer)
	{
		pChild->m_pMovePeer->m_pMovePrevPeer = pChild;
	}
	pParent->m_pMoveChild = pChild;
	pChild->m_pMoveParent = pParent;
	pChild->AddToAimEntsList();
	pChild->OnFastReflectionRenderingChanged();

	Interp_HierarchyUpdateInterpolationAmounts();
}

std::vector< CBaseEntity * >	g_AimEntsList;

//-----------------------------------------------------------------------------
// Moves all aiments
//-----------------------------------------------------------------------------
void CBaseEntity::MarkAimEntsDirty()
{
	// FIXME: With the dirty bits hooked into cycle + sequence, it's unclear
	// that this is even necessary any more (provided aiments are always accessing
	// joints or attachments of the move parent).
	//
	// NOTE: This is a tricky algorithm. This list does not actually contain
	// all aim-ents in its list. It actually contains all hierarchical children,
	// of which aim-ents are a part. We can tell if something is an aiment if it has
	// the EF_BONEMERGE effect flag set.
	// 
	// We will first iterate over all aiments and clear their DIRTY_ABSTRANSFORM flag, 
	// which is necessary to cause them to recompute their aim-ent origin 
	// the next time CalcAbsPosition is called. Because CalcAbsPosition calls MoveToAimEnt
	// and MoveToAimEnt calls SetAbsOrigin/SetAbsAngles, that is how CalcAbsPosition
	// will cause the aim-ent's (and all its children's) dirty state to be correctly updated.
	//
	// Then we will iterate over the loop a second time and call CalcAbsPosition on them,
	int i;
	int c = std::ssize( g_AimEntsList );
	for ( i = 0; i < c; ++i )
	{
		CBaseEntity *pEnt = g_AimEntsList[ i ];
		contract_assert( pEnt && pEnt->GetMoveParent() );
		if ( pEnt->IsEffectActive(EF_BONEMERGE | EF_PARENT_ANIMATES) )
		{
			pEnt->AddEFlags( EFL_DIRTY_ABSTRANSFORM );
		}
	}
}

void CBaseEntity::CalcAimEntPositions()
{
	CVProfScope _( "CalcAimEntPositions", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	int i;
	int c = std::ssize( g_AimEntsList );
	for ( i = 0; i < c; ++i )
	{
		CBaseEntity *pEnt = g_AimEntsList[ i ];
		contract_assert( pEnt );
		contract_assert( pEnt->GetMoveParent() );
		if ( pEnt->IsEffectActive(EF_BONEMERGE) )
		{
			pEnt->CalcAbsolutePosition( );
		}
	}
}

void CBaseEntity::AddToAimEntsList()
{
	// Already in list
	if ( m_AimEntsListHandle != INVALID_AIMENTS_LIST_HANDLE )
		return;

	m_AimEntsListHandle = std::ssize( g_AimEntsList ); g_AimEntsList.push_back( this );
}

void CBaseEntity::RemoveFromAimEntsList()
{
	// Not in list yet
	if ( INVALID_AIMENTS_LIST_HANDLE == m_AimEntsListHandle )
	{
		return;
	}

	unsigned int c = std::ssize( g_AimEntsList );

	contract_assert( m_AimEntsListHandle < c );

	unsigned int last = c - 1;

	if ( last == m_AimEntsListHandle )
	{
		// Just wipe the final entry
		ks::swap_remove( g_AimEntsList, last );
	}
	else
	{
		CBaseEntity *lastEntity = g_AimEntsList[ last ];
		// Remove the last entry
		ks::swap_remove( g_AimEntsList, last );

		// And update it's handle to point to this slot.
		lastEntity->m_AimEntsListHandle = m_AimEntsListHandle;
		g_AimEntsList[ m_AimEntsListHandle ] = lastEntity;
	}

	// Invalidate our handle no matter what.
	m_AimEntsListHandle = INVALID_AIMENTS_LIST_HANDLE;
}

//-----------------------------------------------------------------------------
// Update move-parent if needed. For SourceTV.
//-----------------------------------------------------------------------------
void CBaseEntity::HierarchyUpdateMoveParent()
{
	if ( m_hNetworkMoveParent.ToInt() == m_pMoveParent.ToInt() )
		return;

	HierarchySetParent( m_hNetworkMoveParent );
}

//-----------------------------------------------------------------------------
// Connects us up to hierarchy
//-----------------------------------------------------------------------------
void CBaseEntity::HierarchySetParent( CBaseEntity *pNewParent )
{
	// NOTE: When this is called, we expect to have a valid
	// local origin, etc. that we received from network daa
	EHANDLE newParentHandle;
	newParentHandle.Set( pNewParent );
	if (newParentHandle.ToInt() == m_pMoveParent.ToInt())
		return;
	
	if (m_pMoveParent.IsValid())
	{
		UnlinkChild( m_pMoveParent, this );
	}
	if (pNewParent)
	{
		LinkChild( pNewParent, this );
	}

	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );

	// iterate the hierarchy using a ring buffer
	CBaseEntity *list[1024];	// assume power of 2 elements
	int listReadIndex = 0;
	int listWriteIndex = 1;
	list[0] = this;

	while ( listReadIndex != listWriteIndex )
	{
		CBaseEntity *pParent = list[listReadIndex];
		pParent->InvalidateAttachments();
		listReadIndex = (listReadIndex+1) & (std::size( list )-1);
		for (CBaseEntity *pChild = pParent->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer())
		{
			list[listWriteIndex] = pChild;
			listWriteIndex = (listWriteIndex+1) & (std::size( list )-1);
		}
	}
}

//-----------------------------------------------------------------------------
// Unlinks from hierarchy
//-----------------------------------------------------------------------------
void CBaseEntity::SetParent( CBaseEntity *pParentEntity, int iParentAttachment )
{
	// NOTE: This version is meant to be called *outside* of PostDataUpdate
	// as it assumes the moveparent has a valid handle
	EHANDLE newParentHandle;
	newParentHandle.Set( pParentEntity );
	if (newParentHandle.ToInt() == m_pMoveParent.ToInt())
		return;

	// NOTE: Have to do this before the unlink to ensure local coords are valid
	Vector vecAbsOrigin = GetAbsOrigin();
	QAngle angAbsRotation = GetAbsAngles();
	Vector vecAbsVelocity = GetAbsVelocity();

	// First deal with unlinking
	if (m_pMoveParent.IsValid())
	{
		UnlinkChild( m_pMoveParent, this );
	}

	if (pParentEntity)
	{
		LinkChild( pParentEntity, this );
	}

	if ( !IsServerEntity() )
	{
		m_hNetworkMoveParent = pParentEntity;
	}
	
	m_iParentAttachment = iParentAttachment;
	
	m_vecAbsOrigin.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	m_angAbsRotation.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	m_vecAbsVelocity.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );

	SetAbsOrigin(vecAbsOrigin);
	SetAbsAngles(angAbsRotation);
	SetAbsVelocity(vecAbsVelocity);

}

//-----------------------------------------------------------------------------
// Unlinks from hierarchy
//-----------------------------------------------------------------------------
void CBaseEntity::UnlinkFromHierarchy()
{
	// Clear out links if we're out of the picture...
	if ( m_pMoveParent.IsValid() )
	{
		UnlinkChild( m_pMoveParent, this );
	}

	//Adrian: This was causing problems with the local network backdoor with entities coming in and out of the PVS at certain times.
	//This would work fine if a full entity update was coming (caused by certain factors like too many entities entering the pvs at once).
	//but otherwise it would not detect the change on the client (since the server and client shouldn't be out of sync) and the var would not be updated like it should.
	//m_iParentAttachment = 0;

	// unlink also all move children
	CBaseEntity *pChild = FirstMoveChild();
	while( pChild )
	{
		if ( pChild->m_pMoveParent != this )
		{
			Warning( "CBaseEntity::UnlinkFromHierarchy(): Entity has a child with the wrong parent!\n" );
			contract_assert( 0 );
			UnlinkChild( this, pChild );
			pChild->UnlinkFromHierarchy();
		}
		else
			pChild->UnlinkFromHierarchy();
		pChild = FirstMoveChild();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make sure that the correct model is referenced for this entity
//-----------------------------------------------------------------------------
void CBaseEntity::ValidateModelIndex( void )
{
	SetModelByIndex( m_nModelIndex );
}

bool CBaseEntity::IsParentChanging()
{
	return ( m_hNetworkMoveParent.ToInt() != m_pMoveParent.ToInt() );
}

//-----------------------------------------------------------------------------
// Purpose: Entity data has been parsed and unpacked.  Now do any necessary decoding, munging
// Input  : bnewentity - was this entity new in this update packet?
//-----------------------------------------------------------------------------
void CBaseEntity::PostDataUpdate( DataUpdateType_t updateType )
{
	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "postdataupdate" );

	// NOTE: This *has* to happen first. Otherwise, Origin + angles may be wrong 
	if ( m_bClientSideRagdoll && updateType == DATA_UPDATE_CREATED )
	{
		MoveToLastReceivedPosition( true );
	}
	else
	{
		MoveToLastReceivedPosition( false );
	}

	// If it's the world, force solid flags
	if ( index == 0 )
	{
		m_nModelIndex = 1;
		SetSolid( SOLID_BSP );

		// FIXME: Should these be assertions?
		SetAbsOrigin( vec3_origin );
		SetAbsAngles( vec3_angle );
	}

	if ( m_nOldRenderMode != m_nRenderMode )
	{
		SetRenderMode( (RenderMode_t)m_nRenderMode, true );
	}

	bool animTimeChanged = ( m_flAnimTime != m_flOldAnimTime ) ? true : false;
	bool originChanged = ( m_vecOldOrigin != GetLocalOrigin() ) ? true : false;
	bool anglesChanged = ( m_vecOldAngRotation != GetLocalAngles() ) ? true : false;
	bool simTimeChanged = ( m_flSimulationTime != m_flOldSimulationTime ) ? true : false;

	// Detect simulation changes 
	bool simulationChanged = originChanged || anglesChanged || simTimeChanged;

	bool bPredictable = GetPredictable();

	// For non-predicted and non-client only ents, we need to latch network values into the interpolation histories
	if ( !bPredictable && !IsClientCreated() )
	{
		if ( animTimeChanged )
		{
			OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
		}

		if ( simulationChanged )
		{
			OnLatchInterpolatedVariables( LATCH_SIMULATION_VAR );
		}
	}
	// For predictables, we also need to store off the last networked value
	else if ( bPredictable )
	{
		// Just store off last networked value for use in prediction
		OnStoreLastNetworkedValue();
	}

	// Deal with hierarchy. Have to do it here (instead of in a proxy)
	// because this is the only point at which all entities are loaded
	// If this condition isn't met, then a child was sent without its parent
	contract_assert( m_hNetworkMoveParent.Get() || !m_hNetworkMoveParent.IsValid() );
	HierarchySetParent(m_hNetworkMoveParent);

	MarkMessageReceived();

	// Make sure that the correct model is referenced for this entity
	ValidateModelIndex();

	// If this entity was new, then latch in various values no matter what.
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Construct a random value for this instance
		m_flProxyRandomValue = random_valve->RandomFloat( 0, 1 );

		ResetLatched();

		m_nCreationTick = gpGlobals->tickcount;
	}

	// It's possible that a new entity will need to be forceably added to the 
	//   player simulation list.  If so, do this here
	if ( IsPlayerSimulated() && CBasePlayer::IsLocalPlayer( m_hPlayerSimulationOwner ) )
	{
		// Make sure player is driving simulation (field is only ever sent to local player)
		SetPlayerSimulated( ToBasePlayer( m_hPlayerSimulationOwner ) );
	}

	UpdatePartitionListEntry();
	
	// Add the entity to the nointerp list.
	if ( !IsClientCreated() )
	{
		if ( Teleported() || IsEffectActive(EF_NOINTERP) )
			AddToEntityList(ENTITY_LIST_TELEPORT);
	}

	// if we changed parents, recalculate visibility
	if ( m_hOldMoveParent != m_hNetworkMoveParent )
	{
		UpdateVisibility();
	}
}

static ConVar cl_simulationtimefix( side_registry(), "cl_simulationtimefix", "1", FCVAR_DEVELOPMENTONLY );
void CBaseEntity::OnSimulationTimeChanging( float flPreviousSimulationTime, float flNextSimulationTime )
{
	if ( m_bDisableSimulationFix )
		return;

	if ( !cl_simulationtimefix.GetBool() )
	{
		return;
	}

	if ( GetPredictable() || IsClientCreated() )
	{
		return;
	}

	if ( !ShouldDraw() )
	{
		return;
	}

	// If the m_flSimulationTime is changing faster than or in lockstep with the interpolation amount, then never do a fixup
	float flOriginInterpolationAmount = m_iv_vecOrigin.GetInterpolationAmount();
	float dtSimulationTimestamps = ROUND_TO_TICKS( flNextSimulationTime - flPreviousSimulationTime );
	if ( dtSimulationTimestamps <= flOriginInterpolationAmount )
		return;

	// In the worst case (w/o packet loss) the engine could hit a slow frame and have to run off 0.1 (MAX_FRAMETIME) seconds worth of ticks.
	// Thus, something moving would get new m_flSimulationTime sample up to ROUND_TO_TICKS( 0.1f ) seconds from the previous time which
	//  is longer than the interpolation interval but still considered "smooth" and continuous motion.  We don't want to mess that case up, so what we
	//  do is to see how far in the past the previous packet time stamp was and if the simulation time delta is even greater than that only then do we 
	//  add some fixup samples.

	// Since we haven't called PostDataUpdate, m_flLastMessageTime is the timestamp of the last packet containing this entity and engine->GetLastTimeStamp() is the timestamp of the currently being 
	//  processed packet
	// TBD:  Avoid the call into the engine-> virtual interface!!!
	float dtFromLastPacket = ROUND_TO_TICKS( engine->GetLastTimeStamp() - m_flLastMessageTime );

	// If the m_flSimulationTime time will have changed more quickly than the timestamps of the last two packets received, also skip fixup
	if ( dtSimulationTimestamps <= dtFromLastPacket )
		return;

	// Worst case backfill is 100 msecs (default multiplayer interpolation amount)
	float flSampleBackFillMaxTime = ROUND_TO_TICKS( 0.1f );

	m_iv_vecOrigin.RestoreToLastNetworked();
	m_iv_angRotation.RestoreToLastNetworked();

	float flTimeBeforeWhichToLeaveOldSamples = flPreviousSimulationTime + TICK_INTERVAL * 0.5f;

	for ( float simtime = flNextSimulationTime - flSampleBackFillMaxTime;  
		  simtime < flNextSimulationTime; 
		  simtime += TICK_INTERVAL )
	{
		// Don't stomp preexisting data, though
		if ( simtime < flTimeBeforeWhichToLeaveOldSamples )
			continue;

		m_iv_vecOrigin.NoteChanged( gpGlobals->curtime, simtime, false );
		m_iv_angRotation.NoteChanged( gpGlobals->curtime, simtime, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Latch simulation values when the entity has not changed
//-----------------------------------------------------------------------------
void CBaseEntity::OnDataUnchangedInPVS()
{
	contract_assert( m_hNetworkMoveParent.Get() || !m_hNetworkMoveParent.IsValid() );
	HierarchySetParent(m_hNetworkMoveParent);
	
	MarkMessageReceived();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *context - 
//-----------------------------------------------------------------------------
void CBaseEntity::CheckInitPredictable( const char *context )
{
	if ( !ShouldPredict() )
		return;

	// Prediction is disabled
	if ( !cl_predict->GetInt() )
		return;

	if ( !CBasePlayer::HasAnyLocalPlayer() )
		return;

	if ( !GetPredictionEligible() )
	{
		bool bOkay = false;
		if ( !bOkay )
		{
			return;
		}
	}

	if ( IsClientCreated() )
		return;

	if ( IsIntermediateDataAllocated() )
		return;

	// Msg( "Predicting init %s at %s\n", GetClassname(), context );

	// It's either a player, a weapon or a view model
	CBasePlayer *pOwner = GetPredictionOwner();
	contract_assert( pOwner );
	if ( !pOwner )
		return;

	InitPredictable( pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: See if a predictable should stop predicting
// Input  : *context - 
//-----------------------------------------------------------------------------
void CBaseEntity::CheckShutdownPredictable( const char *context )
{
	if ( IsClientCreated() )
		return;

	if ( !ShouldPredict() || 
		!GetPredictionEligible() ||
		(GetPredictionOwner() == nullptr) )
	{
		if( IsIntermediateDataAllocated() )
		{
			ShutdownPredictable();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
CBasePlayer* CBaseEntity::GetPredictionOwner()
{
	CBasePlayer *pOwner = ToBasePlayer( this );
	if ( !pOwner )
	{
		pOwner = ToBasePlayer( GetOwnerEntity() );
		if ( !pOwner )
		{
			CBaseViewModel *vm = ToBaseViewModel(this);
			if ( vm )
			{
				pOwner = ToBasePlayer( vm->GetOwner() );
			}
		}
	}
	return pOwner;
}

bool CBaseEntity::IsSelfAnimating()
{
	return true;
}

//-----------------------------------------------------------------------------
// EFlags.. 
//-----------------------------------------------------------------------------
int CBaseEntity::GetEFlags() const
{
	return m_iEFlags;
}

void CBaseEntity::SetEFlags( int iEFlags )
{
	m_iEFlags = iEFlags;
}

//-----------------------------------------------------------------------------
// Sets the model... 
//-----------------------------------------------------------------------------
void CBaseEntity::SetModelByIndex( int nModelIndex )
{
	SetModelIndex( nModelIndex );
}

//-----------------------------------------------------------------------------
// Set model... (NOTE: Should only be used by client-only entities
//-----------------------------------------------------------------------------
bool CBaseEntity::SetModel( const char *pModelName )
{
	if ( pModelName )
	{
		int nModelIndex = modelinfo->GetModelIndex( pModelName );
		SetModelByIndex( nModelIndex );
		return ( nModelIndex != -1 );
	}
	else
	{
		SetModelByIndex( -1 );
		return false;
	}
}

void CBaseEntity::OnStoreLastNetworkedValue()
{
	bool bRestore = false;
	Vector savePos;
	QAngle saveAng;

	// Kind of a hack, but we want to latch the actual networked value for origin/angles, not what's sitting in m_vecOrigin in the
	//  ragdoll case where we don't copy it over in MoveToLastNetworkOrigin
	if ( m_bClientSideRagdoll && GetPredictable() )
	{
		bRestore = true;
		savePos = GetLocalOrigin();
		saveAng = GetLocalAngles();

		MoveToLastReceivedPosition( true );
	}

	int c = std::ssize( m_VarMap.m_Entries );
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &m_VarMap.m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();

		if ( type & EXCLUDE_AUTO_LATCH )
			continue;

		watcher->NoteLastNetworkedValue();
	}

	if ( bRestore )
	{
		SetLocalOrigin( savePos );
		SetLocalAngles( saveAng );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The animtime is about to be changed in a network update, store off various fields so that
//  we can use them to do blended sequence transitions, etc.
// Input  : *pState - the (mostly) previous state data
//-----------------------------------------------------------------------------

void CBaseEntity::OnLatchInterpolatedVariables( int flags )
{
	float changetime = GetLastChangeTime( flags );

	bool bUpdateLastNetworkedValue = !(flags & INTERPOLATE_OMIT_UPDATE_LAST_NETWORKED) ? true : false;

	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, bUpdateLastNetworkedValue ? "latch+net" : "latch" );

	int c = std::ssize( m_VarMap.m_Entries );
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &m_VarMap.m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		int type = watcher->GetType();

		if ( !(type & flags) )
			continue;

		if ( type & EXCLUDE_AUTO_LATCH )
			continue;

		if ( watcher->NoteChanged( gpGlobals->curtime, changetime, bUpdateLastNetworkedValue ) )
			e->m_bNeedsToInterpolate = true;
	}
	
	if ( ShouldInterpolate() )
	{
		AddToEntityList(ENTITY_LIST_INTERPOLATE);
	}
}

float CBaseEntity::GetEffectiveInterpolationCurTime( float currentTime )
{
	if ( GetPredictable() || IsClientCreated() )
	{
		int slot = GetSplitUserPlayerPredictionSlot();
		contract_assert( slot != -1 );
		CBasePlayer *localplayer = CBasePlayer::GetLocalPlayer( slot );
		if ( localplayer )
		{
			currentTime = localplayer->GetFinalPredictedTime();
			currentTime -= TICK_INTERVAL;
			currentTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
		}
	}

	return currentTime;
}

static ConVar cl_interp_watch( side_registry(), "cl_interp_watch", "-2", 0 );

int CBaseEntity::BaseInterpolatePart1( float &currentTime, Vector &oldOrigin, QAngle &oldAngles, int &bNoMoreChanges )
{
	// Don't mess with the world!!!
	bNoMoreChanges = 1;
	
	// These get moved to the parent position automatically
	if ( IsFollowingEntity() || !IsInterpolationEnabled() )
	{
		// Assume current origin ( no interpolation )
		MoveToLastReceivedPosition();
		return INTERPOLATE_STOP;
	}

	if ( GetPredictable() || IsClientCreated() )
	{
		int slot = GetSplitUserPlayerPredictionSlot();
		contract_assert( slot != -1 );
		CBasePlayer *localplayer = CBasePlayer::GetLocalPlayer( slot );
		if ( localplayer && currentTime == gpGlobals->curtime )
		{
			currentTime = localplayer->GetFinalPredictedTime();
			currentTime -= TICK_INTERVAL;
			currentTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
		}
	}

	//
	// When playing GOTV demo or watching HLTV make sure that the IN_EYE
	// entity and their weapon don't interpolate causing lag behind shots
	//
	if ( HLTVCamera() && engine->IsPlayingDemo() && engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity &&
		( HLTVCamera()->GetMode() == OBS_MODE_IN_EYE ) )
	{
		static const ConVar *s_pUpdateRate = g_pCVar->FindVar( "cl_updaterate" );
		CBaseEntity *pInEyeEntity = HLTVCamera()->GetPrimaryTarget();
		if ( pInEyeEntity && pInEyeEntity->IsPlayer() && s_pUpdateRate )
		{
			float flAdjustDirection = ( ( this == pInEyeEntity ) || ( this->GetMoveParent() == pInEyeEntity ) ) ? 1.0f : -1.0f;
			float flTimeAdjustment = ( 1.0f / s_pUpdateRate->GetFloat() ); // ( 1/16 ) / ( 1/64 ) = 4
			currentTime += flTimeAdjustment * flAdjustDirection; // differently interpolate IN_EYE observed entity and children
		}
	}

	//
	// Run normal calculations to compute interpolation amount
	//

	oldOrigin = m_vecOrigin;
	oldAngles = m_angRotation;

	bNoMoreChanges = Interp_Interpolate( GetVarMapping(), currentTime );
	if ( cl_interp_all.GetInt() || (m_EntClientFlags & ENTCLIENTFLAG_ALWAYS_INTERPOLATE) )
		bNoMoreChanges = 0;

	if ( cl_interp_watch.GetInt() == index )
	{
		SpewInterpolatedVar( &m_iv_vecOrigin, currentTime, m_iv_vecOrigin.GetInterpolationAmount(), true );

	}

	return INTERPOLATE_CONTINUE;
}

void CBaseEntity::BaseInterpolatePart2( Vector &oldOrigin, QAngle &oldAngles, int nChangeFlags )
{
	if ( m_vecOrigin != oldOrigin )
	{
		nChangeFlags |= POSITION_CHANGED;
	}

	if( m_angRotation != oldAngles )
	{
		nChangeFlags |= ANGLES_CHANGED;
	}

	if ( nChangeFlags != 0 )
	{
		InvalidatePhysicsRecursive( nChangeFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Default interpolation for entities
// Output : true means entity should be drawn, false means probably not
//-----------------------------------------------------------------------------
bool CBaseEntity::Interpolate( float currentTime )
{
	CVProfScope _( "CBaseEntity::Interpolate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	Vector oldOrigin;
	QAngle oldAngles;

	int bNoMoreChanges;
	int retVal = BaseInterpolatePart1( currentTime, oldOrigin, oldAngles, bNoMoreChanges );

	// If all the Interpolate() calls returned that their values aren't going to
	// change anymore, then get us out of the interpolation list.
	if ( bNoMoreChanges )
		RemoveFromEntityList(ENTITY_LIST_INTERPOLATE);

	if ( retVal == INTERPOLATE_STOP )
		return true;

	int nChangeFlags = 0;
	BaseInterpolatePart2( oldOrigin, oldAngles, nChangeFlags );

	return true;
}

CStudioHdr *CBaseEntity::OnNewModel()
{
	OnTranslucencyTypeChanged();
	g_pClientLeafSystem->SetModelType( m_hRender );
	InvalidatePhysicsRecursive( BOUNDS_CHANGED | SEQUENCE_CHANGED );
	SetGlobalFadeScale( GetGlobalFadeScale() );

	// Can we use the model fast path?
	const model_t *pModel = GetModel();
	m_bCanUseBrushModelFastPath = pModel && ( modelinfo->GetModelType( pModel ) == mod_brush ) &&
		!modelinfo->ModelHasMaterialProxy( pModel );

	return nullptr;
}

void CBaseEntity::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	return;
}

void CBaseEntity::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	return;
}

// Above this velocity and we'll assume a warp/teleport
static constexpr auto MAX_INTERPOLATE_VELOCITY = 4000.0f;
static constexpr auto MAX_INTERPOLATE_VELOCITY_PLAYER = 1250.0f;

//-----------------------------------------------------------------------------
// Purpose: Determine whether entity was teleported ( so we can disable interpolation )
// Input  : *ent - 
// Output : bool
//-----------------------------------------------------------------------------
bool CBaseEntity::Teleported( void )
{
	// Disable interpolation when hierarchy changes
	if (m_hOldMoveParent != m_hNetworkMoveParent || m_iOldParentAttachment != m_iParentAttachment)
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Is this a submodel of the world ( model name starts with * )?
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsSubModel( void )
{
	if ( model &&
		modelinfo->GetModelType( model ) == mod_brush &&
		modelinfo->GetModelName( model )[0] == '*' )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Create entity lighting effects
//-----------------------------------------------------------------------------
bool CBaseEntity::CreateLightEffects( void )
{
	dlight_t *dl;

	bool bHasLightEffects = false;
	// Is this for player flashlights only, if so move to linkplayers?
	if ( !IsViewEntity() )
	{
		if (IsEffectActive(EF_BRIGHTLIGHT))
		{
			bHasLightEffects = true;
			dl = effects->CL_AllocDlight ( index );
			dl->origin = GetAbsOrigin();
			dl->origin[2] += 16;
			dl->color.r = dl->color.g = dl->color.b = 250;
			dl->radius = random_valve->RandomFloat(400,431);
			dl->die = gpGlobals->curtime + 0.001;
		}
		if (IsEffectActive(EF_DIMLIGHT))
		{
			bHasLightEffects = true;
			dl = effects->CL_AllocDlight ( index );
			dl->origin = GetAbsOrigin();
			dl->color.r = dl->color.g = dl->color.b = 100;
			dl->radius = random_valve->RandomFloat(200,231);
			dl->die = gpGlobals->curtime + 0.001;
		}
	}
	return bHasLightEffects;
}

void CBaseEntity::MoveToLastReceivedPosition( bool force )
{
	if ( force || ( !m_bClientSideRagdoll ) )
	{
		SetLocalOrigin( GetNetworkOrigin() );
		SetLocalAngles( GetNetworkAngles() );
	}
}

bool CBaseEntity::ShouldInterpolate()
{
	if ( IsViewEntity() )
		return true;

	if ( index == 0 || !GetModel() )
		return false;

	// always interpolate if visible
	if ( INVALID_CLIENT_RENDER_HANDLE != m_hRender &&
		m_VisibilityBits.any() )
	{
		return true;
	}

	// if any movement child needs interpolation, we have to interpolate too
	CBaseEntity *pChild = FirstMoveChild();
	while( pChild )
	{
		if ( pChild->ShouldInterpolate() )	
			return true;

		pChild = pChild->NextMovePeer();
	}

	// don't interpolate
	return false;
}

void CBaseEntity::ProcessTeleportList()
{
	unsigned short iNext;
	for ( unsigned short iCur = g_EntityLists[ENTITY_LIST_TELEPORT].head(); iCur != g_EntityLists[ENTITY_LIST_TELEPORT].invalid; iCur = iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_TELEPORT].next( iCur );
		CBaseEntity *pCur = g_EntityLists[ENTITY_LIST_TELEPORT][iCur];

		bool teleport = pCur->Teleported();
		bool ef_nointerp = pCur->IsEffectActive(EF_NOINTERP);
	
		if ( teleport || ef_nointerp )
		{
			// Undo the teleport flag..
			pCur->m_hOldMoveParent = pCur->m_hNetworkMoveParent;			
			pCur->m_iOldParentAttachment = pCur->m_iParentAttachment;
			// Zero out all but last update.
			pCur->MoveToLastReceivedPosition( true );
			pCur->ResetLatched();
		}
		else
		{
			// Get it out of the list as soon as we can.
			pCur->RemoveFromEntityList(ENTITY_LIST_TELEPORT);
		}
	}
}

void CBaseEntity::CheckInterpolatedVarParanoidMeasurement()
{
	// What we're doing here is to check all the entities that were not in the interpolation
	// list and make sure that there's no entity that should be in the list that isn't.
	
}

void CBaseEntity::ProcessInterpolatedList()
{
	CheckInterpolatedVarParanoidMeasurement();

	// Interpolate the minimal set of entities that need it.
	unsigned short iNext;
	for ( unsigned short iCur = g_EntityLists[ENTITY_LIST_INTERPOLATE].head(); iCur != g_EntityLists[ENTITY_LIST_INTERPOLATE].invalid; iCur = iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_INTERPOLATE].next( iCur );
		CBaseEntity *pCur = g_EntityLists[ENTITY_LIST_INTERPOLATE][iCur];
		
		pCur->m_bReadyToDraw = pCur->Interpolate( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Returns the aiment render origin + angles
//-----------------------------------------------------------------------------
void CBaseEntity::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pOrigin, QAngle *pAngles )
{
	// Should be overridden for things that attach to attchment points

	// Slam origin to the origin of the entity we are attached to...
	*pOrigin = pAttachedTo->GetAbsOrigin();
	*pAngles = pAttachedTo->GetAbsAngles();
}

void CBaseEntity::StopFollowingEntity( )
{

	SetParent( nullptr );
	RemoveEffects( EF_BONEMERGE );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
}

bool CBaseEntity::IsFollowingEntity()
{
	return IsEffectActive(EF_BONEMERGE) && (GetMoveType() == MOVETYPE_NONE) && GetMoveParent();
}

CBaseEntity *CBaseEntity::GetFollowedEntity()
{
	if (!IsFollowingEntity())
		return nullptr;
	return GetMoveParent();
}

//-----------------------------------------------------------------------------
// Default implementation for GetTextureAnimationStartTime
//-----------------------------------------------------------------------------
float CBaseEntity::GetTextureAnimationStartTime()
{
	return m_flSpawnTime;
}

//-----------------------------------------------------------------------------
// Default implementation, indicates that a texture animation has wrapped
//-----------------------------------------------------------------------------
void CBaseEntity::TextureAnimationWrapped()
{
}

void CBaseEntity::ClientThink()
{
}

static ConVar hermite( side_registry(), "hermite_fix", "1", FCVAR_DEVELOPMENTONLY, "Don't interpolate previous hermite sample when fixing up times." );

// Defined in engine
static ConVar cl_interpolate( side_registry(), "cl_interpolate", "1", FCVAR_RELEASE, "Enables or disables interpolation on listen servers or during demo playback" );
// (static function)
void CBaseEntity::InterpolateServerEntities()
{
	CVProfScope _( "CBaseEntity::InterpolateServerEntities", 0, VPROF_BUDGETGROUP_INTERPOLATION, false, BUDGETFLAG_OTHER );

	bool bPrevInterpolate = s_bInterpolate;

	bool bHermiteFix = hermite.GetBool();
	if ( bHermiteFix != g_bHermiteFix )
	{
		g_bHermiteFix = bHermiteFix;
	}

	// Determine whether interpolation is enabled
	bool bInterpolate = cl_interpolate.GetBool();
	if ( !bInterpolate && engine->IsConnected() && !engine->IsPlayingDemo() && !engine->IsClientLocalToActiveServer() )
		bInterpolate = true; // client is connected and not playing demo and not on listen server, force interpolation ON
	s_bInterpolate = bInterpolate;

	// Don't interpolate during timedemo playback or when engine is paused
	if ( engine->IsPlayingTimeDemo() || engine->IsPaused() )
	{										 
		s_bInterpolate = false;
	}

	// Don't interpolate, either, if we are timing out
	INetChannelInfo *nci = engine->GetNetChannelInfo();
	if ( nci && nci->GetTimeSinceLastReceived() > 0.5f )
	{
		s_bInterpolate = false;
	}

	if ( IsSimulatingOnAlternateTicks() != g_bWasSkipping || IsEngineThreaded() != g_bWasThreaded )
	{
		g_bWasSkipping = IsSimulatingOnAlternateTicks();
		g_bWasThreaded = IsEngineThreaded();

		CBaseEntityIterator iterator;
		CBaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != nullptr )
		{
			pEnt->Interp_UpdateInterpolationAmounts( pEnt->GetVarMapping() );
		}
	}

	// Enable extrapolation?
	CInterpolationContext context;
	context.SetLastTimeStamp( engine->GetLastTimeStamp() );
	if ( cl_extrapolate.GetBool() && !engine->IsPaused() )
	{
		context.EnableExtrapolation( true );
	}

	if ( bPrevInterpolate != s_bInterpolate && !s_bInterpolate )
	{
		// Clear interp history when we disable interpolation
		CBaseEntityIterator iterator;
		CBaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != nullptr )
		{
			pEnt->ResetLatched();
		}
	}

	// Smoothly interpolate position for server entities.
	ProcessTeleportList();
	ProcessInterpolatedList();
}

// (static function)
void CBaseEntity::AddVisibleEntities()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void CBaseEntity::OnPreDataChanged( DataUpdateType_t type )
{
	m_hOldMoveParent = m_hNetworkMoveParent;
	m_iOldParentAttachment = m_iParentAttachment;
}

void CBaseEntity::OnDataChanged( DataUpdateType_t type )
{
	// Set up shadows; do it here so that objects can change shadowcasting state
	CreateShadow();

	if ( type == DATA_UPDATE_CREATED )
	{
		UpdateVisibility();
	}

	// These may have changed in the network update
	AlphaProp()->SetRenderFX( GetRenderFX(), GetRenderMode() );
	AlphaProp()->SetDesyncOffset( index );

	// Copy in fade parameters
	AlphaProp()->SetFade( m_flFadeScale, m_fadeMinDist, m_fadeMaxDist );
}

ClientThinkHandle_t CBaseEntity::GetThinkHandle()
{
	return m_hThink;
}

void CBaseEntity::SetThinkHandle( ClientThinkHandle_t hThink )
{
	m_hThink = hThink;
}

//-----------------------------------------------------------------------------
// Determine the color modulation amount
//-----------------------------------------------------------------------------
void CBaseEntity::GetColorModulation( float* color )
{
	color[0] = m_clrRender->r / 255.0f;
	color[1] = m_clrRender->g / 255.0f;
	color[2] = m_clrRender->b / 255.0f;
}

//-----------------------------------------------------------------------------
// Returns true if we should add this to the collision list
//-----------------------------------------------------------------------------
CollideType_t CBaseEntity::GetCollideType( void )
{
	if ( !m_nModelIndex || !model )
		return ENTITY_SHOULD_NOT_COLLIDE;

	if ( !IsSolid( ) )
		return ENTITY_SHOULD_NOT_COLLIDE;

	// If the model is a bsp or studio (i.e. it can collide with the player
	if ( ( modelinfo->GetModelType( model ) != mod_brush ) && ( modelinfo->GetModelType( model ) != mod_studio ) )
		return ENTITY_SHOULD_NOT_COLLIDE;

	// Don't get stuck on point sized entities ( world doesn't count )
	if ( m_nModelIndex != 1 )
	{
		if ( IsPointSized() )
			return ENTITY_SHOULD_NOT_COLLIDE;
	}

	return ENTITY_SHOULD_COLLIDE;
}

//-----------------------------------------------------------------------------
// Is this a brush model?
//-----------------------------------------------------------------------------
bool CBaseEntity::IsBrushModel() const
{
	int modelType = modelinfo->GetModelType( model );
	return (modelType == mod_brush);
}

//-----------------------------------------------------------------------------
// This method works when we've got a studio model
//-----------------------------------------------------------------------------
void CBaseEntity::AddStudioDecal( const Ray_t& ray, int hitbox, int decalIndex, 
								  bool doTrace, trace_t& tr, int maxLODToDecal, int nAdditionalDecalFlags )
{
	if (doTrace)
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );

		// Trace the ray against the entity
		if (tr.fraction == 1.0f)
			return;

		// Set the trace index appropriately...
		tr.m_pEnt = this;
	}

	// Exit out after doing the trace so any other effects that want to happen can happen.
	if ( !r_drawmodeldecals.GetBool() )
		return;

	// Found the point, now lets apply the decals
	CreateModelInstance();

	// FIXME: Pass in decal up?
	Vector up(0, 0, 1);

	if (doTrace && (GetSolid() == SOLID_VPHYSICS) && !tr.startsolid && !tr.allsolid)
	{
		// Choose a more accurate normal direction
		// Also, since we have more accurate info, we can avoid pokethru
		Vector temp;
		VectorSubtract( tr.endpos, tr.plane.normal, temp );
		Ray_t betterRay;
		betterRay.Init( tr.endpos, temp );
		modelrender->AddDecal( m_ModelInstance, betterRay, up, decalIndex, GetStudioBody(), true, maxLODToDecal );
	}
	else
	{
		modelrender->AddDecal( m_ModelInstance, ray, up, decalIndex, GetStudioBody(), false, maxLODToDecal );
	}
}

void CBaseEntity::AddStudioMaterialDecal( const Ray_t& ray, IMaterial *pDecalMaterial, float flInputRadius, Vector vec_up )
{
	
	if ( !r_drawmodeldecals.GetBool() || pDecalMaterial == nullptr )
		return;

	// Found the point, now lets apply the decals
	CreateModelInstance();
	modelrender->AddDecal( m_ModelInstance, ray, -vec_up, -1, GetStudioBody(), false, ADDDECAL_TO_ALL_LODS, pDecalMaterial, flInputRadius, flInputRadius );
}

//-----------------------------------------------------------------------------
// This method works when we've got a brush model
//-----------------------------------------------------------------------------
void CBaseEntity::AddBrushModelDecal( const Ray_t& ray, const Vector& decalCenter, 
									  int decalIndex, bool doTrace, trace_t& tr, const Vector *saxis, int nAdditionalDecalFlags  )
{
	Vector vecNormal;
	if ( doTrace )
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );
		if ( tr.fraction == 1.0f )
			return;
		vecNormal = tr.plane.normal;
	}
	else
	{
		vecNormal = ray.m_Delta;
		VectorNormalize( vecNormal );
		vecNormal *= -1.0f;
	}

	effects->DecalShoot( decalIndex, index, 
		model, GetAbsOrigin(), GetAbsAngles(), decalCenter, saxis, 0, &vecNormal, nAdditionalDecalFlags );
}

//-----------------------------------------------------------------------------
// A method to apply a decal to an entity
//-----------------------------------------------------------------------------
void CBaseEntity::AddDecal( const Vector& rayStart, const Vector& rayEnd,
		const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal, const Vector *saxis, int nAdditionalDecalFlags )
{
	Ray_t ray;
	ray.Init( rayStart, rayEnd );

	// FIXME: Better bloat?
	// Bloat a little bit so we get the intersection
	ray.m_Delta *= 1.1f;

	int modelType = modelinfo->GetModelType( model );
	switch ( modelType )
	{
	case mod_studio:
		AddStudioDecal( ray, hitbox, decalIndex, doTrace, tr, maxLODToDecal, nAdditionalDecalFlags );
		break;

	case mod_brush:
		AddBrushModelDecal( ray, decalCenter, decalIndex, doTrace, tr, saxis, nAdditionalDecalFlags );
		break;

	default:
		// By default, no collision
		tr.fraction = 1.0f;
		break;
	}
}

//-----------------------------------------------------------------------------
// A method to remove all decals from an entity
//-----------------------------------------------------------------------------
void CBaseEntity::RemoveAllDecals( void )
{
	// For now, we only handle removing decals from studiomodels
	if ( modelinfo->GetModelType( model ) == mod_studio )
	{
		CreateModelInstance();
		modelrender->RemoveAllDecals( m_ModelInstance );
	}
}

bool CBaseEntity::SnatchModelInstance( CBaseEntity *pToEntity )
{
	if ( !modelrender->ChangeInstance(  GetModelInstance(), pToEntity ) )
		return false;  // engine could move modle handle

	// remove old handle from toentity if any
	if ( pToEntity->GetModelInstance() != MODEL_INSTANCE_INVALID )
		 pToEntity->DestroyModelInstance();

	// move the handle to other entity
	pToEntity->SetModelInstance(  GetModelInstance() );

	// delete own reference
	SetModelInstance( MODEL_INSTANCE_INVALID );

	return true;
}

//-----------------------------------------------------------------------------
// CBaseEntity new/delete
// All fields in the object are all initialized to 0.
//-----------------------------------------------------------------------------
void *CBaseEntity::operator new( size_t stAllocateBlock )
{
	contract_assert( stAllocateBlock != 0 );	
	void *pMem = MemAlloc_AllocAlignedUnattributed( stAllocateBlock, 16 );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *CBaseEntity::operator new[]( size_t stAllocateBlock )
{
	contract_assert( stAllocateBlock != 0 );				
	void *pMem = MemAlloc_AllocAlignedUnattributed( stAllocateBlock, 16 );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *CBaseEntity::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	contract_assert( stAllocateBlock != 0 );	
	void *pMem = MemAlloc_AllocAlignedFileLine( stAllocateBlock, 16, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *CBaseEntity::operator new[]( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	contract_assert( stAllocateBlock != 0 );				
	void *pMem = MemAlloc_AllocAlignedFileLine( stAllocateBlock, 16, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMem - 
//-----------------------------------------------------------------------------
void CBaseEntity::operator delete( void *pMem )
{

	// get the engine to free the memory
	MemAlloc_FreeAligned( pMem );
}

//========================================================================================
// TEAM HANDLING
//========================================================================================
CTeam *CBaseEntity::GetTeam( void )
{
	return GetGlobalTeam( m_iTeamNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBaseEntity::GetTeamNumber( void ) const
{
	return m_iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBaseEntity::GetPendingTeamNumber( void ) const
{
	return m_iPendingTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseEntity::GetRenderTeamNumber( void )
{
	return GetTeamNumber();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if these entities are both in at least one team together
//-----------------------------------------------------------------------------
bool CBaseEntity::InSameTeam( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return false;

	return ( pEntity->GetTeam() == GetTeam() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity's on the same team as the local player
//-----------------------------------------------------------------------------
bool CBaseEntity::InLocalTeam( void )
{
	return ( GetTeam() == GetLocalTeam() );
}

void CBaseEntity::SetNextClientThink( float nextThinkTime )
{
	contract_assert( GetClientHandle() != INVALID_CLIENTENTITY_HANDLE );
	ClientThinkList()->SetNextClientThink( GetClientHandle(), nextThinkTime );
}

void CBaseEntity::AddToLeafSystem()
{
	AddToLeafSystem( IsRenderingWithViewModels() );
}

void CBaseEntity::AddToLeafSystem( bool bRenderWithViewModels )
{
	m_bRenderWithViewModels = bRenderWithViewModels;
	if( m_hRender == INVALID_CLIENT_RENDER_HANDLE )
	{
		// create new renderer handle
		ClientLeafSystem()->AddRenderable( this, bRenderWithViewModels, ComputeTranslucencyType(), RENDERABLE_MODEL_UNKNOWN_TYPE, ComputeSplitscreenRenderingFlags( this ) );
		ClientLeafSystem()->EnableAlternateSorting( m_hRender, m_bAlternateSorting );
		ClientLeafSystem()->DisableCachedRenderBounds( m_hRender, m_bDisableCachedRenderBounds );
	}
	else
	{
		// handle already exists, just update group & origin
		ClientLeafSystem()->RenderWithViewModels( m_hRender, bRenderWithViewModels );
		// this should already be edge detected, no need to do it per frame
		// ClientLeafSystem()->SetTranslucencyType( m_hRender, ComputeTranslucencyType() );
		ClientLeafSystem()->SetModelType( m_hRender );
		ClientLeafSystem()->DisableCachedRenderBounds( m_hRender, m_bDisableCachedRenderBounds );
	}
	OnFastReflectionRenderingChanged();
	OnDisableShadowDepthRenderingChanged();
	OnDisableCSMRenderingChanged();
	OnShadowDepthRenderingCacheableStateChanged();
}

//-----------------------------------------------------------------------------
// Creates the shadow (if it doesn't already exist) based on shadow cast type
//-----------------------------------------------------------------------------
void CBaseEntity::CreateShadow()
{
	const std::bitset< MAX_SPLITSCREEN_PLAYERS > bvPrevBits = m_ShadowBits;
	m_ShadowBits.reset();

	ShadowType_t typeSeen = SHADOWS_NONE;
	ShadowType_t shadowType[ MAX_SPLITSCREEN_PLAYERS ];
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		shadowType[ hh ]= ShadowCastType();
		if ( shadowType[ hh ] != SHADOWS_NONE )
		{
			m_ShadowBits.set( hh );

			// This check is to make sure that if the shadow gets drawn for an entity that it's always the 
			//  same type for each split screen viewport (or SHADOWS_NONE if not visible in one of the viewports!)
			// For now, we just pick the "best" (highest enum type) shadow
			if ( shadowType[ hh ] > typeSeen  )
			{
				typeSeen = shadowType[ hh ];
			}
		}
	}

	if ( m_ShadowBits.none() || 
		m_ShadowBits != bvPrevBits )
	{
		DestroyShadow();
	}
	
	if ( m_ShadowBits.any() )
	{
		if ( m_ShadowHandle == CLIENTSHADOW_INVALID_HANDLE )
		{
			contract_assert( typeSeen != SHADOWS_NONE );
			int flags = SHADOW_FLAGS_SHADOW;
			if (typeSeen != SHADOWS_SIMPLE)
				flags |= SHADOW_FLAGS_USE_RENDER_TO_TEXTURE;
			if (typeSeen == SHADOWS_RENDER_TO_TEXTURE_DYNAMIC || typeSeen == SHADOWS_RENDER_TO_TEXTURE_DYNAMIC_CUSTOM)
				flags |= SHADOW_FLAGS_ANIMATING_SOURCE;
			if (typeSeen == SHADOWS_RENDER_TO_TEXTURE_DYNAMIC_CUSTOM)
				flags |= SHADOW_FLAGS_ANIMATING_SOURCE | SHADOW_FLAGS_CUSTOM_DRAW;
			m_ShadowHandle = g_pClientShadowMgr->CreateShadow(GetClientHandle(), entindex(), flags, &m_ShadowBits );
		}
		else
		{
			contract_assert( m_ShadowBits == bvPrevBits );
		}
	}
}

//-----------------------------------------------------------------------------
// Removes the shadow
//-----------------------------------------------------------------------------
void CBaseEntity::DestroyShadow()
{
	// NOTE: This will actually cause the shadow type to be recomputed
	// if the entity doesn't immediately go away
	if (m_ShadowHandle != CLIENTSHADOW_INVALID_HANDLE)
	{
		g_pClientShadowMgr->DestroyShadow(m_ShadowHandle);
		m_ShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
}

//-----------------------------------------------------------------------------
// Removes the entity from the leaf system
//-----------------------------------------------------------------------------
void CBaseEntity::RemoveFromLeafSystem()
{
	// Detach from the leaf lists.
	if( m_hRender != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->RemoveRenderable( m_hRender );
		m_hRender = INVALID_CLIENT_RENDER_HANDLE;
	}
	DestroyShadow();
}

//-----------------------------------------------------------------------------
// Purpose: Flags this entity as being inside or outside of this client's PVS
//			on the server.
//			NOTE: this is meaningless for client-side only entities.
// Input  : inside_pvs - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetDormant( bool bDormant )
{
	contract_assert( IsServerEntity() );
	m_bDormant = bDormant;

	// Kill drawing if we became dormant.
	UpdateVisibility();

	ParticleProp()->OwnerSetDormantTo( bDormant );

	OnSetDormant( bDormant );
	cl_entitylist->SetDormant(index, bDormant);
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this entity is dormant. Client/server entities become
//			dormant when they leave the PVS on the server. Client side entities
//			can decide for themselves whether to become dormant.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsDormant( void ) const
{
	if ( IsServerEntity() )
	{
		return m_bDormant;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Tells the entity that it's about to be destroyed due to the client receiving
// an uncompressed update that's caused it to destroy all entities & recreate them.
//-----------------------------------------------------------------------------
void CBaseEntity::SetDestroyedOnRecreateEntities( void )
{
	// Robin: We need to destroy all our particle systems immediately, because 
	// we're about to be recreated, and their owner EHANDLEs will match up to 
	// the new entity, but it won't know anything about them.
	ParticleProp()->StopEmissionAndDestroyImmediately();
}

//-----------------------------------------------------------------------------
// These methods recompute local versions as well as set abs versions
//-----------------------------------------------------------------------------
void CBaseEntity::SetAbsOrigin( const Vector& absOrigin )
{
	// This is necessary to get the other fields of m_rgflCoordinateFrame ok
	CalcAbsolutePosition();

	if ( m_vecAbsOrigin == absOrigin )
		return;

	// All children are invalid, but we are not
	InvalidatePhysicsRecursive( POSITION_CHANGED );
	RemoveEFlags( EFL_DIRTY_ABSTRANSFORM );

	m_vecAbsOrigin = absOrigin;
	MatrixSetColumn( absOrigin, 3, m_rgflCoordinateFrame ); 

	CBaseEntity *pMoveParent = GetMoveParent();

	if ( !pMoveParent )
	{
		m_vecOrigin = absOrigin;
		return;
	}

	// Moveparent case: transform the abs position into local space
	VectorITransform( absOrigin, pMoveParent->EntityToWorldTransform(), (Vector&)m_vecOrigin );
}

void CBaseEntity::SetAbsAngles( const QAngle& absAngles )
{
	// This is necessary to get the other fields of m_rgflCoordinateFrame ok
	CalcAbsolutePosition();

	// FIXME: The normalize caused problems in server code like momentary_rot_button that isn't
	//        handling things like +/-180 degrees properly. This should be revisited.
	//QAngle angleNormalize( AngleNormalize( absAngles.x ), AngleNormalize( absAngles.y ), AngleNormalize( absAngles.z ) );

	if ( m_angAbsRotation == absAngles )
		return;

	InvalidatePhysicsRecursive( ANGLES_CHANGED );
	RemoveEFlags( EFL_DIRTY_ABSTRANSFORM );

	m_angAbsRotation = absAngles;
	AngleMatrix( absAngles, m_rgflCoordinateFrame );
	MatrixSetColumn( m_vecAbsOrigin, 3, m_rgflCoordinateFrame ); 

	CBaseEntity *pMoveParent = GetMoveParent();
	
	if (!pMoveParent)
	{
		m_angRotation = absAngles;
		return;
	}

	// Moveparent case: we're aligned with the move parent
	if ( m_angAbsRotation == pMoveParent->GetAbsAngles() )
	{
		m_angRotation.Init( );
	}
	else
	{
		// Moveparent case: transform the abs transform into local space
		matrix3x4_t worldToParent, localMatrix;
		MatrixInvert( pMoveParent->EntityToWorldTransform(), worldToParent );
		ConcatTransforms( worldToParent, m_rgflCoordinateFrame, localMatrix );
		MatrixAngles( localMatrix, (QAngle &)m_angRotation );
	}
}

void CBaseEntity::SetAbsVelocity( const Vector &vecAbsVelocity )
{
	if ( m_vecAbsVelocity == vecAbsVelocity )
		return;

	// The abs velocity won't be dirty since we're setting it here
	InvalidatePhysicsRecursive( VELOCITY_CHANGED );
	m_iEFlags &= ~EFL_DIRTY_ABSVELOCITY;

	m_vecAbsVelocity = vecAbsVelocity;

	CBaseEntity *pMoveParent = GetMoveParent();

	if (!pMoveParent)
	{
		m_vecVelocity = vecAbsVelocity;
		return;
	}

	// First subtract out the parent's abs velocity to get a relative
	// velocity measured in world space
	Vector relVelocity;
	VectorSubtract( vecAbsVelocity, pMoveParent->GetAbsVelocity(), relVelocity );

	// Transform velocity into parent space
	Vector vNew;
	VectorIRotate( relVelocity, pMoveParent->EntityToWorldTransform(), vNew );
	m_vecVelocity = vNew;
}

/*
void CBaseEntity::SetAbsAngularVelocity( const QAngle &vecAbsAngVelocity )
{
	// The abs velocity won't be dirty since we're setting it here
	InvalidatePhysicsRecursive( EFL_DIRTY_ABSANGVELOCITY );
	m_iEFlags &= ~EFL_DIRTY_ABSANGVELOCITY;

	m_vecAbsAngVelocity = vecAbsAngVelocity;

	CBaseEntity *pMoveParent = GetMoveParent();
	if (!pMoveParent)
	{
		m_vecAngVelocity = vecAbsAngVelocity;
		return;
	}

	// First subtract out the parent's abs velocity to get a relative
	// angular velocity measured in world space
	QAngle relAngVelocity;
	relAngVelocity = vecAbsAngVelocity - pMoveParent->GetAbsAngularVelocity();

	matrix3x4_t entityToWorld;
	AngleMatrix( relAngVelocity, entityToWorld );

	// Moveparent case: transform the abs angular vel into local space
	matrix3x4_t worldToParent, localMatrix;
	MatrixInvert( pMoveParent->EntityToWorldTransform(), worldToParent );
	ConcatTransforms( worldToParent, entityToWorld, localMatrix );
	MatrixAngles( localMatrix, m_vecAngVelocity );
}
*/

// Prevent these for now until hierarchy is properly networked
vec_t CBaseEntity::GetLocalOriginDim( int iDim ) const
{
	return m_vecOrigin[iDim];
}

// Prevent these for now until hierarchy is properly networked
void CBaseEntity::SetLocalOrigin( const Vector& origin )
{
	if (m_vecOrigin != origin)
	{
		InvalidatePhysicsRecursive( POSITION_CHANGED );
		m_vecOrigin = origin;
	}
}

void CBaseEntity::SetLocalOriginDim( int iDim, vec_t flValue )
{
	if (m_vecOrigin[iDim] != flValue)
	{
		InvalidatePhysicsRecursive( POSITION_CHANGED );
		m_vecOrigin.Set( iDim, flValue );
	}
}

// Prevent these for now until hierarchy is properly networked
vec_t CBaseEntity::GetLocalAnglesDim( int iDim ) const
{
	return m_angRotation[iDim];
}

// Prevent these for now until hierarchy is properly networked
void CBaseEntity::SetLocalAngles( const QAngle& angles )
{
	// NOTE: The angle normalize is a little expensive, but we can save
	// a bunch of time in interpolation if we don't have to invalidate everything
	// and sometimes it's off by a normalization amount

	// FIXME: The normalize caused problems in server code like momentary_rot_button that isn't
	//        handling things like +/-180 degrees properly. This should be revisited.
	//QAngle angleNormalize( AngleNormalize( angles.x ), AngleNormalize( angles.y ), AngleNormalize( angles.z ) );

	if (m_angRotation != angles)
	{
		// This will cause the velocities of all children to need recomputation
		InvalidatePhysicsRecursive( ANGLES_CHANGED );
		m_angRotation = angles;
	}
}

void CBaseEntity::SetLocalAnglesDim( int iDim, vec_t flValue )
{
	flValue = AngleNormalize( flValue );
	if (m_angRotation[iDim] != flValue)
	{
		// This will cause the velocities of all children to need recomputation
		InvalidatePhysicsRecursive( ANGLES_CHANGED );
		m_angRotation.Set( iDim, flValue );
	}
}

void CBaseEntity::SetLocalVelocity( const Vector &vecVelocity )
{
	if (m_vecVelocity != vecVelocity)
	{
		InvalidatePhysicsRecursive( VELOCITY_CHANGED );
		m_vecVelocity = vecVelocity; 
	}
}

void CBaseEntity::SetLocalAngularVelocity( const QAngle &vecAngVelocity )
{
	if (m_vecAngVelocity != vecAngVelocity)
	{
//		InvalidatePhysicsRecursive( ANG_VELOCITY_CHANGED );
		m_vecAngVelocity = vecAngVelocity;
	}
}

void CBaseEntity::Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity )
{
	//TODO: Beef this up to work more like the server version.
	contract_assert( GetPredictable() ); //does this even make sense unless we're predicting the teleportation?
	int iEffects = GetEffects();
	if( newPosition )
	{
		SetNetworkOrigin( *newPosition );
		iEffects |= EF_NOINTERP;
	}
	if( newAngles )
	{
		SetNetworkAngles( *newAngles );
		iEffects |= EF_NOINTERP;
	}
	if( newVelocity )
	{
		SetLocalVelocity( *newVelocity );
		iEffects |= EF_NOINTERP;
	}
	SetEffects( iEffects );
}

//-----------------------------------------------------------------------------
// Sets the local position from a transform
//-----------------------------------------------------------------------------
void CBaseEntity::SetLocalTransform( const matrix3x4_t &localTransform )
{
	Vector vecLocalOrigin;
	QAngle vecLocalAngles;
	MatrixGetColumn( localTransform, 3, vecLocalOrigin );
	MatrixAngles( localTransform, vecLocalAngles );
	SetLocalOrigin( vecLocalOrigin );
	SetLocalAngles( vecLocalAngles );
}

//-----------------------------------------------------------------------------
// FIXME: REMOVE!!!
//-----------------------------------------------------------------------------
void CBaseEntity::MoveToAimEnt( )
{
	Vector vecAimEntOrigin;
	QAngle vecAimEntAngles;
	GetAimEntOrigin( GetMoveParent(), &vecAimEntOrigin, &vecAimEntAngles );
	SetAbsOrigin( vecAimEntOrigin );
	SetAbsAngles( vecAimEntAngles );
}

void CBaseEntity::BoneMergeFastCullBloat( Vector &localMins, Vector &localMaxs, const Vector &thisEntityMins, const Vector &thisEntityMaxs ) const
{
	// By default, we bloat the bbox for fastcull ents by the maximum length it could hang out of the parent bbox,
	// it one corner were touching the edge of the parent's box, and the whole diagonal stretched out.
	float flExpand = (thisEntityMaxs - thisEntityMins).Length();

	localMins.x -= flExpand;
	localMins.y -= flExpand;
	localMins.z -= flExpand;

	localMaxs.x += flExpand;
	localMaxs.y += flExpand;
	localMaxs.z += flExpand;
}

matrix3x4_t& CBaseEntity::GetParentToWorldTransform( matrix3x4_t &tempMatrix )
{
	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		contract_assert( false );
		SetIdentityMatrix( tempMatrix );
		return tempMatrix;
	}

	if ( m_iParentAttachment != 0 )
	{
		Vector vOrigin;
		QAngle vAngles;
		if ( pMoveParent->GetAttachment( m_iParentAttachment, vOrigin, vAngles ) )
		{
			AngleMatrix( vAngles, vOrigin, tempMatrix );
			return tempMatrix;
		}
	}
	
	// If we fall through to here, then just use the move parent's abs origin and angles.
	return pMoveParent->EntityToWorldTransform();
}

//-----------------------------------------------------------------------------
// Purpose: Calculates the absolute position of an edict in the world
//			assumes the parent's absolute origin has already been calculated
//-----------------------------------------------------------------------------
void CBaseEntity::CalcAbsolutePosition( )
{
	// There are periods of time where we're gonna have to live with the
	// fact that we're in an indeterminant state and abs queries (which
	// shouldn't be happening at all; I have assertions for those), will
	// just have to accept stale data.
	if (!s_bAbsRecomputationEnabled)
		return;

	// FIXME: Recompute absbox!!!
	if ((m_iEFlags & EFL_DIRTY_ABSTRANSFORM) == 0)
	{
		// quick check to make sure we really don't need an update
		// Assert( m_pMoveParent || m_vecAbsOrigin == GetLocalOrigin() );
		return;
	}

	std::scoped_lock lock( m_CalcAbsolutePositionMutex );

	if ((m_iEFlags & EFL_DIRTY_ABSTRANSFORM) == 0) // need second check in event another thread grabbed mutex and did the calculation
	{
		return;
	}

	RemoveEFlags( EFL_DIRTY_ABSTRANSFORM );

	if (!m_pMoveParent)
	{
		// Construct the entity-to-world matrix
		// Start with making an entity-to-parent matrix
		AngleMatrix( GetLocalAngles(), GetLocalOrigin(), m_rgflCoordinateFrame );
		m_vecAbsOrigin = GetLocalOrigin();
		m_angAbsRotation = GetLocalAngles();
		NormalizeAngles( m_angAbsRotation );
		return;
	}
	
	if ( IsEffectActive(EF_BONEMERGE) )
	{
		MoveToAimEnt();
		return;
	}

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	matrix3x4_t matEntityToParent [[gnu::aligned(16)]];
	AngleMatrix( GetLocalAngles(), matEntityToParent );
	MatrixSetColumn( GetLocalOrigin(), 3, matEntityToParent );

	// concatenate with our parent's transform
	m_pMoveParent->CalcAbsolutePosition();
	matrix3x4_t scratchMatrix [[gnu::aligned(16)]];
	ConcatTransforms( GetParentToWorldTransform( scratchMatrix ), matEntityToParent, m_rgflCoordinateFrame );

	// pull our absolute position out of the matrix
	MatrixGetColumn( m_rgflCoordinateFrame, 3, m_vecAbsOrigin );

	// if we have any angles, we have to extract our absolute angles from our matrix
	if ( m_angRotation == vec3_angle && m_iParentAttachment == 0 )
	{
		// just copy our parent's absolute angles
		VectorCopy( m_pMoveParent->GetAbsAngles(), m_angAbsRotation );
	}
	else
	{
		MatrixAngles( m_rgflCoordinateFrame, m_angAbsRotation );
	}

	// This is necessary because it's possible that our moveparent's CalculateIKLocks will trigger its move children 
	// (ie: this entity) to call GetAbsOrigin(), and they'll use the moveparent's OLD bone transforms to get their attachments
	// since the moveparent is right in the middle of setting up new transforms. 
	//
	// So here, we keep our absorigin invalidated. It means we're returning an origin that is a frame old to CalculateIKLocks,
	// but we'll still render with the right origin.
	if ( m_iParentAttachment != 0 && (m_pMoveParent->GetFlags() & EFL_SETTING_UP_BONES) )
	{
		m_iEFlags |= EFL_DIRTY_ABSTRANSFORM;
	}
}

void CBaseEntity::CalcAbsoluteVelocity()
{
	if ((m_iEFlags & EFL_DIRTY_ABSVELOCITY ) == 0)
		return;

	std::scoped_lock lock( m_CalcAbsoluteVelocityMutex );

	if ((m_iEFlags & EFL_DIRTY_ABSVELOCITY) == 0) // need second check in event another thread grabbed mutex and did the calculation
	{
		return;
	}

	m_iEFlags &= ~EFL_DIRTY_ABSVELOCITY;

	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		m_vecAbsVelocity = m_vecVelocity;
		return;
	}

	VectorRotate( m_vecVelocity, pMoveParent->EntityToWorldTransform(), m_vecAbsVelocity );

	// Add in the attachments velocity if it exists
	if ( m_iParentAttachment != 0 )
	{
		Vector vOriginVel;
		Quaternion vAngleVel;
		if ( pMoveParent->GetAttachmentVelocity( m_iParentAttachment, vOriginVel, vAngleVel ) )
		{
			m_vecAbsVelocity += vOriginVel;
			return;
		}
	}

	// Now add in the parent abs velocity
	m_vecAbsVelocity += pMoveParent->GetAbsVelocity();
}

/*
void CBaseEntity::CalcAbsoluteAngularVelocity()
{
	if ((m_iEFlags & EFL_DIRTY_ABSANGVELOCITY ) == 0)
		return;

	m_iEFlags &= ~EFL_DIRTY_ABSANGVELOCITY;

	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		m_vecAbsAngVelocity = m_vecAngVelocity;
		return;
	}

	matrix3x4_t angVelToParent, angVelToWorld;
	AngleMatrix( m_vecAngVelocity, angVelToParent );
	ConcatTransforms( pMoveParent->EntityToWorldTransform(), angVelToParent, angVelToWorld );
	MatrixAngles( angVelToWorld, m_vecAbsAngVelocity );

	// Now add in the parent abs angular velocity
	m_vecAbsAngVelocity += pMoveParent->GetAbsAngularVelocity();
}
*/

//-----------------------------------------------------------------------------
// Computes the abs position of a point specified in local space
//-----------------------------------------------------------------------------
void CBaseEntity::ComputeAbsPosition( const Vector &vecLocalPosition, Vector *pAbsPosition )
{
	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		*pAbsPosition = vecLocalPosition;
	}
	else
	{
		VectorTransform( vecLocalPosition, pMoveParent->EntityToWorldTransform(), *pAbsPosition );
	}
}

//-----------------------------------------------------------------------------
// Computes the abs position of a point specified in local space
//-----------------------------------------------------------------------------
void CBaseEntity::ComputeAbsDirection( const Vector &vecLocalDirection, Vector *pAbsDirection )
{
	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		*pAbsDirection = vecLocalDirection;
	}
	else
	{
		VectorRotate( vecLocalDirection, pMoveParent->EntityToWorldTransform(), *pAbsDirection );
	}
}

//-----------------------------------------------------------------------------
// Mark shadow as dirty 
//-----------------------------------------------------------------------------
void CBaseEntity::MarkRenderHandleDirty( )
{
	// Invalidate render leaf too
	ClientRenderHandle_t handle = GetRenderHandle();
	if ( handle != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->RenderableChanged( handle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::ShutdownPredictable( void )
{
	contract_assert( GetPredictable() );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		GetPredictables( i )->RemoveFromPredictablesList( this );
	}
	DestroyIntermediateData();
	SetPredictable( false );
}

//-----------------------------------------------------------------------------
// Purpose: Turn entity into something the predicts locally
//-----------------------------------------------------------------------------
void CBaseEntity::InitPredictable( CBasePlayer *pOwner )
{
	contract_assert( !GetPredictable() );
	contract_assert( pOwner );

	// Mark as predictable
	SetPredictable( true );

	int slot = CBasePlayer::GetSplitScreenSlotForPlayer( pOwner );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );
	m_nSplitUserPlayerPredictionSlot = slot;
	// Allocate buffers into which we copy data
	AllocateIntermediateData();
	// Add to list of predictables
	GetPredictables( slot )->AddToPredictableList( this );
	// Copy everything from "this" into the original_state_data
	//  object.  Don't care about client local stuff, so pull from slot 0 which

	//  should be empty anyway...
	PostNetworkDataReceived( 0 );

	// Copy original data into all prediction slots, so we don't get an error saying we "mispredicted" any
	//  values which are still at their initial values
	for ( int i = 0; i < MULTIPLAYER_BACKUP; i++ )
	{
		SaveData( "InitPredictable", i, PC_EVERYTHING );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetPredictable( bool state )
{
	m_bPredictable = state;

	// update interpolation times
	Interp_UpdateInterpolationAmounts( GetVarMapping() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::GetPredictable( void ) const
{
	return m_bPredictable;
}

//-----------------------------------------------------------------------------
// Purpose: Transfer data for intermediate frame to current entity
// Input  : copyintermediate - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void CBaseEntity::PreEntityPacketReceived( int commands_acknowledged )					
{				
	// Don't need to copy intermediate data if server did ack any new commands
	bool copyintermediate = ( commands_acknowledged > 0 ) ? true : false;

	contract_assert( GetPredictable() );
	contract_assert( cl_predict->GetInt() );

	// First copy in any intermediate predicted data for non-networked fields
	if ( copyintermediate )
	{
		RestoreData( "PreEntityPacketReceived", commands_acknowledged - 1, PC_NON_NETWORKED_ONLY );
		RestoreData( "PreEntityPacketReceived", SLOT_ORIGINALDATA, PC_NETWORKED_ONLY );
	}
	else
	{
		RestoreData( "PreEntityPacketReceived(no commands ack)", SLOT_ORIGINALDATA, PC_EVERYTHING );
	}

	// At this point the entity has original network data restored as of the last time the 
	// networking was updated, and it has any intermediate predicted values properly copied over
	// Unpacked and OnDataChanged will fill in any changed, networked fields.

	// That networked data will be copied forward into the starting slot for the next prediction round

}	

//-----------------------------------------------------------------------------
// Purpose: Called every time PreEntityPacket received is called
//  copy any networked data into original_state
// Input  : errorcheck - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void CBaseEntity::PostEntityPacketReceived( void )
{
	contract_assert( GetPredictable() );
	contract_assert( cl_predict->GetInt() );

	// Save networked fields into "original data" store
	SaveData( "PostEntityPacketReceived", SLOT_ORIGINALDATA, PC_NETWORKED_ONLY );
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame after all updating is done
// Input  : errorcheck - 
//			last_predicted - 
//-----------------------------------------------------------------------------
bool CBaseEntity::PostNetworkDataReceived( int commands_acknowledged )
{
	bool haderrors = false;
	contract_assert( GetPredictable() );

	bool errorcheck = ( commands_acknowledged > 0 ) ? true : false;

	// Store network data into post networking pristine state slot (slot 64) 
	SaveData( "PostNetworkDataReceived", SLOT_ORIGINALDATA, PC_EVERYTHING );

	// Show any networked fields that are different
	bool showthis = cl_showerror.GetInt() >= 2;

	if ( cl_showerror.GetInt() < 0 )
	{
		if ( entindex() == -cl_showerror.GetInt() )
		{
			showthis = true;
		}
		else
		{
			showthis = false;
		}
	}

	if ( errorcheck )
	{
		byte *predicted_state_data = (byte *)GetPredictedFrame( commands_acknowledged - 1 );	
		contract_assert( predicted_state_data );												
		const byte *original_state_data =  (const byte *)GetOriginalNetworkDataObject();
		contract_assert( original_state_data );

		CPredictionCopy errorCheckHelper( PC_NETWORKED_ONLY, 
			predicted_state_data, TD_OFFSET_PACKED, 
			original_state_data, TD_OFFSET_PACKED, 
			showthis ? 
				CPredictionCopy::TRANSFERDATA_ERRORCHECK_SPEW : 
				CPredictionCopy::TRANSFERDATA_ERRORCHECK_NOSPEW );
		haderrors = errorCheckHelper.TransferData( "", entindex(), GetPredDescMap() ) > 0 ? true : false;
	}
	return haderrors;
}

void CBaseEntity::HandlePredictionError( bool bErrorInThisEntity )
{

}

// Stuff implemented for weapon prediction code
void CBaseEntity::SetSize( const Vector &vecMin, const Vector &vecMax )
{
	SetCollisionBounds( vecMin, vecMax );
}

//-----------------------------------------------------------------------------
// Purpose: Just look up index
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CBaseEntity::PrecacheModel( const char *name )
{
	return modelinfo->GetModelIndex( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *obj - 
//-----------------------------------------------------------------------------
void CBaseEntity::Remove( )
{
	if ( IsMarkedForDeletion( ) )
		return;
	AddEFlags( EFL_KILLME );	// Make sure to ignore further calls into here or UTIL_Remove.

	// Nothing for now, if it's a predicted entity, could flag as "delete" or dormant
	if ( GetPredictable() || IsClientCreated() )
	{
		// Make it solid
		AddSolidFlags( FSOLID_NOT_SOLID );
		SetMoveType( MOVETYPE_NONE );
	}

	if ( !s_bImmediateRemovesAllowed )
	{
		AddToEntityList( ENTITY_LIST_DELETE );
		return;
	}

	Release();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::GetPredictionEligible( void ) const
{
	return m_bPredictionEligible;
}

CBaseEntity* CBaseEntity::Instance( CBaseHandle hEnt )
{
	return ClientEntityList().GetBaseEntityFromHandle( hEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iEnt - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CBaseEntity::Instance( int iEnt )
{
	return ClientEntityList().GetBaseEntity( iEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CBaseEntity::GetClassname( void )
{
	static char outstr[ 256 ];
	outstr[ 0 ] = 0;
	bool gotname = false;
	ClientClass *pClientClass = GetClientClass();
	if ( pClientClass && pClientClass->m_pMapClassname )
		return pClientClass->m_pMapClassname;

	if ( GetPredDescMap() )
	{
		const char *mapname =  GetClassMap().Lookup( GetPredDescMap()->dataClassName );
		if ( mapname && mapname[ 0 ] ) 
		{
			V_strncpy( outstr, mapname, sizeof( outstr ) );
			gotname = true;
		}
	}

	if ( !gotname )
	{
		V_strncpy( outstr, typeid( *this ).name(), sizeof( outstr ) );
	}

	return outstr;
}

const char *CBaseEntity::GetDebugName( void )
{
	return GetClassname();
}

//-----------------------------------------------------------------------------
// Purpose: Creates an entity by string name, but does not spawn it
// Input  : *className - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CreateEntityByName( const char *className )
{
	CBaseEntity *ent = GetClassMap().CreateEntity( className );
	if ( ent )
	{
		return ent;
	}

	Warning( "Can't find factory for entity: %s\n", className );
	return nullptr;
}

static void dlight_debug( const CCommand &args );
static ConCommand dlight_debug_command( side_registry(), "dlight_debug", dlight_debug, "Creates a dlight in front of the player", FCVAR_CHEAT );
static void dlight_debug( const CCommand &args )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

    //lwss: added 3 more dlights to test 4x(MAX) on 1 surface
    dlight_t *el = effects->CL_AllocDlight( 1 );
    dlight_t *el2 = effects->CL_AllocDlight( 2 );
    dlight_t *el3 = effects->CL_AllocDlight( 3 );
    dlight_t *el4 = effects->CL_AllocDlight( 4 );

	Vector start = player->EyePosition();
	Vector forward;
	player->EyeVectors( &forward );
	// sloppy but works
    Vector forward2 = forward;
    forward2.x += 0.10f;
    forward2.NormalizeInPlace();
    Vector forward3 = forward;
    forward3.x -= 0.10f;
    forward2.NormalizeInPlace();
    Vector forward4 = forward;
    forward4.y += 0.10f;
    forward4.NormalizeInPlace();

    Vector end = start + forward * MAX_TRACE_LENGTH;
	Vector end2 = start + forward2 * MAX_TRACE_LENGTH;
	Vector end3 = start + forward3 * MAX_TRACE_LENGTH;
	Vector end4 = start + forward4 * MAX_TRACE_LENGTH;

	trace_t tr;
	trace_t tr2;
	trace_t tr3;
	trace_t tr4;
	UTIL_TraceLine( start, end, MASK_SHOT_HULL & (~CONTENTS_GRATE), player, COLLISION_GROUP_NONE, &tr );
	UTIL_TraceLine( start, end2, MASK_SHOT_HULL & (~CONTENTS_GRATE), player, COLLISION_GROUP_NONE, &tr2 );
	UTIL_TraceLine( start, end3, MASK_SHOT_HULL & (~CONTENTS_GRATE), player, COLLISION_GROUP_NONE, &tr3 );
	UTIL_TraceLine( start, end4, MASK_SHOT_HULL & (~CONTENTS_GRATE), player, COLLISION_GROUP_NONE, &tr4 );

	el->origin = tr.endpos - forward * 12.0f;
	el->radius = 200; 
	el->decay = el->radius / 5.0f;
	el->die = gpGlobals->curtime + 5.0f;
	el->color.r = 255;
	el->color.g = 192;
	el->color.b = 64;
	el->color.exponent = 5;

    el2->origin = tr2.endpos - forward2 * 12.0f;
    el2->radius = 200;
    el2->decay = el2->radius / 5.0f;
    el2->die = gpGlobals->curtime + 5.0f;
    el2->color.r = 55;
    el2->color.g = 50;
    el2->color.b = 255;
    el2->color.exponent = 5;

    el3->origin = tr3.endpos - forward3 * 12.0f;
    el3->radius = 200;
    el3->decay = el3->radius / 5.0f;
    el3->die = gpGlobals->curtime + 5.0f;
    el3->color.r = 5;
    el3->color.g = 220;
    el3->color.b = 10;
    el3->color.exponent = 5;

    el4->origin = tr4.endpos - forward4 * 12.0f;
    el4->radius = 200;
    el4->decay = el4->radius / 5.0f;
    el4->die = gpGlobals->curtime + 5.0f;
    el4->color.r = 225;
    el4->color.g = 22;
    el4->color.b = 22;
    el4->color.exponent = 5;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsClientCreated( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
//			*module - 
//			line - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CBaseEntity::CreatePredictedEntityByName( const char *classname, const char *module, int line, bool persist /*= false */ )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Called each packet that the entity is created on and finally gets called after the next packet
//  that doesn't have a create message for the "parent" entity so that the predicted version
//  can be removed.  Return true to delete entity right away.
//-----------------------------------------------------------------------------
bool CBaseEntity::OnPredictedEntityRemove( bool isbeingremoved, CBaseEntity *predicted )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetOwnerEntity( CBaseEntity *pOwner )
{
	m_hOwnerEntity = pOwner;
}

//-----------------------------------------------------------------------------
// Purpose: Put the entity in the specified team
//-----------------------------------------------------------------------------
void CBaseEntity::ChangeTeam( int iTeamNum )
{
	m_iTeamNum = iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : name - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetModelName( string_t name )
{
	m_ModelName = name;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : string_t
//-----------------------------------------------------------------------------
string_t CBaseEntity::GetModelName( void ) const
{
	return m_ModelName;
}

//-----------------------------------------------------------------------------
// Purpose: Nothing yet, could eventually supercede Term()
//-----------------------------------------------------------------------------
void CBaseEntity::UpdateOnRemove( void )
{
	VPhysicsDestroyObject(); 

	contract_assert( !GetMoveParent() );
	UnlinkFromHierarchy();
	SetGroundEntity( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : canpredict - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetPredictionEligible( bool canpredict )
{
	m_bPredictionEligible = canpredict;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a value that scales all damage done by this entity.
//-----------------------------------------------------------------------------
float CBaseEntity::GetAttackDamageScale( void )
{
	float flScale = 1;
// Not hooked up to prediction yet
	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsDormantPredictable( void ) const
{
	return m_bDormantPredictable;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dormant - 
//-----------------------------------------------------------------------------
void CBaseEntity::SetDormantPredictable( bool dormant )
{
	contract_assert( IsClientCreated() );

	m_bDormantPredictable = true;
	m_nIncomingPacketEntityBecameDormant = prediction->GetIncomingPacketNumber();

// Do we need to do the following kinds of things?
}

//-----------------------------------------------------------------------------
// Purpose: Used to determine when a dorman client predictable can be safely deleted
//  Note that it can be deleted earlier than this by OnPredictedEntityRemove returning true
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::BecameDormantThisPacket( void ) const
{
	contract_assert( IsDormantPredictable() );

	if ( m_nIncomingPacketEntityBecameDormant != prediction->GetIncomingPacketNumber() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsIntermediateDataAllocated( void ) const
{
	return m_pOriginalData != nullptr ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::AllocateIntermediateData( void )
{				
	if ( m_pOriginalData )
		return;
	size_t allocsize = GetIntermediateDataSize();
	contract_assert( allocsize > 0 );

	m_pOriginalData = new unsigned char[ allocsize ];
	V_memset( m_pOriginalData, 0, allocsize );
	for ( int i = 0; i < std::size( m_pIntermediateData ); i++ )
	{
		m_pIntermediateData[ i ] = new unsigned char[ allocsize ];
		V_memset( m_pIntermediateData[ i ], 0, allocsize );
	}

	if( !physenv || physenv->IsPredicted() ) //either predicted physics or don't know if we're predicting physics
	{
		for ( int i = 0; i < std::size( m_pIntermediateData_FirstPredicted ); i++ )
		{
			m_pIntermediateData_FirstPredicted[i] = new unsigned char[ allocsize ];
			V_memset( m_pIntermediateData_FirstPredicted[ i ], 0, allocsize );
		}

		m_nIntermediateData_FirstPredictedShiftMarker = -1;
	}

	m_nIntermediateDataCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::DestroyIntermediateData( void )
{
	if ( !m_pOriginalData )
		return;
	for ( int i = 0; i < std::size( m_pIntermediateData ); i++ )
	{
		delete[] m_pIntermediateData[ i ];
		m_pIntermediateData[ i ] = nullptr;
	}

	if( m_pIntermediateData_FirstPredicted[0] != nullptr )
	{
		for ( int i = 0; i < std::size( m_pIntermediateData_FirstPredicted ); i++ )
		{
			delete[] m_pIntermediateData_FirstPredicted[ i ];
			m_pIntermediateData_FirstPredicted[ i ] = nullptr;
		}		
	}

	delete[] m_pOriginalData;
	m_pOriginalData = nullptr;

	m_nIntermediateDataCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove - 
//			number_of_commands_run - 
//-----------------------------------------------------------------------------
void CBaseEntity::ShiftIntermediateDataForward( int slots_to_remove, int number_of_commands_run )
{
	contract_assert( m_pIntermediateData );
	if ( !m_pIntermediateData )
		return;

	contract_assert( number_of_commands_run >= slots_to_remove );

	// Just moving pointers, yeah
	byte *saved[ std::size( m_pIntermediateData ) ];

	// Remember first slots
	int i = 0;
	for ( ; i < slots_to_remove; i++ )
	{
		saved[ i ] = m_pIntermediateData[ i ];
	}

	// Move rest of slots forward up to last slot
	for ( ; i < number_of_commands_run; i++ )
	{
		m_pIntermediateData[ i - slots_to_remove ] = m_pIntermediateData[ i ];
	}

	// Put remembered slots onto end
	for ( i = 0; i < slots_to_remove; i++ )
	{
		int slot = number_of_commands_run - slots_to_remove + i;

		m_pIntermediateData[ slot ] = saved[ i ];
	}

	m_nIntermediateDataCount -= slots_to_remove;
	if( m_nIntermediateDataCount < -1 )
	{
		m_nIntermediateDataCount = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove -  
//-----------------------------------------------------------------------------
void CBaseEntity::ShiftFirstPredictedIntermediateDataForward( int slots_to_remove )
{
	if ( !m_pIntermediateData_FirstPredicted[0] || m_nIntermediateData_FirstPredictedShiftMarker == -1 )
		return;

	if( m_nIntermediateData_FirstPredictedShiftMarker <= slots_to_remove ) //acknowledged more commands than we predicted, early out
	{
		m_nIntermediateData_FirstPredictedShiftMarker = 0;
		return;
	}

	// Just moving pointers, yeah
	byte *saved_FirstPredicted[ std::size( m_pIntermediateData_FirstPredicted ) ];

	// Remember first slots
	int i = 0;
	for ( ; i < slots_to_remove; i++ )
	{
		saved_FirstPredicted[ i ] = m_pIntermediateData_FirstPredicted[ i ];
	}

	// Move rest of slots forward up to last slot
	for ( ; i <= m_nIntermediateData_FirstPredictedShiftMarker; i++ )
	{
		m_pIntermediateData_FirstPredicted[ i - slots_to_remove ] = m_pIntermediateData_FirstPredicted[ i ];
	}

	int iEndBase = (m_nIntermediateData_FirstPredictedShiftMarker + 1) - slots_to_remove;

	contract_assert( iEndBase >= 0 );

	// Put remembered slots onto end
	for ( i = 0; i < slots_to_remove; i++ )
	{
		m_pIntermediateData_FirstPredicted[ iEndBase + i ] = saved_FirstPredicted[ i ];
	}

	m_nIntermediateData_FirstPredictedShiftMarker -= slots_to_remove;
	
}

//-----------------------------------------------------------------------------
// Purpose: For a predicted entity that is physically simulated, compensate for the prediction frames being coupled to player commands
//			by shifting them around if there's an unequal number of ticks and player commands executed on the server
// Input  : delta - number of server ticks elapsed minus the number of player commands acknowledged
//			last_slot - the number of valid frames currently stored in m_pIntermediateData
//-----------------------------------------------------------------------------
void CBaseEntity::ShiftIntermediateData_TickAdjust( int delta, int last_slot )
{
	contract_assert( m_pIntermediateData );
	if ( !m_pIntermediateData || (last_slot == 0) )
		return;

	//Warning( "CBaseEntity::ShiftIntermediateData_TickAdjust( %f ) delta: %i  last: %i\n", gpGlobals->curtime, delta, last_slot );

	if( delta > last_slot )
	{
		delta = last_slot;
	}
	else if( delta < -last_slot )
	{
		return; //acknowledged more commands than we predicted. Won't be restoring frames anyway
	}

	//Warning( "\t" );

	size_t allocsize = GetIntermediateDataSize();

	byte *saved[ std::size( m_pIntermediateData ) ];
	memcpy( saved, m_pIntermediateData, last_slot * sizeof( byte * ) );

	if( delta < 0 ) //more commands acknowledged than ticks run, slots indices should increment in value by negative delta
	{
		int i = 0;
		int iStop = last_slot + delta;
		for( ; i < iStop; ++i )
		{
			//Warning( "%i<-%i,", i - delta, i );
			m_pIntermediateData[i - delta] = saved[i];
		}

		for( ; i < last_slot; ++i )
		{
			//Warning( "%i<-%i,", i - iStop, i );
			m_pIntermediateData[i - iStop] = saved[i];
			memcpy( m_pIntermediateData[i - iStop], saved[0], allocsize ); //make duplicates of the first frame we have available
		}
	}
	else //more ticks run than commands acknowledged, slot indices should decrement by delta
	{
		int i = 0;
		int iStop = last_slot - delta;
		for( ; i < iStop; ++i )
		{
			//Warning( "%i<-%i,", i, i + delta );
			m_pIntermediateData[i] = saved[i + delta];
		}

		for( ; i < last_slot; ++i )
		{
			//Warning( "%i<-%i,", i, i - iStop );
			m_pIntermediateData[i] = saved[i - iStop];
			memcpy( m_pIntermediateData[i], saved[last_slot - 1], allocsize ); //make duplicates of the last frame we have available
		}
	}

	//Warning( "\n" );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : framenumber - 
//-----------------------------------------------------------------------------
void *CBaseEntity::GetPredictedFrame( int framenumber )
{
	contract_assert( framenumber >= 0 );

	if ( !m_pOriginalData )
	{
		contract_assert( 0 );
		return nullptr;
	}
	return (void *)m_pIntermediateData[ framenumber % std::size( m_pIntermediateData ) ];
}

void *CBaseEntity::GetFirstPredictedFrame( int framenumber )
{
	contract_assert( framenumber >= 0 );
	contract_assert( m_pIntermediateData_FirstPredicted[0] != 0 );

	return (void *)m_pIntermediateData_FirstPredicted[ framenumber % std::size( m_pIntermediateData_FirstPredicted ) ];
}

//-----------------------------------------------------------------------------
// Purpose: Get the range of predicted frames we may restore from in prediction
//-----------------------------------------------------------------------------
void CBaseEntity::GetUnacknowledgedPredictedFrameRange( int &iStart, int &iEnd )
{
	iStart = std::max( s_nIncomingPacketCommandsAcknowledged, 0 );
	iEnd = m_nIntermediateDataCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void *CBaseEntity::GetOriginalNetworkDataObject( void )
{
	if ( !m_pOriginalData )
	{
		contract_assert( 0 );
		return nullptr;
	}
	return (void *)m_pOriginalData;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::ComputePackedOffsets( void )
{
	datamap_t *map = GetPredDescMap();
	if ( !map || map->m_pOptimizedDataMap )
		return;
	CPredictionCopy::PrepareDataMap( map );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBaseEntity::GetIntermediateDataSize( void )
{
	ComputePackedOffsets();

	const datamap_t *map = GetPredDescMap();

	contract_assert( map->m_pOptimizedDataMap );

	int size = map->m_nPackedSize;

	contract_assert( size > 0 );	

	// At least 4 bytes to avoid some really bad stuff
	return std::max( size, 4 );
}	

// Convenient way to delay removing oneself
void CBaseEntity::SUB_Remove( void )
{
	if (m_iHealth > 0)
	{
		// this situation can screw up NPCs who can't tell their entity pointers are invalid.
		m_iHealth = 0;
		DevWarning( 2, "SUB_Remove called on entity with health > 0\n");
	}

	Remove( );
}

CBaseEntity *FindEntityInFrontOfLocalPlayer()
{

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		// Get the entity under my crosshair
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,	MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 && TraceDidHitNonWorldEntity( tr ) )
		{
			return tr.Ent<CBaseEntity>();
		}
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Debug command to wipe the decals off an entity
//-----------------------------------------------------------------------------
static void RemoveDecals_f( void )
{
	CBaseEntity *pHit = FindEntityInFrontOfLocalPlayer();
	if ( pHit )
	{
		pHit->RemoveAllDecals();
	}
}

static ConCommand cl_removedecals( side_registry(), "cl_removedecals", RemoveDecals_f, "Remove the decals from the entity under the crosshair.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::ClearBBoxVisualization( void )
{
	m_fBBoxVisFlags = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::ToggleBBoxVisualization( int fVisFlags )
{
	if ( m_fBBoxVisFlags & fVisFlags )
	{
		m_fBBoxVisFlags &= ~fVisFlags;
	}
	else
	{
		m_fBBoxVisFlags |= fVisFlags;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void ToggleBBoxVisualization( int fVisFlags, const CCommand &args )
{
	CBaseEntity *pHit;

	int iEntity = -1;
	if ( args.ArgC() >= 2 )
	{
		iEntity = atoi( args[ 1 ] );
	}

	if ( iEntity == -1 )
	{
		pHit = FindEntityInFrontOfLocalPlayer();
	}
	else
	{
		pHit = cl_entitylist->GetBaseEntity( iEntity );
	}

	if ( pHit )
	{
		pHit->ToggleBBoxVisualization( fVisFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
static void cl_ent_bbox( const CCommand &args );
static ConCommand cl_ent_bbox_command( side_registry(), "cl_ent_bbox", cl_ent_bbox, "Displays the client's bounding box for the entity under the crosshair.", FCVAR_CHEAT );
static void cl_ent_bbox( const CCommand &args )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_COLLISION_BOUNDS, args );
}

//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
static void cl_ent_absbox( const CCommand &args );
static ConCommand cl_ent_absbox_command( side_registry(), "cl_ent_absbox", cl_ent_absbox, "Displays the client's absbox for the entity under the crosshair.", FCVAR_CHEAT );
static void cl_ent_absbox( const CCommand &args )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_SURROUNDING_BOUNDS, args );
}

//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
static void cl_ent_rbox( const CCommand &args );
static ConCommand cl_ent_rbox_command( side_registry(), "cl_ent_rbox", cl_ent_rbox, "Displays the client's render box for the entity under the crosshair.", FCVAR_CHEAT );
static void cl_ent_rbox( const CCommand &args )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_RENDER_BOUNDS, args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseEntity::DrawBBoxVisualizations( void )
{
	if ( m_fBBoxVisFlags & VISUALIZE_COLLISION_BOUNDS )
	{
		debugoverlay->AddBoxOverlay( CollisionProp()->GetCollisionOrigin(), CollisionProp()->OBBMins(),
			CollisionProp()->OBBMaxs(), CollisionProp()->GetCollisionAngles(), 190, 190, 0, 0, 0.01 );
	}

	if ( m_fBBoxVisFlags & VISUALIZE_SURROUNDING_BOUNDS )
	{
		Vector vecSurroundMins, vecSurroundMaxs;
		CollisionProp()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
		debugoverlay->AddBoxOverlay( vec3_origin, vecSurroundMins,
			vecSurroundMaxs, vec3_angle, 0, 255, 255, 0, 0.01 );
	}

	if ( m_fBBoxVisFlags & VISUALIZE_RENDER_BOUNDS || r_drawrenderboxes.GetInt() > 0 )
	{
		Vector vecRenderMins, vecRenderMaxs;

		if ( (r_drawrenderboxes.GetInt() & 0x1) > 0 )
		{
			GetRenderBounds( vecRenderMins, vecRenderMaxs );

			debugoverlay->AddBoxOverlay( GetRenderOrigin(), vecRenderMins, vecRenderMaxs,
				GetRenderAngles(), 255, 0, 255, 0, 0.01 );
		}

		if ( (r_drawrenderboxes.GetInt() & 0x2) > 0 )
		{
			GetRenderBoundsWorldspace( vecRenderMins, vecRenderMaxs );

			// Now draw the AABB 
			debugoverlay->AddBoxOverlay( vec3_origin, vecRenderMins, vecRenderMaxs,
				vec3_angle, 0, 255, 0, 0, 0.01 );
		}
	}
}

//-----------------------------------------------------------------------------
// Sets the render mode
//-----------------------------------------------------------------------------
void CBaseEntity::SetRenderMode( RenderMode_t nRenderMode, bool bForceUpdate )
{
	if ( nRenderMode != m_nRenderMode )
	{
		m_nRenderMode = nRenderMode;
		m_pClientAlphaProperty->SetRenderFX( GetRenderFX(), nRenderMode );
	}
}

void CBaseEntity::SetRenderFX( RenderFx_t nRenderFX, float flStartTime, float flDuration )
{
	bool bStartTimeUnspecified = ( flStartTime == std::numeric_limits<float>::max() );
	if ( nRenderFX != m_nRenderFX || !bStartTimeUnspecified )
	{
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		m_nRenderFX = nRenderFX;
		m_pClientAlphaProperty->SetRenderFX( nRenderFX, GetRenderMode(), flStartTime, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy from this entity into one of the save slots (original or intermediate)
// Input  : slot - 
//			type - 
//			false - 
//			false - 
//			true - 
//			false - 
//			NULL - 
// Output : int
//-----------------------------------------------------------------------------
void CBaseEntity::SaveData( const char *context, int slot, int type )
{
	CVProfScope _( "CBaseEntity::SaveData", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	void *dest = ( slot == SLOT_ORIGINALDATA ) ? GetOriginalNetworkDataObject() : GetPredictedFrame( slot );
	contract_assert( dest );
	if ( slot != SLOT_ORIGINALDATA )
	{
		// Remember high water mark so that we can detect below if we are reading from a slot not yet predicted into...
		m_nIntermediateDataCount = slot;
	}

	CPredictionCopy copyHelper( type, (byte *)dest, TD_OFFSET_PACKED, (const byte *)this, TD_OFFSET_NORMAL, CPredictionCopy::TRANSFERDATA_COPYONLY );
	copyHelper.TransferData( "CBaseEntity::SaveData", entindex(), GetPredDescMap() );
}

//-----------------------------------------------------------------------------
// Purpose: Restore data from specified slot into current entity
// Input  : slot - 
//			type - 
//			false - 
//			false - 
//			true - 
//			false - 
//			NULL - 
// Output : int
//-----------------------------------------------------------------------------
void CBaseEntity::RestoreData( const char *context, int slot, int type )
{
	CVProfScope _( "CBaseEntity::RestoreData", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	const void *src = ( slot == SLOT_ORIGINALDATA ) ? GetOriginalNetworkDataObject() : GetPredictedFrame( slot );
	contract_assert( src );
	
	// some flags shouldn't be predicted - as we find them, add them to the savedEFlagsMask
	const int savedEFlagsMask = EFL_DIRTY_SHADOWUPDATE | EFL_DIRTY_SPATIAL_PARTITION;
	int savedEFlags = GetEFlags() & savedEFlagsMask;

	CPredictionCopy copyHelper( type, (byte *)this, TD_OFFSET_NORMAL, (const byte *)src, TD_OFFSET_PACKED, CPredictionCopy::TRANSFERDATA_COPYONLY  );
	copyHelper.TransferData( "CBaseEntity::RestoreData", entindex(), GetPredDescMap() );

	// set non-predicting flags back to their prior state
	RemoveEFlags( savedEFlagsMask );
	AddEFlags( savedEFlags );
}

void CBaseEntity::OnPostRestoreData()
{
	// HACK Force recomputation of origin
	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );

	if ( GetMoveParent() )
	{
		AddToAimEntsList();
	}

	// If our model index has changed, then make sure it's reflected in our model pointer.
	if ( GetModel() != modelinfo->GetModel( GetModelIndex() ) )
	{
		MDLCACHE_CRITICAL_SECTION();
		SetModelByIndex( GetModelIndex() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine approximate velocity based on updates from server
// Input  : vel - 
//-----------------------------------------------------------------------------
void CBaseEntity::EstimateAbsVelocity( Vector& vel )
{
	if ( CBasePlayer::IsLocalPlayer( this ) )
	{
		vel = GetAbsVelocity();
		return;
	}

	CInterpolationContext context;
	context.EnableExtrapolation( true );
	m_iv_vecOrigin.GetDerivative_SmoothVelocity( &vel, gpGlobals->curtime, !IsPlayer() ); //disable Hermite interpolation fix for the velocity estimation for players. Fixes bugbait #82165. Limiting to players only to reduce risk of regressions
}

void CBaseEntity::Interp_Reset( VarMapping_t *map )
{
	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "reset" );
	int c = std::ssize( map->m_Entries );
	for ( int i = 0; i < c; i++ )
	{
		VarMapEntry_t *e = &map->m_Entries[ i ];
		IInterpolatedVar *watcher = e->watcher;

		watcher->Reset( gpGlobals->curtime );
	}
}

void CBaseEntity::ResetLatched()
{
	if ( IsClientCreated() )
		return;

	Interp_Reset( GetVarMapping() );
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, this needs a better solution
// Input  : flags - 
// Output : float
//-----------------------------------------------------------------------------

static float AdjustInterpolationAmount( CBaseEntity *pEntity, float baseInterpolation )
{
	if ( cl_interp_npcs.GetFloat() > 0 )
	{
		const float minNPCInterpolationTime = cl_interp_npcs.GetFloat();
		const float minNPCInterpolation = TICK_INTERVAL * ( TIME_TO_TICKS( minNPCInterpolationTime ) + 1 );

		if ( minNPCInterpolation > baseInterpolation )
		{
			while ( pEntity )
			{
				if ( pEntity->IsNPC() )
					return minNPCInterpolation;

				pEntity = pEntity->GetMoveParent();
			}
		}
	}

	return baseInterpolation;
}

//-------------------------------------
float CBaseEntity::GetInterpolationAmount( int flags )
{
	// If single player server is "skipping ticks" everything needs to interpolate for a bit longer
	int serverTickMultiple = 1;
	if ( IsSimulatingOnAlternateTicks() )
	{
		serverTickMultiple = 2;
	}

	if ( GetPredictable() || IsClientCreated() )
	{
		return TICK_INTERVAL * serverTickMultiple;
	}

	// Always fully interpolate during multi-player or during demo playback...
	if ( ( gpGlobals->maxClients > 1 && gpGlobals->IsRemoteClient() ) || engine->IsPlayingDemo() )
	{
		int numTicksToInterpolate = TIME_TO_TICKS( GetClientInterpAmount() ) + serverTickMultiple;
		return AdjustInterpolationAmount( this, TICKS_TO_TIME( numTicksToInterpolate ) );
	}

	int expandedServerTickMultiple = serverTickMultiple;

	if ( IsAnimatedEveryTick() && IsSimulatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}

	if ( ( flags & LATCH_ANIMATION_VAR ) && IsAnimatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}
	if ( ( flags & LATCH_SIMULATION_VAR ) && IsSimulatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}

	return AdjustInterpolationAmount( this, TICK_INTERVAL * ( TIME_TO_TICKS( GetClientInterpAmount() ) +  serverTickMultiple ) );
}

float CBaseEntity::GetLastChangeTime( int flags )
{
	if ( GetPredictable() || IsClientCreated() )
	{
		return gpGlobals->curtime;
	}
	
	// make sure not both flags are set, we can't resolve that
	contract_assert( !( (flags & LATCH_ANIMATION_VAR) && (flags & LATCH_SIMULATION_VAR) ) );
	
	if ( flags & LATCH_ANIMATION_VAR )
	{
		return GetAnimTime();
	}

	if ( flags & LATCH_SIMULATION_VAR )
	{
		float st = GetSimulationTime();
		if ( st == 0.0f )
		{
			return gpGlobals->curtime;
		}
		return st;
	}

	contract_assert( 0 );

	return gpGlobals->curtime;
}

const Vector& CBaseEntity::GetPrevLocalOrigin() const
{
	return m_iv_vecOrigin.GetPrev();
}

const QAngle& CBaseEntity::GetPrevLocalAngles() const
{
	return m_iv_angRotation.GetPrev();
}

//-----------------------------------------------------------------------------
// Simply here for game shared 
//-----------------------------------------------------------------------------
bool CBaseEntity::IsFloating()
{
	// NOTE: This is only here because it's called by game shared.
	// The server uses it to lower falling impact damage
	return false;
}

IMPLEMENT_REFLECT_DATAMAP_NO_BASE( CBaseEntity )

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::ShouldSavePhysics()
{
	return false;
}

//-----------------------------------------------------------------------------
// handler to do stuff before you are saved
//-----------------------------------------------------------------------------
void CBaseEntity::OnSave()
{
	// Here, we must force recomputation of all abs data so it gets saved correctly
	// We can't leave the dirty bits set because the loader can't cope with it.
	CalcAbsolutePosition();
	CalcAbsoluteVelocity();
}

//-----------------------------------------------------------------------------
// handler to do stuff after you are restored
//-----------------------------------------------------------------------------
void CBaseEntity::OnRestore()
{
	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );
	
	UpdatePartitionListEntry();
	CollisionProp()->UpdatePartition();

	UpdateVisibility();
}

void CBaseEntity::SetClassname( const char *className )
{
	m_iClassname = MAKE_STRING( className );
}

//-----------------------------------------------------------------------------
// capabilities
//-----------------------------------------------------------------------------
int CBaseEntity::ObjectCaps( void ) 
{
	return 0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : C_AI_BaseNPC


//-----------------------------------------------------------------------------
// Purpose: For each client (only can be local client in client .dll ) checks the client has disabled CC and if so, removes them from 
//  the recipient list.
// Input  : filter - 
//-----------------------------------------------------------------------------
ConVar closecaption( side_registry(), "closecaption", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Enable close captioning." );

void CBaseEntity::RemoveRecipientsIfNotCloseCaptioning( CRecipientFilter& filter )
{
	if ( !closecaption.GetBool() )
	{
		filter.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : recording - 
// Output : inline void
//-----------------------------------------------------------------------------
void CBaseEntity::EnableInToolView( bool bEnable )
{
	m_bEnabledInToolView = bEnable;
	UpdateVisibility();
}

void CBaseEntity::SetToolRecording( bool recording )
{
	m_bToolRecording = recording;
	if ( m_bToolRecording )
	{
		recordinglist->AddToList( GetClientHandle() );
		OnToolStartRecording();
	}
	else
	{
        recordinglist->RemoveFromList( GetClientHandle() );
	}
}

bool CBaseEntity::HasRecordedThisFrame() const
{
	contract_assert( m_nLastRecordedFrame <= gpGlobals->framecount );
	return m_nLastRecordedFrame == gpGlobals->framecount;
}

void CBaseEntity::GetToolRecordingState( KeyValues *msg )
{
	contract_assert( ToolsEnabled() );
	if ( !ToolsEnabled() )
		return;

	CVProfScope _( "CBaseEntity::GetToolRecordingState", 0, VPROF_BUDGETGROUP_TOOLS, false, BUDGETFLAG_OTHER );

	CBaseEntity *pOwner = m_hOwnerEntity;

	static BaseEntityRecordingState_t state;

	state.m_flTime = gpGlobals->curtime;
	state.m_pModelName = modelinfo->GetModelName( GetModel() );
	state.m_nOwner = pOwner ? pOwner->entindex() : -1;
	state.m_fEffects = m_fEffects;
	state.m_bVisible = ShouldDraw() && !IsDormant();
	state.m_bRecordFinalVisibleSample = false;
	state.m_vecRenderOrigin = GetRenderOrigin();
	state.m_vecRenderAngles = GetRenderAngles();
	state.m_numEffects = 0;
	state.m_pEffects = nullptr;

	// use EF_NOINTERP if the owner or a hierarchical parent has NO_INTERP
	if ( pOwner && pOwner->IsEffectActive( EF_NOINTERP ) )
	{
		state.m_fEffects |= EF_NOINTERP;
	}
	CBaseEntity *pParent = GetMoveParent();
	while ( pParent )
	{
		if ( pParent->IsEffectActive( EF_NOINTERP ) )
		{
			state.m_fEffects |= EF_NOINTERP;
			break;
		}

		pParent = pParent->GetMoveParent();
	}

	msg->SetPtr( "baseentity", &state );
}

void CBaseEntity::CleanupToolRecordingState( KeyValues *msg )
{
}

void CBaseEntity::RecordToolMessage()
{
	contract_assert( IsToolRecording() );
	if ( !IsToolRecording() )
		return;

	if ( HasRecordedThisFrame() )
		return;

	KeyValues *msg = new KeyValues( "entity_state" );

	// Post a message back to all IToolSystems
	GetToolRecordingState( msg );
	contract_assert( (int)GetToolHandle() != 0 );
	ToolFramework_PostToolMessage( GetToolHandle(), msg );
	CleanupToolRecordingState( msg );

	msg->deleteThis();

	m_nLastRecordedFrame = gpGlobals->framecount;
}

bool CBaseEntity::IsSpottedBy( int nPlayerIndex ) 
{ 
	contract_assert( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS );
	if ( nPlayerIndex >= 0 && nPlayerIndex <= MAX_PLAYERS )
	{
		int maskBitIndex = nPlayerIndex;
		int maskIndex = maskBitIndex >> 5;
		return ( m_bSpottedByMask[ maskIndex ] & ( 1u << ( maskBitIndex & 31 ) ) ) != 0;
	}
	return false;
}

bool CBaseEntity::IsSpottedByFriends( int nPlayerIndex )
{
	int nPlayerEntIndex = nPlayerIndex + 1;

	CBasePlayer* pThisPlayer = UTIL_PlayerByIndex( nPlayerEntIndex );
	if ( !pThisPlayer )
		return false;

	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && 
			 pPlayer != pThisPlayer && 
			 pThisPlayer->GetTeamNumber() == pPlayer->GetTeamNumber() && 
			 IsSpottedBy( i ) )
		{
			return true;
		}
	}

	return false;
}

void CBaseEntity::SetIsSpottedBy( int nPlayerIndex )
{
	if ( engine->IsPlayingDemo() && nPlayerIndex == 64 ) // old demo artifact
	{
		return;
	}

	contract_assert( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS );
	if ( nPlayerIndex >= 0 && nPlayerIndex < MAX_PLAYERS )
	{
		int maskBitIndex = nPlayerIndex;
		int maskIndex = maskBitIndex >> 5;
		m_bSpottedByMask.Set( maskIndex, m_bSpottedByMask.Get( maskIndex ) | ( 1u << ( maskBitIndex & 31 ) ) );
	}
}

void CBaseEntity::AddToEntityList( entity_list_ids_t listId )
{
	contract_assert( listId < NUM_ENTITY_LISTS );
	if ( m_ListEntry[listId] == 0xFFFF )
	{
		m_ListEntry[listId] = g_EntityLists[listId].emplace_back( this );
	}
}

void CBaseEntity::RemoveFromEntityList( entity_list_ids_t listId )
{
	contract_assert( listId < NUM_ENTITY_LISTS );
	if ( m_ListEntry[listId] != 0xFFFF )
	{
		g_EntityLists[listId].erase( m_ListEntry[listId] );
		m_ListEntry[listId] = 0xFFFF;
	}
}

void CBaseEntity::AddVar( void *data, IInterpolatedVar *watcher, int type, bool bSetup )
{
	// Only add it if it hasn't been added yet.
	bool bAddIt = true;
	for ( int i=0; i < std::ssize( m_VarMap.m_Entries ); i++ )
	{
		if ( m_VarMap.m_Entries[i].watcher == watcher )
		{
			if ( (type & EXCLUDE_AUTO_INTERPOLATE) != (watcher->GetType() & EXCLUDE_AUTO_INTERPOLATE) )
			{
				// Its interpolation mode changed, so get rid of it and re-add it.
				RemoveVar( m_VarMap.m_Entries[i].data, true );
			}
			else
			{
				// They're adding something that's already there. No need to re-add it.
				bAddIt = false;
			}
			
			break;	
		}
	}
	
	if ( bAddIt )
	{
		// watchers must have a debug name set
		contract_assert( watcher->GetDebugName() != nullptr );

		VarMapEntry_t map;
		map.data = data;
		map.watcher = watcher;
		map.type = type;
		map.m_bNeedsToInterpolate = true;
		if ( type & EXCLUDE_AUTO_INTERPOLATE )
		{
			m_VarMap.m_Entries.push_back( map );
		}
		else
		{
			m_VarMap.m_Entries.insert( m_VarMap.m_Entries.begin(), map );
			++m_VarMap.m_nInterpolatedEntries;
		}
	}

	if ( bSetup )
	{
		watcher->Setup( data, type );
		watcher->SetInterpolationAmount( GetInterpolationAmount( watcher->GetType() ) );
	}
}

void CBaseEntity::RemoveVar( void *data, bool bAssert )
{
	for ( int i=0; i < std::ssize( m_VarMap.m_Entries ); i++ )
	{
		if ( m_VarMap.m_Entries[i].data == data )
		{
			if ( !( m_VarMap.m_Entries[i].type & EXCLUDE_AUTO_INTERPOLATE ) )
				--m_VarMap.m_nInterpolatedEntries;

			m_VarMap.m_Entries.erase( m_VarMap.m_Entries.begin() + i );
			return;
		}
	}
	if ( bAssert )
	{
		contract_assert( !"RemoveVar" );
	}
}

void CBaseEntity::CheckCLInterpChanged()
{
	float flCurValue_Interp = GetClientInterpAmount();
	static float flLastValue_Interp = flCurValue_Interp;

	float flCurValue_InterpNPCs = cl_interp_npcs.GetFloat();
	static float flLastValue_InterpNPCs = flCurValue_InterpNPCs;
	
	if ( flLastValue_Interp != flCurValue_Interp || 
		 flLastValue_InterpNPCs != flCurValue_InterpNPCs  )
	{
		flLastValue_Interp = flCurValue_Interp;
		flLastValue_InterpNPCs = flCurValue_InterpNPCs;
	
		// Tell all the existing entities to update their interpolation amounts to account for the change.
		CBaseEntityIterator iterator;
		CBaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != nullptr )
		{
			pEnt->Interp_UpdateInterpolationAmounts( pEnt->GetVarMapping() );
		}
	}
}

void CBaseEntity::DontRecordInTools()
{
	m_bRecordInTools = false;
}

int CBaseEntity::GetCreationTick() const
{
	return m_nCreationTick;
}

static CCallQueue s_SimulateEntitiesCallQueue;

CCallQueue *CBaseEntity::GetSimulateCallQueue( void )
{
	return &s_SimulateEntitiesCallQueue;
}

// static method
void CBaseEntity::SimulateEntities()
{
	s_bImmediateRemovesAllowed = false;

	if ( !report_cliententitysim.GetBool() )
	{
		unsigned short iNext;
		for ( unsigned short iCur = g_EntityLists[ENTITY_LIST_SIMULATE].head(); iCur != g_EntityLists[ENTITY_LIST_SIMULATE].invalid; iCur = iNext )
		{
			iNext = g_EntityLists[ENTITY_LIST_SIMULATE].next( iCur );
			CBaseEntity *pCur = g_EntityLists[ENTITY_LIST_SIMULATE][iCur];
			if ( pCur->IsEFlagSet( EFL_KILLME ) )
				continue;

			bool bRemove = !pCur->Simulate();
			if ( bRemove )
			{
				pCur->RemoveFromEntityList(ENTITY_LIST_SIMULATE);
			}
		}
	}
	else
	{
		CFastTimer fastTimer;
	
		unsigned short iNext;
		for ( unsigned short iCur = g_EntityLists[ENTITY_LIST_SIMULATE].head(); iCur != g_EntityLists[ENTITY_LIST_SIMULATE].invalid; iCur = iNext )
		{
			iNext = g_EntityLists[ENTITY_LIST_SIMULATE].next( iCur );
			CBaseEntity *pCur = g_EntityLists[ENTITY_LIST_SIMULATE][iCur];
			if ( pCur->IsEFlagSet( EFL_KILLME ) )
				continue;

			fastTimer.Start();
			bool bRemove = !pCur->Simulate();
			if ( bRemove )
			{
				pCur->RemoveFromEntityList(ENTITY_LIST_SIMULATE);
			}
			fastTimer.End();
			Msg( "Entity(%d): %s - %f\n", pCur->entindex(), pCur->GetDebugName(), fastTimer.GetDuration().GetMillisecondsF() );
		}

		// Report only once per turn on.
		report_cliententitysim.SetValue( 0 );
	}

	s_SimulateEntitiesCallQueue.CallQueued();

	s_bImmediateRemovesAllowed = true;
	PurgeRemovedEntities();
}

// static method
void CBaseEntity::PurgeRemovedEntities()
{
	unsigned short iNext;
	for ( unsigned short iCur = g_EntityLists[ENTITY_LIST_DELETE].head(); iCur != g_EntityLists[ENTITY_LIST_DELETE].invalid; iCur = iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_DELETE].next( iCur );
		CBaseEntity *pCur = g_EntityLists[ENTITY_LIST_DELETE][iCur];
		pCur->Release();
	}
	g_EntityLists[ENTITY_LIST_DELETE].clear();
}

// static method
// This is the per-viewport setup hook
void CBaseEntity::PreRenderEntities( int nSplitScreenPlayerSlot )
{
	MDLCACHE_CRITICAL_SECTION();
	unsigned short iNext;
	for ( unsigned short iCur = g_EntityLists[ENTITY_LIST_PRERENDER].head(); iCur != g_EntityLists[ENTITY_LIST_PRERENDER].invalid; iCur = iNext )
	{
		iNext = g_EntityLists[ENTITY_LIST_PRERENDER].next( iCur );
		CBaseEntity *pCur = g_EntityLists[ENTITY_LIST_PRERENDER][iCur];

		bool bRemove = !pCur->PreRender(nSplitScreenPlayerSlot);
		if ( bRemove )
		{
			pCur->RemoveFromEntityList(ENTITY_LIST_PRERENDER);
		}
	}

}

bool CBaseEntity::PreRender( int nSplitScreenPlayerSlot )
{
	bool bNeedsPrerender = false;

	// Create flashlight effects, etc.
	if ( CreateLightEffects() )
	{
		bNeedsPrerender = true;
	}
	return bNeedsPrerender;
}

bool CBaseEntity::IsViewEntity() const
{
	return render->IsViewEntity( entindex() );
}

bool CBaseEntity::IsAbleToHaveFireEffect( void ) const
{
	return !UTIL_IsLowViolence();
}

void CBaseEntity::SetBlurState( bool bShouldBlur )
{
	if( bShouldBlur != m_bIsBlurred )
	{
		m_bIsBlurred = bShouldBlur;
		OnTranslucencyTypeChanged();
	}
}

bool CBaseEntity::IsBlurred( void )
{
	return m_bIsBlurred;
}

void CBaseEntity::OnParseMapDataFinished()
{
}

//-----------------------------------------------------------------------------
// Adjust the number of cell bits
//-----------------------------------------------------------------------------
bool CBaseEntity::SetCellBits( int cellbits )
{
	if ( m_cellbits == cellbits )
		return false;

	m_cellbits = cellbits;
	m_cellwidth = ( 1 << cellbits );
	return true;
}

bool CBaseEntity::ShouldRegenerateOriginFromCellBits() const
{
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::GetScriptInstance()
{
	ks::ScratchArena<> scratch;
	if ( !m_hScriptInstance )
	{
		if ( m_iszScriptId == NULL_STRING )
		{
			char *szName = (char *)scratch.AllocBytes( 1024 );
			g_pScriptVM->GenerateUniqueKey( ( m_iName != NULL_STRING ) ? STRING(GetEntityName()) : GetClassname(), szName, 1024 );
			m_iszScriptId = AllocPooledString( szName );
		}

		m_hScriptInstance = g_pScriptVM->RegisterInstance( GetScriptDesc(), this );
		g_pScriptVM->SetInstanceUniqeId( m_hScriptInstance, STRING(m_iszScriptId) );
	}
	return m_hScriptInstance;
}

bool CBaseEntity::IsAutoaimTarget( void ) const
{
	return m_bIsAutoaimTarget;
}

//------------------------------------------------------------------------------
void CC_CL_Find_Ent( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: cl_find_ent <substring>\n" );
		return;
	}

	int iCount = 0;
	const char *pszSubString = args[1];
	Msg("Searching for client entities with classname containing substring: '%s'\n", pszSubString );

	CBaseEntity *ent = nullptr;
	while ( (ent = ClientEntityList().NextBaseEntity(ent)) != nullptr )
	{
		const char *pszClassname = ent->GetClassname();

		bool bMatches = false;
		if ( pszClassname && pszClassname[0] )
		{
			if ( V_stristr( pszClassname, pszSubString ) )
			{
				bMatches = true;
			}
		}

		if ( bMatches )
		{
			iCount++;
			Msg("   '%s' (entindex %d) %s \n", pszClassname ? pszClassname : "[NO NAME]", ent->entindex(), ent->IsDormant() ? "(DORMANT)" : "" );
		}
	}

	Msg("Found %d matches.\n", iCount);
}
static ConCommand cl_find_ent(side_registry(), "cl_find_ent", CC_CL_Find_Ent, "Find and list all client entities with classnames that contain the specified substring.\nFormat: cl_find_ent <substring>\n", FCVAR_CHEAT);

//------------------------------------------------------------------------------
void CC_CL_Find_Ent_Index( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: cl_find_ent_index <index>\n" );
		return;
	}

	int iIndex = atoi(args[1]);
	CBaseEntity *ent = ClientEntityList().GetBaseEntity( iIndex );
	if ( ent )
	{
		const char *pszClassname;
		pszClassname = ent->GetClassname();
		Msg("   '%s' (entindex %d) %s \n", pszClassname ? pszClassname : "[NO NAME]", iIndex, ent->IsDormant() ? "(DORMANT)" : "" );
	}
	else
	{
		Msg("Found no entity at %d.\n", iIndex);
	}
}
static ConCommand cl_find_ent_index(side_registry(), "cl_find_ent_index", CC_CL_Find_Ent_Index, "Display data for clientside entity matching specified index.\nFormat: cl_find_ent_index <index>\n", FCVAR_CHEAT);

//-----------------------------------------------------------------------------
// An inline version the game code can use
//-----------------------------------------------------------------------------
CCollisionProperty *CBaseEntity::CollisionProp()
{
	return m_Collision.Get();
}

const CCollisionProperty *CBaseEntity::CollisionProp() const
{
	return m_Collision.Get();
}

CClientAlphaProperty *CBaseEntity::AlphaProp()
{
	return m_pClientAlphaProperty;
}

const CClientAlphaProperty *CBaseEntity::AlphaProp() const
{
	return m_pClientAlphaProperty;
}

//-----------------------------------------------------------------------------
// An inline version the game code can use
//-----------------------------------------------------------------------------
CParticleProperty *CBaseEntity::ParticleProp()
{
	return m_Particles.Get();
}

const CParticleProperty *CBaseEntity::ParticleProp() const
{
	return m_Particles.Get();
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
matrix3x4_t &CBaseEntity::EntityToWorldTransform()
{ 
	contract_assert( s_bAbsQueriesValid );
	CalcAbsolutePosition();
	return m_rgflCoordinateFrame; 
}

const matrix3x4_t &CBaseEntity::EntityToWorldTransform() const
{
	contract_assert( s_bAbsQueriesValid );
	const_cast<CBaseEntity*>(this)->CalcAbsolutePosition();
	return m_rgflCoordinateFrame; 
}

//-----------------------------------------------------------------------------
// Some helper methods that transform a point from entity space to world space + back
//-----------------------------------------------------------------------------
void CBaseEntity::EntityToWorldSpace( const Vector &in, Vector *pOut ) const
{
	if ( GetAbsAngles() == vec3_angle )
	{
		VectorAdd( in, GetAbsOrigin(), *pOut );
	}
	else
	{
		VectorTransform( in, EntityToWorldTransform(), *pOut );
	}
}

void CBaseEntity::WorldToEntitySpace( const Vector &in, Vector *pOut ) const
{
	if ( GetAbsAngles() == vec3_angle )
	{
		VectorSubtract( in, GetAbsOrigin(), *pOut );
	}
	else
	{
		VectorITransform( in, EntityToWorldTransform(), *pOut );
	}
}

const Vector &CBaseEntity::GetAbsVelocity( ) const
{
	contract_assert( s_bAbsQueriesValid );
	const_cast<CBaseEntity*>(this)->CalcAbsoluteVelocity();
	return m_vecAbsVelocity;
}

CBaseEntity	*CBaseEntity::Instance( IClientEntity *ent )
{
	return ent ? EntityCast<CBaseEntity>( ent->GetBaseEntity() ) : nullptr;
}

// For debugging shared code
bool	CBaseEntity::IsServer( void )
{
	return false;
}

bool	CBaseEntity::IsClient( void )
{
	return true;
}

const char *CBaseEntity::GetDLLType( void )
{
	return "client";
}

//-----------------------------------------------------------------------------
// Methods relating to solid type + flags
//-----------------------------------------------------------------------------
void CBaseEntity::SetSolidFlags( int nFlags )
{
	CollisionProp()->SetSolidFlags( nFlags );
}

bool CBaseEntity::IsSolidFlagSet( int flagMask ) const
{
	return CollisionProp()->IsSolidFlagSet( flagMask );
}

int	CBaseEntity::GetSolidFlags( void ) const
{
	return CollisionProp()->GetSolidFlags( );
}

void CBaseEntity::AddSolidFlags( int nFlags )
{
	CollisionProp()->AddSolidFlags( nFlags );
}

void CBaseEntity::RemoveSolidFlags( int nFlags )
{
	CollisionProp()->RemoveSolidFlags( nFlags );
}

bool CBaseEntity::IsSolid() const
{
	return CollisionProp()->IsSolid( );
}

void CBaseEntity::SetSolid( SolidType_t val )
{
	CollisionProp()->SetSolid( val );
}

SolidType_t CBaseEntity::GetSolid( ) const
{
	return CollisionProp()->GetSolid( );
}

void CBaseEntity::SetCollisionBounds( const Vector& mins, const Vector &maxs )
{
	CollisionProp()->SetCollisionBounds( mins, maxs );
}

//-----------------------------------------------------------------------------
// Methods relating to bounds
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::WorldAlignMins( ) const
{
	contract_assert( !CollisionProp()->IsBoundsDefinedInEntitySpace() );
	contract_assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
	return CollisionProp()->OBBMins();
}

const Vector& CBaseEntity::WorldAlignMaxs( ) const
{
	contract_assert( !CollisionProp()->IsBoundsDefinedInEntitySpace() );
	contract_assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
	return CollisionProp()->OBBMaxs();
}

const Vector& CBaseEntity::WorldAlignSize( ) const
{
	contract_assert( !CollisionProp()->IsBoundsDefinedInEntitySpace() );
	contract_assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
	return CollisionProp()->OBBSize();
}

float CBaseEntity::BoundingRadius() const
{
	return CollisionProp()->BoundingRadius();
}

bool CBaseEntity::IsPointSized() const
{
	return CollisionProp()->BoundingRadius() == 0.0f;
}

//-----------------------------------------------------------------------------
// Methods relating to traversing hierarchy
//-----------------------------------------------------------------------------
CBaseEntity *CBaseEntity::GetMoveParent( void ) const
{
	return m_pMoveParent; 
}

CBaseEntity *CBaseEntity::FirstMoveChild( void ) const
{
	return m_pMoveChild; 
}

CBaseEntity *CBaseEntity::NextMovePeer( void ) const
{
	return m_pMovePeer; 
}

//-----------------------------------------------------------------------------
// Velocity
//-----------------------------------------------------------------------------
void CBaseEntity::SetFriction( float flFriction ) 
{ 
	m_flFriction = flFriction;
}

float CBaseEntity::GetElasticity( void )	const			
{ 
	return m_flElasticity; 
}

const color24 CBaseEntity::GetRenderColor() const
{
	color24 c = { m_clrRender->r, m_clrRender->g, m_clrRender->b }; 
	return c;
}

byte CBaseEntity::GetRenderColorR() const
{
	return m_clrRender->r;
}

byte CBaseEntity::GetRenderColorG() const
{
	return m_clrRender->g;
}

byte CBaseEntity::GetRenderColorB() const
{
	return m_clrRender->b;
}

void CBaseEntity::SetRenderColor( byte r, byte g, byte b )
{
	m_clrRender.SetR( r );
	m_clrRender.SetG( g );
	m_clrRender.SetB( b );
}

void CBaseEntity::SetRenderColorR( byte r )
{
	m_clrRender.SetR( r );
}

void CBaseEntity::SetRenderColorG( byte g )
{
	m_clrRender.SetG( g );
}

void CBaseEntity::SetRenderColorB( byte b )
{
	m_clrRender.SetB( b );
}

RenderMode_t CBaseEntity::GetRenderMode() const
{
	return (RenderMode_t)m_nRenderMode;
}

RenderFx_t CBaseEntity::GetRenderFX() const
{
	return (RenderFx_t)m_nRenderFX;
}

void	CBaseEntity::SetCPULevels( int nMinCPULevel, int nMaxCPULevel )
{
	m_nMinCPULevel = nMinCPULevel;
	m_nMaxCPULevel = nMaxCPULevel;
}

void CBaseEntity::SetGPULevels( int nMinGPULevel, int nMaxGPULevel )
{
	m_nMinGPULevel = nMinGPULevel;
	m_nMaxGPULevel = nMaxGPULevel;
}

int CBaseEntity::GetMinCPULevel( ) const
{
	return m_nMinCPULevel;
}

int CBaseEntity::GetMaxCPULevel( ) const
{
	return m_nMaxCPULevel;
}

int CBaseEntity::GetMinGPULevel( ) const
{
	return m_nMinGPULevel;
}

int CBaseEntity::GetMaxGPULevel( ) const
{
	return m_nMaxGPULevel;
}

void CBaseEntity::AddEFlags( int nEFlagMask )
{
	m_iEFlags |= nEFlagMask;
}

void CBaseEntity::RemoveEFlags( int nEFlagMask )
{
	m_iEFlags &= ~nEFlagMask;
}

unsigned char CBaseEntity::GetParentAttachment() const
{
	return m_iParentAttachment;
}

ClientRenderHandle_t CBaseEntity::GetRenderHandle() const 
{ 
	return m_hRender; 
}

ClientRenderHandle_t& CBaseEntity::RenderHandle()
{
	return m_hRender;
}

//-----------------------------------------------------------------------------
// Methods to cast away const
//-----------------------------------------------------------------------------
Vector CBaseEntity::EyePosition( void ) const
{
	return const_cast<CBaseEntity*>(this)->EyePosition();
}

const QAngle &CBaseEntity::EyeAngles( void ) const		// Direction of eyes in world space
{
	return const_cast<CBaseEntity*>(this)->EyeAngles();
}

const QAngle &CBaseEntity::LocalEyeAngles( void ) const	// Direction of eyes
{
	return const_cast<CBaseEntity*>(this)->LocalEyeAngles();
}

Vector	CBaseEntity::EarPosition( void ) const			// position of ears
{
	return const_cast<CBaseEntity*>(this)->EarPosition();
}

VarMapping_t* CBaseEntity::GetVarMapping()
{
	return &m_VarMap;
}

//-----------------------------------------------------------------------------
// Should we be interpolating?
//-----------------------------------------------------------------------------
bool	CBaseEntity::IsInterpolationEnabled()
{
	return s_bInterpolate;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : inline void
//-----------------------------------------------------------------------------
void CBaseEntity::SetToolHandle( HTOOLHANDLE handle )
{
	m_ToolHandle = handle;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : inline HTOOLHANDLE
//-----------------------------------------------------------------------------
HTOOLHANDLE CBaseEntity::GetToolHandle() const
{
	return m_ToolHandle;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CBaseEntity::IsEnabledInToolView() const
{
	return m_bEnabledInToolView;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : inline bool
//-----------------------------------------------------------------------------
bool CBaseEntity::ShouldRecordInTools() const
{
	return m_bRecordInTools;
}

bool CBaseEntity::HasSpawnFlags( int nFlags ) const
{ 
	return (m_spawnflags & nFlags) != 0; 
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
const char *CBaseEntity::GetEntityName() 
{ 
	return m_iName; 
}

void CBaseEntity::Think( void)
{
	contract_assert( ( m_pfnThink != &CBaseEntity::Think ) && "Infinite recursion is infinitely bad." );

	if ( m_pfnThink )
	{
		( this->*m_pfnThink )();
	}
}

CAbsQueryScopeGuard::CAbsQueryScopeGuard( bool state)
{
	m_bSavedState = CBaseEntity::IsAbsQueriesValid();
	CBaseEntity::SetAbsQueriesValid( state );
}
