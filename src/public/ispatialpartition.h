#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

struct Ray_t;
class IHandleEntity;


inline constexpr const char *INTERFACEVERSION_SPATIALPARTITION = "SpatialPartition001";

//-----------------------------------------------------------------------------
// These are the various partition lists. Note some are server only, some
// are client only
//-----------------------------------------------------------------------------

enum
{
	PARTITION_ENGINE_SOLID_EDICTS		= (1 << 0),		// every edict_t that isn't SOLID_TRIGGER or SOLID_NOT (and static props)
	PARTITION_ENGINE_TRIGGER_EDICTS		= (1 << 1),		// every edict_t that IS SOLID_TRIGGER
	PARTITION_CLIENT_SOLID_EDICTS		= (1 << 2),
	PARTITION_CLIENT_RESPONSIVE_EDICTS	= (1 << 3),		// these are client-side only objects that respond to being forces, etc.
	PARTITION_ENGINE_NON_STATIC_EDICTS	= (1 << 4),		// everything in solid & trigger except the static props, includes SOLID_NOTs
	PARTITION_CLIENT_STATIC_PROPS		= (1 << 5),
	PARTITION_ENGINE_STATIC_PROPS		= (1 << 6),
	PARTITION_CLIENT_NON_STATIC_EDICTS	= (1 << 7),		// everything except the static props
	PARTITION_CLIENT_TRIGGER_ENTITIES   = (1 << 8),		// client side prediction related triggers
	PARTITION_CLIENT_IK_ATTACHMENT	    = (1 << 9),		// Can be used as an IK attachment
	PARTITION_ENGINE_PUSHABLE			= (1 << 10),	// everything with a movetype that can be pushed by MOVETYPE_PUSH ents
};

// Use this to look for all client edicts.
inline constexpr auto PARTITION_ALL_CLIENT_EDICTS = (		 	PARTITION_CLIENT_NON_STATIC_EDICTS |	 	PARTITION_CLIENT_STATIC_PROPS |			 	PARTITION_CLIENT_RESPONSIVE_EDICTS |	 	PARTITION_CLIENT_SOLID_EDICTS |			 	PARTITION_CLIENT_TRIGGER_ENTITIES  	);


// These are the only handles in the spatial partition that the game is controlling (everything but static props)
// These masks are used to handle updating the dirty spatial partition list in each game DLL
inline constexpr auto PARTITION_CLIENT_GAME_EDICTS = (PARTITION_ALL_CLIENT_EDICTS & ~PARTITION_CLIENT_STATIC_PROPS);
inline constexpr auto PARTITION_SERVER_GAME_EDICTS = (PARTITION_ENGINE_SOLID_EDICTS|PARTITION_ENGINE_TRIGGER_EDICTS|PARTITION_ENGINE_NON_STATIC_EDICTS);

//-----------------------------------------------------------------------------
// Clients that want to know about all elements within a particular
// volume must inherit from this
//-----------------------------------------------------------------------------

enum IterationRetval_t
{
	ITERATION_CONTINUE = 0,
	ITERATION_STOP,
};


typedef unsigned short SpatialPartitionHandle_t;

// A combination of the PARTITION_ flags above.
typedef int SpatialPartitionListMask_t;	

typedef int SpatialTempHandle_t;


//-----------------------------------------------------------------------------
// Any search in the CSpatialPartition must use this to filter out entities it doesn't want.
// You're forced to use listMasks because it can filter by listMasks really fast. Any other
// filtering can be done by EnumElement.
//-----------------------------------------------------------------------------

class IPartitionEnumerator
{
public:
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity ) = 0;
};


//-----------------------------------------------------------------------------
// Installs a callback to call right before a spatial partition query occurs
//-----------------------------------------------------------------------------
class IPartitionQueryCallback
{
public:
	virtual void OnPreQuery( SpatialPartitionListMask_t listMask ) = 0;
	virtual void OnPostQuery( SpatialPartitionListMask_t listMask ) = 0;
};


//-----------------------------------------------------------------------------
// This is the spatial partition manager, groups objects into buckets
//-----------------------------------------------------------------------------
enum
{
	PARTITION_INVALID_HANDLE = (SpatialPartitionHandle_t)~0
};


class ISpatialPartition
{
public:
	// Create/destroy a handle for this dude in our system. Destroy
	// will also remove it from all lists it happens to be in
	SpatialPartitionHandle_t CreateHandle( IHandleEntity *pHandleEntity );

	// A fast method of creating a handle + inserting into the tree in the right place
	SpatialPartitionHandle_t CreateHandle( IHandleEntity *pHandleEntity,
		SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs ); 

	void DestroyHandle( SpatialPartitionHandle_t handle );

	// Adds, removes an handle from a particular spatial partition list
	// There can be multiple partition lists; each has a unique id
	void Insert( SpatialPartitionListMask_t listMask, 
		SpatialPartitionHandle_t handle );
	void Remove( SpatialPartitionListMask_t listMask, 
		SpatialPartitionHandle_t handle );

	// Same as calling Remove() then Insert(). For performance-sensitive areas where you want to save a call.
	void RemoveAndInsert( SpatialPartitionListMask_t removeMask, SpatialPartitionListMask_t insertMask, 
		SpatialPartitionHandle_t handle );

	// This will remove a particular handle from all lists
	void Remove( SpatialPartitionHandle_t handle );

	// Call this when an entity moves...
	void ElementMoved( SpatialPartitionHandle_t handle, 
		const Vector& mins, const Vector& maxs );

	// A fast method to insert + remove a handle from the tree...
	// This is used to suppress collision of a single model..
	SpatialTempHandle_t HideElement( SpatialPartitionHandle_t handle );
	void UnhideElement( SpatialPartitionHandle_t handle, SpatialTempHandle_t tempHandle );
	
	// Installs callbacks to get called right before a query occurs
	void InstallQueryCallback( IPartitionQueryCallback *pCallback );
	void RemoveQueryCallback( IPartitionQueryCallback *pCallback );

	// Gets all entities in a particular volume...
	// if coarseTest == true, it'll return all elements that are in
	// spatial partitions that intersect the box
	// if coarseTest == false, it'll return only elements that truly intersect
	void EnumerateElementsInBox(
		SpatialPartitionListMask_t listMask,  
		const Vector& mins, 
		const Vector& maxs, 
		bool coarseTest, 
		IPartitionEnumerator* pIterator 
		);

	void EnumerateElementsInSphere(
		SpatialPartitionListMask_t listMask, 
		const Vector& origin, 
		float radius, 
		bool coarseTest, 
		IPartitionEnumerator* pIterator 
		);

	void EnumerateElementsAlongRay(
		SpatialPartitionListMask_t listMask, 
		const Ray_t& ray, 
		bool coarseTest, 
		IPartitionEnumerator* pIterator 
		);

	void EnumerateElementsAtPoint( 
		SpatialPartitionListMask_t listMask, 
		const Vector& pt, 
		bool coarseTest, 
		IPartitionEnumerator* pIterator
		);

	// For debugging.... suppress queries on particular lists
	void SuppressLists( SpatialPartitionListMask_t nListMask, bool bSuppress );
	SpatialPartitionListMask_t GetSuppressedLists();

	void RenderAllObjectsInTree( float flTime );
	void RenderObjectsInPlayerLeafs( const Vector &vecPlayerMin, const Vector &vecPlayerMax, float flTime );
	void RenderLeafsForRayTraceStart( float flTime );
	void RenderLeafsForRayTraceEnd( void );
	void RenderLeafsForHullTraceStart( float flTime );
	void RenderLeafsForHullTraceEnd( void );
	void RenderLeafsForBoxStart( float flTime );
	void RenderLeafsForBoxEnd( void );
	void RenderLeafsForSphereStart( float flTime );
	void RenderLeafsForSphereEnd( void );

	void RenderObjectsInBox( const Vector &vecMin, const Vector &vecMax, float flTime );
	void RenderObjectsInSphere( const Vector &vecCenter, float flRadius, float flTime );
	void RenderObjectsAlongRay( const Ray_t& ray, float flTime );

	void ReportStats( const char *pFileName );
};
