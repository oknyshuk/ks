// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <time.h>
#include <math.h>
#include "basehandle.h"
#include "engine/ivmodelinfo.h"
#include "ispsharedmemory.h"
#include "edict.h"
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "smoke_fog_overlay.h"
#include "icliententity.h"
#include "smoke_trail.h"
#include "vcollide_parse.h"
#include "irecipientfilter.h"
#include "soundflags.h"
#include "string_t.h"
#include "igameevents.h"
#include "view_shared.h"
#include "cdll_int.h"
#include "iclient.h"
#include "networkvar.h"
#include "vphysics/player_controller.h"
#include "vphysics_interface.h"
#include "baseplayer.h"
#include "predictable_entity.h"
#include "engine/SndInfo.h"
#include "inputsystem/ButtonCode.h"
#include "eventlist.h"
#include "fire_smoke.h"
#include "iclientrenderable.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "networkstringtabledefs.h"
#include "ihandleentity.h"
#include "ispatialpartition.h"
#include "te_effect_dispatch.h"
module ks.client:sharedvar;

import std;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import :util;
import :particlemgr;
import ks.tier1.convar;
import :baseentity;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import ks.tier1.intrusive_ptr;
import :iinput;
import ks.tier1.envelope;
import ks.tier1.icvar;
import ks.core.platform;
import ks.core.types;
import ks.math.vector2d;
import ks.tier1.bitbuf;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
import ks.tier1.tracked_ref;
import :baseanimatedtextureproxy;
import ks.game.shareddefs;
import ks.game.entitycore;
import :game_controls.baseviewport;
import ks.game.sun_shared;
import ks.math.vplane;
import ks.tier1.createinterface;
import ks.tier1.intrusive_list;
import ks.tier1.map_ops;
import ks.tier1.fixed_pool;

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

	inline EHANDLE Client_IntToEHandle( int iIn )
	{
		if ( iIn == INVALID_NETWORKED_EHANDLE_VALUE )
		{
			return INVALID_EHANDLE;
		}
		else
		{
			int iEntity = iIn & ((1 << MAX_EDICT_BITS) - 1);
			int iSerialNum = iIn >> MAX_EDICT_BITS;

			EHANDLE hOut;
			hOut.Init( iEntity, iSerialNum );
			return hOut;
		}
	}

template< class Type, class Changer >
class CSharedVarBase
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

	void Initialize( char *pchName, int iEntNum )
	{
		contract_assert( !m_pSharedMemory && !m_pValue );

		m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		m_pSharedMemory->Init( sizeof( Type ) );
		m_pValue = (Type*)m_pSharedMemory->Base();
	}

	operator const Type&() const 
	{
		return *m_pValue; 
	}

	const Type& Get() const 
	{
		return *m_pValue; 
	}

	const Type* operator->() const 
	{
		return m_pValue; 
	}

private:
public:
	ISPSharedMemory		*m_pSharedMemory;
	Type				*m_pValue;
};

// Shared color wrapper.
template< class Type, class Changer >
class CSharedColor32Base : public CSharedVarBase< Type, Changer >
{
public:

	inline byte GetR() const { return CSharedColor32Base<Type,Changer>::m_pValue->r; }
	inline byte GetG() const { return CSharedColor32Base<Type,Changer>::m_pValue->g; }
	inline byte GetB() const { return CSharedColor32Base<Type,Changer>::m_pValue->b; }
	inline byte GetA() const { return CSharedColor32Base<Type,Changer>::m_pValue->a; }

};

// Shared vector wrapper.
template< class Type, class Changer >
class CSharedVectorBase : public CSharedVarBase< Type, Changer >
{
public:

	inline float GetX() const { return CSharedVectorBase<Type,Changer>::m_pValue->x; }
	inline float GetY() const { return CSharedVectorBase<Type,Changer>::m_pValue->y; }
	inline float GetZ() const { return CSharedVectorBase<Type,Changer>::m_pValue->z; }
	inline const float& operator[]( int i ) const { return (*CSharedVectorBase<Type,Changer>::m_pValue)[i]; }

	bool operator==( const Type &val ) const 
	{ 
		return (*CSharedVectorBase<Type,Changer>::m_pValue) == (Type)val; 
	}

	bool operator!=( const Type &val ) const 
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) != (Type)val; 
	}

	const Type operator+( const Type &val ) const 
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return (*CSharedVectorBase<Type,Changer>::m_pValue) - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator*( float val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) / val; 
	}

};

// Shared quaternion wrapper.
template< class Type, class Changer >
class CSharedQuaternionBase : public CSharedVarBase< Type, Changer >
{
public:

	inline float GetX() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->x; }
	inline float GetY() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->y; }
	inline float GetZ() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->z; }
	inline float GetW() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->w; }
	inline const float& operator[]( int i ) const { return (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i]; }

	bool operator==( const Type &val ) const 
	{ 
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) == (Type)val; 
	}

	bool operator!=( const Type &val ) const 
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) != (Type)val; 
	}

	const Type operator+( const Type &val ) const 
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator*( float val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) / val; 
	}

};

// Prevent Shared Vars from using handle templates without the handle wrapper
template< class Type, class Changer >
class CSharedVarBase< CHandle<Type>, Changer >
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

protected:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CHandle<Type>		*m_pValue;
};

// Prevent Shared Vars from using handle templates without the handle wrapper
template< class Changer >
class CSharedVarBase< CBaseHandle, Changer >
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

protected:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CBaseHandle			*m_pValue;
};

// Shared handle wrapper.
template< class Type, class Changer >
class CSharedHandleBase : public CSharedVarBase< EHANDLE, Changer >
{
public:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() {}

	void Initialize( char *pchName, int iEntNum )
	{
		contract_assert( !this->m_pSharedMemory && !this->m_pValue );

		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( EHANDLE ) + sizeof( int ) );		// Also allocate memory for the int that converts to a Client handle
		this->m_pValue = (EHANDLE*)this->m_pSharedMemory->Base();
		m_piClientIndex = (int*)((void*)(this->m_pValue + 1));
	}

	bool operator !() const 
	{ 
		return !GetHandle_Internal()->Get(); 
	}

	operator const Type*() const 
	{ 
		return Get_Internal();
	}

	const Type* Get() const 
	{ 
		return Get_Internal();
	}

	const Type* operator->() const 
	{ 
		return Get_Internal();
	}

	bool operator==( const Type *val ) const 
	{
		return Get_Internal() == val; 
	}

	bool operator!=( const Type *val ) const 
	{
		return Get_Internal() != val;
	}

private:
	EHANDLE GetHandle_Internal() const 
	{
			// Client needs to get the handle translated from the stroed int
			EHANDLE hClient = Client_IntToEHandle( *m_piClientIndex );
			return hClient;
	}

	Type* Get_Internal() const 
	{
		return static_cast< Type* >( GetHandle_Internal().Get() );
	}

public:
	int		*m_piClientIndex;	// Pointer to ints for Client handle translations
};

// Shared array wrapper.
template< class Type, class Changer >
class CSharedArrayBase : public CSharedVarBase< Type, Changer >
{
public:
	CSharedArrayBase( void )
	{
		m_iCount = 0;
	}

	void Initialize( char *pchName, int iEntNum, int iCount )
	{
		contract_assert( !this->m_pSharedMemory && !(CSharedArrayBase<Type,Changer>::m_pValue) && iCount > 0 );

		m_iCount = iCount;
		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( Type ) * m_iCount );
		CSharedArrayBase<Type,Changer>::m_pValue = (Type*)this->m_pSharedMemory->Base();
	}

	const int& Count() const
	{
		return m_iCount;
	}

	const Type* Get() const 
	{
		return CSharedArrayBase<Type,Changer>::m_pValue; 
	}

	const Type& Get( int i ) const
	{
		contract_assert( i >= 0 && i < m_iCount );
		return CSharedArrayBase<Type,Changer>::m_pValue[i];
	}

	const Type& operator[]( int i ) const
	{
		return Get( i );
	}

private:
public:
	int		m_iCount;
};

// Shared handle array wrapper.
template< class Type, class Changer >
class CSharedHandleArrayBase : public CSharedArrayBase< EHANDLE, Changer >
{
public:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() {}

	void Initialize( char *pchName, int iEntNum, int iCount )
	{
		contract_assert( !this->m_pSharedMemory && !(CSharedHandleArrayBase<Type,Changer>::m_pValue) && iCount > 0 );

		this->m_iCount = iCount;
		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( EHANDLE ) * this->m_iCount + sizeof( int ) * this->m_iCount );	// Also allocate memory for the ints that convert to Client handles
		this->m_pValue = (EHANDLE*)this->m_pSharedMemory->Base();
		m_piClientIndex = (int*)((void*)(CSharedHandleArrayBase<Type,Changer>::m_pValue + this->m_iCount));
	}

	const int& Count() const
	{
		return this->m_iCount;
	}

	const Type& Get( int i ) const
	{
		contract_assert( i >= 0 && i < this->m_iCount );
		return Get_Internal( i );
	}

	const Type* operator[]( int i ) const
	{
		return Get_Internal( i );
	}

private:
	EHANDLE GetHandle_Internal( int i ) const 
	{
			// Client needs to get the handle translated from the stroed int
			EHANDLE hClient = Client_IntToEHandle( m_piClientIndex[i] );
			return hClient;
	}

	Type* Get_Internal( int i ) const 
	{
		return static_cast< Type* >( GetHandle_Internal(i).Get() );
	}

public:
	int		*m_piClientIndex;	// Pointer to ints for Client handle translations
};

// Prevent Shared Vars from using UtlVector templates without the UtlVector wrapper
template< class Type, class Changer >
class CSharedVarBase< std::vector< Type >, Changer >
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

protected:
	virtual void Shared_CUtlVector_Vars_Must_Use_CSharedUtlVector() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	std::vector< Type >	*m_pValue;
};

// Shared UtlVector wrapper.
template< class Type, class Changer >
class CSharedUtlVectorBase : public CSharedVarBase< std::vector< Type >, Changer >
{
public:
	void Initialize( char *pchName, int iEntNum )
	{
		contract_assert( !this->m_pSharedMemory && !(CSharedUtlVectorBase<Type,Changer>::m_pValue) );

		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( std::vector< Type > ) );
		CSharedUtlVectorBase<Type,Changer>::m_pValue = (std::vector< Type >*)this->m_pSharedMemory->Base();
	}

	operator const std::vector< Type >&() const 
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const std::vector< Type >& Get() const 
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const std::vector< Type >* operator->() const 
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const Type& operator[]( int i ) const
	{
		return Get( i );
	}

	const Type& Get( int i ) const
	{
		contract_assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		return (*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i];
	}

	const int Count() const
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue->Count();
	}

protected:
	virtual void Shared_CUtlVector_Vars_Must_Use_CSharedUtlVector() {}

private:
};

// Use this macro to define a network variable.
#define CSharedVar( type, name ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedVarBase )

// Helper for color32's. Contains GetR(), SetR(), etc.. functions.
#define CSharedColor32( name ) \
	SHARED_VAR_START( color32, name ) \
	SHARED_VAR_END( color32, name, CSharedColor32Base )

// Vectors + some convenient helper functions.
#define CSharedVector( name ) CSharedVectorInternal( Vector, name )
#define CSharedQAngle( name ) CSharedVectorInternal( QAngle, name )

#define CSharedVectorInternal( type, name ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedVectorBase )

#define CSharedQuaternion( name ) \
	SHARED_VAR_START( Quaternion, name ) \
	SHARED_VAR_END( Quaternion, name, CSharedQuaternionBase )

#define CSharedHandle( handletype, name ) \
	SHARED_VAR_START( handletype, name ) \
	SHARED_VAR_END( handletype, name, CSharedHandleBase )

#define CSharedArray( type, name, count ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedArrayBase ) \
	enum { m_SharedArray_count_##name = count };

#define CSharedString( name, length ) \
	CSharedArray( char, name, length )

#define CSharedHandleArray( handletype, name, count ) \
	SHARED_VAR_START( handletype, name ) \
	SHARED_VAR_END( handletype, name, CSharedHandleArrayBase ) \
	enum { m_SharedArray_count_##name = count };

#define CSharedUtlVector( utlvectortype, name ) \
	SHARED_VAR_START( utlvectortype, name ) \
	SHARED_VAR_END( utlvectortype, name, CSharedUtlVectorBase ) \


// Internal macros used in definitions of network vars.
#define SHARED_VAR_START( type, name ) \
	class SharedVar_##name; \
	friend class SharedVar_##name; \
	typedef ThisClass MakeASharedVar_##name; \
	class SharedVar_##name \
	{ \
	public: \
		template <typename T> friend int ServerClassInit(T *);

#define SHARED_VAR_END( type, name, base ) \
	}; \
	base< type, SharedVar_##name > name;

#define DECLARE_SHAREDCLASS() \
	virtual void InitSharedVars( void );

#define IMPLEMENT_SHAREDCLASS_DT( type ) \
	void type::InitSharedVars( void ) \
	{ \
		BaseClass::InitSharedVars();

#define END_SHARED_TABLE() \
	}

#define SharedProp( var ) \
	var.Initialize( "SharedVar_"#var, entindex() );

#define SharedPropArray( var ) \
	var.Initialize( "SharedVar_"#var, entindex(), m_SharedArray_count_##var );

#undef CSharedVar
#undef CSharedColor32
#undef CSharedVector
#undef CSharedQAngle
#undef CSharedVectorInternal
#undef CSharedQuaternion
#undef CSharedHandle
#undef CSharedArray
#undef CSharedString
#undef CSharedHandleArray
#undef CSharedUtlVector
#undef SHARED_VAR_START
#undef SHARED_VAR_END
#undef DECLARE_SHAREDCLASS
#undef IMPLEMENT_SHAREDCLASS_DT
#undef END_SHARED_TABLE
#undef SharedProp
#undef SharedPropArray

extern ConVar ai_shot_bias_min;
extern ConVar ai_shot_bias_max;

//---------------------------------------------------------
// Caches off a shot direction and allows you to perform
// various operations on it without having to recalculate
// vecRight and vecUp each time. 
//---------------------------------------------------------
class CShotManipulator
{
public:
	CShotManipulator( const Vector &vecForward )
	{
		SetShootDir( vecForward );
	};

	void SetShootDir( const Vector &vecForward )
	{
		m_vecShotDirection = vecForward;
		VectorVectors( m_vecShotDirection, m_vecRight, m_vecUp );
	}

	const Vector &ApplySpread( const Vector &vecSpread, float bias = 1.0 );
	const Vector &ApplyAngularSpread( const Vector &vecSpread, float bias = 1.0 );

	const Vector &GetShotDirection()	{ return m_vecShotDirection; }
	const Vector &GetResult()			{ return m_vecResult; }
	const Vector &GetRightVector()		{ return m_vecRight; }
	const Vector &GetUpVector()			{ return m_vecUp;}

private:
	Vector m_vecShotDirection;
	Vector m_vecRight;
	Vector m_vecUp;
	Vector m_vecResult;
};

//---------------------------------------------------------
// Take a vector (direction) and another vector (spread) 
// and modify the direction to point somewhere within the 
// spread. This used to live inside FireBullets.
//---------------------------------------------------------
const Vector &CShotManipulator::ApplySpread( const Vector &vecSpread, float bias )
{
	// get circular gaussian spread
	float x, y, z;

	if ( bias > 1.0 )
		bias = 1.0;
	else if ( bias < 0.0 )
		bias = 0.0;

	float shotBiasMin = ai_shot_bias_min.GetFloat();
	float shotBiasMax = ai_shot_bias_max.GetFloat();

	// 1.0 gaussian, 0.0 is flat, -1.0 is inverse gaussian
	float shotBias = ( ( shotBiasMax - shotBiasMin ) * bias ) + shotBiasMin;

	float flatness = ( std::fabsf(shotBias) * 0.5 );

	do
	{
		x = random_valve->RandomFloat(-1,1) * flatness + random_valve->RandomFloat(-1,1) * (1 - flatness);
		y = random_valve->RandomFloat(-1,1) * flatness + random_valve->RandomFloat(-1,1) * (1 - flatness);
		if ( shotBias < 0 )
		{
			x = ( x >= 0 ) ? 1.0 - x : -1.0 - x;
			y = ( y >= 0 ) ? 1.0 - y : -1.0 - y;
		}
		z = x*x+y*y;
	} while (z > 1);

	m_vecResult = m_vecShotDirection + x * vecSpread.x * m_vecRight + y * vecSpread.y * m_vecUp;

	return m_vecResult;
}

const Vector &CShotManipulator::ApplyAngularSpread( const Vector &vecSpread, float bias )
{
	float x, y, z;
	x = vecSpread[0] * random_valve->RandomFloat(-0.5f, 0.5f);
	y = vecSpread[1] * random_valve->RandomFloat(-0.5f, 0.5f);
	z = vecSpread[2] * random_valve->RandomFloat(-0.5f, 0.5f);

	matrix3x4_t matrix;
	QAngle qa(x,y,z);
	AngleMatrix( qa, matrix );
	VectorTransform(m_vecShotDirection, matrix, m_vecResult);
	return m_vecResult;
}

enum simplekeyinterp_t
{
	KEY_LINEAR = 0,
	KEY_SPLINE,
	KEY_ACCELERATE,
	KEY_DECELERATE,
};

class CSimpleKeyInterp : public Vector
{
public:
	CSimpleKeyInterp( float t, simplekeyinterp_t interp, float x, float y = 0, float z = 0 );
	
	float GetTime() const { return m_keyTime; }
	
	// out = t*start + (1-t) * end (may be splinear or linear)
	static void Interp( Vector &out, float t, const CSimpleKeyInterp &start, const CSimpleKeyInterp &end );

	float				m_keyTime;
	simplekeyinterp_t	m_interp;
};

class CSimpleKeyList
{
public:
	int		Insert( const CSimpleKeyInterp &key );
	bool	Interp( Vector &out, float t );

	std::vector< CSimpleKeyInterp >	m_list;
};

inline constexpr auto ST_EPS = 0.001;

//-----------------------------------------------------------------------------

class CSimpleSimTimer
{
public:
	CSimpleSimTimer()
	 : m_next( -1 )
	{ 
	}

	void Force()
	{
		m_next = -1;
	}

	bool Expired() const
	{
		return ( gpGlobals->curtime - m_next > -ST_EPS );
	}

	float Delay( float delayTime )
	{
		return (m_next += delayTime);
	}
	
	float GetNext() const
	{
		return m_next;
	}

	void Set( float interval )
	{
		m_next = gpGlobals->curtime + interval;
	}

	void Set( float minInterval, float maxInterval );

	float GetRemaining() const;

	DECLARE_SIMPLE_DATADESC();
	
protected:
	float m_next;
};

//-----------------------------------------------------------------------------

class CSimTimer : public CSimpleSimTimer
{
public:
	CSimTimer( float interval = 0.0, bool startExpired = true )	
	{ 
		Set( interval, startExpired );
	}
	
	void Set( float interval, bool startExpired = true );

	void Reset( float interval = -1.0 )
	{
		if ( interval == -1.0 )
		{
			m_next = gpGlobals->curtime + m_interval;
		}
		else
		{
			m_next = gpGlobals->curtime + interval;
		}
	}

	float GetInterval() const
	{
		return m_interval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_interval;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class CRandSimTimer : public CSimpleSimTimer
{
public:
	CRandSimTimer( float minInterval = 0.0, float maxInterval = 0.0, bool startExpired = true )	
	{ 
		Set( minInterval, maxInterval, startExpired );
	}
	
	void Set( float minInterval, float maxInterval = 0.0, bool startExpired = true );

	void Reset()
	{
		if ( m_maxInterval == 0 )
			m_next = gpGlobals->curtime + m_minInterval;
		else
			m_next = gpGlobals->curtime + random_valve->RandomFloat( m_minInterval, m_maxInterval );
	}

	float GetMinInterval() const
	{
		return m_minInterval;
	}

	float GetMaxInterval() const
	{
		return m_maxInterval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_minInterval;
	float m_maxInterval;
};

//-----------------------------------------------------------------------------

class CStopwatchBase  : public CSimpleSimTimer
{
public:
	CStopwatchBase()	
	{ 
		m_fIsRunning = false;
	}

	bool IsRunning() const
	{
		return m_fIsRunning;
	}
	
	void Stop()
	{
		m_fIsRunning = false;
	}

	bool Expired() const
	{
		return ( m_fIsRunning && CSimpleSimTimer::Expired() );
	}
	
	DECLARE_SIMPLE_DATADESC();
	
protected:
	bool m_fIsRunning;
	
};

//-------------------------------------
class CSimpleStopwatch  : public CStopwatchBase
{
public:
	void Start( float minCountdown, float maxCountdown = 0.0 );

	void Stop()
	{
		m_fIsRunning = false;
	}

	bool Expired() const
	{
		return ( m_fIsRunning && CSimpleSimTimer::Expired() );
	}
};
//-------------------------------------

class CStopwatch : public CStopwatchBase
{
public:
	CStopwatch ( float interval = 0.0 )
	{ 
		Set( interval );
	}
	
	void Set( float interval )
	{ 
		m_interval = interval;
	}

	void Start( float intervalOverride );

	void Start()
	{
		Start( m_interval );
	}
	
	float GetInterval() const
	{
		return m_interval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_interval;
};

//-------------------------------------

class CRandStopwatch : public CStopwatchBase
{
public:
	CRandStopwatch( float minInterval = 0.0, float maxInterval = 0.0 )	
	{ 
		Set( minInterval, maxInterval );
	}
	
	void Set( float minInterval, float maxInterval = 0.0 );

	void Start( float minOverride, float maxOverride = 0.0 );

	void Start()
	{
		Start( m_minInterval, m_maxInterval );
	}
	
	float GetInterval() const
	{
		return m_minInterval;
	}

	float GetMinInterval() const
	{
		return m_minInterval;
	}

	float GetMaxInterval() const
	{
		return m_maxInterval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_minInterval;
	float m_maxInterval;
};

//-----------------------------------------------------------------------------

class CThinkOnceSemaphore
{
public:
	CThinkOnceSemaphore()
	 :	m_lastTime( -1 )
	{
	}

	bool EnterThink();

	bool DidThink() const;

	void SetDidThink()
	{
		m_lastTime = gpGlobals->curtime;
	}

private:
	float m_lastTime;
};

//-----------------------------------------------------------------------------

class CBaseEntity;

using namespace ::ks::reflect;

struct SlideMaterialList_t
{
	char				szSlideKeyword[64];
	std::vector< int >		iSlideMaterials;
	std::vector< int >		iSlideIndex;
};

class CSlideshowDisplay : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SlideshowDisplay" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSlideshowDisplay, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CSlideshowDisplay();
	virtual ~CSlideshowDisplay();

	void Spawn( void );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void ClientThink( void );

	bool IsEnabled( void ) { return m_bEnabled; }

	void GetDisplayText( char *pchText ) { V_strcpy( pchText, m_szDisplayText ); }
	int CurrentMaterialIndex( void ) { return m_iCurrentMaterialIndex; }
	int GetMaterialIndex( int iSlideIndex );
	int NumMaterials( void );
	int CurrentSlideIndex( void ) { return m_iCurrentSlideIndex; }

private:

	void BuildSlideShowImagesList( void );

private:

	[[= ks::reflect::Net{} ]] bool	m_bEnabled;

	[[= ks::reflect::Net{} ]] char	m_szDisplayText[ 128 ];

	[[= ks::reflect::Net{} ]] char	m_szSlideshowDirectory[ 128 ];

	std::vector< SlideMaterialList_t* >	m_SlideMaterialLists;
	[[= ks::reflect::Net{} ]] unsigned char						m_chCurrentSlideLists[ 16 ];
	int									m_iCurrentMaterialIndex;
	int									m_iCurrentSlideIndex;

	[[= ks::reflect::Net{} ]] float	m_fMinSlideTime;
	[[= ks::reflect::Net{} ]] float	m_fMaxSlideTime;

	float	m_NextSlideTime;

	[[= ks::reflect::Net{} ]] int		m_iCycleType;
	[[= ks::reflect::Net{} ]] bool	m_bNoListRepeats;
	int		m_iCurrentSlideList;
	int		m_iCurrentSlide;
};

extern std::vector< CSlideshowDisplay* > g_SlideshowDisplays;

inline constexpr auto TRADE_DURATION_MIN = 5;
inline constexpr auto TRADE_DURATION_MAX = 10;
inline constexpr auto SMOKEGRENADE_PARTICLERADIUS = 55;

inline constexpr auto SMOKESPHERE_EXPAND_TIME = 5.5; // Take N seconds to expand to SMOKESPHERE_MAX_RADIUS.

inline constexpr auto NUM_PARTICLES_PER_DIMENSION = 6;
inline constexpr auto SMOKEPARTICLE_OVERLAP = 0;

inline constexpr auto SMOKEPARTICLE_SIZE = 55;
inline constexpr auto NUM_MATERIAL_HANDLES = 1;

void InitSmokeFogOverlay();
void TermSmokeFogOverlay();
void DrawSmokeFogOverlay();

// Set these before calling DrawSmokeFogOverlay.
extern float	g_SmokeFogOverlayAlpha;
extern Vector	g_SmokeFogOverlayColor;

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseParticleEntity;
class SimpleParticle;

using namespace ::ks::reflect;


// This defines the client-side SmokeTrail entity. It can also be used without
// an entity, in which case you must pass calls to it and set its position each frame.

//
// Smoke Trail
//

class CSmokeTrail : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSmokeTrail, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					CSmokeTrail();
	virtual			~CSmokeTrail();

public:

	//For attachments
	void			GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);

// CBaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	// Effect parameters. These will assume default values but you can change them.
	[[= ks::reflect::Net{} ]] float			m_SpawnRate;			// How many particles per second.

	[[= ks::reflect::Net{} ]] Vector			m_StartColor;			// Fade between these colors.
	[[= ks::reflect::Net{} ]] Vector			m_EndColor;
	[[= ks::reflect::Net{} ]] float			m_Opacity;

	[[= ks::reflect::Net{} ]] float			m_ParticleLifetime;		// How long do the particles live?
	[[= ks::reflect::Net{} ]] float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	[[= ks::reflect::Net{} ]] float			m_MinSpeed;				// Speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxSpeed;
	
	[[= ks::reflect::Net{} ]] float			m_MinDirectedSpeed;		// Directed speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxDirectedSpeed;

	[[= ks::reflect::Net{} ]] float			m_StartSize;			// Size ramp.
	[[= ks::reflect::Net{} ]] float			m_EndSize;

	[[= ks::reflect::Net{} ]] float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	[[= ks::reflect::Net{} ]] bool			m_bEmit;				// Keep emitting particles?

	[[= ks::reflect::Net{} ]] int				m_nAttachment;

private:
	CSmokeTrail( const CSmokeTrail & );

	PMaterialHandle	m_MaterialHandle[2];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	ks::intrusive_ptr<CSimpleEmitter> m_pSmokeEmitter;
};

//==================================================
// CRocketTrail
//==================================================

class CRocketTrail : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_RocketTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CRocketTrail, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					CRocketTrail();
	virtual			~CRocketTrail();

public:

	//For attachments
	void			GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);

// CBaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	// Effect parameters. These will assume default values but you can change them.
	[[= ks::reflect::Net{} ]] float			m_SpawnRate;			// How many particles per second.

	[[= ks::reflect::Net{} ]] Vector			m_StartColor;			// Fade between these colors.
	[[= ks::reflect::Net{} ]] Vector			m_EndColor;
	[[= ks::reflect::Net{} ]] float			m_Opacity;

	[[= ks::reflect::Net{} ]] float			m_ParticleLifetime;		// How long do the particles live?
	[[= ks::reflect::Net{} ]] float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	[[= ks::reflect::Net{} ]] float			m_MinSpeed;				// Speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxSpeed;
	
	[[= ks::reflect::Net{} ]] float			m_StartSize;			// Size ramp.
	[[= ks::reflect::Net{} ]] float			m_EndSize;

	[[= ks::reflect::Net{} ]] float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	[[= ks::reflect::Net{} ]] bool			m_bEmit;				// Keep emitting particles?
	[[= ks::reflect::Net{} ]] bool			m_bDamaged;				// Has been shot down (should be on fire, etc)

	[[= ks::reflect::Net{} ]] int				m_nAttachment;

	Vector			m_vecLastPosition;		// Last known position of the rocket
	[[= ks::reflect::Net{} ]] float			m_flFlareScale;			// Size of the flare

private:
	CRocketTrail( const CRocketTrail & );

	PMaterialHandle	m_MaterialHandle[2];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	ks::intrusive_ptr<CSimpleEmitter> m_pRocketEmitter;
};

class SporeSmokeEffect;

//==================================================
// SporeEffect
//==================================================

class SporeEffect : public CSimpleEmitter
{
public:
	explicit 				SporeEffect( const char *pDebugName );
	static SporeEffect*		Create( const char *pDebugName );

	virtual void			UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector			UpdateColor( const SimpleParticle *pParticle );
	virtual float			UpdateAlpha( const SimpleParticle *pParticle );

private:
	SporeEffect( const SporeEffect & );
};

//==================================================
// CSporeExplosion
//==================================================

class CSporeExplosion : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_SporeExplosion" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSporeExplosion, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
	CSporeExplosion( void );
	virtual	~CSporeExplosion( void );

public:

// CBaseEntity
public:
	virtual	void	OnDataChanged( DataUpdateType_t updateType );

// IPrototypeAppEffect
public:
	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );

// IParticleEffect
public:
	virtual void	Update( float fTimeDelta );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	[[= ks::reflect::Net{} ]] float	m_flSpawnRate;
	[[= ks::reflect::Net{} ]] float	m_flParticleLifetime;
	[[= ks::reflect::Net{} ]] float	m_flStartSize;
	[[= ks::reflect::Net{} ]] float	m_flEndSize;
	[[= ks::reflect::Net{} ]] float	m_flSpawnRadius;
	float	m_flPreviousSpawnRate;

	[[= ks::reflect::Net{} ]] bool	m_bEmit;
	[[= ks::reflect::Net{} ]] bool	m_bDontRemove;

private:
	CSporeExplosion( const CSporeExplosion & );

	void	AddParticles( void );

	PMaterialHandle		m_hMaterial;
	TimedEvent			m_teParticleSpawn;

	SporeEffect			*m_pSporeEffect;
	CParticleMgr		*m_pParticleMgr;
};

//
// Particle trail
//

class CSmokeParticle;

class CFireTrail : public CParticleTrail
{
	[[= ks::reflect::NetTable{ .name = "DT_FireTrail" } ]]
	[[= ks::reflect::From<"m_nAttachment", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_flLifetime", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFireTrail, CParticleTrail );
	DECLARE_CLIENTCLASS();

	CFireTrail( void );
	virtual ~CFireTrail( void );

	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );
	virtual void	Update( float fTimeDelta );

private:

	enum
	{
		// Smoke
		FTRAIL_SMOKE1,
		FTRAIL_SMOKE2,

		// Large flame
		FTRAIL_FLAME1,
		FTRAIL_FLAME2,
		FTRAIL_FLAME3,
		FTRAIL_FLAME4,
		FTRAIL_FLAME5,

		NUM_FTRAIL_MATERIALS
	};

	ks::intrusive_ptr<CSimpleEmitter>	m_pTrailEmitter;
	ks::intrusive_ptr<CSmokeParticle>	m_pSmokeEmitter;

	PMaterialHandle				m_hMaterial[NUM_FTRAIL_MATERIALS];

	Vector						m_vecLastPosition;

	CFireTrail( const CFireTrail & );
};

//-----------------------------------------------------------------------------
// Purpose:  High drag, non color changing particle
//-----------------------------------------------------------------------------

class CDustFollower : public CSimpleEmitter
{
public:
	
	explicit CDustFollower( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CDustFollower *Create( const char *pDebugName )
	{
		return new CDustFollower( pDebugName );
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		return ( ((float)pParticle->m_uchStartAlpha/255.0f) * std::sin( std::numbers::pi * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
	}

	virtual	void	UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_vecVelocity = pParticle->m_vecVelocity * ExponentialDecay( 0.3, timeDelta );
	}

	//Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta *= ExponentialDecay( 0.5, timeDelta );

		return pParticle->m_flRoll;
	}

private:
	CDustFollower( const CDustFollower & );
};

//==================================================
// CDustTrail
//==================================================

class CDustTrail : public CBaseParticleEntity, public IPrototypeAppEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_DustTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDustTrail, CBaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					CDustTrail();
	virtual			~CDustTrail();

public:

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);

// CBaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

public:
	// Effect parameters. These will assume default values but you can change them.
	[[= ks::reflect::Net{} ]] float			m_SpawnRate;			// How many particles per second.

	[[= ks::reflect::Net{} ]] Vector			m_Color;
	[[= ks::reflect::Net{} ]] float			m_Opacity;

	[[= ks::reflect::Net{} ]] float			m_ParticleLifetime;		// How long do the particles live?
	float			m_StartEmitTime;		// When did I start emitting particles?
	[[= ks::reflect::Net{} ]] float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	[[= ks::reflect::Net{} ]] float			m_MinSpeed;				// Speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxSpeed;
	
	[[= ks::reflect::Net{} ]] float			m_MinDirectedSpeed;		// Directed speed range.
	[[= ks::reflect::Net{} ]] float			m_MaxDirectedSpeed;

	[[= ks::reflect::Net{} ]] float			m_StartSize;			// Size ramp.
	[[= ks::reflect::Net{} ]] float			m_EndSize;

	[[= ks::reflect::Net{} ]] float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	[[= ks::reflect::Net{} ]] bool			m_bEmit;				// Keep emitting particles?

private:
	CDustTrail( const CDustTrail & );

	PMaterialHandle	m_MaterialHandle[DUSTTRAIL_MATERIALS];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	ks::intrusive_ptr<CSimpleEmitter> m_pDustEmitter;
};

// solid_t parsing
class CSolidSetDefaults : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) {}
	virtual void SetDefaults( void *pData );
};

extern CSolidSetDefaults g_SolidSetup;

class CBasePlayer;
struct EmitSound_t;



class CSoundPatch;

enum soundcommands_t
{
	SOUNDCTRL_CHANGE_VOLUME,
	SOUNDCTRL_CHANGE_PITCH,
	SOUNDCTRL_STOP,
	SOUNDCTRL_DESTROY,
};

//Envelope point
struct envelopePoint_t
{
	float	amplitudeMin, amplitudeMax;
	float	durationMin, durationMax;
};

//Envelope description
struct envelopeDescription_t
{
	envelopePoint_t	*pPoints;
	int				nNumPoints;
};

class CSoundEnvelopeController
{
public:
	virtual void		SystemReset( void ) = 0;
	virtual void		SystemUpdate( void ) = 0;
	virtual void		Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime = 0 ) = 0;
	virtual void		CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float value ) = 0;
	virtual void		CommandClear( CSoundPatch *pSound ) = 0;
	virtual void		Shutdown( CSoundPatch *pSound ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
							float attenuation, float scriptVolume = 1.0f ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, const Vector *pSoundOrigin, float scriptVolume = 1.0f ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
							soundlevel_t soundlevel ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es ) = 0;

	virtual void		SoundDestroy( CSoundPatch	* ) = 0;
	virtual void		SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime ) = 0;
	virtual void		SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime ) = 0;
	virtual void		SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout = false ) = 0;
	virtual float		SoundGetPitch( CSoundPatch *pSound ) = 0;
	virtual float		SoundGetVolume( CSoundPatch *pSound ) = 0;

	virtual float		SoundGetElapsedTime( CSoundPatch *pSound) = 0;
	virtual bool		SoundIsStillPlaying( CSoundPatch *pSound) = 0;
	virtual int			SoundGetGuid( CSoundPatch	* ) = 0;

	virtual float		SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints ) = 0;
	virtual float		SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope ) = 0;

	virtual void		CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer ) = 0;

	virtual string_t	SoundGetName( CSoundPatch *pSound ) = 0;
	virtual string_t	SoundGetScriptName( CSoundPatch *pSound ) = 0;
	static	CSoundEnvelopeController &GetController( void );

	virtual void		SoundSetCloseCaptionDuration( CSoundPatch *pSound, float flDuration ) = 0;
};

class CHintSystem;
class CBasePlayer;

using namespace ::ks::reflect;



//--------------------------------------------------------------------------------------------------------------
class CHintMessage
{
public:
	CHintMessage( const char * hintString, std::vector< const char * > * args, float duration );
	~CHintMessage();

	float GetDuration() const { return m_duration; }
	void Send( CBasePlayer *client );

	bool IsEquivalent( const char *hintString, std::vector< const char * > * args ) const;

private:
	const char * m_hintString;					///< hintString is a pointer to a string that should never be deleted.
	std::vector< char * > m_args;				///< list of arguments.  The memory for these strings is internal to the CHintMessage.
	float m_duration;							///< time until the next message can be displayed
};

//--------------------------------------------------------------------------------------------------------------
class CHintMessageQueue
{
public:
	CHintMessageQueue( CBasePlayer *pPlayer );
	void		Reset();
	void		Update();
	bool		AddMessage( const char* message, float duration = 6.0f, std::vector< const char * > * args = nullptr );
	inline bool IsEmpty() { return m_messages.empty(); }

private:
	float		m_tmMessageEnd;
	std::vector< CHintMessage * > m_messages;
	CBasePlayer *m_pPlayer;
};

//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Timers that manage hint messages that should be displayed after some time.
class CHintMessageTimers
{
public:
	CHintMessageTimers( void );
	CHintMessageTimers( CHintSystem *pSystem, CHintMessageQueue *pQueue );

	void		Reset();
	void		Update();

	// Add / Register timers that will be started/stopped during play
	void		AddTimer( int iHintID, float timer_duration, float message_duration = 6.0f, std::vector< const char * > * args = nullptr );
	void		RemoveTimer( int iHintID );

	// Start / Stop timers that were previously registered via AddTimer()
	void		StartTimer( int iHintID );
	void		StopTimer( int iHintID );

private:
	int			GetTimerIndex( int iHintID );

private:
	struct hintmessagetime_t
	{
		hintmessagetime_t( float flTimerDuration ) :
			timer(flTimerDuration)
		{
			iHintID = 0;
			flMessageDuration = 6.0;
		}

		~hintmessagetime_t()
		{
			for ( int i=0; i<std::ssize( args ); ++i )
			{
				delete[] args[i];
			}
			args.clear();
		}

		int						iHintID;
		CStopwatch				timer;
		float					flMessageDuration;
		std::vector< char * >	args;
	};

	std::vector< hintmessagetime_t* >  m_Timers;
	CHintMessageQueue				  *m_pQueue;
	CHintSystem						  *m_pHintSystem;
};

class CHintMessageQueue;
class CHintMessageTimers;
class CBasePlayer;

// Purpose: A class embedded in players to provide hints to that player



typedef bool (*HintTimerCallback)( CBasePlayer *pOnPlayer );

//-----------------------------------------------------------------------------
// Purpose: A class embedded in players to provide hints to that player
//-----------------------------------------------------------------------------
class CHintSystem
{
	DECLARE_CLASS_NOBASE( CHintSystem );

public:
	CHintSystem();
	~CHintSystem();

	//-----------------------------------------------------
	// Call this from your player constructor
	void Init( CBasePlayer *pPlayer, int iMaxHintTypes, const char **pszHintStrings );

	//-----------------------------------------------------
	// CBasePlayer calls these for you, if you fall back to its 
	// versions of Spawn(), Event_Killed(), and PreThink().
	// Call this when your player respawns
	void				ResetHints( void );

	// Call this when your player dies
	void				ResetHintTimers( void );

	// Call this when in your player PreThink()
	void				Update( void );

	//-----------------------------------------------------
	// Hint addition
	// Call these to add a hint directly onscreen
	bool 				HintMessage( int hint, bool bForce = false, bool bOnlyIfClear = false );
	void 				HintMessage( const char *pMessage );

	// Call this to add a hint timer. It'll be reset for you automatically 
	// everytime ResetHintTimers() is called.
	void				RegisterHintTimer( int iHintID, float flTimerDuration, bool bOnlyIfClear = false, HintTimerCallback pfnCallback = nullptr );

	// Call these to start & stop registered hint timers
	void				StartHintTimer( int iHintID );
	void				StopHintTimer( int iHintID );
	void				RemoveHintTimer( int iHintID );
	bool				TimerShouldFire( int iHintID );

	// Set whether a player should see any hints at all
	void				SetShowHints( bool bShowHints ) { m_bShowHints = bShowHints; }
	void				SetHintPlayed( int iHintID );
	bool				ShouldShowHints( void );

	// Returns true if the hint has been played already
	bool				HasPlayedHint( int iHintID );
	void				PlayedAHint( void );
	void				ClearHintHistory( void ) { std::ranges::fill( m_HintHistory, false ); }

	// Not really an optimal solution, but saves us querying the hud element,
	// which wouldn't be easy with derived versions in different mods.
	bool				HintIsCurrentlyVisible( void ) { return (gpGlobals->curtime - m_flLastHintPlayedAt < 11 ); }

private:
	void				ReAddHintTimerIfNotDisplayed( int iHintID, float flTimerDuration );

private:
	CBasePlayer			*m_pPlayer;

	float				m_flLastHintPlayedAt;
	bool				m_bShowHints;
	std::vector< bool >	m_HintHistory;
	const char			**m_pszHintMessages;
	CHintMessageQueue	*m_pHintMessageQueue;
	CHintMessageTimers	*m_pHintMessageTimers;

	struct onresethints_t
	{
		int					iHintID;
		float				flTimer;
		bool				bOnlyIfClear;
		HintTimerCallback	pfnCallback;
	};
	std::vector< onresethints_t >	m_RegisteredResetHints;
};

// Derive from this if you have an entity that wants to display a hint
// when the player waves his target ID over it on the client.
class ITargetIDProvidesHint
{
public:
	virtual void	DisplayHintTo( CBasePlayer *pPlayer ) = 0;
};

class CUserCmd;
class CBaseEntity;
class CBasePlayer;


class CHLTVCamera : public CGameEventListener
{
public:
	CHLTVCamera();
	virtual ~CHLTVCamera();

	enum AutodirectorState_t
	{
		AUTODIRECTOR_ON = 0,
		AUTODIRECTOR_OFF,
		AUTODIRECTOR_PAUSED
	};

	void Init();
	void Reset();
	void Update();

	void CalcView(CViewSetup *pSetup);
	void FireGameEvent( IGameEvent *event );

	void SetMode(int iMode);
	void SetChaseCamParams( float flOffset, float flDistance, float flTheta, float flPhi  );
	void SpecNextPlayer( bool bInverse );
	void SpecNamedPlayer( const char *szPlayerName );
	void SpecPlayerByIndex( int iIndex );
	void SpecPlayerByAccountID( const char *pszPlayerID );
	void ToggleChaseAsFirstPerson();
	bool IsPVSLocked();
	bool HasCameraMan() { return ( m_iCameraMan != 0 ); }
	
	void SetAutoDirector( AutodirectorState_t eState );
	AutodirectorState_t AutoDirectorState() const;
	bool IsAutoDirectorOn() const;
	
	int  GetMode();	// returns current camera mode
	CBaseEntity *GetPrimaryTarget();  // return primary target
	void SetPrimaryTarget( int nEntity); // set the primary obs target
	CBasePlayer *GetCameraMan();  // return camera entity if any
	Vector GetCameraPosition() { return m_vCamOrigin; }
	int GetCurrentOrLastTarget() { return (m_iTarget1 != 0) ? m_iTarget1 : m_iLastTarget1; }
	int GetCurrentTargetEntindex() { return m_iTarget1; }

	void SetWatchingGrenade( CBaseEntity *pGrenade, bool bWatching );
	bool IsWatchingGrenade( void ) { return m_bIsFollowingGrenade; }
	void CreateMove(CUserCmd *cmd);
	void FixupMovmentParents();
	void PostEntityPacketReceived();
	const char* GetTitleText() { return m_szTitleText; }
	int  GetNumSpectators() { return m_nNumSpectators; }
	float	GetIdealOverviewScale( void ) { return m_flIdealOverviewScale; }
	void SpecCameraGotoPos( Vector vecPos, QAngle angAngle, int nPlayerIndex = 0 );
	void SpecCameraLerptoPos( const Vector &origin, const QAngle &angles, int nPlayerIndex = 0, float flTime = 0.0f );

protected:

	void CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void CalcFixedView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void CalcInEyeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov);
	void CalcChaseOverview( CViewSetup &pSetup );
	Vector CalcIdealOverviewPosition( Vector vecStartPos, Vector vOldOverviewPos );

	//void CalcOverview( CViewSetup *pSetup );

	void SmoothCameraAngle( QAngle& targetAngle );
	void SetCameraAngle( QAngle& targetAngle );
	void Accelerate( Vector& wishdir, float wishspeed, float accel );

	int			m_nCameraMode; // current camera mode
	int			m_iCameraMan; // camera man entindex or 0
	Vector		m_vCamOrigin;  //current camera origin
	Vector		m_vLastGrenadeVelocity;  //current camera origin
	float		m_flLastGrenadeVelocityUpdate;
	QAngle		m_aCamAngle;   //current camera angle
	int			m_iTarget1;	// first tracked target or 0
	int			m_iLastTarget1; // the last target before we switched
	int			m_iTarget2; // second tracked target or 0
	float		m_flFOV; // current FOV
	float		m_flOffset;  // z-offset from target origin
	float		m_flDistance; // distance to traget origin+offset
	float		m_flLastDistance; // too smooth distance
	float		m_flTheta; // view angle horizontal 
	float		m_flPhi; // view angle vertical
	float		m_flInertia; // camera inertia 0..100
	float		m_flLastAngleUpdateTime;
	bool		m_bEntityPacketReceived;	// true after a new packet was received
	int			m_nNumSpectators;
	char		m_szTitleText[64];
	CUserCmd	m_LastCmd;
	Vector		m_vecVelocity;
	float       m_flAutodirectorPausedTime; // negative if autodirector is not paused
	float		m_flIdealOverviewScale;
	Vector		m_vOldOverviewPos;
	Vector		m_vIdealOverviewPos;
	float		m_flLastCamZPos;
	float		m_flNextIdealOverviewPosUpdate;
	bool		m_bIsFollowingGrenade;
	bool					m_bIsSpecLerping;
	float					m_flSpecLerpTime;
	float					m_flSpecLerpEndTime;
	Vector					m_vecSpecLerpIdealPos;
	QAngle					m_angSpecLerpIdealAng;
	Vector					m_vecSpecLerpOldPos;
	QAngle					m_angSpecLerpOldAng;
	float			m_flObserverChaseApproach;
	Vector			m_vecObserverEyeDirPrevious;
};

extern CHLTVCamera *HLTVCamera();	// get Singleton

class CBasePlayer;


// this struct is followed by matrix3x4a_t bones
struct alignas(16) CachedRagdollBones_t
{
	int nBones;
	int nBodyParts;
	bool bAllAsleep;

	matrix3x4a_t *GetBones() { return ( matrix3x4a_t * )( this + 1 ); }
	matrix3x4a_t *GetBodyParts() { return GetBones() + nBones; }

};

class CHltvReplaySystem: public CGameEventListener
{
public:
	struct ReplayParams_t
	{
		int nRequest; // member of enum ReplayEventType_t
		int nPrimaryTargetEntIndex;
		float flEventTime;

		float flTime; // when gpGlobal->curtime reaches this value, replay starts
		int nPrimaryVictimEntIndex;
		bool bPrimaryVictimIsLocalPlayer;
		bool bFinalKillOfRound;

		ReplayParams_t()
		{
			Reset();
		}
		void Stop()
		{
			nRequest = -1;
		}
		void Reset()
		{
			nRequest = -1;
			flTime = -1000;
			nPrimaryVictimEntIndex = nPrimaryVictimEntIndex = -1;
			bPrimaryVictimIsLocalPlayer = false;
			flEventTime = 0;
			bFinalKillOfRound = false;
		}

		bool IsValid() { return nRequest >= 0; }
		void Invalidate() { nRequest = -1; }
	};

	class CLocalPlayerProps
	{
	public:
		bool m_bLastSeenAlive;
		int m_nLastTickUpdated;
	public:
		CLocalPlayerProps();
		bool Update();
	};
public:
	CHltvReplaySystem();
	~CHltvReplaySystem();

	void OnHltvReplay( const ks::net::CSVCMsg_HltvReplay  &msg );
	void OnHltvReplayTick();

	void EmitTimeJump();

	int GetHltvReplayDelay() { return m_nHltvReplayDelay; }
	void StopHltvReplay();
	float GetReplayVideoFadeAmount()const { m_flDebugLastRequestedReplayVideoFadeAmount = m_flReplayVideoFadeAmount;  return m_flReplayVideoFadeAmount; }
	float GetHltvReplayDistortAmount()const;

	bool IsHltvReplayButtonEnabled();

	bool IsHltvReplayButtonTimedOut();

	void RequestHltvReplayDeath();
	void RequestHltvReplay( const ReplayParams_t &replay );
	void RequestCancelHltvReplay( bool bSuppressFadeout = false );
	bool IsHltvReplayFeatureEnabled();

	bool IsFadeoutFinished();
	bool IsFadeoutActive();
	bool IsDelayedReplayRequestPending() { return m_DelayedReplay.IsValid(); }
	bool PrepareHltvReplayCountdown( );
	void CancelDelayedHltvReplay();

	void OnLevelInit();
	void OnPlayerDeath( IGameEvent *event );

	bool WantsReplayEffect()const;
	void OnDemoPlayback( bool bPlaying );
	void SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights );
	void SetDemoPlaybackFadeBrackets( int nCurrentPlaybackTick, int nStartAt, int nStopAt );
	bool IsDemoPlayback() const { return m_bDemoPlayback; }
	bool IsDemoPlaybackLowLights() const { return m_bDemoPlaybackLowLights; }
	void FireGameEvent( IGameEvent *event );
	void OnLevelShutdown();
	void OnLocalPlayerRespawning();

	int GetPrimaryVictimEntIndex()const { return m_DelayedReplay.nPrimaryVictimEntIndex; }
	bool IsPrimaryVictimLocalPlayer()const { return m_DelayedReplay.bPrimaryVictimIsLocalPlayer; }
	bool IsReplayingFinalKillOfRound()const { return m_nHltvReplayDelay && m_DelayedReplay.bFinalKillOfRound; }

	void StopFades();
	void StopFadeout();
	void StartFadeout(float flEndTime, float flDuration );
	void StartFadein( float flStartRealTime );
	void SetReplaySoundMixLayer( float flFade );

	CBasePlayer *GetDemoPlaybackPlayer();
	bool IsDemoPlaybackXuidOther()const;

	void Update();

	void PurgeRagdollBoneCache();

	void CacheRagdollBones();
	CachedRagdollBones_t *GetCachedRagdollBones( int nEntIndex, bool bTake );
	void FreeCachedRagdollBones( CachedRagdollBones_t* );

	enum ExperimentalEvents_t
	{
		EE_REPLAY_OFFERED = 1,
		EE_REPLAY_REQUESTED = 2,
		EE_REPLAY_STARTED = 4,
		EE_REPLAY_CANCELLED = 8,
		EE_REPLAY_AUTOMATIC	= 16,
		EE_REPLAY_STUCK = 32
		//EE_REPLAY_VICTIM_POV = 64 // never shipped, removed after discussions with Vitaliy, BrianL, Gautam
	};

	uint GetExperimentalEvents() const { return m_nExperimentalEvents; }
	bool UpdateHltvReplayButtonTimeOutState();
protected:
	static float GetReplayMessageTime();
protected:
	int		m_nHltvReplayDelay;
	int		m_nHltvReplayStopAt;
	int		m_nHltvReplayStartAt;
	int		m_nHltvReplaySlowdownBegin;
	int		m_nHltvReplaySlowdownEnd;
	float	m_flHltvReplaySlowdownRate;
	float	m_flHltvLastRequestRealTime;
	float	m_flLastReplayStoppedRealTime;
	float	m_flLastPlayerDeath;
	bool	m_bHltvReplayButtonTimedOut;
	int		m_nHltvReplayPrimaryTarget;
	CLocalPlayerProps m_LocalPlayer;
	bool	m_bWaitingForHltvReplayTick;
	float	m_flStartedWaitingForHltvReplayTickRealTime;
	int		m_nHltvReplayBeginTick;

	ReplayParams_t m_DelayedReplay;

	bool	m_bDemoPlayback;
	bool	m_bListeningForGameEvents;

	int		m_nDemoPlaybackStartAt;
	int		m_nDemoPlaybackStopAt;
	uint64	m_nDemoPlaybackXuid;
	bool	m_bDemoPlaybackLowLights;
	CHandle< CBasePlayer > m_DemoPlaybackPlayer;
	int		m_nCurrentPlaybackTick;
	float	m_flCurrentPlaybackTime;

	float m_flReplayVideoFadeAmount;
	float m_flReplaySoundFadeAmount;

	float m_flFadeinStartRealTime;
	float m_flFadeinDuration;
	float m_flFadeoutEndTime;
	float m_flFadeoutDuration;

	mutable float m_flDebugLastRequestedReplayVideoFadeAmount;

	uint32 m_nExperimentalEvents;

	float m_flFreeCachedRagdollBonesTime; // time (when not replaying) to destroy cached-off ragdoll bones
	struct CachedRagdollBonesDeleter { void operator()( CachedRagdollBones_t *pBones ) const; };
	std::unordered_map< int, std::unique_ptr< CachedRagdollBones_t, CachedRagdollBonesDeleter > > m_mapCachedRagdollBones;
};

extern CHltvReplaySystem g_HltvReplaySystem;
extern int CL_GetHltvReplayDelay();

class CHintSystem;
class CNewParticleEffect;
class CPrediction;
class CUserCmd;
class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseViewModel;
class CColorCorrection;
class CPostProcessController;
class IRagdoll;

using namespace ::ks::reflect;

// Purpose: Client-side CBasePlayer.
//			- Manages the player's flashlight effect.

extern int g_nKillCamMode;
extern int g_nKillCamTarget1;
extern int g_nKillCamTarget2;

class CCommandContext
{
public:
	bool			needsprocessing;

	CUserCmd		cmd;
	int				command_number;
};

class CPredictionError
{
public:
	float	time;
	Vector	error;
};

enum PlayerRenderMode_t
{
	PLAYER_RENDER_NONE = 0,
	PLAYER_RENDER_FIRSTPERSON,
	PLAYER_RENDER_THIRDPERSON,
};

bool IsInFreezeCam( void );

// DT_LocalPlayerExclusive is named as the "localdata" prop of DT_BasePlayer, so the table has to be
// visible where the annotation sits.
namespace DT_LocalPlayerExclusive { extern RecvTable g_RecvTable; }

//-----------------------------------------------------------------------------
// Purpose: Base Player class
//-----------------------------------------------------------------------------
// named by the From<> annotations below, so they cannot be class statics
void RecvProxy_LocalVelocityX( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_LocalVelocityY( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_LocalVelocityZ( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CBasePlayer : public CBaseCombatCharacter, public CPlayerMoveCore
{
	[[= ks::reflect::NetTable{ .name = "DT_BasePlayer" } ]]
	[[= ks::reflect::SubTable<"localdata", &DT_LocalPlayerExclusive::g_RecvTable,
	nullptr, true>{} ]]
	[[= ks::reflect::From<"m_iHealth", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_lifeState", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iAmmo", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fFlags", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_hGroundEntity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_PlayerFog.m_hCtrl", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_iHealth", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_nNextThinkTick", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_lifeState", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_nWaterLevel", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_vecBaseVelocity", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.05 } >{} ]]
	[[= ks::reflect::PredFrom<"m_hGroundEntity", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_flFriction", ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_nNextThinkTick", ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_vecBaseVelocity", ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_nWaterLevel", ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_vecViewOffset",
	ks::reflect::Net{ .table = "DT_LocalPlayerExclusive", .index = 0 }>{} ]]
	[[= ks::reflect::From<"m_vecViewOffset",
	ks::reflect::Net{ .table = "DT_LocalPlayerExclusive", .index = 1 }>{} ]]
	[[= ks::reflect::From<"m_vecViewOffset",
	ks::reflect::Net{ .table = "DT_LocalPlayerExclusive", .index = 2 }>{} ]]
	[[= ks::reflect::From<"m_vecVelocity",
	ks::reflect::Net{ .table = "DT_LocalPlayerExclusive", .index = 0 },
	RecvProxy_LocalVelocityX>{} ]]
	[[= ks::reflect::From<"m_vecVelocity",
	ks::reflect::Net{ .table = "DT_LocalPlayerExclusive", .index = 1 },
	RecvProxy_LocalVelocityY>{} ]]
	[[= ks::reflect::From<"m_vecVelocity",
	ks::reflect::Net{ .table = "DT_LocalPlayerExclusive", .index = 2 },
	RecvProxy_LocalVelocityZ>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBasePlayer, CBaseCombatCharacter );

	// Names CPlayerMoveCore shares with the entity chain mean the entity's here.
	using CBaseCombatCharacter::GetFlags;
	using CBaseCombatCharacter::AddFlag;
	using CBaseCombatCharacter::RemoveFlag;
	using CBaseCombatCharacter::GetMoveType;
	using CBaseCombatCharacter::GetMoveCollide;
	using CBaseCombatCharacter::SetMoveCollide;
	using CBaseCombatCharacter::GetBaseVelocity;
	using CBaseCombatCharacter::SetBaseVelocity;
	using CBaseCombatCharacter::GetWaterLevel;
	using CBaseCombatCharacter::SetWaterLevel;
	using CBaseCombatCharacter::GetWaterType;
	using CBaseCombatCharacter::SetWaterType;
	using CBaseCombatCharacter::GetGravity;
	using CBaseCombatCharacter::SetGravity;
	using CBaseCombatCharacter::GetAbsVelocity;
	using CBaseCombatCharacter::entindex;
	using CBaseCombatCharacter::IsServer;
	using CBaseCombatCharacter::GetGroundEntity;
	using CBaseCombatCharacter::SetGroundEntity;
	using CBaseCombatCharacter::SetMoveType;
	using CBaseCombatCharacter::GetViewOffset;
	using CBaseCombatCharacter::SetViewOffset;
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	CBasePlayer();
	virtual			~CBasePlayer();

	virtual void	Spawn( void );
	virtual void	SharedSpawn(); // Shared between client and server.
	virtual bool	GetPlayerID( CPlayerId *pID );
	virtual void	UpdateOnRemove( void );
	Class_T		Classify( void ) { return CLASS_PLAYER; }

	// IClientEntity overrides.
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	OnTimeJump();
	static void OnTimeJumpAllPlayers();
	
	virtual void	ClientThink( ) override;

	virtual void	ReceiveMessage( int classID, bf_read &msg );

	virtual void	OnRestore();

	virtual void	MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );

	virtual void	GetToolRecordingState( KeyValues *msg );

	virtual float GetPlayerMaxSpeed();
	
	void	SetAnimationExtension( const char *pExtension );

	CBaseViewModel				*GetViewModel( int viewmodelindex = 0 ) const;
	CBaseCombatWeapon	*GetActiveWeapon( void ) const;
	const char			*GetTracerType( void );

	// View model prediction setup
	virtual void		CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	virtual void		CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles);
	
	// Handle view smoothing when going up stairs
	void				SmoothViewOnStairs( Vector& eyeOrigin );
	virtual float		CalcRoll (const QAngle& angles, const Vector& velocity, float rollangle, float rollspeed);
	void				CalcViewRoll( QAngle& eyeAngles );
	virtual void		CalcViewBob( Vector& eyeOrigin );
	virtual void		CalcAddViewmodelCameraAnimation( Vector& eyeOrigin, QAngle& eyeAngles );

	virtual void			SetPlayerUnderwater( bool state );
	void					UpdateUnderwaterState( void );
	bool					IsPlayerUnderwater( void ) { return m_bPlayerUnderwater; }

	virtual	CBaseCombatCharacter *ActivePlayerCombatCharacter( void ) { return this; }

	virtual Vector			Weapon_ShootPosition();
	virtual bool			Weapon_CanUse( CBaseCombatWeapon *pWeapon );
	virtual void			Weapon_DropPrimary( void ) {}

	virtual Vector			GetAutoaimVector( float flScale );
	void					SetSuitUpdate(const char *name, int fgroup, int iNoRepeat);

	// Input handling
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual void	AvoidPhysicsProps( CUserCmd *pCmd );
	
	virtual void	PlayerUse( void );
	CBaseEntity		*FindUseEntity( void );
	virtual bool	IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );

	// Data handlers
	virtual bool	IsPlayer( void ) const { return true; }
	virtual int		GetHealth() const { return m_iHealth; }

	int		GetBonusProgress() const { return m_iBonusProgress; }
	int		GetBonusChallenge() const { return m_iBonusChallenge; }

	// observer mode
	virtual int			GetObserverMode() const;
	virtual CBaseEntity	*GetObserverTarget() const;
	virtual void		SetObserverTarget( EHANDLE hObserverTarget );

	bool			AudioStateIsUnderwater( Vector vecMainViewOrigin );

	bool IsObserver() const;
	bool IsCameraMan() const;
	bool IsActiveCameraMan() const { return m_bActiveCameraMan; }
	bool IsHLTV() const;
	void ResetObserverMode();
	bool IsBot( void ) const; 

	// Eye position..
	virtual Vector		 EyePosition();
	virtual const QAngle &EyeAngles();		// Direction of eyes
	void				 EyePositionAndVectors( Vector *pPosition, Vector *pForward, Vector *pRight, Vector *pUp );
	virtual const QAngle &LocalEyeAngles();		// Direction of eyes
	
	// This can be overridden to return something other than m_pRagdoll if the mod uses separate 
	// entities for ragdolls.
	virtual IRagdoll* GetRepresentativeRagdoll() const;

	// override the initial bone position for ragdolls
	virtual void GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );

	// Returns eye vectors
	void			EyeVectors( Vector *pForward, Vector *pRight = nullptr, Vector *pUp = nullptr );

	bool			IsSuitEquipped( void ) { return m_Local.m_bWearingSuit; };

	// Team handlers
	virtual void	TeamChange( int iNewTeam );

	// Flashlight
	void	Flashlight( void );
	void	UpdateFlashlight( void );
	void	TurnOffFlashlight( void );	// TERROR
	virtual const char *GetFlashlightTextureName( void ) const { return nullptr; } // TERROR
	virtual float GetFlashlightFOV( void ) const { return 0.0f; } // TERROR
	virtual float GetFlashlightFarZ( void ) const { return 0.0f; } // TERROR
	virtual float GetFlashlightLinearAtten( void ) const { return 0.0f; } // TERROR
	virtual bool CastsFlashlightShadows( void ) const { return true; } // TERROR
	virtual void GetFlashlightOffset( const Vector &vecForward, const Vector &vecRight, const Vector &vecUp, Vector *pVecOffset ) const;
	Vector	m_vecFlashlightOrigin;
	Vector	m_vecFlashlightForward;
	Vector	m_vecFlashlightUp;
	Vector	m_vecFlashlightRight;

	// Weapon selection code
	virtual bool				IsAllowedToSwitchWeapons( void ) { return !IsObserver(); }
	virtual CBaseCombatWeapon	*GetActiveWeaponForSelection( void );

	// Returns the view model if this is the local player. If you're in third person or 
	// this is a remote player, it returns the active weapon
	// (and its appropriate left/right weapon if this is TF2).
	virtual CBaseAnimating*	GetRenderedWeaponModel();

	virtual bool				IsOverridingViewmodel( void ) { return false; };
	virtual int					DrawOverriddenViewmodel( CBaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance ) { return 0; };

	virtual float				GetDefaultAnimSpeed( void ) { return 1.0; }

	void						SetMaxSpeed( float flMaxSpeed ) { m_flMaxspeed = flMaxSpeed; }
	float						MaxSpeed() const		{ return m_flMaxspeed; }

	// Should this object cast shadows?
	virtual ShadowType_t		ShadowCastType() { return SHADOWS_NONE; }

	virtual bool				ShouldReceiveProjectedTextures( int flags )
	{
		return false;
	}

	// Makes sure s_pLocalPlayer is properly initialized
	void						CheckForLocalPlayer( int nSplitScreenSlot );
	void						SetAsLocalPlayer();

	/// Is the passed in player one of the split screen users
	static bool					IsLocalPlayer( const CBaseEntity *pl );
	/// is this player a local player ( call when you have already verified that your pointer really is a CBasePlayer )
	inline bool					IsLocalPlayer( void ) const;

	// Global/static methods
	virtual void				ThirdPersonSwitch( bool bThirdperson );
	bool						ShouldDrawLocalPlayer();
	static CBasePlayer			*GetLocalPlayer( int nSlot = -1 );
	static void					SetRemoteSplitScreenPlayerViewsAreLocalPlayer( bool bSet ); //if true, calls to GetLocalPlayer() will return a remote splitscreen player when applicable.
	static bool					HasAnyLocalPlayer();
	static int					GetSplitScreenSlotForPlayer( CBaseEntity *pl );

	void						AddSplitScreenPlayer( CBasePlayer *pOther );
	void						RemoveSplitScreenPlayer( CBasePlayer *pOther );
	std::vector< CHandle< CBasePlayer > >& GetSplitScreenPlayers( void );
	void						AddPictureInPicturePlayer( CBasePlayer *pOther );
	void						RemovePictureInPicturePlayer( CBasePlayer *pOther );
	std::vector< CHandle< CBasePlayer > >& GetSplitScreenAndPictureInPicturePlayers( void );
	std::vector< CHandle< CBasePlayer > >& GetPictureInPicturePlayers( void );

	bool						IsSplitScreenPartner( CBasePlayer *pPlayer );

	bool						IsSplitScreenPlayer() const;
	int							GetSplitScreenPlayerSlot();
	bool						HasAttachedSplitScreenPlayers() const;

	virtual IClientModelRenderable*	GetClientModelRenderable();
	
	CrossPlayPlatform_t			GetCrossPlayPlatform( void ) const;

	virtual bool				PreRender( int nSplitScreenPlayerSlot );

	int							GetUserID( void ) const;
	virtual bool				CanSetSoundMixer( void );

	// return the entity used for soundscape radius checks
	virtual CBaseEntity		*GetSoundscapeListener();

	virtual int					GetLastKillerIndex() { return 0; }

	void						AddToPlayerSimulationList( CBaseEntity *other );
	void						SimulatePlayerSimulatedEntities( void );
	void						RemoveFromPlayerSimulationList( CBaseEntity *ent );
	void						ClearPlayerSimulationList( void );

	virtual void				PhysicsSimulate( void );
	virtual void				VPhysicsShadowUpdate( IPhysicsObject *pPhysics );
	virtual bool				IsFollowingPhysics( void ) { return false; }
	bool						IsRideablePhysics( IPhysicsObject *pPhysics );
	IPhysicsObject				*GetGroundVPhysics();
	void						UpdatePhysicsShadowToCurrentPosition( void );
	void						UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival );
	void						UpdatePhysicsShadowToPosition( const Vector &vecAbsOrigin );
	void						PostThinkVPhysics( void );
	void						SetTouchedPhysics( bool bTouch );
	bool						TouchedPhysics( void );
	void						SetPhysicsFlag( int nFlag, bool bSet );
	bool						HasPhysicsFlag( unsigned int flag ) { return (m_afPhysicsFlags & flag) != 0; }
	void						SetVCollisionState( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, int collisionState );
	virtual unsigned int		PhysicsSolidMaskForEntity( void ) const { return MASK_PLAYERSOLID; }
	void						PhysicsTouchTriggers( const Vector *pPrevAbsOrigin = nullptr ); // prediction calls it on CBasePlayer object

	// Prediction stuff
	virtual bool				ShouldPredict( void );
	virtual CBasePlayer		*GetPredictionOwner( void );

	virtual void				PreThink( void );
	virtual void				PostThink( void );

	virtual void				ItemPreFrame( void );
	virtual void				ItemPostFrame( void );
	virtual void				AbortReload( void );

	virtual void				SelectLastItem(void);
	virtual void				Weapon_SetLast( CBaseCombatWeapon *pWeapon );
	virtual bool				Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon ) { return true; }
	virtual bool				Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon );
	virtual	void				Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ );
	virtual	bool				Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	virtual CBaseCombatWeapon *GetLastWeapon( void ) { return m_hLastWeapon.Get(); }
	void						ResetAutoaim( void );
	virtual void 				SelectItem( const char *pstr, int iSubType = 0 );

	virtual void				UpdateClientData( void );

	virtual float				GetFOV( void ) const;	
	virtual int					GetDefaultFOV( void ) const;
	virtual bool				IsZoomed( void )	{ return false; }
	bool						SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate = 0.0f, int iZoomStart = 0 );
	void						ClearZoomOwner( void );

	float						GetFOVDistanceAdjustFactor();

	virtual void				ViewPunch( const QAngle &angleOffset );
	void						ViewPunchReset( float tolerance = 0 );

	void						UpdateButtonState( int nUserCmdButtonMask );
	int							GetImpulse( void ) const;

	virtual bool				Simulate();

	virtual bool				ShouldInterpolate();

	virtual bool				ShouldDraw();
	virtual int					DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool				ShouldSuppressForSplitScreenPlayer( int nSlot );
	virtual const char *		GetPlayerModelName( void );

	// Called when not in tactical mode. Allows view to be overriden for things like driving a tank.
	virtual void				OverrideView( CViewSetup *pSetup );

	CBaseEntity				*GetViewEntity( void ) const { return m_hViewEntity; }

	// returns the player name
	const char *				GetPlayerName();
	virtual const Vector		GetPlayerMins( void ) const; // uses local player
	virtual const Vector		GetPlayerMaxs( void ) const; // uses local player

	virtual void				UpdateCollisionBounds( void );

	// Is the player dead?
	bool				IsPlayerDead();
	bool				IsPoisoned( void ) { return m_Local.m_bPoisoned; }

	virtual CBaseEntity* GetUseEntity( void ) const;
	virtual CBaseEntity* GetPotentialUseEntity( void ) const;

	// Vehicles...

	virtual void			SetAnimation( PLAYER_ANIM playerAnim );

	float					GetTimeBase( void ) const;
	float					GetFinalPredictedTime() const;
	float					PredictedServerTime() const;

	float					m_fLastUpdateServerTime;
	int						m_nLastUpdateTickBase;
	int						m_nLastUpdateServerTickCount;

	bool					IsInVGuiInputMode() const;
	bool					IsInViewModelVGuiInputMode() const;

	CCommandContext		*GetCommandContext();

	// Get the command number associated with the current usercmd we're running (if in predicted code).
	int CurrentCommandNumber() const;
	const CUserCmd *GetCurrentUserCommand() const;
	CUserCmd const *GetLastUserCommand( void );

	virtual QAngle			GetViewPunchAngle();
	void					SetViewPunchAngle( const QAngle &angle );
	virtual QAngle			GetAimPunchAngle();
	void					SetAimPunchAngle( const QAngle &angle );
	void					SetAimPunchAngleVelocity( const QAngle &punchAngleVelocity );
	
	// TrackIR
	const Vector&		GetEyeOffset() const;
	void				SetEyeOffset( const Vector& v );

	const QAngle &		GetEyeAngleOffset() const;
	void				SetEyeAngleOffset( const QAngle & qa );
	// TrackIR

	const Vector &		GetAimDirection() const;
	void				SetAimDirection( const Vector & v );

	bool					IsCoach( void ) const;
	int						GetCoachingTeam( void ) const;

	// This supercedes GetTeamNumber(). It returns the same value if the user is a T or CT.
	// If the player is a coach it returns coaching team.
	int						GetAssociatedTeamNumber( void ) const;	

	// Is TEAM_SPECTATOR and is not a coach.
	// Use this when you want true omniscient spectators and not team coaches.
	bool					IsSpectator( void ) const; 

	// Returns the eye or pointer angle plus the punch angle.
	QAngle					GetFinalAimAngle();

	float					GetWaterJumpTime() const;
	void					SetWaterJumpTime( float flWaterJumpTime );
	float					GetSwimSoundTime( void ) const;
	void					SetSwimSoundTime( float flSwimSoundTime );

	float					GetNextDecalTime( void ) const { return m_flNextDecalTime; }
	float					GetDeathTime( void ) { return m_flDeathTime; }
	float					GetForceTeamTime( void ) { return m_fForceTeam; }

	void		SetPreviouslyPredictedOrigin( const Vector &vecAbsOrigin );
	const Vector &GetPreviouslyPredictedOrigin() const;

	// CS wants to allow small FOVs for zoomed-in AWPs.
	virtual float GetMinFOV() const;

	virtual void DoMuzzleFlash();

	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual surfacedata_t * GetFootstepSurface( const Vector &origin, const char *surfaceName );
	virtual void GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking );

	// Called by prediction when it detects a prediction correction.
	// vDelta is the line from where the client had predicted the player to at the usercmd in question,
	// to where the server says the client should be at said usercmd.
	void NotePredictionError( const Vector &vDelta );
	
	// Called by the renderer to apply the prediction error smoothing.
	void GetPredictionErrorSmoothingVector( Vector &vOffset ); 

	virtual void ExitLadder() {}
	surfacedata_t *GetLadderSurface( const Vector &origin );

	void	ForceButtons( int nButtons );
	void	UnforceButtons( int nButtons );

	void SetLadderNormal( Vector vecLadderNormal ) { m_vecLadderNormal = vecLadderNormal; }
	const Vector &GetLadderNormal( void ) const { return m_vecLadderNormal; }
	int GetLadderSurfaceProps( void ) const { return m_ladderSurfaceProps; }

	// Hints
	virtual CHintSystem		*Hints( void ) { return nullptr; }
	bool					ShouldShowHints( void ) { return Hints() ? Hints()->ShouldShowHints() : false; }
	bool 					HintMessage( int hint, bool bForce = false, bool bOnlyIfClear = false ) { return Hints() ? Hints()->HintMessage( hint, bForce, bOnlyIfClear ) : false; }
	void 					HintMessage( const char *pMessage ) { if (Hints()) Hints()->HintMessage( pMessage ); }

	virtual	IMaterial			*GetHeadLabelMaterial( void );
	virtual void				UpdateSpeechVOIP( bool bVoice );
	virtual bool				ShouldShowVOIPIcon() const;
	virtual const char			*GetVOIPParticleEffectName() const { return /*TODO: make real effect*/"impact_physics_dust"; }
	virtual CNewParticleEffect	*GetVOIPParticleEffect( void );
	virtual bool				IsPlayerTalkingOverVOIP() { return m_bPlayerIsTalkingOverVOIP; }
	bool						m_bPlayerIsTalkingOverVOIP;

	void SetLastKillerDamageAndFreezeframe( int nLastKillerDamageTaken, int nLastKillerHitsTaken, int nLastKillerDamageGiven, int nLastKillerHitsGiven );

	// Fog
	virtual fogparams_t		*GetFogParams( void ) { return &m_CurrentFog; }
	void					FogControllerChanged( bool bSnap );
	void					UpdateFogController( void );
	void					UpdateFogBlend( void );

	CPostProcessController* GetActivePostProcessController() const;
	CColorCorrection*		GetActiveColorCorrection() const;

	void					IncrementEFNoInterpParity();
	int						GetEFNoInterpParity() const;

	float					GetFOVTime( void ){ return m_flFOVTime; }

	virtual PlayerRenderMode_t GetPlayerRenderMode( int nSlot );

	virtual void			OnAchievementAchieved( int iAchievement ) {}

	bool					ShouldAnnounceAchievement( void ){ return m_flNextAchievementAnnounceTime < gpGlobals->curtime; }
	void					SetNextAchievementAnnounceTime( float flTime ){ m_flNextAchievementAnnounceTime = flTime; }
	virtual void			OnSwitchWeapons( CBaseCombatWeapon* pWeapon ){}

	bool					HasFiredWeapon( void ) { return m_bFiredWeapon; }
	void					SetFiredWeapon( bool bFlag ) { m_bFiredWeapon = bFlag; }

	CNetworkVar( int, m_iCoachingTeam, [[= ks::reflect::Net{} ]] );	// When on TEAM_SPECTATOR, is this player restricted to a team, aka 'coaching' a team

protected:
	fogparams_t				m_CurrentFog;
	EHANDLE					m_hOldFogController;

public:
	// RecvProxies
	
	static void RecvProxy_ObserverTarget( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_ObserverMode( const CRecvProxyData *pData, void *pStruct, void *pOut );

	void OnObserverModeChange( bool bIsObserverTarget = false );

	static void RecvProxy_LocalOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_LocalOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_NonLocalOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_NonLocalOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_NonLocalCellOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_NonLocalCellOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut );

	virtual bool ShouldRegenerateOriginFromCellBits() const;

	void SetUseEntity( CBaseEntity *pUseEntity );

public:

	CNetworkVar( float, m_flDuckAmount, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flDuckSpeed, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	Vector2D m_vecLastPositionAtFullCrouchSpeed;

	// Data for only the local player
	CNetworkVarEmbedded( CPlayerLocalData, m_Local, [[= ks::reflect::Pred{} ]] [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] );

	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] EHANDLE m_hTonemapController;

	// Data common to all other players, too
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{} ]] CPlayerState			pl;

public:
// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)

// FTYPEDESC_INSENDTABLE STUFF
	// Player FOV values
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] int						m_iFOV;				// field of view
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = 0 } ]] int						m_iFOVStart;		// starting value of the FOV changing over time (client only)
	int						m_afButtonLast;
	int						m_afButtonPressed;
	int						m_afButtonReleased;
	int						m_nButtons;
protected:
	int						m_nImpulse;
	CNetworkVar( int, m_ladderSurfaceProps, [[= ks::reflect::Net{} ]] );
	int						m_flPhysics;
public:
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = 0 } ]] float					m_flFOVTime;		// starting time of the FOV zoom
private:
protected:
private:
	CNetworkVector( m_vecLadderNormal, [[= ks::reflect::Net{} ]] );

// FTYPEDESC_INSENDTABLE STUFF (end)
public:
	char					m_szAnimExtension[32];
private:
	int						m_nOldTickBase;
private:
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] int						m_iBonusProgress;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] int						m_iBonusChallenge;

private:
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.5f } ]] float					m_flMaxspeed;

public:
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] EHANDLE					m_hZoomOwner;		// This is a pointer to the entity currently controlling the player's zoom
protected:
	//HACKHACK: these 9 are only partially ported from server counterpart
	IPhysicsPlayerController	*m_pPhysicsController;
	IPhysicsObject				*m_pShadowStand;
	IPhysicsObject				*m_pShadowCrouch;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] int							m_vphysicsCollisionState;
	Vector						m_oldOrigin;
	bool						m_bTouchedPhysObject;
	bool						m_bPhysicsWasFrozen;
	Vector						m_vNewVPhysicsPosition;
	Vector						m_vNewVPhysicsVelocity;
	CUserCmd					m_LastCmd;

	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK } ]] unsigned int			m_afPhysicsFlags;
	typedef CHandle<CBaseCombatWeapon> CBaseCombatWeaponHandle;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] CBaseCombatWeaponHandle	m_hLastWeapon;
	// players own view models, left & right hand
	[[= ks::reflect::Net{ .varlen = true } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] CHandle< CBaseViewModel >	m_hViewModel[ MAX_VIEWMODELS ];	

	ks::tracked_ref< CNewParticleEffect > m_speechVOIPParticleEffect;

	bool m_bCanShowFreezeFrameNow;
public:
	int m_nLastKillerDamageTaken;
	int m_nLastKillerHitsTaken;
	int m_nLastKillerDamageGiven;
	int m_nLastKillerHitsGiven;
public:
	// For weapon prediction
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] bool					m_fOnTarget;		//Is the crosshair on a target?

	[[= ks::reflect::Net{} ]] EHANDLE			m_hUseEntity;

// END PREDICTION DATA COMPACTION
public:

	[[= ks::reflect::Net{} ]] int						m_iDefaultFOV;		// default FOV if no other zooms are occurring
												// Only this entity can change the zoom state once it has ownership
	int				m_afButtonForced;	// These are forced onto the player's inputs

	CUserCmd		*m_pCurrentCommand;

	[[= ks::reflect::Net{} ]] EHANDLE			m_hViewEntity;
	[[= ks::reflect::Net{} ]] bool			m_bShouldDrawPlayerWhileUsingViewEntity;

	// Movement constraints
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] EHANDLE m_hConstraintEntity;
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] Vector m_vecConstraintCenter;
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_flConstraintRadius;
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_flConstraintWidth;
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_flConstraintSpeedFactor;
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] bool m_bConstraintPastRadius;

	CUserMessageBinder m_UMCMsg_SendLastKillerDamageToClient;

	// Record hits on client and server for comparison.
	int m_totalHitsOnClient; 

	[[= ks::reflect::Net{} ]] int			m_iDeathPostEffect;

protected:

	virtual void		CalcPlayerView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual void		CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual Vector		GetChaseCamViewOffset( CBaseEntity *target );
	void				CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void				CalcInEyeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual float		GetDeathCamInterpolationTime();
	virtual void		CalcDeathCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual void		CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov);
	virtual void		CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );

	// Check to see if we're in vgui input mode...
	void DetermineVguiInputMode( CUserCmd *pCmd );

	// Used by prediction, sets the view angles for the player
	virtual void SetLocalViewAngles( const QAngle &viewAngles );
	virtual void SetViewAngles( const QAngle& ang );

	// used by client side player footsteps 
	surfacedata_t* GetGroundSurface();

protected:
	// Did we just enter a vehicle this frame?

// DATA
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<CBasePlayer::RecvProxy_ObserverMode, ks::reflect::WireSide::Recv>{} ]] int				m_iObserverMode;	// if in spectator mode != 0
	[[= ks::reflect::Net{} ]] bool			m_bActiveCameraMan;
	[[= ks::reflect::Net{} ]] bool			m_bCameraManXRay;
	[[= ks::reflect::Net{} ]] bool			m_bCameraManOverview;
	[[= ks::reflect::Net{} ]] bool			m_bCameraManScoreBoard;
	[[= ks::reflect::Net{} ]] uint8			m_uCameraManGraphs;
	bool			m_bLastActiveCameraManState;
	bool			m_bLastCameraManXRayState;
	bool			m_bLastCameraManOverviewState;
	bool			m_bLastCameraManScoreBoardState;
	uint8			m_uLastCameraManGraphsState;
	int				m_iOldObserverMode;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<CBasePlayer::RecvProxy_ObserverTarget, ks::reflect::WireSide::Recv>{} ]] EHANDLE			m_hObserverTarget;	// current observer target
	float			m_flObserverChaseDistance; // last distance to observer traget
	float			m_flObserverChaseApproach;
	Vector			m_vecObserverEyeDirPrevious;
	Vector			m_vecFreezeFrameStart;
	float			m_flFreezeFrameStartTime;	// Time at which we entered freeze frame observer mode
	float			m_flFreezeFrameDistance;
	bool			m_bWasFreezeFraming; 
	float			m_flFreezePanelExtendedStartTime;
	bool			m_bWasFreezePanelExtended;
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_flDeathTime;		// last time player died
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_flNextDecalTime;	// next time player can paint a decal
	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_fForceTeam;		// last time player died
	CDiscontinuousInterpolatedVar< Vector >	m_iv_vecViewOffset;

private:
	// Make sure no one calls this...
	CBasePlayer& operator=( const CBasePlayer& src );
	CBasePlayer( const CBasePlayer & ); // not defined, not accessible

	// Vehicle stuff.
	
	// Not replicated

protected:
	QAngle			m_vecOldViewAngles;

private:
	bool			m_bWasFrozen;

	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] int				m_nTickBase;
	int				m_nFinalPredictedTick;

	EHANDLE			m_pCurrentVguiScreen;

	bool			m_bFiredWeapon;

	// Player flashlight dynamic light pointers
	bool			m_bFlashlightEnabled[ MAX_SPLITSCREEN_PLAYERS ];

	std::vector< CHandle< CBaseEntity > > m_SimulatedByThisPlayer;
	
	float					m_flOldPlayerZ;
	float					m_flOldPlayerViewOffsetZ;
	

	// For UI purposes...
	int				m_iOldAmmo[ MAX_AMMO_TYPES ];

	CCommandContext		m_CommandContext;

	bool							m_bPlayerUnderwater;

	friend class CPrediction;
	friend class CASW_Prediction;
	friend class CDOTAPrediction;

	// HACK FOR TF2 Prediction
	friend class CTFGameMovementRecon;
	friend class CTFGameMovement;
	friend class CCSGameMovement;
	friend class CHL2GameMovement;
	friend class CPortalGameMovement;
	friend class CASW_MarineGameMovement;
	
	// Accessors for gamemovement
	float GetStepSize( void ) const { return m_Local.m_flStepSize; }

	float m_flNextAvoidanceTime;
	float m_flAvoidanceRight;
	float m_flAvoidanceForward;
	float m_flAvoidanceDotForward;
	float m_flAvoidanceDotRight;

protected:
	virtual bool IsDucked( void ) const { return m_Local.m_bDucked; }
	virtual bool IsDucking( void ) const { return m_Local.m_bDucking; }
	virtual float GetFallVelocity( void ) { return m_Local.m_flFallVelocity; }
	void ForceSetupBonesAtTimeFakeInterpolation( matrix3x4a_t *pBonesOut, float curtimeOffset );

	[[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] float m_flLaggedMovementValue;

	// These are used to smooth out prediction corrections. They're most useful when colliding with
	// vphysics objects. The server will be sending constant prediction corrections, and these can help
	// the errors not be so jerky.
	Vector m_vecPredictionError;
	float m_flPredictionErrorTime;
	
	Vector m_vecPreviouslyPredictedOrigin; // Used to determine if non-gamemovement game code has teleported, or tweaked the player's origin

	[[= ks::reflect::Net{} ]] char m_szLastPlaceName[MAX_PLACE_NAME_LENGTH];	// received from the server

	// Texture names and surface data, used by CGameMovement

	bool			m_bStartedFreezeFrame;
	bool			m_bAbortedFreezeFrame;
	bool			m_bSentFreezeFrame;
	float			m_flFreezeZOffset;
	[[= ks::reflect::Net{} ]] byte			m_ubEFNoInterpParity;
	byte			m_ubOldEFNoInterpParity;

	float			m_flNextAchievementAnnounceTime;

	// If we have any attached split users, this is the list of them
	std::vector< CHandle< CBasePlayer > > m_hSplitScreenPlayers;
	std::vector< CHandle< CBasePlayer > > m_hSplitScreenAndPipPlayers;
	std::vector< CHandle< CBasePlayer > > m_hPipPlayers;
	int						m_nSplitScreenSlot; //-1 == not a split player
	CHandle< CBasePlayer > m_hSplitOwner;
	bool					m_bIsLocalPlayer;

private:

	struct StepSoundCache_t
	{
		StepSoundCache_t() : m_usSoundNameIndex( 0 ) {}
		CSoundParameters	m_SoundParameters;
		unsigned short		m_usSoundNameIndex;
	};
	// One for left and one for right side of step
	StepSoundCache_t		m_StepSoundCache[ 2 ];

public:
	// HACK: Only used for cstrike players, making virtual here because a ton of base player code needs to know about that state. 
	enum eObserverInterpState
	{
		OBSERVER_INTERP_NONE,			// Not interpolating
		OBSERVER_INTERP_TRAVELING,		// Camera moving quickly towards target, hide most 1st person effects
		OBSERVER_INTERP_SETTLING,		// Camera very close to final position but still interpolating (to avoid a pop)... draw viewmodel/scope state but keep interpolating
	};
	virtual eObserverInterpState GetObserverInterpState( void ) const { return OBSERVER_INTERP_NONE; }
	virtual bool IsInObserverInterpolation( void ) const { return false; }

	const char *GetLastKnownPlaceName( void ) const	{ return m_szLastPlaceName; }	// return the last nav place name the player occupied

	float GetLaggedMovementValue( void ){ return m_flLaggedMovementValue;	}
	bool  ShouldGoSouth( Vector vNPCForward, Vector vNPCRight ); //Such a bad name.

	void SetOldPlayerZ( float flOld ) { m_flOldPlayerZ = flOld;	}
	
	const fogplayerparams_t& GetPlayerFog() const { return m_PlayerFog; }

private:
	friend class CMoveHelperClient;

	CNetworkHandle( CPostProcessController, m_hPostProcessCtrl, [[= ks::reflect::Net{} ]] );	// active postprocessing controller
	CNetworkHandle( CColorCorrection, m_hColorCorrectionCtrl, [[= ks::reflect::Net{} ]] );		// active FXVolume color correction

	// fog params
	fogplayerparams_t		m_PlayerFog;

	float m_flTimeLastTouchedGround;

public:
	float GetAirTime( void );
	virtual bool IsHoldingTaunt( void ) const { return false; }
	virtual bool IsHoldingLookAtWeapon( void ) const { return false; }

private:
	void UpdateSplitScreenAndPictureInPicturePlayerList();

	//HACK: always contains the last origin we received through CBasePlayer::RecvProxy_LocalOriginXY() & CBasePlayer::RecvProxy_LocalOriginZ(). Intended to fix bug 85693 without as small a scale change as possible
	//only works because we receive both the local and nonlocal representations of our origin on recreation. It just happens that the nonlocal wins out by default because it comes last
	Vector m_vecHack_RecvProxy_LocalPlayerOrigin; 

private:
	// Eye/angle offsets - used for TrackIR and motion controllers
	Vector							m_vecEyeOffset;
	QAngle							m_EyeAngleOffset;
	Vector							m_AimDirection;
};

EXTERN_RECV_TABLE(DT_BasePlayer);

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CBasePlayer *ToBasePlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	return static_cast<CBasePlayer *>( pEntity );
}

inline const CBasePlayer *ToBasePlayer( const CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	return static_cast<const CBasePlayer *>( pEntity );
}

extern bool g_bEngineIsHLTV;

bool	CBasePlayer::IsLocalPlayer( void ) const
{
	return m_bIsLocalPlayer;
}

class CBaseHudChat;
class CBaseHudWeaponSelection;
class CNoopAnimController;
class CUserCmd;
class CBaseEntity;
class CBasePlayer;
class IClientMode;

using namespace ::ks::reflect;



inline constexpr int ACHIEVEMENT_ANNOUNCEMENT_MIN_TIME = 10;
template <typename T1> [[gnu::always_inline]] inline auto USERID2PLAYER( T1 i ) { return ToBasePlayer( ClientEntityList().GetEnt( engine->GetPlayerForUserID( i ) ) ); }

extern IClientMode *GetClientModeNormal(); // must be implemented
extern IClientMode *GetFullscreenClientMode();

// This class implements client mode functionality common to HL2 and TF2.
class ClientModeShared : public IClientMode, public CGameEventListener
{
// IClientMode overrides.
public:
	DECLARE_CLASS_NOBASE( ClientModeShared );

					ClientModeShared();
	virtual			~ClientModeShared();
	
	virtual void	Init();
	virtual void	InitViewport();
	virtual void	UI_Shutdown();
	virtual void	Shutdown();

	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );

	virtual void	Enable();
	virtual void	Disable();
	virtual void	Layout( bool bForce = false );

	virtual void	OverrideView( CViewSetup *pSetup );
	virtual bool	OverrideRenderBounds( int &x, int &y, int &w, int &h, int &insetX, int &insetY ) { return false; }
	virtual void	OverrideAudioState( AudioState_t *pAudioState ) { return; }
	virtual bool	ShouldDrawDetailObjects( );
	virtual bool	ShouldDrawEntity(CBaseEntity *pEnt);
	virtual bool	ShouldDrawLocalPlayer( CBasePlayer *pPlayer );
	virtual bool	ShouldDrawViewModel();
	virtual bool	ShouldDrawParticles( );
	virtual bool	ShouldDrawCrosshair( void );
	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual void	PreRender(CViewSetup *pSetup);
	virtual void	PostRender();
	virtual void	PostRenderVGui();
	virtual void	ProcessInput(bool bActive);
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *cmd );
	virtual void	Update();
	virtual void	SetBlurFade( float scale ) {}
	virtual float	GetBlurFade( void ) { return 0.0f; }

	// Input
	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual void	OverrideMouseInput( float *x, float *y );
	virtual void	StartMessageMode( int iMessageModeType );

	// The mode can choose to not draw fog
	virtual bool	ShouldDrawFog( void );
	
	virtual float	GetViewModelFOV( void );

	// Gets at the viewports vgui panel animation controller, if there is one...
	virtual CNoopAnimController *GetViewportAnimationController()
		{ return m_pViewport->GetAnimationController(); }
	
	void FireGameEvent( IGameEvent *event );

	virtual bool CanRecordDemo( char *errorMsg, int length ) const { return true; }

	virtual int HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual void InitChatHudElement( void );
	virtual void InitWeaponSelectionHudElement( void );

	virtual wchar_t*	GetServerName( void ) { return nullptr; }
	virtual void		SetServerName( wchar_t *name ) {}
	virtual wchar_t*	GetMapName( void ) { return nullptr; }
	virtual void		SetMapName( wchar_t *name ) {}

	virtual void	UpdateCameraManUIState( int iType, int nOptionalParam, uint64 xuid );
	virtual void	ScoreboardOff( void );
	virtual void	GraphPageChanged( void );

	CUserMessageBinder m_UMCMsgVGUIMenu;
	CUserMessageBinder m_UMCMsgRumble;

protected:
	CBaseViewport			*m_pViewport;

	int			GetSplitScreenPlayerSlot() const;

private:
	// Message mode handling
	// All modes share a common chat interface
	CBaseHudChat			*m_pChatElement;
	CBaseHudWeaponSelection *m_pWeaponSelection;
	int						m_nRootSize[2];
};

bool ContainsBinding( const char *pszBindingString, const char *pszBinding, bool bSearchAliases = true );

void SendCameraManUIStateChange( HltvUiType_t eventType, int nOptionalParam = 0 );

inline constexpr auto AE_TYPE_SERVER = ( 1 << 0 );
inline constexpr auto AE_TYPE_SCRIPTED = ( 1 << 1 ); // see scriptevent.h
inline constexpr auto AE_TYPE_SHARED = ( 1 << 2 );
inline constexpr auto AE_TYPE_WEAPON = ( 1 << 3 );
inline constexpr auto AE_TYPE_CLIENT = ( 1 << 4 );
inline constexpr auto AE_TYPE_FACEPOSER = ( 1 << 5 );

inline constexpr auto AE_TYPE_NEWEVENTSYSTEM = ( 1 << 10 ); //Temporary flag.

inline constexpr auto AE_NOT_AVAILABLE = -1;

typedef enum
{
	AE_INVALID = -1,			// So we have something more succint to check for than '-1'
	AE_EMPTY,
	AE_NPC_LEFTFOOT, // #define	NPC_EVENT_LEFTFOOT			2050
	AE_NPC_RIGHTFOOT, // #define NPC_EVENT_RIGHTFOOT			2051
	AE_NPC_BODYDROP_LIGHT, //#define NPC_EVENT_BODYDROP_LIGHT	2001
	AE_NPC_BODYDROP_HEAVY, //#define NPC_EVENT_BODYDROP_HEAVY	2002
	AE_NPC_SWISHSOUND, //#define NPC_EVENT_SWISHSOUND		2010
	AE_NPC_180TURN, //#define NPC_EVENT_180TURN			2020
	AE_NPC_ITEM_PICKUP, //#define NPC_EVENT_ITEM_PICKUP					2040
	AE_NPC_WEAPON_DROP, //#define NPC_EVENT_WEAPON_DROP					2041
	AE_NPC_WEAPON_SET_SEQUENCE_NAME, //#define NPC_EVENT_WEAPON_SET_SEQUENCE_NAME		2042
	AE_NPC_WEAPON_SET_SEQUENCE_NUMBER, //#define NPC_EVENT_WEAPON_SET_SEQUENCE_NUMBER	2043
	AE_NPC_WEAPON_SET_ACTIVITY, //#define NPC_EVENT_WEAPON_SET_ACTIVITY			2044
	AE_NPC_HOLSTER,
	AE_NPC_DRAW,
	AE_NPC_WEAPON_FIRE,

	AE_CL_PLAYSOUND, // #define CL_EVENT_SOUND				5004	// Emit a sound 
	AE_SV_PLAYSOUND,
	AE_CL_STOPSOUND,

	AE_START_SCRIPTED_EFFECT,
	AE_STOP_SCRIPTED_EFFECT,

	AE_CLIENT_EFFECT_ATTACH,
	
	AE_MUZZLEFLASH,			// Muzzle flash from weapons held by the player
	AE_NPC_MUZZLEFLASH,		// Muzzle flash from weapons held by NPCs
	
	AE_THUMPER_THUMP,		//Thumper Thump!
	AE_AMMOCRATE_PICKUP_AMMO,	//Ammo crate pick up ammo!

	AE_NPC_RAGDOLL,

	AE_NPC_ADDGESTURE,
	AE_NPC_RESTARTGESTURE,

	AE_NPC_ATTACK_BROADCAST,

	AE_NPC_HURT_INTERACTION_PARTNER,
	AE_NPC_SET_INTERACTION_CANTDIE,

	AE_SV_DUSTTRAIL,

	AE_CL_CREATE_PARTICLE_EFFECT,
	AE_CL_STOP_PARTICLE_EFFECT,
	AE_CL_ADD_PARTICLE_EFFECT_CP,
	AE_CL_CREATE_PARTICLE_BRASS,

	AE_RAGDOLL,

	AE_CL_ENABLE_BODYGROUP,
	AE_CL_DISABLE_BODYGROUP,
	AE_CL_BODYGROUP_SET_VALUE,
	AE_CL_BODYGROUP_SET_VALUE_CMODEL_WPN,

	AE_CL_BODYGROUP_SET_TO_CLIP,	// Updates the value of bodygroup 1 to the number of bullets in m_iClip1
	AE_CL_BODYGROUP_SET_TO_NEXTCLIP,	// Updates the value of bodygroup 1 to the number of bullets that can be reloaded next

	AE_CL_HIDE_SILENCER,
	AE_CL_ATTACH_SILENCER_COMPLETE,
	AE_CL_SHOW_SILENCER,
	AE_CL_DETACH_SILENCER_COMPLETE,

	AE_CL_EJECT_MAG, // Create a physical magazine to drop in the world and hide the fake-animated one
	AE_CL_EJECT_MAG_UNHIDE, //Unhide the non-physical mag that AE_CL_EJECT_MAG hid

	AE_WPN_PRIMARYATTACK,	// Used by weapons that want their primary attack to occur during an attack anim (i.e. grenade throwing)
	AE_WPN_INCREMENTAMMO,

	AE_WPN_HIDE,		// Used to hide player weapons
	AE_WPN_UNHIDE,		// Used to unhide player weapons

	AE_WPN_PLAYWPNSOUND,	// Play a weapon sound from the weapon script file

	AE_WPN_COMPLETE_RELOAD,	// the event that fires when a weapon has completed a gun reload visually according to the animation

	// Alien Swarm Infested shared events
	AE_ASW_FOOTSTEP,		// asw, played as each foot steps down
	AE_MARINE_FOOTSTEP,
	AE_MARINE_RELOAD_SOUND_A,	// anim event fired reloading sound
	AE_MARINE_RELOAD_SOUND_B,	// anim event fired reloading sound
	AE_MARINE_RELOAD_SOUND_C,	// anim event fired reloading sound
	AE_REMOVE_CLIENT_AIM,	// asw, removes this entity from the client autoaim list

	AE_MELEE_DAMAGE,
	AE_MELEE_START_COLLISION_DAMAGE,
	AE_MELEE_STOP_COLLISION_DAMAGE,
	AE_SCREEN_SHAKE,
	AE_START_DETECTING_COMBO,
	AE_STOP_DETECTING_COMBO,
	AE_COMBO_TRANSITION,
	AE_ALLOW_MOVEMENT,
	AE_SKILL_EVENT,				// marine skill event triggered (event options describes the skill)

	AE_TUG_INCAP,

	AE_CLIENT_EJECT_BRASS,

	AE_BEGIN_TAUNT_LOOP,

	AE_CL_SET_STATTRAK_GLOW,

	AE_WPN_CZ_DUMP_CURRENT_MAG,
	AE_WPN_CZ_UPDATE_BODYGROUP,

	AE_WPN_CLIP_TO_POSEPARAM,
	AE_WPN_NEXTCLIP_TO_POSEPARAM,
	AE_WPN_EMPTYSHOTS_TO_POSEPARAM,

	LAST_SHARED_ANIMEVENT,
} Animevent;

typedef struct evententry_s evententry_t;

//=========================================================
//=========================================================
extern void EventList_Init( void );
extern void EventList_Free( void );
extern bool EventList_RegisterSharedEvent( const char *pszEventName, int iEventIndex, int iType = 0 );
extern Animevent EventList_RegisterPrivateEvent( const char *pszEventName );
extern int EventList_IndexForName( const char *pszEventName );
extern const char *EventList_NameForIndex( int iEventIndex );
Animevent EventList_RegisterPrivateEvent( const char *pszEventName );

// This macro guarantees that the names of each event and the constant used to
// reference it in the code are identical.
#define REGISTER_PRIVATE_ANIMEVENT( _n ) _n = EventList_RegisterPrivateEvent( #_n );

// Implemented in shared code
extern void EventList_RegisterSharedEvents( void );
extern int EventList_GetEventType( int eventIndex );

#undef REGISTER_PRIVATE_ANIMEVENT

class CFireOverlay;

class CNewParticleEffect;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


void RecvProxy_Scale( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_ScaleTime( const CRecvProxyData *pData, void *pStruct, void *pOut );


class CFireSprite : public CSprite
{
	DECLARE_CLASS( CFireSprite, CSprite );

private:
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

public:
	Vector	m_vecMoveDir;
	bool	m_bFadeFromAbove;
};

class CFireFromAboveSprite : public CSprite
{
	DECLARE_CLASS( CFireFromAboveSprite, CSprite );

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
};

inline constexpr auto SMOKE_RISE_RATE = 92.0f;
inline constexpr auto SMOKE_LIFETIME = 2.0f;
inline constexpr auto EMBER_LIFETIME = 2.0f;

inline constexpr auto FLAME_CHILD_SPREAD = 64.0f;
inline constexpr auto FLAME_SOURCE_HEIGHT = 128.0f;
inline constexpr auto FLAME_FROM_ABOVE_SOURCE_HEIGHT = 32.0f;

//==================================================
// CFireSmoke
//==================================================

//NOTENOTE: Mirrored in dlls/fire_smoke.h
inline constexpr int bitsFIRESMOKE_SMOKE = 0x00000002;
inline constexpr int bitsFIRESMOKE_SMOKE_COLLISION = 0x00000004;
inline constexpr int bitsFIRESMOKE_GLOW = 0x00000008;
inline constexpr int bitsFIRESMOKE_VISIBLE_FROM_ABOVE = 0x00000010;

inline constexpr auto OVERLAY_MAX_VISIBLE_RANGE = 512.0f;

class CFireSmoke : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FireSmoke" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( CFireSmoke, CBaseEntity );

	CFireSmoke();
	~CFireSmoke();

	void	Start( void );

	void	StartClientOnly( void );
	void	RemoveClientOnly( void );

protected:
	void	Update( void );
	void	UpdateAnimation( void );
	void	UpdateScale( void );
	void	UpdateFlames( void );
	void	AddFlames( void );
	void	SpawnSmoke( void );
	void	FindClipPlane( void );
	
//CBaseEntity
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual bool	ShouldDraw();

	float GetScale( void ) const { return m_flScaleRegister;	}
	
//From the server
public:
	[[= ks::reflect::Net{} ]] float	m_flStartScale;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Scale, ks::reflect::WireSide::Recv>{} ]] float	m_flScale;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_ScaleTime, ks::reflect::WireSide::Recv>{} ]] float	m_flScaleTime;
	[[= ks::reflect::Net{} ]] int		m_nFlags;
	[[= ks::reflect::Net{} ]] int		m_nFlameModelIndex;
	[[= ks::reflect::Net{} ]] int		m_nFlameFromAboveModelIndex;

//Client-side only
public:
	float	m_flScaleRegister;
	float	m_flScaleStart;
	float	m_flScaleEnd;
	float	m_flScaleTimeStart;
	float	m_flScaleTimeEnd;
	float	m_flChildFlameSpread;

	VPlane	m_planeClip;
	float	m_flClipPerc;
	bool	m_bClipTested;

	bool	m_bFadingOut;

protected:

	void	UpdateEffects( void );

	//ks::intrusive_ptr<CEmberEffect> m_pEmberEmitter;

	CFireSprite			m_entFlames[NUM_CHILD_FLAMES];
	CFireFromAboveSprite	m_entFlamesFromAbove[NUM_CHILD_FLAMES];
	float					m_entFlameScales[NUM_CHILD_FLAMES];

	TimedEvent			m_tParticleSpawn;

	CFireOverlay		*m_pFireOverlay;

	// New Particle Fire Effect
	ks::tracked_ref<CNewParticleEffect> m_hEffect;
private:
	CFireSmoke( const CFireSmoke & );
};

//Fire overlay
class CFireOverlay : public CGlowOverlay
{
public:
	
	//Constructor
	explicit CFireOverlay( CFireSmoke *owner );

	//-----------------------------------------------------------------------------
	// Purpose: Generate a flicker value
	// Output : scalar value
	//-----------------------------------------------------------------------------
	float GetFlickerScale( void )
	{
		float	result;

		float	time = Helper_GetTime() + m_nGUID;

		result = std::sin( time * 1000.0f );
		result += 0.5f * sin( time * 2000.0f );
		result -= 0.5f * std::cos( time * 8000.0f );
		
		return result;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Update the overlay
	//-----------------------------------------------------------------------------
	virtual bool Update( void )
	{
		if ( m_pOwner == nullptr )
			return false;

		float scale	 = m_pOwner->GetScale();
		float dscale = scale - m_flScale;

		m_vPos[2] += dscale * FLAME_SOURCE_HEIGHT;
		m_flScale = scale;

		scale *= 0.75f;

		float flickerScale = GetFlickerScale();

		float newScale = scale + ( scale * flickerScale * 0.1f );

		m_Sprites[0].m_flHorzSize = ( newScale * 0.2f ) + ( m_Sprites[0].m_flHorzSize * 0.8f );
		m_Sprites[0].m_flVertSize = m_Sprites[0].m_flHorzSize * 1.5f;
		
		float	cameraDistance = ( CurrentViewOrigin() - (m_pOwner->GetAbsOrigin())).Length();

		CBasePlayer *local = CBasePlayer::GetLocalPlayer();
		if ( local )
		{
			cameraDistance *= local->GetFOVDistanceAdjustFactor();
		}

		if ( cameraDistance > OVERLAY_MAX_VISIBLE_RANGE )
			cameraDistance = OVERLAY_MAX_VISIBLE_RANGE;

		float alpha = 1.0f - ( cameraDistance / OVERLAY_MAX_VISIBLE_RANGE );

		Vector	newColor = m_vBaseColors[0] + ( m_vBaseColors[0] * flickerScale * 0.5f );
		m_Sprites[0].m_vColor = ( newColor * 0.1f ) + ( m_Sprites[0].m_vColor * 0.9f ) * alpha;

		return true;
	}

public:

	CFireSmoke	*m_pOwner;
	Vector		m_vBaseColors[MAX_SUN_LAYERS];
	float		m_flScale;
	int			m_nGUID;
};

using namespace ::ks::reflect;

class CFlashlightEffect
{
public:

	CFlashlightEffect(int nEntIndex = 0, const char *pszTextureName = nullptr, float flFov = 0.0f, float flFarZ = 0.0f, float flLinearAtten = 0.0f );
	~CFlashlightEffect();

	void UpdateLight( int nEntIdx, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov, 
						float flFarZ, float flLinearAtten, bool castsShadows, const char* pTextureName );
	void UpdateLight(	int nEntIdx, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov,
						bool castsShadows, ITexture *pFlashlightTexture, const Vector &vecBrightness, bool bTracePlayers = true );

	void TurnOn();
	void TurnOff();
	void SetMuzzleFlashEnabled( bool bEnabled, float flBrightness );
	bool IsOn( void ) { return m_bIsOn;	}

	ClientShadowHandle_t GetFlashlightHandle( void ) { return m_FlashlightHandle; }
	void SetFlashlightHandle( ClientShadowHandle_t Handle ) { m_FlashlightHandle = Handle;	}

	const char *GetFlashlightTextureName( void ) const
	{
		return m_textureName;
	}

	int GetEntIndex( void ) const
	{
		return m_nEntIndex;
	}

protected:

	bool UpdateDefaultFlashlightState(	FlashlightState_t& state, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight,
										const Vector &vecUp, bool castsShadows, bool bTracePlayers = true );
	bool ComputeLightPosAndOrientation( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp,
										Vector& vecFinalPos, Quaternion& quatOrientation, bool bTracePlayers );
	void LightOff();

	void UpdateFlashlightTexture( const char* pTextureName );
	void UpdateLightTopDown(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp);

	bool m_bIsOn;
	int m_nEntIndex;
	ClientShadowHandle_t m_FlashlightHandle;

	bool m_bMuzzleFlashEnabled;
	float m_flMuzzleFlashBrightness;

	float m_flFov;
	float m_flFarZ;
	float m_flLinearAtten;
	bool  m_bCastsShadows;

	float m_flCurrentPullBackDist;

	// Texture for flashlight
	CTextureReference m_FlashlightTexture;

	// Texture for muzzle flash
	CTextureReference m_MuzzleFlashTexture;

	char m_textureName[64];
};

class CHeadlightEffect : public CFlashlightEffect
{
public:
	
	CHeadlightEffect();
	~CHeadlightEffect();

	virtual void UpdateLight(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance);
};

class CFlashlightEffectManager
{
private:
	CFlashlightEffect *m_pFlashlightEffect;
	const char *m_pFlashlightTextureName;
	int m_nFlashlightEntIndex;
	float m_flFov;
	float m_flFarZ;
	float m_flLinearAtten;
	int m_nMuzzleFlashFrameCountdown;
	CountdownTimer m_muzzleFlashTimer;
	float m_flMuzzleFlashBrightness;
	bool m_bFlashlightOn;
	int m_nFXComputeFrame;
	bool m_bFlashlightOverride;

public:
	CFlashlightEffectManager() : m_pFlashlightEffect( nullptr ), m_pFlashlightTextureName( nullptr ), m_nFlashlightEntIndex( -1 ), m_flFov( 0.0f ),
								m_flFarZ( 0.0f ), m_flLinearAtten( 0.0f ), m_nMuzzleFlashFrameCountdown( 0 ), m_flMuzzleFlashBrightness( 1.0f ),
								m_bFlashlightOn( false ), m_nFXComputeFrame( -1 ), m_bFlashlightOverride( false ) {}

	void TurnOnFlashlight( int nEntIndex = 0, const char *pszTextureName = nullptr, float flFov = 0.0f, float flFarZ = 0.0f, float flLinearAtten = 0.0f );

	void TurnOffFlashlight( bool bForce = false );

	bool IsFlashlightOn() const { return m_bFlashlightOn; }

	void UpdateFlashlight( const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, float flFov, bool castsShadows,
		float flFarZ, float flLinearAtten, const char* pTextureName = nullptr );

	void SetEntityIndex( int index )
	{
		m_nFlashlightEntIndex = index;
	}

	void TriggerMuzzleFlash();

	const char *GetFlashlightTextureName( void ) const
	{
		return m_pFlashlightTextureName;
	}

	int GetFlashlightEntIndex( void ) const
	{
		return m_nFlashlightEntIndex;
	}

	void EnableFlashlightOverride( bool bEnable );

	void UpdateFlashlightOverride(	bool bFlashlightOn, const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp,
									float flFov, bool castsShadows, ITexture *pFlashlightTexture, const Vector &vecBrightness );
};

CFlashlightEffectManager & FlashlightEffectManager( int32 nSplitscreenPlayerOverride = -1 );

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

const int MAX_PUSHED_ENTITIES = 32;
struct physicspushlist_t
{
	float	localMoveTime;
	Vector	localOrigin;
	QAngle	localAngles;
	int		pushedCount;
	EHANDLE	pushedEnts[MAX_PUSHED_ENTITIES];
	Vector	pushVec[MAX_PUSHED_ENTITIES];
};

extern ConVar sv_gravity;
extern ConVar sv_stopspeed;
extern ConVar sv_noclipaccelerate;
extern ConVar sv_noclipspeed;
extern ConVar sv_maxspeed;
extern ConVar sv_accelerate;
extern ConVar sv_airaccelerate;
extern ConVar sv_wateraccelerate;
extern ConVar sv_waterfriction;
extern ConVar sv_footsteps;
extern ConVar sv_rollspeed;
extern ConVar sv_rollangle;
extern ConVar sv_friction;
extern ConVar sv_bounce;
extern ConVar sv_maxvelocity;
extern ConVar sv_stepsize;
extern ConVar sv_skyname;
extern ConVar sv_backspeed;
extern ConVar sv_waterdist;
extern ConVar sv_specaccelerate;
extern ConVar sv_specspeed;
extern ConVar sv_specnoclip;

// Vehicle convars
extern ConVar r_VehicleViewDampen;

// Jeep convars
extern ConVar r_JeepViewDampenFreq;
extern ConVar r_JeepViewDampenDamp;
extern ConVar r_JeepViewZHeight;

// Airboat convars
extern ConVar r_AirboatViewDampenFreq;
extern ConVar r_AirboatViewDampenDamp;
extern ConVar r_AirboatViewZHeight;

class CBaseEntity;

using namespace ::ks::reflect;

class CMovieDisplay : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_MovieDisplay" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CMovieDisplay, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CMovieDisplay();
	~CMovieDisplay();

	bool IsEnabled( void ) const { return m_bEnabled; }
	bool IsLooping( void ) const { return m_bLooping; }
	bool IsStretchingToFill( void ) const { return m_bStretchToFill; }
	bool IsForcedSlave( void ) const { return m_bForcedSlave; }

	bool IsUsingCustomUVs( void ) const { return m_bUseCustomUVs; }
	bool GetWantsToBeMaster( void ) const { return m_bWantsToBeMaster; }
	void SetMasterAttempted( void ) { m_bWantsToBeMaster = false; m_bForcedSlave = false; }
	float GetUMin( void ) const { return m_flUMin; }
	float GetUMax( void ) const { return m_flUMax; }
	float GetVMin( void ) const { return m_flVMin; }
	float GetVMax( void ) const { return m_flVMax; }
	
	virtual void ReceiveMessage( int classID, bf_read &msg );

	const char *GetMovieFilename( void ) const { return m_szMovieFilename; }
	const char *GetGroupName( void ) const { return m_szGroupName; }

private:
	[[= ks::reflect::Net{} ]] bool	m_bEnabled;
	[[= ks::reflect::Net{} ]] bool	m_bLooping;
	[[= ks::reflect::Net{} ]] char	m_szMovieFilename[128];
	[[= ks::reflect::Net{} ]] char	m_szGroupName[128];
	[[= ks::reflect::Net{} ]] bool	m_bStretchToFill;
	[[= ks::reflect::Net{} ]] bool	m_bForcedSlave;
	[[= ks::reflect::Net{} ]] bool	m_bUseCustomUVs;
	bool	m_bWantsToBeMaster;
	[[= ks::reflect::Net{} ]] float	m_flUMin;
	[[= ks::reflect::Net{} ]] float	m_flUMax;
	[[= ks::reflect::Net{} ]] float	m_flVMin;
	[[= ks::reflect::Net{} ]] float	m_flVMax;
};

//-----------------------------------------------------------------------------
// TF Concepts
//-----------------------------------------------------------------------------
inline constexpr auto MP_CONCEPT_NONE = -1;

enum
{
	MP_CONCEPT_FIREWEAPON = 0,				// "TLK_FIREWEAPON"
	MP_CONCEPT_HURT,						// "TLK_HURT"
	MP_CONCEPT_PLAYER_EXPRESSION,			// "TLK_PLAYER_EXPRESSION"
	MP_CONCEPT_WINDMINIGUN,					// "TLK_WINDMINIGUN"
	MP_CONCEPT_FIREMINIGUN,					// "TLK_FIREMINIGUN"
	MP_CONCEPT_PLAYER_MEDIC,				// "TLK_PLAYER_MEDIC"
	MP_CONCEPT_DETONATED_OBJECT,			// "TLK_DETONATED_OBJECT"
	MP_CONCEPT_KILLED_PLAYER,				// "TLK_KILLED_PLAYER"
	MP_CONCEPT_KILLED_OBJECT,				// "TLK_KILLED_OBJECT"
	MP_CONCEPT_PLAYER_PAIN,					// "TLK_PLAYER_PAIN"
	MP_CONCEPT_PLAYER_ATTACKER_PAIN,		// "TLK_PLAYER_ATTACKER_PAIN"
	MP_CONCEPT_PLAYER_TAUNT,				// "TLK_PLAYER_TAUNT"
	MP_CONCEPT_PLAYER_HELP,					// "TLK_PLAYER_HELP"
	MP_CONCEPT_PLAYER_GO,					// "TLK_PLAYER_GO"
	MP_CONCEPT_PLAYER_MOVEUP,				// "TLK_PLAYER_MOVEUP"
	MP_CONCEPT_PLAYER_LEFT,					// "TLK_PLAYER_LEFT"
	MP_CONCEPT_PLAYER_RIGHT,				// "TLK_PLAYER_RIGHT"
	MP_CONCEPT_PLAYER_YES,					// "TLK_PLAYER_YES"
	MP_CONCEPT_PLAYER_NO,					// "TLK_PLAYER_NO"
	MP_CONCEPT_PLAYER_INCOMING,				// "TLK_PLAYER_INCOMING"
	MP_CONCEPT_PLAYER_CLOAKEDSPY,			// "TLK_PLAYER_CLOAKEDSPY"
	MP_CONCEPT_PLAYER_SENTRYAHEAD,			// "TLK_PLAYER_SENTRYAHEAD"
	MP_CONCEPT_PLAYER_TELEPORTERHERE,		// "TLK_PLAYER_TELEPORTERHERE"
	MP_CONCEPT_PLAYER_DISPENSERHERE,		// "TLK_PLAYER_DISPENSERHERE"
	MP_CONCEPT_PLAYER_SENTRYHERE,			// "TLK_PLAYER_SENTRYHERE"
	MP_CONCEPT_PLAYER_ACTIVATECHARGE,		// "TLK_PLAYER_ACTIVATECHARGE"
	MP_CONCEPT_PLAYER_CHARGEREADY,			// "TLK_PLAYER_CHARGEREADY"
	MP_CONCEPT_PLAYER_TAUNTS,				// "TLK_PLAYER_TAUNTS"	
	MP_CONCEPT_PLAYER_BATTLECRY,			// "TLK_PLAYER_BATTLECRY"
	MP_CONCEPT_PLAYER_CHEERS,				// "TLK_PLAYER_CHEERS"
	MP_CONCEPT_PLAYER_JEERS,				// "TLK_PLAYER_JEERS"
	MP_CONCEPT_PLAYER_POSITIVE,				// "TLK_PLAYER_POSITIVE"
	MP_CONCEPT_PLAYER_NEGATIVE,				// "TLK_PLAYER_NEGATIVE"
	MP_CONCEPT_PLAYER_NICESHOT,				// "TLK_PLAYER_NICESHOT"
	MP_CONCEPT_PLAYER_GOODJOB,				// "TLK_PLAYER_GOODJOB"
	MP_CONCEPT_MEDIC_STARTEDHEALING,		// "TLK_MEDIC_STARTEDHEALING"
	MP_CONCEPT_MEDIC_CHARGEREADY,			// "TLK_MEDIC_CHARGEREADY"
	MP_CONCEPT_MEDIC_STOPPEDHEALING,		// "TLK_MEDIC_STOPPEDHEALING"
	MP_CONCEPT_MEDIC_CHARGEDEPLOYED,		// "TLK_MEDIC_CHARGEDEPLOYED"
	MP_CONCEPT_FLAGPICKUP,					// "TLK_FLAGPICKUP"
	MP_CONCEPT_FLAGCAPTURED,				// "TLK_FLAGCAPTURED"
	MP_CONCEPT_ROUND_START,					// "TLK_ROUND_START"
	MP_CONCEPT_SUDDENDEATH_START,			// "TLK_SUDDENDEATH_START"
	MP_CONCEPT_ONFIRE,						// "TLK_ONFIRE"
	MP_CONCEPT_STALEMATE,					// "TLK_STALEMATE"
	MP_CONCEPT_BUILDING_OBJECT,				// "TLK_BUILDING_OBJECT"
	MP_CONCEPT_LOST_OBJECT,					// "TLK_LOST_OBJECT"
	MP_CONCEPT_SPY_SAPPER,					// "TLK_SPY_SAPPER"
	MP_CONCEPT_TELEPORTED,					// "TLK_TELEPORTED"
	MP_CONCEPT_LOST_CONTROL_POINT,			// "TLK_LOST_CONTROL_POINT"
	MP_CONCEPT_CAPTURED_POINT,				// "TLK_CAPTURED_POINT"
	MP_CONCEPT_CAPTURE_BLOCKED,				// "TLK_CAPTURE_BLOCKED"
	MP_CONCEPT_HEALTARGET_STARTEDHEALING,	// "TLK_HEALTARGET_STARTEDHEALING"
	MP_CONCEPT_HEALTARGET_CHARGEREADY,		// "TLK_HEALTARGET_CHARGEREADY"
	MP_CONCEPT_HEALTARGET_STOPPEDHEALING,	// "TLK_HEALTARGET_STOPPEDHEALING"
	MP_CONCEPT_HEALTARGET_CHARGEDEPLOYED,	// "TLK_HEALTARGET_CHARGEDEPLOYED"
	MP_CONCEPT_MINIGUN_FIREWEAPON,			// "TLK_MINIGUN_FIREWEAPON"
	MP_CONCEPT_DIED,						// "TLK_DIED"
	MP_CONCEPT_PLAYER_THANKS,				// "TLK_PLAYER_THANKS"
	MP_CONCEPT_CART_MOVING_FORWARD,			// "TLK_CART_MOVING_FORWARD"
	MP_CONCEPT_CART_MOVING_BACKWARD,		// "TLK_CART_MOVING_BACKWARD"
	MP_CONCEPT_CART_STOP,					// "TLK_CART_STOP"
	MP_CONCEPT_ATE_FOOD,					// "TLK_ATE_FOOD"
	MP_TF_CONCEPT_COUNT

	// Other MP_CONCEPT_* start he using MP_TF_CONCEPT_COUNT + 1 as start.
};

extern const char *g_pszMPConcepts[];
int GetMPConceptIndexFromString( const char *pszConcept );

using namespace ::ks::reflect;

inline constexpr auto MAX_SPEAK_CONCEPT_LEN = 64;
inline constexpr auto MAX_VOICE_COMMAND_SUBTITLE = 256;

typedef struct
{
	// localizable subtitle
	char m_szSubtitle[MAX_VOICE_COMMAND_SUBTITLE];

	// localizable string for menu
	char m_szMenuLabel[MAX_VOICE_COMMAND_SUBTITLE];

} VoiceCommandMenuItem_t;

extern ConVar mp_timelimit;

class CBaseMultiplayerPlayer;

//=========================================================
// CMultiplayRules - rules for the basic half life multiplayer
// competition
//=========================================================
class CMultiplayRules : public CGameRules
{
public:
	DECLARE_CLASS( CMultiplayRules, CGameRules );

// Functions to verify the single/multiplayer status of a game
	virtual bool IsMultiplayer( void );

	// Damage query implementations.
	virtual bool	Damage_IsTimeBased( int iDmgType );			// Damage types that are time-based.
	virtual bool	Damage_ShouldGibCorpse( int iDmgType );		// Damage types that gib the corpse.
	virtual bool	Damage_ShowOnHUD( int iDmgType );				// Damage types that have client HUD art.
	virtual bool	Damage_NoPhysicsForce( int iDmgType );		// Damage types that don't have to supply a physics force & position.
	virtual bool	Damage_ShouldNotBleed( int iDmgType );			// Damage types that don't make the player bleed.
	// TEMP: These will go away once DamageTypes become enums.
	virtual int		Damage_GetTimeBased( void );
	virtual int		Damage_GetShouldGibCorpse( void );
	virtual int		Damage_GetShowOnHud( void );
	virtual int		Damage_GetNoPhysicsForce( void );
	virtual int		Damage_GetShouldNotBleed( void );

	CMultiplayRules();
	virtual ~CMultiplayRules() {}

	void LoadVoiceCommandScript( void );

	public:
		const char *GetVoiceCommandSubtitle( int iMenu, int iItem );
		bool GetVoiceMenuLabels( int iMenu, KeyValues *pKV );

	private:
		std::vector< std::vector< VoiceCommandMenuItem_t > > m_VoiceCommandMenus;
		
};

inline CMultiplayRules* MultiplayRules()
{
	return static_cast<CMultiplayRules*>(g_pGameRules);
}

class IGameSystem;

// Purpose: Contains an interface to enable positional audio support in Mumble

class CMumbleSystem : public CBaseGameSystemPerFrame
{
public:
	// Methods of IGameSystem
	virtual bool	Init();
	virtual void	LevelInitPostEntity();
	virtual void	LevelShutdownPreEntity();
	virtual void	PostRender();
};

IGameSystem *MumbleSystem();

extern INetworkStringTableContainer *networkstringtable;

// String tables used by the client DLL	
// (see InstallStringTableCallback for where they're initialized)
extern INetworkStringTable *g_StringTableVguiScreen;
extern INetworkStringTable *g_StringTableEffectDispatch;
extern INetworkStringTable *g_StringTableMaterials;
extern INetworkStringTable *g_pStringTableInfoPanel;
extern INetworkStringTable *g_pStringTableClientSideChoreoScenes;

class CBaseAnimating;


struct animevent_t
{
	// see mstudioevent_for_client_server_t comment below
	union
	{
		unsigned short	_event_highword;
		unsigned short	event_newsystem;
	};
	unsigned short	_event_lowword;

	const char		*options;
	float			cycle;
	float			eventtime;
	int				type;
	CBaseAnimating	*pSource;
	bool			m_bHandledByScript;

	// see mstudioevent_for_client_server_t comment below
	int Event_OldSystem( void ) const { return *static_cast< const int* >( static_cast< const void* >( &_event_highword ) ); }
	void Event_OldSystem( int nEvent ) { *static_cast< int* >( static_cast< void* >( &_event_highword ) ) = nEvent; }
	int Event( void ) const
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
			return event_newsystem;

		return Event_OldSystem();
	}
	void Event( int nEvent )
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
		{
			event_newsystem = nEvent;
		}
		else
		{
			Event_OldSystem( nEvent );
		}
	}
};
inline constexpr auto EVENT_SPECIFIC = 0;
inline constexpr auto EVENT_SCRIPTED = 1000; // see scriptevent.h
inline constexpr auto EVENT_SHARED = 2000;
inline constexpr auto EVENT_WEAPON = 3000;
inline constexpr auto EVENT_CLIENT = 5000;

inline constexpr auto NPC_EVENT_BODYDROP_LIGHT = 2001;
inline constexpr auto NPC_EVENT_BODYDROP_HEAVY = 2002;

inline constexpr auto NPC_EVENT_SWISHSOUND = 2010;

inline constexpr auto NPC_EVENT_180TURN = 2020;

inline constexpr auto NPC_EVENT_ITEM_PICKUP = 2040;
inline constexpr auto NPC_EVENT_WEAPON_DROP = 2041;
inline constexpr auto NPC_EVENT_WEAPON_SET_SEQUENCE_NAME = 2042;
inline constexpr auto NPC_EVENT_WEAPON_SET_SEQUENCE_NUMBER = 2043;
inline constexpr auto NPC_EVENT_WEAPON_SET_ACTIVITY = 2044;

inline constexpr auto NPC_EVENT_LEFTFOOT = 2050;
inline constexpr auto NPC_EVENT_RIGHTFOOT = 2051;

inline constexpr auto NPC_EVENT_OPEN_DOOR = 2060;

// !! DON'T CHANGE TO ORDER OF THESE.  THEY ARE HARD CODED IN THE WEAPON QC FILES (YUCK!) !!
inline constexpr auto EVENT_WEAPON_MELEE_HIT = 3001;
inline constexpr auto EVENT_WEAPON_SMG1 = 3002;
inline constexpr auto EVENT_WEAPON_MELEE_SWISH = 3003;
inline constexpr auto EVENT_WEAPON_SHOTGUN_FIRE = 3004;
inline constexpr auto EVENT_WEAPON_THROW = 3005;
inline constexpr auto EVENT_WEAPON_AR1 = 3006;
inline constexpr auto EVENT_WEAPON_AR2 = 3007;
inline constexpr auto EVENT_WEAPON_HMG1 = 3008;
inline constexpr auto EVENT_WEAPON_SMG2 = 3009;
inline constexpr auto EVENT_WEAPON_MISSILE_FIRE = 3010;
inline constexpr auto EVENT_WEAPON_SNIPER_RIFLE_FIRE = 3011;
inline constexpr auto EVENT_WEAPON_AR2_GRENADE = 3012;
inline constexpr auto EVENT_WEAPON_THROW2 = 3013;
inline constexpr auto EVENT_WEAPON_PISTOL_FIRE = 3014;
inline constexpr auto EVENT_WEAPON_RELOAD = 3015;
inline constexpr auto EVENT_WEAPON_THROW3 = 3016;
inline constexpr auto EVENT_WEAPON_RELOAD_SOUND = 3017; // Use this + EVENT_WEAPON_RELOAD_FILL_CLIP to prevent shooting during the reload animation
inline constexpr auto EVENT_WEAPON_RELOAD_FILL_CLIP = 3018;
inline constexpr auto EVENT_WEAPON_SMG1_BURST1 = 3101; // first round in a 3-round burst
inline constexpr auto EVENT_WEAPON_SMG1_BURSTN = 3102; // 2, 3 rounds
inline constexpr auto EVENT_WEAPON_AR2_ALTFIRE = 3103;

inline constexpr auto EVENT_WEAPON_SEQUENCE_FINISHED = 3900;

// NOTE: MUST BE THE LAST WEAPON EVENT -- ONLY WEAPON EVENTS BETWEEN EVENT_WEAPON AND THIS
inline constexpr auto EVENT_WEAPON_LAST = 3999;

// Because the client and server have a shared memory space for event data,
// but use different indexes for the event (servers cache them all upfront, 
// while client creates them as sequences fire) we're going to store the server
// index in the low word and the client index in the high word.
//
// studio.h is a public header, so we'll use this matching struct to do the
// conversion and not force us to macro all the code that checks the event member.
// This struct's data MUST match mstudioevent_t
//
// BUT events that use the old event system use the entire dword which will always
// match between client and server. Instead of wrapping everything in if checks, 
// for ( type & AE_TYPE_NEWEVENTSYSTEM ) we use the Event accessors to set/get 
// while doing the check under the hood.
//
// -Jeep
struct mstudioevent_for_client_server_t
{
	float				cycle;
	union
	{
		// Client shares with the high word
		unsigned short	_event_highword;
		unsigned short	event_newsystem;
	};
	unsigned short	_event_lowword;		// Placeholder for the low word

	int					type;
	inline const char * pszOptions( void ) const { return options; }
	char				options[64];

	int					szeventindex;
	inline char * const pszEventName( void ) const { return ((char *)this) + szeventindex; }

	// For setting and getting through Event, check the AE_TYPE_NEWEVENTSYSTEM flag to decide
	// if we should set/get just the short used by the client/server or use the whole int.
	int Event_OldSystem( void ) const { return *static_cast< const int* >( static_cast< const void* >( &_event_highword ) ); }
	void Event_OldSystem( int nEvent ) { *static_cast< int* >( static_cast< void* >( &_event_highword ) ) = nEvent; }
	int Event( void ) const
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
			return event_newsystem;

		return Event_OldSystem();
	}
	void Event( int nEvent )
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
		{
			event_newsystem = nEvent;
		}
		else
		{
			Event_OldSystem( nEvent );
		}
	}
};

class CBaseAnimating;

using namespace ::ks::reflect;

// Purpose: Functionality to render object motion blur

//-----------------------------------------------------------------------------
// Central registry for objects that want motion blur applied to them,
// responsible for tracking and drawing models.
//
// Currently requires CBaseAnimating objects, could be made to work with
// other object types as long as they can 1) be scaled and 2) be rendered.
//-----------------------------------------------------------------------------
class CObjectMotionBlurManager
{
public:
	CObjectMotionBlurManager() :
	m_nFirstFreeSlot( ObjectMotionBlurDefinition_t::END_OF_FREE_LIST )
	{
	}

	// Registers an entity with the object manager and returns an integer handle which must be unregistered before the object gdies
	int RegisterObject( CBaseAnimating *pEntity, float flVelocityScale );
	
	// Unregisters a previously registered entity
	void UnregisterObject( int nObjectHandle );

	void SetVelocityScale( int nObjectHandle, float flVelocityScale );
	
	// Iterates through all valid registered objects and calls ObjectMotionBlurDefinition_t::DrawModel() on them
	void DrawObjects();

	int GetDrawableObjectCount();

private:
	struct ObjectMotionBlurDefinition_t
	{
		bool ShouldDraw() const { return m_pEntity && m_pEntity->ShouldDraw() && m_flVelocityScale > 0; }
		bool IsUnused() const { return m_nNextFreeSlot != ObjectMotionBlurDefinition_t::ENTRY_IN_USE; }
		void DrawModel();

		CBaseAnimating *m_pEntity;
		float m_flVelocityScale;

		// Linked list of free slots
		int m_nNextFreeSlot;

		// Special values for ObjectMotionBlurDefinition_t::m_nNextFreeSlot
		static const int END_OF_FREE_LIST = -1;
		static const int ENTRY_IN_USE = -2;
	};

	std::vector< ObjectMotionBlurDefinition_t > m_ObjectMotionBlurDefinitions;
	int m_nFirstFreeSlot;
};

extern CObjectMotionBlurManager g_ObjectMotionBlurManager;

//-----------------------------------------------------------------------------
// A scope-based auto-registration class for object motion blur
//-----------------------------------------------------------------------------
class CMotionBlurObject
{
public:
	CMotionBlurObject( CBaseAnimating *pEntity, float flVelocityScale = 0.2f )
	{
		m_nObjectHandle = g_ObjectMotionBlurManager.RegisterObject( pEntity, flVelocityScale );
	}

	~CMotionBlurObject()
	{
		g_ObjectMotionBlurManager.UnregisterObject( m_nObjectHandle );
	}

	void SetVelocityScale( float flVelocityScale )
	{
		g_ObjectMotionBlurManager.SetVelocityScale( m_nObjectHandle, flVelocityScale );
	}

private:
	int m_nObjectHandle;

	// Assignment & copy-construction disallowed
	CMotionBlurObject( const CMotionBlurObject &other );
	CMotionBlurObject& operator=( const CMotionBlurObject &other );
};

class CUserCmd;
class CBaseCombatCharacter;
class CBaseEntity;

using namespace ::ks::reflect;


//--------------------------------------------------------------------------------------------------------------
bool IsPushAwayEntity( CBaseEntity *pEnt );
bool IsPushableEntity( CBaseEntity *pEnt );

//--------------------------------------------------------------------------------------------------------------
bool IsBreakableEntity( CBaseEntity *pEnt );

//--------------------------------------------------------------------------------------------------------------
class CPushAwayEnumerator : public IPartitionEnumerator
{
public:
	// Forced constructor
	CPushAwayEnumerator(CBaseEntity **ents, int nMaxEnts);
	
	// Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );

		if ( IsPushAwayEntity( pEnt ) && m_nAlreadyHit < m_nMaxHits )
		{
			m_AlreadyHit[m_nAlreadyHit] = pEnt;
			m_nAlreadyHit++;
		}

		return ITERATION_CONTINUE;
	}

public:

	CBaseEntity **m_AlreadyHit;
	int m_nAlreadyHit;
	int m_nMaxHits;
};

//--------------------------------------------------------------------------------------------------------------
// Retrieves physics objects near pPushingEntity
void AvoidPushawayProps(  CBaseCombatCharacter *pPlayer, CUserCmd *pCmd );
int GetPushawayEnts( CBaseCombatCharacter *pPushingEntity, CBaseEntity **ents, int nMaxEnts, float flPlayerExpand, int PartitionMask, CPushAwayEnumerator *enumerator = nullptr );

//--------------------------------------------------------------------------------------------------------------
// Pushes physics objects away from the entity
void PerformObstaclePushaway( CBaseCombatCharacter *pPushingEntity );

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Base entity for beam te's
//-----------------------------------------------------------------------------
class CTEBaseBeam : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseBeam", .base = false } ]]
	[[= ks::reflect::From<"r", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"g", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"b", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"a", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEBaseBeam, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

private:

public:

					CTEBaseBeam( void );
	virtual			~CTEBaseBeam( void );

	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nHaloIndex;
	[[= ks::reflect::Net{} ]] int				m_nStartFrame;
	[[= ks::reflect::Net{} ]] int				m_nFrameRate;
	[[= ks::reflect::Net{} ]] float			m_fLife;
	[[= ks::reflect::Net{} ]] float			m_fWidth;
	[[= ks::reflect::Net{} ]] float			m_fEndWidth;
	[[= ks::reflect::Net{} ]] int				m_nFadeLength;
	[[= ks::reflect::Net{} ]] float			m_fAmplitude;
	int				r, g, b, a;
	[[= ks::reflect::Net{} ]] int				m_nSpeed;
	[[= ks::reflect::Net{} ]] int				m_nFlags;
};

EXTERN_RECV_TABLE(DT_BaseBeam);

class CEffectData;


typedef void (*ClientEffectCallback)( const CEffectData &data );

class CClientEffectRegistration
{
public:
	CClientEffectRegistration( const char *pEffectName, ClientEffectCallback fn );

public:
	const char *m_pEffectName;
	ClientEffectCallback m_pFunction;
	CClientEffectRegistration *m_pNext;

	static CClientEffectRegistration *s_pHead;
};

//
// Use this macro to register a client effect callback. 
// If you do DECLARE_CLIENT_EFFECT( MyEffectName, MyCallback ), then MyCallback will be 
// called when the server does DispatchEffect( "MyEffectName", data )
//

void DispatchEffectToCallback( const char *pEffectName, const CEffectData &m_EffectData );
void DispatchEffect( const char *pName, const CEffectData &data );
void DispatchEffect( IRecipientFilter& filter, float flDelay, const char *pName, const CEffectData &data );
void DispatchEffect( IRecipientFilter& filter, float delay, KeyValues *pKeyValues );

class CLocalTempEntity;

class CBaseAnimating;

using namespace ::ks::reflect;

// Temporary entity array
inline constexpr int TENTPRIORITY_LOW = 0;
inline constexpr int TENTPRIORITY_HIGH = 1;

// TEMPENTITY flags
inline constexpr int FTENT_NONE = 0x00000000;
inline constexpr int FTENT_SINEWAVE = 0x00000001;
inline constexpr int FTENT_GRAVITY = 0x00000002;
inline constexpr int FTENT_ROTATE = 0x00000004;
inline constexpr int FTENT_SLOWGRAVITY = 0x00000008;
inline constexpr int FTENT_SMOKETRAIL = 0x00000010;
inline constexpr int FTENT_COLLIDEWORLD = 0x00000020;
inline constexpr int FTENT_FLICKER = 0x00000040;
inline constexpr int FTENT_FADEOUT = 0x00000080;
inline constexpr int FTENT_SPRANIMATE = 0x00000100;
inline constexpr int FTENT_HITSOUND = 0x00000200;
inline constexpr int FTENT_SPIRAL = 0x00000400;
inline constexpr int FTENT_SPRCYCLE = 0x00000800;
inline constexpr auto FTENT_COLLIDEALL = 0x00001000; // will collide with world and slideboxes
inline constexpr auto FTENT_PERSIST = 0x00002000; // tent is not removed when unable to draw 
inline constexpr auto FTENT_COLLIDEKILL = 0x00004000; // tent is removed upon collision with anything
inline constexpr auto FTENT_PLYRATTACHMENT = 0x00008000; // tent is attached to a player (owner)
inline constexpr auto FTENT_SPRANIMATELOOP = 0x00010000; // animating sprite doesn't die when last frame is displayed
inline constexpr auto FTENT_SMOKEGROWANDFADE = 0x00020000; // rapid grow and fade. Very specific for gunsmoke
inline constexpr auto FTENT_ATTACHTOTARGET = 0x00040000; // attach to whatever we hit, and stay there until die time is up
inline constexpr auto FTENT_NOMODEL = 0x00080000; // Doesn't have a model, never try to draw ( it just triggers other things )
inline constexpr auto FTENT_CLIENTCUSTOM = 0x00100000; // Must specify callback.  Callback function is responsible for killing tempent and updating fields ( unless other flags specify how to do things )
inline constexpr auto FTENT_WINDBLOWN = 0x00200000; // This is set when the temp entity is blown by the wind
inline constexpr auto FTENT_NEVERDIE = 0x00400000; // Don't die as long as die != 0
inline constexpr auto FTENT_BEOCCLUDED = 0x00800000; // Don't draw if my specified normal's facing away from the view
inline constexpr auto FTENT_CHANGERENDERONCOLLIDE = 0x01000000; //when we collide with something, change our rendergroup to RENDER_GROUP_OTHER
inline constexpr auto FTENT_COLLISIONGROUP = 0x02000000; // if set, use the CBaseEntity::GetCollisionGroup when doing collide trace
inline constexpr auto FTENT_ALIGNTOMOTION = 0x04000000; // Change angles to always point in the direction of motion
inline constexpr auto FTENT_CLIENTSIDEPARTICLES = 0x08000000; // The object has a clientside particle system.
inline constexpr auto FTENT_USEFASTCOLLISIONS = 0x10000000; // Use fast collisions (cl_fasttempentcollision).


typedef int (*pfnDrawHelper)( CLocalTempEntity *entity, int flags );

//-----------------------------------------------------------------------------
// Purpose: Should this derive from some other class
//-----------------------------------------------------------------------------
class CLocalTempEntity : public CBaseAnimating, public CSpriteRenderer
{
public:
	DECLARE_CLASS( CLocalTempEntity, CBaseAnimating );

	CLocalTempEntity();

	virtual void					Prepare( model_t *pmodel, float time );

	virtual bool					IsActive( void );
	virtual bool					Frame( float frametime, int framenumber );

	// CBaseAnimating , etc. override
	virtual int						DrawModel( int flags, const RenderableInstance_t &instance );

	// Sets the velocity
	void SetVelocity( const Vector &vecVelocity );
	const Vector &GetVelocity() const { return m_vecTempEntVelocity; }

	// Set the acceleration
	void SetAcceleration( const Vector &vecAccel );
	const Vector &GetAcceleration() const { return m_vecTempEntAcceleration; }

	void							SetDrawHelper( pfnDrawHelper helper ) { m_pfnDrawHelper = helper; }
	void							OnRemoveTempEntity();

	void							SetImpactEffect( const char *pszImpactEffect ) { m_pszImpactEffect = pszImpactEffect; }
	void							AddParticleEffect( const char *pszParticleEffect );
	void							SetParticleEffect( const char *pszParticleEffect ) { m_pszParticleEffect = pszParticleEffect; }

protected:

	pfnDrawHelper					m_pfnDrawHelper;

public:
	int								flags;
	float							die;
	float							m_flFrameMax;
	float							x;
	float							y;
	float							fadeSpeed;
	float							bounceFactor;
	int								hitSound;
	int								priority;
	// if attached, this is the index of the client to stick to
	// if COLLIDEALL, this is the index of the client to ignore
	// TENTS with FTENT_PLYRATTACHMENT MUST set the clientindex! 
	short							clientIndex;	

	// if attached, client origin + tentOffset = tent origin.
	Vector							tentOffset;		

	// Used by temp entities.
	QAngle							m_vecTempEntAngVelocity;
	int								tempent_renderamt;
	Vector							m_vecNormal;

	float							m_flSpriteScale;
	int								m_nFlickerFrame;

	// 
	float							m_flFrameRate;
	float							m_flFrame;

	const char						*m_pszImpactEffect;
	const char						*m_pszParticleEffect;
	bool							m_bParticleCollision;

	int								m_iLastCollisionFrame;
	Vector							m_vLastCollisionOrigin;

private:
	CLocalTempEntity( const CLocalTempEntity & );

	Vector							m_vecTempEntVelocity;
	Vector							m_vecPrevLocalOrigin;

	Vector							m_vecTempEntAcceleration;

	// Draw tempent as a studio model
	int								DrawStudioModel( int flags );

};

class CBaseEntity;
class CLocalTempEntity;


extern color24 white24;
enum
{
	CS_SHELL_9MM = 0,
	CS_SHELL_57,
	CS_SHELL_12GAUGE,
	CS_SHELL_556,
	CS_SHELL_762NATO,
	CS_SHELL_338MAG,
};

//-----------------------------------------------------------------------------
// Purpose: Interface for lecacy temp entities
//-----------------------------------------------------------------------------
class ITempEnts
{
public:
	virtual						~ITempEnts() {}

	virtual void				Init( void ) = 0;
	virtual void				Shutdown( void ) = 0;
	virtual void				LevelInit() = 0;
	virtual void				LevelShutdown() = 0;

	virtual void				Update( void ) = 0;
	virtual void				Clear( void ) = 0;

	virtual CLocalTempEntity	*FindTempEntByID( int nID, int nSubID ) = 0;

	virtual void				BloodSprite( const Vector &org, int r, int g, int b, int a, int modelIndex, int modelIndex2, float size ) = 0;
	virtual void				RicochetSprite( const Vector &pos, model_t *pmodel, float duration, float scale ) = 0;
	virtual void				MuzzleFlash( int type, ClientEntityHandle_t hEntity, int attachmentIndex, bool firstPerson ) = 0;
	virtual void				MuzzleFlash( const Vector &pos1, const QAngle &angles, int type, ClientEntityHandle_t hEntity, bool firstPerson ) = 0;
	virtual CLocalTempEntity   *SpawnTempModel( model_t *pModel, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, float flLifeTime, int iFlags ) = 0;
	virtual void				BreakModel( const Vector &pos, const QAngle &angles, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags) = 0;
	virtual void				Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed ) = 0;
	virtual void				BubbleTrail( const Vector &start, const Vector &end, float flWaterZ, int modelIndex, int count, float speed ) = 0;
	virtual void				Sprite_Explode( CLocalTempEntity *pTemp, float scale, int flags ) = 0;
	virtual void				FizzEffect( CBaseEntity *pent, int modelIndex, int density, int current ) = 0;
	virtual CLocalTempEntity	*DefaultSprite( const Vector &pos, int spriteIndex, float framerate ) = 0;
	virtual void				Sprite_Smoke( CLocalTempEntity *pTemp, float scale ) = 0;
	virtual CLocalTempEntity	*TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags, const Vector &normal = vec3_origin ) = 0;
	virtual void				AttachTentToPlayer( int client, int modelIndex, float zoffset, float life ) = 0;
	virtual void				KillAttachedTents( int client ) = 0;
	virtual void				Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand ) = 0;
	virtual void				Sprite_Trail( const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed ) = 0;
	virtual void				RocketFlare( const Vector& pos ) = 0;	
	virtual void				PlaySound ( CLocalTempEntity *pTemp, float damp ) = 0;
	virtual void				PhysicsProp( int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects = 0, color24 renderColor = white24 ) = 0;
	virtual CLocalTempEntity	*ClientProjectile( const Vector& vecOrigin, const Vector& vecVelocity, const Vector& vecAccel, int modelindex, int lifetime, CBaseEntity *pOwner, const char *pszImpactEffect = nullptr, const char *pszParticleEffect = nullptr ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Default implementation of the temp entity interface
//-----------------------------------------------------------------------------
class CTempEnts : public ITempEnts
{
// Construction
public:
							CTempEnts( void );
	virtual					~CTempEnts( void );
// Exposed interface
public:
	virtual void			Init( void );
	virtual void			Shutdown( void );

	virtual void			LevelInit();

	virtual void			LevelShutdown();

	virtual void			Update( void );
	virtual void			Clear( void );

	virtual CLocalTempEntity	*FindTempEntByID( int nID, int nSubID );

	// Legacy temp entities still supported
	virtual void			BloodSprite( const Vector &org, int r, int g, int b, int a, int modelIndex, int modelIndex2, float size );
	virtual void			RicochetSprite( const Vector &pos, model_t *pmodel, float duration, float scale );

	virtual void			MuzzleFlash( int type, ClientEntityHandle_t hEntity, int attachmentIndex, bool firstPerson );
	virtual void			MuzzleFlash( const Vector &pos1, const QAngle &angles, int type, ClientEntityHandle_t hEntity, bool firstPerson = false );
	
	virtual void			BreakModel(const Vector &pos, const QAngle &angles, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags);
	virtual void			Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed );
	virtual void			BubbleTrail( const Vector &start, const Vector &end, float height, int modelIndex, int count, float speed );
	virtual void			Sprite_Explode( CLocalTempEntity *pTemp, float scale, int flags );
	virtual void			FizzEffect( CBaseEntity *pent, int modelIndex, int density, int current );
	virtual CLocalTempEntity		*DefaultSprite( const Vector &pos, int spriteIndex, float framerate );
	virtual void			Sprite_Smoke( CLocalTempEntity *pTemp, float scale );
	virtual CLocalTempEntity		*TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags, const Vector &normal = vec3_origin );
	virtual void			AttachTentToPlayer( int client, int modelIndex, float zoffset, float life );
	virtual void			KillAttachedTents( int client );
	virtual void			Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand );
	void					Sprite_Trail( const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed );

	virtual void			PlaySound ( CLocalTempEntity *pTemp, float damp );
	virtual CLocalTempEntity		*SpawnTempModel( model_t *pModel, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, float flLifeTime, int iFlags );
	void					RocketFlare( const Vector& pos );
	void					PhysicsProp( int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects = 0, color24 renderColor = white24 );
	CLocalTempEntity		*ClientProjectile( const Vector& vecOrigin, const Vector& vecVelocity, const Vector& vecAcceleration, int modelindex, int lifetime, CBaseEntity *pOwner, const char *pszImpactEffect = nullptr, const char *pszParticleEffect = nullptr );

// Data
	enum
	{ 
		MAX_TEMP_ENTITIES = 500,
		MAX_TEMP_ENTITY_SPRITES = 200,
		MAX_TEMP_ENTITY_STUDIOMODEL = 50,
	};
private:
	// Global temp entity pool
	ks::object_pool< CLocalTempEntity >	m_TempEntsPool;
	// std::list: effects triggered from Update() may allocate or free other tempents mid-iteration
	std::list< CLocalTempEntity * >	m_TempEnts;

	// Muzzle flash sprites
	struct model_t			*m_pSpriteMuzzleFlash[10];
	struct model_t			*m_pSpriteAR2Flash[4];
	struct model_t			*m_pShells[3];
	struct model_t			*m_pSpriteCombineFlash[2];

	struct model_t			*m_pCS_9MMShell;
	struct model_t			*m_pCS_57Shell;
	struct model_t			*m_pCS_12GaugeShell;
	struct model_t			*m_pCS_556Shell;
	struct model_t			*m_pCS_762NATOShell;
	struct model_t			*m_pCS_338MAGShell;

// Internal methods also available to children
protected:
	CLocalTempEntity		*TempEntAlloc( const Vector& org, model_t *model );
	CLocalTempEntity		*TempEntAllocHigh( const Vector& org, model_t *model );

// Material handle caches
private:

	inline void				CacheMuzzleFlashes( void );
	PMaterialHandle			m_Material_MuzzleFlash_Player[4];
	PMaterialHandle			m_Material_MuzzleFlash_NPC[4];
	PMaterialHandle			m_Material_Combine_MuzzleFlash_Player[2];
	PMaterialHandle			m_Material_Combine_MuzzleFlash_NPC[2];

// Internal methods
private:
	CTempEnts( const CTempEnts & );

	void					TempEntFree( std::list< CLocalTempEntity * >::iterator it );
	CLocalTempEntity		*TempEntAlloc();	
	bool					FreeLowPriorityTempEnt();

	int						AddVisibleTempEntity( CLocalTempEntity *pEntity );

	// AR2
	void					MuzzleFlash_AR2_Player( const Vector &origin, const QAngle &angles, ClientEntityHandle_t hEntity );
	void					MuzzleFlash_AR2_NPC( const Vector &origin, const QAngle &angles, ClientEntityHandle_t hEntity );
							
	// SMG1					
	void					MuzzleFlash_SMG1_Player( ClientEntityHandle_t hEntity, int attachmentIndex );
	void					MuzzleFlash_SMG1_NPC( ClientEntityHandle_t hEntity, int attachmentIndex );
							
	// Shotgun				
	void					MuzzleFlash_Shotgun_Player( ClientEntityHandle_t hEntity, int attachmentIndex );
	void					MuzzleFlash_Shotgun_NPC( ClientEntityHandle_t hEntity, int attachmentIndex );
							
	// Pistol				
	void					MuzzleFlash_Pistol_Player( ClientEntityHandle_t hEntity, int attachmentIndex );
	void					MuzzleFlash_Pistol_NPC( ClientEntityHandle_t hEntity, int attachmentIndex );
							
	// Combine				
	void					MuzzleFlash_Combine_Player( ClientEntityHandle_t hEntity, int attachmentIndex );
	void					MuzzleFlash_Combine_NPC( ClientEntityHandle_t hEntity, int attachmentIndex );

	// 357
	void					MuzzleFlash_357_Player( ClientEntityHandle_t hEntity, int attachmentIndex );

	// RPG
	void					MuzzleFlash_RPG_NPC( ClientEntityHandle_t hEntity, int attachmentIndex );
};

extern ITempEnts *tempents;

class CParticleMgr;
class CParticleRenderIterator;
class CParticleSimulateIterator;

using namespace ::ks::reflect;

// This file defines the CTEParticleSystem class, which handles most of the
// interfacing with the particle manager, simulation, and rendering.

// NOTE: most tempents are singletons.

inline constexpr auto SIMSHIFT = 10;

typedef enum {
	pt_static, 
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2,
	pt_vox_slowgrav,
	pt_vox_grav,
	pt_snow,
	pt_rain,
	pt_clientcustom   // Must have callback function specified
} ptype_t;

class CTEParticleSystem : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEParticleSystem" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CTEParticleSystem, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEParticleSystem();

public:

	// particle effect sort origin
	[[= ks::reflect::Net{ .index = 2 } ]] [[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] Vector			m_vecOrigin;
};

// This is the class that legacy tempents use to emit particles.
// They use it in this pattern:
//	CTEParticleRenderer *pRen = CTEParticleRenderer::Create();
//	pRen->AddParticle....
//	pRen->Release();

class CTEParticleRenderer : public CParticleEffect
{
public:
	DECLARE_CLASS( CTEParticleRenderer, CParticleEffect );
	virtual						~CTEParticleRenderer();

	// Create a CTEParticleRenderer. Pass in your sort origin (m_vecOrigin).
	static ks::intrusive_ptr<CTEParticleRenderer>	Create( const char *pDebugName, const Vector &vOrigin );

	StandardParticle_t*		AddParticle();

	CParticleMgr*			GetParticleMgr();

	void					SetParticleType( StandardParticle_t *pParticle, ptype_t type );
	ptype_t					GetParticleType( StandardParticle_t *pParticle );

	// Get/set lifetime. Note: lifetime here is a counter. You set it to a value and it 
	// counts down and disappears after that long.
	void					SetParticleLifetime( StandardParticle_t *pParticle, float lifetime );
	float					GetParticleLifetime( StandardParticle_t *pParticle );

// IParticleEffect overrides.	
public:
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

private:
	explicit		CTEParticleRenderer( const char *pDebugName );
					CTEParticleRenderer( const CTEParticleRenderer & ); // = delete; not defined, not accessible

	int				m_nActiveParticles;
	float			m_ParticleSize;
	PMaterialHandle	m_MaterialHandle;
};

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Client side CTeam class


// named by the BareArray<> annotation below; both live in c_team.cpp
void RecvProxy_PlayerList( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxyArrayLength_PlayerArray( void *pStruct, int objectID, int currentArrayLength );

class CTeam : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Team", .base = false } ]]
	[[= ks::reflect::BareArray<"player_array_element", "\"player_array\"", MAX_PLAYERS, 0,
	kSizeofIgnore, ks::reflect::Net{},
	RecvProxy_PlayerList, RecvProxyArrayLength_PlayerArray>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CTeam, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					CTeam();
	virtual			~CTeam();

	virtual void	PreDataUpdate( DataUpdateType_t updateType );

	// Data Handling
	virtual const char	*Get_Name( void );
	virtual const char	*Get_ClanName( void );
	virtual const char	*Get_FlagImageString( void );
	virtual const char	*Get_LogoImageString( void );
	int				Get_Score( void )				{ return m_scoreTotal; }
	int				Get_Score_First_Half( void )	{ return m_scoreFirstHalf; }
	int				Get_Score_Second_Half( void )	{ return m_scoreSecondHalf; }	
	int				Get_Score_Overtime( void )		{ return m_scoreOvertime; }
	uint32			GetClanID( void )				{ return m_iClanID; }
	
	virtual int		Get_Deaths( void );
	virtual int		Get_Ping( void );

	// Player Handling
	virtual int		Get_Number_Players( void );
	virtual bool	ContainsPlayer( int iPlayerIndex );
	CBasePlayer*	GetPlayer( int idx );

	// for shared code, use the same function name
	virtual int		GetNumPlayers( void ) { return Get_Number_Players(); }

	virtual int		GetGGLeader( int nTeam );

	int		GetTeamNumber() const;

	void	RemoveAllPlayers();

// IClientThinkable overrides.
public:

	virtual	void				ClientThink();

public:

	// Data received from the server
	std::vector< int > m_aPlayers;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamname[ MAX_TEAM_NAME_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szClanTeamname[ MAX_TEAM_NAME_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamFlagImage[ MAX_TEAM_FLAG_ICON_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamLogoImage[ MAX_TEAM_LOGO_ICON_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamMatchStat[ MAX_PATH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreTotal;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreFirstHalf;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreSecondHalf;	
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreOvertime;
	[[= ks::reflect::Net{} ]] int		m_nGGLeaderEntIndex_CT;
	[[= ks::reflect::Net{} ]] int		m_nGGLeaderEntIndex_T;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] uint32	m_iClanID;

	// Data for the scoreboard
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iDeaths;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iPing;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iPacketloss;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_iTeamNum;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_bSurrendered;
	[[= ks::reflect::Net{} ]] int		m_numMapVictories;
};

// Global list of client side team entities
extern std::vector< CTeam * > g_Teams;

// Global team handling functions
CTeam *GetLocalTeam( void );
CTeam *GetGlobalTeam( int iTeamNumber );
CTeam *GetPlayersTeam( int iPlayerIndex );
CTeam *GetPlayersTeam( CBasePlayer *pPlayer );
bool ArePlayersOnSameTeam( int iPlayerIndex1, int iPlayerIndex2 );
extern int GetNumberOfTeams( void );

class CBasePlayer;

using namespace ::ks::reflect;


//
// teamplay_gamerules.h
//

inline constexpr auto MAX_TEAMNAME_LENGTH = 16;

inline constexpr auto TEAMPLAY_TEAMLISTLENGTH = MAX_TEAMS*MAX_TEAMNAME_LENGTH;

class CTeamplayRules : public CMultiplayRules
{
public:
	DECLARE_CLASS( CTeamplayRules, CMultiplayRules );

	// Return the value of this player towards capturing a point
	virtual int	 GetCaptureValueForPlayer( CBasePlayer *pPlayer ) { return 1; }
	virtual bool TeamMayCapturePoint( int iTeam, int iPointIndex ) { return true; }
	virtual bool PlayerMayCapturePoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = nullptr, int iMaxReasonLength = 0 ) { return true; }
	virtual bool PlayerMayBlockPoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = nullptr, int iMaxReasonLength = 0 ) { return false; }

	// Return false if players aren't allowed to cap points at this time (i.e. in WaitingForPlayers)
	virtual bool PointsMayBeCaptured( void ) { return true; }
	virtual void SetLastCapPointChanged( int iIndex ) { return; }

};

inline CTeamplayRules* TeamplayGameRules()
{
	return static_cast<CTeamplayRules*>(g_pGameRules);
}

using namespace ::ks::reflect;

// Purpose: round based game rule convars shared by the multiplayer rules.

extern ConVar mp_respawnwavetime;

// Utility function
bool FindInList( const char **pStrings, const char *pToFind );

class CBaseEntity;

using namespace ::ks::reflect;

class CTesla : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Tesla" } ]]
	static constexpr bool ks_class = true;

public:
	
	DECLARE_CLASS( CTesla, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CTesla();

	virtual void ReceiveMessage( int classID, bf_read &msg );
	virtual void ClientThink();

public:

	std::vector<CTeslaInfo> m_QueuedCommands;
	[[= ks::reflect::Net{} ]] char m_SoundName[64];
	[[= ks::reflect::Net{} ]] char m_iszSpriteName[256];
};

class IHudTextMessage
{
public:
	virtual char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size ) = 0;
	virtual const char * const BufferedLocaliseTextString( const char *msg ) = 0;
	virtual const char *LookupString( const char *msg_name, int *msg_dest = nullptr ) = 0;
};

extern IHudTextMessage *hudtextmessage;
