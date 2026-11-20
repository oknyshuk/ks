// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <stdio.h>
#include "basehandle.h"
#include "string_t.h"
#include "bspfile.h"
#include "filesystem.h"
#include "tier2/tier2_logging.h"
#include "declare_class.h"
#include "edict.h"
#include "server_class.h"
#include "activitylist.h"
#include "datamap.h"
#include "dt_recv.h"
#include "dt_send.h"
#include "eiface.h"
#include "iclientunknown.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "vphysics_interface.h"
#include "predictable_entity.h"
#include "choreoactor.h"
#include "choreoscene.h"
#include "iscenetokenprocessor.h"
#include "studio.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "bittools.h"
#include "worldsize.h"
#include "shareddefs.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "igameevents.h"
#include "ivoiceserver.h"
#include "proto_types.h"
#include "vscript/ivscript.h"
#include "enginecallback.h"
#include "irecipientfilter.h"
#include "shake.h"
#include "util.h"
#include "vscript_game.h"
#include "responserules/response_types.h"
#include "vcollide_parse.h"
#include "videocfg/videocfg.h"
#include "ai_criteria.h"
#include "baseentity.h"
#include "vscript_shared.h"
module ks.server:baseentity;

import ks.tier1.createinterface;
import ks.core.basetypes;
import ks.math.vector;
import std;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.core.platform;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.core.threadtools;
import ks.tier1.reflect_wirevar;
import ks.core.vprof;
import ks.game.ai_activity;
import ks.math.ssequaternion;
import ks.core.dbg;
import ks.tier1.convar;
import ks.core.random;
import ks.math.ssemath;
import ks.math.vplane;
import ks.tier1.exprevaluator;
import ks.game.shareddefs;
import ks.core.color;
import ks.math.vmatrix;
import ks.tier1.icvar;
import ks.tier1.playerid;
import :entity_kind;
import ks.game.entity_cast;
import ks.tier1.strtools;
import ks.game.entitycore;
import ks.game.mapentities_shared;

class CEntInfo
{
public:
	IHandleEntity	*m_pEntity;
	int				m_SerialNumber;
	CEntInfo		*m_pPrev;
	CEntInfo		*m_pNext;
	string_t		m_iName;
	string_t		m_iClassName;

	void			ClearLinks();
};

class CBaseEntityList
{
public:
	CBaseEntityList();
	~CBaseEntityList();
	
	// Add and remove entities. iForcedSerialNum should only be used on the client. The server
	// gets to dictate what the networkable serial numbers are on the client so it can send
	// ehandles over and they work.
	CBaseHandle AddNetworkableEntity( IHandleEntity *pEnt, int index, int iForcedSerialNum = -1 );
	CBaseHandle AddNonNetworkableEntity( IHandleEntity *pEnt );
	void RemoveEntity( CBaseHandle handle );

	// Get an ehandle from a networkable entity's index (note: if there is no entity in that slot,
	// then the ehandle will be invalid and produce NULL).
	CBaseHandle GetNetworkableHandle( int iEntity ) const;

	// ehandles use this in their Get() function to produce a pointer to the entity.
	IHandleEntity* LookupEntity( const CBaseHandle &handle ) const;
	IHandleEntity* LookupEntityByNetworkIndex( int edictIndex ) const;

	// Use these to iterate over all the entities.
	CBaseHandle FirstHandle() const;
	CBaseHandle NextHandle( CBaseHandle hEnt ) const;
	static CBaseHandle InvalidHandle();

	const CEntInfo *FirstEntInfo() const;
	const CEntInfo *NextEntInfo( const CEntInfo *pInfo ) const;
	const CEntInfo *GetEntInfoPtr( const CBaseHandle &hEnt ) const;
	const CEntInfo *GetEntInfoPtrByIndex( int index ) const;

	// Used by Foundry when an entity is respawned/edited.
	// We force the new entity's ehandle to be the same so anyone pointing at it still gets a valid CBaseEntity out of their ehandle.
	void ForceEntSerialNumber( int iEntIndex, int iSerialNumber );

// Overridables.
protected:

	// These are notifications to the derived class. It can cache info here if it wants.
	virtual void OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle );
	
	// It is safe to delete the entity here. We won't be accessing the pointer after
	// calling OnRemoveEntity.
	virtual void OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle );

private:

	CBaseHandle AddEntityAtSlot( IHandleEntity *pEnt, int iSlot, int iForcedSerialNum );
	void RemoveEntityAtSlot( int iSlot );

private:
	
	class CEntInfoList
	{
	public:
		CEntInfoList();

		const CEntInfo	*Head() const { return m_pHead; }
		const CEntInfo	*Tail() const { return m_pTail; }
		CEntInfo		*Head() { return m_pHead; }
		CEntInfo		*Tail() { return m_pTail; }
		void			AddToHead( CEntInfo *pElement ) { LinkAfter( nullptr, pElement ); }
		void			AddToTail( CEntInfo *pElement ) { LinkBefore( nullptr, pElement ); }

		void LinkBefore( CEntInfo *pBefore, CEntInfo *pElement );
		void LinkAfter( CEntInfo *pBefore, CEntInfo *pElement );
		void Unlink( CEntInfo *pElement );
		bool IsInList( CEntInfo *pElement );
	
	private:
		CEntInfo		*m_pHead;
		CEntInfo		*m_pTail;
	};

	int GetEntInfoIndex( const CEntInfo *pEntInfo ) const;

	// The first MAX_EDICTS entities are networkable. The rest are client-only or server-only.
	CEntInfo m_EntPtrArray[NUM_ENT_ENTRIES];
	CEntInfoList	m_activeList;
	CEntInfoList	m_freeNonNetworkableList;
};

// ------------------------------------------------------------------------------------ //
// Inlines.
// ------------------------------------------------------------------------------------ //

	

extern CBaseEntityList *g_pEntityList;

class CBaseEntity;

// -------------------------------------------------------------------------------------------------- //
// Game-code CBaseHandle implementation.
// -------------------------------------------------------------------------------------------------- //

extern CBaseEntityList *g_pEntityList;

inline IHandleEntity* EntityFromHandle( const CBaseHandle &handle )
{
	return g_pEntityList->LookupEntity( handle );
}

inline bool operator ==( const CBaseHandle &handle, const IHandleEntity *pEnt )
{
	return EntityFromHandle( handle ) == pEnt;
}

inline bool operator !=( const CBaseHandle &handle, const IHandleEntity *pEnt )
{
	return EntityFromHandle( handle ) != pEnt;
}

// -------------------------------------------------------------------------------------------------- //
// CHandle.
//
// Only safe to use in cases where you can statically verify that T* can safely be reinterpret-casted
// to IHandleEntity*; that is, that it's derived from IHandleEntity and IHandleEntity is the
// first base class.
//
// Unfortunately some classes are forward-declared and the compiler can't determine at compile time
// how to static_cast<> them to IHandleEntity.
// -------------------------------------------------------------------------------------------------- //
template< class T >
class CHandle : public CBaseHandle
{
public:

			CHandle();
			CHandle( int iEntry, int iSerialNumber );
	/*implicit*/ CHandle( T *pVal );
	/*implicit*/ CHandle( INVALID_EHANDLE_tag );
	// Declared, not implicit: GCC 16 crashes declaring the implicit members of a specialisation
	// that another partition instantiated.
	CHandle( const CHandle & ) = default;
	CHandle &operator=( const CHandle & ) = default;
	~CHandle() = default;

	// NOTE: The following two constructor functions are not type-safe, and can allow creating a
	// CHandle<T> that doesn't actually point to an object of type T.
	//
	// It is your responsibility to ensure that the target of the handle actually points to the
	// correct type of object before calling these functions.

	static CHandle<T> UnsafeFromBaseHandle( const CBaseHandle& handle );

	// The index should have come from a call to CBaseHandle::ToInt(). If it hasn't, you're in trouble.
	static CHandle<T> UnsafeFromIndex( int index );

	T*		Get() const;
	void	Set( const T* pVal )
	{
		CBaseHandle::Set( reinterpret_cast<const IHandleEntity*>( pVal ) );
	}

	/*implicit*/ operator T*();
	/*implicit*/ operator T*() const;

	bool	operator !() const;
	bool	operator==( T *val ) const;
	bool	operator!=( T *val ) const;
	CHandle& operator=( const T *val )
	{
		Set( val );
		return *this;
	}

	T*		operator->() const;
};

// ----------------------------------------------------------------------- //
// Inlines.
// ----------------------------------------------------------------------- //

template<class T>
inline CHandle<T>::CHandle()
{
}

template<class T>
inline CHandle<T>::CHandle( INVALID_EHANDLE_tag )
	: CBaseHandle( INVALID_EHANDLE )
{
}

template<class T>
inline CHandle<T>::CHandle( int iEntry, int iSerialNumber )
{
	Init( iEntry, iSerialNumber );
}

template<class T>
inline CHandle<T>::CHandle( T *pObj )
	: CBaseHandle( INVALID_EHANDLE )
{
	Set( pObj );
}

template<class T>
inline CHandle<T> CHandle<T>::UnsafeFromBaseHandle( const CBaseHandle &handle )
{
	return UnsafeFromIndex( handle.ToInt() );
}

template<class T>
inline CHandle<T> CHandle<T>::UnsafeFromIndex( int index )
{
	CHandle<T> ret;
	ret.m_Index = index;
	return ret;
}

template<class T>
inline T* CHandle<T>::Get() const
{
	return (T*)EntityFromHandle( *this );
}

template<class T>
inline CHandle<T>::operator T *() 
{ 
	return Get( ); 
}

template<class T>
inline CHandle<T>::operator T *() const
{ 
	return Get( ); 
}

template<class T>
inline bool CHandle<T>::operator !() const
{
	return !Get();
}

template<class T>
inline bool CHandle<T>::operator==( T *val ) const
{
	return Get() == val;
}

template<class T>
inline bool CHandle<T>::operator!=( T *val ) const
{
	return Get() != val;
}

template<class T>
T* CHandle<T>::operator -> () const
{
	return Get();
}

// specialization of EnsureValidValue for CHandle<T>
template<typename T>
[[gnu::always_inline]] inline void EnsureValidValue( CHandle<T> &x ) { x = INVALID_EHANDLE; }

// One instantiation for the module: imported partitions then share it instead of each merging its own.
template class CHandle<CBaseEntity>;

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

typedef void ( *FN_FIELD_COMPARE )( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, bool noterrorchecked, bool differs, bool withintolerance, const char *value );

// Each datamap_t is broken down into two flattened arrays of fields, 
//  one for PC_NETWORKED_DATA and one for PC_NON_NETWORKED_ONLY (optimized_datamap_t::datamapinfo_t::flattenedoffsets_t)
// Each flattened array is sorted by offset for better cache performance
// Finally, contiguous "runs" off offsets are precomputed (optimized_datamap_t::datamapinfo_t::datacopyruns_t) for fast copy operations

// A data run is a set of DEFINE_PRED_FIELD fields in a c++ object which are contiguous and can be processing
//  using a single memcpy operation
struct datarun_t
{
	datarun_t() : m_nStartFlatField( 0 ), m_nEndFlatField( 0 ), m_nLength( 0 )
	{
		for ( int i = 0 ; i < TD_OFFSET_COUNT; ++i )
		{
			m_nStartOffset[ i ] = 0;
		}
	}

	// Indices of start/end fields in the flattened typedescription_t list
	int m_nStartFlatField;
	int m_nEndFlatField;

	// Offsets for run in the packed/unpacked data (I think the run starts need to be properly aligned)
	int m_nStartOffset[ TD_OFFSET_COUNT ];
	int m_nLength;
};

struct datacopyruns_t 
{
public:
	std::vector< datarun_t > m_vecRuns;
};

struct flattenedoffsets_t
{
	std::vector< typedescription_t >	m_Flattened;
	int								m_nPackedSize; // Contiguous memory to pack all of these together for TD_OFFSET_PACKED
	int								m_nPackedStartOffset;
};

struct datamapinfo_t
{
	// Flattened list, with FIELD_EMBEDDED, FTYPEDESC_PRIVATE, 
	//  and FTYPEDESC_OVERRIDE (overridden) fields removed
	flattenedoffsets_t	m_Flat;
	datacopyruns_t		m_CopyRuns;
};

struct optimized_datamap_t
{
	// Optimized info for PC_NON_NETWORKED and PC_NETWORKED data
	datamapinfo_t	m_Info[ PC_COPYTYPE_COUNT ];
};

inline optimized_datamap_t *OptimizedDataMap( const datamap_t *pMap )
{
	return static_cast<optimized_datamap_t *>( pMap->m_pOptimizedDataMap );
}

class CPredictionCopy
{
public:
	typedef enum
	{
		DIFFERS = 0,
		IDENTICAL,
		WITHINTOLERANCE,
	} difftype_t;

	typedef enum
	{
		TRANSFERDATA_COPYONLY = 0,  // Data copying only (uses runs)
		TRANSFERDATA_ERRORCHECK_NOSPEW, // Checks for errors, returns after first error found
		TRANSFERDATA_ERRORCHECK_SPEW,   // checks for errors, reports all errors to console
		TRANSFERDATA_ERRORCHECK_DESCRIBE, // used by hud_pdump, dumps values, etc, for all fields
	} optype_t;

	CPredictionCopy( int type, byte *dest, bool dest_packed, const byte *src, bool src_packed,
		optype_t opType, FN_FIELD_COMPARE func = nullptr );
	
	int		TransferData( const char *operation, int entindex, datamap_t *dmap );

	static bool PrepareDataMap( datamap_t *dmap );
	static const typedescription_t *FindFlatFieldByName( const char *fieldname, const datamap_t *dmap );
private:

	// Operations:
	void	TransferDataCopyOnly( const datamap_t *dmap );
	void	TransferDataErrorCheckNoSpew( char const *pchOperation, const datamap_t *dmap );
	void	TransferDataErrorCheckSpew( char const *pchOperation, const datamap_t *dmap );
	void	TransferDataDescribe( char const *pchOperation, const datamap_t *dmap );

	// Report function
	void	ReportFieldsDiffer( const datamap_t *pCurrentMap, const typedescription_t *pField, const char *fmt, ... );
	void	OutputFieldDescription( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t dt, const char *fmt, ... );
	
	// Helper for TransferDataCopyOnly
	void	CopyFlatFieldsUsingRuns( const datamap_t *pCurrentMap, int nPredictionCopyType );
	void	CopyFlatFields( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	[[gnu::always_inline]] inline void CopyField( difftype_t difftype, T *outvalue, const T *invalue, int count );

	// Helper for TransferDataErrorCheckNoSpew
	void	ErrorCheckFlatFields_NoSpew( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	[[gnu::always_inline]] inline void ProcessField_Compare_NoSpew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helper for TransferDataErrorCheckSpew
	void	ErrorCheckFlatFields_Spew( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	[[gnu::always_inline]] inline void ProcessField_Compare_Spew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helper for TransferDataDescribe
	void	DescribeFields( const std::vector< const datamap_t * > &vecGroups, const datamap_t *pCurrentMap, int nPredictionCopyType );
	// Main entry point
	template< class T >
	[[gnu::always_inline]] inline void ProcessField_Describe( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helpers for entity field watcher
	void	DetermineWatchField( const char *operation, int entindex, const datamap_t *dmap );
	void	WatchMsg( const typedescription_t *pField, const char *fmt, ... );
	void	DumpWatchField( const typedescription_t *pField, const byte *outvalue, int count );
	template< class T >
	[[gnu::always_inline]] inline void WatchField( const typedescription_t *pField, const T *outvalue, int count );

	// Helper for ErrorCheck ops
	template< class T >
	[[gnu::always_inline]] inline difftype_t CompareField( const typedescription_t *pField, const T *outvalue, const T *invalue, int count );
	// Used by TRANSFERDATA_ERRORCHECK_SPEW and by TRANSFERDATA_ERRORCHECK_DESCRIBE
	template< class T >
	[[gnu::always_inline]] inline void DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const T *outvalue, const T *invalue, int count );

private:

	optype_t		m_OpType;
	int				m_nType;
	byte			*m_pDest;
	const byte		*m_pSrc;
	int				m_nDestOffsetIndex;
	int				m_nSrcOffsetIndex;
	int				m_nErrorCount;
	int				m_nEntIndex;

	FN_FIELD_COMPARE	m_FieldCompareFunc;

	const typedescription_t	 *m_pWatchField;
	char const			*m_pOperation;

};

typedef void (*FN_FIELD_DESCRIPTION)( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, const char *value );

// 
// Compare methods
// 
// Specializations
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const float *outvalue, const float *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Vector *outvalue, const Vector *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Quaternion *outvalue, const Quaternion *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const char *outvalue, const char *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const EHANDLE *outvalue, const EHANDLE *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const color32 *outvalue, const color32 *invalue, int count );

//
// Describe Methods
//

// Specializations
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const short *outvalue, const short *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const int *outvalue, const int *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const bool *outvalue, const bool *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const float *outvalue, const float *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const char *outvalue, const char *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Vector* outValue, const Vector *inValue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Quaternion* outValue, const Quaternion *inValue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const EHANDLE *outvalue, const EHANDLE *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const color32 *outvalue, const color32 *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const uint8 *outvalue, const uint8 *invalue, int count );

//
// Watch Methods
//
// Specializations
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const short *outvalue,int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const int *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const bool *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const float *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Vector *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Quaternion *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const EHANDLE *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const char *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const color32 *outvalue, int count );
//
// Copy Methods
//
// specializations
template<> [[gnu::always_inline]] inline void CPredictionCopy::CopyField( difftype_t difftype, char *outvalue, const char *invalue, int count );

template< class T >
[[gnu::always_inline]] inline void CPredictionCopy::ProcessField_Compare_NoSpew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	if ( difftype == DIFFERS )
	{
		++m_nErrorCount;
	}
}

template< class T >
[[gnu::always_inline]] inline void CPredictionCopy::ProcessField_Compare_Spew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	DescribeField( pCurrentMap, pField, difftype, pOutputData, pInputData, fieldSize );
}

template< class T >
[[gnu::always_inline]] inline void CPredictionCopy::ProcessField_Describe( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	DescribeField( pCurrentMap, pField, difftype, pOutputData, pInputData, fieldSize );
}

// Text stats get to print their stuff by implementing this type of function.
typedef void (*TextStatPrintFn)( IFileSystem *pFileSys, FileHandle_t hFile, void *pUserData );
typedef void (*TextStatFileFn)();

// The CTextStatsMgr is just a collection of CTextStat's that go into the same file.
class CTextStatsMgr
{
public:
	CTextStatsMgr( void );

	// Write a file with all the registered stats.
	bool				WriteFile( IFileSystem *pFileSystem, const char *pFilename = nullptr );

	// Get the preset filename to write stats to, if none is specified when writing
	char				*GetStatsFilename( void );

	// Set the filename to write stats to, if none is specified when writing
	void				SetStatsFilename( const char *sFilename );

private:
	char	m_szStatFilename[ MAX_PATH ];
};

// This is the default CTextStatsMgr, but there can be any number of them.
extern CTextStatsMgr g_TextStatsMgr;

// Make these to register text stat functions.
class CTextStat
{
friend class CTextStatsMgr;

public:
				CTextStat();
				CTextStat( TextStatPrintFn fn, void *pUserData, CTextStatsMgr *pMgr=&g_TextStatsMgr );
				~CTextStat();

	// This can be called if you don't want to pass parameters into the constructor.
	void		Init( TextStatPrintFn printFn, void *pUserData, CTextStatsMgr *pMgr=&g_TextStatsMgr );
	void		Term();

private:

				// Special constructor to just tie off the linked list.
						CTextStat( bool bGlobalListHead );

	// The global list of CTextStats.
	static CTextStat*	GetTextStatsList();
	
	static void	RemoveFn( void *pUserData );
	
	// Link it into the global list.
	CTextStat	*m_pPrev;
	CTextStat	*m_pNext;
	
	CTextStatsMgr	*m_pMgr;
	
	TextStatPrintFn	m_PrintFn;
	void			*m_pUserData;
};

// This class registers like a ConVar and acts like an int. When the game is shutdown,
// its value will be saved in the stats file along with its name.s
class CTextStatInt
{
public:
				CTextStatInt( const char *pName, int initialValue=0, CTextStatsMgr *pMgr=&g_TextStatsMgr );

				operator int() const	{ return m_Value; }
	int			operator=( int val )	{ m_Value = val; return m_Value; }

	int			operator++()			{ m_Value++; return m_Value; }
	int			operator--()			{ m_Value--; return m_Value; }
	int			operator+=( int val )	{ m_Value += val; return m_Value; }
	int			operator-=( int val )	{ m_Value -= val; return m_Value; }
	int			operator*=( int val )	{ m_Value *= val; return m_Value; }
	int			operator/=( int val )	{ m_Value /= val; return m_Value; }

private:

	static void	PrintFn( IFileSystem *pFileSys, FileHandle_t hFile, void *pUserData );
	
private:
	
	const char		*m_pName;
	int				m_Value;

	CTextStat	m_Reg;	// Use to register ourselves.
};

// This can be registered to get a callback when the text stats mgr is saving its files.
// You can write data out to your own file in here.
class CTextStatFile
{
public:
	CTextStatFile( TextStatFileFn fn );

private:
	friend class CTextStatsMgr;

	static CTextStatFile *s_pHead;
	CTextStatFile *m_pNext;
	TextStatFileFn m_pFn;
};

class CTimedEventMgr;

//
//
// These classes provide fast timed event callbacks. To use them, make a CTimedEventMgr
// and put CEventRegister objects in your objects that want the timed events.
//
//


class IEventRegisterCallback
{
public:
	virtual void FireEvent() = 0;
};

class CEventRegister
{
friend bool TimedEventMgr_LessFunc( CEventRegister* const &a, CEventRegister* const &b );
friend class CTimedEventMgr;

public:
	CEventRegister();
	~CEventRegister();

	// Call this before ever calling SetUpdateInterval().
	void Init( CTimedEventMgr *pMgr, IEventRegisterCallback *pCallback );
	
	// Use these to start and stop getting updates.
	void SetUpdateInterval( float interval );
	void StopUpdates();

	inline bool IsRegistered() const { return m_bRegistered; }

private:

	void Reregister();	// After having an event processed, this is called to have it register for the next one.
	void Term();

private:
	
	CTimedEventMgr *m_pEventMgr;
	float m_flNextEventTime;
	float m_flUpdateInterval;
	IEventRegisterCallback *m_pCallback;
	bool m_bRegistered;
};

class CTimedEventMgr
{
friend class CEventRegister;

public:
	CTimedEventMgr();

	// Call this each frame to fire events.
	void FireEvents();

private:

	// Things used by CEventRegister.
	void RegisterForNextEvent( CEventRegister *pEvent );
	void RemoveEvent( CEventRegister *pEvent );	
	
private:	
	
	// Events, heap-ordered by the time at which they will fire (ks::heap_*).
	std::vector<CEventRegister*> m_Events;
};

class CBaseEntity;

using namespace ::ks::reflect;

//
// Lightweight base class for networkable data on the server.
//
class CServerNetworkProperty : public IServerNetworkable, public IEventRegisterCallback
{
public:
	DECLARE_CLASS_NOBASE( CServerNetworkProperty );

public:
	CServerNetworkProperty();
	virtual	~CServerNetworkProperty();

public:
// IServerNetworkable implementation.
	virtual IHandleEntity  *GetEntityHandle( );
	virtual edict_t			*GetEdict() const;
	virtual CBaseNetworkable* GetBaseNetworkable();
	virtual IServerUnknown*	GetBaseEntity();
	virtual ServerClass*	GetServerClass();
	virtual const char*		GetClassName() const;
	virtual void			Release();
	virtual int				AreaNum() const;
	virtual PVSInfo_t*		GetPVSInfo();

public:
	// Other public methods
	void Init( CBaseEntity *pEntity );
	void CacheServerClass();

	void AttachEdict( edict_t *pRequiredEdict = nullptr );
	
	// Methods to get the entindex + edict
	int	entindex() const;
	edict_t *edict();
	const edict_t *edict() const;

	// Sets the edict pointer (for swapping edicts)
	void SetEdict( edict_t *pEdict );

	// All these functions call through to CNetStateMgr. 
	// See CNetStateMgr for details about these functions.
	void NetworkStateForceUpdate();
	void NetworkStateChanged();
	void NetworkStateChanged( unsigned short offset );

	// Marks the PVS information dirty
	void MarkPVSInformationDirty();

	// Marks for deletion
	void MarkForDeletion();
	bool IsMarkedForDeletion() const;

	// Sets the network parent
	void SetNetworkParent( EHANDLE hParent );
	CServerNetworkProperty* GetNetworkParent();

	// This is useful for entities that don't change frequently or that the client
	// doesn't need updates on very often. If you use this mode, the server will only try to
	// detect state changes every N seconds, so it will save CPU cycles and bandwidth.
	//
	// Note: N must be less than AUTOUPDATE_MAX_TIME_LENGTH.
	//
	// Set back to zero to disable the feature.
	//
	// This feature works on top of manual mode. 
	// - If you turn it on and manual mode is off, it will autodetect changes every N seconds.
	// - If you turn it on and manual mode is on, then every N seconds it will only say there
	//   is a change if you've called NetworkStateChanged.
	void			SetUpdateInterval( float N );

	// You can use this to override any entity's ShouldTransmit behavior.
	// void SetTransmitProxy( CBaseTransmitProxy *pProxy );

	// This version does a PVS check which also checks for connected areas
	bool IsInPVS( const CCheckTransmitInfo *pInfo );

	// This version doesn't do the area check
	bool IsInPVS( const edict_t *pRecipient, const void *pvs, int pvssize );

	// Called by the timed event manager when it's time to detect a state change.
	virtual void FireEvent();

	// Recomputes PVS information
	void RecomputePVSInformation();

private:
	// Detaches the edict.. should only be called by CBaseNetworkable's destructor.
	void DetachEdict();
	CBaseEntity *GetOuter();

	// Marks the networkable that it will should transmit
	void SetTransmit( CCheckTransmitInfo *pInfo );

private:
	CBaseEntity *m_pOuter;
	// CBaseTransmitProxy *m_pTransmitProxy;
	edict_t	*m_pPev;
	PVSInfo_t m_PVSInfo;
	ServerClass *m_pServerClass;

	// NOTE: This state is 'owned' by the entity. It's only copied here
	// also to help improve cache performance in networking code.
	EHANDLE m_hParent;

	// Counters for SetUpdateInterval.
	CEventRegister	m_TimerEvent;
	bool m_bPendingStateChange : 1;

//	friend class CBaseTransmitProxy;
};

typedef enum { GLOBAL_OFF = 0, GLOBAL_ON = 1, GLOBAL_DEAD = 2 } GLOBALESTATE;

void		GlobalEntity_SetState( int globalIndex, GLOBALESTATE state );
void		GlobalEntity_SetMap( int globalIndex, string_t mapname );
int			GlobalEntity_Add( const char *pGlobalname, const char *pMapName, GLOBALESTATE state );

int			GlobalEntity_GetIndex( const char *pGlobalname );
GLOBALESTATE GlobalEntity_GetState( int globalIndex );
const char	*GlobalEntity_GetMap( int globalIndex );
const char	*GlobalEntity_GetName( int globalIndex );

int GlobalEntity_GetCounter( int globalIndex );
void GlobalEntity_SetCounter( int globalIndex, int counter );
int GlobalEntity_AddToCounter( int globalIndex, int delta );

int GlobalEntity_GetFlags( int globalIndex );
void GlobalEntity_SetFlags( int globalIndex, int flags );
void GlobalEntity_AddFlags( int globalIndex, int flags );
void GlobalEntity_RemoveFlags( int globalIndex, int flags );

int			GlobalEntity_GetNumGlobals( void );
void		GlobalEntity_EnableStateUpdates( bool bEnable );

inline int GlobalEntity_Add( string_t globalname, string_t mapName, GLOBALESTATE state )
{
	return GlobalEntity_Add( STRING(globalname), STRING(mapName), state );
}

inline int GlobalEntity_GetIndex( string_t globalname )
{
	return GlobalEntity_GetIndex( STRING(globalname) );
}

inline int GlobalEntity_IsInTable( string_t globalname )
{
	return GlobalEntity_GetIndex( STRING(globalname) ) >= 0 ? true : false;
}

inline int GlobalEntity_IsInTable( const char *pGlobalname )
{
	return GlobalEntity_GetIndex( pGlobalname ) >= 0 ? true : false;
}

inline void GlobalEntity_SetState( string_t globalname, GLOBALESTATE state )
{
	GlobalEntity_SetState( GlobalEntity_GetIndex( globalname ), state );
}

inline void GlobalEntity_SetMap( string_t globalname, string_t mapname )
{
	GlobalEntity_SetMap( GlobalEntity_GetIndex( globalname ), mapname );
}

inline GLOBALESTATE GlobalEntity_GetState( string_t globalname )
{
	return GlobalEntity_GetState( GlobalEntity_GetIndex( globalname ) );
}

inline GLOBALESTATE GlobalEntity_GetState( const char *pGlobalName )
{
	return GlobalEntity_GetState( GlobalEntity_GetIndex( pGlobalName ) );
}

inline int GlobalEntity_GetCounter( string_t globalname )
{
	return GlobalEntity_GetCounter( GlobalEntity_GetIndex( globalname ) );
}

inline int GlobalEntity_GetCounter( const char *pGlobalName )
{
	return GlobalEntity_GetCounter( GlobalEntity_GetIndex( pGlobalName ) );
}

inline void GlobalEntity_SetCounter( string_t globalname, int counter )
{
	GlobalEntity_SetCounter( GlobalEntity_GetIndex( globalname ), counter );
}

inline void GlobalEntity_SetCounter( const char *pGlobalName, int counter )
{
	GlobalEntity_SetCounter( GlobalEntity_GetIndex( pGlobalName ), counter );
}

inline int GlobalEntity_AddToCounter( string_t globalname, int delta )
{
	return GlobalEntity_AddToCounter( GlobalEntity_GetIndex( globalname ), delta );
}

inline int GlobalEntity_AddToCounter( const char *pGlobalName, int delta )
{
	return GlobalEntity_AddToCounter( GlobalEntity_GetIndex( pGlobalName ), delta );
}

inline int GlobalEntity_GetFlags( string_t globalname )
{
	return GlobalEntity_GetFlags( GlobalEntity_GetIndex( globalname ) );
}

inline int GlobalEntity_GetFlags( const char *pGlobalName )
{
	return GlobalEntity_GetFlags( GlobalEntity_GetIndex( pGlobalName ) );
}

inline void GlobalEntity_SetFlags( string_t globalname, int flags )
{
	GlobalEntity_SetFlags( GlobalEntity_GetIndex( globalname ), flags );
}

inline void GlobalEntity_SetFlags( const char *pGlobalName, int flags )
{
	GlobalEntity_SetFlags( GlobalEntity_GetIndex( pGlobalName ), flags );
}

inline void GlobalEntity_AddFlags( string_t globalname, int flags )
{
	GlobalEntity_AddFlags( GlobalEntity_GetIndex( globalname ), flags );
}

inline void GlobalEntity_AddFlags( const char *pGlobalName, int flags )
{
	GlobalEntity_AddFlags( GlobalEntity_GetIndex( pGlobalName ), flags );
}

inline void GlobalEntity_RemoveFlags( string_t globalname, int flags )
{
	GlobalEntity_RemoveFlags( GlobalEntity_GetIndex( globalname ), flags );
}

inline void GlobalEntity_RemoveFlags( const char *pGlobalName, int flags )
{
	GlobalEntity_RemoveFlags( GlobalEntity_GetIndex( pGlobalName ), flags );
}

inline GLOBALESTATE GlobalEntity_GetStateByIndex( int iIndex )
{
	return GlobalEntity_GetState( iIndex );
}

void ResetGlobalState( void );

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

//-----------------------------------------------------------------------------
// Purpose: Used for tracking many to one ground entity chains ( many ents can share a single ground entity )
//-----------------------------------------------------------------------------
struct groundlink_t
{
	EHANDLE					entity;
	groundlink_t			*nextLink;
	groundlink_t			*prevLink;
};

class CBaseEntity;

// Purpose: Contains the set of functions for manipulating entity hierarchies.


void		UnlinkFromParent( CBaseEntity *pRemove );
void		TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent );
void		LinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
void		UnlinkAllChildren( CBaseEntity *pParent );
int			GetAllChildren( CBaseEntity *pParent, std::vector< CBaseEntity * > &list );
bool		EntityIsParentOf( CBaseEntity *pParent, CBaseEntity *pEntity );
int			GetAllInHierarchy( CBaseEntity *pParent, std::vector< CBaseEntity * > &list );

typedef struct activityentry_s activityentry_t;

class CActivityRemap
{
public:

	CActivityRemap()
	{
		pExtraBlock = nullptr;
	}

	void SetExtraKeyValueBlock ( KeyValues *pKVBlock )
	{
		pExtraBlock = pKVBlock;
	}

	KeyValues *GetExtraKeyValueBlock ( void ) { return pExtraBlock; }

	Activity 		activity;
	Activity		mappedActivity;

private:

	KeyValues		*pExtraBlock;
};

class CActivityRemapCache
{
public:

	CActivityRemapCache()
	{
	}

	CActivityRemapCache( const CActivityRemapCache& src );

	CActivityRemapCache& operator = ( const CActivityRemapCache& src )
	{
		if ( this == &src )
			return *this;

		int c = std::ssize( src.m_cachedActivityRemaps );
		for ( int i = 0; i < c; i++ )
		{
			m_cachedActivityRemaps.push_back( src.m_cachedActivityRemaps[ i ] );
		}

		return *this;
	}

	std::vector< CActivityRemap > m_cachedActivityRemaps;
};

void UTIL_LoadActivityRemapFile( const char *filename, const char *section, std::vector< CActivityRemap > &entries );

//=========================================================
//=========================================================
extern void ActivityList_Init( void );
extern void ActivityList_Free( void );
extern bool ActivityList_RegisterSharedActivity( const char *pszActivityName, int iActivityIndex );
extern Activity ActivityList_RegisterPrivateActivity( const char *pszActivityName );
extern int ActivityList_IndexForName( const char *pszActivityName );
extern const char *ActivityList_NameForIndex( int iActivityIndex );
extern int ActivityList_HighestIndex();

// This macro guarantees that the names of each activity and the constant used to
// reference it in the code are identical.

// Implemented in shared code
extern void ActivityList_RegisterSharedActivities( void );

class CBaseEntity;

using namespace ::ks::reflect;


// Purpose: Hooks and classes for the support of humanoid NPCs with
//			groovy facial animation capabilities, aka, "Actors"

//-----------------------------------------------------------------------------
// CAI_BaseActor
//
// Purpose: The base class for all facially expressive NPCS.
//
//-----------------------------------------------------------------------------

class CAI_InterestTarget_t
{
public:
	enum CAI_InterestTarget_e
	{
		LOOKAT_ENTITY = 0,
		LOOKAT_POSITION, 
		LOOKAT_BOTH
	};

public:
	bool			IsThis( CBaseEntity *pThis );
	const Vector	&GetPosition( void );
	bool			IsActive( void );
	float			Interest( void );

public:
	CAI_InterestTarget_e	m_eType; // ????

	EHANDLE		m_hTarget;
	Vector		m_vecPosition;
	float		m_flStartTime;
	float		m_flEndTime;
	float		m_flRamp;
	float		m_flInterest;

	DECLARE_SIMPLE_DATADESC();
};

class CAI_InterestTarget : public std::vector< CAI_InterestTarget_t >
{
public:
	void Add( CBaseEntity *pTarget, float flImportance, float flDuration, float flRamp );
	void Add( const Vector &vecPosition, float flImportance, float flDuration, float flRamp );
	void Add( CBaseEntity *pTarget, const Vector &vecPosition, float flImportance, float flDuration, float flRamp );
	int Find( CBaseEntity *pTarget );
	
	void Cleanup( void );

private:
	void Add( CAI_InterestTarget_t::CAI_InterestTarget_e type, CBaseEntity *pTarget, const Vector &vecPosition, float flImportance, float flDuration, float flRamp );
};

//-----------------------------------------------------------------------------

// using ResponseRules::IResponseFilter;
// using ResponseRules::IResponseSystem;

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void SendProxy_Solid( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void SendProxy_SolidFlags( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void RecvProxy_IntDirtySurround( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_OBBMaxs( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_OBBMins( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_VectorDirtySurround( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Solid( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_SolidFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );

//-----------------------------------------------------------------------------
// Force spatial partition updates (to avoid threading problems caused by lazy update)
//-----------------------------------------------------------------------------
void UpdateDirtySpatialPartitionEntities();

//-----------------------------------------------------------------------------
// Specifies how to compute the surrounding box
//-----------------------------------------------------------------------------
enum SurroundingBoundsType_t
{
	USE_OBB_COLLISION_BOUNDS = 0,
	USE_BEST_COLLISION_BOUNDS,		// Always use the best bounds (most expensive)
	USE_HITBOXES,
	USE_SPECIFIED_BOUNDS,
	USE_GAME_CODE,
	USE_ROTATION_EXPANDED_BOUNDS,
	USE_COLLISION_BOUNDS_NEVER_VPHYSICS,
	USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS,

	SURROUNDING_TYPE_BIT_COUNT = 3
};

//-----------------------------------------------------------------------------
// Encapsulates collision representation for an entity
//-----------------------------------------------------------------------------
class CCollisionProperty : public ICollideable
{
	[[= ks::reflect::NetTable{ .name = "DT_CollisionProperty", .base = false } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS_NOBASE( CCollisionProperty );
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_PREDICTABLE();

	DECLARE_DATADESC();

public:
	CCollisionProperty();
	~CCollisionProperty();

	void Init( CBaseEntity *pEntity );

	// Methods of ICollideable
	virtual IHandleEntity	*GetEntityHandle();
 	virtual const Vector&	OBBMins( ) const;
	virtual const Vector&	OBBMaxs( ) const;
	virtual void			WorldSpaceTriggerBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs ) const;
	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual bool			TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual int				GetCollisionModelIndex();
	virtual const model_t*	GetCollisionModel();
	virtual const Vector&	GetCollisionOrigin() const;
	virtual const QAngle&	GetCollisionAngles() const;
	virtual const matrix3x4_t&	CollisionToWorldTransform() const;
	virtual SolidType_t		GetSolid() const;
	virtual int				GetSolidFlags() const;
	virtual IClientUnknown*	GetIClientUnknown();
	virtual int				GetCollisionGroup() const;
	virtual void			WorldSpaceSurroundingBounds( Vector *pVecMins, Vector *pVecMaxs );
	virtual uint			GetRequiredTriggerFlags() const;
	virtual const matrix3x4_t *GetRootParentToWorldTransform() const;
	virtual IPhysicsObject	*GetVPhysicsObject() const;

public:
	// Spatial partition management
	void						CreatePartitionHandle();
	void						DestroyPartitionHandle();
	SpatialPartitionHandle_t	GetPartitionHandle() const;

	// Marks the spatial partition dirty
	void			MarkPartitionHandleDirty();

	// Sets the collision bounds + the size (OBB)
	void			SetCollisionBounds( const Vector& mins, const Vector &maxs );

	// Sets special trigger bounds. The bloat amount indicates how much bigger the 
	// trigger bounds should be beyond the bounds set in SetCollisionBounds
	// This method will also set the FSOLID flag FSOLID_USE_TRIGGER_BOUNDS
	void			UseTriggerBounds( bool bEnable, float flBloat = 0.0f );

	// Sets the method by which the surrounding collision bounds is set
	// You must pass in values for mins + maxs if you select the USE_SPECIFIED_BOUNDS type. 
	void			SetSurroundingBoundsType( SurroundingBoundsType_t type, const Vector *pMins = nullptr, const Vector *pMaxs = nullptr );
	SurroundingBoundsType_t GetSurroundingBoundsType() const;

	// Sets the solid type (which type of collision representation)
	void			SetSolid( SolidType_t val );

	// Methods related to size. The OBB here is measured in CollisionSpace
	// (specified by GetCollisionToWorld)
	const Vector&	OBBSize( ) const;

	// Returns a radius (or the square of the radius) of a sphere 
	// *centered at the world space center* bounding the collision representation 
	// of the entity. NOTE: The world space center *may* move when the entity rotates.
	float			BoundingRadius() const;
	float			BoundingRadius2D() const;

	// Returns the center of the OBB in collision space
	const Vector &	OBBCenter( ) const;

	// center point of entity measured in world space
	// NOTE: This point *may* move when the entity moves depending on
	// which solid type is being used.
	const Vector &	WorldSpaceCenter( ) const;

	// Methods related to solid flags
	void			ClearSolidFlags( void );	
	void			RemoveSolidFlags( int flags );
	void			AddSolidFlags( int flags );
	bool			IsSolidFlagSet( int flagMask ) const;
	void		 	SetSolidFlags( int flags );
	bool			IsSolid() const;

	// Updates the spatial partition
	void			UpdatePartition( );

	// Are the bounds defined in entity space?
	bool			IsBoundsDefinedInEntitySpace() const;

	// Transforms a point in OBB space to world space
	const Vector &	CollisionToWorldSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a point in world space to OBB space
	const Vector &	WorldToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a direction in world space to OBB space
	const Vector &	WorldDirectionToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Selects a random point in the bounds given the normalized 0-1 bounds 
	void			RandomPointInBounds( const Vector &vecNormalizedMins, const Vector &vecNormalizedMaxs, Vector *pPoint) const;

	// Is a worldspace point within the bounds of the OBB?
	bool			IsPointInBounds( const Vector &vecWorldPt ) const;

	// Computes a bounding box in world space surrounding the collision bounds
	void			WorldSpaceAABB( Vector *pWorldMins, Vector *pWorldMaxs ) const;

	// Computes a "normalized" point (range 0,0,0 - 1,1,1) in collision space
	// Useful for things like getting a point 75% of the way along z on the OBB, for example
	const Vector &	NormalizedToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Computes a "normalized" point (range 0,0,0 - 1,1,1) in world space
	const Vector &	NormalizedToWorldSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a point in world space to normalized space
	const Vector &	WorldToNormalizedSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a point in collision space to normalized space
	const Vector &	CollisionToNormalizedSpace( const Vector &in, Vector *pResult ) const;

	// Computes the nearest point in the OBB to a point specified in world space
	void			CalcNearestPoint( const Vector &vecWorldPt, Vector *pVecNearestWorldPt ) const;

	// Computes the distance from a point in world space to the OBB
	float			CalcDistanceFromPoint( const Vector &vecWorldPt ) const;
	float			CalcSqrDistanceFromPoint( const Vector &vecWorldPt ) const;

	// Does a rotation make us need to recompute the surrounding box?
	bool			DoesRotationInvalidateSurroundingBox( ) const;

	// Does VPhysicsUpdate make us need to recompute the surrounding box?
	bool			DoesVPhysicsInvalidateSurroundingBox( ) const;

	// Does a sequence change make us need to recompute the surrounding box?
	bool			DoesSequenceChangeInvalidateSurroundingBox( ) const;

	// Marks the entity has having a dirty surrounding box
	void			MarkSurroundingBoundsDirty();

	// Compute the largest dot product of the OBB and the specified direction vector
	float			ComputeSupportMap( const Vector &vecDirection ) const;

private:
	// Transforms an AABB measured in collision space to a box that surrounds it in world space
	void CollisionAABBToWorldAABB( const Vector &entityMins, const Vector &entityMaxs, Vector *pWorldMins, Vector *pWorldMaxs ) const;

	// Expand trigger bounds..
	void ComputeVPhysicsSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Expand trigger bounds..
	bool ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );
	bool ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds based on the current sequence box
	void ComputeOBBBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds from the current sequence box
	void ComputeRotationExpandedSequenceBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds based on whatever algorithm we want...
	void ComputeCollisionSurroundingBox( bool bUseVPhysics, Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds from the OBB (not vphysics)
	void ComputeRotationExpandedBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds based on whatever algorithm we want...
	void ComputeSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Check for untouch
	void CheckForUntouch();

	// Updates the spatial partition
	void UpdateServerPartitionMask( );
	uint ComputeServerPartitionMask( );
	inline const Vector&	GetCollisionOrigin_Inline() const;

	// Outer
	CBaseEntity *GetOuter();
	const CBaseEntity *GetOuter() const;

private:
	CBaseEntity *m_pOuter;

// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
	CNetworkVector( m_vecMins, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Proxy<RecvProxy_OBBMins, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	CNetworkVector( m_vecMaxs, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Proxy<RecvProxy_OBBMaxs, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	CNetworkVar( unsigned short, m_usSolidFlags, [[= ks::reflect::Net{ .bits = FSOLID_MAX_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_SolidFlags, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Proxy<RecvProxy_SolidFlags, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	// One of the SOLID_ defines. Use GetSolid/SetSolid.
	CNetworkVar( unsigned char, m_nSolidType, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "solid" } ]] [[= ks::reflect::Proxy<SendProxy_Solid, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Proxy<RecvProxy_Solid, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);			
	CNetworkVar( unsigned char , m_triggerBloat, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
// END PREDICTION DATA COMPACTION

	float m_flRadius;

	// Spatial partition
	SpatialPartitionHandle_t m_Partition;
	CNetworkVar( unsigned char, m_nSurroundType, [[= ks::reflect::Net{ .bits = SURROUNDING_TYPE_BIT_COUNT, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<RecvProxy_IntDirtySurround, ks::reflect::WireSide::Recv>{} ]]);

	// SUCKY: We didn't use to have to store this previously
	// but storing it here means that we can network it + avoid a ton of
	// client-side mismatch problems
	CNetworkVector( m_vecSpecifiedSurroundingMins, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecSpecifiedSurroundingMaxs, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Proxy<RecvProxy_VectorDirtySurround, ks::reflect::WireSide::Recv>{} ]]);

	// Cached off world-aligned surrounding bounds
	Vector	m_vecSurroundingMins;
	Vector	m_vecSurroundingMaxs;

	// pointer to the entity's physics object (vphysics.dll)
	//IPhysicsObject	*m_pPhysicsObject;
	
	friend class CBaseEntity;
};

//-----------------------------------------------------------------------------
// For networking this bad boy
//-----------------------------------------------------------------------------
EXTERN_SEND_TABLE( DT_CollisionProperty );

class CBaseEntity;


//
// A variant class for passing data in entity input/output connections.
//
class variant_t
{
	union
	{
		bool bVal;
		string_t iszVal;
		int iVal;
		float flVal;
		float vecVal[3];
		color32 rgbaVal;
	};
	CHandle<CBaseEntity> eVal; // this can't be in the union because it has a constructor.

	fieldtype_t fieldType;

public:

	// constructor
	variant_t() : fieldType(FIELD_VOID), iVal(0) {}

	inline bool Bool( void ) const						{ return( fieldType == FIELD_BOOLEAN ) ? bVal : false; }
	inline const char *String( void ) const				{ return( fieldType == FIELD_STRING ) ? STRING(iszVal) : ToString(); }
	inline string_t StringID( void ) const				{ return( fieldType == FIELD_STRING ) ? iszVal : NULL_STRING; }
	inline int Int( void ) const						{ return( fieldType == FIELD_INTEGER ) ? iVal : 0; }
	inline float Float( void ) const					{ return( fieldType == FIELD_FLOAT ) ? flVal : 0; }
	inline const CHandle<CBaseEntity> &Entity(void) const;
	inline color32 Color32(void) const					{ return rgbaVal; }
	inline void Vector3D(Vector &vec) const;

	fieldtype_t FieldType( void ) { return fieldType; }

	void SetBool( bool b ) { bVal = b; fieldType = FIELD_BOOLEAN; }
	void SetString( string_t str ) { iszVal = str, fieldType = FIELD_STRING; }
	void SetInt( int val ) { iVal = val, fieldType = FIELD_INTEGER; }
	void SetFloat( float val ) { flVal = val, fieldType = FIELD_FLOAT; }
	void SetEntity( CBaseEntity *val );
	void SetVector3D( const Vector &val ) { vecVal[0] = val[0]; vecVal[1] = val[1]; vecVal[2] = val[2]; fieldType = FIELD_VECTOR; }
	void SetPositionVector3D( const Vector &val ) { vecVal[0] = val[0]; vecVal[1] = val[1]; vecVal[2] = val[2]; fieldType = FIELD_POSITION_VECTOR; }
	void SetColor32( color32 val ) { rgbaVal = val; fieldType = FIELD_COLOR32; }
	void SetColor32( int r, int g, int b, int a ) { rgbaVal.r = r; rgbaVal.g = g; rgbaVal.b = b; rgbaVal.a = a; fieldType = FIELD_COLOR32; }
	void Set( fieldtype_t ftype, void *data );
	void SetOther( void *data );
	bool Convert( fieldtype_t newType );

protected:

	//
	// Returns a string representation of the value without modifying the variant.
	//
	const char *ToString( void ) const;

	friend class CVariantSaveDataOps;
};

//-----------------------------------------------------------------------------
// Purpose: Returns this variant as a vector.
//-----------------------------------------------------------------------------
void variant_t::Vector3D(Vector &vec) const
{
	if (( fieldType == FIELD_VECTOR ) || ( fieldType == FIELD_POSITION_VECTOR ))
	{
		vec[0] =  vecVal[0];
		vec[1] =  vecVal[1];
		vec[2] =  vecVal[2];
	}
	else
	{
		vec = vec3_origin;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns this variant as an EHANDLE.
//-----------------------------------------------------------------------------
const CHandle<CBaseEntity> &variant_t::Entity(void) const
{
	if ( fieldType == FIELD_EHANDLE )
		return eVal;

	static const CHandle<CBaseEntity> hNull(INVALID_EHANDLE);
	return(hNull);
}

//-----------------------------------------------------------------------------
// Purpose: Used to request a value, or a set of values, from a set of entities.
//			used when a multi-input variable needs to refresh it's inputs
//-----------------------------------------------------------------------------
class CMultiInputVar
{
public:
	CMultiInputVar() : m_InputList(nullptr) {}
	~CMultiInputVar();

	struct inputitem_t
	{
		variant_t value;	// local copy of variable (maybe make this a variant?) 
		int	outputID;		// the ID number of the output that sent this
		inputitem_t *next;

		// allocate and free from MPool memory
		static void *operator new( std::size_t stAllocBlock );	
		static void *operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );
		static void operator delete( void *pMem );
		static void operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine ) { operator delete(pMem); }
	};

	inputitem_t *m_InputList;	// list of data
	int m_bUpdatedThisFrame;

	void AddValue( variant_t newVal, int outputID );
	
	DECLARE_SIMPLE_DATADESC();
};

class IEntityListener;

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

class CBaseEntityClassList
{
public:
	CBaseEntityClassList();
	~CBaseEntityClassList();
	virtual void LevelShutdownPostEntity() = 0;

	CBaseEntityClassList *m_pNextClassList;
};

template< class T >
class CEntityClassList : public CBaseEntityClassList
{
public:
	virtual void LevelShutdownPostEntity()  { m_pClassList = nullptr; }

	void Insert( T *pEntity )
	{
		pEntity->m_pNext = m_pClassList;
		m_pClassList = pEntity;
	}

	void Remove( T *pEntity )
	{
		T **pPrev = &m_pClassList;
		T *pCur = *pPrev;
		while ( pCur )
		{
			if ( pCur == pEntity )
			{
				*pPrev = pCur->m_pNext;
				return;
			}
			pPrev = &pCur->m_pNext;
			pCur = *pPrev;
		}
	}

	static T *m_pClassList;
};

// Derive a class from this if you want to filter entity list searches
class IEntityFindFilter
{
public:
	virtual bool ShouldFindEntity( CBaseEntity *pEntity ) = 0;
	virtual CBaseEntity *GetFilterResult( void ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: a global list of all the entities in the game.  All iteration through
//			entities is done through this object.
//-----------------------------------------------------------------------------
class CGlobalEntityList : public CBaseEntityList
{
public:
private:
	int m_iHighestEnt; // the topmost used array index
	int m_iNumEnts;
	int m_iNumEdicts;

	bool m_bClearingEntities;
	std::vector< IEntityListener * >	m_entityListeners;

public:
	CBaseHandle AddNetworkableEntity( IHandleEntity *pEnt, int index, int iForcedSerialNum = -1 );
	CBaseHandle AddNonNetworkableEntity( IHandleEntity *pEnt );

	void UpdateName( IHandleEntity *pEnt, CBaseHandle hEnt );
	void UpdateName( IHandleEntity *pEnt );

	IServerNetworkable* GetServerNetworkable( CBaseHandle hEnt ) const;
	CBaseNetworkable* GetBaseNetworkable( CBaseHandle hEnt ) const;
	CBaseEntity* GetBaseEntity( CBaseHandle hEnt ) const;
	edict_t* GetEdict( CBaseHandle hEnt ) const;
	
	int NumberOfEntities( void );
	int NumberOfEdicts( void );

	// mark an entity as deleted
	void AddToDeleteList( IServerNetworkable *ent );
	// call this before and after each frame to delete all of the marked entities.
	void CleanupDeleteList( void );
	int ResetDeleteList( void );

	// frees all entities in the game
	void Clear( void );

	// Returns true while in the Clear() call.
	bool	IsClearingEntities()	{return m_bClearingEntities;}
	
	// add a class that gets notified of entity events
	void AddListenerEntity( IEntityListener *pListener );
	void RemoveListenerEntity( IEntityListener *pListener );

	void ReportEntityFlagsChanged( CBaseEntity *pEntity, unsigned int flagsOld, unsigned int flagsNow );
	// Schedule this entity for notification once client messages have been sent
	void AddPostClientMessageEntity( CBaseEntity *pEntity );
	void PostClientMessagesSent();

	// entity is about to be removed, notify the listeners
	void NotifyCreateEntity( CBaseEntity *pEnt );
	void NotifySpawn( CBaseEntity *pEnt );
	void NotifyRemoveEntity( CBaseEntity *pEnt );
	// iteration functions

	// returns the next entity after pCurrentEnt;  if pCurrentEnt is NULL, return the first entity
	CBaseEntity *NextEnt( CBaseEntity *pCurrentEnt );
	CBaseEntity *FirstEnt() { return NextEnt(nullptr); }

	// returns the next entity of the specified class, using RTTI
	template< class T >
	T *NextEntByClass( T *start )
	{
		for ( CBaseEntity *x = NextEnt( start ); x; x = NextEnt( x ) )
		{
			start = dynamic_cast<T*>( x );
			if ( start )
				return start;
		}
		return nullptr;
	}

	// search functions
	bool		 IsEntityPtr( void *pTest );
	CBaseEntity *FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName );
	CBaseEntity *FindEntityByName( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr, IEntityFindFilter *pFilter = nullptr );
	CBaseEntity *FindEntityByName( CBaseEntity *pStartEntity, string_t iszName, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr, IEntityFindFilter *pFilter = nullptr )
	{
		return FindEntityByName( pStartEntity, STRING(iszName), pSearchingEntity, pActivator, pCaller, pFilter );
	}
	CBaseEntity *FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius );
	CBaseEntity *FindEntityByTarget( CBaseEntity *pStartEntity, const char *szName );
	CBaseEntity *FindEntityByModel( CBaseEntity *pStartEntity, const char *szModelName );
	CBaseEntity	*FindEntityByOutputTarget( CBaseEntity *pStartEntity, string_t iTarget );

	CBaseEntity *FindEntityByNameNearest( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr );
	CBaseEntity *FindEntityByNameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr );
	CBaseEntity *FindEntityByClassnameNearest( const char *szName, const Vector &vecSrc, float flRadius );
	CBaseEntity *FindEntityByClassnameNearest2D( const char *szName, const Vector &vecSrc, float flRadius );
	CBaseEntity *FindEntityByClassnameWithin( CBaseEntity *pStartEntity , const char *szName, const Vector &vecSrc, float flRadius );
	CBaseEntity *FindEntityByClassnameWithin( CBaseEntity *pStartEntity , const char *szName, const Vector &vecMins, const Vector &vecMaxs );

	CBaseEntity *FindEntityGeneric( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr );
	CBaseEntity *FindEntityGenericWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr );
	CBaseEntity *FindEntityGenericNearest( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr );
	
	CBaseEntity *FindEntityNearestFacing( const Vector &origin, const Vector &facing, float threshold);
	CBaseEntity *FindEntityClassNearestFacing( const Vector &origin, const Vector &facing, float threshold, char *classname);
	CBaseEntity *FindEntityByNetname( CBaseEntity *pStartEntity, const char *szModelName );

	CBaseEntity *FindEntityProcedural( const char *szName, CBaseEntity *pSearchingEntity = nullptr, CBaseEntity *pActivator = nullptr, CBaseEntity *pCaller = nullptr );
	
	// Fast versions that require a (real) string_t, and won't do wildcarding
	CBaseEntity *FindEntityByClassnameFast( CBaseEntity *pStartEntity, string_t iszClassname );
	CBaseEntity *FindEntityByClassnameNearestFast( string_t iszClassname, const Vector &vecSrc, float flRadius );
	CBaseEntity *FindEntityByNameFast( CBaseEntity *pStartEntity, string_t iszName );

	CGlobalEntityList();

// CBaseEntityList overrides.
protected:

	virtual void OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle );
	virtual void OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle );

};

extern CGlobalEntityList gEntList;

//-----------------------------------------------------------------------------
// Inlines.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Common finds
//-----------------------------------------------------------------------------
// Purpose: Simple object for storing a list of objects
//-----------------------------------------------------------------------------
struct entitem_t
{
	EHANDLE hEnt;
	struct entitem_t *pNext;

	// uses pool memory
	static void* operator new( std::size_t stAllocateBlock );
	static void *operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );
	static void operator delete( void *pMem );
	static void operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine ) { operator delete( pMem ); }
};

class CEntityList
{
public:
	CEntityList();
	~CEntityList();

	int m_iNumItems;
	entitem_t *m_pItemList;	// null terminated singly-linked list

	void AddEntity( CBaseEntity * );
	void DeleteEntity( CBaseEntity * );
};

enum notify_system_event_t
{
	NOTIFY_EVENT_TELEPORT = 0,
	NOTIFY_EVENT_DESTROY,
};

struct notify_teleport_params_t
{
	Vector prevOrigin;
	QAngle prevAngles;
	bool physicsRotate;
};

struct notify_destroy_params_t
{
};

struct notify_system_event_params_t
{
	union
	{
		const notify_teleport_params_t *pTeleport;
		const notify_destroy_params_t *pDestroy;
	};
	notify_system_event_params_t( const notify_teleport_params_t *pInTeleport ) { pTeleport = pInTeleport; }
	notify_system_event_params_t( const notify_destroy_params_t *pInDestroy ) { pDestroy = pInDestroy; }
};

class INotify
{
public:
	// Add notification for an entity
	virtual void AddEntity( CBaseEntity *pNotify, CBaseEntity *pWatched ) = 0;

	// Remove notification for an entity
	virtual void RemoveEntity( CBaseEntity *pNotify, CBaseEntity *pWatched ) = 0;

	// Call the named input in each entity who is watching pEvent's status
	virtual void ReportNamedEvent( CBaseEntity *pEntity, const char *pEventName ) = 0;

	// System events don't make sense as inputs, so are handled through a generic notify function
	virtual void ReportSystemEvent( CBaseEntity *pEntity, notify_system_event_t eventType, const notify_system_event_params_t &params ) = 0;

	inline void ReportDestroyEvent( CBaseEntity *pEntity )
	{
		notify_destroy_params_t destroy;
		ReportSystemEvent( pEntity, NOTIFY_EVENT_DESTROY, notify_system_event_params_t(&destroy) );
	}
	
	inline void ReportTeleportEvent( CBaseEntity *pEntity, const Vector &prevOrigin, const QAngle &prevAngles, bool physicsRotate )
	{
		notify_teleport_params_t teleport;
		teleport.prevOrigin = prevOrigin;
		teleport.prevAngles = prevAngles;
		teleport.physicsRotate = physicsRotate;
		ReportSystemEvent( pEntity, NOTIFY_EVENT_TELEPORT, notify_system_event_params_t(&teleport) );
	}
	
	// Remove this entity from the notify list
	virtual void ClearEntity( CBaseEntity *pNotify ) = 0;
};

// Implement this class and register with gEntList to receive entity create/delete notification
class IEntityListener
{
public:
	virtual void OnEntityCreated( CBaseEntity *pEntity ) {};
	virtual void OnEntitySpawned( CBaseEntity *pEntity ) {};
	virtual void OnEntityDeleted( CBaseEntity *pEntity ) {};
};

// singleton
extern INotify *g_pNotify;

void EntityTouch_Add( CBaseEntity *pEntity );
void EntityTouch_Remove( CBaseEntity *pEntity );
int AimTarget_ListCount();
int AimTarget_ListCopy( CBaseEntity *pList[], int listMax );
CBaseEntity *AimTarget_ListElement( int iIndex );
void AimTarget_ForceRepopulateList();

void SimThink_EntityChanged( CBaseEntity *pEntity );
int SimThink_ListCount();
int SimThink_ListCopy( CBaseEntity *pList[], int listMax );

class CEventAction;

class CBaseEntity;

// Purpose: Declares basic entity communications classes, for input/output of data
//			between entities


inline constexpr auto EVENT_FIRE_ALWAYS = -1;

//-----------------------------------------------------------------------------
// Purpose: A COutputEvent consists of an array of these CEventActions. 
//			Each CEventAction holds the information to fire a single input in 
//			a target entity, after a specific delay.
//-----------------------------------------------------------------------------
class CEventAction
{
public:
	CEventAction( const char *ActionData = nullptr );
	CEventAction( const CEventAction &p_EventAction );

	string_t m_iTarget; // name of the entity(s) to cause the action in
	string_t m_iTargetInput; // the name of the action to fire
	string_t m_iParameter; // parameter to send, 0 if none
	float m_flDelay; // the number of seconds to wait before firing the action
	int m_nTimesToFire; // The number of times to fire this event, or EVENT_FIRE_ALWAYS.

	int m_iIDStamp;	// unique identifier stamp

	static int s_iNextIDStamp;

	CEventAction *m_pNext; 

	// allocates memory from engine.MPool/g_EntityListPool
	static void *operator new( std::size_t stAllocateBlock );
	static void *operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );
	static void operator delete( void *pMem );
	static void operator delete( void *pMem , int nBlockUse, const char *pFileName, int nLine ) { operator delete(pMem); }

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: Stores a list of connections to other entities, for data/commands to be
//			communicated along.
//-----------------------------------------------------------------------------
class CBaseEntityOutput
{
public:
	~CBaseEntityOutput();

	void ParseEventAction( const char *EventData );
	void AddEventAction( CEventAction *pEventAction );
	void RemoveEventAction( CEventAction *pEventAction );

	int NumberOfElements( void );

	float GetMaxDelay( void );

	fieldtype_t ValueFieldType() { return m_Value.FieldType(); }

	void FireOutput( variant_t Value, CBaseEntity *pActivator, CBaseEntity *pCaller, float fDelay = 0 );

	/// Delete every single action in the action list. 
	void DeleteAllElements( void ) ;

	CEventAction *GetFirstAction() { return m_ActionList; }

	const CEventAction *GetActionForTarget( string_t iSearchTarget ) const;
protected:
	variant_t m_Value;
	CEventAction *m_ActionList;
	DECLARE_SIMPLE_DATADESC();

	CBaseEntityOutput() {} // this class cannot be created, only it's children

private:
	CBaseEntityOutput( CBaseEntityOutput& ); // protect from accidental copying
};

//-----------------------------------------------------------------------------
// Purpose: wraps variant_t data handling in convenient, compiler type-checked template
//-----------------------------------------------------------------------------
template< class Type, fieldtype_t fieldType >
class CEntityOutputTemplate : public CBaseEntityOutput
{
public:
	//
	// Sets an initial value without firing the output.
	//
	void Init( Type value ) 
	{
		m_Value.Set( fieldType, &value );
	}

	//
	// Sets a value and fires the output.
	//
	void Set( Type value, CBaseEntity *pActivator, CBaseEntity *pCaller ) 
	{
		m_Value.Set( fieldType, &value );
		FireOutput( m_Value, pActivator, pCaller );
	}

	//
	// Returns the current value.
	//
	Type Get( void )
	{
		return *((Type*)&m_Value);
	}
};

//
// Template specializations for type Vector, so we can implement Get, Set, and Init differently.
//
template<>
class CEntityOutputTemplate< Vector, FIELD_VECTOR> : public CBaseEntityOutput
{
public:
	void Init( const Vector &value )
	{
		m_Value.SetVector3D( value );
	}

	void Set( const Vector &value, CBaseEntity *pActivator, CBaseEntity *pCaller )
	{
		m_Value.SetVector3D( value );
		FireOutput( m_Value, pActivator, pCaller );
	}

	void Get( Vector &vec )
	{
		m_Value.Vector3D(vec);
	}
};

template<>
class CEntityOutputTemplate< Vector, FIELD_POSITION_VECTOR> : public CBaseEntityOutput
{
public:
	void Init( const Vector &value )
	{
		m_Value.SetPositionVector3D( value );
	}

	void Set( const Vector &value, CBaseEntity *pActivator, CBaseEntity *pCaller )
	{
		m_Value.SetPositionVector3D( value );
		FireOutput( m_Value, pActivator, pCaller );
	}

	void Get( Vector &vec )
	{
		m_Value.Vector3D(vec);
	}
};

//-----------------------------------------------------------------------------
// Purpose: parameterless entity event
//-----------------------------------------------------------------------------
class COutputEvent : public CBaseEntityOutput
{
public:
	// void Firing, no parameter
	void FireOutput( CBaseEntity *pActivator, CBaseEntity *pCaller, float fDelay = 0 );
};

// useful typedefs for allowed output data types
typedef CEntityOutputTemplate<variant_t,FIELD_INPUT>		COutputVariant;
typedef CEntityOutputTemplate<int,FIELD_INTEGER>			COutputInt;
typedef CEntityOutputTemplate<float,FIELD_FLOAT>			COutputFloat;
typedef CEntityOutputTemplate<string_t,FIELD_STRING>		COutputString;
typedef CEntityOutputTemplate<EHANDLE,FIELD_EHANDLE>		COutputEHANDLE;
typedef CEntityOutputTemplate<Vector,FIELD_VECTOR>			COutputVector;
typedef CEntityOutputTemplate<Vector,FIELD_POSITION_VECTOR>	COutputPositionVector;
typedef CEntityOutputTemplate<color32,FIELD_COLOR32>		COutputColor32;

class CBaseEntity;



void PhysicsSplash( IPhysicsFluidController *pFluid, IPhysicsObject *pObject, CBaseEntity *pEntity );

class CBaseEntity;
struct gamevcollisionevent_t;



struct impactentry_t
{
	float	impulse;
	float	damage;
};

// UNDONE: Add a flag to turn off aggregation of mass in object systems (e.g. ragdolls, vehicles)?
struct impactdamagetable_t
{
	impactentry_t	*linearTable;
	impactentry_t	*angularTable;
	int			linearCount;	// array size of linearTable
	int			angularCount;	// array size of angularTable

	float		minSpeedSqr;	// minimum squared impact speed for damage
	float		minRotSpeedSqr;
	float		minMass;		// minimum mass to do damage

	// filter out reall small objects, set all to zero to disable
	float		smallMassMax;
	float		smallMassCap;
	float		smallMassMinSpeedSqr;

	// exaggerate the effects of really large objects, set all to 1 to disable
	float		largeMassMin;
	float		largeMassScale;
	float		largeMassFallingScale;	// emphasize downward impacts so that this will kill instead of stress (we have more information here than there)
	float		myMinVelocity;			// filter out any energy lost by me unless my velocity is greater than this
};

extern impactdamagetable_t gDefaultNPCImpactDamageTable;
extern impactdamagetable_t gDefaultPlayerImpactDamageTable;
extern impactdamagetable_t gDefaultPlayerVehicleImpactDamageTable;

// NOTE Default uses default NPC table
float CalculateDefaultPhysicsDamage( int index, gamevcollisionevent_t *pEvent, float energyScale, bool allowStaticDamage, int &damageTypeOut, string_t iszDamageTableName = NULL_STRING, bool bDamageFromHeldObjects = false );

// use passes in the table
float CalculatePhysicsImpactDamage( int index, gamevcollisionevent_t *pEvent, const impactdamagetable_t &table, float energyScale, bool allowStaticDamage, int &damageTypeOut, bool bDamageFromHeldObjects = false );

struct vphysics_objectstress_t
{
	float		exertedStress;
	float		receivedStress;
	bool		hasNonStaticStress;
	bool		hasLargeObjectContact;
};

float CalculateObjectStress( IPhysicsObject *pObject, CBaseEntity *pOwnerEntity, vphysics_objectstress_t *pOutput );

class CBaseEntity;
class CBaseFlex;

typedef CHandle<CBaseEntity> EHANDLE;


class CSceneEntity;

//-----------------------------------------------------------------------------
// Purpose: One of a number of currently playing scene events for this actor
//-----------------------------------------------------------------------------
// FIXME: move this, it's only used in in baseflex and baseactor
class CSceneEventInfo
{
public:
	CSceneEventInfo();
	~CSceneEventInfo(); // out-of-line default destructor to deal with template on forward-declared class

	// The event handle of the current scene event
	CChoreoEvent	*m_pEvent;

	// Current Scene
	CChoreoScene	*m_pScene;

	// Current actor
	CChoreoActor	*m_pActor;

	CHandle< CSceneEntity >	m_hSceneEntity;

	// Set after the first time the event has been configured ( allows
	//  bumping markov index only at start of event playback, not every frame )
	bool			m_bStarted;

public:
	//	EVENT local data...
	// FIXME: Evil, make accessors or figure out better place
	// FIXME: This won't work, scenes don't save and restore...
	int						m_iLayer;
	int						m_iPriority;
	int						m_nSequence;
	bool					m_bIsGesture;
	float					m_flWeight; // used for suppressions of posture while moving

	// movement, faceto targets?
	EHANDLE					m_hTarget;
	bool					m_bIsMoving;
	bool					m_bHasArrived;
	float					m_flInitialYaw;
	float					m_flTargetYaw;
	float					m_flFacingYaw;

	// generic AI events
	int						m_nType;
	float					m_flNext;

	// is this event only client side?
	bool					m_bClientSide; 

	// cached flex file
	const flexsettinghdr_t *m_pExpHdr; 

	void					InitWeight( CBaseFlex *pActor );
	float					UpdateWeight( CBaseFlex *pActor );
};

//-----------------------------------------------------------------------------
// Purpose: Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	const char	*CurrentToken( void );
	bool		GetToken( bool crossline );
	bool		TokenAvailable( void );
	void		Error( const char *fmt, ... );
	void		SetBuffer( char *buffer );
private:
	const char	*m_pBuffer;
	char		m_szToken[ 1024 ];
};

extern CSceneTokenProcessor g_TokenProcessor;

void Scene_Printf( const char *pFormat, ... );
extern ConVar scene_clientflex;

inline constexpr auto SCRIPT_EVENT_DEAD = 1000; // character is now dead
inline constexpr auto SCRIPT_EVENT_NOINTERRUPT = 1001; // does not allow interrupt
inline constexpr auto SCRIPT_EVENT_CANINTERRUPT = 1002; // will allow interrupt
inline constexpr auto SCRIPT_EVENT_FIREEVENT = 1003; // Fires OnScriptEventXX output in the script entity, where XX is the event number from the options data.
inline constexpr auto SCRIPT_EVENT_SOUND = 1004; // Play named wave file (on CHAN_BODY)
inline constexpr auto SCRIPT_EVENT_SENTENCE = 1005; // Play named sentence
inline constexpr auto SCRIPT_EVENT_INAIR = 1006; // Leave the character in air at the end of the sequence (don't find the floor)
inline constexpr auto SCRIPT_EVENT_ENDANIMATION = 1007; // Set the animation by name after the sequence completes
inline constexpr auto SCRIPT_EVENT_SOUND_VOICE = 1008; // Play named wave file (on CHAN_VOICE)
inline constexpr auto SCRIPT_EVENT_SENTENCE_RND1 = 1009; // Play sentence group 25% of the time
inline constexpr auto SCRIPT_EVENT_NOT_DEAD = 1010; // Bring back to life (for life/death sequences)
inline constexpr auto SCRIPT_EVENT_EMPHASIS = 1011; // Emphasis point for gestures

inline constexpr auto SCRIPT_EVENT_BODYGROUPON = 1020; // Turn a bodygroup on
inline constexpr auto SCRIPT_EVENT_BODYGROUPOFF = 1021; // Turn a bodygroup off
inline constexpr auto SCRIPT_EVENT_BODYGROUPTEMP = 1022; // Turn a bodygroup on until this sequence ends

inline constexpr auto SCRIPT_EVENT_FIRE_INPUT = 1100; // Fires named input on the event handler

inline constexpr auto SENDPROP_SIMULATION_TIME_PRIORITY = 0;
inline constexpr auto SENDPROP_TICKBASE_PRIORITY = 1;

inline constexpr auto SENDPROP_LOCALPLAYER_ORIGINXY_PRIORITY = 2;
inline constexpr auto SENDPROP_PLAYER_EYE_ANGLES_PRIORITY = 3;
inline constexpr auto SENDPROP_LOCALPLAYER_ORIGINZ_PRIORITY = 4;

inline constexpr auto SENDPROP_PLAYER_VELOCITY_XY_PRIORITY = 5;
inline constexpr auto SENDPROP_PLAYER_VELOCITY_Z_PRIORITY = 6;

// Nonlocal players exclude local player origin X and Y, not vice-versa,
// so our props should come after the most frequent local player props
// so we don't eat up their prop index bits.
inline constexpr auto SENDPROP_NONLOCALPLAYER_ORIGINXY_PRIORITY = 7;
inline constexpr auto SENDPROP_NONLOCALPLAYER_ORIGINZ_PRIORITY = 8;

inline constexpr auto SENDPROP_CELL_INFO_PRIORITY = 32; // SendProp priority for cell bits and x/y/z.

// Set these to a low priority so that they don't push other
// props up -- this saves us from scanning across these 330
// props.
inline constexpr auto SENDPROP_MATCHSTATS_PRIORITY = 140;

// Purpose: implements various common send proxies

void SendProxy_Color32ToInt( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
void SendProxy_EHandleToInt( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_IntAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_ShortAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
// Defined in player.cpp and used by several tables, which previously forward-declared it in
// each .cpp. A Proxy<> annotation needs it visible where the class is declared, so it belongs
// here with the other shared proxies.
void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );

SendProp SendPropBool(
	const char *pVarName,
	int offset,
	int sizeofVar );

SendProp SendPropEHandle(
	const char *pVarName,
	int offset,
	int flags = 0,
	int sizeofVar=kSizeofIgnore,
	SendVarProxyFn proxyFn=SendProxy_EHandleToInt );

SendProp SendPropTime(
	const char *pVarName,
	int offset,
	int sizeofVar=kSizeofIgnore );

SendProp SendPropIntWithMinusOneFlag(
	const char *pVarName,
	int offset,
	int bits,
	int sizeofVar=kSizeofIgnore,
	SendVarProxyFn proxyFn=SendProxy_IntAddOne );

// Send a string_t as a string property.
SendProp SendPropStringT( const char *pVarName, int offset, int sizeofVar );

//-----------------------------------------------------------------------------
// Purpose: Proxy that only sends data to team members
//-----------------------------------------------------------------------------
void* SendProxy_OnlyToTeam( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );

class CBaseEntity;
struct inputdata_t;


// Entity input handler. typedescription_t keeps it in an inputfunc_storage_t, which datamap.h
// leaves untyped so that it names no game class.
typedef void (CBaseEntity::*inputfunc_t)(inputdata_t &data);

inline inputfunc_storage_t ToInputStorage( inputfunc_t pFunction )
{
	static_assert( sizeof( inputfunc_t ) == sizeof( inputfunc_storage_t ) );
	inputfunc_storage_t storage = {};
	std::memcpy( &storage, &pFunction, sizeof( pFunction ) );
	return storage;
}

inline inputfunc_t FromInputStorage( const inputfunc_storage_t &storage )
{
	inputfunc_t pFunction;
	std::memcpy( &pFunction, &storage, sizeof( pFunction ) );
	return pFunction;
}
struct FireBulletsInfo_t
{
	FireBulletsInfo_t()
	{
		m_iShots = 1;
		m_vecSpread.Init( 0, 0, 0 );
		m_flDistance = 8192;
		m_iTracerFreq = 4;
		m_flDamage = 0.0f;
		m_flPlayerDamage = 0.0f;
		m_pAttacker = nullptr;
		m_nFlags = 0;
		m_pAdditionalIgnoreEnt = nullptr;
		m_flDamageForceScale = 1.0f;

		m_bPrimaryAttack = true;
	}

	FireBulletsInfo_t( int nShots, const Vector &vecSrc, const Vector &vecDir, const Vector &vecSpread, float flDistance, int nAmmoType, bool bPrimaryAttack = true )
	{
		m_iShots = nShots;
		m_vecSrc = vecSrc;
		m_vecDirShooting = vecDir;
		m_vecSpread = vecSpread;
		m_flDistance = flDistance;
		m_iAmmoType = nAmmoType;
		m_iTracerFreq = 4;
		m_flDamage = 0;
		m_flPlayerDamage = 0;
		m_pAttacker = nullptr;
		m_nFlags = 0;
		m_pAdditionalIgnoreEnt = nullptr;
		m_flDamageForceScale = 1.0f;
		m_bPrimaryAttack = bPrimaryAttack;
	}

	int m_iShots;
	Vector m_vecSrc;
	Vector m_vecDirShooting;
	Vector m_vecSpread;
	float m_flDistance;
	int m_iAmmoType;
	int m_iTracerFreq;
	float m_flDamage;
	float m_flPlayerDamage;	// Damage to be used instead of m_flDamage if we hit a player
	int m_nFlags;			// See FireBulletsFlags_t
	float m_flDamageForceScale;
	CBaseEntity *m_pAttacker;
	CBaseEntity *m_pAdditionalIgnoreEnt;
	bool m_bPrimaryAttack;
};

struct EmitSound_t
{
	EmitSound_t() :
		m_nChannel( 0 ),
		m_pSoundName( 0 ),
		m_flVolume( VOL_NORM ),
		m_SoundLevel( SNDLVL_NONE ),
		m_nFlags( 0 ),
		m_nPitch( PITCH_NORM ),
		m_pOrigin( 0 ),
		m_flSoundTime( 0.0f ),
		m_pflSoundDuration( 0 ),
		m_bEmitCloseCaption( true ),
		m_bWarnOnMissingCloseCaption( false ),
		m_bWarnOnDirectWaveReference( false ),
		m_nSpeakerEntity( -1 ),
		m_UtlVecSoundOrigin(),
		m_hSoundScriptHash( SOUNDEMITTER_INVALID_HASH ),
		m_nSoundEntryVersion( 1 )
	{
	}

	EmitSound_t( const CSoundParameters &src );

	int							m_nChannel;
	char const					*m_pSoundName;
	float						m_flVolume;
	soundlevel_t				m_SoundLevel;
	int							m_nFlags;
	int							m_nPitch;
	const Vector				*m_pOrigin;
	float						m_flSoundTime; ///< NOT DURATION, but rather, some absolute time in the future until which this sound should be delayed
	float						*m_pflSoundDuration;
	bool						m_bEmitCloseCaption;
	bool						m_bWarnOnMissingCloseCaption;
	bool						m_bWarnOnDirectWaveReference;
	int							m_nSpeakerEntity;
	mutable std::vector< Vector >	m_UtlVecSoundOrigin;  ///< Actual sound origin(s) (can be multiple if sound routed through speaker entity(ies) )
	mutable HSOUNDSCRIPTHASH	m_hSoundScriptHash;
	int							m_nSoundEntryVersion;
};

bool IsHeadTrackingEnabled();

// Purpose: Exposes client-server neutral interfaces implemented in both places

class IMapData;

extern IFileSystem				*filesystem;
extern CUniformRandomStream		*random_valve;
extern CGaussianRandomStream *randomgaussian;
extern IEngineSound				*enginesound;
extern IMapData					*g_pMapData;			// TODO: current implementations of the 
														// interface are in TF2, should probably move
														// to TF2/HL2 neutral territory

class CBaseEntity;


extern IVEngineServer			*engine;
extern IVoiceServer				*g_pVoiceServer;
extern IFileSystem				*filesystem;
extern IStaticPropMgrServer		*staticpropmgr;
extern ISpatialPartition		*partition;
extern IEngineSound				*enginesound;
extern IVModelInfo				*modelinfo;
extern IEngineTrace				*enginetrace;
extern CFileLoggingListener		*filelogginglistener;
extern IGameEventManager2		*gameeventmanager;
extern IVDebugOverlay			*debugoverlay;
extern IScriptManager			*scriptmanager;

//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nMaterialIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for particle systems
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName );
int GetParticleSystemIndex( const char *pParticleSystemName );
const char *GetParticleSystemNameFromIndex( int nIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for movies
//-----------------------------------------------------------------------------
void PrecacheMovie( const char *pMovieName );
int GetMovieIndex( const char *pMovieName );
const char *GetMovieNameFromIndex( int nMovieIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for effects (used by DispatchEffect)
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pParticleSystemName );
int GetEffectIndex( const char *pParticleSystemName );
const char *GetEffectNameFromIndex( int nIndex );

void EntityMessageBegin( CBaseEntity * entity, bool reliable = false );
void MessageEnd( void );

// bytewise
void MessageWriteByte( int iValue);
void MessageWriteChar( int iValue);
void MessageWriteShort( int iValue);
void MessageWriteWord( int iValue );
void MessageWriteLong( int iValue);
void MessageWriteFloat( float flValue);
void MessageWriteAngle( float flValue);
void MessageWriteCoord( float flValue);
void MessageWriteVec3Coord( const Vector& rgflValue);
void MessageWriteVec3Normal( const Vector& rgflValue);
void MessageWriteAngles( const QAngle& rgflValue);
void MessageWriteString( const char *sz );
void MessageWriteEntity( int iValue);
void MessageWriteEHandle( CBaseEntity *pEntity ); //encoded as a long

// bitwise
void MessageWriteBool( bool bValue );
void MessageWriteUBitLong( unsigned int data, int numbits );
void MessageWriteSBitLong( int data, int numbits );
void MessageWriteBits( const void *pIn, int nBits );
void MessageWriteBitVecIntegral( const Vector& vecValue );

// Bytewise
#define WRITE_WORD		(MessageWriteWord)
inline constexpr auto WRITE_LONG = (MessageWriteLong);
inline constexpr auto WRITE_FLOAT = (MessageWriteFloat);
inline constexpr auto WRITE_ANGLE = (MessageWriteAngle);
inline constexpr auto WRITE_COORD = (MessageWriteCoord);
inline constexpr auto WRITE_VEC3COORD = (MessageWriteVec3Coord);
inline constexpr auto WRITE_VEC3NORMAL = (MessageWriteVec3Normal);
inline constexpr auto WRITE_ANGLES = (MessageWriteAngles);
inline constexpr auto WRITE_STRING = (MessageWriteString);
inline constexpr auto WRITE_ENTITY = (MessageWriteEntity);
inline constexpr auto WRITE_EHANDLE = (MessageWriteEHandle);

// Bitwise
inline constexpr auto WRITE_BOOL = (MessageWriteBool);
inline constexpr auto WRITE_UBITLONG = (MessageWriteUBitLong);
inline constexpr auto WRITE_SBITLONG = (MessageWriteSBitLong);
inline constexpr auto WRITE_BITS = (MessageWriteBits);
inline constexpr auto WRITE_VEC3_INTEGRAL = (MessageWriteBitVecIntegral);

//-----------------------------------------------------------------------------
// Send a user message
//-----------------------------------------------------------------------------
// Serializes and hands the engine bytes, so the interface carries no schema.
template < class T >
void SendUserMessage( IRecipientFilter &filter, int message, const T &msg )
{
	std::string data( ks::proto::byte_size( msg ), '\0' );
	ks::proto::write( msg, ( std::byte * ) data.data() );
	engine->SendUserMessage( filter, message, data.data(), int( data.size() ) );
}

#undef WRITE_WORD

struct FindClosestPassableSpace_TraceAdapter_t;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: Shared util code between client and server.

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

extern ConVar developer;	// developer mode

// Clear all recipients of a send proxy and set only the specified client (and its split screen
// players), or set all recipients save for it. Server only. Note: these are CLIENT indices.
void SendProxyRecipients_SetOnly( CSendProxyRecipients *pRecipients, int iClient );
void SendProxyRecipients_ExcludeOnly( CSendProxyRecipients *pRecipients, int iClient );

// Returns true if the trace hit the world entity.
// If this returns true, then you can't use GetHitBoxIndex().
bool TraceDidHitWorld( const CGameTrace &tr );

// Returns true if the trace hit something and it wasn't the world.
bool TraceDidHitNonWorldEntity( const CGameTrace &tr );

// Gets the entity's network index if the trace has hit an entity.
// If not, returns -1.
int TraceGetEntityIndex( const CGameTrace &tr );

//-----------------------------------------------------------------------------
// Language IDs.
//-----------------------------------------------------------------------------
inline constexpr auto LANGUAGE_ENGLISH = 0;
inline constexpr auto LANGUAGE_GERMAN = 1;
inline constexpr auto LANGUAGE_FRENCH = 2;
inline constexpr auto LANGUAGE_BRITISH = 3;

//-----------------------------------------------------------------------------
// Pitch + yaw
//-----------------------------------------------------------------------------
float		UTIL_VecToYaw			(const Vector &vec);
float		UTIL_VecToPitch			(const Vector &vec);
float		UTIL_VecToYaw			(const matrix3x4_t& matrix, const Vector &vec);
float		UTIL_VecToPitch			(const matrix3x4_t& matrix, const Vector &vec);
Vector		UTIL_YawToVector		( float yaw );

//-----------------------------------------------------------------------------
// Shared random number generators for shared/predicted code:
// whenever generating random numbers in shared/predicted code, these functions
// have to be used. Each call should specify a unique "sharedname" string that
// seeds the random number generator. In loops make sure the "additionalSeed"
// is increased with the loop counter, otherwise it will always return the
// same random number
//-----------------------------------------------------------------------------
float	SharedRandomFloat( const char *sharedname, float flMinVal, float flMaxVal, int additionalSeed = 0 );
int		SharedRandomInt( const char *sharedname, int iMinVal, int iMaxVal, int additionalSeed = 0 );
Vector	SharedRandomVector( const char *sharedname, float minVal, float maxVal, int additionalSeed = 0 );
QAngle	SharedRandomAngle( const char *sharedname, float minVal, float maxVal, int additionalSeed = 0 );

//-----------------------------------------------------------------------------
// Standard collision filters...
//-----------------------------------------------------------------------------
bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass );
bool StandardFilterRules( IHandleEntity *pHandleEntity, int fContentsMask );

// "weapon_"
inline constexpr auto WEAPON_CLASSNAME_PREFIX_LENGTH = 7;
bool IsWeaponClassname( const char *pszClassName );

//-----------------------------------------------------------------------------
// Converts an IHandleEntity to an CBaseEntity
//-----------------------------------------------------------------------------
const CBaseEntity *EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity );
CBaseEntity *EntityFromEntityHandle( IHandleEntity *pHandleEntity );

typedef bool (*ShouldHitFunc_t)( IHandleEntity *pHandleEntity, int contentsMask );

//-----------------------------------------------------------------------------
// traceline methods
//-----------------------------------------------------------------------------
class CTraceFilterSimple : public CTraceFilter
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CTraceFilterSimple );
	
	CTraceFilterSimple( const IHandleEntity *passentity, int collisionGroup, ShouldHitFunc_t pExtraShouldHitCheckFn = nullptr );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
	virtual void SetPassEntity( const IHandleEntity *pPassEntity ) { m_pPassEnt = pPassEntity; }
	virtual void SetCollisionGroup( int iCollisionGroup ) { m_collisionGroup = iCollisionGroup; }

	const IHandleEntity *GetPassEntity( void ){ return m_pPassEnt;}
	int GetCollisionGroup( void ) const { return m_collisionGroup; }

private:
	const IHandleEntity *m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;

};

class CTraceFilterSkipTwoEntities : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterSkipTwoEntities, CTraceFilterSimple );
	
	CTraceFilterSkipTwoEntities( const IHandleEntity *passentity = nullptr, const IHandleEntity *passentity2 = nullptr, int collisionGroup = COLLISION_GROUP_NONE );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
	virtual void SetPassEntity2( const IHandleEntity *pPassEntity2 ) { m_pPassEnt2 = pPassEntity2; }

private:
	const IHandleEntity *m_pPassEnt2;
};

class CTraceFilterSimpleList : public CTraceFilterSimple
{
public:
	CTraceFilterSimpleList( int collisionGroup );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

	void	AddEntityToIgnore( IHandleEntity *pEntity );
	void	AddEntitiesToIgnore( int nCount, IHandleEntity **ppEntities );

protected:
	std::vector< IHandleEntity* >	m_PassEntities;
};

class CTraceFilterNoNPCsOrPlayer : public CTraceFilterSimple
{
public:
	CTraceFilterNoNPCsOrPlayer( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

class CTraceFilterNoPlayers : public CTraceFilterSimple
{
public:
	CTraceFilterNoPlayers( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for NPC LOS traces
//-----------------------------------------------------------------------------
class CTraceFilterLOS : public CTraceFilterSkipTwoEntities
{
public:
	CTraceFilterLOS( IHandleEntity *pHandleEntity, int collisionGroup, IHandleEntity *pHandleEntity2 = nullptr );
	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

// helper
void DebugDrawLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, int r, int g, int b, bool test, float duration );

extern ConVar r_visualizetraces;

[[gnu::always_inline]] inline constexpr auto DidTraceSpike() { return false; }

inline void UTIL_TraceLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 const IHandleEntity *ignore, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSimple traceFilter( ignore, collisionGroup );

	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd );
			CTraceFilterSimple traceFilter( ignore, collisionGroup );

			enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
			if ( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 ITraceFilter *pFilter, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );

	enginetrace->TraceRay( ray, mask, pFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd );

			enginetrace->TraceRay( ray, mask, pFilter, ptr );

			if ( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceHull( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin, 
					 const Vector &hullMax,	unsigned int mask, const IHandleEntity *ignore, 
					 int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );
	CTraceFilterSimple traceFilter( ignore, collisionGroup );

	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 255, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );
			CTraceFilterSimple traceFilter( ignore, collisionGroup );

			enginetrace->TraceRay( ray, mask, &traceFilter, ptr );

			if ( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceHull( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin, 
					 const Vector &hullMax,	unsigned int mask, ITraceFilter *pFilter, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );

	enginetrace->TraceRay( ray, mask, pFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 255, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );

			enginetrace->TraceRay( ray, mask, pFilter, ptr );

			if( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceRay( const Ray_t &ray, unsigned int mask, 
						  const IHandleEntity *ignore, int collisionGroup, trace_t *ptr )
{
	CTraceFilterSimple traceFilter( ignore, collisionGroup );

	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	
	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

inline void UTIL_TraceRay( const Ray_t &ray, unsigned int mask, 
						  ITraceFilter *pFilter, trace_t *ptr )
{
	enginetrace->TraceRay( ray, mask, pFilter, ptr );

	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}
// Sweeps a particular entity through the world
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr );
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, ITraceFilter *pFilter, trace_t *ptr );
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, const IHandleEntity *ignore, int collisionGroup, trace_t *ptr );

bool UTIL_EntityHasMatchingRootParent( CBaseEntity *pRootParent, CBaseEntity *pEntity );

inline int UTIL_PointContents( const Vector &vec, int contentsMask )
{
	return enginetrace->GetPointContents( vec, contentsMask );
}

// Sweeps against a particular model, using collision rules 
void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
					  const Vector &hullMax, CBaseEntity *pentModel, int collisionGroup, trace_t *ptr );

void UTIL_ClipTraceToPlayers( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter *filter, trace_t *tr );

// Particle effect tracer
void		UTIL_ParticleTracer( const char *pszTracerEffectName, const Vector &vecStart, const Vector &vecEnd, int iEntIndex = 0, int iAttachment = 0, bool bWhiz = false );

// Old style, non-particle system, tracers
void		UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex = 0, int iAttachment = TRACER_DONT_USE_ATTACHMENT, float flVelocity = 0, bool bWhiz = false, const char *pCustomTracerName = nullptr, int iParticleID = 0 );

bool		UTIL_IsLowViolence( void );
bool		UTIL_ShouldShowBlood( int bloodColor );
void		UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount );

void		UTIL_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount );
void		UTIL_BloodDecalTrace( trace_t *pTrace, int bloodColor );
void		UTIL_DecalTrace( trace_t *pTrace, char const *decalName );
bool		UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax );
bool		UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax, unsigned int mask, ITraceFilter *pFilter );

// Search for water transition along a vertical line
float UTIL_WaterLevel( const Vector &position, float minz, float maxz );

// Like UTIL_WaterLevel, but *way* less expensive.
// I didn't replace UTIL_WaterLevel everywhere to avoid breaking anything.
float UTIL_FindWaterSurface( const Vector &position, float minz, float maxz );

void		UTIL_StringToVector( float *pVector, const char *pString );
void		UTIL_StringToFloatArray( float *pVector, int count, const char *pString );

CBasePlayer *UTIL_PlayerByIndex( int entindex );

// decodes/encodes a buffer using a 64bit ICE key (inplace)
void		UTIL_DecodeICE( unsigned char * buffer, int size, const unsigned char *key );
void		UTIL_EncodeICE( unsigned char * buffer, unsigned int size, const unsigned char *key );
unsigned short UTIL_GetAchievementEventMask( void );	

//assumes the object is already in a mostly passable space
inline constexpr auto FL_AXIS_DIRECTION_NONE = ( 0 );
inline constexpr auto FL_AXIS_DIRECTION_X = ( 1 << 0 );
inline constexpr auto FL_AXIS_DIRECTION_NX = ( 1 << 1 );
inline constexpr auto FL_AXIS_DIRECTION_Y = ( 1 << 2 );
inline constexpr auto FL_AXIS_DIRECTION_NY = ( 1 << 3 );
inline constexpr auto FL_AXIS_DIRECTION_Z = ( 1 << 4 );
inline constexpr auto FL_AXIS_DIRECTION_NZ = ( 1 << 5 );

typedef void (*FN_RayTraceAdapterFunc)( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter );
typedef bool (*FN_PointIsOutsideWorld)( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter );

//derive from this to tack on additional data to your adapted functions
struct FindClosestPassableSpace_TraceAdapter_t
{
	FN_RayTraceAdapterFunc pTraceFunc;
	FN_PointIsOutsideWorld pPointOutsideWorldFunc;

	ITraceFilter *pTraceFilter;
	unsigned int fMask;
};

bool		UTIL_FindClosestPassableSpace( const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter );
bool		UTIL_FindClosestPassableSpace( const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags = FL_AXIS_DIRECTION_NONE );
bool		UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, unsigned int iIterations, Vector &vOriginOut, Vector *pStartingPosition = nullptr, int nAxisRestrictionFlags = FL_AXIS_DIRECTION_NONE );
bool		UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, Vector *pStartingPosition = nullptr, int nAxisRestrictionFlags = FL_AXIS_DIRECTION_NONE );

//--------------------------------------------------------------------------------------------------------------
/**
 * Given a position and a ray, return the shortest distance between the two.
 * If 'pos' is beyond either end of the ray, the returned distance is negated.
 */
inline float DistanceToRay( const Vector &pos, const Vector &rayStart, const Vector &rayEnd, float *along = nullptr, Vector *pointOnRay = nullptr )
{
	Vector to = pos - rayStart;
	Vector dir = rayEnd - rayStart;
	float length = dir.NormalizeInPlace();

	float rangeAlong = DotProduct( dir, to );
	if (along)
	{
		*along = rangeAlong;
	}

	float range;

	if (rangeAlong < 0.0f)
	{
		// off start point
		range = -(pos - rayStart).Length();

		if (pointOnRay)
		{
			*pointOnRay = rayStart;
		}
	}
	else if (rangeAlong > length)
	{
		// off end point
		range = -(pos - rayEnd).Length();

		if (pointOnRay)
		{
			*pointOnRay = rayEnd;
		}
	}
	else // within ray bounds
	{
		Vector onRay = rayStart + rangeAlong * dir;
		range = (pos - onRay).Length();

		if (pointOnRay)
		{
			*pointOnRay = onRay;
		}
	}

	return range;
}

//--------------------------------------------------------------------------------------------------------------
/**
* Macro for creating an interface that when inherited from automatically maintains a list of instances
* that inherit from that interface.
*/

// interface for entities that want to a auto maintained global list
#define DECLARE_AUTO_LIST( interfaceName ) \
	class interfaceName; \
	class interfaceName \
	{ \
	public: \
		interfaceName( bool bAutoAdd = true ); \
		virtual ~interfaceName(); \
		virtual CBaseEntity* GetEntity( void ) = 0; \
		static void Add( interfaceName *pElement ) { m_##interfaceName##AutoList.AddToTail( pElement ); } \
		static void Remove( interfaceName *pElement ) { m_##interfaceName##AutoList.FindAndFastRemove( pElement ); } \
		static const std::vector< interfaceName* >& AutoList( void ) { return m_##interfaceName##AutoList; } \
	private: \
	static std::vector< interfaceName* > m_##interfaceName##AutoList; \
	};

// Creates a simple function for accessing the higher level entity
#define IMPLEMENT_AUTO_LIST_GET() \
	virtual CBaseEntity* GetEntity( void ) { return this; }

// Creates the auto add/remove constructor/destructor...
// Pass false to the constructor to not auto add
#define IMPLEMENT_AUTO_LIST( interfaceName ) \
	std::vector< class interfaceName* > interfaceName::m_##interfaceName##AutoList; \
	interfaceName::interfaceName( bool bAutoAdd ) \
	{ \
		if ( bAutoAdd ) \
		{ \
			Add( this ); \
		} \
	} \
	interfaceName::~interfaceName() \
	{ \
		Remove( this ); \
	}

//--------------------------------------------------------------------------------------------------------------
/**
 * Simple class for tracking intervals of game time.
 * Upon creation, the timer is invalidated.  To measure time intervals, start the timer via Start().
 */
class IntervalTimer
{
	[[= ks::reflect::NetTable{ .name = "DT_IntervalTimer", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( IntervalTimer );
	DECLARE_EMBEDDED_NETWORKVAR();

	IntervalTimer( void ) : m_timestamp( -1.0f )
	{
	}

	void Reset( void )
	{
		m_timestamp = Now();
	}		

	void Start( void )
	{
		m_timestamp = Now();
	}

	void StartFromTime( float startTime )
	{
		m_timestamp = startTime;
	}

	void Invalidate( void )
	{
		m_timestamp = -1.0f;
	}		

	bool HasStarted( void ) const
	{
		return (m_timestamp > 0.0f);
	}

	/// if not started, elapsed time is very large
	float GetElapsedTime( void ) const
	{
		return (HasStarted()) ? (Now() - m_timestamp) : 99999.9f;
	}

	bool IsLessThen( float duration ) const
	{
		return (Now() - m_timestamp < duration) ? true : false;
	}

	bool IsGreaterThen( float duration ) const
	{
		return (Now() - m_timestamp > duration) ? true : false;
	}

	float GetStartTime( void ) const
	{
		return m_timestamp;
	}

protected:
	CNetworkVar( float, m_timestamp, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	float Now( void ) const;		// work-around since client header doesn't like inlined gpGlobals->curtime
};

EXTERN_SEND_TABLE(DT_IntervalTimer);

//--------------------------------------------------------------------------------------------------------------
/**
 * Simple class for counting down a short interval of time.
 * Upon creation, the timer is invalidated.  Invalidated countdown timers are considered to have elapsed.
 */
class CountdownTimer
{
	[[= ks::reflect::NetTable{ .name = "DT_CountdownTimer", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CountdownTimer );
	DECLARE_EMBEDDED_NETWORKVAR();

	CountdownTimer( void ) : 
		m_timestamp( -1.0f), m_duration( 0.0f )
	{
	}

	void Reset( void )
	{
		m_timestamp = Now() + m_duration;
	}		

	void Start( float duration );

	void StartFromTime( float startTime, float duration );

	void Invalidate( void )
	{
		m_timestamp = -1.0f;
	}		

	bool HasStarted( void ) const
	{
		return (m_timestamp > 0.0f);
	}

	bool IsElapsed( void ) const
	{
		return (Now() > m_timestamp);
	}

	float GetElapsedTime( void ) const
	{
		return Now() - m_timestamp + m_duration;
	}

	float GetRemainingTime( void ) const
	{
		return (m_timestamp - Now());
	}

	float GetTargetTime() const
	{
		return m_timestamp;
	}

	/// return original countdown time
	float GetCountdownDuration( void ) const
	{
		return (m_timestamp > 0.0f) ? m_duration : 0.0f;
	}
	
	/// 1.0 for newly started, 0.0 for elapsed
	float GetRemainingRatio( void ) const
	{
		if ( HasStarted() )
		{
			float left = GetRemainingTime() / m_duration;
			if ( left < 0.0f )
				return 0.0f;
			if ( left > 1.0f )
				return 1.0f;
			return left;
		}
		
		return 0.0f;
	}

	float GetElapsedRatio() const;

	// Usage:
	//    Declaration: CountdownTimer mTimer;
	//    Think function:
	//        while(mTimer.RunEvery( timerInterval ))
	//        {
	//			  do fixed-rate stuff
	//        }
	//
	//        nextThinkTime = min(nextThinkTime, mTimer.GetTargetTime());
	//
	// This avoids 'losing' ticks on a repeating timer when
	// the think rate is not a multiple of the timer duration,
	// especially since SetNextThink rounds ticks up/down, causing
	// even a timer that is running exactly at the think rate of
	// the underlying class to not elapse correctly.
	// 
	// It also makes sure that ticks are never lost
	bool RunEvery( float amount = -1.0f )
	{
		// First call starts the timer
		if(!HasStarted())
		{
			if(amount > 0.0f)
				Start( amount );

			return false;
		}

		if( IsElapsed() )
		{
			if ( amount > 0.0f )
				m_duration = amount;

			m_timestamp += m_duration;
			return true;
		}

		return false;
	}

	// Same as RunEvery() but only returns true once per 'tick', then guarantees being non-elapsed.
	// Useful when "do fixed rate stuff" is idempotent, like updating something to match
	// the current time.
	bool Interval( float amount = -1.0f )
	{
		// First call starts the timer
		if ( !HasStarted() )
		{
			if ( amount > 0.0f )
				Start( amount );

			return false;
		}

		if ( IsElapsed() )
		{
			if ( amount > 0.0f )
				m_duration = amount;

			m_timestamp += m_duration;

			// If we are still expired, add a multiple of the interval 
			// until we become non-elapsed
			float remaining = GetRemainingTime();
			if ( remaining < 0.0f)
			{
				float numIntervalsRequired = -std::floorf( remaining / m_duration );
				m_timestamp += m_duration * numIntervalsRequired;
			}

			// We should no longer be elapsed
			contract_assert( !IsElapsed() );

			return true;
		}

		return false;
	}

private:
	CNetworkVar( float, m_duration, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_timestamp, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	float Now( void ) const;		// work-around since client header doesn't like inlined gpGlobals->curtime
};

EXTERN_SEND_TABLE(DT_CountdownTimer);

//--------------------------------------------------------------------------------------------------------------
/**
* Simple class for tracking change in values over time.
*/
inline constexpr auto TIMELINE_ARRAY_SIZE = 64;
inline constexpr auto TIMELINE_INTERVAL_START = 0.25f;

enum TimelineCompression_t
{
	TIMELINE_COMPRESSION_SUM,
	TIMELINE_COMPRESSION_COUNT_PER_INTERVAL,
	TIMELINE_COMPRESSION_AVERAGE,
	TIMELINE_COMPRESSION_AVERAGE_BLEND,

	TIMELINE_COMPRESSION_TOTAL
};

class CTimeline : public IntervalTimer
{
	[[= ks::reflect::NetTable{ .name = "DT_Timeline", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTimeline, IntervalTimer );
	DECLARE_EMBEDDED_NETWORKVAR();

	CTimeline( void )
	{
		ClearValues();
	}

	void ClearValues( void );
	void ClearAndStart( void ) { ClearValues(); Start(); }
	void StopRecording( void ) { m_bStopped = true; }
	void RecordValue( float flValue );
	void RecordFinalValue( float flValue ) { RecordValue( flValue ); StopRecording(); }

	int Count( void ) const
	{
		return m_nBucketCount;
	}

	float GetValue( int i ) const;

	float GetValueAtInterp( float fInterp ) const;

	float GetValueTime( int i ) const;

	float GetInterval( void ) const
	{
		return m_flInterval;
	}

	void SetCompressionType( TimelineCompression_t nCompressionType )
	{
		m_nCompressionType = nCompressionType;
	}

	TimelineCompression_t GetCompressionType( void ) const
	{
		return m_nCompressionType;
	}

private:
	int GetCurrentBucket( void )
	{
		return static_cast<float>( Now() - m_timestamp ) / m_flInterval;
	}

	void Compress( void );

	CNetworkArray( float, m_flValues, TIMELINE_ARRAY_SIZE, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkArray( int, m_nValueCounts, TIMELINE_ARRAY_SIZE, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Float } ]] );
	CNetworkVar( int, m_nBucketCount, [[= ks::reflect::Net{ .bits = NumBitsForCount( TIMELINE_ARRAY_SIZE ), .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flInterval, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFinalValue, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( TimelineCompression_t, m_nCompressionType, [[= ks::reflect::Net{ .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bStopped, [[= ks::reflect::Net{} ]] );
};

EXTERN_SEND_TABLE(DT_Timeline);

const char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename = nullptr );

int UTIL_StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings );

int UTIL_CountNumBitsSet( unsigned int nVar );
int UTIL_CountNumBitsSet( uint64 nVar );

bool UTIL_GetModDir( char *lpszTextOut, unsigned int nSize );

/*UTIL_CalcFrustumThroughPolygon - Given a frustum and a polygon, calculate how the current frustum would clip the polygon, then generate a new frustum that runs along the edge of the clipped polygon.
-returns number of planes in the output frustum, 0 if the polygon was completely clipped by the input frustum
-vFrustumOrigin can be thought of as the camera origin if your frustum is a view frustum
-planes should face inward
-iPreserveCount will preserve N planes at the end of your input frustum and ensure they're at the end of your output frustum. Assuming your input frustum is of type "Frustum", a value of 2 would preserve your near and far planes
-to ensure that your output frustum can hold the entire complex frustum we generate. Make it of size (iPolyVertCount + iCurrentFrustumPlanes + iPreserveCount). Otherwise the output frustum will be simplified to fit your maximum output by eliminating bounding planes with the clipped area.
-a lack of input frustum is considered valid input*/
int UTIL_CalcFrustumThroughConvexPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &vFrustumOrigin, const VPlane *pInputFrustumPlanes, int iInputFrustumPlanes, VPlane *pOutputFrustumPlanes, int iMaxOutputPlanes, int iPreserveCount );

//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------
// enumerate entities that match a set of edict flags into a static array
class CFlaggedEntitiesEnum : public IPartitionEnumerator
{
public:
	CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask );

	// This gets called	by the enumeration methods with each element
	// that passes the test.
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	int GetCount() { return m_count; }
	bool AddToList( CBaseEntity *pEntity );

private:
	CBaseEntity		**m_pList;
	int				m_listMax;
	int				m_flagMask;
	int				m_count;
};

int			UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum  );

inline int UTIL_EntitiesAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );
	return UTIL_EntitiesAlongRay( ray, &rayEnum );
}

#define EASY_DIFFPRINT_CONDITIONAL( conditions, entity, szFormatString, ... )
#define EASY_DIFFPRINT( entity, szFormatString, ... )

void		UTIL_GetNextCommandLength( const char *pText, int nMaxLen, int *pCommandLength, int *pNextCommandOffset );

//--------------------------------------------------------------------------------------------------------------
/**
 *  remove double spaces and empty bold/italic HTML tags from a string
 */
void UTIL_TrimEmptyWhitespaceFromHTML( wchar_t* pszDescWriter, std::size_t descWriterByteSize, const wchar_t* pszDescReader );
void UTIL_TrimEmptyWhitespaceFromHTML( char* pszDescWriter, std::size_t descWriterByteSize, const char* pszDescReader );

#undef DECLARE_AUTO_LIST
#undef IMPLEMENT_AUTO_LIST_GET
#undef IMPLEMENT_AUTO_LIST
#undef EASY_DIFFPRINT_CONDITIONAL
#undef EASY_DIFFPRINT

class IEntityFactory;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Misc utility code.

// A map transition target, used by trigger_changelevel and BuildChangeList.
// It used to live in the save/restore header; the save system is gone but the
// changelevel machinery still needs the type.
struct levellist_t
{
	char	mapName[ MAX_MAP_NAME ];
	char	landmarkName[ 32 ];
	edict_t	*pentLandmark;
	Vector	vecLandmarkOrigin;
};


// entity creation
// creates an entity that has not been linked to a classname
template< class T >
T *_CreateEntityTemplate( T *newEnt, const char *className )
{
	// T has to be a class of the closed set of entity kinds (entity_kind_list.h); the thunks of its kind are
	// defined where the class is registered (LINK_ENTITY_TO_CLASS / KS_ENTITY_UNSAVED_CLASS).
	static_assert( entity_kinds::is_instantiable<T>(), "entity class is not an instantiable entity kind (entity_kind_list.h)" );
	g_PendingEntityKind = kind_of<T>;
	newEnt = new T; // this is the only place 'new' should be used!
	if ( newEnt->GetKind() != kind_of<T> )
		EntityKindFailure( "constructed without the kind of its class", className, (int)kind_of<T> );
	newEnt->PostConstructor( className );
	return newEnt;
}

enum EUtilSayTextMessageType_t
{
	kEUtilSayTextMessageType_Default,		// default text
	kEUtilSayTextMessageType_TeamonlyChat,	// teamonly chat
	kEUtilSayTextMessageType_AllChat,		// all chat
};

// This is the glue that hooks .MAP entity class names to our CPP classes
class IEntityFactoryDictionary
{
public:
	virtual void InstallFactory( IEntityFactory *pFactory, const char *pClassName ) = 0;
	virtual IServerNetworkable *Create( const char *pClassName ) = 0;
	virtual void Destroy( const char *pClassName, IServerNetworkable *pNetworkable ) = 0;
	virtual IEntityFactory *FindFactory( const char *pClassName ) = 0;
	virtual const char *GetCannonicalName( const char *pClassName ) = 0;
};

IEntityFactoryDictionary *EntityFactoryDictionary();

inline bool CanCreateEntityClass( const char *pszClassname )
{
	return ( EntityFactoryDictionary() != nullptr && EntityFactoryDictionary()->FindFactory( pszClassname ) != nullptr );
}

class IEntityFactory
{
public:
	virtual IServerNetworkable *Create( const char *pClassName ) = 0;
	virtual void Destroy( IServerNetworkable *pNetworkable ) = 0;
	virtual std::size_t GetEntitySize() = 0;
};

template <class T>
class CEntityFactory : public IEntityFactory
{
public:
	CEntityFactory( const char *pClassName )
	{
		EntityFactoryDictionary()->InstallFactory( this, pClassName );
	}

	IServerNetworkable *Create( const char *pClassName )
	{
		T* pEnt = _CreateEntityTemplate((T*)nullptr, pClassName);
		return pEnt->NetworkProp();
	}

	void Destroy( IServerNetworkable *pNetworkable )
	{
		if ( pNetworkable )
		{
			pNetworkable->Release();
		}
	}

	virtual std::size_t GetEntitySize()
	{
		return sizeof(T);
	}
};

// Registers an entity class: the factory for the map's classname, and the dispatch thunks of the class' kind
// (entity_dispatch.h; the class must be in entity_kind_list.h, which is checked here).

// A further map name of a class that is already registered (once per class: the kind's thunks are defined there).

// A class that is only made with CREATE_UNSAVED_ENTITY, never from a map: it still is an entity kind.

//
// Conversion among the three types of "entity", including identity-conversions.
//
extern CGlobalVars *gpGlobals;
extern bool g_bIsLogging;

inline int ENTINDEX( edict_t *pEdict )
{ 
	if ( !pEdict ) 
		return 0;
	int edictIndex = pEdict - gpGlobals->pEdicts; 
	contract_assert( edictIndex < MAX_EDICTS && edictIndex >= 0 );
	return edictIndex;
}

int	  ENTINDEX( CBaseEntity *pEnt );

inline edict_t* INDEXENT( int iEdictNum )		
{
	contract_assert( iEdictNum>=0 && iEdictNum < MAX_EDICTS );
	if ( gpGlobals->pEdicts )
	{
		edict_t *pEdict = gpGlobals->pEdicts + iEdictNum;
		if ( pEdict->IsFree() )
			return nullptr;
		return pEdict;
	}
	return nullptr;
}

// Testing the three types of "entity" for nullity
inline bool FNullEnt(const edict_t* pent)
{ 
	return pent == nullptr || ENTINDEX((edict_t*)pent) == 0; 
}

// Dot products for view cone checking
inline constexpr auto VIEW_FIELD_FULL = (float)-1.0; // +-180 degrees
inline constexpr auto VIEW_FIELD_WIDE = (float)-0.7; // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks
inline constexpr auto VIEW_FIELD_NARROW = (float)0.7; // +-45 degrees, more narrow check used to set up ranged attacks
inline constexpr auto VIEW_FIELD_ULTRA_NARROW = (float)0.9; // +-25 degrees, more narrow check used to set up ranged attacks


// Misc useful
inline bool FStrEq(const char *sz1, const char *sz2)
{
	return ( sz1 == sz2 || stricmp(sz1, sz2) == 0 );
}

// Misc. Prototypes
void		UTIL_SetSize			(CBaseEntity *pEnt, const Vector &vecMin, const Vector &vecMax);
void		UTIL_ClearTrace			( trace_t &trace );
void		UTIL_SetTrace			(trace_t& tr, const Ray_t &ray, edict_t* edict, float fraction, int hitgroup, unsigned int contents, const Vector& normal, float intercept );

int			UTIL_PrecacheDecal		( const char *name, bool preload = false );

//-----------------------------------------------------------------------------

float		UTIL_GetSimulationInterval();

//-----------------------------------------------------------------------------
// Purpose: Gets a player pointer by 1-based index
//			If player is not yet spawned or connected, returns NULL
// Input  : playerIndex - index of the player - first player is index 1
//-----------------------------------------------------------------------------

// NOTENOTE: Use UTIL_GetLocalPlayer instead of UTIL_PlayerByIndex IF you're in single player
// and you want the player.
CBasePlayer	*UTIL_PlayerByIndex( int playerIndex );

// NOTENOTE: Use this instead of UTIL_PlayerByIndex IF you're in single player
// and you want the player.
// not useable in multiplayer - see UTIL_GetListenServerHost()
CBasePlayer* UTIL_GetLocalPlayer( void );

// The player of a single player game, else nullptr (no warning in multiplayer).
inline bool UTIL_IsSinglePlayer() { return gpGlobals->maxClients == 1; }
inline CBasePlayer *UTIL_GetSinglePlayer() { return UTIL_IsSinglePlayer() ? UTIL_GetLocalPlayer() : nullptr; }

// get the local player on a listen server
CBasePlayer *UTIL_GetListenServerHost( void );

// Convenience function so we don't have to make this check all over
inline CBasePlayer * UTIL_GetLocalPlayerOrListenServerHost( void )
{
	if ( gpGlobals->maxClients > 1 )
	{
		if ( engine->IsDedicatedServer() )
		{
			return nullptr;
		}

		return UTIL_GetListenServerHost();
	}

	return UTIL_GetLocalPlayer();
}

CBasePlayer* UTIL_PlayerByUserId( int userID );
CBasePlayer* UTIL_PlayerByName( const char *name ); // not case sensitive
CBasePlayer* UTIL_PlayerByAccountID( AccountID_t accountID );

// Returns true if the command was issued by the listenserver host, or by the dedicated server, via rcon or the server console.
// This is valid during ConCommand execution.
bool UTIL_IsCommandIssuedByServerAdmin( void );

CBaseEntity* UTIL_EntityByIndex( int entityIndex );

void		UTIL_GetPlayerConnectionInfo( int playerIndex, int& ping, int &packetloss );

void		UTIL_SetClientVisibilityPVS( edict_t *pClient, const unsigned char *pvs, int pvssize );
bool		UTIL_ClientPVSIsExpanded();

edict_t		*UTIL_FindClientInPVS( edict_t *pEdict );
edict_t		*UTIL_FindClientInVisibilityPVS( edict_t *pEdict );

edict_t		*UTIL_FindClientInPVSGuts(edict_t *pEdict, unsigned char *pvs, unsigned pvssize );

// This is a version which finds any clients whose PVS intersects the box
CBaseEntity *UTIL_FindClientInPVS( const Vector &vecBoxMins, const Vector &vecBoxMaxs );

CBaseEntity *UTIL_EntitiesInPVS( CBaseEntity *pPVSEntity, CBaseEntity *pStartingEntity );

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
int			UTIL_EntitiesInBox( const Vector &mins, const Vector &maxs, CFlaggedEntitiesEnum *pEnum  );
int			UTIL_EntitiesInSphere( const Vector &center, float radius, CFlaggedEntitiesEnum *pEnum  );

inline int UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask )
{
	CFlaggedEntitiesEnum boxEnum( pList, listMax, flagMask );
	return UTIL_EntitiesInBox( mins, maxs, &boxEnum );
}

inline int UTIL_EntitiesInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius, int flagMask )
{
	CFlaggedEntitiesEnum sphereEnum( pList, listMax, flagMask );
	return UTIL_EntitiesInSphere( center, radius, &sphereEnum );
}

// marks the entity for deletion so it will get removed next frame
void UTIL_Remove( IServerNetworkable *oldObj );
void UTIL_Remove( CBaseEntity *oldObj );

// deletes an entity, without any delay.  Only use this when sure no pointers rely on this entity.
void UTIL_DisableRemoveImmediate();
void UTIL_EnableRemoveImmediate();
void UTIL_RemoveImmediate( CBaseEntity *oldObj );

// make this a fixed size so it just sits on the stack
inline constexpr auto MAX_SPHERE_QUERY = 512;
class CEntitySphereQuery
{
public:
	// currently this builds the list in the constructor
	// UNDONE: make an iterative query of ISpatialPartition so we could
	// make queries like this optimal
	CEntitySphereQuery( const Vector &center, float radius, int flagMask=0 );
	CBaseEntity *GetCurrentEntity();
	inline void NextEntity() { m_listIndex++; }

private:
	int			m_listIndex;
	int			m_listCount;
	CBaseEntity *m_pList[MAX_SPHERE_QUERY];
};

// Drops an entity onto the floor
int			UTIL_DropToFloor( CBaseEntity *pEntity, unsigned int mask, CBaseEntity *pIgnore = nullptr );

// Returns false if any part of the bottom of the entity is off an edge that is not a staircase.
bool		UTIL_CheckBottom( CBaseEntity *pEntity, ITraceFilter *pTraceFilter, float flStepSize );

void		UTIL_SetOrigin			( CBaseEntity *entity, const Vector &vecOrigin, bool bFireTriggers = false );
void		UTIL_EmitAmbientSound	( int entindex, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float soundtime = 0.0f, float *duration = nullptr );
void		UTIL_ParticleEffect		( const Vector &vecOrigin, const Vector &vecDirection, uint32 ulColor, int ulCount );
void		UTIL_ScreenShake		( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake = false, std::vector< CBasePlayer * > *ignore = nullptr );
void		UTIL_ScreenShakeObject	( CBaseEntity *pEnt, const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake=false );
void		UTIL_ScreenTilt			( const Vector &center, const QAngle &tiltAngle, float duration, float radius, float tiltTime, ShakeCommand_t eCommand, bool bEaseInOut );
void		UTIL_ViewPunch			( const Vector &center, QAngle angPunch, float radius, bool bInAir );
void		UTIL_ShowMessage		( const char *pString, CBasePlayer *pPlayer );
void		UTIL_ShowMessageAll		( const char *pString );
void		UTIL_ScreenFadeAll		( const color32 &color, float fadeTime, float holdTime, int flags );
void		UTIL_ScreenFade			( CBaseEntity *pEntity, const color32 &color, float fadeTime, float fadeHold, int flags );
void		UTIL_MuzzleFlash		( const Vector &origin, const QAngle &angles, int scale, int type );
Vector		UTIL_PointOnLineNearestPoint(const Vector& vStartPos, const Vector& vEndPos, const Vector& vPoint, bool clampEnds = false );

int			UTIL_EntityInSolid( CBaseEntity *ent );

bool		UTIL_IsMasterTriggered	(string_t sMaster, CBaseEntity *pActivator);
void		UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount );
void		UTIL_BloodSpray( const Vector &pos, const Vector &dir, int color, int amount, int flags );
void		UTIL_BloodSprayPrecache();
Vector		UTIL_RandomBloodVector( void );
void		UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName = nullptr );
void		UTIL_PlayerDecalTrace( trace_t *pTrace, Vector const &right, int playernum );
void		UTIL_Smoke( const Vector &origin, const float scale, const float framerate );
void		UTIL_AxisStringToPointDir( Vector &start, Vector &dir, const char *pString );
void		UTIL_AxisStringToPointPoint( Vector &start, Vector &end, const char *pString );
void		UTIL_AxisStringToUnitDir( Vector &dir, const char *pString );
void		UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip );
void		UTIL_PredictedPosition( CBaseEntity *pTarget, float flTimeDelta, Vector *vecPredictedPosition );
void		UTIL_Beam( Vector &Start, Vector &End, int nModelIndex, int nHaloIndex, unsigned char FrameStart, unsigned char FrameRate,
				float Life, unsigned char Width, unsigned char EndWidth, unsigned char FadeLength, unsigned char Noise, unsigned char Red, unsigned char Green,
				unsigned char Blue, unsigned char Brightness, unsigned char Speed);

const char	*UTIL_VarArgs( const char *format, ... ) [[gnu::format(printf, 1, 2)]];
bool		UTIL_IsValidEntity( CBaseEntity *pEnt );
bool		UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 );

// snaps a vector to the nearest axis vector (if within epsilon)
void		UTIL_SnapDirectionToAxis( Vector &direction, float epsilon = 0.002f );

//Set the entity to point at the target specified
bool UTIL_PointAtEntity( CBaseEntity *pEnt, CBaseEntity *pTarget );
void UTIL_PointAtNamedEntity( CBaseEntity *pEnt, string_t strTarget );

// Copy the pose parameter values from one entity to the other
bool UTIL_TransferPoseParameters( CBaseEntity *pSourceEntity, CBaseEntity *pDestEntity );

void UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count );
void UTIL_BubbleTrail( const Vector& from, const Vector& to, int count );

// allows precacheing of other entities
void UTIL_PrecacheOther( const char *szClassname, const char *modelName = nullptr );

// Creates the netoworking baseline for this entity's serverclass if it doesn't yet exist in the engine's list.
void UTIL_EnsureInstanceBaseline( const char *szClassname );

// prints a message to each client
void			UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1 = nullptr, const char *param2 = nullptr, const char *param3 = nullptr, const char *param4 = nullptr );
inline void		UTIL_CenterPrintAll( const char *msg_name, const char *param1 = nullptr, const char *param2 = nullptr, const char *param3 = nullptr, const char *param4 = nullptr ) 
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, msg_name, param1, param2, param3, param4 );
}

void UTIL_ValidateSoundName( string_t &name, const char *defaultStr );

void UTIL_ClientPrintFilter( IRecipientFilter& filter, int msg_dest, const char *msg_name, const char *param1 = nullptr, const char *param2 = nullptr, const char *param3 = nullptr, const char *param4 = nullptr );

// prints messages through the HUD
void ClientPrint( CBasePlayer *player, int msg_dest, const char *msg_name, const char *param1 = nullptr, const char *param2 = nullptr, const char *param3 = nullptr, const char *param4 = nullptr );

// prints a message to the HUD say (chat)
void		UTIL_SayText( const char *pText, CBasePlayer *pEntity );
void		UTIL_SayTextAll( const char *pText, CBasePlayer *pEntity = nullptr, EUtilSayTextMessageType_t eMessageType = kEUtilSayTextMessageType_Default );
void		UTIL_SayTextFilter( IRecipientFilter& filter, const char *pText, CBasePlayer *pEntity, EUtilSayTextMessageType_t eMessageType );
void		UTIL_SayText2Filter( IRecipientFilter& filter, CBasePlayer *pEntity, EUtilSayTextMessageType_t eMessageType, const char *msg_name, const char *param1 = nullptr, const char *param2 = nullptr, const char *param3 = nullptr, const char *param4 = nullptr );

byte		*UTIL_LoadFileForMe( const char *filename, int *pLength );
void        UTIL_FreeFile( byte *buffer );

//-----------------------------------------------------------------------------
// These are inlined for backwards compatibility
//-----------------------------------------------------------------------------
inline float UTIL_Approach( float target, float value, float speed )
{
	return Approach( target, value, speed );
}

inline float UTIL_ApproachAngle( float target, float value, float speed )
{
	return ApproachAngle( target, value, speed );
}

inline float UTIL_AngleDistance( float next, float cur )
{
	return AngleDistance( next, cur );
}

inline float UTIL_AngleMod(float a)
{
	return anglemod(a);
}

inline float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	return AngleDiff( destAngle, srcAngle );
}

typedef struct hudtextparms_s
{
	float		x;
	float		y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime;
	float		fadeoutTime;
	float		holdTime;
	float		fxTime;
	int			channel;
} hudtextparms_t;

//-----------------------------------------------------------------------------
// Sets the model to be associated with an entity
//-----------------------------------------------------------------------------
void UTIL_SetModel( CBaseEntity *pEntity, const char *pModelName );

// prints as transparent 'title' to the HUD
void			UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage );
void			UTIL_HudMessage( CBasePlayer *pToPlayer, const hudtextparms_t &textparms, const char *pMessage );

// brings up hud keyboard hints display
void			UTIL_HudHintText( CBaseEntity *pEntity, const char *pMessage );

// Writes message to console with timestamp and FragLog header.
void			UTIL_LogPrintf( const char *fmt, ... ) [[gnu::format(printf, 1, 2)]];

// Sorta like FInViewCone, but for nonNPCs. 
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir );

void UTIL_StripToken( const char *pKey, char *pDest );// for redundant keynames

// Misc functions
int BuildChangeList( levellist_t *pLevelList, int maxList );

// computes gravity scale for an absolute gravity.  Pass the result into CBaseEntity::SetGravity()
float UTIL_ScaleForGravity( float desiredGravity );
//
// How did I ever live without ASSERT?
//
#ifdef	DEBUG
void DBG_AssertFunction(bool fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage);
#else	// !DEBUG
#endif	// !DEBUG

//
// Constants that were used only by QC (maybe not used at all now)
//
// Un-comment only as needed
//

inline constexpr auto LFO_SQUARE = 1;
inline constexpr auto LFO_TRIANGLE = 2;
inline constexpr auto LFO_RANDOM = 3;

// func_rotating
inline constexpr auto SF_BRUSH_ROTATE_Y_AXIS = 0;
inline constexpr auto SF_BRUSH_ROTATE_START_ON = 1;
inline constexpr auto SF_BRUSH_ROTATE_BACKWARDS = 2;
inline constexpr auto SF_BRUSH_ROTATE_Z_AXIS = 4;
inline constexpr auto SF_BRUSH_ROTATE_X_AXIS = 8;

inline constexpr auto SF_BRUSH_ROTATE_SMALLRADIUS = 128;
inline constexpr auto SF_BRUSH_ROTATE_MEDIUMRADIUS = 256;
inline constexpr auto SF_BRUSH_ROTATE_LARGERADIUS = 512;
inline constexpr auto SF_BRUSH_ROTATE_CLIENTSIDE = 1024;

inline constexpr auto PUSH_BLOCK_ONLY_X = 1;
inline constexpr auto PUSH_BLOCK_ONLY_Y = 2;

inline constexpr auto SF_LIGHT_START_OFF = 1;

inline constexpr auto SPAWNFLAG_NOMESSAGE = 1;
inline constexpr auto SPAWNFLAG_NOTOUCH = 1;
inline constexpr auto SPAWNFLAG_DROIDONLY = 4;

inline constexpr auto SPAWNFLAG_USEONLY = 1; // can't be touched, must be used (buttons)

inline constexpr auto TELE_PLAYER_ONLY = 1;
inline constexpr auto TELE_SILENT = 2;

// Sound Utilities

void SENTENCEG_Init();
void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick);
int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, float volume, soundlevel_t soundlevel, int flags, int pitch);
int SENTENCEG_PlayRndSz(edict_t *entity, const char *szrootname, float volume, soundlevel_t soundlevel, int flags, int pitch);
int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szrootname, float volume, soundlevel_t soundlevel, int flags, int pitch, int ipick, int freset);
void SENTENCEG_PlaySentenceIndex( edict_t *entity, int iSentenceIndex, float volume, soundlevel_t soundlevel, int flags, int pitch );
int SENTENCEG_PickRndSz(const char *szrootname);
int SENTENCEG_GetIndex(const char *szrootname);
int SENTENCEG_Lookup(const char *sample);

char TEXTURETYPE_Find( trace_t *ptr );

void UTIL_EmitSoundSuit(edict_t *entity, const char *sample);
int  UTIL_EmitGroupIDSuit(edict_t *entity, int isentenceg);
int  UTIL_EmitGroupnameSuit(edict_t *entity, const char *groupname);
void UTIL_RestartAmbientSounds( void );

class EntityMatrix : public VMatrix
{
public:
	void InitFromEntity( CBaseEntity *pEntity, int iAttachment=0 );
	void InitFromEntityLocal( CBaseEntity *entity );

	inline Vector LocalToWorld( const Vector &vVec ) const
	{
		return VMul4x3( vVec );
	}

	inline Vector WorldToLocal( const Vector &vVec ) const
	{
		return VMul4x3Transpose( vVec );
	}

	inline Vector LocalToWorldRotation( const Vector &vVec ) const
	{
		return VMul3x3( vVec );
	}

	inline Vector WorldToLocalRotation( const Vector &vVec ) const
	{
		return VMul3x3Transpose( vVec );
	}
};

inline float UTIL_DistApprox( const Vector &vec1, const Vector &vec2 );
inline float UTIL_DistApprox2D( const Vector &vec1, const Vector &vec2 );

//---------------------------------------------------------
//---------------------------------------------------------
inline float UTIL_DistApprox( const Vector &vec1, const Vector &vec2 )
{
	float dx;
	float dy;
	float dz;

	dx = vec1.x - vec2.x;
	dy = vec1.y - vec2.y;
	dz = vec1.z - vec2.z;

	return std::fabs(dx) + std::fabs(dy) + std::fabs(dz);
}

//---------------------------------------------------------
//---------------------------------------------------------
inline float UTIL_DistApprox2D( const Vector &vec1, const Vector &vec2 )
{
	float dx;
	float dy;

	dx = vec1.x - vec2.x;
	dy = vec1.y - vec2.y;

	return std::fabs(dx) + std::fabs(dy);
}

// Find out if an entity is facing another entity or position within a given tolerance range
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, const Vector &vecPosition, float flDotTolerance, float *pflDot = nullptr );
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, CBaseEntity *pTarget, float flDotTolerance, float *pflDot = nullptr );

void UTIL_GetDebugColorForRelationship( int nRelationship, int &r, int &g, int &b );

extern const char	*UTIL_FunctionToName( datamap_t *pMap, inputfunc_t pFunction );
extern void UTIL_FunctionFromName( datamap_t *pMap, const char *pName, inputfunc_t *ppFunction );

int UTIL_GetCommandClientIndex( void );
CBasePlayer *UTIL_GetCommandClient( void );

AngularImpulse WorldToLocalRotation( const VMatrix &localToWorld, const Vector &worldAxis, float rotation );
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles );
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat );
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles );
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat );

bool UTIL_LoadAndSpawnEntitiesFromScript( std::vector< CBaseEntity* > &entities, const char *pScriptFile, const char *pBlock, bool bActivate = true );

// HudMessagePanel helpers
void UTIL_MessageTextAll( const char *text, Color color = Color( 0, 0, 0, 0 ) );	// Send a HudMessagePanel string to clients
void UTIL_MessageText( CBasePlayer *player, const char *text, Color color = Color( 0, 0, 0, 0 ) );	// Send a HudMessagePanel string to a client
void UTIL_ResetMessageTextAll( void );												// Reset clients' HudMessagePanel
void UTIL_ResetMessageText( CBasePlayer *player );									// Reset a client's HudMessagePanel

void UTIL_SendClientCommandKVToPlayer( KeyValues *pKV, CBasePlayer *pPlayer = nullptr );
void UTIL_RecordAchievementEvent( const char *pszAchievementname, CBasePlayer *pPlayer = nullptr );

//--------------------------------------------------------------------------------------------------------
/**
 * Return true if ground is fairly level within the given radius around an entity
 * Trace 4 vertical hull-quadrants and test their collisions and ground heights and normals
 */
bool UTIL_IsGroundLevel( float radius, const Vector &position, float hullHeight, int mask, const CBaseEntity *ignore, bool debugTraces = false );

// Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
void UTIL_BoundToWorldSize( Vector *pVecPos );

// maximum number of targets a single multi_manager entity may be assigned.
inline constexpr auto MAX_MULTI_TARGETS = 16;

// NPCEvent.h declarations

extern void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

// people gib if their health is <= this at the time of death
inline constexpr auto GIB_HEALTH_VALUE = -30;

inline constexpr auto MAX_OLD_ENEMIES = 4; // how many old enemies to remember

// used by suit voice to indicate damage sustained and repaired type to player

enum
{
	itbd_Paralyze = 0,
	itbd_NerveGas,
	itbd_PoisonRecover,
	itbd_Radiation,
	itbd_DrownRecover,
	itbd_Acid,
	itbd_SlowBurn,
	itbd_SlowFreeze,

	// Must be last!
	CDMG_TIMEBASED
};

// when calling KILLED(), a value that governs gib behavior is expected to be 
// one of these three values
inline constexpr auto GIB_NORMAL = 0; // gib if entity was overkilled
inline constexpr auto GIB_NEVER = 1; // never gib, no matter how much death damage is done ( freezing, etc )
inline constexpr auto GIB_ALWAYS = 2; // always gib

// The parts of the script interface that are bound to one game side: GetScriptDesc<T> is specialised
// for the side's classes and CScriptScope runs on the side's VM (g_pScriptVM). They are not part
// of ivscript.h, which is shared with the engine.

struct ScriptNoBase_t;

// We use template specialization to allow classes to optionally override this function.
// For a given class, if this function is NOT overridden, the class' descriptor will use the base class'
// IScriptInstanceHelper object.
// If this function IS overridden, it will use the return value of the overridden function
template < typename TScriptClass > 
IScriptInstanceHelper *GetScriptInstanceHelperOverride( IScriptInstanceHelper *pBaseClassHelper )
{
	return pBaseClassHelper;
}

inline IScriptInstanceHelper *GetScriptInstanceHelper_ScriptNoBase_t()
{
	return nullptr;
}

// A root class has no base helper: the global GetScriptInstanceHelper_ScriptNoBase_t is not
// redeclared, which a module could not do.

template <typename T> ScriptClassDesc_t *GetScriptDesc(T *);

template <>
inline ScriptClassDesc_t *GetScriptDesc<ScriptNoBase_t>( ScriptNoBase_t *) { return nullptr; }

//-----------------------------------------------------------------------------
// Script scope helper class
//-----------------------------------------------------------------------------

extern IScriptVM *g_pScriptVM;
class CDefScriptScopeBase
{
public:
	static IScriptVM *GetVM()
	{
		return g_pScriptVM;
	}
};

template <class BASE_CLASS = CDefScriptScopeBase>
class CScriptScopeT : public CDefScriptScopeBase
{
public:
	CScriptScopeT() :
		m_hScope( INVALID_HSCRIPT ),
		m_flags( 0 )
	{
	}

	~CScriptScopeT()
	{
		Term();
	}

	bool IsInitialized()
	{
		return m_hScope != INVALID_HSCRIPT;
	}

	bool Init( const char *pszName )
	{
		m_hScope = GetVM()->CreateScope( pszName );
		return ( m_hScope != nullptr );
	}

	bool Init( HSCRIPT hScope, bool bExternal = true )
	{
		if ( bExternal )
		{
			m_flags |= EXTERNAL;
		}
		m_hScope = hScope;
		return ( m_hScope != nullptr );
	}

	bool InitGlobal()
	{
		contract_assert( 0 ); // todo [3/24/2008 tom]
		m_hScope = GetVM()->CreateScope( "" );
		return ( m_hScope != nullptr );
	}

	void Term()
	{
		if ( m_hScope != INVALID_HSCRIPT )
		{
			IScriptVM *pVM = GetVM();
			if ( pVM )
			{
				for ( HSCRIPT *pFunc : m_FuncHandles )
					pVM->ReleaseFunction( *pFunc );
			}
			m_FuncHandles.clear();
			if ( m_hScope && pVM && !(m_flags & EXTERNAL) )
			{
				pVM->ReleaseScope( m_hScope );
			}
			m_hScope = INVALID_HSCRIPT;
		}
		m_flags = 0;
	}

	void InvalidateCachedValues()
	{
		IScriptVM *pVM = GetVM();
		for ( HSCRIPT *pFunc : m_FuncHandles )
		{
			if ( *pFunc )
				pVM->ReleaseFunction( *pFunc );
			*pFunc = INVALID_HSCRIPT;
		}
		m_FuncHandles.clear();
	}

	operator HSCRIPT()
	{
		return ( m_hScope != INVALID_HSCRIPT ) ? m_hScope : nullptr;
	}

	bool ValueExists( const char *pszKey )																							{ return GetVM()->ValueExists( m_hScope, pszKey ); }
	bool SetValue( const char *pszKey, const ScriptVariant_t &value )																{ return GetVM()->SetValue(m_hScope, pszKey, value ); }
	bool GetValue( const char *pszKey, ScriptVariant_t *pValue )																	{ return GetVM()->GetValue(m_hScope, pszKey, pValue ); }
	void ReleaseValue( ScriptVariant_t &value )																						{ GetVM()->ReleaseValue( value ); }
	bool ClearValue( const char *pszKey)																							{ return GetVM()->ClearValue( m_hScope, pszKey ); }

	ScriptStatus_t Run( HSCRIPT hScript )
	{
		InvalidateCachedValues();
		return GetVM()->Run( hScript, m_hScope );
	}

	ScriptStatus_t Run( const char *pszScriptText, const char *pszScriptName = nullptr )
	{
		InvalidateCachedValues();
		HSCRIPT hScript = GetVM()->CompileScript( pszScriptText, pszScriptName );
		if ( hScript )
		{
			ScriptStatus_t result = GetVM()->Run( hScript, m_hScope );
			GetVM()->ReleaseScript( hScript );
			return result; 
		}
		return SCRIPT_ERROR;
	}

	ScriptStatus_t Run( const unsigned char *pszScriptText, const char *pszScriptName = nullptr )
	{
		return Run( (const char *)pszScriptText, pszScriptName);
	}

	HSCRIPT LookupFunction( const char *pszFunction )
	{
		return GetVM()->LookupFunction( pszFunction, m_hScope );
	}

	void ReleaseFunction( HSCRIPT hScript )
	{
		GetVM()->ReleaseFunction( hScript );
	}

	bool FunctionExists( const char *pszFunction )
	{
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		GetVM()->ReleaseFunction( hFunction );
		return ( hFunction != nullptr ) ;
	}

	//-----------------------------------------------------

	enum Flags_t
	{
		EXTERNAL = 0x01,
	};

	//-----------------------------------------------------

	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn = nullptr )
	{
		return GetVM()->ExecuteFunction( hFunction, nullptr, 0, pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1 )
	{
		ScriptVariant_t args[1]; args[0] = arg1;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2 )
	{
		ScriptVariant_t args[2]; args[0] = arg1; args[1] = arg2;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3 )
	{
		ScriptVariant_t args[3]; args[0] = arg1; args[1] = arg2; args[2] = arg3;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4 )
	{
		ScriptVariant_t args[4]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5 )
	{
		ScriptVariant_t args[5]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6 )
	{
		ScriptVariant_t args[6]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7 )
	{
		ScriptVariant_t args[7]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8 )
	{
		ScriptVariant_t args[8]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9 )
	{
		ScriptVariant_t args[9]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10 )
	{
		ScriptVariant_t args[10]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11 )
	{
		ScriptVariant_t args[11]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12 )
	{
		ScriptVariant_t args[12]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13 )
	{
		ScriptVariant_t args[13]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13, ARG_TYPE_14 arg14 )
	{
		ScriptVariant_t args[14]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13; args[13] = arg14; 
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn = nullptr )
	{
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, nullptr, 0, pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1 )
	{
		ScriptVariant_t args[1]; args[0] = arg1;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2 )
	{
		ScriptVariant_t args[2]; args[0] = arg1; args[1] = arg2;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3 )
	{
		ScriptVariant_t args[3]; args[0] = arg1; args[1] = arg2; args[2] = arg3;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4 )
	{
		ScriptVariant_t args[4]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5 )
	{
		ScriptVariant_t args[5]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6 )
	{
		ScriptVariant_t args[6]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7 )
	{
		ScriptVariant_t args[7]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8 )
	{
		ScriptVariant_t args[8]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9 )
	{
		ScriptVariant_t args[9]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10 )
	{
		ScriptVariant_t args[10]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11 )
	{
		ScriptVariant_t args[11]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12 )
	{
		ScriptVariant_t args[12]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13 )
	{
		ScriptVariant_t args[13]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13, ARG_TYPE_14 arg14 )
	{
		ScriptVariant_t args[14]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13; args[13] = arg14; 
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

protected:
	HSCRIPT m_hScope;
	int m_flags;
	std::vector<HSCRIPT *> m_FuncHandles;
};

typedef CScriptScopeT<> CScriptScope;

#define VScriptAddEnumToScope_( scope, enumVal, scriptName )	(scope).SetValue( scriptName, (int)enumVal )
#define VScriptAddEnumToScope( scope, enumVal )					VScriptAddEnumToScope_( scope, enumVal, #enumVal )

#define VScriptAddEnumToRoot( enumVal )					g_pScriptVM->SetValue( #enumVal, (int)enumVal )

#undef VScriptAddEnumToScope_
#undef VScriptAddEnumToScope
#undef VScriptAddEnumToRoot

class CBaseEntity;

class CAI_BaseNPC;
class CAI_Senses;
class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBasePlayer;
class CCollisionEvent;
class CEventAction;
class CRecipientFilter;
class CScriptKeyValues;
class CSkyCamera;
class CTakeDamageInfo;
class CTeam;
class CUserCmd;
class CWorld;
struct gamevcollisionevent_t;
struct notify_system_event_params_t;
struct touchlink_t;

using namespace ::ks::reflect;

inline constexpr auto TEAMNUM_NUM_BITS = 6;

// CPU_LEVEL_BIT_COUNT / GPU_LEVEL_BIT_COUNT, named by the Net annotations on the four
// m_n{Min,Max}{CPU,GPU}Level members.

// Named by the Net annotation on m_iParentAttachment. The definition lives in
// game/shared/baseentity_shared.h, which includes *this* header, so it is not visible from
// there; an identical redefinition is well-formed.

// Named by the Net annotations on m_vecOrigin and m_angRotation, so they have to be constant
// expressions here rather than const ints in baseentity.cpp beside the table.
constexpr int SENDPROP_ANGROTATION_DEFAULT_BITS = 13;
constexpr int SENDPROP_VECORIGIN_FLAGS = SPROP_CELL_COORD|SPROP_CHANGES_OFTEN;

// DT_BaseEntity names DT_AnimTimeMustBeFirst as a sub-table prop; the global is defined by
// IMPLEMENT_REFLECT_TABLE_IN in baseentity.cpp.
namespace DT_AnimTimeMustBeFirst { extern SendTable g_SendTable; }

void SendProxy_AnimTime( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );
// Defined in baseentity.cpp; named by the Net annotation on m_flSimulationTime.
void SendProxy_SimulationTime( const SendProp *pProp, const void *pStruct,
    const void *pVarData, DVariant *pOut, int iElement, int objectID );
// Defined in baseentity.cpp; named by the Net annotation on m_angRotation.
void SendProxy_Angles( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );
// Defined in baseentity.cpp; named by the SubTable annotation on CBaseEntity, and declared again
// beside CBaseAnimating, which sends the same proxy for its own serveranimdata sub-table.
void *SendProxy_ClientSideAnimation( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients,
    int objectID );

#ifndef AI_CriteriaSet
#endif
using ResponseRules::IResponseSystem;

// Matching the high level concept is significantly better than other criteria
// FIXME:  Could do this in the script file by making it required and bumping up weighting there instead...
inline constexpr auto CONCEPT_WEIGHT = 5.0f;

#define MANUALMODE_GETSET_PROP(type, accessorName, varName) \
	private:\
		type varName;\
	public:\
		inline const type& Get##accessorName##() const { return varName; } \
		inline type& Get##accessorName##() { return varName; } \
		inline void Set##accessorName##( const type &val ) { varName = val; m_NetStateMgr.StateChanged(); }

#define MANUALMODE_GETSET_EHANDLE(type, accessorName, varName) \
	private:\
		CHandle<type> varName;\
	public:\
		inline type* Get##accessorName##() { return varName.Get(); } \
		inline void Set##accessorName##( type *pType ) { varName = pType; m_NetStateMgr.StateChanged(); }

class CSquadNPC;
typedef struct KeyValueData_s KeyValueData;
class INextBot;
class CGlobalEvent;

typedef std::vector< CBaseEntity* > EntityList_t;

//
// Structure passed to input handlers.
//
struct inputdata_t
{
	CBaseEntity *pActivator;		// The entity that initially caused this chain of output events.
	CBaseEntity *pCaller;			// The entity that fired this particular output.
	variant_t value;				// The data parameter for this output.
	int nOutputID;					// The unique ID of the output that was fired.
};

// Serializable list of context as set by entity i/o and used for deducing proper
//  speech state, et al.
struct ResponseContext_t
{
	DECLARE_SIMPLE_DATADESC();

	string_t		m_iszName;
	string_t		m_iszValue;
	float			m_fExpirationTime;		// when to expire context (0 == never)
};

//-----------------------------------------------------------------------------
// Entity events... targetted to a particular entity
// Each event has a well defined structure to use for parameters
//-----------------------------------------------------------------------------
enum EntityEvent_t
{
	ENTITY_EVENT_WATER_TOUCH = 0,		// No data needed
	ENTITY_EVENT_WATER_UNTOUCH,			// No data needed
	ENTITY_EVENT_PARENT_CHANGED,		// No data needed
};

//-----------------------------------------------------------------------------

typedef void (CBaseEntity::*BASEPTR)(void);
typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther );
typedef void (CBaseEntity::*USEPTR)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

#define DEFINE_THINKFUNC( function ) DEFINE_FUNCTION_RAW( function, BASEPTR )
#define DEFINE_ENTITYFUNC( function ) DEFINE_FUNCTION_RAW( function, ENTITYFUNCPTR )
#define DEFINE_USEFUNC( function ) DEFINE_FUNCTION_RAW( function, USEPTR )

// Things that toggle (buttons/triggers/doors) need this
enum TOGGLE_STATE
{
	TS_AT_TOP,
	TS_AT_BOTTOM,
	TS_GOING_UP,
	TS_GOING_DOWN
};

// Debug overlay bits
enum DebugOverlayBits_t
{
	OVERLAY_TEXT_BIT			=	0x00000001,		// show text debug overlay for this entity
	OVERLAY_NAME_BIT			=	0x00000002,		// show name debug overlay for this entity
	OVERLAY_BBOX_BIT			=	0x00000004,		// show bounding box overlay for this entity
	OVERLAY_PIVOT_BIT			=	0x00000008,		// show pivot for this entity
	OVERLAY_MESSAGE_BIT			=	0x00000010,		// show messages for this entity
	OVERLAY_ABSBOX_BIT			=	0x00000020,		// show abs bounding box overlay
	OVERLAY_RBOX_BIT			=   0x00000040,     // show the rbox overlay
	OVERLAY_SHOW_BLOCKSLOS		=	0x00000080,		// show entities that block NPC LOS
	OVERLAY_ATTACHMENTS_BIT		=	0x00000100,		// show attachment points
	OVERLAY_AUTOAIM_BIT			=	0x00000200,		// Display autoaim radius

	OVERLAY_NPC_SELECTED_BIT	=	0x00001000,		// the npc is current selected
	OVERLAY_NPC_NEAREST_BIT		=	0x00002000,		// show the nearest node of this npc
	OVERLAY_NPC_ROUTE_BIT		=	0x00004000,		// draw the route for this npc
	OVERLAY_NPC_TRIANGULATE_BIT =	0x00008000,		// draw the triangulation for this npc
	OVERLAY_NPC_ZAP_BIT			=	0x00010000,		// destroy the NPC
	OVERLAY_NPC_ENEMIES_BIT		=	0x00020000,		// show npc's enemies
	OVERLAY_NPC_CONDITIONS_BIT	=	0x00040000,		// show NPC's current conditions
	OVERLAY_NPC_SQUAD_BIT		=	0x00080000,		// show npc squads
	OVERLAY_NPC_TASK_BIT		=	0x00100000,		// show npc task details
	OVERLAY_NPC_FOCUS_BIT		=	0x00200000,		// show line to npc's enemy and target
	OVERLAY_NPC_VIEWCONE_BIT	=	0x00400000,		// show npc's viewcone
	OVERLAY_NPC_KILL_BIT		=	0x00800000,		// kill the NPC, running all appropriate AI.

	OVERLAY_WC_CHANGE_ENTITY	=	0x01000000,		// object changed during WC edit
	OVERLAY_BUDDHA_MODE			=	0x02000000,		// take damage but don't die

	OVERLAY_NPC_STEERING_REGULATIONS	=	0x04000000,	// Show the steering regulations associated with the NPC

	OVERLAY_TASK_TEXT_BIT		=	0x08000000,		// show task and schedule names when they start

	OVERLAY_PROP_DEBUG			=	0x10000000,

	OVERLAY_NPC_RELATION_BIT	=	0x20000000,		// show relationships between target and all children

	OVERLAY_VIEWOFFSET			=	0x40000000,		// show view offset
};

struct TimedOverlay_t;

/* =========  CBaseEntity  ======== 

  All objects in the game are derived from this.

a list of all CBaseEntitys is kept in gEntList
================================ */

// creates an entity by string name, but does not spawn it
// If iForceEdictIndex is not -1, then it will use the edict by that index. If the index is 
// invalid or there is already an edict using that index, it will error out.
CBaseEntity *CreateEntityByName( const char *className, int iForceEdictIndex = -1, bool bNotify = true );
CBaseNetworkable *CreateNetworkableByName( const char *className );
CBaseEntity *ToEnt( HSCRIPT hScript );

// creates an entity by name, and ensure it's correctness
// does not spawn the entity
// use the CREATE_ENTITY() macro which wraps this, instead of using it directly
template< class T >
T *_CreateEntity( T *newClass, const char *className )
{
	T *newEnt = dynamic_cast<T*>( CreateEntityByName(className, -1) );
	if ( !newEnt )
	{
		Warning( "classname %s used to create wrong class type\n", className );
		contract_assert( 0 );
	}

	return newEnt;
}

// creates an entity and calls all the necessary spawn functions
extern void SpawnEntityByName( const char *className, CEntityMapData *mapData = nullptr );

// calls the spawn functions for an entity
extern int DispatchSpawn( CBaseEntity *pEntity, bool bRunVScripts = true );

inline CBaseEntity *GetContainingEntity( edict_t *pent );

//-----------------------------------------------------------------------------
// Purpose: think contexts
//-----------------------------------------------------------------------------
struct thinkfunc_t
{
	BASEPTR		m_pfnThink;
	string_t	m_iszContext;
	int			m_nNextThinkTick;
	int			m_nLastThinkTick;

	DECLARE_SIMPLE_DATADESC();
};

struct rotatingpushmove_t;

#define CREATE_PREDICTED_ENTITY( className )	\
	CBaseEntity::CreatePredictedEntityByName( className, __FILE__, __LINE__ );

//
// Base Entity.  All entity types derive from this
//
extern CWorld *g_WorldEntity;
class CBaseEntity : public IServerEntity, public CEntityCore
{

public:
	DECLARE_CLASS_NOBASE( CBaseEntity );	

	//----------------------------------------
	// Class vars and functions
	//----------------------------------------
	static inline void Debug_Pause(bool bPause);
	static inline bool Debug_IsPaused(void);
	static inline void Debug_SetSteps(int nSteps);
	static inline bool Debug_ShouldStep(void);
	static inline bool Debug_Step(void);

	static bool				m_bInDebugSelect;
	static int				m_nDebugPlayer;

protected:

	static bool				m_bDebugPause;		// Whether entity i/o is paused for debugging.
	static int				m_nDebugSteps;		// Number of entity outputs to fire before pausing again.

	static bool				sm_bDisableTouchFuncs;	// Disables PhysicsTouch and PhysicsStartTouch function calls
public:
	static bool				sm_bAccurateTriggerBboxChecks;	// SOLID_BBOX entities do a fully accurate trigger vs bbox check when this is set

public:
	// If bServerOnly is true, then the ent never goes to the client. This is used
	// by logical entities.
	CBaseEntity( bool bServerOnly=false );
	virtual ~CBaseEntity();

	// The most-derived class of this entity (entity_kind.h), set once by the constructor from the class the
	// factory asked for. The dispatched methods (entity_dispatch_list.h) and is_a<T>/entity_cast<T> use it.
	EntityKind GetKind() const { return m_kind; }
private:
	EntityKind m_kind = EntityKind::None;
public:

	// Entities are not copyable
	CBaseEntity( const CBaseEntity& ) = delete;
	CBaseEntity& operator=( const CBaseEntity& ) = delete;

	// prediction system
	DECLARE_PREDICTABLE();
	// network data
	DECLARE_SERVERCLASS();
	// data description
	DECLARE_DATADESC();
	// script description
	DECLARE_ENT_SCRIPTDESC();
	
	// memory handling
    void *operator new( std::size_t stAllocateBlock );
    void *operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );
	void operator delete( void *pMem );
	void operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine ) { operator delete(pMem); }

	// Class factory
	static CBaseEntity				*CreatePredictedEntityByName( const char *classname, const char *module, int line, bool persist = false );

// IHandleEntity overrides.
public:
	virtual void			SetRefEHandle( const CBaseHandle &handle );
	virtual const			CBaseHandle& GetRefEHandle() const;

// IServerUnknown overrides
	virtual ICollideable	*GetCollideable();
	virtual IServerNetworkable *GetNetworkable();
	virtual CBaseEntity		*GetBaseEntity();

// IServerEntity overrides.
public:
	virtual void			SetModelIndex( int index );
	virtual int				GetModelIndex( void ) const;
 	virtual string_t		GetModelName( void ) const;
	const char				*ScriptGetModelName( void ) const;

	 string_t		GetAIAddOn( void ) const;

public:
	// virtual methods for derived classes to override
	bool TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace );
	bool TestCollision_Impl( const Ray_t& ray, unsigned int mask, trace_t& trace );
	bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	bool TestHitboxes_Impl( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	void ComputeWorldSpaceSurroundingBox( Vector *pWorldMins, Vector *pWorldMaxs );
	void ComputeWorldSpaceSurroundingBox_Impl( Vector *pWorldMins, Vector *pWorldMaxs );

	// non-virtual methods. Don't override these!
public:
	// An inline version the game code can use
	CCollisionProperty		 *CollisionProp();
	const CCollisionProperty *CollisionProp() const;
	CServerNetworkProperty   *NetworkProp();
	const CServerNetworkProperty *NetworkProp() const;

	bool					IsCurrentlyTouching( void ) const;
	const Vector&			GetAbsOrigin( void ) const;
	const QAngle&			GetAbsAngles( void ) const;  // see also GetVectors()
	inline Vector			Forward() const __restrict__; ///< get my forward (+x) vector
	inline Vector			Left() const __restrict__;    ///< get my left    (+y) vector
	inline Vector			Up() const __restrict__;      ///< get my up      (+z) vector

	SolidType_t				GetSolid() const;
	int			 			GetSolidFlags( void ) const;

	int						GetEFlags() const;
	void					SetEFlags( int iEFlags );
	void					AddEFlags( int nEFlagMask );
	void					RemoveEFlags( int nEFlagMask );

	// Quick way to ask if we have a player entity as a child anywhere in our hierarchy.
	void					RecalcHasPlayerChildBit();
	bool					DoesHavePlayerChild();

	void					SetNavIgnore( float duration = std::numeric_limits<float>::max() );
	void					ClearNavIgnore();
	bool					IsNavIgnored() const;

	// Is the entity floating?
	bool					IsFloating();

	// Called by physics to see if we should avoid a collision test....
	bool ShouldCollide( int collisionGroup, int contentsMask ) const;
	bool ShouldCollide_Impl( int collisionGroup, int contentsMask ) const;

	// Move type / move collide
	void					SetMoveType( MoveType_t val, MoveCollide_t moveCollide = MOVECOLLIDE_DEFAULT );

	// Returns the entity-to-world transform
	matrix3x4_t				&EntityToWorldTransform();
	const matrix3x4_t		&EntityToWorldTransform() const;

	// Some helper methods that transform a point from entity space to world space + back
	void					EntityToWorldSpace( const Vector &in, Vector *pOut ) const;
	void					WorldToEntitySpace( const Vector &in, Vector *pOut ) const;

	// This function gets your parent's transform. If you're parented to an attachment,
	// this calculates the attachment's transform and gives you that.
	//
	// You must pass in tempMatrix for scratch space - it may need to fill that in and return it instead of 
	// pointing you right at a variable in your parent.
	matrix3x4_t&			GetParentToWorldTransform( matrix3x4_t &tempMatrix );

	// Externalized data objects ( see sharreddefs.h for DataObjectType_t )
	bool					HasDataObjectType( int type ) const;
	void					AddDataObjectType( int type );
	void					RemoveDataObjectType( int type );

	void					*GetDataObject( int type );
	void					*CreateDataObject( int type );
	void					DestroyDataObject( int type );
	void					DestroyAllDataObjects( void );

public:
	void SetScaledPhysics( IPhysicsObject *pNewObject );

	// virtual methods; you can override these
public:
	// Owner entity.
	// FIXME: These are virtual only because of CNodeEnt
	CBaseEntity				*GetOwnerEntity() const;
	void SetOwnerEntity( CBaseEntity* pOwner );
	void SetOwnerEntity_Impl( CBaseEntity* pOwner );
	void					SetEffectEntity( CBaseEntity *pEffectEnt );
	CBaseEntity				*GetEffectEntity() const;
	HSCRIPT					GetScriptOwnerEntity();
	 void			SetScriptOwnerEntity( HSCRIPT pOwner );

	// Only CBaseEntity implements these. CheckTransmit calls the virtual ShouldTransmit to see if the
	// entity wants to be sent. If so, it calls SetTransmit, which will mark any dependents for transmission too.
	int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );

	// update the global transmit state if a transmission rule changed
		    int				SetTransmitState( int nFlag);
			int				GetTransmitState( void );
	int						DispatchUpdateTransmitState();
	
	// Do NOT call this directly. Use DispatchUpdateTransmitState.
	int UpdateTransmitState();
	int UpdateTransmitState_Impl();
	
	// Entities (like ropes) use this to own the transmit state of another entity
	// by forcing it to not call UpdateTransmitState.
	void					IncrementTransmitStateOwnedCounter();
	void					DecrementTransmitStateOwnedCounter();
public:

	// This marks the entity for transmission and passes the SetTransmit call to any dependents.
	void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	// This function finds out if the entity is in the 3D skybox. If so, it sets the EFL_IN_SKYBOX
	// flag so the entity gets transmitted to all the clients.
	// Entities usually call this during their Activate().
	// Returns true if the entity is in the skybox (and EFL_IN_SKYBOX was set).
	bool					DetectInSkybox();

	// Returns which skybox the entity is in
	CSkyCamera				*GetEntitySkybox();

	bool					IsSimulatedEveryTick() const;
	bool					IsAnimatedEveryTick() const;
	void					SetSimulatedEveryTick( bool sim );
	void					SetAnimatedEveryTick( bool anim );

public:

	const char * GetTracerType( void );
	const char * GetTracerType_Impl( void );

	// returns a pointer to the entities edict, if it has one.  should be removed!
	inline edict_t			*edict( void )			{ return NetworkProp()->edict(); }
	inline const edict_t	*edict( void ) const	{ return NetworkProp()->edict(); }
	inline int				entindex( ) const		{ return m_Network.entindex(); };
	inline int				GetSoundSourceIndex() const		{ return entindex(); }

	void	SetFadeDistance( float minFadeDist, float maxFadeDist );
	void	SetGlobalFadeScale( float flFadeScale );
	float	GetGlobalFadeScale() const;

	// These methods encapsulate MOVETYPE_FOLLOW, which became obsolete
	void FollowEntity( CBaseEntity *pBaseEntity, bool bBoneMerge = true );
	void StopFollowingEntity( );	// will also change to MOVETYPE_NONE
	bool IsFollowingEntity();
	CBaseEntity *GetFollowedEntity();

	// initialization
	void Spawn( void );
	void Spawn_Impl( void );
	void Precache( void );
	void Precache_Impl( void ) {}

	void SetModel( const char *szModelName );
	void SetModel_Impl( const char *szModelName );

protected:
	// Notification on model load. May be called multiple times for dynamic models.
	// Implementations must call BaseClass::OnNewModel and pass return value through.
	public:
	CStudioHdr* OnNewModel();
	CStudioHdr* OnNewModel_Impl();
protected:

public:

	 void InitSharedVars( void ) {};
	void PostConstructor( const char *szClassname );
	void PostConstructor_Impl( const char *szClassname );
	void PostClientActive( void );
	void PostClientActive_Impl( void );
	void ParseMapData( CEntityMapData *mapData );
	 void OnParseMapDataFinished();
	bool KeyValue( const char *szKeyName, const char *szValue );
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	bool KeyValue( const char *szKeyName, float flValue );
	bool KeyValue_Impl( const char *szKeyName, float flValue );
	bool KeyValue( const char *szKeyName, int nValue );
	bool KeyValue_Impl( const char *szKeyName, int nValue );
	bool KeyValue( const char *szKeyName, const Vector &vecValue );
	bool KeyValue_Impl( const char *szKeyName, const Vector &vecValue );
	bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );
	bool GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen );
	bool KeyValueFromString( const char *szKeyName, const char *szValue )		{ return KeyValue( szKeyName, szValue ); }
	bool KeyValueFromFloat( const char *szKeyName, float flValue )				{ return KeyValue( szKeyName, flValue ); }
	bool KeyValueFromInt( const char *szKeyName, int nValue )					{ return KeyValue( szKeyName, nValue ); }
	bool KeyValueFromVector( const char *szKeyName, const Vector &vecValue )	{ return KeyValue( szKeyName, vecValue ); }

	void ValidateEntityConnections();
	void FireNamedOutput( const char *pszOutput, variant_t variant, CBaseEntity *pActivator, CBaseEntity *pCaller, float flDelay = 0.0f );
	CBaseEntityOutput *FindNamedOutput( const char *pszOutput );

	// Activate - called for each entity after each load game and level load
	void Activate( void );
	void Activate_Impl( void );

	// Called once per frame after the server frame loop has finished and after all messages being
	//  sent to clients have been sent.
	// NOTE: This will not be called unless the entity requests it via gEntList.AddPostClientMessageEntity
	void PostClientMessagesSent( void );

	// Hierarchy traversal
	CBaseEntity *GetMoveParent( void );
	CBaseEntity *GetRootMoveParent();
	CBaseEntity *FirstMoveChild( void );
	CBaseEntity *NextMovePeer( void );

	void		SetName( string_t newTarget );
	void		SetParent( string_t newParent, CBaseEntity *pActivator, int iAttachment = -1 );
	
	// Set the movement parent. Your local origin and angles will become relative to this parent.
	// If iAttachment is a valid attachment on the parent, then your local origin and angles 
	// are relative to the attachment on this entity. If iAttachment == -1, it'll preserve the
	// current m_iParentAttachment.
	void SetParent( CBaseEntity* pNewParent, int iAttachment = -1 );
	void SetParent_Impl( CBaseEntity* pNewParent, int iAttachment = -1 );
	CBaseEntity* GetParent();
	int			GetParentAttachment();

	string_t	GetEntityName();
	const char *GetEntityNameAsCStr();	// This method is temporary for VSCRIPT functionality until we figure out what to do with string_t (sjb)
	const char *GetPreTemplateName(); // Not threadsafe. Get the name stripped of template unique decoration

	bool		NameMatches( const char *pszNameOrWildcard );
	bool		ClassMatches( const char *pszClassOrWildcard );
	bool		NameMatches( string_t nameStr );
	bool		ClassMatches( string_t nameStr );
	bool		NameMatchesExact( string_t nameStr );
	bool		ClassMatchesExact( string_t nameStr );

	void		MarkNeedsNamePurge();

	template <typename T>
	bool		Downcast( string_t iszClass, T **ppResult );

	const float	GetUseLookAtAngle( void ) { return m_flUseLookAtAngle; }
	void		SetUseLookAtAngle( float flNewValue ) { m_flUseLookAtAngle = flNewValue; }

private:

	CNetworkVar( float, m_flUseLookAtAngle, [[= ks::reflect::Net{ .bits = 32 } ]] ); // dot product angle used for +USE events

	bool		NameMatchesComplex( const char *pszNameOrWildcard );
	bool		ClassMatchesComplex( const char *pszClassOrWildcard );
	void		TransformStepData_WorldToParent( CBaseEntity *pParent );
	void		TransformStepData_ParentToParent( CBaseEntity *pOldParent, CBaseEntity *pNewParent );
	void		TransformStepData_ParentToWorld( CBaseEntity *pParent );

public:
	int			GetSpawnFlags( void ) const;
	void		AddSpawnFlags( int nFlags );
	void		RemoveSpawnFlags( int nFlags );
	void		ClearSpawnFlags( void );
	bool		HasSpawnFlags( int nFlags ) const;

	int			GetEffects( void ) const;
	void		AddEffects( int nEffects );
	void		RemoveEffects( int nEffects );
	void		ClearEffects( void );
	void		SetEffects( int nEffects );
	bool		IsEffectActive( int nEffects ) const;

	// makes the entity inactive
	void		MakeDormant( void );
	int			IsDormant( void );

protected:
	
public:
	void		RemoveDeferred( void );	// Sets the entity invisible, and makes it remove itself on the next frame

	// checks to see if the entity is marked for deletion

	// capabilities
	int ObjectCaps( void );
	int ObjectCaps_Impl( void );

	// Verifies that the data description is valid in debug builds.

	// handles an input (usually caused by outputs)
	// returns true if the the value in the pass in should be set, false if the input is to be ignored
	 bool AcceptInput( const char *szInputName, CBaseEntity *pActivator, CBaseEntity *pCaller, variant_t Value, int outputID );

	//
	// Input handlers.
	//
	[[= ks::reflect::Input{ .name = "AlternativeSorting", .type = FIELD_BOOLEAN } ]] void InputAlternativeSorting( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Alpha", .type = FIELD_INTEGER } ]] void InputAlpha( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Color", .type = FIELD_COLOR32 } ]] void InputColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetParent", .type = FIELD_STRING } ]] void InputSetParent( inputdata_t &inputdata );
	void SetParentAttachment( const char *szInputName, const char *szAttachment, bool bMaintainOffset );
	[[= ks::reflect::Input{ .name = "SetParentAttachment", .type = FIELD_STRING } ]] void InputSetParentAttachment( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetParentAttachmentMaintainOffset", .type = FIELD_STRING } ]] void InputSetParentAttachmentMaintainOffset( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ClearParent", .type = FIELD_VOID } ]] void InputClearParent( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetLocalOrigin", .type = FIELD_STRING } ]] void InputSetLocalOrigin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetLocalAngles", .type = FIELD_STRING } ]] void InputSetLocalAngles( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTeam", .type = FIELD_INTEGER } ]] void InputSetTeam( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Use", .type = FIELD_VOID } ]] void InputUse( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Kill", .type = FIELD_VOID } ]] void InputKill( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "KillHierarchy", .type = FIELD_VOID } ]] void InputKillHierarchy( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetDamageFilter", .type = FIELD_STRING } ]] void InputSetDamageFilter( inputdata_t &inputdata );
	void InputDispatchEffect( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableDamageForces", .type = FIELD_VOID } ]] void InputEnableDamageForces( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableDamageForces", .type = FIELD_VOID } ]] void InputDisableDamageForces( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddContext", .type = FIELD_STRING } ]] void InputAddContext( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RemoveContext", .type = FIELD_STRING } ]] void InputRemoveContext( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ClearContext", .type = FIELD_STRING } ]] void InputClearContext( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DispatchResponse", .type = FIELD_STRING } ]] void InputDispatchResponse( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "DisableShadow", .type = FIELD_VOID } ]] void InputDisableShadow( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableShadow", .type = FIELD_VOID } ]] void InputEnableShadow( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableReceivingFlashlight", .type = FIELD_VOID } ]] void InputDisableReceivingFlashlight( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableReceivingFlashlight", .type = FIELD_VOID } ]] void InputEnableReceivingFlashlight( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableDrawInFastReflection", .type = FIELD_VOID } ]] void InputDisableDrawInFastReflection( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableDrawInFastReflection", .type = FIELD_VOID } ]] void InputEnableDrawInFastReflection( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableDraw", .type = FIELD_VOID } ]] void InputDisableDraw( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableDraw", .type = FIELD_VOID } ]] void InputEnableDraw( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddOutput", .type = FIELD_STRING } ]] void InputAddOutput( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireUser1", .type = FIELD_STRING } ]] void InputFireUser1( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireUser2", .type = FIELD_STRING } ]] void InputFireUser2( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireUser3", .type = FIELD_STRING } ]] void InputFireUser3( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FireUser4", .type = FIELD_STRING } ]] void InputFireUser4( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RunScriptCode", .type = FIELD_STRING } ]] void InputRunScript( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RunScriptFile", .type = FIELD_STRING } ]] void InputRunScriptFile( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CallScriptFunction", .type = FIELD_STRING } ]] void InputCallScriptFunction( inputdata_t &inputdata );

	bool RunScriptFile( const char *pScriptFile, bool bUseRootScope = false );
	bool RunScript( const char *pScriptText, const char *pDebugFilename = "CBaseEntity::RunScript" );

	// tries to read a field from the entities data description - result is placed in variant_t
	bool ReadKeyField( const char *varName, variant_t *var );

	// classname access
	void		SetClassname( const char *className );
	const char* GetClassname();

	const char * GetPlayerName() const;
	const char * GetPlayerName_Impl() const{ return nullptr; }

	// Debug Overlays
	void		 EntityText( int text_offset, const char *text, float flDuration, int r = 255, int g = 255, int b = 255, int a = 255 );
	const char	*GetDebugName(void); // do not make this virtual -- designed to handle NULL this
	void         DrawVPhysicsObjectCenterAndContactPoints(IPhysicsObject *obj);
	void DrawDebugGeometryOverlays(void);
	void DrawDebugGeometryOverlays_Impl(void);					
	int DrawDebugTextOverlays(void);
	int DrawDebugTextOverlays_Impl(void);
	void		 DrawTimedOverlays( void );
	void		 DrawBBoxOverlay( float flDuration = 0.0f );
	void		 DrawAbsBoxOverlay();
	void		 DrawRBoxOverlay();

	void		 DrawInputOverlay(const char *szInputName, CBaseEntity *pCaller, variant_t Value);
	void		 DrawOutputOverlay(CEventAction *ev);
	void		 SendDebugPivotOverlay( void );
	void		 AddTimedOverlay( const char *msg, int endTime );

	void		SetSolid( SolidType_t val );

	// save/restore
	// only overload these if you have special data to serialize
	bool ShouldSavePhysics();
	bool ShouldSavePhysics_Impl();

	// handler to reset stuff after you are restored
	// called after all entities have been loaded from all affected levels
	// called before activate
	// NOTE: Always chain to base class when implementing this!
	void OnRestore();
	void OnRestore_Impl();

	int			 GetTextureFrameIndex( void );
	void		 SetTextureFrameIndex( int iIndex );

	// Entities block Line-Of-Sight for NPCs by default.
	// Set this to false if you want to change this behavior.
	void		 SetBlocksLOS( bool bBlocksLOS );
	bool		 BlocksLOS( void );

	void		 SetAIWalkable( bool bBlocksLOS );
	bool		 IsAIWalkable( void );

	// This comes from the "id" key/value that Hammer adds to entities.
	// It is used by Foundry to match up live (engine) entities with Hammer entities.
	inline int		GetHammerID() const { return m_iHammerID; }
private:

public:
	// Networking related methods
	void	NetworkStateChanged();
	void	NetworkStateChanged( void *pVar );

public:
 	void CalcAbsolutePosition();

	// returns the edict index the entity requires when used in save/restore (eg players, world)
	// -1 means it doesn't require any special index
	int RequiredEdictIndex( void );
	int RequiredEdictIndex_Impl( void ){ return -1; } 

	// interface function pts
	void (CBaseEntity::*m_pfnMoveDone)(void);
	void MoveDone( void );
	void MoveDone_Impl( void ){ if (m_pfnMoveDone) (this->*m_pfnMoveDone)();};

	// Why do we have two separate static Instance functions?
	static CBaseEntity *Instance( const CBaseHandle &hEnt );
	static CBaseEntity *Instance( const edict_t *pent );
	static CBaseEntity *Instance( edict_t *pent );
	static CBaseEntity* Instance( int iEnt );

	// Think function handling
	void (CBaseEntity::*m_pfnThink)(void);
	void Think( void );
	void Think_Impl( void ){ if (m_pfnThink) (this->*m_pfnThink)();};

	// Think functions with contexts
	int		RegisterThinkContext( const char *szContext );
	BASEPTR	ThinkSet( BASEPTR func, float flNextThinkTime = 0, const char *szContext = nullptr );
	void	SetNextThink( float nextThinkTime, const char *szContext = nullptr );
	float	GetNextThink( const char *szContext = nullptr );
	float	GetLastThink( const char *szContext = nullptr );
	int		GetNextThinkTick( const char *szContext = nullptr );
	int		GetLastThinkTick( const char *szContext = nullptr );

	float				GetAnimTime() const;
	void				SetAnimTime( float at );

	float				GetSimulationTime() const;
	void				SetSimulationTime( float st );

	void				SetRenderMode( RenderMode_t nRenderMode );
	RenderMode_t		GetRenderMode() const;
	void				SetRenderFX( RenderFx_t nRenderFX );
	RenderFx_t			GetRenderFX() const;

private:
	// NOTE: Keep this near vtable so it's in cache with vtable.
	CServerNetworkProperty m_Network;

public:
	// members
	[[= ks::reflect::Key{ .name = "classname" } ]] string_t m_iClassname;  // identifier for entity creation and save/restore

public:
	const color24 GetRenderColor() const;
	void SetRenderColor( byte r, byte g, byte b );
	void SetRenderColorR( byte r );
	void SetRenderColorG( byte g );
	void SetRenderColorB( byte b );
	void SetRenderAlpha( byte a );
	byte GetRenderAlpha( ) const;

	// was pev->animtime:  consider moving to CBaseAnimating
	float		m_flPrevAnimTime;
	CNetworkVar( float, m_flAnimTime, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN|SPROP_ENCODED_AGAINST_TICKCOUNT, .enc = ks::reflect::WireEnc::Int, .table = "DT_AnimTimeMustBeFirst" } ]] [[= ks::reflect::Proxy<SendProxy_AnimTime, ks::reflect::WireSide::Send>{} ]] );  // this is the point in time that the client will interpolate to position,angle,frame,etc.
	CNetworkVar( float, m_flSimulationTime, [[= ks::reflect::Net{ .bits = SIMULATION_TIME_WINDOW_BITS, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN|SPROP_ENCODED_AGAINST_TICKCOUNT, .enc = ks::reflect::WireEnc::Int, .priority = SENDPROP_SIMULATION_TIME_PRIORITY } ]] [[= ks::reflect::Proxy<SendProxy_SimulationTime, ks::reflect::WireSide::Send>{} ]] );

	int				m_nLastThinkTick;

	// used so we know when things are no longer touching
	int			touchStamp;			

protected:

	// think function handling
	enum thinkmethods_t
	{
		THINK_FIRE_ALL_FUNCTIONS,
		THINK_FIRE_BASE_ONLY,
		THINK_FIRE_ALL_BUT_BASE,
	};
	int		GetIndexForThinkContext( const char *pszContext );
	std::vector< thinkfunc_t >	m_aThinkFunctions;

	void RemoveExpiredConcepts( void );
	bool ContextExpired( int index ) const;
public:
	void		AddContext( const char *nameandvalue ); ///< when name and value are catenated together into one string like name:value
	void		AddContext( const char *pName, const char *pValue, float duration );
	int			GetContextCount() const;						// Call RemoveExpiredConcepts to clean out expired concepts
	int 		FindContextByName( const char *name ) const;
	const char *GetContextName( int index ) const;		// note: context may be expired
	const char *GetContextValue( int index ) const; 	// note: context may be expired
	inline const ResponseContext_t	*GetContextData( int index ) const; // note: context may be expired
	void		ClearAllContexts( void );

protected:
	std::vector< ResponseContext_t > m_ResponseContexts;

	// Map defined context sets
	[[= ks::reflect::Key{ .name = "ResponseContext" } ]] string_t	m_iszResponseContext;

private:
	// list handling
	friend class CGlobalEntityList;
	friend class CThinkSyncTester;

	// was pev->nextthink
	CNetworkVarForDerived( int, m_nNextThinkTick, [[= ks::reflect::As{ FIELD_TICK } ]] [[= ks::reflect::Key{ .name = "nextthink" } ]] );
	// was pev->effects
	CNetworkVar( int, m_fEffects, [[= ks::reflect::Key{ .name = "effects" } ]] [[= ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED } ]] );

////////////////////////////////////////////////////////////////////////////

public:

	// Returns a CBaseAnimating if the entity is derived from CBaseAnimating.
	CBaseAnimating* GetBaseAnimating();
	CBaseAnimating* GetBaseAnimating_Impl(){ return 0; }
	CBaseAnimatingOverlay * GetBaseAnimatingOverlay();
	CBaseAnimatingOverlay * GetBaseAnimatingOverlay_Impl(){ return nullptr; }

	ResponseRules::IResponseSystem * GetResponseSystem();
	ResponseRules::IResponseSystem * GetResponseSystem_Impl();
	void DispatchResponse( const char *conceptName );
	void DispatchResponse_Impl( const char *conceptName );

// Classify - returns the type of group (i.e, "houndeye", or "human military" so that NPCs with different classnames
// still realize that they are teammates. (overridden for NPCs that form groups)
	Class_T Classify( void );
	Class_T Classify_Impl( void );
	 void	DeathNotice ( CBaseEntity *pVictim ) {}// NPC maker children use this to tell the NPC maker that they have died.
	 bool	ShouldAttractAutoAim( CBaseEntity *pAimingEnt ) { return ((GetFlags() & FL_AIMTARGET) != 0); }
	float GetAutoAimRadius();
	float GetAutoAimRadius_Impl();
	Vector GetAutoAimCenter();
	Vector GetAutoAimCenter_Impl(){ return WorldSpaceCenter(); }

	 ITraceFilter*	GetBeamTraceFilter( void );

	// Call this to do a TraceAttack on an entity, performs filtering. Don't call TraceAttack() directly except when chaining up to base class
	void			DispatchTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	bool PassesDamageFilter( const CTakeDamageInfo &info );
	bool PassesDamageFilter_Impl( const CTakeDamageInfo &info );

protected:
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	public: void TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

public:

	 bool	CanBeHitByMeleeAttack( CBaseEntity *pAttacker ) { return true; }

	// returns the amount of damage inflicted
	int OnTakeDamage( const CTakeDamageInfo &info );
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );

	// This is what you should call to apply damage to an entity.
	void TakeDamage( const CTakeDamageInfo &info );

	int TakeHealth( float flHealth, int bitsDamageType );
	int TakeHealth_Impl( float flHealth, int bitsDamageType );

	bool	IsAlive( void )const;
	// Entity killed (only fired once)
	void Event_Killed( const CTakeDamageInfo &info );
	void Event_Killed_Impl( const CTakeDamageInfo &info );
	
	void SendOnKilledGameEvent( const CTakeDamageInfo &info );

	// Notifier that I've killed some other entity. (called from Victim's Event_Killed).
	void Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );
	void Event_KilledOther_Impl( CBaseEntity *pVictim, const CTakeDamageInfo &info ){ return; }

	// UNDONE: Make this data?
	int BloodColor( void );
	int BloodColor_Impl( void );

	void					TraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType );
	bool IsTriggered( CBaseEntity *pActivator );
	bool IsTriggered_Impl( CBaseEntity *pActivator ){return true;}
	bool IsNPC( void ) const;
	bool IsNPC_Impl( void ) const{ return false; }
	CBaseCombatCharacter * MyCombatCharacterPointer( void );
	CBaseCombatCharacter * MyCombatCharacterPointer_Impl( void ){ return nullptr; }
	 INextBot		*MyNextBotPointer( void ) { return nullptr; }
	float GetDelay( void );
	float GetDelay_Impl( void ){ return 0; }
	bool IsMoving( void );
	bool IsMoving_Impl( void );
	bool					IsWorld() const {  return (void *)this == (void *)g_WorldEntity; }
	char const * DamageDecal( int bitsDamageType, int gameMaterial );
	char const * DamageDecal_Impl( int bitsDamageType, int gameMaterial );
	void DecalTrace( trace_t *pTrace, char const *decalName );
	void DecalTrace_Impl( trace_t *pTrace, char const *decalName );
	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName = nullptr );
	void ImpactTrace_Impl( trace_t *pTrace, int iDamageType, const char *pCustomImpactName = nullptr );

	void			AddPoints( int score, bool bAllowNegativeScore );
	void			AddPointsToTeam( int score, bool bAllowNegativeScore );
	void			RemoveAllDecals( void );

	bool OnControls( CBaseEntity *pControls );
	bool OnControls_Impl( CBaseEntity *pControls ){ return false; }
	 bool	HasTarget( string_t targetname );
	bool IsPlayer( void ) const;
	bool IsPlayer_Impl( void ) const{ return false; }
	bool IsNetClient( void ) const;
	bool IsNetClient_Impl( void ) const{ return false; }
	bool IsTemplate( void );
	bool IsTemplate_Impl( void ){ return false; }
	 bool	IsBaseObject( void ) const { return false; }
	bool IsBaseTrain( void ) const;
	bool IsBaseTrain_Impl( void ) const{ return false; }
	bool			IsBSPModel() const;
	bool			IsInWorld( void ) const;

	bool IsBaseCombatWeapon( void ) const;
	bool IsBaseCombatWeapon_Impl( void ) const{ return false; }
	CBaseCombatWeapon * MyCombatWeaponPointer( void );
	CBaseCombatWeapon * MyCombatWeaponPointer_Impl( void ){ return nullptr; }
	//virtual bool					IsGrenadeProjectile( void ) { return false; };

	// If this is a vehicle, returns the vehicle interface

	// UNDONE: Make this data instead of procedural?
	bool IsViewable( void );
	bool IsViewable_Impl( void );					// is this something that would be looked at (model, sprite, etc.)?

	// Team Handling
	CTeam			*GetTeam( void ) const;				// Get the Team this entity is on
	int				GetTeamNumber( void ) const;		// Get the Team number of the team this entity is on
	int				GetPendingTeamNumber( void ) const;	// Get the pending Team number of the team this is entity will be on at the beginning of the next round
	void ChangeTeam( int iTeamNum );
	void ChangeTeam_Impl( int iTeamNum );			// Assign this entity to a team.
	bool			IsInTeam( CTeam *pTeam ) const;		// Returns true if this entity's in the specified team
	bool			InSameTeam( CBaseEntity *pEntity ) const;	// Returns true if the specified entity is on the same team as this one
	bool			IsInAnyTeam( void ) const;			// Returns true if this entity is in any team
	const char		*TeamID( void ) const;				// Returns the name of the team this entity is on.

	// Entity events... these are events targetted to a particular entity
	// Each event defines its own well-defined event data structure
	void OnEntityEvent( EntityEvent_t event, void *pEventData );
	void OnEntityEvent_Impl( EntityEvent_t event, void *pEventData );

	// can stand on this entity?
	bool IsStandable() const;

	// UNDONE: Do these three functions actually need to be virtual???
	bool CanStandOn( CBaseEntity *pSurface ) const;
	bool CanStandOn_Impl( CBaseEntity *pSurface ) const{ return (pSurface && !pSurface->IsStandable()) ? false : true; }
	bool CanStandOn( edict_t	*ent ) const;
	bool CanStandOn_Impl( edict_t	*ent ) const{ return CanStandOn( GetContainingEntity( ent ) ); }
	CBaseEntity * GetEnemy( void );
	CBaseEntity * GetEnemy_Impl( void ){ return nullptr; }
	CBaseEntity * GetEnemy( void ) const;
	CBaseEntity * GetEnemy_Impl( void ) const{ return nullptr; }

	// Paint helper
	// Should never be called on anything that doesn't use PropPaintPowerUser, which overrides this.

	void	ViewPunch( const QAngle &angleOffset );
	void	VelocityPunch( const Vector &vecForce );

	CBaseEntity *GetNextTarget( void );
	
	// fundamental callbacks
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void StartTouch( CBaseEntity *pOther );
	void StartTouch_Impl( CBaseEntity *pOther );
	void Touch( CBaseEntity *pOther );
	void Touch_Impl( CBaseEntity *pOther ); 
	void EndTouch( CBaseEntity *pOther );
	void EndTouch_Impl( CBaseEntity *pOther );
	void StartBlocked( CBaseEntity *pOther );
	void StartBlocked_Impl( CBaseEntity *pOther ){}
	void Blocked( CBaseEntity *pOther );
	void Blocked_Impl( CBaseEntity *pOther );
	void EndBlocked( void );
	void EndBlocked_Impl( void ){}

	// Physics simulation
	void PhysicsSimulate( void );
	void PhysicsSimulate_Impl( void );

public:
	// HACKHACK:Get the trace_t from the last physics touch call (replaces the even-hackier global trace vars)
	static const trace_t &	GetTouchTrace( void );

	// FIXME: Should be private, but I can't make em private just yet
	void					PhysicsImpact( CBaseEntity *other, trace_t &trace );
 	void					PhysicsMarkEntitiesAsTouching( CBaseEntity *other, trace_t &trace );
	void					PhysicsMarkEntitiesAsTouchingEventDriven( CBaseEntity *other, trace_t &trace );
	void PhysicsTouchTriggers( const Vector *pPrevAbsOrigin = nullptr );
	void PhysicsTouchTriggers_Impl( const Vector *pPrevAbsOrigin = nullptr );
	 void			PhysicsLandedOnGround( float flFallingSpeed ) { return; }

	// Physics helper
	static void				PhysicsRemoveTouchedList( CBaseEntity *ent );
	static void				PhysicsNotifyOtherOfUntouch( CBaseEntity *ent, CBaseEntity *other );
	static void				PhysicsRemoveToucher( CBaseEntity *other, touchlink_t *link );

	groundlink_t			*AddEntityToGroundList( CBaseEntity *other );
	void					PhysicsStartGroundContact( CBaseEntity *pentOther );

	static void				PhysicsNotifyOtherOfGroundRemoval( CBaseEntity *ent, CBaseEntity *other );
	static void				PhysicsRemoveGround( CBaseEntity *other, groundlink_t *link );
	static void				PhysicsRemoveGroundList( CBaseEntity *ent );

	void					StartGroundContact( CBaseEntity *ground );
	void					EndGroundContact( CBaseEntity *ground );

	void					SetGroundChangeTime( float flTime );
	float					GetGroundChangeTime( void );

	// Remove this as ground entity for all object resting on this object
	void					WakeRestingObjects();
	bool					HasNPCsOnIt();

	void UpdateOnRemove( void );
	void UpdateOnRemove_Impl( void );
	 void			StopLoopingSounds( void ) {}

	// common member functions
	void					SUB_Remove( void );
	void					SUB_DoNothing( void );
	void					SUB_StartFadeOut( float delay = 10.0f, bool bNotSolid = true );
	void					SUB_StartFadeOutInstant();
	void					SUB_FadeOut ( void );
	void					SUB_Vanish( void );
	void					SUB_CallUseToggle( void ) { this->Use( this, this, USE_TOGGLE, 0 ); }
	void					SUB_PerformFadeOut( void );
	bool SUB_AllowedToFade( void );
	bool SUB_AllowedToFade_Impl( void );

	// change position, velocity, orientation instantly
	// passing NULL means no change
	void Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts = true );
	void Teleport_Impl( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts = true );
	// notify that another entity (that you were watching) was teleported
	void NotifySystemEvent( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );
	void NotifySystemEvent_Impl( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

	int						ShouldToggle( USE_TYPE useType, int currentState );

	// UNDONE: Move these virtuals to CBaseCombatCharacter?
	void MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	void MakeTracer_Impl( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	 int	GetTracerAttachment( void );
	void FireBullets( const FireBulletsInfo_t &info );
	void FireBullets_Impl( const FireBulletsInfo_t &info );
	void DoImpactEffect( trace_t &tr, int nDamageType );
	void DoImpactEffect_Impl( trace_t &tr, int nDamageType ); // give shooter a chance to do a custom impact.

	// OLD VERSION! Use the struct version
	void FireBullets( int cShots, const Vector &vecSrc, const Vector &vecDirShooting, 
		const Vector &vecSpread, float flDistance, int iAmmoType, int iTracerFreq = 4, 
		int firingEntID = -1, int attachmentID = -1, float flDamage = 0, 
		CBaseEntity *pAttacker = nullptr, bool bFirstShotAccurate = false, bool bPrimaryAttack = true );

	CBaseEntity * Respawn( void );
	CBaseEntity * Respawn_Impl( void ){ return nullptr; }

	// Method used to deal with attacks passing through triggers
	void TraceAttackToTriggers( const CTakeDamageInfo &info, const Vector& start, const Vector& end, const Vector& dir );

	// Do the bounding boxes of these two intersect?
	bool	Intersects( CBaseEntity *pOther );
	bool IsLockedByMaster( void );
	bool IsLockedByMaster_Impl( void ){ return false; }

	// Health accessors.
	 int		GetMaxHealth()  const	{ return m_iMaxHealth; }
	void	SetMaxHealth( int amt )	{ m_iMaxHealth = amt; }

	int		GetHealth() const		{ return m_iHealth; }
	void SetHealth( int amt );
	void SetHealth_Impl( int amt ){ m_iHealth = amt; }

	// Ugly code to lookup all functions to make sure they are in the table when set.
	// Turn this off in 64-bit builds because OSX-64 fails to compile.
	void ModifyOrAppendCriteria( AI_CriteriaSet& set );
	void ModifyOrAppendCriteria_Impl( AI_CriteriaSet& set );
	// this computes criteria that depend on the other criteria having been set. 
	// needs to be done in a second pass because we may have multiple overrids for
	// a context before it all settles out.
	 void	ModifyOrAppendDerivedCriteria( AI_CriteriaSet& set ) {};
	void			AppendContextToCriteria( AI_CriteriaSet& set, const char *prefix = "" );
	void			DumpResponseCriteria( void );
protected:
	[[= ks::reflect::Key{ .name = "model", .global = true } ]] [[= ks::reflect::As{ FIELD_MODELNAME } ]] string_t		m_ModelName;

	CNetworkVar( bool, m_bIsAutoaimTarget, [[= ks::reflect::Key{ .name = "is_autoaim_target" } ]] [[= ks::reflect::Net{} ]] );
private:
	friend class CAI_Senses;
	CBaseEntity	*m_pLink;// used for temporary link-list operations. 

public:
	// variables promoted from edict_t
	[[= ks::reflect::Key{ .name = "target" } ]] string_t	m_target;

	// virtual functions used by a few classes
	
	// creates an entity of a specified class, by name
	static CBaseEntity *Create( const char *szName, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner = nullptr );
	static CBaseEntity *CreateNoSpawn( const char *szName, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner = nullptr );

	// Collision group accessors
	int				GetCollisionGroup() const;
	void			SetCollisionGroup( int collisionGroup );
	void			CollisionRulesChanged();

	// Damage accessors
	int GetDamageType() const;
	int GetDamageType_Impl() const;
	float GetDamage();
	float GetDamage_Impl(){ return 0; }
	void SetDamage(float flDamage);
	void SetDamage_Impl(float flDamage){}

	Vector EyePosition( void );
	Vector EyePosition_Impl( void );			// position of eyes
	const QAngle & EyeAngles( void );
	const QAngle & EyeAngles_Impl( void );		// Direction of eyes in world space
	const QAngle & LocalEyeAngles( void );
	const QAngle & LocalEyeAngles_Impl( void );	// Direction of eyes
	 Vector	EarPosition( void );			// position of ears
	void	EyePositionZOnly( Vector *pPosition );	// position of eyes, ignoring X and Y

	float GetDistanceToEntity( const CBaseEntity *other ) const; // Distance between GetAbsOrigins.

	// Const versions are intentionally not virtual so you don't have to override two versions.  The const one just 
	// gets rid of const and then calls the virtual regular version.
	Vector	EyePosition( void ) const;			// position of eyes
	const QAngle &EyeAngles( void ) const;		// Direction of eyes in world space
	const QAngle &LocalEyeAngles( void ) const;	// Direction of eyes
	Vector	EarPosition( void ) const;			// position of ears

	Vector BodyTarget( const Vector &posSrc, bool bNoisy = true);
	Vector BodyTarget_Impl( const Vector &posSrc, bool bNoisy = true);		// position to shoot at
	 Vector	HeadTarget( const Vector &posSrc );
	void GetVectors(Vector* forward, Vector* right, Vector* up) const;
	void GetVectors_Impl(Vector* forward, Vector* right, Vector* up) const;

	 const Vector &GetViewOffset() const;
	 void SetViewOffset( const Vector &v );

	// NOTE: Setting the abs velocity in either space will cause a recomputation
	// in the other space, so setting the abs velocity will also set the local vel
	void			SetLocalVelocity( const Vector &vecVelocity );
	void			ApplyLocalVelocityImpulse( const Vector &vecImpulse );
	void			SetAbsVelocity( const Vector &vecVelocity );
	void			ApplyAbsVelocityImpulse( const Vector &vecImpulse );
	void			ApplyLocalAngularVelocityImpulse( const AngularImpulse &angImpulse );
	const Vector&	ScriptGetLocalAngularVelocity( void );
	void			ScriptSetLocalAngularVelocity( float pitchVel, float yawVel, float rollVel );

	const Vector&	GetAbsVelocity( ) const;

	// NOTE: Setting the abs velocity in either space will cause a recomputation
	// in the other space, so setting the abs velocity will also set the local vel
	void			SetLocalAngularVelocity( const QAngle &vecAngVelocity );

	// FIXME: While we're using (dPitch, dYaw, dRoll) as our local angular velocity
	// representation, we can't actually solve this problem
//	void			SetAbsAngularVelocity( const QAngle &vecAngVelocity );
//	const QAngle&	GetAbsAngularVelocity( ) const;


	Vector GetSmoothedVelocity( void );
	Vector GetSmoothedVelocity_Impl( void );

	// FIXME: Figure out what to do about this
	void GetVelocity(Vector *vVelocity, AngularImpulse *vAngVelocity = nullptr);
	void GetVelocity_Impl(Vector *vVelocity, AngularImpulse *vAngVelocity = nullptr);

	 float	GetFriction( void ) const;
	void			SetFriction( float flFriction );

	// Mechanism for overriding friction for a short duration
	void			OverrideFriction( float duration, float friction );

	bool FVisible( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = nullptr );
	bool FVisible_Impl( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = nullptr );
	bool FVisible( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = nullptr );
	bool FVisible_Impl( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = nullptr );

	 bool CanBeSeenBy( CAI_BaseNPC *pNPC ) { return true; } // allows entities to be 'invisible' to NPC senses.

	// This function returns a value that scales all damage done by this entity.
	// Use CDamageModifier to hook in damage modifiers on a guy.
	 float			GetAttackDamageScale( CBaseEntity *pVictim );
	// This returns a value that scales all damage done to this entity
	// Use CDamageModifier to hook in damage modifiers on a guy.
	 float			GetReceivedDamageScale( CBaseEntity *pAttacker );

 	void					SetCheckUntouch( bool check );
	bool					GetCheckUntouch() const;

	void					SetGroundEntity( CBaseEntity *ground );
	CBaseEntity				*GetGroundEntity( void );
	CBaseEntity				*GetGroundEntity( void ) const { return const_cast<CBaseEntity *>(this)->GetGroundEntity(); }
	 void			OnGroundChanged( CBaseEntity *oldGround, CBaseEntity *newGround ) {}

	// Gets the velocity we impart to a player standing on us
	void GetGroundVelocityToApply( Vector &vecGroundVel );
	void GetGroundVelocityToApply_Impl( Vector &vecGroundVel ){ vecGroundVel = vec3_origin; }


	bool PhysicsSplash( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed );
	bool PhysicsSplash_Impl( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed ){ return false; }
	void Splash();
	void Splash_Impl(){}

	void					ClearSolidFlags( void );	
	void					RemoveSolidFlags( int flags );
	void					AddSolidFlags( int flags );
	bool					IsSolidFlagSet( int flagMask ) const;
	void				 	SetSolidFlags( int flags );
	bool					IsSolid() const;
	
	void					SetModelName( string_t name );

	model_t					*GetModel( void );

	void					SetAIAddOn( string_t name );

	// These methods return a *world-aligned* box relative to the absorigin of the entity.
	// This is used for collision purposes and is *not* guaranteed
	// to surround the entire entity's visual representation
	// NOTE: It is illegal to ask for the world-aligned bounds for
	// SOLID_BSP objects
	const Vector&			WorldAlignMins( ) const;
	const Vector&			WorldAlignMaxs( ) const;

	// This defines collision bounds in OBB space
	void					SetCollisionBounds( const Vector& mins, const Vector &maxs );
	const Vector&			ScriptGetBoundingMins( void );
	const Vector&			ScriptGetBoundingMaxs( void );

	// NOTE: The world space center *may* move when the entity rotates.
	const Vector& WorldSpaceCenter( ) const;
	const Vector& WorldSpaceCenter_Impl( ) const;
 	const Vector&			WorldAlignSize( ) const;

	// Returns a radius of a sphere 
	// *centered at the world space center* bounding the collision representation 
	// of the entity. NOTE: The world space center *may* move when the entity rotates.
	float					BoundingRadius() const;
	bool					IsPointSized() const;

	// NOTE: Setting the abs origin or angles will cause the local origin + angles to be set also
	void					SetAbsOrigin( const Vector& origin );
	void					SetAbsAngles( const QAngle& angles );

	// Origin and angles in local space ( relative to parent )
	// NOTE: Setting the local origin or angles will cause the abs origin + angles to be set also
	void					SetLocalOrigin( const Vector& origin );

	void					SetLocalAngles( const QAngle& angles );

	void					SetElasticity( float flElasticity );
	float					GetElasticity( void ) const;

	void					SetShadowCastDistance( float flDistance );
	float					GetShadowCastDistance( void ) const;
	void					SetShadowCastDistance( float flDesiredDistance, float flDelay );

	float					GetLocalTime( void ) const;
	void					IncrementLocalTime( float flTimeDelta );
	float					GetMoveDoneTime( ) const;
	void					SetMoveDoneTime( float flTime );

	// Cell position
	void					SetCellBits( int cellbits = CELL_BASEENTITY_ORIGIN_CELL_BITS );
	void					UpdateCell();
	
	static void SendProxy_CellX( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID);
	static void SendProxy_CellY( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID);
	static void SendProxy_CellZ( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID);
	static void SendProxy_CellOrigin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
	static void SendProxy_CellOriginXY( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
	static void SendProxy_CellOriginZ( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
		
	// Used by the PAS filters to ask the entity where in world space the sounds it emits come from.
	// This is used right now because if you have something sitting on an incline, using our axis-aligned 
	// bounding boxes can return a position in solid space, so you won't hear sounds emitted by the object.
	// For now, we're hacking around it by moving the sound emission origin up on certain objects like vehicles.
	//
	// When OBBs get in, this can probably go away.
	 Vector			GetSoundEmissionOrigin() const;


	// Sets the local position from a transform
	void					SetLocalTransform( const matrix3x4_t &localTransform );

	// See CSoundEmitterSystem
	// Will return the sound guid. If negative, the guid is unknown (call may be successful or not). 0 if the sound was not emitted. Positive if the guid is valid.
	int						EmitSound( const char *soundname, float soundtime = 0.0f, float *duration = nullptr );  // Override for doing the general case of CPASAttenuationFilter filter( this ), and EmitSound( filter, entindex(), etc. );
	int						EmitSound( const char *soundname, HSOUNDSCRIPTHASH& handle, float soundtime = 0.0f, float *duration = nullptr );  // Override for doing the general case of CPASAttenuationFilter filter( this ), and EmitSound( filter, entindex(), etc. );
	void					ScriptEmitSound( const char *soundname );
	void					ScriptStopSound( const char *soundname );
	float					ScriptSoundDuration( const char *soundname, const char *actormodel );
	void					StopSound( const char *soundname );
	void					StopSound( const char *soundname, HSOUNDSCRIPTHASH& handle );
	void					GenderExpandString( char const *in, char *out, int maxlen );

	void					SetPendingTeamNum( int nTeamID );

	void UpdateLastMadeNoiseTime( const char* pszSoundName = nullptr );

	static float GetSoundDuration( const char *soundname, char const *actormodel );

	static bool	GetParametersForSound( const char *soundname, CSoundParameters &params, char const *actormodel );
	static bool	GetParametersForSound( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters &params, char const *actormodel );

	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, const Vector *pOrigin = nullptr, float soundtime = 0.0f, float *duration = nullptr );
	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, HSOUNDSCRIPTHASH& handle, const Vector *pOrigin = nullptr, float soundtime = 0.0f, float *duration = nullptr );
	static void StopSound( int iEntIndex, const char *soundname );
	static soundlevel_t LookupSoundLevel( const char *soundname );
	static soundlevel_t LookupSoundLevel( const char *soundname, HSOUNDSCRIPTHASH& handle );

	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params );
	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params, HSOUNDSCRIPTHASH& handle );

	static void StopSound( int iEntIndex, int iChannel, const char *pSample, bool bIsStoppingSpeakerSound = false );

	static void EmitAmbientSound( int entindex, const Vector& origin, const char *soundname, int flags = 0, float soundtime = 0.0f, float *duration = nullptr );

	// keep track of other-player audible sounds.
	CNetworkVar( float, m_flLastMadeNoiseTime, [[= ks::reflect::Net{ .bits = 32 } ]] );

	// These files need to be listed in scripts/game_sounds_manifest.txt
	static HSOUNDSCRIPTHASH PrecacheScriptSound( const char *soundname );
	static void PrefetchScriptSound( const char *soundname );
	void VScriptPrecacheScriptSound( const char *soundname );

	// For each client who appears to be a valid recipient, checks the client has disabled CC and if so, removes them from 
	//  the recipient list.
	static void RemoveRecipientsIfNotCloseCaptioning( CRecipientFilter& filter );
	static void EmitCloseCaption( IRecipientFilter& filter, int entindex, char const *token, std::vector< Vector >& soundorigins, float duration, bool warnifmissing = false );
	static void	EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM,
		const Vector *pOrigin = nullptr, const Vector *pDirection = nullptr, bool bUpdatePositions = true, float soundtime = 0.0f );

	static bool IsPrecacheAllowed();
	static void SetAllowPrecache( bool allow );

	static bool m_bAllowPrecache;

	static bool IsSimulatingOnAlternateTicks();

//	void Relink() {}

public:

	// VPHYSICS Integration -----------------------------------------------
	//
	// --------------------------------------------------------------------
	// UNDONE: Move to IEntityVPhysics? or VPhysicsProp() ?
	// Called after spawn, and in the case of self-managing objects, after load
	bool CreateVPhysics();
	bool CreateVPhysics_Impl();

	// Convenience routines to init the vphysics simulation for this object.
	// This creates a static object.  Something that behaves like world geometry - solid, but never moves
	IPhysicsObject *VPhysicsInitStatic( void );

	// This creates a normal vphysics simulated object - physics determines where it goes (gravity, friction, etc)
	// and the entity receives updates from vphysics.  SetAbsOrigin(), etc do not affect the object!
	IPhysicsObject *VPhysicsInitNormal( SolidType_t solidType, int nSolidFlags, bool createAsleep, solid_t *pSolid = nullptr );

	// This creates a vphysics object with a shadow controller that follows the AI
	// Move the object to where it should be and call UpdatePhysicsShadowToCurrentPosition()
	IPhysicsObject *VPhysicsInitShadow( bool allowPhysicsMovement, bool allowPhysicsRotation, solid_t *pSolid = nullptr );

	// Force a non-solid (ie. solid_trigger) physics object to collide with other entities.
	bool ForceVPhysicsCollide( CBaseEntity *pEntity );
	bool ForceVPhysicsCollide_Impl( CBaseEntity *pEntity ){ return false; }

private:
	// called by all vphysics inits
	bool			VPhysicsInitSetup();
public:

	void			VPhysicsSetObject( IPhysicsObject *pPhysics );
	// destroy and remove the physics object for this entity
	void VPhysicsDestroyObject( void );
	void VPhysicsDestroyObject_Impl( void );
	void			VPhysicsSwapObject( IPhysicsObject *pSwap );

	void VPhysicsUpdate( IPhysicsObject *pPhysics );
	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	void			VPhysicsUpdatePusher( IPhysicsObject *pPhysics );

	// react physically to damage (called from CBaseEntity::OnTakeDamage() by default)
	 int		VPhysicsTakeDamage( const CTakeDamageInfo &info );
	void VPhysicsShadowCollision( int index, gamevcollisionevent_t *pEvent );
	void VPhysicsShadowCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics );
	void VPhysicsShadowUpdate_Impl( IPhysicsObject *pPhysics ){}
	void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	void VPhysicsFriction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit );
	void VPhysicsFriction_Impl( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit );
	
	// update the shadow so it will coincide with the current AI position at some time
	// in the future (or 0 for now)
	void UpdatePhysicsShadowToCurrentPosition( float deltaTime );
	void UpdatePhysicsShadowToCurrentPosition_Impl( float deltaTime );
	int VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );
	int VPhysicsGetObjectList_Impl( IPhysicsObject **pList, int listMax );
	 bool	VPhysicsIsFlesh( void );
	// --------------------------------------------------------------------

public:
	// The player drives simulation of this entity
	void					SetPlayerSimulated( CBasePlayer *pOwner );
	void					UnsetPlayerSimulated( void );
	bool					IsPlayerSimulated( void ) const;
	CBasePlayer				*GetSimulatingPlayer( void );
	// FIXME: Make these private!
	void					PhysicsCheckForEntityUntouch( void );
 	bool					PhysicsRunThink( thinkmethods_t thinkMethod = THINK_FIRE_ALL_FUNCTIONS );
	bool					PhysicsRunSpecificThink( int nContextIndex, BASEPTR thinkFunc );
	bool					PhysicsTestEntityPosition( CBaseEntity **ppEntity = nullptr );
	void					PhysicsPushEntity( const Vector& push, trace_t *pTrace );
	bool					PhysicsCheckWater( void );
	void					PhysicsCheckWaterTransition( void );
	void					PhysicsStepRecheckGround();
	// Computes the water level + type
	void					UpdateWaterState();
	bool					IsEdictFree() const { return edict()->IsFree(); }
	 bool			CanPushEntity( CBaseEntity *other ) const {return true;}	//Any special reason we can't push this other thing while moving?

	// Callbacks for the physgun/cannon picking up an entity
	CBasePlayer * HasPhysicsAttacker( float dt );
	CBasePlayer * HasPhysicsAttacker_Impl( float dt ){ return nullptr; }

	// UNDONE: Make this data?
	unsigned int PhysicsSolidMaskForEntity( void ) const;
	unsigned int PhysicsSolidMaskForEntity_Impl( void ) const;

	// Computes the abs position of a point specified in local space
	void					ComputeAbsPosition( const Vector &vecLocalPosition, Vector *pAbsPosition );

	// Computes the abs position of a direction specified in local space
	void					ComputeAbsDirection( const Vector &vecLocalDirection, Vector *pAbsDirection );

	void					SetPredictionEligible( bool canpredict );

	// Precache model sounds + particles
	static void PrecacheModelComponents( int nModelIndex );
	static void PrecacheSoundHelper( const char *pName );

protected:
	// Invalidates the abs state of all children
	void					InvalidatePhysicsRecursive( int nChangeFlags );

	int						PhysicsClipVelocity (const Vector& in, const Vector& normal, Vector& out, float overbounce );
	void					PhysicsRelinkChildren( float dt );

	// Performs the collision resolution for fliers.
	void					PerformFlyCollisionResolution( trace_t &trace, Vector &move );
	void					ResolveFlyCollisionBounce( trace_t &trace, Vector &vecVelocity, float flMinTotalElasticity = 0.0f );
	void					ResolveFlyCollisionSlide( trace_t &trace, Vector &vecVelocity );
	void ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );
public: void ResolveFlyCollisionCustom_Impl( trace_t &trace, Vector &vecVelocity );
protected:

private:
	// Physics-related private methods
	void					PhysicsStep( void );
	void					PhysicsPusher( void );
	void					PhysicsNone( void );
	void					PhysicsNoclip( void );
	void					PhysicsStepRunTimestep( float timestep );
	void					PhysicsToss( void );
	void					PhysicsCustom( void );
	void					PerformPush( float movetime );

	// Simulation in local space of rigid children
	void					PhysicsRigidChild( void );

	// Computes the base velocity
	void					UpdateBaseVelocity( void );

	// Implement this if you use MOVETYPE_CUSTOM
	 void			PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity );

	void					PhysicsDispatchThink( BASEPTR thinkFunc );

	touchlink_t				*PhysicsMarkEntityAsTouched( CBaseEntity *other );
	void					PhysicsTouch( CBaseEntity *pentOther );
	void					PhysicsStartTouch( CBaseEntity *pentOther );

	CBaseEntity				*PhysicsPushMove( float movetime );
	CBaseEntity				*PhysicsPushRotate( float movetime );

	CBaseEntity				*PhysicsCheckRotateMove( rotatingpushmove_t &rotPushmove, CBaseEntity **pPusherList, int pusherListCount );
	CBaseEntity				*PhysicsCheckPushMove( const Vector& move, CBaseEntity **pPusherList, int pusherListCount );
	int						PhysicsTryMove( float flTime, trace_t *steptrace );

	void					PhysicsCheckVelocity( void );
	void					PhysicsAddHalfGravity( float timestep );
	void					PhysicsAddGravityMove( Vector &move );

	void					CalcAbsoluteVelocity();
	void					CalcAbsoluteAngularVelocity();

	// Checks a sweep without actually performing the move
	void					PhysicsCheckSweep( const Vector& vecAbsStart, const Vector &vecAbsDelta, trace_t *pTrace );

	// Computes new angles based on the angular velocity
	void					SimulateAngles( float flFrameTime );

	void					CheckStepSimulationChanged();
	// Run regular think and latch off angle/origin changes so we can interpolate them on the server to fake simulation
	void					StepSimulationThink( float dt );

	// Compute network origin
private:
	void					ComputeStepSimulationNetwork( StepSimulationData *step );

public:
	bool					UseStepSimulationNetworkOrigin( const Vector **out_v, int cell[3] = nullptr ); // cell should be a 3 int array
	bool					UseStepSimulationNetworkAngles( const QAngle **out_a );

public:
	// Add a discontinuity to a step
	bool					AddStepDiscontinuity( float flTime, const Vector &vecOrigin, const QAngle &vecAngles );
	int						GetFirstThinkTick();	// get first tick thinking on any context
	void					RebaseThinkTicks( bool bMakeDeltas );	// Rebase all the think times as deltas or from deltas to current ticks
private:
	// origin and angles to use in step calculations
	public:
	Vector GetStepOrigin( void ) const;
	Vector GetStepOrigin_Impl( void ) const;
private:
	public:
	QAngle GetStepAngles( void ) const;
	QAngle GetStepAngles_Impl( void ) const;
private:
	
	// These set entity flags (EFL_*) to help optimize queries
	void					CheckHasThinkFunction( bool isThinkingHint = false );
	void					CheckHasGamePhysicsSimulation();
	bool					WillThink();
	bool					WillSimulateGamePhysics();

	friend class CPushBlockerEnum;

	// Sets/Gets the next think based on context index
	void SetNextThink( int nContextIndex, float thinkTime );
	void SetLastThink( int nContextIndex, float thinkTime );
	float GetNextThink( int nContextIndex ) const;
	int	GetNextThinkTick( int nContextIndex ) const;

	// Shot statistics

protected:
	// Handle shot entering water
	bool HandleShotImpactingWater( const FireBulletsInfo_t &info, const Vector &vecEnd, ITraceFilter *pTraceFilter, Vector *pVecTracerDest );

	// Handle shot entering water
	void HandleShotImpactingGlass( const FireBulletsInfo_t &info, const trace_t &tr, const Vector &vecDir, ITraceFilter *pTraceFilter );

	// Should we draw bubbles underwater?
	bool ShouldDrawUnderwaterBulletBubbles();

	// Computes the tracer start position
	void ComputeTracerStartPosition( const Vector &vecShotSrc, Vector *pVecTracerStart );

	// Computes the tracer start position
	void CreateBubbleTrailTracer( const Vector &vecShotSrc, const Vector &vecShotEnd, const Vector &vecShotDir );

	 bool ShouldDrawWaterImpacts() { return true; }

private:
	// Changes shadow cast distance over time
	void ShadowCastDistThink( );

public:
	// was pev->renderfx
	CNetworkVar( unsigned char, m_nRenderFX, [[= ks::reflect::Key{ .name = "renderfx" } ]] [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	// was pev->rendermode
	CNetworkVar( unsigned char, m_nRenderMode, [[= ks::reflect::Key{ .name = "rendermode" } ]] [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( short, m_nModelIndex, [[= ks::reflect::Key{ .name = "modelindex", .global = true } ]] [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	// was pev->rendercolor
	CNetworkColor32( m_clrRender, [[= ks::reflect::Key{ .name = "rendercolor" } ]] [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] );

protected:
	// Which frame did I simulate?

protected:
	// was pev->flags

	CNetworkVar( string_t, m_iName, [[= ks::reflect::Net{} ]] ); // name used to identify this entity

	// Damage modifiers

	EHANDLE m_pParent;  // for movement hierarchy
	byte	m_nTransmitStateOwnedCounter;
	CNetworkVar( unsigned char,  m_iParentAttachment, [[= ks::reflect::Net{ .bits = NUM_PARENTATTACHMENT_BITS, .flags = SPROP_UNSIGNED } ]] ); // 0 if we're relative to the parent's absorigin and absangles.

	// Our immediate parent in the movement hierarchy.
	// FIXME: clarify m_pParent vs. m_pMoveParent
	CNetworkHandle( CBaseEntity, m_hMoveParent, [[= ks::reflect::Net{ .wire = "moveparent" } ]] );
	// cached child list
	EHANDLE m_hMoveChild;
	// generated from m_pMoveParent
	EHANDLE m_hMovePeer;	

	friend class CCollisionProperty;
	friend class CServerNetworkProperty;
	CNetworkVarEmbedded( CCollisionProperty, m_Collision, [[= ks::reflect::Net{} ]] );

	CNetworkHandle( CBaseEntity, m_hOwnerEntity, [[= ks::reflect::Net{} ]] );	// only used to point to an edict it won't collide with

	CNetworkVar( int, m_CollisionGroup, [[= ks::reflect::Key{ .name = "CollisionGroup" } ]] [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] );		// used to cull collision tests

	// Sets water type + level for physics objects
	unsigned char	m_nWaterTouch;
	unsigned char	m_nSlimeTouch;
	float			m_flNavIgnoreUntilTime;

	
	// Velocity of the thing we're standing on (world space)

	// Global velocity

	// Local angular velocity

	// Global angular velocity
//	QAngle			m_vecAbsAngVelocity;

	// local coordinate frame of entity

	// was pev->friction
	CNetworkVar( float, m_flElasticity, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_COORD } ]] );
	// was pev->ltime
	[[= ks::reflect::Key{ .name = "ltime" } ]] float			m_flLocalTime;
	// local time at the beginning of this frame
	float			m_flVPhysicsUpdateLocalTime;
	// local time the movement has ended
	float			m_flMoveDoneTime;

	// A counter to help quickly build a list of potentially pushed objects for physics
	int				m_nPushEnumCount;

	
	// Physics state
	EHANDLE			m_pBlocker;

	//Adrian
	CNetworkVar( unsigned char, m_iTextureFrameIndex, [[= ks::reflect::Key{ .name = "texframeindex" } ]] [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	
	CNetworkVar( bool, m_bSimulatedEveryTick, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bAnimatedEveryTick, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bAlternateSorting, [[= ks::reflect::Net{} ]] );

	CNetworkVar( unsigned char, m_nMinCPULevel, [[= ks::reflect::Key{ .name = "mincpulevel" } ]] [[= ks::reflect::Net{ .bits = CPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_nMaxCPULevel, [[= ks::reflect::Key{ .name = "maxcpulevel" } ]] [[= ks::reflect::Net{ .bits = CPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_nMinGPULevel, [[= ks::reflect::Key{ .name = "mingpulevel" } ]] [[= ks::reflect::Net{ .bits = GPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_nMaxGPULevel, [[= ks::reflect::Key{ .name = "maxgpulevel" } ]] [[= ks::reflect::Net{ .bits = GPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED } ]] );

public:
	// was pev->speed
	[[= ks::reflect::Key{ .name = "globalname", .global = true } ]] string_t m_iGlobalname; // identifier for carrying entity across level transitions
	[[= ks::reflect::Key{ .name = "parentname" } ]] string_t m_iParent;	// the name of the entities parent; linked into m_pParent during Activate()

	// This comes from the "id" key/value that Hammer adds to entities.
	// It is used by Foundry to match up live (engine) entities with Hammer entities.
	[[= ks::reflect::Key{ .name = "hammerid" } ]] int		m_iHammerID; // Hammer unique edit id number
	[[= ks::reflect::Key{ .name = "speed" } ]] float		m_flSpeed;
	CNetworkVarForDerived( int, m_iMaxHealth, [[= ks::reflect::Key{ .name = "max_health" } ]] ); // CBaseEntity doesn't care about changes to this variable, but there are derived classes that do.
	CNetworkVarForDerived( int, m_iHealth, [[= ks::reflect::Key{ .name = "health" } ]] );
	// Damage filtering
	[[= ks::reflect::Key{ .name = "damagefilter" } ]] string_t	m_iszDamageFilterName;	// The name of the entity to use as our damage filter.
	EHANDLE		m_hDamageFilter;		// The entity that controls who can damage us.

	void (CBaseEntity ::*m_pfnTouch)( CBaseEntity *pOther );
	void (CBaseEntity ::*m_pfnUse)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void (CBaseEntity ::*m_pfnBlocked)( CBaseEntity *pOther );

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
	static uint GetOriginPrefetchOffset() { return __builtin_offsetof(CBaseEntity,m_vecAbsOrigin); }
#pragma GCC diagnostic pop
	CNetworkVarForDerived( bool, m_bClientSideRagdoll );
	CNetworkVarForDerived( char, m_lifeState );
	CNetworkVarForDerived( char , m_takedamage );

private:
	CNetworkVar( bool, m_bIsPlayerSimulated );
	// Player who is driving my simulation
	CNetworkHandle( CBasePlayer, m_hPlayerSimulationOwner );
	// User outputs. Fired when the "FireInputX" input is triggered.
	[[= ks::reflect::Key{ .name = "OnUser1" } ]] COutputEvent m_OnUser1;
	[[= ks::reflect::Key{ .name = "OnUser2" } ]] COutputEvent m_OnUser2;
	[[= ks::reflect::Key{ .name = "OnUser3" } ]] COutputEvent m_OnUser3;
	[[= ks::reflect::Key{ .name = "OnUser4" } ]] COutputEvent m_OnUser4;

	COutputEvent m_OnKilled;

	// We cache the cell width for convenience
	int m_cellwidth;

	CNetworkVar( int, m_cellbits, [[= ks::reflect::Net{ .bits = MINIMUM_BITS_NEEDED( 32 ), .flags = SPROP_UNSIGNED, .priority = SENDPROP_CELL_INFO_PRIORITY } ]] );

	// Cell of the current origin
//	CNetworkArray( int, m_cellXY, 2 );
	CNetworkVar( int, m_cellX, [[= ks::reflect::Net{ .bits = CELL_COUNT_BITS( CELL_BASEENTITY_ORIGIN_CELL_BITS ), .flags = SPROP_UNSIGNED, .priority = SENDPROP_CELL_INFO_PRIORITY } ]] [[= ks::reflect::Proxy<CBaseEntity::SendProxy_CellX, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( int, m_cellY, [[= ks::reflect::Net{ .bits = CELL_COUNT_BITS( CELL_BASEENTITY_ORIGIN_CELL_BITS ), .flags = SPROP_UNSIGNED, .priority = SENDPROP_CELL_INFO_PRIORITY } ]] [[= ks::reflect::Proxy<CBaseEntity::SendProxy_CellY, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( int, m_cellZ, [[= ks::reflect::Net{ .bits = CELL_COUNT_BITS( CELL_BASEENTITY_ORIGIN_CELL_BITS ), .flags = SPROP_UNSIGNED, .priority = SENDPROP_CELL_INFO_PRIORITY } ]] [[= ks::reflect::Proxy<CBaseEntity::SendProxy_CellZ, ks::reflect::WireSide::Send>{} ]] );

	// The class-level reflection annotations sit here, after every proxy they name.
	[[= ks::reflect::NetTable{ .name = "DT_AnimTimeMustBeFirst", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_BaseEntity", .base = false } ]]
	[[= ks::reflect::SubTable<"AnimTimeMustBeFirst", &DT_AnimTimeMustBeFirst::g_SendTable,
	SendProxy_ClientSideAnimation, true>{} ]]
	[[= ks::reflect::KeyFrom<"m_vecAngVelocity", ks::reflect::Key{ .name = "avelocity" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_flGravity", ks::reflect::Key{ .name = "gravity" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_MoveType", ks::reflect::Key{ .name = "MoveType" } >{} ]]
	[[= ks::reflect::From<"m_MoveType", ks::reflect::Net{ .bits = MOVETYPE_MAX_BITS, .flags = SPROP_UNSIGNED, .wire = "movetype" } >{} ]]
	[[= ks::reflect::From<"m_MoveCollide", ks::reflect::Net{ .bits = MOVECOLLIDE_MAX_BITS, .flags = SPROP_UNSIGNED, .wire = "movecollide" } >{} ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = CELL_BASEENTITY_ORIGIN_CELL_BITS, .flags = SENDPROP_VECORIGIN_FLAGS, .enc = ks::reflect::WireEnc::Vector }, &CBaseEntity::SendProxy_CellOrigin>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = SENDPROP_ANGROTATION_DEFAULT_BITS, .flags = SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::QAngles }, &SendProxy_Angles>{} ]]
	[[= ks::reflect::KeyFrom<"m_nWaterLevel", ks::reflect::Key{ .name = "waterlevel" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_vecBaseVelocity", ks::reflect::Key{ .name = "basevelocity" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_flFriction", ks::reflect::Key{ .name = "friction" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_vecVelocity", ks::reflect::Key{ .name = "velocity" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_vecViewOffset", ks::reflect::Key{ .name = "view_ofs" } >{} ]]
	static constexpr bool ks_class = true;

	// was pev->view_ofs ( FIXME:  Move somewhere up the hierarch, CBaseAnimating, etc. )

	CBaseEntity	**		m_ppListByClassHead;	// head slot of this classname's list in g_EntsByClassname
	CBaseEntity	*		m_pPrevByClass;
	CBaseEntity	*		m_pNextByClass;
	// So it can get at the physics methods
	friend class CCollisionEvent;

	// Team handling
	[[= ks::reflect::Key{ .name = "TeamNum", .input = true } ]] int			m_iInitialTeamNum;		// Team number of this entity's team read from file
	CNetworkVar( int, m_iTeamNum, [[= ks::reflect::Key{ .name = "teamnumber" } ]] [[= ks::reflect::Net{ .bits = TEAMNUM_NUM_BITS } ]] );				// Team number of this entity's team. 
	CNetworkVar( int, m_iPendingTeamNum, [[= ks::reflect::Key{ .name = "pendingteamnumber" } ]] [[= ks::reflect::Net{ .bits = TEAMNUM_NUM_BITS } ]] );				// Team number of this entity's pending team. 

protected:
	// FIXME: Make this private! Still too many references to do so...
	CNetworkVar( int, m_spawnflags, [[= ks::reflect::Key{ .name = "spawnflags" } ]] );
	[[= ks::reflect::Key{ .name = "addon" } ]] string_t		m_AIAddOn;
	// was pev->gravity;
	CNetworkHandle( CBaseEntity, m_hEffectEntity, [[= ks::reflect::Net{} ]] );	// Fire/Dissolve entity.
	CNetworkVar( float, m_fadeMinDist, [[= ks::reflect::Key{ .name = "fademindist", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Point at which fading is absolute
	CNetworkVar( float, m_fadeMaxDist, [[= ks::reflect::Key{ .name = "fademaxdist", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Point at which fading is inactive
	CNetworkVar( float, m_flFadeScale, [[= ks::reflect::Key{ .name = "fadescale" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Scale applied to min / max
	CNetworkVar( float, m_flShadowCastDistance, [[= ks::reflect::Key{ .name = "shadowcastdist" } ]] [[= ks::reflect::Net{ .bits = 12, .flags = SPROP_UNSIGNED } ]] );
	float		m_flDesiredShadowCastDistance;

// Methods shared by client and server
public:
	void							SetSize( const Vector &vecMin, const Vector &vecMax ); // UTIL_SetSize( this, mins, maxs );
	static int						PrecacheModel( const char *name, bool bPreload = true ); 
	static bool						PrecacheSound( const char *name );
	static void						PrefetchSound( const char *name );
	void							Remove( ); // UTIL_Remove( this );

	void							SetNetworkQuantizeOriginAngAngles( bool bQuantize );

	// Default implementation, assumes SPROP_COORD precision and default CBaseEntity SendPropQAngles!!!
	void							NetworkQuantize( Vector &org, QAngle &angles );

	bool							ShouldLagCompensate() const;

private:

	// This is a random seed used by the networking code to allow client - side prediction code
	//  randon number generators to spit out the same random numbers on both sides for a particular
	//  usercmd input.
	static int						m_nPredictionRandomSeed;
	static int						m_nPredictionRandomSeedServer;
	static CBasePlayer				*m_pPredictionPlayer;

	// FIXME: Make hierarchy a member of CBaseEntity
	// or a contained private class...
	friend void UnlinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
	friend void LinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
	friend void ClearParent( CBaseEntity *pEntity );
	friend void UnlinkAllChildren( CBaseEntity *pParent );
	friend void UnlinkFromParent( CBaseEntity *pRemove );
	friend void TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent );

	bool m_bNetworkQuantizeOriginAndAngles;
	[[= ks::reflect::Key{ .name = "LagCompensate" } ]] bool m_bLagCompensate; // Special flag for certain l4d2 props to use

	bool m_bForcePurgeFixedupStrings; // For template entites so we don't leak strings.
	
public:
	// Accessors for above
	static int						GetPredictionRandomSeed( bool bUseUnSyncedServerPlatTime = false );
	static void						SetPredictionRandomSeed( const CUserCmd *cmd );
	static CBasePlayer				*GetPredictionPlayer( void );
	static void						SetPredictionPlayer( CBasePlayer *player );

	// For debugging shared code
	static bool						IsServer( void )
	{
		return true;
	}

	static bool						IsClient( void )
	{
		return false;
	}

	static char const				*GetDLLType( void )
	{
		return "server";
	}
	
	// Used to access m_vecAbsOrigin during restore when it's unsafe to call GetAbsOrigin.
	friend class CPlayerRestoreHelper;
	
	static bool s_bAbsQueriesValid;

	CGlobalEvent	*m_pEvent;
	// Debugging / devolopment fields
	int				m_debugOverlays;	// For debug only (bitfields)
	TimedOverlay_t*	m_pTimedOverlay;	// For debug only

	// VSCRIPT
	HSCRIPT GetScriptInstance();
	bool ValidateScriptScope();
	void RunVScripts();
	void RunVScripts_Impl();
	bool CallScriptFunction( const char *pFunctionName, ScriptVariant_t *pFunctionReturn );
	void ConnectOutputToScript( const char *pszOutput, const char *pszScriptFunc );
	void DisconnectOutputFromScript( const char *pszOutput, const char *pszScriptFunc );
	void ScriptThink( );
	const char *GetScriptId();
	HSCRIPT GetScriptScope();
	void RunPrecacheScripts( void );
	void RunOnPostSpawnScripts( void );

	HSCRIPT ScriptGetMoveParent( void );
	HSCRIPT ScriptGetRootMoveParent();
	HSCRIPT ScriptFirstMoveChild( void );
	HSCRIPT ScriptNextMovePeer( void );

	const Vector &ScriptEyePosition( void ) { static Vector vec; vec = EyePosition(); return vec;}
	void ScriptSetAngles( float fPitch, float fYaw, float fRoll ) {QAngle angles(fPitch,fYaw,fRoll); Teleport(nullptr, &angles, nullptr);}
	const Vector &ScriptGetAngles( void ) { static Vector vec; QAngle qa = GetAbsAngles(); vec.x = qa.x; vec.y = qa.y; vec.z = qa.z; return vec;}
	
	void ScriptSetSize( const Vector &mins, const Vector &maxs ) { UTIL_SetSize( this, mins, maxs ); }
	void ScriptUtilRemove( void ) { UTIL_Remove( this ); }
	void ScriptSetOwner( HSCRIPT hEntity ) { SetOwnerEntity( ToEnt( hEntity ) ); }
	void ScriptSetOrigin( const Vector &v ) { Teleport( &v, nullptr, nullptr ); }
	void ScriptSetForward( const Vector &v ) { QAngle angles; VectorAngles( v, angles ); Teleport( nullptr, &angles, nullptr ); }
	const Vector &ScriptGetForward( void ) { static Vector vecForward; GetVectors( &vecForward, nullptr, nullptr ); return vecForward; }
	const Vector &ScriptGetLeft( void ) { static Vector vecLeft; GetVectors( nullptr, &vecLeft, nullptr ); return vecLeft; }
	const Vector &ScriptGetUp( void ) { static Vector vecUp; GetVectors( nullptr, nullptr, &vecUp ); return vecUp; }

	HSCRIPT ScriptGetModelKeyValues( void );

	void ScriptPrecacheModel( const char *name );
	void ScriptPrecacheScriptSound( const char *name );

	[[= ks::reflect::Key{ .name = "vscripts" } ]] string_t		m_iszVScripts;
	[[= ks::reflect::Key{ .name = "thinkfunction" } ]] string_t		m_iszScriptThinkFunction;
	CScriptScope	m_ScriptScope;
	HSCRIPT			m_hScriptInstance;
	string_t		m_iszScriptId;
	CScriptKeyValues *m_pScriptModelKeyValues;

	// Call this when hierarchy is not completely set up (such as during Restore) to throw asserts
	// when people call GetAbsAnything. 
	static inline void SetAbsQueriesValid( bool bValid )
	{
		s_bAbsQueriesValid = bValid;
	}
	
	static inline bool IsAbsQueriesValid()
	{
		return s_bAbsQueriesValid;
	}

public:

public:
	void	ClearSpotRule( int nRuleFlags );
	void	SetSpotRules( int nRuleFlags );
	bool	CanBeSpotted( void );

	bool	IsSpotted( void );
	void	SetIsSpotted( bool bSpotted );
	void	ClearSpottedBy( void );
	void	SetIsSpottedBy( int nPlayerEntIndex );
	int		GetSpotRules( void );

protected:
	// SpottingRules_T flags for limiting conditions under
	// which an entity is spotted
	int		m_nSpotRules;
	CNetworkVar( bool, m_bSpotted, [[= ks::reflect::Net{} ]] );

	CNetworkArray( uint32, m_bSpottedByMask, kNumSpottedByMask, [[= ks::reflect::Net{ .flags = SPROP_UNSIGNED } ]] );
	float	m_fLastSpotCheck;
};

// Send tables exposed in this module.
EXTERN_SEND_TABLE(DT_Edict);
EXTERN_SEND_TABLE(DT_BaseEntity);

// Ugly technique to override base member functions
// Normally it's illegal to cast a pointer to a member function of a derived class to a pointer to a 
// member function of a base class.  static_cast is a sleezy way around that problem.

#define SetBlocked( a ) m_pfnBlocked = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)

// handling entity/edict transforms
inline CBaseEntity *GetContainingEntity( edict_t *pent )
{
	if ( pent && pent->GetUnknown() )
	{
		return EntityCast<CBaseEntity>( pent->GetUnknown()->GetBaseEntity() );
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Pauses or resumes entity i/o events. When paused, no outputs will
//			fire unless Debug_SetSteps is called with a nonzero step value.
// Input  : bPause - true to pause, false to resume.
//-----------------------------------------------------------------------------
void CBaseEntity::Debug_Pause(bool bPause)
{
	CBaseEntity::m_bDebugPause = bPause;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if entity i/o is paused, false if not.
//-----------------------------------------------------------------------------
bool CBaseEntity::Debug_IsPaused(void)
{
	return(CBaseEntity::m_bDebugPause);
}

//-----------------------------------------------------------------------------
// Purpose: Decrements the debug step counter. Used when the entity i/o system
//			is in single step mode, this is called every time an output is fired.
// Output : Returns true on to continue firing outputs, false to stop.
//-----------------------------------------------------------------------------
bool CBaseEntity::Debug_Step(void)
{
	if (CBaseEntity::m_nDebugSteps > 0)
	{
		CBaseEntity::m_nDebugSteps--;
	}
	return(CBaseEntity::m_nDebugSteps > 0);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the number of entity outputs to allow to fire before pausing
//			the entity i/o system.
// Input  : nSteps - Number of steps to execute.
//-----------------------------------------------------------------------------
void CBaseEntity::Debug_SetSteps(int nSteps)
{
	CBaseEntity::m_nDebugSteps = nSteps;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if we should allow outputs to be fired, false if not.
//-----------------------------------------------------------------------------
bool CBaseEntity::Debug_ShouldStep(void)
{
	return(!CBaseEntity::m_bDebugPause || CBaseEntity::m_nDebugSteps > 0);
}

template <typename T>
inline bool CBaseEntity::Downcast( string_t iszClass, T **ppResult ) 
{ 
	if ( IDENT_STRINGS( iszClass, m_iClassname ) )
	{
		*ppResult = (T *)this;
		return true;
	}
	*ppResult = nullptr;
	return false;
}

//-----------------------------------------------------------------------------
// Network state optimization
//-----------------------------------------------------------------------------
inline CBaseCombatCharacter *ToBaseCombatCharacter( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return nullptr;

	return pEntity->MyCombatCharacterPointer();
}

/*
// FIXME: While we're using (dPitch, dYaw, dRoll) as our local angular velocity
// representation, we can't actually solve this problem
const QAngle &CBaseEntity::GetAbsAngularVelocity( ) const
{
	if (IsEFlagSet(EFL_DIRTY_ABSANGVELOCITY))
	{
		const_cast<CBaseEntity*>(this)->CalcAbsoluteAngularVelocity();
	}

	return m_vecAbsAngVelocity;
}
*/

//-----------------------------------------------------------------------------
// An inline version the game code can use
//-----------------------------------------------------------------------------

		 	 			 

	

//-----------------------------------------------------------------------------
// AddOn related methods
//-----------------------------------------------------------------------------

const ResponseContext_t	*CBaseEntity::GetContextData( int index ) const
{
	return &m_ResponseContexts[index];
}

// Ugly technique to override base member functions
// Normally it's illegal to cast a pointer to a member function of a derived class to a pointer to a 
// member function of a base class.  static_cast is a sleezy way around that problem.

inline bool FClassnameIs(CBaseEntity *pEntity, const char *szClassname)
{
	contract_assert( pEntity );
	return pEntity && pEntity->ClassMatches(szClassname); 
}

class CPointEntity : public CBaseEntity
{
public:
	DECLARE_CLASS( CPointEntity, CBaseEntity );

	void	Spawn_Impl( void );
	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
private:
};

// Has a position + size
class CServerOnlyEntity : public CBaseEntity
{
	DECLARE_CLASS( CServerOnlyEntity, CBaseEntity );
public:
	CServerOnlyEntity() : CBaseEntity( true ) {}
	
	int ObjectCaps_Impl( void ) { return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION); }
};

// Has only a position, no size
class CServerOnlyPointEntity : public CServerOnlyEntity
{
	DECLARE_CLASS( CServerOnlyPointEntity, CServerOnlyEntity );

public:
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
};

// Has no position or size
class CLogicalEntity : public CServerOnlyEntity
{
	DECLARE_CLASS( CLogicalEntity, CServerOnlyEntity );

public:
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
};

class CAbsQueryScopeGuard
{
public:
	CAbsQueryScopeGuard( bool state );
	~CAbsQueryScopeGuard()
	{
		CBaseEntity::SetAbsQueriesValid( m_bSavedState );
	}
private:
	bool	m_bSavedState;
};

// Network proxy functions

void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
void SendProxy_OriginXY( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
void SendProxy_OriginZ( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

extern bool EntityNamesMatchCStrings( const char *pszQuery, const char *pszNameToMatch );

[[gnu::always_inline]] inline bool EntityNamesMatch( const char *pszQuery, string_t nameToMatch )
{
	if ( nameToMatch == NULL_STRING )
		return (*pszQuery == 0 || *pszQuery == '*');

	const char *pszNameToMatch = STRING(nameToMatch);

	// If the pointers are identical, we're identical
	if ( pszNameToMatch == pszQuery )
		return true;

	return EntityNamesMatchCStrings( pszQuery, pszNameToMatch );
}

#undef MANUALMODE_GETSET_PROP
#undef MANUALMODE_GETSET_EHANDLE
#undef DEFINE_THINKFUNC
#undef DEFINE_ENTITYFUNC
#undef DEFINE_USEFUNC
#undef CREATE_PREDICTED_ENTITY
#undef SetBlocked

class CBaseEntity;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// The rotor wash shooter. It emits gibs when pushed by a rotor wash
//-----------------------------------------------------------------------------
class IRotorWashShooter
{
public:
	virtual CBaseEntity *DoWashPush( float flWashStartTime, const Vector &vecForce ) = 0;
};

//-----------------------------------------------------------------------------
// Gets at the interface if the entity supports it
//-----------------------------------------------------------------------------
IRotorWashShooter *GetRotorWashShooter( CBaseEntity *pEntity );

class CEnvQuadraticBeam : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_QuadraticBeam" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvQuadraticBeam, CPointEntity );

public:
	void Spawn_Impl();
	void SetSpline( const Vector &control, const Vector &target );
	void SetScrollRate( float rate )
	{
		m_scrollRate = rate;
	}

	void SetWidth( float width )
	{
		m_flWidth = width;
	}

private:
	CNetworkVector( m_targetPosition, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_controlPosition, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_scrollRate, [[= ks::reflect::Net{ .bits = 8, .low = -4, .high = 4 } ]] );
	CNetworkVar( float, m_flWidth, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	DECLARE_SERVERCLASS();
};
CEnvQuadraticBeam *CreateQuadraticBeam( const char *pSpriteName, const Vector &start, const Vector &control, const Vector &end, float width, CBaseEntity *pOwner );
