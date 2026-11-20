#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "qlimits.h"
#include "bspfile.h"
import std;

#include "filesystem.h"
#include "filesystem_engine.h"

#include "dispcoll_common.h"
import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.fltx4;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.tier1.interfaces;
import ks.tier1.small_vector;
import ks.tier1.strtools;

class CDispCollTree;
class CCollisionBSPData;
struct cbrush_t;

inline constexpr auto MAX_CHECK_COUNT_DEPTH = 2;

struct TraceVisits_t
{
	std::vector< bool > m_Brushes;
	std::vector< bool > m_Disps;
};

typedef uint32 TraceCounter_t;
typedef std::vector< TraceCounter_t > CTraceCounterVec;

struct TraceInfo_t
{
	TraceInfo_t()
	{
		std::memset( this, 0, sizeof( TraceInfo_t ) );
		m_nCheckDepth = -1;
	}

	Vector m_start;
	Vector m_end;
	Vector m_mins;
	Vector m_maxs;
	Vector m_extents;
	Vector m_delta;
	Vector m_invDelta;

	trace_t m_trace;
	trace_t m_stabTrace;

	int m_contents;
	bool m_ispoint;
	bool m_isswept;

	// BSP Data
	CCollisionBSPData *m_pBSPData;

	// Displacement Data
	Vector m_DispStabDir;		// the direction to stab in
	int m_bDispHit;				// hit displacement surface last

	bool m_bCheckPrimary;
	int m_nCheckDepth;
	TraceCounter_t m_Count[MAX_CHECK_COUNT_DEPTH];

	CTraceCounterVec m_BrushCounters[MAX_CHECK_COUNT_DEPTH];
	CTraceCounterVec m_DispCounters[MAX_CHECK_COUNT_DEPTH];

	TraceCounter_t GetCount()				{ return m_Count[m_nCheckDepth]; }
	TraceCounter_t *GetBrushCounters()		{ return m_BrushCounters[m_nCheckDepth].data(); }
	TraceCounter_t *GetDispCounters()		{ return m_DispCounters[m_nCheckDepth].data(); }

	bool Visit( cbrush_t *pBrush, int ndxBrush );
	bool Visit( int dispIndex );

	bool Visit( cbrush_t *pBrush, int ndxBrush, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters );
	bool Visit( int dispIndex, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters );
};


struct OcclusionTestResults_t;

struct COcclusionInfo
{
	COcclusionInfo()
	{
		V_memset( m_nBrushesChecked, 0, sizeof( m_nBrushesChecked ) );
		m_nBrushesChecked[ 0 ]/*[ 0 ] = m_nBrushesChecked[ 0 ][ 1 ] */= 0xFFFF; // the only associative line that can conceivably contain 0 is the line 0; set it to invalid value, effectively making it empty
	}

	bool PrepareCheckBrush( uint16 nBrush )
	{
		const uint nMask = std::size( m_nBrushesChecked ) - 1;
		uint16 * pLine = &m_nBrushesChecked[ nBrush & nMask ];
		if ( pLine[ 0 ] == nBrush /*|| pLine[ 1 ] == nBrush*/ )
		{
			s_nAssocArrayHits++;
			return false; // we already saw this brush
		}
		if ( ( pLine[ 0 ] & nMask ) == ( nBrush & nMask ) )
			++s_nAssocArrayCollisions; // this entry was valid, we have to overwrite it with another entry
		//pLine[ 1 ] = pLine[ 0 ];
		++s_nAssocArrayMisses;
		pLine[ 0 ] = nBrush;
		return true; // ok, maybe we saw this brush and maybe we didn't, we don't know for sure, but we didn't see it recently
	}

	fltx4 m_StartXnXYnY; // {p0+X0,p0-X0,p0+Y0,p0-Y0}
	fltx4 m_EndXnXYnY;// {p1+X1,p1-X1,p1+Y1,p1-Y1}
	fltx4 m_StartEndZnZ; // {p0+Z0,p0-Z0,p1+Z1,p1-Z1}

	[[gnu::always_inline]] inline fltx4 GetStartX()const { return ShuffleXYXY( m_StartXnXYnY ); }    // {p0+X0, p0-X0,p0+X0,p0-X0}
	[[gnu::always_inline]] inline fltx4 GetStartY()const { return ShuffleZZWW( m_StartXnXYnY ); }    // {p1+Y1, p1+Y1,p1-Y1,p1-Y1}
	[[gnu::always_inline]] inline fltx4 GetEndX()const  { return ShuffleXYXY( m_EndXnXYnY ); }       // {p0+X0, p0-X0,p0+X0,p0-X0}
	[[gnu::always_inline]] inline fltx4 GetEndY()const { return ShuffleZZWW( m_EndXnXYnY ); }        // {p1+Y1, p1+Y1,p1-Y1,p1-Y1}
	[[gnu::always_inline]] inline fltx4 GetStartZ0()const { return SplatXSIMD( m_StartEndZnZ ); }
	[[gnu::always_inline]] inline fltx4 GetStartZ1()const { return SplatYSIMD( m_StartEndZnZ ); }
	[[gnu::always_inline]] inline fltx4 GetEndZ0()const { return SplatZSIMD( m_StartEndZnZ ); }
	[[gnu::always_inline]] inline fltx4 GetEndZ1()const { return SplatWSIMD( m_StartEndZnZ ); }

	uint16 m_nBrushesChecked[ 64 ]/*[ 2 ]*/; // associative array of brushes that have already been checked
	VectorAligned m_traceMins;
	VectorAligned m_traceMaxs;
	VectorAligned m_delta;
	VectorAligned m_deltaPos;

	static uint64 s_nAssocArrayCollisions;
	static uint64 s_nAssocArrayHits;
	static uint64 s_nAssocArrayMisses;

	Vector m_start;
	Vector m_end;
	Vector m_extents;
	Vector m_uvwExtents;
	Vector m_uvwMins;
	Vector m_uvwMaxs;
	Vector m_deltaSigns;
// 	Vector m_minsPos;
// 	Vector m_maxsPos;
	int m_contents;
	// BSP Data
	CCollisionBSPData *m_pBSPData;
	OcclusionTestResults_t *m_pResults;
};


extern TraceInfo_t g_PrimaryTraceInfo;

inline constexpr auto NEVER_UPDATED = -99999;

//=============================================================================
//
// Local CModel Structures (cmodel.cpp and cmodel_disp.cpp)
//

struct cbrushside_t
{
	cplane_t		*plane;
	unsigned short	surfaceIndex;
	byte			bBevel;			// is the side a bevel plane?
	byte			bThin;			// is a thin brush side?
};

inline constexpr auto NUMSIDES_BOXBRUSH = 0xFFFF;

struct cbrush_t
{
	int				contents;
	unsigned short	numsides;
	unsigned short	firstbrushside;

	inline int GetBox() const { return firstbrushside; }
	inline void SetBox( int boxID )
	{
		numsides = NUMSIDES_BOXBRUSH;
		firstbrushside = boxID;
	}
	inline bool IsBox() const { return numsides == NUMSIDES_BOXBRUSH ? true : false; }
};

// 48-bytes, aligned to 16-byte boundary
// this is a brush that is an AABB.  It's encoded this way instead of with 6 brushsides
struct cboxbrush_t
{
	VectorAligned	mins;
	VectorAligned	maxs;

	unsigned short	surfaceIndex[6];
	unsigned short	thinMask;
	unsigned short	pad;
};

struct cleaf_t
{
	int			    contents;
	short			cluster;
	short			area : 9;
	short			flags : 7;
	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
	unsigned short	dispListStart;
	unsigned short	dispCount;
};

struct carea_t
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;							// if two areas have equal floodnums, they are connected
	int		floodvalid;
};


struct cnode_t
{
	cplane_t	*plane;
	int			children[2];		// negative numbers are leafs
};


// global collision checkcount
TraceInfo_t *BeginTrace();
void PushTraceVisits( TraceInfo_t *pTraceInfo );
void PopTraceVisits( TraceInfo_t *pTraceInfo );
void EndTrace( TraceInfo_t *&pTraceInfo );


//-----------------------------------------------------------------------------
// We keep running into overflow errors here. This is to avoid that problem
//-----------------------------------------------------------------------------
template <class T>
class CRangeValidatedArray
{
public:
	void Attach( int nCount, T* pData );
	void Detach();

	T &operator[]( int i );
	const T &operator[]( int i ) const;

	T *Base();

private:
	T *m_pArray;

};

template <class T>
inline T &CRangeValidatedArray<T>::operator[]( int i )
{
	return m_pArray[i];
}

template <class T>
inline const T &CRangeValidatedArray<T>::operator[]( int i ) const
{
	return m_pArray[i];
}

template <class T>
inline void CRangeValidatedArray<T>::Attach( int nCount, T* pData )
{
	m_pArray = pData;

}

template <class T>
inline void CRangeValidatedArray<T>::Detach()
{
	m_pArray = nullptr;

}

template <class T>
inline T *CRangeValidatedArray<T>::Base()
{
	return m_pArray;
}

//-----------------------------------------------------------------------------
// A dumpable version of RangeValidatedArray
//-----------------------------------------------------------------------------
template <class T> 
class CDiscardableArray 
{
public:
	CDiscardableArray()
	{
		m_nCount = 0;
		m_nOffset = -1;
		std::memset( m_pFilename, 0, sizeof( m_pFilename ) );
	}

	~CDiscardableArray()
	{
	}

	void Init( char* pFilename, int nOffset, int nCount, void *pData = nullptr )
	{
		if ( !m_data.empty() )
		{
			Discard();
		}

		m_nCount = nCount;
		V_strcpy_safe( m_pFilename, pFilename );
		m_nOffset = nOffset;

		// can preload as required
		if ( pData )
		{
			const std::byte *pBytes = static_cast< const std::byte * >( pData );
			m_data.assign( pBytes, pBytes + nCount );
		}
	}

	// Either get or load the array as needed:
	T* Get()
	{
		if ( m_data.empty() )
		{

			auto data = g_pFileSystem->ReadFile( m_pFilename, nullptr, sizeof(T) * m_nCount, m_nOffset );
			if ( !data )
			{
				return nullptr;
			}
			m_data = std::move( *data );
		}

		return (T *)m_data.data();
	}

	void Discard()
	{
		m_data.clear();
		m_data.shrink_to_fit();
	}

protected:
	int			m_nCount;
	std::vector< std::byte >	m_data;
	char		m_pFilename[256];
	int			m_nOffset;
};

const int SURFACE_INDEX_INVALID = 0xFFFF;

//=============================================================================
//
// Collision BSP Data Class
//
class CCollisionBSPData
{
public:
	// This is sort of a hack, but it was a little too painful to do this any other way
	// The goal of this dude is to allow us to override the tree with some
	// other tree (or a subtree)
	cnode_t*					map_rootnode;

	char						mapPathName[MAX_QPATH];
	static csurface_t			nullsurface;

	int									numbrushsides;
	CRangeValidatedArray<cbrushside_t>	map_brushsides;
	int									numboxbrushes;
	CRangeValidatedArray<cboxbrush_t>	map_boxbrushes;
	int									numplanes;
	CRangeValidatedArray<cplane_t>		map_planes;
	int									numnodes;
	CRangeValidatedArray<cnode_t>		map_nodes;
	int									numleafs;				// allow leaf funcs to be called without a map
	CRangeValidatedArray<cleaf_t>		map_leafs;
	int									emptyleaf, solidleaf;
	int									numleafbrushes;
	CRangeValidatedArray<unsigned short> map_leafbrushes;
	int									numcmodels;
	CRangeValidatedArray<cmodel_t>		map_cmodels;
	int									numbrushes;
	CRangeValidatedArray<cbrush_t>		map_brushes;
	int									numdisplist;
	CRangeValidatedArray<unsigned short> map_dispList;
	
	// this points to the whole block of memory for vis data, but it is used to
	// reference the header at the top of the block.
	int									numvisibility;
	dvis_t								*map_vis;

	int									numentitychars;
	CDiscardableArray<char>				map_entitystring;

	int									numareas;
	CRangeValidatedArray<carea_t>		map_areas;
	int									numareaportals;
	CRangeValidatedArray<dareaportal_t>	map_areaportals;
	int									numclusters;
	const char							*map_nullname;
	int									numtextures;
	char    							*map_texturenames;
	CRangeValidatedArray<csurface_t>	map_surfaces;
	int									floodvalid;
	int									numportalopen;
	CRangeValidatedArray<bool>			portalopen;
	int									allcontents;

	csurface_t							*GetSurfaceAtIndex( unsigned short surfaceIndex );
};

//=============================================================================
//
// physics collision
//
class IPhysicsSurfaceProps;
class IPhysicsCollision;

extern IPhysicsSurfaceProps	*physprops;
extern IPhysicsCollision	*physcollision;

//=============================================================================
//
// This might eventually become a class/interface when we want multiple instances
// etc.......for now....
//
extern csurface_t nullsurface;

extern bool bStartSolidDisp;

bool CollisionBSPData_Init( CCollisionBSPData *pBSPData );
void CollisionBSPData_Destroy( CCollisionBSPData *pBSPData );
void CollisionBSPData_LinkPhysics( void );

void CollisionBSPData_PreLoad( CCollisionBSPData *pBSPData );
bool CollisionBSPData_Load( const char *pPathName, CCollisionBSPData *pBSPData, texinfo_t *pTexinfo, int texInfoCount );
void CollisionBSPData_PostLoad( void );

//-----------------------------------------------------------------------------
// Returns the collision tree associated with the ith displacement
//-----------------------------------------------------------------------------

CDispCollTree* CollisionBSPData_GetCollisionTree( int i );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CCollisionBSPData *GetCollisionBSPData( void /*int ndxBSP*/ )
{
	extern CCollisionBSPData g_BSPData;								// the global collision bsp
	return &g_BSPData;
}

//=============================================================================
//
// Collision Model Counts
//


//=============================================================================
//
// Older Code That Should Live Here!!!!
// a shared file should contain all the CDispCollTree stuff
//

//=============================================================================
//
// Displacement Collision Functions and Data
//
// UNDONE: Find a way to overlap the counter/contents bits with mins.w/maxs.w without hosing performance
struct alignedbbox_t
{
	VectorAligned	mins;
	VectorAligned	maxs;
	int				dispCounter;
	int				dispContents;
	int				pad[2];
	void SetCounter(int counter)
	{
		dispCounter = counter;
	}
	int GetCounter()
	{
		return dispCounter;
	}
	void SetContents(int contents)
	{
		dispContents = contents;
	}
	int GetContents()
	{
		return dispContents;
	}
	void Init( const Vector &minsIn, const Vector &maxsIn, int counterIn, int contentsIn )
	{
		mins = minsIn;
		SetCounter(counterIn);
		maxs = maxsIn;
		SetContents(contentsIn);
	}
};

inline constexpr auto TLD_DEF_BRUSH_MAX = 64;
inline constexpr auto TLD_DEF_DISP_MAX = 32;
inline constexpr auto TLD_DEF_ENTITY_MAX = 64;

class ICollideable;
class IHandleEntity;
class IEngineTrace;
struct collideable_handleentity_t
{
	IHandleEntity *pEntity;
	ICollideable *pCollideable;
};

class CTraceListData : public IPartitionEnumerator, public ITraceListData
{
public:

	CTraceListData() 
	{
		m_pEngineTrace = nullptr;
		m_bFoundNonSolidLeaf = false;
		m_mins.Init();
		m_maxs.Init();
	}
	~CTraceListData() {}

	void Reset()
	{
		m_brushList.clear();
		m_dispList.clear();
		m_entityList.clear();
		m_staticPropList.clear();
		m_mins.Init();
		m_maxs.Init();
		m_pEngineTrace = nullptr;
		m_bFoundNonSolidLeaf = false;
	}

	bool IsEmpty() { return m_pEngineTrace == nullptr ? true : false; }
	// For entities...
	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
	bool CanTraceRay( const Ray_t &ray );

public:

	ks::small_vector< unsigned short, TLD_DEF_BRUSH_MAX >	m_brushList;
	ks::small_vector< unsigned short, TLD_DEF_DISP_MAX >	m_dispList;
	ks::small_vector< collideable_handleentity_t, TLD_DEF_ENTITY_MAX >	m_entityList;
	ks::small_vector< collideable_handleentity_t, TLD_DEF_ENTITY_MAX >	m_staticPropList;

	Vector	m_mins;
	Vector	m_maxs;
	class CEngineTrace *m_pEngineTrace;
	bool	m_bFoundNonSolidLeaf;
};


extern int g_DispCollTreeCount;
extern CDispCollTree *g_pDispCollTrees;
extern alignedbbox_t *g_pDispBounds;
extern csurface_t dispSurf;

// memory allocation/de-allocation
void DispCollTrees_FreeLeafList( CCollisionBSPData *pBSPData );

// setup
void CM_DispTreeLeafnum( CCollisionBSPData *pBSPData );

// collision
void CM_TestInDispTree( TraceInfo_t *pTraceInfo, const unsigned short *pDispList, int dispListCount, const Vector &traceStart, 
				const Vector &boxMin, const Vector &boxMax, int collisionMask, trace_t *pTrace );
template <bool IS_POINT>
void CM_TraceToDispTree( TraceInfo_t *pTraceInfo, CDispCollTree *pDispTree, float startFrac, float endFrac );
void CM_PostTraceToDispTree( TraceInfo_t *pTraceInfo );

//=============================================================================
//
// profiling purposes only -- remove when done!!!
//

void CM_TestBoxInBrush ( const Vector& mins, const Vector& maxs, const Vector& p1,
					  trace_t *trace, cbrush_t *brush, BOOL bDispSurf );
void CM_TestBoxInBrush( TraceInfo_t *pTraceInfo, const cbrush_t *brush );
void CM_RecursiveHullCheck ( TraceInfo_t *pTraceInfo, int num, const float p1f, const float p2f );
bool CM_RecursiveOcclusionPass( COcclusionInfo &oi, int num, const float p1f, const float p2f, const Vector& p1, const Vector& p2 );
void CM_GetTraceDataForBSP( const Vector &mins, const Vector &maxs, CTraceListData &traceData );


//=============================================================================

inline bool TraceInfo_t::Visit( cbrush_t *pBrush, int ndxBrush, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters )
{
	TraceCounter_t * __restrict__ pCounter = pCachedCounters + ndxBrush;

	if ( *pCounter == cachedCount )
	{
		return false;
	}

	*pCounter = cachedCount;
	return true;
}

[[gnu::always_inline]] inline bool TraceInfo_t::Visit( int dispCounter, TraceCounter_t cachedCount, TraceCounter_t *pCachedCounters )
{
	TraceCounter_t * __restrict__ pCounter = pCachedCounters + dispCounter;

	if ( *pCounter == cachedCount )
	{
		return false;
	}

	*pCounter = cachedCount;
	return true;
}

[[gnu::always_inline]] inline bool TraceInfo_t::Visit( cbrush_t *pBrush, int ndxBrush )
{
	return Visit( pBrush, ndxBrush, GetCount(), GetBrushCounters() );
}

[[gnu::always_inline]] inline bool TraceInfo_t::Visit( int dispIndex )
{
	return Visit( dispIndex, GetCount(), GetDispCounters() );
}
