// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <time.h>
#include <math.h>
#include "datamap.h"
#include "networkvar.h"
#include "dt_send.h"
#include "eiface.h"
#include "iclientunknown.h"
#include "reflect.h"
#include "vphysics_interface.h"
#include "worldsize.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "dt_recv.h"
#include "reflect_annotations.h"
#include "predictable_entity.h"
#include "igameevents.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "shareddefs.h"
#include "util.h"
#include "declare_class.h"
module ks.server;

import std;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import ks.game.shareddefs;
import ks.game.mapentities_shared;
import :nav_4;
import ks.core.basetypes;
import ks.core.numeric;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.tslist;
import ks.math.aabb;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vmatrix;
import :entity_2;
import ks.math.transform;
import ks.tier1.interfaces;
import :entity_dispatch;
import :entity_kind;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.core.dbg;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.iconvar;
import ks.tier1.vector_ops;
import ks.game.ai_activity;
import ks.core.types;
import ks.math.vector2d;
import ks.tier1.exprevaluator;
import ks.core.vprof;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;

class CBaseEntity;

using namespace ::ks::reflect;

							   // that's the whole point of this class.

coerciblevariant_t::~coerciblevariant_t()
{
	if (szVal)
		delete[] szVal;
}

// Note: the constructor and SetXXX() functions are
// almost identical, except that the Sets() have a Void()
// step, because they may need to wipe out a string that's 
// been allocated. 

coerciblevariant_t::coerciblevariant_t( bool b ) : szVal(nullptr)
{
	// do the bool, int, and float conversions right now, set the flags
	bVal = b;
	iVal = b ? 1 : 0;
	flVal = b ? 1.0f : 0.0f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_BOOLEAN;
}

void coerciblevariant_t::SetBool( bool b )
{
	Void();

	// do the bool, int, and float conversions right now, set the flags
	bVal = b;
	iVal = b ? 1 : 0;
	flVal = b ? 1.0f : 0.0f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_BOOLEAN;
}

coerciblevariant_t::coerciblevariant_t( int i ) : szVal(nullptr)
{
	// do the bool, int, and float conversions right now, set the flags
	bVal = i;
	iVal = i;
	flVal = i;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_INTEGER;
}

void coerciblevariant_t::SetInt( int i )
{
	Void();

	// do the bool, int, and float conversions right now, set the flags
	bVal = i;
	iVal = i;
	flVal = i;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_INTEGER;
}

coerciblevariant_t::coerciblevariant_t( float f ) : szVal(nullptr)
{
	// do the bool, int, and float conversions right now, set the flags
	bVal = f;
	iVal = f;
	flVal = f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_FLOAT;
}

void coerciblevariant_t::SetFloat( float f )
{
	Void();

	// do the bool, int, and float conversions right now, set the flags
	bVal = f;
	iVal = f;
	flVal = f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_FLOAT;
}

coerciblevariant_t::coerciblevariant_t( const char * str )
{
	SetString(str);
}

void coerciblevariant_t::SetString( const char * str )
{
	Void();

	// make a copy of the string for me.
	szVal = new char[DEFAULT_VARIANT_STRING_SIZE];
	V_strncpy( szVal, str, DEFAULT_VARIANT_STRING_SIZE );
	m_bvInitFields = kINIT_STRING;
	fieldType = FIELD_STRING;
}

coerciblevariant_t::coerciblevariant_t( const EHANDLE &handle )
	: eVal(handle), fieldType(FIELD_EHANDLE), m_bvInitFields(kINIT_EHANDLE), szVal(nullptr)
{};

coerciblevariant_t::coerciblevariant_t( CBaseEntity *ent )
	: eVal( ent ), fieldType(FIELD_EHANDLE), m_bvInitFields(kINIT_EHANDLE), szVal(nullptr)
{};

void coerciblevariant_t::SetEntity( CBaseEntity *val )
{
	Void();

	eVal = val;
	m_bvInitFields = kINIT_EHANDLE;
	fieldType = FIELD_EHANDLE;
}

// Get me as a string
const char * coerciblevariant_t::String( void )
{
	if ( (m_bvInitFields & kINIT_STRING) == 0 )
	{
		// need to initialize the string.
		contract_assert( ( szVal == nullptr ) && "A variant tried to convert itself to a string when it already was one." );

		// convert from the appropriate native type
		switch (fieldType)
		{
		case FIELD_BOOLEAN:
			szVal = new char[2];
			szVal[0] = bVal ? '1' : '0';
			szVal[1] = '\0';
			break;
		case FIELD_INTEGER:
			szVal = new char[24];
			V_snprintf( szVal, 24, "%d", iVal );
			break;
		case FIELD_FLOAT:
			szVal = new char[32];
			V_snprintf( szVal, 32, "%.3f", flVal );
			break;
		case FIELD_EHANDLE: // entity name, or ""
		{
			
			CBaseEntity *ent = eVal.Get();
			if (ent)
			{
				szVal = new char[DEFAULT_VARIANT_STRING_SIZE];
				V_strncpy( szVal, ent->GetEntityName().ToCStr(), DEFAULT_VARIANT_STRING_SIZE );
			}
			else
			{
				szVal = new char[1];
				szVal[0] = 0;
			}
			break;
		}
		case FIELD_STRING:
			contract_assert( false && "Tried to convert a variant from string to string?!" );
			break;
		default:
			contract_assert( false && "Tried to convert a variant from unknown field type %d" );
			szVal = new char[1];
			szVal[0] = 0;
			break;
		}

		m_bvInitFields |= kINIT_STRING;
	}

	return szVal;
}

float coerciblevariant_t::ConvertFloat() const
{
	// convert from salient native type to a float.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to turn the string into a float
		return std::atof(szVal);

	case FIELD_EHANDLE:
		contract_assert( false && "Variant tried to convert from EHANDLE to float?!" );
		return eVal.Get() ? 1 : 0;

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		contract_assert( false && "Can't-happen: coercible variant tried to convert from float to a number, which should already have been done" );
		return flVal;
	default:
		contract_assert( false && "Tried to convert a variant from unknown field type %d" );
		return 0;
	}
}

int coerciblevariant_t::ConvertInt() const
{
	// convert from salient native type to an int.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to turn the string into an int
		return std::atoi(szVal);

	case FIELD_EHANDLE:
		contract_assert( false && "Variant tried to convert from EHANDLE to int?!" );
		return eVal.Get() ? 1 : 0;

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		contract_assert( false && "Can't-happen: coercible variant tried to convert from int to a number, which should already have been done" );
		return iVal;
	default:
		contract_assert( false && "Tried to convert a variant from unknown field type %d" );
		return 0;
	}
}

bool coerciblevariant_t::ConvertBool() const
{
	// convert from salient native type to an bool.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to turn the string into an int
		return std::atoi(szVal);

	case FIELD_EHANDLE:
		return eVal.Get();

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		contract_assert( false && "Can't-happen: coercible variant tried to convert from int to a number, which should already have been done" );
		return iVal;
	default:
		contract_assert( false && "Tried to convert a variant from unknown field type %d" );
		return 0;
	}
}

CBaseEntity *coerciblevariant_t::ConvertEntity() const
{
	// convert from salient native type to an entity pointer.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to look up the entity by name
		return gEntList.FindEntityByName(nullptr, szVal);

	case FIELD_EHANDLE:
		return eVal.Get();

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		contract_assert( false && "Coercible variant tried to turn a number into an entity, which is just plain wierd." );
		return nullptr;
	default:
		contract_assert( false && "Tried to convert a variant from unknown field type %d" );
		return nullptr;
	}
}

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

//#define DEBUG_SNC_TYPE_PUN_BUG

//-----------------------------------------------------------------------------
// KD tree query callbacks
//-----------------------------------------------------------------------------
class CDirtySpatialPartitionEntityList : public CAutoGameSystem, public IPartitionQueryCallback
{
public:
	CDirtySpatialPartitionEntityList( char const *name );

	// Members of IGameSystem
	virtual bool Init();
	virtual void Shutdown();
	virtual void LevelShutdownPostEntity();

	// Members of IPartitionQueryCallback
	virtual void OnPreQuery( SpatialPartitionListMask_t listMask );
	virtual void OnPostQuery( SpatialPartitionListMask_t listMask );

	void AddEntity( CBaseEntity *pEntity );
	
	~CDirtySpatialPartitionEntityList();
	void LockPartitionForRead()
	{
		int nThreadId = g_nThreadID;
		if ( m_nReadLockCount[nThreadId] == 0 )
		{
			m_partitionMutex.lock_shared();
		}
		m_nReadLockCount[nThreadId]++;
	}
	void UnlockPartitionForRead()
	{
		int nThreadId = g_nThreadID;
		m_nReadLockCount[nThreadId]--;
		if ( m_nReadLockCount[nThreadId] == 0 )
		{
			m_partitionMutex.unlock_shared();
		}
	}

private:
	CTSListWithFreeList<CBaseHandle> m_DirtyEntities;
	std::shared_mutex	 m_partitionMutex;
	int			 m_partitionWriteId;
	int m_nReadLockCount[MAX_THREADS_SUPPORTED];
};

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDirtySpatialPartitionEntityList s_DirtyKDTree( "CDirtySpatialPartitionEntityList" );

//-----------------------------------------------------------------------------
// Force spatial partition updates (to avoid threading problems caused by lazy update)
//-----------------------------------------------------------------------------
void UpdateDirtySpatialPartitionEntities()
{
	SpatialPartitionListMask_t listMask;
	listMask = PARTITION_SERVER_GAME_EDICTS;
	s_DirtyKDTree.OnPreQuery( listMask );
	s_DirtyKDTree.OnPostQuery( listMask );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CDirtySpatialPartitionEntityList::CDirtySpatialPartitionEntityList( char const *name ) : CAutoGameSystem( name )
{
	m_DirtyEntities.Purge();
	std::memset( m_nReadLockCount, 0, sizeof( m_nReadLockCount ) );
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor.
//-----------------------------------------------------------------------------
CDirtySpatialPartitionEntityList::~CDirtySpatialPartitionEntityList()
{
	m_DirtyEntities.Purge();
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CDirtySpatialPartitionEntityList::Init()
{
	::partition->InstallQueryCallback( this );
	return true;
}

void CDirtySpatialPartitionEntityList::Shutdown()
{
	::partition->RemoveQueryCallback( this );
}

//-----------------------------------------------------------------------------
// Makes sure all entries in the KD tree are in the correct position
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::AddEntity( CBaseEntity *pEntity )
{
	m_DirtyEntities.PushItem( pEntity->GetRefEHandle() );
}

//-----------------------------------------------------------------------------
// Members of IGameSystem
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::LevelShutdownPostEntity()
{
	m_DirtyEntities.RemoveAll();
}

//-----------------------------------------------------------------------------
// Makes sure all entries in the KD tree are in the correct position
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::OnPreQuery( SpatialPartitionListMask_t listMask )
{
	const int validMask = PARTITION_SERVER_GAME_EDICTS;

	if ( !( listMask & validMask ) )
		return;

	int nThreadID = g_nThreadID;

	if ( m_partitionWriteId != 0 && m_partitionWriteId == nThreadID + 1 )
		return;

	// if you're holding a read lock, then these are entities that were still dirty after your trace started
	// or became dirty due to some other thread or callback. Updating them may cause corruption further up the
	// stack (e.g. partition iterator).  Ignoring the state change should be safe since it happened after the 
	// trace was requested or was unable to be resolved in a previous attempt (still dirty).
	if ( m_DirtyEntities.Count() && !m_nReadLockCount[nThreadID] )
	{
		std::vector< CBaseHandle > vecStillDirty;
		std::scoped_lock writeLock( m_partitionMutex );
		m_partitionWriteId = nThreadID + 1;
		CTSListWithFreeList<CBaseHandle>::Node_t *pCurrent, *pNext;
		while ( ( pCurrent = m_DirtyEntities.Detach() ) != nullptr )
		{
			while ( pCurrent )
			{
				CBaseHandle handle = pCurrent->elem;
				pNext = (CTSListWithFreeList<CBaseHandle>::Node_t *)pCurrent->Next;
				m_DirtyEntities.FreeNode( pCurrent );
				pCurrent = pNext;

				CBaseEntity *pEntity = gEntList.GetBaseEntity( handle );

				if ( pEntity )
				{
					// If an entity is in the middle of bone setup, don't call UpdatePartition
					//  which can cause it to redo bone setup on the same frame causing a recursive
					//  call to bone setup.
					if ( !pEntity->IsEFlagSet( EFL_SETTING_UP_BONES ) )
					{
						pEntity->CollisionProp()->UpdatePartition();
					}
					else
					{
						vecStillDirty.push_back( handle );
					}
				}
			}
		}
		if ( !vecStillDirty.empty() )
		{
			for ( int i = 0; i < std::ssize( vecStillDirty ); i++ )
			{
				m_DirtyEntities.PushItem( vecStillDirty[i] );
			}
		}
		m_partitionWriteId = 0;
	}
	LockPartitionForRead();
}

//-----------------------------------------------------------------------------
// Makes sure all entries in the KD tree are in the correct position
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::OnPostQuery( SpatialPartitionListMask_t listMask )
{
	if ( !( listMask & PARTITION_SERVER_GAME_EDICTS ) )
		return;

	if ( m_partitionWriteId != 0 )
		return;

	UnlockPartitionForRead();
}

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------

	IMPLEMENT_REFLECT_DATAMAP_NO_BASE( CCollisionProperty )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------

 void SendProxy_Solid( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	pOut->m_Int = ((CCollisionProperty*)pStruct)->GetSolid();
}

 void SendProxy_SolidFlags( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	pOut->m_Int = ((CCollisionProperty*)pStruct)->GetSolidFlags();
}

IMPLEMENT_REFLECT_TABLE( CCollisionProperty, DT_CollisionProperty );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CCollisionProperty::CCollisionProperty()
{
	m_Partition = PARTITION_INVALID_HANDLE;
	Init( nullptr );
}

CCollisionProperty::~CCollisionProperty()
{
	DestroyPartitionHandle();
}

const Vector&	CCollisionProperty::GetCollisionOrigin_Inline() const 
{ 
	return GetOuter()->GetAbsOrigin(); 
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CCollisionProperty::Init( CBaseEntity *pEntity )
{
	m_pOuter = pEntity;
	m_vecMins.GetForModify().Init();
	m_vecMaxs.GetForModify().Init();
	m_flRadius = 0.0f;
	m_triggerBloat = 0;
	m_usSolidFlags = 0;
	m_nSolidType = SOLID_NONE;

	// NOTE: This replicates previous behavior; we may always want to use BEST_COLLISION_BOUNDS
	m_nSurroundType = USE_OBB_COLLISION_BOUNDS;
	m_vecSurroundingMins = vec3_origin;
	m_vecSurroundingMaxs = vec3_origin;
	m_vecSpecifiedSurroundingMins.GetForModify().Init();
	m_vecSpecifiedSurroundingMaxs.GetForModify().Init();
}

//-----------------------------------------------------------------------------
// EntityHandle
//-----------------------------------------------------------------------------
IHandleEntity *CCollisionProperty::GetEntityHandle()
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Collision group
//-----------------------------------------------------------------------------
int CCollisionProperty::GetCollisionGroup() const
{
	return m_pOuter->GetCollisionGroup();
}

uint CCollisionProperty::GetRequiredTriggerFlags() const
{
	// debris only touches certain triggers
	if ( GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
		return FSOLID_TRIGGER_TOUCH_DEBRIS;

	// triggers don't touch other triggers (might be solid to other ents as well as trigger)
	if ( IsSolidFlagSet( FSOLID_TRIGGER ) )
		return 0;

	return FSOLID_TRIGGER;
}

const matrix3x4_t *CCollisionProperty::GetRootParentToWorldTransform() const
{
	if ( IsSolidFlagSet( FSOLID_ROOT_PARENT_ALIGNED ) )
	{
		CBaseEntity *pEntity = m_pOuter->GetRootMoveParent();
		contract_assert( pEntity );
		if ( pEntity )
		{
			return &pEntity->CollisionProp()->CollisionToWorldTransform();
		}
	}
	return nullptr;
}

IPhysicsObject *CCollisionProperty::GetVPhysicsObject() const
{
	return m_pOuter->VPhysicsGetObject();
}

//-----------------------------------------------------------------------------
// IClientUnknown
//-----------------------------------------------------------------------------
IClientUnknown* CCollisionProperty::GetIClientUnknown()
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Check for untouch
//-----------------------------------------------------------------------------
void CCollisionProperty::CheckForUntouch()
{
	if ( !IsSolid() && !IsSolidFlagSet(FSOLID_TRIGGER))
	{
		// If this ent's touch list isn't empty, it's transitioning to not solid
		if ( m_pOuter->IsCurrentlyTouching() )
		{
			// mark ent so that at the end of frame it will check to 
			// see if it's no longer touching ents
			m_pOuter->SetCheckUntouch( true );
		}
	}
}

//-----------------------------------------------------------------------------
// Sets the solid type
//-----------------------------------------------------------------------------
void CCollisionProperty::SetSolid( SolidType_t val )
{
	if ( m_nSolidType == val )
		return;

	bool bWasNotSolid = IsSolid();

	MarkSurroundingBoundsDirty();

	// OBB is not yet implemented
	if ( val == SOLID_BSP )
	{
		if ( GetOuter()->GetMoveParent() )
		{
			if ( GetOuter()->GetRootMoveParent()->GetSolid() != SOLID_BSP )
			{
				// must be SOLID_VPHYSICS because parent might rotate
				val = SOLID_VPHYSICS;
			}
		}
		// UNDONE: This should be fine in the client DLL too.  Move GetAllChildren() into shared code.
		// If the root of the hierarchy is SOLID_BSP, then assume that the designer
		// wants the collisions to rotate with this hierarchy so that the player can
		// move while riding the hierarchy.
		if ( !GetOuter()->GetMoveParent() )
		{
			// NOTE: This assumes things don't change back from SOLID_BSP
			// NOTE: This is 100% true for HL2 - need to support removing the flag to support changing from SOLID_BSP
			std::vector< CBaseEntity * > list;
			GetAllChildren( GetOuter(), list );
			for ( int i = std::ssize( list )-1; i>=0; --i )
			{
				list[i]->AddSolidFlags( FSOLID_ROOT_PARENT_ALIGNED );
			}
		}
	}

	m_nSolidType = val;

	m_pOuter->CollisionRulesChanged();

	UpdateServerPartitionMask( );

	if ( bWasNotSolid != IsSolid() )
	{
		CheckForUntouch();
	}
}

SolidType_t CCollisionProperty::GetSolid() const
{
	return (SolidType_t)m_nSolidType.Get();
}

//-----------------------------------------------------------------------------
// Sets the solid flags
//-----------------------------------------------------------------------------
void CCollisionProperty::SetSolidFlags( int flags )
{
	int oldFlags = m_usSolidFlags;
	m_usSolidFlags = (unsigned short)(flags & 0xFFFF);
	if ( oldFlags == m_usSolidFlags )
		return;

	// These two flags, if changed, can produce different surrounding bounds
	if ( (oldFlags & (FSOLID_FORCE_WORLD_ALIGNED | FSOLID_USE_TRIGGER_BOUNDS)) != 
		 (m_usSolidFlags & (FSOLID_FORCE_WORLD_ALIGNED | FSOLID_USE_TRIGGER_BOUNDS)) )
	{
		MarkSurroundingBoundsDirty();
	}

	if ( (oldFlags & (FSOLID_NOT_SOLID|FSOLID_TRIGGER)) != (m_usSolidFlags & (FSOLID_NOT_SOLID|FSOLID_TRIGGER)) )
	{
		m_pOuter->CollisionRulesChanged();
	}

	if ( (oldFlags & (FSOLID_NOT_SOLID | FSOLID_TRIGGER)) != (m_usSolidFlags & (FSOLID_NOT_SOLID | FSOLID_TRIGGER)) )
	{
		UpdateServerPartitionMask( );
		CheckForUntouch();
	}
}

//-----------------------------------------------------------------------------
// Coordinate system of the collision model
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::GetCollisionOrigin() const
{
	return GetCollisionOrigin_Inline();
}

const QAngle& CCollisionProperty::GetCollisionAngles() const
{
	if ( IsBoundsDefinedInEntitySpace() )
	{
		return m_pOuter->GetAbsAngles();
	}

	return vec3_angle;
}

const matrix3x4_t& CCollisionProperty::CollisionToWorldTransform() const
{
	static matrix3x4_t s_matTemp[4];
	static int s_nIndex = 0;

	matrix3x4_t &matResult = s_matTemp[s_nIndex];
	s_nIndex = (s_nIndex+1) & 0x3;

	if ( IsBoundsDefinedInEntitySpace() )
	{
		return m_pOuter->EntityToWorldTransform();
	}

	SetIdentityMatrix( matResult );
	MatrixSetColumn( GetCollisionOrigin_Inline(), 3, matResult );
	return matResult;
}

//-----------------------------------------------------------------------------
// Sets the collision bounds + the size
//-----------------------------------------------------------------------------
void CCollisionProperty::SetCollisionBounds( const Vector& mins, const Vector &maxs )
{
	if ( (m_vecMins == mins) && (m_vecMaxs == maxs) )
		return;
		
	m_vecMins = mins;
	m_vecMaxs = maxs;
	
	ASSERT_COORD( mins );
	ASSERT_COORD( maxs );

	Vector vecSize;
	VectorSubtract( maxs, mins, vecSize );
	m_flRadius = vecSize.Length() * 0.5f;

	MarkSurroundingBoundsDirty();
}

//-----------------------------------------------------------------------------
// Lazily calculates the 2D bounding radius. If we do this enough, we should
// calculate this in SetCollisionBounds above and cache the results in a data member!
//-----------------------------------------------------------------------------
float CCollisionProperty::BoundingRadius2D() const
{
	Vector vecSize;
	VectorSubtract( m_vecMaxs, m_vecMins, vecSize );

	vecSize.z = 0;	
	return vecSize.Length() * 0.5f;
}

//-----------------------------------------------------------------------------
// Bounding representation (OBB)
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::OBBMins( ) const
{
	return m_vecMins.Get();
}

const Vector& CCollisionProperty::OBBMaxs( ) const
{
	return m_vecMaxs.Get();
}

//-----------------------------------------------------------------------------
// Special trigger representation (OBB)
//-----------------------------------------------------------------------------
void CCollisionProperty::WorldSpaceTriggerBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs ) const
{
	WorldSpaceAABB( pVecWorldMins, pVecWorldMaxs );
	if ( ( GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS ) == 0 )
		return;

	// Don't bloat below, we don't want to trigger it with our heads
	pVecWorldMins->x -= m_triggerBloat;
	pVecWorldMins->y -= m_triggerBloat;

	pVecWorldMaxs->x += m_triggerBloat;
	pVecWorldMaxs->y += m_triggerBloat;
	pVecWorldMaxs->z += (float)m_triggerBloat * 0.5f;
}

void CCollisionProperty::UseTriggerBounds( bool bEnable, float flBloat )
{
	contract_assert( flBloat <= 127.0f );
	m_triggerBloat = (char )flBloat;
	if ( bEnable )
	{
		AddSolidFlags( FSOLID_USE_TRIGGER_BOUNDS );
		contract_assert( flBloat > 0.0f );
	}
	else
	{
		RemoveSolidFlags( FSOLID_USE_TRIGGER_BOUNDS );
	}
}

//-----------------------------------------------------------------------------
// Collision model (BSP)
//-----------------------------------------------------------------------------
int CCollisionProperty::GetCollisionModelIndex()
{
	return m_pOuter->GetModelIndex();
}

const model_t* CCollisionProperty::GetCollisionModel()
{
	return m_pOuter->GetModel();
}

//-----------------------------------------------------------------------------
// Collision methods implemented in the entity
// FIXME: This shouldn't happen there!!
//-----------------------------------------------------------------------------
bool CCollisionProperty::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return m_pOuter->TestCollision( ray, fContentsMask, tr );
}

bool CCollisionProperty::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return m_pOuter->TestHitboxes( ray, fContentsMask, tr );
}

//-----------------------------------------------------------------------------
// Computes a "normalized" point (range 0,0,0 - 1,1,1) in collision space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::NormalizedToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	pResult->x = Lerp( in.x, m_vecMins.Get().x, m_vecMaxs.Get().x );
	pResult->y = Lerp( in.y, m_vecMins.Get().y, m_vecMaxs.Get().y );
	pResult->z = Lerp( in.z, m_vecMins.Get().z, m_vecMaxs.Get().z );
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in collision space to normalized space
//-----------------------------------------------------------------------------
const Vector &	CCollisionProperty::CollisionToNormalizedSpace( const Vector &in, Vector *pResult ) const
{
	Vector vecSize = OBBSize( );
	pResult->x = ( vecSize.x != 0.0f ) ? ( in.x - m_vecMins.Get().x ) / vecSize.x : 0.5f;
	pResult->y = ( vecSize.y != 0.0f ) ? ( in.y - m_vecMins.Get().y ) / vecSize.y : 0.5f;
	pResult->z = ( vecSize.z != 0.0f ) ? ( in.z - m_vecMins.Get().z ) / vecSize.z : 0.5f;
	return *pResult;
}

//-----------------------------------------------------------------------------
// Computes a "normalized" point (range 0,0,0 - 1,1,1) in world space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::NormalizedToWorldSpace( const Vector &in, Vector *pResult ) const
{
	Vector vecCollisionSpace;
	NormalizedToCollisionSpace( in, &vecCollisionSpace );
	CollisionToWorldSpace( vecCollisionSpace, pResult );
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in world space to normalized space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::WorldToNormalizedSpace( const Vector &in, Vector *pResult ) const
{
	Vector vecCollisionSpace;
	WorldToCollisionSpace( in, &vecCollisionSpace );
	CollisionToNormalizedSpace( vecCollisionSpace, pResult );
	return *pResult;
}

//-----------------------------------------------------------------------------
// Selects a random point in the bounds given the normalized 0-1 bounds 
//-----------------------------------------------------------------------------
void CCollisionProperty::RandomPointInBounds( const Vector &vecNormalizedMins, const Vector &vecNormalizedMaxs, Vector *pPoint) const
{
	Vector vecNormalizedSpace;
	vecNormalizedSpace.x = random_valve->RandomFloat( vecNormalizedMins.x, vecNormalizedMaxs.x );
	vecNormalizedSpace.y = random_valve->RandomFloat( vecNormalizedMins.y, vecNormalizedMaxs.y );
	vecNormalizedSpace.z = random_valve->RandomFloat( vecNormalizedMins.z, vecNormalizedMaxs.z );
	NormalizedToWorldSpace( vecNormalizedSpace, pPoint );
}

const
bool g_bRepeatTransformAABB = false;

//-----------------------------------------------------------------------------
// Transforms an AABB measured in entity space to a box that surrounds it in world space
//-----------------------------------------------------------------------------
void CCollisionProperty::CollisionAABBToWorldAABB( const Vector &entityMins, 
	const Vector &entityMaxs, Vector *pWorldMins, Vector *pWorldMaxs ) const
{
	Vector copyEntityMins = entityMins, copyEntityMaxs = entityMaxs;
	ASSERT_COORD( entityMins );
	ASSERT_COORD( entityMaxs );
	if ( !IsBoundsDefinedInEntitySpace() || (GetCollisionAngles() == vec3_angle) )
	{
		VectorAdd( entityMins, GetCollisionOrigin_Inline(), *pWorldMins );
		VectorAdd( entityMaxs, GetCollisionOrigin_Inline(), *pWorldMaxs );
		ASSERT_COORD( *pWorldMins );
		ASSERT_COORD( *pWorldMaxs );
	}
	else
	{
		do
		{
			matrix3x4_t tm = CollisionToWorldTransform();
			ASSERT_COORD( entityMins );
			ASSERT_COORD( entityMaxs );
			TransformAABB( tm, entityMins, entityMaxs, *pWorldMins, *pWorldMaxs );
			ASSERT_COORD( *pWorldMins );
			ASSERT_COORD( *pWorldMaxs );
		}
		while( g_bRepeatTransformAABB );
	}
}

/*
void CCollisionProperty::WorldAABBToCollisionAABB( const Vector &worldMins, const Vector &worldMaxs, Vector *pEntityMins, Vector *pEntityMaxs ) const
{
	if ( !IsBoundsDefinedInEntitySpace() || (GetCollisionAngles() == vec3_angle) )
	{
		VectorSubtract( worldMins, GetAbsOrigin(), *pEntityMins );
		VectorSubtract( worldMaxs, GetAbsOrigin(), *pEntityMaxs );
	}
	else
	{
		ITransformAABB( CollisionToWorldTransform(), worldMins, worldMaxs, *pEntityMins, *pEntityMaxs );
	}
}
*/

//-----------------------------------------------------------------------------
// Is a worldspace point within the bounds of the OBB?
//-----------------------------------------------------------------------------
bool CCollisionProperty::IsPointInBounds( const Vector &vecWorldPt ) const
{
	Vector vecLocalSpace;
	WorldToCollisionSpace( vecWorldPt, &vecLocalSpace );
	return ( ( vecLocalSpace.x >= m_vecMins.Get().x && vecLocalSpace.x <= m_vecMaxs.Get().x ) &&
			( vecLocalSpace.y >= m_vecMins.Get().y && vecLocalSpace.y <= m_vecMaxs.Get().y ) &&
			( vecLocalSpace.z >= m_vecMins.Get().z && vecLocalSpace.z <= m_vecMaxs.Get().z ) );
}

//-----------------------------------------------------------------------------
// Computes the nearest point in the OBB to a point specified in world space
//-----------------------------------------------------------------------------
void CCollisionProperty::CalcNearestPoint( const Vector &vecWorldPt, Vector *pVecNearestWorldPt ) const
{
	// Calculate physics force
	Vector localPt, localClosestPt;
	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( m_vecMins.Get(), m_vecMaxs.Get(), localPt, localClosestPt );
	CollisionToWorldSpace( localClosestPt, pVecNearestWorldPt );
}

//-----------------------------------------------------------------------------
// Computes the nearest point in the OBB to a point specified in world space
//-----------------------------------------------------------------------------
float CCollisionProperty::CalcDistanceFromPoint( const Vector &vecWorldPt ) const
{
	// Calculate physics force
	Vector localPt, localClosestPt;
	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( m_vecMins.Get(), m_vecMaxs.Get(), localPt, localClosestPt );
	return localPt.DistTo( localClosestPt );
}

//-----------------------------------------------------------------------------
// Computes the square distance of the closest point in the OBB to a point specified in world space
//-----------------------------------------------------------------------------
float CCollisionProperty::CalcSqrDistanceFromPoint( const Vector &vecWorldPt ) const
{
	// Calculate physics force
	Vector localPt, localClosestPt;
	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( m_vecMins.Get(), m_vecMaxs.Get(), localPt, localClosestPt );
	return localPt.DistToSqr( localClosestPt );
}

//-----------------------------------------------------------------------------
// Compute the largest dot product of the OBB and the specified direction vector
//-----------------------------------------------------------------------------
float CCollisionProperty::ComputeSupportMap( const Vector &vecDirection ) const
{
	Vector vecCollisionDir;
	WorldDirectionToCollisionSpace( vecDirection, &vecCollisionDir );

	float flResult = DotProduct( GetCollisionOrigin_Inline(), vecDirection );
	flResult += (( vecCollisionDir.x >= 0.0f ) ? m_vecMaxs.Get().x : m_vecMins.Get().x) * vecCollisionDir.x;
	flResult += (( vecCollisionDir.y >= 0.0f ) ? m_vecMaxs.Get().y : m_vecMins.Get().y) * vecCollisionDir.y;
	flResult += (( vecCollisionDir.z >= 0.0f ) ? m_vecMaxs.Get().z : m_vecMins.Get().z) * vecCollisionDir.z;

	return flResult;
}

//-----------------------------------------------------------------------------
// Expand trigger bounds..
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeVPhysicsSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	bool bSetBounds = false;
	IPhysicsObject *pPhysicsObject = GetOuter()->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		if ( pPhysicsObject->GetCollide() )
		{
			physcollision->CollideGetAABB( pVecWorldMins, pVecWorldMaxs, 
				pPhysicsObject->GetCollide(), GetCollisionOrigin_Inline(), GetCollisionAngles() );
			bSetBounds = true;
		}
		else if ( pPhysicsObject->GetSphereRadius( ) )
		{
			float flRadius = pPhysicsObject->GetSphereRadius( );
			Vector vecExtents( flRadius, flRadius, flRadius );
			VectorSubtract( GetCollisionOrigin_Inline(), vecExtents, *pVecWorldMins );
			VectorAdd( GetCollisionOrigin_Inline(), vecExtents, *pVecWorldMaxs );
			bSetBounds = true;
		}
	}

	if ( !bSetBounds )
	{
		*pVecWorldMins = GetCollisionOrigin_Inline();
		*pVecWorldMaxs = *pVecWorldMins;
	}

	// Also, lets expand for the trigger bounds also
	if ( IsSolidFlagSet( FSOLID_USE_TRIGGER_BOUNDS ) )
	{
		Vector vecWorldTriggerMins, vecWorldTriggerMaxs;
		WorldSpaceTriggerBounds( &vecWorldTriggerMins, &vecWorldTriggerMaxs );
		VectorMin( vecWorldTriggerMins, *pVecWorldMins, *pVecWorldMins );
		VectorMax( vecWorldTriggerMaxs, *pVecWorldMaxs, *pVecWorldMaxs );
	}
}

//-----------------------------------------------------------------------------
// Expand trigger bounds..
//-----------------------------------------------------------------------------
bool CCollisionProperty::ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
	if (pAnim)
	{
		return pAnim->ComputeHitboxSurroundingBox( pVecWorldMins, pVecWorldMaxs );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on the current sequence box
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeOBBBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	bool bUseVPhysics = false;
	if ( ( GetSolid() == SOLID_VPHYSICS ) && ( GetOuter()->GetMoveType() == MOVETYPE_VPHYSICS ) )
	{
		// UNDONE: This may not be necessary any more.
		IPhysicsObject *pPhysics = GetOuter()->VPhysicsGetObject();
		bUseVPhysics = pPhysics && pPhysics->IsAsleep();
	}
	ComputeCollisionSurroundingBox( bUseVPhysics, pVecWorldMins, pVecWorldMaxs );
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds from the current sequence box
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeRotationExpandedSequenceBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
	if ( !pAnim )
	{
		ComputeOBBBounds( pVecWorldMins, pVecWorldMaxs );
		return;
	}

	Vector mins, maxs;
	pAnim->ExtractBbox( pAnim->GetSequence(), mins, maxs );

	float flRadius = std::max( std::max( FloatMakePositive( mins.x ), FloatMakePositive( maxs.x ) ),
					      std::max( FloatMakePositive( mins.y ), FloatMakePositive( maxs.y ) ) );
	mins.x = mins.y = -flRadius;
	maxs.x = maxs.y = flRadius;

	// Add bloat to account for gesture sequences
	Vector vecBloat( 6, 6, 0 );
	mins -= vecBloat;
	maxs += vecBloat;
	ASSERT_COORD( m_vecSurroundingMins );
	ASSERT_COORD( m_vecSurroundingMaxs );
	// NOTE: This is necessary because the server doesn't know how to blend
	// animations together. Therefore, we have to just pick a box that can
	// surround all of our potential sequences. This should be something we
	// should be able to compute @ tool time instead, however.
	VectorMin( mins, m_vecSurroundingMins, mins );
	VectorMax( maxs, m_vecSurroundingMaxs, maxs );

	VectorAdd( mins, GetCollisionOrigin_Inline(), *pVecWorldMins );
	VectorAdd( maxs, GetCollisionOrigin_Inline(), *pVecWorldMaxs );
}

//-----------------------------------------------------------------------------
// Expand trigger bounds..
//-----------------------------------------------------------------------------
bool CCollisionProperty::ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
	if (pAnim)
	{
		return pAnim->ComputeEntitySpaceHitboxSurroundingBox( pVecWorldMins, pVecWorldMaxs );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds from the the OBB (not vphysics)
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeRotationExpandedBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	if ( !IsBoundsDefinedInEntitySpace() )
	{
		*pVecWorldMins = m_vecMins;
		*pVecWorldMaxs = m_vecMaxs;
	}
	else
	{
		float flMaxVal;
		flMaxVal = std::max( FloatMakePositive(m_vecMins.Get().x), FloatMakePositive(m_vecMaxs.Get().x) );
		pVecWorldMins->x = -flMaxVal;
		pVecWorldMaxs->x = flMaxVal;

		flMaxVal = std::max( FloatMakePositive(m_vecMins.Get().y), FloatMakePositive(m_vecMaxs.Get().y) );
		pVecWorldMins->y = -flMaxVal;
		pVecWorldMaxs->y = flMaxVal;

		flMaxVal = std::max( FloatMakePositive(m_vecMins.Get().z), FloatMakePositive(m_vecMaxs.Get().z) );
		pVecWorldMins->z = -flMaxVal;
		pVecWorldMaxs->z = flMaxVal;
	}
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on whatever algorithm we want...
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeCollisionSurroundingBox( bool bUseVPhysics, Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	contract_assert( GetSolid() != SOLID_CUSTOM );

	// NOTE: For solid none, we are still going to use the bounds; necessary because
	// the surrounding box is used for the PVS...
	// FIXME: Should we make some other call for the PVS stuff?? If so, we should return
	// a point bounds for SOLID_NONE...
//	if ( GetSolid() == SOLID_NONE )
//	{
//		*pVecWorldMins = GetCollisionOrigin_Inline();
//		*pVecWorldMaxs = *pVecWorldMins;
//		return;
//	}

	if ( bUseVPhysics )
	{
		ComputeVPhysicsSurroundingBox( pVecWorldMins, pVecWorldMaxs );
	}
	else
	{
		// Will expand the bounds for the trigger, if it is a trigger
		WorldSpaceTriggerBounds( pVecWorldMins, pVecWorldMaxs );
	}
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on whatever algorithm we want...
//-----------------------------------------------------------------------------

void CCollisionProperty::ComputeSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	if (( GetSolid() == SOLID_CUSTOM ) && (m_nSurroundType != USE_GAME_CODE ))
	{
		// NOTE: This can only happen in transition periods, say during network
		// reception on the client. We expect USE_GAME_CODE to be used with SOLID_CUSTOM
		*pVecWorldMins = GetCollisionOrigin_Inline();
		*pVecWorldMaxs = *pVecWorldMins;
		return;
	}

	switch( m_nSurroundType )
	{
	case USE_OBB_COLLISION_BOUNDS:
		contract_assert( GetSolid() != SOLID_CUSTOM );
		ComputeOBBBounds( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_BEST_COLLISION_BOUNDS:
		contract_assert( GetSolid() != SOLID_CUSTOM );
		ComputeCollisionSurroundingBox( (GetSolid() == SOLID_VPHYSICS), pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS:
		ComputeRotationExpandedSequenceBounds( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
		contract_assert( GetSolid() != SOLID_CUSTOM );
		ComputeCollisionSurroundingBox( false, pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_HITBOXES:
		ComputeHitboxSurroundingBox( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_ROTATION_EXPANDED_BOUNDS:
		ComputeRotationExpandedBounds( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_SPECIFIED_BOUNDS:
		VectorAdd( GetCollisionOrigin_Inline(), m_vecSpecifiedSurroundingMins, *pVecWorldMins );
		VectorAdd( GetCollisionOrigin_Inline(), m_vecSpecifiedSurroundingMaxs, *pVecWorldMaxs );
		break;

	case USE_GAME_CODE:
		GetOuter()->ComputeWorldSpaceSurroundingBox( pVecWorldMins, pVecWorldMaxs );
		contract_assert( pVecWorldMins->x <= pVecWorldMaxs->x );
		contract_assert( pVecWorldMins->y <= pVecWorldMaxs->y );
		contract_assert( pVecWorldMins->z <= pVecWorldMaxs->z );
		return;
	}

//#ifdef DEBUG
//#endif
}

//-----------------------------------------------------------------------------
// Sets the method by which the surrounding collision bounds is set
//-----------------------------------------------------------------------------
void CCollisionProperty::SetSurroundingBoundsType( SurroundingBoundsType_t type, const Vector *pMins, const Vector *pMaxs )
{	
	m_nSurroundType = type;
	if (type != USE_SPECIFIED_BOUNDS)
	{
		contract_assert( !pMins && !pMaxs );
		MarkSurroundingBoundsDirty();
	}
	else
	{
		contract_assert( pMins && pMaxs );
		ASSERT_COORD( *pMins );
		ASSERT_COORD( *pMaxs );
		m_vecSpecifiedSurroundingMins = *pMins;
		m_vecSpecifiedSurroundingMaxs = *pMaxs;
		m_vecSurroundingMins = *pMins;
		m_vecSurroundingMaxs = *pMaxs;

		ASSERT_COORD( m_vecSurroundingMins );
		ASSERT_COORD( m_vecSurroundingMaxs );
	}
}

//-----------------------------------------------------------------------------
// Marks the entity has having a dirty surrounding box
//-----------------------------------------------------------------------------
void CCollisionProperty::MarkSurroundingBoundsDirty()
{
	// don't bother with the world
	if ( m_pOuter->entindex() == 0 )
		return;

	GetOuter()->AddEFlags( EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS );
	MarkPartitionHandleDirty();

	GetOuter()->NetworkProp()->MarkPVSInformationDirty();
}

//-----------------------------------------------------------------------------
// Does VPhysicsUpdate make us need to recompute the surrounding box?
//-----------------------------------------------------------------------------
bool CCollisionProperty::DoesVPhysicsInvalidateSurroundingBox( ) const
{
	switch ( m_nSurroundType )
	{
	case USE_BEST_COLLISION_BOUNDS:
		return true;

	case USE_OBB_COLLISION_BOUNDS:
		return (GetSolid() == SOLID_VPHYSICS) && (GetOuter()->GetMoveType() == MOVETYPE_VPHYSICS) && GetOuter()->VPhysicsGetObject();

	// In the case of game code, we don't really know, so we have to assume it does
	case USE_GAME_CODE:
		return true;

	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
	case USE_HITBOXES:
	case USE_ROTATION_EXPANDED_BOUNDS:
	case USE_SPECIFIED_BOUNDS:
	case USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS:
		return false;

	default:
		contract_assert( 0 );
		return true;
	}
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on whatever algorithm we want...
//-----------------------------------------------------------------------------
void CCollisionProperty::WorldSpaceSurroundingBounds( Vector *pVecMins, Vector *pVecMaxs )
{
	const Vector &vecAbsOrigin = GetCollisionOrigin_Inline();
	if ( GetOuter()->IsEFlagSet( EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS ))
	{
		GetOuter()->RemoveEFlags( EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS );
		ComputeSurroundingBox( pVecMins, pVecMaxs );
		ASSERT_COORD( *pVecMins );
		ASSERT_COORD( *pVecMaxs );
		VectorSubtract( *pVecMins, vecAbsOrigin, m_vecSurroundingMins );
		VectorSubtract( *pVecMaxs, vecAbsOrigin, m_vecSurroundingMaxs );
		
		ASSERT_COORD( m_vecSurroundingMins );
		ASSERT_COORD( m_vecSurroundingMaxs );
	}
	else
	{
		VectorAdd( m_vecSurroundingMins, vecAbsOrigin, *pVecMins );
		VectorAdd( m_vecSurroundingMaxs, vecAbsOrigin, *pVecMaxs );
	}
}

//-----------------------------------------------------------------------------
// Spatial partition
//-----------------------------------------------------------------------------
void CCollisionProperty::CreatePartitionHandle()
{
	// Put the entity into the spatial partition.
	contract_assert( m_Partition == PARTITION_INVALID_HANDLE );
	m_Partition = ::partition->CreateHandle( GetEntityHandle() );
}

void CCollisionProperty::DestroyPartitionHandle()
{
	if ( m_Partition != PARTITION_INVALID_HANDLE )
	{
		::partition->DestroyHandle( m_Partition );
		m_Partition = PARTITION_INVALID_HANDLE;
	}
}

uint CCollisionProperty::ComputeServerPartitionMask( )
{
	uint nMask = 0;
	// Don't bother with deleted things
	// don't add the world
	if ( m_pOuter->edict() && m_pOuter->entindex() != 0 )
	{

		// Make sure it's in the list of all entities
		bool bIsSolid = IsSolid() || IsSolidFlagSet(FSOLID_TRIGGER);
		if ( bIsSolid || m_pOuter->IsEFlagSet(EFL_USE_PARTITION_WHEN_NOT_SOLID) )
		{
			nMask |= PARTITION_ENGINE_NON_STATIC_EDICTS;
		}

		if ( bIsSolid )
		{
			// Insert it into the appropriate lists.
			// We have to continually reinsert it because its solid type may have changed
			if ( !IsSolidFlagSet(FSOLID_NOT_SOLID) )
			{
				nMask |= PARTITION_ENGINE_SOLID_EDICTS;
			}
			if ( IsSolidFlagSet(FSOLID_TRIGGER) )
			{
				nMask |=	PARTITION_ENGINE_TRIGGER_EDICTS;
			}
			int nMoveType = GetOuter()->GetMoveType();
			if ( IsPushableMoveType( nMoveType )  )
			{
				nMask |= PARTITION_ENGINE_PUSHABLE;
			}
		}
	}
	return nMask;
}

//-----------------------------------------------------------------------------
// Updates the spatial partition
//-----------------------------------------------------------------------------
void CCollisionProperty::UpdateServerPartitionMask( )
{
	SpatialPartitionHandle_t handle = GetPartitionHandle();
	if ( handle == PARTITION_INVALID_HANDLE )
		return;

	uint nMask = ComputeServerPartitionMask();
	// Remove it from whatever lists it may be in at the moment
	// We'll re-add it below if we need to.
	::partition->RemoveAndInsert( ~0, nMask, handle );
}

//-----------------------------------------------------------------------------
// Marks the spatial partition dirty
//-----------------------------------------------------------------------------
void CCollisionProperty::MarkPartitionHandleDirty()
{
	if ( !m_pOuter->IsEFlagSet( EFL_DIRTY_SPATIAL_PARTITION ) )
	{
		s_DirtyKDTree.AddEntity( m_pOuter );
		m_pOuter->AddEFlags( EFL_DIRTY_SPATIAL_PARTITION );
	}
}

//-----------------------------------------------------------------------------
// Updates the spatial partition
//-----------------------------------------------------------------------------
void CCollisionProperty::UpdatePartition( )
{
	if ( m_pOuter->IsEFlagSet( EFL_DIRTY_SPATIAL_PARTITION ) )
	{
		m_pOuter->RemoveEFlags( EFL_DIRTY_SPATIAL_PARTITION );

		contract_assert( m_pOuter->entindex() != 0 );

		// Don't bother with deleted things
		if ( !m_pOuter->edict() )
			return;

		if ( GetPartitionHandle() == PARTITION_INVALID_HANDLE )
		{
			CreatePartitionHandle();
			UpdateServerPartitionMask();
		}

		// We don't need to bother if it's not a trigger or solid
		if ( IsSolid() || IsSolidFlagSet( FSOLID_TRIGGER ) || m_pOuter->IsEFlagSet( EFL_USE_PARTITION_WHEN_NOT_SOLID ) )
		{
			// Bloat a little bit...
			if ( BoundingRadius() != 0.0f )
			{
				Vector vecSurroundMins, vecSurroundMaxs;
				ASSERT_COORD( m_vecSurroundingMins );
				ASSERT_COORD( m_vecSurroundingMaxs );
				WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
				ASSERT_COORD( m_vecSurroundingMins );
				ASSERT_COORD( m_vecSurroundingMaxs );
				vecSurroundMins -= Vector( 1, 1, 1 );
				vecSurroundMaxs += Vector( 1, 1, 1 );
				::partition->ElementMoved( GetPartitionHandle(), vecSurroundMins,  vecSurroundMaxs );
			}
			else
			{
				::partition->ElementMoved( GetPartitionHandle(), GetCollisionOrigin_Inline(),  GetCollisionOrigin_Inline() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
CBaseEntity *CCollisionProperty::GetOuter()
{
	return m_pOuter;
}

const CBaseEntity *CCollisionProperty::GetOuter() const
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Spatial partition
//-----------------------------------------------------------------------------
SpatialPartitionHandle_t CCollisionProperty::GetPartitionHandle() const
{
	return m_Partition;
}

SurroundingBoundsType_t CCollisionProperty::GetSurroundingBoundsType() const
{
	return (SurroundingBoundsType_t)m_nSurroundType.Get();
}

//-----------------------------------------------------------------------------
// Methods related to size
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::OBBSize( ) const
{
	// NOTE: Could precache this, but it's not used that often..
	Vector &temp = AllocTempVector();
	VectorSubtract( m_vecMaxs, m_vecMins, temp );
	return temp;
}

//-----------------------------------------------------------------------------
// Bounding radius size
//-----------------------------------------------------------------------------
float CCollisionProperty::BoundingRadius() const
{
	return m_flRadius;
}

//-----------------------------------------------------------------------------
// Methods relating to solid flags
//-----------------------------------------------------------------------------
bool CCollisionProperty::IsBoundsDefinedInEntitySpace() const
{
	return (( m_usSolidFlags & FSOLID_FORCE_WORLD_ALIGNED ) == 0 ) &&
			( m_nSolidType != SOLID_BBOX ) && ( m_nSolidType != SOLID_NONE );
}

void CCollisionProperty::ClearSolidFlags( void )
{
	SetSolidFlags( 0 );
}

void CCollisionProperty::RemoveSolidFlags( int flags )
{
	SetSolidFlags( m_usSolidFlags & ~flags );
}

void CCollisionProperty::AddSolidFlags( int flags )
{
	SetSolidFlags( m_usSolidFlags | flags );
}

int CCollisionProperty::GetSolidFlags( void ) const
{
	return m_usSolidFlags;
}

bool CCollisionProperty::IsSolidFlagSet( int flagMask ) const
{
	return (m_usSolidFlags & flagMask) != 0;
}

bool CCollisionProperty::IsSolid() const
{
	return ::IsSolid( (SolidType_t)(unsigned char)m_nSolidType, m_usSolidFlags );
}

//-----------------------------------------------------------------------------
// Returns the center in OBB space
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::OBBCenter( ) const
{
	Vector &vecResult = AllocTempVector();
	VectorLerp( m_vecMins, m_vecMaxs, 0.5f, vecResult );
	return vecResult;
}

//-----------------------------------------------------------------------------
// center point of entity
//-----------------------------------------------------------------------------
const Vector &CCollisionProperty::WorldSpaceCenter( ) const 
{
	Vector &vecResult = AllocTempVector();
	CollisionToWorldSpace( OBBCenter(), &vecResult );
	return vecResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in OBB space to world space
//-----------------------------------------------------------------------------
const Vector &CCollisionProperty::CollisionToWorldSpace( const Vector &in, Vector *pResult ) const 
{
	// Makes sure we don't re-use the same temp twice
	if ( !IsBoundsDefinedInEntitySpace() || ( GetCollisionAngles() == vec3_angle ) )
	{
		VectorAdd( in, GetCollisionOrigin(), *pResult );
	}
	else
	{
		VectorTransform( in, CollisionToWorldTransform(), *pResult );
	}
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in world space to OBB space
//-----------------------------------------------------------------------------
const Vector &CCollisionProperty::WorldToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	if ( !IsBoundsDefinedInEntitySpace() || ( GetCollisionAngles() == vec3_angle ) )
	{
		VectorSubtract( in, GetCollisionOrigin(), *pResult );
	}
	else
	{
		VectorITransform( in, CollisionToWorldTransform(), *pResult );
	}
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a direction in world space to OBB space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::WorldDirectionToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	if ( !IsBoundsDefinedInEntitySpace() || ( GetCollisionAngles() == vec3_angle ) )
	{
		*pResult = in;
	}
	else
	{
		VectorIRotate( in, CollisionToWorldTransform(), *pResult );
	}
	return *pResult;
}

//-----------------------------------------------------------------------------
// Computes a bounding box in world space surrounding the collision bounds
//-----------------------------------------------------------------------------
void CCollisionProperty::WorldSpaceAABB( Vector *pWorldMins, Vector *pWorldMaxs ) const
{
	CollisionAABBToWorldAABB( m_vecMins, m_vecMaxs, pWorldMins, pWorldMaxs );
}

//-----------------------------------------------------------------------------
// Does a rotation make us need to recompute the surrounding box?
//-----------------------------------------------------------------------------
bool CCollisionProperty::DoesSequenceChangeInvalidateSurroundingBox( ) const
{
	return ( m_nSurroundType == USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS );
}

//-----------------------------------------------------------------------------
// Does a rotation make us need to recompute the surrounding box?
//-----------------------------------------------------------------------------
bool CCollisionProperty::DoesRotationInvalidateSurroundingBox( ) const
{
	if ( IsSolidFlagSet(FSOLID_ROOT_PARENT_ALIGNED) )
		return true;

	switch ( m_nSurroundType )
	{
	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
	case USE_OBB_COLLISION_BOUNDS:
	case USE_BEST_COLLISION_BOUNDS:
		return IsBoundsDefinedInEntitySpace();

	// In the case of game code, we don't really know, so we have to assume it does
	case USE_HITBOXES:
	case USE_GAME_CODE:
		return true;

	case USE_ROTATION_EXPANDED_BOUNDS:
	case USE_SPECIFIED_BOUNDS:
	case USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS:
		return false;

	default:
		contract_assert( 0 );
		return true;
	}
}

class CBasePlayer;
class CColorCorrection;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Color correction entity.

inline float COLOR_CORRECTION_ENT_THINK_RATE() { return TICK_INTERVAL; }

static const char *s_pFadeInContextThink = "ColorCorrectionFadeInThink";
static const char *s_pFadeOutContextThink = "ColorCorrectionFadeOutThink";

LINK_ENTITY_TO_CLASS(color_correction, CColorCorrection);

IMPLEMENT_REFLECT_DATAMAP( CColorCorrection )

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
IMPLEMENT_REFLECT_SERVERCLASS( CColorCorrection, DT_ColorCorrection )

CColorCorrection::CColorCorrection() : BaseClass()
{
	m_bEnabled = true;
	m_MinFalloff = 0.0f;
	m_MaxFalloff = 1000.0f;
	m_flMaxWeight = 1.0f;
	m_flCurWeight.Set( 0.0f );
	m_flFadeInDuration = 0.0f;
	m_flFadeOutDuration = 0.0f;
	m_flStartFadeInWeight = 0.0f;
	m_flStartFadeOutWeight = 0.0f;
	m_flTimeStartFadeIn = 0.0f;
	m_flTimeStartFadeOut = 0.0f;
	m_netlookupFilename.GetForModify()[0] = 0;
	m_lookupFilename = NULL_STRING;
	m_bMaster = false;
	m_bClientSide = false;
	m_bExclusive = false;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CColorCorrection::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CColorCorrection::Spawn_Impl( void )
{
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT | EFL_DIRTY_ABSTRANSFORM );
	Precache();
	SetSolid( SOLID_NONE );

	// To fade in/out the weight.
	SetContextThink( &CColorCorrection::FadeInThink, TICK_NEVER_THINK, s_pFadeInContextThink );
	SetContextThink( &CColorCorrection::FadeOutThink, TICK_NEVER_THINK, s_pFadeOutContextThink );
	
	if( m_bStartDisabled )
	{
		m_bEnabled = false;
		m_flCurWeight.Set ( 0.0f );
	}
	else
	{
		m_bEnabled = true;
		m_flCurWeight.Set ( 1.0f );
	}

	m_bMaster = IsMaster();
	m_bClientSide = IsClientSide();

	BaseClass::Spawn_Impl();
}

void CColorCorrection::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	V_strncpy( m_netlookupFilename.GetForModify(), STRING( m_lookupFilename ), MAX_PATH );
}

//-----------------------------------------------------------------------------
// Purpose: Sets up internal vars needed for fade in lerping
//-----------------------------------------------------------------------------
void CColorCorrection::FadeIn ( void )
{
	if ( m_bClientSide || ( m_bEnabled && m_flCurWeight >= m_flMaxWeight ) )
		return;

	m_bEnabled = true;
	m_flTimeStartFadeIn = gpGlobals->curtime;
	m_flStartFadeInWeight = m_flCurWeight;
	SetNextThink ( gpGlobals->curtime + COLOR_CORRECTION_ENT_THINK_RATE(), s_pFadeInContextThink );
}

//-----------------------------------------------------------------------------
// Purpose: Sets up internal vars needed for fade out lerping
//-----------------------------------------------------------------------------
void CColorCorrection::FadeOut ( void )
{
	if ( m_bClientSide || ( !m_bEnabled && m_flCurWeight <= 0.0f ) )
		return;

	m_bEnabled = false;
	m_flTimeStartFadeOut = gpGlobals->curtime;
	m_flStartFadeOutWeight = m_flCurWeight;
	SetNextThink ( gpGlobals->curtime + COLOR_CORRECTION_ENT_THINK_RATE(), s_pFadeOutContextThink );
}

//-----------------------------------------------------------------------------
// Purpose: Fades lookup weight from CurWeight->MaxWeight
//-----------------------------------------------------------------------------
void CColorCorrection::FadeInThink( void )
{
	// Check for conditions where we shouldnt fade in
	if (		m_flFadeInDuration <= 0 ||  // not set to fade in
		 m_flCurWeight >= m_flMaxWeight ||  // already past max weight
							!m_bEnabled ||  // fade in/out mutex
				  m_flMaxWeight == 0.0f ||  // min==max
  m_flStartFadeInWeight >= m_flMaxWeight )  // already at max weight
	{
		SetNextThink ( TICK_NEVER_THINK, s_pFadeInContextThink );
		return;
	}
	
	// If we started fading in without fully fading out, use a truncated duration
    float flTimeToFade = m_flFadeInDuration;
	if ( m_flStartFadeInWeight > 0.0f )
	{	
		float flWeightRatio		= m_flStartFadeInWeight / m_flMaxWeight;
		flWeightRatio = clamp ( flWeightRatio, 0.0f, 0.99f );
		flTimeToFade			= m_flFadeInDuration * (1.0 - flWeightRatio);
	}	
	
	contract_assert( flTimeToFade > 0.0f );
	float flFadeRatio = (gpGlobals->curtime - m_flTimeStartFadeIn) / flTimeToFade;
	flFadeRatio = clamp ( flFadeRatio, 0.0f, 1.0f );
	m_flStartFadeInWeight = clamp ( m_flStartFadeInWeight, 0.0f, 1.0f );

	m_flCurWeight = Lerp( flFadeRatio, (float)m_flStartFadeInWeight, (float)m_flMaxWeight );

	SetNextThink( gpGlobals->curtime + COLOR_CORRECTION_ENT_THINK_RATE(), s_pFadeInContextThink );
}

//-----------------------------------------------------------------------------
// Purpose: Fades lookup weight from CurWeight->0.0 
//-----------------------------------------------------------------------------
void CColorCorrection::FadeOutThink( void )
{
	// Check for conditions where we shouldn't fade out
	if ( m_flFadeOutDuration <= 0 || // not set to fade out
			m_flCurWeight <= 0.0f || // already faded out
					   m_bEnabled || // fade in/out mutex
		   m_flMaxWeight == 0.0f  || // min==max
	 m_flStartFadeOutWeight <= 0.0f )// already at min weight
	{
		SetNextThink ( TICK_NEVER_THINK, s_pFadeOutContextThink );
		return;
	}

	// If we started fading out without fully fading in, use a truncated duration
    float flTimeToFade = m_flFadeOutDuration;
	if ( m_flStartFadeOutWeight < m_flMaxWeight )
	{	
		float flWeightRatio		= m_flStartFadeOutWeight / m_flMaxWeight;
		flWeightRatio = clamp ( flWeightRatio, 0.01f, 1.0f );
		flTimeToFade			= m_flFadeOutDuration * flWeightRatio;
	}	
	
	contract_assert( flTimeToFade > 0.0f );
	float flFadeRatio = (gpGlobals->curtime - m_flTimeStartFadeOut) / flTimeToFade;
	flFadeRatio = clamp ( flFadeRatio, 0.0f, 1.0f );
	m_flStartFadeOutWeight = clamp ( m_flStartFadeOutWeight, 0.0f, 1.0f );

	m_flCurWeight = Lerp( 1.0f - flFadeRatio, 0.0f, m_flStartFadeOutWeight );

	SetNextThink( gpGlobals->curtime + COLOR_CORRECTION_ENT_THINK_RATE(), s_pFadeOutContextThink );
}

//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CColorCorrection::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;

	if ( m_flFadeInDuration > 0.0f )
	{
		FadeIn();
	}
	else
	{
		m_flCurWeight = m_flMaxWeight;
	}
	
}

void CColorCorrection::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;

	if ( m_flFadeOutDuration > 0.0f )
	{
		FadeOut();
	}
	else
	{
		m_flCurWeight = 0.0f;
	}
	
}

void CColorCorrection::InputSetFadeInDuration( inputdata_t& inputdata )
{
	m_flFadeInDuration = inputdata.value.Float();
}

void CColorCorrection::InputSetFadeOutDuration( inputdata_t& inputdata )
{
	m_flFadeOutDuration = inputdata.value.Float();
}

CColorCorrectionSystem s_ColorCorrectionSystem( "ColorCorrectionSystem" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CColorCorrectionSystem *ColorCorrectionSystem( void )
{
	return &s_ColorCorrectionSystem;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out the fog controller.
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::LevelInitPreEntity( void )
{
	m_hMasterController = nullptr;
	ListenForGameEvent( "round_start" );
}

//-----------------------------------------------------------------------------
// Purpose: Find the master controller.  If no controller is 
//			set as Master, use the first controller found.
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::InitMasterController( void )
{
	CColorCorrection *pColorCorrection = nullptr;
	do
	{
		pColorCorrection = dynamic_cast<CColorCorrection*>( gEntList.FindEntityByClassname( pColorCorrection, "color_correction" ) );
		if ( pColorCorrection )
		{
			if ( m_hMasterController.Get() == nullptr )
			{
				m_hMasterController = pColorCorrection;
			}
			else
			{
				if ( pColorCorrection->IsMaster() )
				{
					m_hMasterController = pColorCorrection;
				}
			}
		}
	} while ( pColorCorrection );
}

//-----------------------------------------------------------------------------
// Purpose: On a multiplayer map restart, re-find the master controller.
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::FireGameEvent( IGameEvent *pEvent )
{
	InitMasterController();
}

//-----------------------------------------------------------------------------
// Purpose: On level load find the master fog controller.  If no controller is 
//			set as Master, use the first fog controller found.
//-----------------------------------------------------------------------------
void CColorCorrectionSystem::LevelInitPostEntity( void )
{
	InitMasterController();

	// HACK: Singleplayer games don't get a call to CBasePlayer::Spawn on level transitions.
	// CBasePlayer::Activate is called before this is called so that's too soon to set up the fog controller.
	// We don't have a hook similar to Activate that happens after LevelInitPostEntity
	// is called, or we could just do this in the player itself.
	if ( gpGlobals->maxClients == 1 )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer && ( pPlayer->m_hColorCorrectionCtrl.Get() == nullptr ) )
		{
			pPlayer->InitColorCorrectionController();
		}
	}
}

class CBaseEntity;
class CBaseTrigger;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Color correction entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class CColorCorrectionVolume : public CBaseTrigger
{
	[[= ks::reflect::NetTable{ .name = "DT_ColorCorrectionVolume", .base = false } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CColorCorrectionVolume, CBaseTrigger );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CColorCorrectionVolume();

	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState_Impl();

	void ThinkFunc();

	virtual bool PassesTriggerFilters(CBaseEntity *pOther);
	void StartTouch_Impl( CBaseEntity *pEntity );
	void EndTouch_Impl( CBaseEntity *pEntity );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }
	
	// Inputs
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "enabled" } ]] bool		m_bEnabled;
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool		m_bStartDisabled;

	CNetworkVar( float, m_Weight, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_MaxWeight, [[= ks::reflect::Key{ .name = "maxweight" } ]] ); 
	CNetworkString( m_lookupFilename, MAX_PATH, [[= ks::reflect::Net{} ]] );

	float		m_LastEnterWeight;
	float		m_LastEnterTime;

	float		m_LastExitWeight;
	float		m_LastExitTime;

	[[= ks::reflect::Key{ .name = "fadeDuration" } ]] float		m_FadeDuration;
};

LINK_ENTITY_TO_CLASS(color_correction_volume, CColorCorrectionVolume);

IMPLEMENT_REFLECT_DATAMAP( CColorCorrectionVolume )

IMPLEMENT_REFLECT_SERVERCLASS( CColorCorrectionVolume, DT_ColorCorrectionVolume )

CColorCorrectionVolume::CColorCorrectionVolume() : BaseClass()
{
	m_bEnabled = true;
	m_MaxWeight = 1.0f;
	m_lookupFilename.GetForModify()[0] = 0;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CColorCorrectionVolume::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CColorCorrectionVolume::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "filename" ) )
	{
		V_strncpy( m_lookupFilename.GetForModify(), szValue, MAX_PATH );

		return true;
	}
	else if ( FStrEq( szKeyName, "maxweight" ) )
	{
		float max_weight;
		std::sscanf( szValue, "%f", &max_weight );
		m_MaxWeight = max_weight;

		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CColorCorrectionVolume::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	AddEFlags( EFL_FORCE_CHECK_TRANSMIT | EFL_DIRTY_ABSTRANSFORM );
	Precache();

	SetSolid( SOLID_BSP );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
	SetModel( STRING( GetModelName() ) );

	SetThink( &CColorCorrectionVolume::ThinkFunc );
	SetNextThink( gpGlobals->curtime + 0.01f );

	if( m_bStartDisabled )
	{
		m_bEnabled = false;
	}
	else
	{
		m_bEnabled = true;
	}
}

bool CColorCorrectionVolume::PassesTriggerFilters( CBaseEntity *pEntity )
{
	if( pEntity == UTIL_GetLocalPlayer() )
		return true;

	return false;
}

void CColorCorrectionVolume::StartTouch_Impl( CBaseEntity *pEntity )
{
	m_LastEnterTime = gpGlobals->curtime;
	m_LastEnterWeight = m_Weight;
}

void CColorCorrectionVolume::EndTouch_Impl( CBaseEntity *pEntity )
{
	m_LastExitTime = gpGlobals->curtime;
	m_LastExitWeight = m_Weight;
}

void CColorCorrectionVolume::ThinkFunc( )
{
	if( !m_bEnabled )
	{
		m_Weight.Set( 0.0f );
	}
	else
	{
		if( m_LastEnterTime > m_LastExitTime )
		{
			// we most recently entered the volume
		
			if( m_Weight < 1.0f )
			{
				float dt = gpGlobals->curtime - m_LastEnterTime;
				float weight = m_LastEnterWeight + dt / ((1.0f-m_LastEnterWeight)*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = weight;
			}
		}
		else
		{
			// we most recently exitted the volume
		
			if( m_Weight > 0.0f )
			{
				float dt = gpGlobals->curtime - m_LastExitTime;
				float weight = (1.0f-m_LastExitWeight) + dt / (m_LastExitWeight*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = 1.0f - weight;
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.01f );
}

//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CColorCorrectionVolume::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CColorCorrectionVolume::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: contains entities who have no physical representation in the game, and who
//		must be triggered by other entities to cause their effects.

//-----------------------------------------------------------------------------
// Purpose: when fired, it changes which track the CD is playing
//-----------------------------------------------------------------------------
class CTargetCDAudioRep : public CPointEntity
{
public:
	DECLARE_CLASS( CTargetCDAudioRep, CPointEntity );

	[[= ks::reflect::Input{ .name = "ChangeCDTrack", .type = FIELD_VOID } ]] void InputChangeCDTrack( inputdata_t &inputdata );
	
	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "track" } ]] int m_iTrack;  // CD track to change to when fired
};

LINK_ENTITY_TO_CLASS( target_cdaudio, CTargetCDAudioRep );

IMPLEMENT_REFLECT_DATAMAP( CTargetCDAudioRep )

//-----------------------------------------------------------------------------
// Purpose: Changes the current playing CD track
//-----------------------------------------------------------------------------
void CTargetCDAudioRep::InputChangeCDTrack( inputdata_t &inputdata )
{
	int iTrack = m_iTrack;
	
	edict_t *pClient = nullptr;
	if ( gpGlobals->maxClients == 1 )
	{
		pClient = INDEXENT( 1 );
	}
	else
	{
		// In multiplayer, send it back to the activator
		CBasePlayer *player = entity_cast<CBasePlayer>( inputdata.pActivator );
		if ( player )
		{
			pClient = player->edict();
		}
	}
	
	// Can't play if the client is not connected!
	if ( !pClient )
		return;

	if ( iTrack < -1 || iTrack > 30 )
	{
		Warning( "TargetCDAudio - Track %d out of range\n", iTrack );
		return;
	}

	if ( iTrack == -1 )
	{
		engine->ClientCommand( pClient, "cd pause\n");
	}
	else
	{
		engine->ClientCommand ( pClient, "cd play %3d\n", iTrack );
	}
}

//-----------------------------------------------------------------------------
// Purpose: changes the gravity of the player who activates this entity
//-----------------------------------------------------------------------------
class CTargetChangeGravity : public CPointEntity
{
public:
	DECLARE_CLASS( CTargetChangeGravity, CPointEntity );

	DECLARE_DATADESC();

	[[= ks::reflect::Input{ .name = "ChangeGrav", .type = FIELD_VOID } ]] void InputChangeGrav( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ResetGrav", .type = FIELD_VOID } ]] void InputResetGrav( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "gravity" } ]] int m_iGravity;

	int m_iOldGrav;
};

LINK_ENTITY_TO_CLASS( target_changegravity, CTargetChangeGravity );

IMPLEMENT_REFLECT_DATAMAP( CTargetChangeGravity )

//-----------------------------------------------------------------------------
// Purpose: Input handler for changing the activator's gravity.
//-----------------------------------------------------------------------------
void CTargetChangeGravity::InputChangeGrav( inputdata_t &inputdata )
{
	CBasePlayer *pl = ToBasePlayer( inputdata.pActivator );
	if ( !pl )
		return;

	// Save the gravity to restore it in InputResetGrav
	if ( m_iOldGrav )
		m_iOldGrav = pl->GetGravity();

	pl->SetGravity(m_iGravity);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for resetting the activator's gravity.
//-----------------------------------------------------------------------------
void CTargetChangeGravity::InputResetGrav( inputdata_t &inputdata )
{
	CBasePlayer *pl = ToBasePlayer( inputdata.pActivator );
	if ( !pl )
		return;

	pl->SetGravity(m_iOldGrav);
}

using namespace ::ks::reflect;

//=========================================================
// Plane
//=========================================================
CPlane::CPlane ( void )
{
	m_fInitialized = false;
}

//=========================================================
// InitializePlane - Takes a normal for the plane and a
// point on the plane and 
//=========================================================
void CPlane::InitializePlane ( const Vector &vecNormal, const Vector &vecPoint )
{
	m_vecNormal = vecNormal;
	m_flDist = DotProduct ( m_vecNormal, vecPoint );
	m_fInitialized = true;
}

//=========================================================
// PointInFront - determines whether the given vector is 
// in front of the plane. 
//=========================================================
bool CPlane::PointInFront ( const Vector &vecPoint )
{
	float flFace;

	if ( !m_fInitialized )
	{
		return false;
	}

	flFace = DotProduct ( m_vecNormal, vecPoint ) - m_flDist;

	if ( flFace >= 0 )
	{
		return true;
	}

	return false;
}

//=========================================================
//=========================================================
float CPlane::PointDist ( const Vector &vecPoint )
{
	float flDist;

	if ( !m_fInitialized )
	{
		return false;
	}

	flDist = DotProduct ( m_vecNormal, vecPoint ) - m_flDist;

	return flDist;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

ConVar ai_debug_ragdoll_magnets( side_registry(), "ai_debug_ragdoll_magnets", "0");

LINK_ENTITY_TO_CLASS( phys_ragdollmagnet, CRagdollMagnet );
IMPLEMENT_REFLECT_DATAMAP( CRagdollMagnet )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CRagdollMagnet::InputEnable( inputdata_t &inputdata )
{
	Enable( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CRagdollMagnet::InputDisable( inputdata_t &inputdata )
{
	Enable( false );
}

//-----------------------------------------------------------------------------
// Purpose: Find the ragdoll magnet entity that should pull this entity's ragdoll
// Input  : *pNPC - the npc that's dying
// Output : CRagdollMagnet - the magnet that's best to use.
//
// NOTES:
//
// The nearest ragdoll magnet pulls me in IF:
//	- Present
//	- I'm within the magnet's RADIUS
//	- LATER: There is clear line of sight from my center to the magnet
//  - LATER: I'm not flagged to ignore ragdoll magnets
//	- LATER: The magnet is not turned OFF
//-----------------------------------------------------------------------------
CRagdollMagnet *CRagdollMagnet::FindBestMagnet( CBaseEntity *pNPC )
{
	CRagdollMagnet	*pMagnet = nullptr;
	CRagdollMagnet	*pBestMagnet;

	float			flClosestDist;

	// Assume we won't find one.
	pBestMagnet = nullptr;
	flClosestDist = std::numeric_limits<float>::max();
	
	do
	{
		pMagnet = (CRagdollMagnet *)gEntList.FindEntityByClassname( pMagnet, "phys_ragdollmagnet" );

		if( pMagnet && pMagnet->IsEnabled() )
		{
			if( pMagnet->m_target != NULL_STRING )
			{
				// if this magnet has a target, only affect that target!
				if( pNPC->GetEntityName() == pMagnet->m_target )
				{
					return pMagnet;
				}
				else
				{
					continue;
				}
			}

			float flDist;
			flDist = pMagnet->DistToPoint( pNPC->WorldSpaceCenter() );

			if( flDist < flClosestDist && flDist <= pMagnet->GetRadius() )
			{
				// This is the closest magnet that can pull this npc.
				flClosestDist = flDist;
				pBestMagnet = pMagnet;
			}
		}

	} while( pMagnet );

	return pBestMagnet;
}

//-----------------------------------------------------------------------------
// Purpose: Get the force that we should add to this NPC's ragdoll.
// Input  : *pNPC - 
// Output : Vector
//
// NOTE: This function assumes pNPC is within this magnet's radius.
//-----------------------------------------------------------------------------
Vector CRagdollMagnet::GetForceVector( CBaseEntity *pNPC )
{
	Vector vecForceToApply;

	if( IsBarMagnet() )
	{
		CPlane axis;
		Vector vecForceDir;
		Vector vecClosest;

		CalcClosestPointOnLineSegment( pNPC->WorldSpaceCenter(), GetAbsOrigin(), m_axis, vecClosest, nullptr );

		vecForceDir = (vecClosest - pNPC->WorldSpaceCenter() );
		VectorNormalize( vecForceDir );

		vecForceToApply = vecForceDir * m_force;
	}
	else
	{
		Vector vecForce;

		vecForce = GetAbsOrigin() - pNPC->WorldSpaceCenter();
		VectorNormalize( vecForce );

		vecForceToApply = vecForce * m_force;
	}

	if( ai_debug_ragdoll_magnets.GetBool() )
	{
		IPhysicsObject *pPhysObject;

		pPhysObject = pNPC->VPhysicsGetObject();

		if( pPhysObject )
		{
			Msg("Ragdoll magnet adding %f inches/sec to %s\n", m_force/pPhysObject->GetMass(), pNPC->GetClassname() );
		}
	}

	return vecForceToApply;
}

//-----------------------------------------------------------------------------
// Purpose: How far away is this point? This is different for point and bar magnets
// Input  : &vecPoint - the point
// Output : float - the dist
//-----------------------------------------------------------------------------
float CRagdollMagnet::DistToPoint( const Vector &vecPoint )
{
	if( IsBarMagnet() )
	{
		// I'm a bar magnet, so the point's distance is really the plane constant.
		// A bar magnet is a cylinder who's length is AbsOrigin() to m_axis, and whose
		// diameter is m_radius.

		// first we build two planes. The TOP and BOTTOM planes.
		// the idea is that vecPoint must be on the right side of both
		// planes to be affected by this particular magnet.
		// TOP and BOTTOM planes can be visualized as the 'caps' of the cylinder
		// that describes the bar magnet, and they point towards each other.
		// We're making sure vecPoint is between the caps.
		Vector vecAxis;

		vecAxis = GetAxisVector();
		VectorNormalize( vecAxis );

		CPlane top, bottom;

		bottom.InitializePlane( -vecAxis, m_axis );
		top.InitializePlane( vecAxis, GetAbsOrigin() );

		if( top.PointInFront( vecPoint ) && bottom.PointInFront( vecPoint ) )
		{
			// This point is between the two caps, so calculate the distance
			// of vecPoint from the axis of the bar magnet
			CPlane axis;
			Vector vecUp;
			Vector vecRight;

			// Horizontal and Vertical distances.
			float hDist, vDist;

			// Need to get a vector that's right-hand to m_axis
			VectorVectors( vecAxis, vecRight, vecUp );
			
			//CrossProduct( vecAxis, vecUp, vecRight );
			//VectorNormalize( vecRight );
			//VectorNormalize( vecUp );

			// Set up the plane to measure horizontal dist.
			axis.InitializePlane( vecRight, GetAbsOrigin() );
			hDist = fabs( axis.PointDist( vecPoint ) );

			axis.InitializePlane( vecUp, GetAbsOrigin() );
			vDist = fabs( axis.PointDist( vecPoint ) );

			return std::max( hDist, vDist );
		}
		else
		{
			return std::numeric_limits<float>::max();
		}
	}
	else
	{
		// I'm a point magnet. Just return dist
		return ( GetAbsOrigin() - vecPoint ).Length();
	}
}

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: System to generate events as specified entities become visible to players.
//			This contains some specific early-outs and culling code, so it's not
//			exactly general purpose yet. (sjb)

ConVar debug_visibility_monitor(side_registry(), "debug_visibility_monitor", "0", FCVAR_CHEAT );
ConVar vismon_poll_frequency(side_registry(), "vismon_poll_frequency", ".5", FCVAR_CHEAT );
ConVar vismon_trace_limit(side_registry(), "vismon_trace_limit", "12", FCVAR_CHEAT );

//=============================================================================
// This structure packages up an entity for the visibility monitor.
//=============================================================================
static constexpr auto NO_VISIBILITY_MEMORY = 0;
struct visibility_target_t
{
	EHANDLE						entity;
	float						minDistSqr;
	int							memory;// A bit vector that remembers which clients have seen this thing already.
	bool						bNotVisibleThroughGlass;
	VisibilityMonitorCallback	pfnCallback;
	VisibilityMonitorEvaluator  pfnEvaluator;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CVisibilityMonitor : public CAutoGameSystemPerFrame
{
public:
	CVisibilityMonitor( char const *name ) : CAutoGameSystemPerFrame( name )
	{
		m_flPollFrequency = 1.0f;
		m_flTimeNextPoll = 0.0f;
	}

	// Methods of IGameSystem
	virtual char const *Name() { return "VisibilityMonitor"; }

	virtual bool Init();
	virtual void FrameUpdatePostEntityThink();
	virtual bool EntityIsVisibleToPlayer( const visibility_target_t &target, CBasePlayer *pPlayer, int *numTraces );
	virtual void Shutdown();

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();

	// Visibility Monitor Methods
	void AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator, bool bNotVisibleThroughGlass = false );
	void RemoveEntity( CBaseEntity *pEntity );

	bool IsTrackingEntity( CBaseEntity *pEntity );

private:
	std::vector< visibility_target_t >	m_Entities;
	float								m_flPollFrequency;
	float								m_flTimeNextPoll;

	int									m_iMaxTracesPerThink;
	int									m_iMaxEntitiesPerThink;

	int									m_iStartElement;
};

//=========================================================
// Auto game system instantiation
//=========================================================
CVisibilityMonitor VisibilityMonitor( "CVisibilityMonitor" );

//---------------------------------------------------------
//---------------------------------------------------------
bool CVisibilityMonitor::Init()
{
	return true;
}

//---------------------------------------------------------
// Purpose: See if it is time to poll and do so. 
//
// SOON: We need to load-balance this. 
//---------------------------------------------------------
void CVisibilityMonitor::FrameUpdatePostEntityThink()
{
	if( gpGlobals->curtime < m_flTimeNextPoll )
		return;

	m_flTimeNextPoll = gpGlobals->curtime + vismon_poll_frequency.GetFloat();

	int iDebugging = debug_visibility_monitor.GetInt();

	if( std::ssize( m_Entities ) > m_iMaxEntitiesPerThink )
		m_iMaxEntitiesPerThink = std::ssize( m_Entities );

	if( iDebugging > 1 )
	{
		Msg("\nVisMon: Polling now. (Frequency: %f)\n", m_flPollFrequency );
		Msg("VisMon: Time: %f - Tracking %d Entities. (Max:%d)\n", gpGlobals->curtime, (int)std::ssize( m_Entities ), m_iMaxEntitiesPerThink );
	}

	// Cleanup, dump entities that have been removed since we last polled.
	for ( int i = 0 ; i < std::ssize( m_Entities ) ; i++ )
	{
		if ( m_Entities[i].entity == nullptr )
		{
			ks::swap_remove( m_Entities, i );
			if ( i >= std::ssize( m_Entities ) )
			{
				break;
			}
		}
	}

	int numTraces = 0;
	bool bHitTraceLimit = false;

	if( m_iStartElement >= std::ssize( m_Entities ) )
	{
		if( iDebugging > 1 )
		{
			Msg("VisMon: RESET\n");
		}

		m_iStartElement = 0;
	}

	if( iDebugging > 1 )
	{
		Msg("VisMon: Starting at element: %d\n", m_iStartElement );
	}

	for( int i = m_iStartElement ; i < std::ssize( m_Entities ) ; i++ )
	{
		for( int j = 1 ; j <= gpGlobals->maxClients ; j++ )
		{
			CBasePlayer *pPlayer =UTIL_PlayerByIndex( j );

			if( pPlayer != nullptr && pPlayer->IsAlive() && !pPlayer->IsBot() )
			{
				int memoryBit = 1 << j; // The bit that is used to remember whether a given entity has been seen by a given player.

				CBaseEntity *pSeeEntity = m_Entities[ i ].entity.Get();

				if( pSeeEntity == nullptr )
				{
					continue;
				}

				if( !(m_Entities[i].memory & memoryBit) )
				{
					// If this player hasn't seen this entity yet, check it.
					if( EntityIsVisibleToPlayer( m_Entities[i], pPlayer, &numTraces ) )
					{
						bool bIgnore = false;

						if( m_Entities[i].pfnEvaluator != nullptr && !m_Entities[i].pfnEvaluator( m_Entities[i].entity, pPlayer ) )
						{
							bIgnore = true;
						}

						// See it! Generate our event.
						if( iDebugging > 0 )
						{
							if( bIgnore )
							{
								Msg("VisMon: Player %s IGNORING VISIBILE Entity: %s\n", pPlayer->GetDebugName(), pSeeEntity->GetDebugName() );
								NDebugOverlay::Cross3D( pSeeEntity->WorldSpaceCenter(), 16, 255, 0, 0, false, 10.0f );
							}
							else
							{
								Msg("VisMon: Player %s sees Entity: %s\n", pPlayer->GetDebugName(), pSeeEntity->GetDebugName() );
								NDebugOverlay::Cross3D( pSeeEntity->WorldSpaceCenter(), 16, 0, 255, 0, false, 10.0f );
							}
						}

						if( !bIgnore )
						{
							bool bGenerateEvent = true;

							if( m_Entities[i].pfnCallback != nullptr )
							{
								// Make the callback, and let it determine whether to generate the simple event.
								bGenerateEvent = m_Entities[i].pfnCallback( m_Entities[i].entity, pPlayer );
							}

							if( bGenerateEvent )
							{
								// No callback, generate the generic game event.
								IGameEvent * event = gameeventmanager->CreateEvent( "entity_visible" );
								if ( event )
								{
									event->SetInt( "userid", pPlayer->GetUserID() );
									event->SetInt( "subject", pSeeEntity->entindex() );
									event->SetString( "classname", pSeeEntity->GetClassname() );
									event->SetString( "entityname", STRING( pSeeEntity->GetEntityName() ) );
									gameeventmanager->FireEvent( event );
								}
							}

							// Remember that this entity was visible to the player
							m_Entities[i].memory |= memoryBit;
						}
					}
				}
			}
		}

		if( numTraces >= vismon_trace_limit.GetInt() )
		{
			if( iDebugging > 1 )
			{
				Msg("VisMon: MAX Traces. Stopping after element %d\n", i );
			}

			m_iStartElement = i + 1; // Pick up here next think.
			bHitTraceLimit = true;
			break;
		}
	}

	if( !bHitTraceLimit )
	{
		m_iStartElement = 0;
	}

	if( numTraces > m_iMaxTracesPerThink )
		m_iMaxTracesPerThink = numTraces;

	if( iDebugging > 1 )
	{
		Msg("VisMon: %d traces performed during this polling cycle (Max: %d)\n\n", numTraces, m_iMaxTracesPerThink );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CVisibilityMonitor::EntityIsVisibleToPlayer( const visibility_target_t &target, CBasePlayer *pPlayer, int *numTraces )
{
	// if it's both invisible and not solid or interactive, we don't see it
	if ( target.entity->m_nRenderMode == kRenderNone && !target.entity->IsSolidFlagSet( FSOLID_TRIGGER ) )
	{
		// It's invisible
		return false;
	}

	CBaseCombatCharacter *pEyeEntity = pPlayer->ActivePlayerCombatCharacter();

	Vector vecTargetOrigin = target.entity->WorldSpaceCenter();
	Vector vecPlayerOrigin = pEyeEntity->EyePosition();

	float flDistSqr = vecPlayerOrigin.DistToSqr( vecTargetOrigin );

	if( flDistSqr <= target.minDistSqr )
	{
		// Increment the counter of traces done during this polling cycle
		*numTraces += 1;

		int mask = MASK_BLOCKLOS_AND_NPCS & ~CONTENTS_BLOCKLOS;
		if ( target.bNotVisibleThroughGlass )
		{
			mask |= CONTENTS_WINDOW;
		}

		trace_t tr;
		UTIL_TraceLine( vecPlayerOrigin, vecTargetOrigin, mask, pEyeEntity, COLLISION_GROUP_NONE, &tr );

		if( tr.fraction == 1.0f || tr.Ent<CBaseEntity>() == target.entity )
			return true;

		if( debug_visibility_monitor.GetInt() > 1 )
		{
			NDebugOverlay::Line( vecPlayerOrigin, vecTargetOrigin, 255, 0, 0, false, vismon_poll_frequency.GetFloat() );
		}
	}

	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::Shutdown()
{
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::LevelInitPreEntity()
{
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::LevelInitPostEntity()
{
	m_iMaxTracesPerThink = 0;
	m_iMaxEntitiesPerThink = 0;
	m_iStartElement = 0;
	m_flTimeNextPoll = gpGlobals->curtime + vismon_poll_frequency.GetFloat();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::LevelShutdownPreEntity()
{
	m_Entities.clear();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator, bool bNotVisibleThroughGlass )
{
	contract_assert( pEntity != nullptr );

	if( !IsTrackingEntity( pEntity ) )
	{
		visibility_target_t newTarget;

		newTarget.entity = pEntity;
		newTarget.minDistSqr = Square(flMinDist);
		newTarget.memory = NO_VISIBILITY_MEMORY;
		newTarget.pfnCallback = pfnCallback;
		newTarget.pfnEvaluator = pfnEvaluator;
		newTarget.bNotVisibleThroughGlass = bNotVisibleThroughGlass;

		m_Entities.push_back( newTarget );

		if( debug_visibility_monitor.GetBool() )
		{
			Msg("VisMon: Added Entity: %s (%s)\n", pEntity->GetClassname(), pEntity->GetDebugName() );
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::RemoveEntity( CBaseEntity *pEntity )
{
	contract_assert( pEntity != nullptr );

	for( int i = 0 ; i < std::ssize( m_Entities ) ; i++ )
	{
		if( m_Entities[i].entity == pEntity )
		{
			m_Entities.erase( m_Entities.begin() + i );

			if( debug_visibility_monitor.GetBool() )
			{
				Msg("VisMon: Removed Entity: %s (%s)\n", pEntity->GetClassname(), pEntity->GetDebugName() );
			}

			return;
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CVisibilityMonitor::IsTrackingEntity( CBaseEntity *pEntity )
{
	contract_assert( pEntity != nullptr );

	for( int i = 0 ; i < std::ssize( m_Entities ) ; i++ )
	{
		if( m_Entities[i].entity == pEntity )
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------
// Purpose: Adds an entity to the list of entities that 
// the visibility monitor is tracking.
//---------------------------------------------------------
void VisibilityMonitor_AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator )
{
	VisibilityMonitor.AddEntity( pEntity, flMinDist, pfnCallback, pfnEvaluator );
}

void VisibilityMonitor_AddEntity_NotVisibleThroughGlass( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator )
{
	VisibilityMonitor.AddEntity( pEntity, flMinDist, pfnCallback, pfnEvaluator, true );
}

//---------------------------------------------------------
// Purpose: Remove (stop looking for) this entity
//---------------------------------------------------------
void VisibilityMonitor_RemoveEntity( CBaseEntity *pEntity )
{
	VisibilityMonitor.RemoveEntity( pEntity );
}



class CBaseAnimating;
class CTakeDamageInfo;

using namespace ::ks::reflect;

Activity GetDeathPoseActivity( CBaseAnimating *entity, const CTakeDamageInfo &info )
{
	if ( !entity )
	{
		return ACT_INVALID;
	}

	// Get the entity's current activity.
	Activity aCurrentActivity = entity->GetSequenceActivity( entity->GetSequence() );

	// Determine if the enitiy is crouched or not
	bool bCrouched = false;
	switch ( aCurrentActivity )
	{
		case ACT_CROUCHIDLE:
		case ACT_RUN_CROUCH:
			bCrouched = true;
			break;
	}

	Activity aActivity;

	Vector vForward, vRight;
	entity->GetVectors( &vForward, &vRight, nullptr );

	Vector vDir = -info.GetDamageForce();
	VectorNormalize( vDir );

	float flDotForward	= DotProduct( vForward, vDir );
	float flDotRight	= DotProduct( vRight, vDir );

	bool bNegativeForward = false;
	bool bNegativeRight = false;

	if ( flDotForward < 0.0f )
	{
		bNegativeForward = true;
		flDotForward = flDotForward * -1;
	}

	if ( flDotRight < 0.0f )
	{
		bNegativeRight = true;
		flDotRight = flDotRight * -1;
	}

	if ( flDotRight > flDotForward )
	{
		if ( bNegativeRight == true )
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_LEFTSIDE;
			else
				aActivity = ACT_DIE_LEFTSIDE;
		}
		else 
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_RIGHTSIDE;
			else
				aActivity = ACT_DIE_RIGHTSIDE;
		}
	}
	else
	{
		if ( bNegativeForward == true )
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_BACKSIDE;
			else
				aActivity = ACT_DIE_BACKSIDE;
		}
		else 
		{
			if ( bCrouched == true )
				aActivity = ACT_DIE_CROUCH_FRONTSIDE;
			else
				aActivity = ACT_DIE_FRONTSIDE;
		}
	}

	return aActivity;
}

void SelectDeathPoseActivityAndFrame( CBaseAnimating *entity, const CTakeDamageInfo &info, int hitgroup, Activity& activity, int& frame )
{
	activity = ACT_INVALID;
	frame = 0;

	if ( !entity->GetModelPtr() )
		return;

	activity = GetDeathPoseActivity( entity, info );
	frame = DEATH_FRAME_HEAD;

	switch( hitgroup )
	{
		//Do normal ragdoll stuff if no specific hitgroup was hit.
		case HITGROUP_GENERIC:
		default:
		{
			return;
		}
			
		case HITGROUP_HEAD:
		{
			frame = DEATH_FRAME_HEAD;
			break;
		}
		case HITGROUP_LEFTARM:
			frame = DEATH_FRAME_LEFTARM;
			break;

		case HITGROUP_RIGHTARM:
			frame = DEATH_FRAME_RIGHTARM;
			break;

		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			frame = DEATH_FRAME_STOMACH;
			break;
	
		case HITGROUP_LEFTLEG:
			frame = DEATH_FRAME_LEFTLEG;
			break;

		case HITGROUP_RIGHTLEG:
			frame = DEATH_FRAME_RIGHTLEG;
			break;
	}
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Utility functions for using debug overlays to visualize information
//			in the world.  Uses the IVDebugOverlay interface.

static constexpr auto MAX_OVERLAY_DIST_SQR = 90000000;

//-----------------------------------------------------------------------------
// Purpose: Local player on the server or client
//-----------------------------------------------------------------------------
CBasePlayer *GetLocalPlayer( void )
{
	return UTIL_GetListenServerHost();
}

//-----------------------------------------------------------------------------
// Purpose: Debug player by index
//-----------------------------------------------------------------------------
CBasePlayer *GetDebugPlayer( void )
{
	return UTIL_PlayerByIndex(CBaseEntity::m_nDebugPlayer);
}

//-----------------------------------------------------------------------------
// Purpose: Draw a capsule
//-----------------------------------------------------------------------------
void NDebugOverlay::Capsule(const Vector &vStart, const Vector &vEnd, const float &flRadius, int r, int g, int b, int a, float flDuration)
{
	if ( debugoverlay )
	{
		debugoverlay->AddCapsuleOverlay( vStart, vEnd, flRadius, r, g, b, a, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw a box with no orientation
//-----------------------------------------------------------------------------
void NDebugOverlay::Box(const Vector &origin, const Vector &mins, const Vector &maxs, int r, int g, int b, int a, float flDuration)
{
	BoxAngles( origin, mins, maxs, vec3_angle, r, g, b, a, flDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Draw box oriented to a Vector direction
//-----------------------------------------------------------------------------
void NDebugOverlay::BoxDirection(const Vector &origin, const Vector &mins, const Vector &maxs, const Vector &orientation, int r, int g, int b, int a, float duration)
{
	// convert forward vector to angles
	QAngle f_angles = vec3_angle;
	f_angles.y = UTIL_VecToYaw( orientation );

	BoxAngles( origin, mins, maxs, f_angles, r, g, b, a, duration );
}

//-----------------------------------------------------------------------------
// Purpose: Draw box oriented to a QAngle direction
//-----------------------------------------------------------------------------
void NDebugOverlay::BoxAngles(const Vector &origin, const Vector &mins, const Vector &maxs, const QAngle &angles, int r, int g, int b, int a, float duration)
{
	if ( debugoverlay )
	{
		debugoverlay->AddBoxOverlay( origin, mins, maxs, angles, r, g, b, a, duration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a swept box
//-----------------------------------------------------------------------------
void NDebugOverlay::SweptBox( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, const QAngle & angles, int r, int g, int b, int a, float flDuration)
{
	if ( debugoverlay )
	{
		debugoverlay->AddSweptBoxOverlay( start, end, mins, maxs, angles, r, g, b, a, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a box around an entity
//-----------------------------------------------------------------------------
void NDebugOverlay::EntityBounds( const CBaseEntity *pEntity, int r, int g, int b, int a, float flDuration )
{
	const CCollisionProperty *pCollide = pEntity->CollisionProp();
	BoxAngles( pCollide->GetCollisionOrigin(), pCollide->OBBMins(), pCollide->OBBMaxs(), pCollide->GetCollisionAngles(), r, g, b, a, flDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Find the players view position and forward position
//-----------------------------------------------------------------------------

static bool GetPlayerView( Vector &vecPosition, Vector &vecForward )
{
	CBasePlayer *player = GetLocalPlayer();

	if ( !player )
	{
		vecPosition = vecForward = Vector( 0, 0, 0 );
		return false;
	}

	player->EyePositionAndVectors( &vecPosition, &vecForward, nullptr, nullptr );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Draws a line from one position to another
//-----------------------------------------------------------------------------
void NDebugOverlay::Line( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration )
{
	// --------------------------------------------------------------
	// Clip the line before sending so we 
	// don't overflow the client message buffer
	// --------------------------------------------------------------
	CBasePlayer *player = GetLocalPlayer();

	if ( player == nullptr )
		return;

/*
	Assert( GameRules() );
	if ( !GameRules()->IsTopDown() )
	{
		// Clip line that is far away	
		if (((player->GetAbsOrigin() - origin).LengthSqr() > MAX_OVERLAY_DIST_SQR) &&
			((player->GetAbsOrigin() - target).LengthSqr() > MAX_OVERLAY_DIST_SQR) ) 
			return;

		// Clip line that is behind the client 
		Vector clientForward;
		player->EyeVectors( &clientForward );

		Vector toOrigin		= origin - player->GetAbsOrigin();
		Vector toTarget		= target - player->GetAbsOrigin();
 		float  dotOrigin	= DotProduct(clientForward,toOrigin);
 		float  dotTarget	= DotProduct(clientForward,toTarget);
		
		if (dotOrigin < 0 && dotTarget < 0) 
			return;
	}
*/

	if ( debugoverlay )
	{
		debugoverlay->AddLineOverlay( origin, target, r, g, b, noDepthTest, duration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw triangle
//-----------------------------------------------------------------------------
void NDebugOverlay::Triangle( const Vector &p1, const Vector &p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float duration )
{
	Vector clipOrigin, clipForward;

	if ( !GetPlayerView( clipOrigin, clipForward ) )
		return;

	contract_assert( GameRules() );
	if ( !GameRules()->IsTopDown() )
	{
		// Clip triangles that are far away
		Vector to1 = p1 - clipOrigin;
		Vector to2 = p2 - clipOrigin;
		Vector to3 = p3 - clipOrigin;

		if ((to1.LengthSqr() > MAX_OVERLAY_DIST_SQR) && 
			(to2.LengthSqr() > MAX_OVERLAY_DIST_SQR) && 
			(to3.LengthSqr() > MAX_OVERLAY_DIST_SQR))
		{
			return;
		}

		// Clip triangles that are behind the client 
 		float  dot1 = DotProduct(clipForward, to1);
 		float  dot2 = DotProduct(clipForward, to2);
 		float  dot3 = DotProduct(clipForward, to3);

		if (dot1 < 0 && dot2 < 0 && dot3 < 0) 
			return;
	}

	if ( debugoverlay )
	{
		debugoverlay->AddTriangleOverlay( p1, p2, p3, r, g, b, a, noDepthTest, duration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw entity text overlay
//-----------------------------------------------------------------------------
void NDebugOverlay::EntityText( int entityID, int text_offset, const char *text, float duration, int r, int g, int b, int a )
{
	if ( debugoverlay )
	{
		debugoverlay->AddEntityTextOverlay( entityID, text_offset, duration, 
			(int)clamp(r * 255.f,0.f,255.f), (int)clamp(g * 255.f,0.f,255.f), (int)clamp(b * 255.f,0.f,255.f), 
			(int)clamp(a * 255.f,0.f,255.f), text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw entity text overlay at a specific position
//-----------------------------------------------------------------------------
void NDebugOverlay::EntityTextAtPosition( const Vector &origin, int text_offset, const char *text, float duration, int r, int g, int b, int a )
{
	if ( debugoverlay )
	{
		debugoverlay->AddTextOverlayRGB( origin, text_offset, duration, r, g, b, a, "%s", text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add grid overlay 
//-----------------------------------------------------------------------------
void NDebugOverlay::Grid( const Vector &vPosition )
{
	if ( debugoverlay )
	{
		debugoverlay->AddGridOverlay( vPosition );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw debug text at a position
//-----------------------------------------------------------------------------
void NDebugOverlay::Text( const Vector &origin, const char *text, bool bViewCheck, float duration )
{
	Vector clipOrigin, clipForward;

	if ( !GetPlayerView( clipOrigin, clipForward ) )
		return;

	contract_assert( GameRules() );
	if ( !GameRules()->IsTopDown() )
	{
		// Clip text that is far away
		if ( ( clipOrigin - origin ).LengthSqr() > MAX_OVERLAY_DIST_SQR ) 
			return;

		// Clip text that is behind the client 
		Vector toText	= origin - clipOrigin;
 		float  dotPr	= DotProduct( clipForward, toText );
		
		if (dotPr < 0) 
			return;

		// Clip text that is obscured
		if (bViewCheck)
		{
			trace_t tr;
			UTIL_TraceLine( clipOrigin, origin, MASK_OPAQUE, nullptr, COLLISION_GROUP_NONE, &tr);
			
			if ((tr.endpos - origin).Length() > 10)
				return;
		}
	}

	if ( debugoverlay )
	{
		debugoverlay->AddTextOverlay( origin, duration, "%s", text );
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Add debug overlay text with screen position
//-----------------------------------------------------------------------------
void NDebugOverlay::ScreenText( float flXpos, float flYpos, const char *text, int r, int g, int b, int a, float duration )
{
	if ( debugoverlay )
	{
		debugoverlay->AddScreenTextOverlay( flXpos, flYpos, duration, r, g, b, a, text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw a colored 3D cross of the given hull size at the given position
//-----------------------------------------------------------------------------
void NDebugOverlay::Cross3D(const Vector &position, const Vector &mins, const Vector &maxs, int r, int g, int b, bool noDepthTest, float fDuration )
{
	Vector start = mins + position;
	Vector end   = maxs + position;
	Line(start,end, r, g, b, noDepthTest,fDuration);

	start.x += (maxs.x - mins.x);
	end.x	-= (maxs.x - mins.x);
	Line(start,end, r, g, b, noDepthTest,fDuration);

	start.y += (maxs.y - mins.y);
	end.y	-= (maxs.y - mins.y);
	Line(start,end, r, g, b, noDepthTest,fDuration);

	start.x -= (maxs.x - mins.x);
	end.x	+= (maxs.x - mins.x);
	Line(start,end, r, g, b, noDepthTest,fDuration);
}

//-----------------------------------------------------------------------------
// Purpose: Draw a colored 3D cross of the given size at the given position
//-----------------------------------------------------------------------------
void NDebugOverlay::Cross3D(const Vector &position, float size, int r, int g, int b, bool noDepthTest, float flDuration )
{
	Line( position + Vector(size,0,0), position - Vector(size,0,0), r, g, b, noDepthTest, flDuration );
	Line( position + Vector(0,size,0), position - Vector(0,size,0), r, g, b, noDepthTest, flDuration );
	Line( position + Vector(0,0,size), position - Vector(0,0,size), r, g, b, noDepthTest, flDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Draw an oriented, colored 3D cross of the given size at the given position (via a vector)
//-----------------------------------------------------------------------------
void NDebugOverlay::Cross3DOriented( const Vector &position, const QAngle &angles, float size, int r, int g, int b, bool noDepthTest, float flDuration )
{
	Vector forward, right, up;
	AngleVectors( angles, &forward, &right, &up );

	forward *= size;
	right *= size;
	up *= size;

	Line( position + right, position - right, r, g, b, noDepthTest, flDuration );
	Line( position + forward, position - forward, r, g, b, noDepthTest, flDuration );
	Line( position + up, position - up, r, g, b, noDepthTest, flDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Draw an oriented, colored 3D cross of the given size at the given position (via a matrix)
//-----------------------------------------------------------------------------
void NDebugOverlay::Cross3DOriented( const matrix3x4_t &m, float size, int c, bool noDepthTest, float flDuration )
{
	Vector forward, left, up, position;

	MatrixGetColumn( m, 0, forward );
	MatrixGetColumn( m, 1, left );
	MatrixGetColumn( m, 2, up );
	MatrixGetColumn( m, 3, position );

	forward *= size;
	left *= size;
	up *= size;

	Line( position + left, position - left, 0, c, 0, noDepthTest, flDuration );
	Line( position + forward, position - forward, c, 0, 0, noDepthTest, flDuration );
	Line( position + up, position - up, 0, 0, c, noDepthTest, flDuration );
}

//--------------------------------------------------------------------------------
// Purpose : Draw tick marks between start and end position of the given distance
//			 with text every tickTextDist steps apart. 
//--------------------------------------------------------------------------------
void NDebugOverlay::DrawTickMarkedLine(const Vector &startPos, const Vector &endPos, float tickDist, int tickTextDist, int r, int g, int b, bool noDepthTest, float duration )
{
	CBasePlayer* pPlayer = GetDebugPlayer();

	if ( !pPlayer ) 
		return;
	
	Vector	lineDir		= (endPos - startPos);
	float	lineDist	= VectorNormalize( lineDir );
	int		numTicks	= lineDist/tickDist;
	Vector	vBodyDir;
	
	vBodyDir = pPlayer->BodyDirection2D( );

	Vector  upVec		= 4*vBodyDir;
	Vector	sideDir;
	Vector	tickPos		= startPos;
	int		tickTextCnt = 0;

	CrossProduct(lineDir, upVec, sideDir);

	// First draw the line
	Line(startPos, endPos, r,g,b,noDepthTest,duration);

	// Now draw the ticks
	for (int i=0;i<numTicks+1;i++)
	{
		// Draw tick mark
		Vector tickLeft	 = tickPos - sideDir;
		Vector tickRight = tickPos + sideDir;
		
		// Draw tick mark text
		if (tickTextCnt == tickTextDist)
		{
			char text[25];
			V_snprintf(text,sizeof(text),"%i",i);
			Vector textPos = tickLeft + Vector(0,0,8);
			Line(tickLeft, tickRight, 255,255,255,noDepthTest,duration);
			Text( textPos, text, true, 0 );
			tickTextCnt = 0;
		}
		else
		{
			Line(tickLeft, tickRight, r,g,b,noDepthTest,duration);
		}
		
		tickTextCnt++;

		tickPos = tickPos + (tickDist * lineDir);
	}
}

//------------------------------------------------------------------------------
// Purpose : Draw crosshair on ground where player is looking
//------------------------------------------------------------------------------
void NDebugOverlay::DrawGroundCrossHairOverlay( void )
{
	CBasePlayer* pPlayer = GetDebugPlayer();

	if ( !pPlayer ) 
		return;

	// Trace a line to where player is looking
	Vector vForward;
	Vector vSource = pPlayer->EyePosition();
	pPlayer->EyeVectors( &vForward );

	trace_t tr;
	UTIL_TraceLine ( vSource, vSource + vForward * 2048, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr);
	float dotPr = DotProduct(Vector(0,0,1),tr.plane.normal);
	if (tr.fraction != 1.0 &&  dotPr > 0.5)
	{
		tr.endpos.z += 1;
		float	scale	 = 6;
		Vector	startPos = tr.endpos + Vector (-scale,0,0);
		Vector	endPos	 = tr.endpos + Vector ( scale,0,0);
		Line(startPos, endPos, 255, 0, 0,false,0);

		startPos = tr.endpos + Vector (0,-scale,0);
		endPos	 = tr.endpos + Vector (0, scale,0);
		Line(startPos, endPos, 255, 0, 0,false,0);
	}
}

//--------------------------------------------------------------------------------
// Purpose : Draw a horizontal arrow pointing in the specified direction
//--------------------------------------------------------------------------------
void NDebugOverlay::HorzArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration)
{
	Vector	lineDir		= (endPos - startPos);
	VectorNormalize( lineDir );
	Vector  upVec		= Vector( 0, 0, 1 );
	Vector	sideDir;
	float   radius		= width / 2.0;

	CrossProduct(lineDir, upVec, sideDir);

	Vector p1 =	startPos - sideDir * radius;
	Vector p2 = endPos - lineDir * width - sideDir * radius;
	Vector p3 = endPos - lineDir * width - sideDir * width;
	Vector p4 = endPos;
	Vector p5 = endPos - lineDir * width + sideDir * width;
	Vector p6 = endPos - lineDir * width + sideDir * radius;
	Vector p7 =	startPos + sideDir * radius;

	// Outline the arrow
	Line(p1, p2, r,g,b,noDepthTest,flDuration);
	Line(p2, p3, r,g,b,noDepthTest,flDuration);
	Line(p3, p4, r,g,b,noDepthTest,flDuration);
	Line(p4, p5, r,g,b,noDepthTest,flDuration);
	Line(p5, p6, r,g,b,noDepthTest,flDuration);
	Line(p6, p7, r,g,b,noDepthTest,flDuration);

	if ( a > 0 )
	{
		// Fill us in with triangles
		Triangle( p5, p4, p3, r, g, b, a, noDepthTest, flDuration ); // Tip
		Triangle( p1, p7, p6, r, g, b, a, noDepthTest, flDuration ); // Shaft
		Triangle( p6, p2, p1, r, g, b, a, noDepthTest, flDuration );

		// And backfaces
		Triangle( p3, p4, p5, r, g, b, a, noDepthTest, flDuration ); // Tip
		Triangle( p6, p7, p1, r, g, b, a, noDepthTest, flDuration ); // Shaft
		Triangle( p1, p2, p6, r, g, b, a, noDepthTest, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose : Draw a horizontal arrow pointing in the specified direction by yaw value
//-----------------------------------------------------------------------------
void NDebugOverlay::YawArrow( const Vector &startPos, float yaw, float length, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration)
{
	Vector forward = UTIL_YawToVector( yaw );

	HorzArrow( startPos, startPos + forward * length, width, r, g, b, a, noDepthTest, flDuration );
}

//--------------------------------------------------------------------------------
// Purpose : Draw a vertical arrow at a position
//--------------------------------------------------------------------------------
void NDebugOverlay::VertArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration)
{
	Vector	lineDir		= (endPos - startPos);
	VectorNormalize( lineDir );
	Vector  upVec;
	Vector	sideDir;
	float   radius		= width / 2.0;

	VectorVectors( lineDir, sideDir, upVec );

	Vector p1 =	startPos - upVec * radius;
	Vector p2 = endPos - lineDir * width - upVec * radius;
	Vector p3 = endPos - lineDir * width - upVec * width;
	Vector p4 = endPos;
	Vector p5 = endPos - lineDir * width + upVec * width;
	Vector p6 = endPos - lineDir * width + upVec * radius;
	Vector p7 =	startPos + upVec * radius;

	// Outline the arrow
	Line(p1, p2, r,g,b,noDepthTest,flDuration);
	Line(p2, p3, r,g,b,noDepthTest,flDuration);
	Line(p3, p4, r,g,b,noDepthTest,flDuration);
	Line(p4, p5, r,g,b,noDepthTest,flDuration);
	Line(p5, p6, r,g,b,noDepthTest,flDuration);
	Line(p6, p7, r,g,b,noDepthTest,flDuration);

	if ( a > 0 )
	{
		// Fill us in with triangles
		Triangle( p5, p4, p3, r, g, b, a, noDepthTest, flDuration ); // Tip
		Triangle( p1, p7, p6, r, g, b, a, noDepthTest, flDuration ); // Shaft
		Triangle( p6, p2, p1, r, g, b, a, noDepthTest, flDuration );

		// And backfaces
		Triangle( p3, p4, p5, r, g, b, a, noDepthTest, flDuration ); // Tip
		Triangle( p6, p7, p1, r, g, b, a, noDepthTest, flDuration ); // Shaft
		Triangle( p1, p2, p6, r, g, b, a, noDepthTest, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw an axis
//-----------------------------------------------------------------------------
void NDebugOverlay::Axis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration )
{
	Vector xvec, yvec, zvec;
	AngleVectors( angles, &xvec, &yvec, &zvec );
	
	xvec = position + (size * xvec);
	yvec = position - (size * yvec); // Left is positive
	zvec = position + (size * zvec);

	Line( position, xvec, 255, 0, 0, noDepthTest, flDuration );
	Line( position, yvec, 0, 255, 0, noDepthTest, flDuration );
	Line( position, zvec, 0, 0, 255, noDepthTest, flDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Draw circles to suggest a sphere
//-----------------------------------------------------------------------------
void NDebugOverlay::Sphere( const Vector &center, float radius, int r, int g, int b, bool noDepthTest, float flDuration )
{
	Vector edge, lastEdge;

	float axisSize = radius;
	Line( center + Vector( 0, 0, -axisSize ), center + Vector( 0, 0, axisSize ), r, g, b, noDepthTest, flDuration );
	Line( center + Vector( 0, -axisSize, 0 ), center + Vector( 0, axisSize, 0 ), r, g, b, noDepthTest, flDuration );
	Line( center + Vector( -axisSize, 0, 0 ), center + Vector( axisSize, 0, 0 ), r, g, b, noDepthTest, flDuration );

	lastEdge = Vector( radius + center.x, center.y, center.z );
	float angle;
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = radius * std::cosf( angle / 180.0f * std::numbers::pi ) + center.x;
		edge.y = center.y;
		edge.z = radius * std::sinf( angle / 180.0f * std::numbers::pi ) + center.z;

		Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

		lastEdge = edge;
	}

	lastEdge = Vector( center.x, radius + center.y, center.z );
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = center.x;
		edge.y = radius * std::cosf( angle / 180.0f * std::numbers::pi ) + center.y;
		edge.z = radius * std::sinf( angle / 180.0f * std::numbers::pi ) + center.z;

		Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

		lastEdge = edge;
	}

	lastEdge = Vector( center.x, radius + center.y, center.z );
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = radius * std::cosf( angle / 180.0f * std::numbers::pi ) + center.x;
		edge.y = radius * std::sinf( angle / 180.0f * std::numbers::pi ) + center.y;
		edge.z = center.z;

		Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

		lastEdge = edge;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw a circle whose center is at a position, facing the camera
//-----------------------------------------------------------------------------
void NDebugOverlay::Circle( const Vector &position, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
{
	CBasePlayer *player = GetLocalPlayer();
	if ( player == nullptr )
		return;

	Vector clientForward;
	player->EyeVectors( &clientForward );

	QAngle vecAngles;
	VectorAngles( clientForward, vecAngles );
	
	Circle( position, vecAngles, radius, r, g, b, a, bNoDepthTest, flDuration );
}

//-----------------------------------------------------------------------------
// Purpose: Draw a circle whose center is at a position and is facing a specified direction
//-----------------------------------------------------------------------------
void NDebugOverlay::Circle( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
{
	// Setup our transform matrix
	matrix3x4_t xform;
	AngleMatrix( angles, position, xform );
	Vector xAxis, yAxis;
	// default draws circle in the y/z plane
	MatrixGetColumn( xform, 2, xAxis );
	MatrixGetColumn( xform, 1, yAxis );
	Circle( position, xAxis, yAxis, radius, r, g, b, a, bNoDepthTest, flDuration );
}

void NDebugOverlay::Circle( const Vector &position, const Vector &xAxis, const Vector &yAxis, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
{
	const unsigned int nSegments = 16;
	const float flRadStep = (std::numbers::pi*2.0f) / (float) nSegments;

	Vector vecLastPosition;
	
	// Find our first position
	// Retained for triangle fanning
	Vector vecStart = position + xAxis * radius;
	Vector vecPosition = vecStart;

	// Draw out each segment (fanning triangles if we have an alpha amount)
	for ( int i = 1; i <= nSegments; i++ )
	{
		// Store off our last position
		vecLastPosition = vecPosition;

		// Calculate the new one
		float flSin, flCos;
		SinCos( flRadStep*i, &flSin, &flCos );
		vecPosition = position + (xAxis * flCos * radius) + (yAxis * flSin * radius);

		// Draw the line
		Line( vecLastPosition, vecPosition, r, g, b, bNoDepthTest, flDuration );

		// If we have an alpha value, then draw the fan
		if ( a && i > 1 )
		{		
			debugoverlay->AddTriangleOverlay( vecStart, vecLastPosition, vecPosition, r, g, b, a, bNoDepthTest, flDuration );
		}
	}
}

void NDebugOverlay::Sphere( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
{
	// Setup our transform matrix
	matrix3x4_t xform;
	AngleMatrix( angles, position, xform );
	Vector xAxis, yAxis, zAxis;
	// default draws circle in the y/z plane
	MatrixGetColumn( xform, 0, xAxis );
	MatrixGetColumn( xform, 1, yAxis );
	MatrixGetColumn( xform, 2, zAxis );
	Circle( position, xAxis, yAxis, radius, r, g, b, a, bNoDepthTest, flDuration );	// xy plane
	Circle( position, yAxis, zAxis, radius, r, g, b, a, bNoDepthTest, flDuration );	// yz plane
	Circle( position, xAxis, zAxis, radius, r, g, b, a, bNoDepthTest, flDuration );	// xz plane
}

static constexpr auto NUM_CONE_LINES = 8;
static constexpr auto NUM_CONE_CIRCLES = 3;
void NDebugOverlay::Cone( const Vector & position, const Vector & axis, float angleRadians, float length, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
{
	//draw lines down the length of the cone.
	float radiusStep = ( std::numbers::pi * 2.0f ) / NUM_CONE_LINES;
	float sinHalfAngle = std::sinf( angleRadians * 0.5f ); 
	float finalRadius = sinHalfAngle * length;

	QAngle vecAngles;
	VectorAngles( axis, vecAngles );

	matrix3x4_t xform;
	AngleMatrix( vecAngles, position, xform );
	Vector sideAxis, upAxis;
	// default draws circle in the y/z plane
	MatrixGetColumn( xform, 2, sideAxis );
	MatrixGetColumn( xform, 1, upAxis );

	Vector coneEnd = position + ( axis * length );

	for( int lineNum = 0; lineNum < NUM_CONE_LINES; ++lineNum )
	{
		float flSin, flCos;
		SinCos( radiusStep * lineNum, & flSin, & flCos );
		Vector circlePos = coneEnd + ( sideAxis * flCos * finalRadius ) + ( upAxis * flSin * finalRadius );
		Line( position, circlePos, r, g, b, bNoDepthTest, flDuration );
	}

	// draw three cicles around the cone.
	float lengthStep = length / NUM_CONE_CIRCLES;

	for( int circNum = 0; circNum < NUM_CONE_CIRCLES; ++circNum )
	{
		float segLength = lengthStep * ( circNum + 1 );
		Vector circPos = position + ( axis * segLength );
		float circRad = sinHalfAngle * segLength;
		Circle( circPos, sideAxis, upAxis, circRad, r, g, b, 0, bNoDepthTest, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw a cross whose center is at a position, facing the camera
//-----------------------------------------------------------------------------
void NDebugOverlay::Cross( const Vector &position, float radius, int r, int g, int b, bool bNoDepthTest, float flDuration )
{
	CBasePlayer *player = GetLocalPlayer();
	if ( player == nullptr )
		return;

	Vector clientForward;
	Vector clientRight;
	Vector clientUp;
	player->EyeVectors( &clientForward, &clientRight, &clientUp );

	Line( position - radius * clientRight, position + radius * clientRight, r, g, b, bNoDepthTest, flDuration );
	Line( position - radius * clientUp, position + radius * clientUp, r, g, b, bNoDepthTest, flDuration );
}

void NDebugOverlay::PurgeTextOverlays()
{
	if ( debugoverlay )
	{
		debugoverlay->PurgeTextOverlays();
	}
}

using namespace ::ks::reflect;

#define DECAL_LIST_FILE "scripts/decals_subrect.txt"
//#define DECAL_LIST_FILE "scripts/decals.txt"
inline constexpr const char *TRANSLATION_DATA_SECTION = "TranslationData";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDecalEmitterSystem : public IDecalEmitterSystem, public CAutoGameSystem
{
public:
	CDecalEmitterSystem( char const *name ) : CAutoGameSystem( name )
	{
	}

	virtual bool		Init();
	virtual void		Shutdown();
	virtual void		LevelInitPreEntity();

	// Public interface
	virtual int			GetDecalIndexForName( char const *decalname );
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial );

private:
	char const		*ImpactDecalForGameMaterial( int gamematerial );
	void			LoadDecalsFromScript( char const *filename );
	void			Clear();

	struct DecalListEntry
	{
		DecalListEntry()
		{
			precache_index	= -1;
			weight			= 1.0f;
		}

		std::string_view	name;	// view into m_DecalFileNames
		int			precache_index;
		float		weight;
	};

	struct DecalEntry
	{
		DecalEntry()
		{
		}

		DecalEntry( const DecalEntry& src )
		{
			int c = std::ssize( src.indices );
			for ( int i = 0; i < c; i++ )
			{
				indices.push_back( src.indices[ i ] );
			}
		}

		DecalEntry& operator = ( const DecalEntry& src )
		{
			if ( this == &src )
				return *this;

			int c = std::ssize( src.indices );
			for ( int i = 0; i < c; i++ )
			{
				indices.push_back( src.indices[ i ] );
			}

			return *this;
		}

		std::vector< int >	indices;
	};

	std::vector< DecalListEntry >	m_AllDecals;
	ks::idict< DecalEntry >			m_Decals;
	ks::intern_pool					m_DecalFileNames;
	// Game material -> its decal in m_Decals; both are only cleared together.
	ks::idict< ks::idict< DecalEntry >::const_iterator >	m_GameMaterialTranslation;
};

static CDecalEmitterSystem g_DecalSystem( "CDecalEmitterSystem" );
IDecalEmitterSystem *decalsystem = &g_DecalSystem;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *decalname - 
// Output : int
//-----------------------------------------------------------------------------
int CDecalEmitterSystem::GetDecalIndexForName( char const *decalname )
{
	if ( !decalname  || !decalname[ 0 ] )
		return -1;

	DecalEntry *e = ks::find_ptr( m_Decals, decalname );
	if ( !e )
		return -1;

	contract_assert( e );
	int count = std::ssize( e->indices );
	if ( count <= 0 )
		return -1;

	float totalweight = 0.0f;
	int slot = 0;

	for ( int i = 0; i < count; i++ )
	{
		int idx = e->indices[ i ];
		DecalListEntry *item = &m_AllDecals[ idx ];
		contract_assert( item );
		
		if ( !totalweight )
		{
			slot = idx;
		}

		// Always assume very first slot will match
		totalweight += item->weight;
		if ( !totalweight || random_valve->RandomFloat(0,totalweight) < item->weight )
		{
			slot = idx;
		}
	}

	return m_AllDecals[ slot ].precache_index;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDecalEmitterSystem::Init()
{
	LoadDecalsFromScript( DECAL_LIST_FILE );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::LevelInitPreEntity()
{
	// Precache all entries
	int c = std::ssize( m_AllDecals );
	for ( int i = 0 ; i < c; i++ )
	{
		DecalListEntry& e = m_AllDecals[ i ];
		e.precache_index = engine->PrecacheDecal( e.name.data() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::LoadDecalsFromScript( char const *filename )
{
	KeyValues *kv = new KeyValues( filename );
	contract_assert( kv );
	if ( kv )
	{
		KeyValues *translation = nullptr;
		if ( kv->LoadFromFile( filesystem, filename ) )
		{
			KeyValues *p = kv;
			while ( p )
			{
				if ( p->GetFirstSubKey() )
				{
					char const *keyname = p->GetName();

					if ( !V_stricmp( keyname, TRANSLATION_DATA_SECTION ) )
					{
						translation = p;
					}
					else
					{
						DecalEntry entry;

						for ( KeyValues *sub = p->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
						{

							DecalListEntry decal;
							decal.precache_index = -1;
							decal.name = m_DecalFileNames.intern( sub->GetName() );
							decal.weight = sub->GetFloat();

							// Add to global list
							int idx = std::ssize( m_AllDecals ); m_AllDecals.push_back( decal );

							// Add index only to local list
							entry.indices.push_back( idx );
						}

						// Add entry to main dictionary; a repeated name keeps the first
						m_Decals.emplace( keyname, entry );
					}
				}
				p = p->GetNextKey();
			}
		}
		else
		{
			Msg( "CDecalEmitterSystem::LoadDecalsFromScript:  Unable to load '%s'\n", filename );
		}

		if ( !translation )
		{
			Msg( "CDecalEmitterSystem::LoadDecalsFromScript:  Script '%s' missing section '%s'\n",
				filename,
				TRANSLATION_DATA_SECTION );
		}
		else
		{
			// Now parse game material to entry translation table
			for ( KeyValues *sub = translation->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
			{
				// Don't add NULL string to list
				if ( !V_stricmp( sub->GetString(), "" ) )
					continue;

				auto itDecal = m_Decals.find( sub->GetString() );
				if ( itDecal != m_Decals.end() )
				{
					const char *value = sub->GetName();
					int gameMaterial;
					if ( !V_isdigit( value[0]) )
					{
						gameMaterial = std::toupper( value[0] );
					}
					else
					{
						gameMaterial = std::atoi( value );
					}

					char gm[ 2 ];
					gm[0] = gameMaterial;
					gm[1] = 0;
					m_GameMaterialTranslation.emplace( gm, itDecal );
				}
				else
				{
					Msg( "CDecalEmitterSystem::LoadDecalsFromScript:  Translation for game material type '%s' references unknown decal '%s'\n",
						sub->GetName(), sub->GetString() );
				}
			}
		}

		kv->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : gamematerial - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CDecalEmitterSystem::ImpactDecalForGameMaterial( int gamematerial )
{
	char gm[ 2 ];
	gm[0] = (char)gamematerial;
	gm[1] = 0;

	auto it = m_GameMaterialTranslation.find( gm );
	if ( it == m_GameMaterialTranslation.end() )
		return nullptr;

	return it->second->first.c_str();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::Shutdown()
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::Clear()
{
	m_DecalFileNames.clear();
	m_GameMaterialTranslation.clear();
	m_Decals.clear();
	m_AllDecals.clear(); m_AllDecals.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *decalName - 
//			gamematerial - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CDecalEmitterSystem::TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial )
{
	if ( gamematerial == CHAR_TEX_CONCRETE )
		return decalName;

	if ( !V_stricmp( decalName, "Impact.Concrete" ) )
	{
		if ( gamematerial == '-' )
			return "";

		char const *d = ImpactDecalForGameMaterial( gamematerial );
		if ( d )
		{
			return d;
		}
	}

	return decalName;
}

#undef DECAL_LIST_FILE
