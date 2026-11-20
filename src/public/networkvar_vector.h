#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Included by networkvar.h

#include "networkvar.h"
import ks.math.compressed_vector;
import ks.math.vector;

// This is the normal case.. you've got a SendPropVector to match your CNetworkVector
#define CNetworkVector( name, ... ) CNetworkVectorInternal( Vector, name, NetworkStateChanged, CNetworkVectorBase __VA_OPT__(,) __VA_ARGS__ )

// This variant of a CNetworkVector should be used if you want to use SendPropFloat
// on each individual component of the vector.
#define CNetworkVectorXYZ( name, ... ) CNetworkVectorInternal( Vector, name, NetworkStateChanged, CNetworkVectorXYZBase __VA_OPT__(,) __VA_ARGS__ )

// This variant of a CNetworkVector should be used if you want to use SendPropVectorXY
// for the XY components and SendPropFloat for the Z component.
#define CNetworkVectorXY_SeparateZ( name, ... ) CNetworkVectorInternal( Vector, name, NetworkStateChanged, CNetworkVectorXY_SeparateZBase __VA_OPT__(,) __VA_ARGS__ )

// This is the normal case.. you've got a SendPropQAngle to match your CNetworkQAngle
#define CNetworkQAngle( name, ... ) CNetworkVectorInternal( QAngle, name, NetworkStateChanged, CNetworkVectorBase __VA_OPT__(,) __VA_ARGS__ )

// This variant of a CNetworkQAngle should be used if you want to use SendPropFloat
// on each individual component of the vector.
#define CNetworkQAngleXYZ( name, ... ) CNetworkVectorInternal( QAngle, name, NetworkStateChanged, CNetworkVectorXYZBase __VA_OPT__(,) __VA_ARGS__ )

//
// Use these variants if you want the networkvar to not trigger a change in the baseclass
// version but you might want it to trigger changes in derived classes that do network that variable.
//
#define CNetworkVectorForDerived( name, ... ) \
	NETWORK_VAR_FOR_DERIVED_HOOK( name ) \
	CNetworkVectorInternal( Vector, name, NetworkStateChanged_##name, CNetworkVectorBase __VA_OPT__(,) __VA_ARGS__ )
		
#define CNetworkVectorXYZForDerived( name, ... ) \
	NETWORK_VAR_FOR_DERIVED_HOOK( name ) \
	CNetworkVectorInternal( Vector, name, NetworkStateChanged_##name, CNetworkVectorXYZBase __VA_OPT__(,) __VA_ARGS__ )




#define CNetworkVectorInternal( type, name, stateChangedFn, baseClass, ... ) \
	NETWORK_VAR_START( type, name ) \
	NETWORK_VAR_END( type, name, baseClass, stateChangedFn __VA_OPT__(,) __VA_ARGS__ )



// Network vector wrapper.
//
// The common base is shared between all CNetworkVectors.
// It includes everything but the Set() and operator=() functions,
// because the behavior of each of those is different for each vector type.
template< class Type, class Changer >
class CNetworkVectorCommonBase : public CNetworkVarBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:

	[[gnu::always_inline]] inline  void Init( float ix=0, float iy=0, float iz=0 ) 
	{
		base::Set( Type( ix, iy, iz ) );
	}
	
	[[gnu::always_inline]] inline float GetX() const { return this->m_Value.x; }
	[[gnu::always_inline]] inline float GetY() const { return this->m_Value.y; }
	[[gnu::always_inline]] inline float GetZ() const { return this->m_Value.z; }
	[[gnu::always_inline]] inline float operator[]( int i ) const { return this->m_Value[i]; }

	[[gnu::always_inline]] inline bool operator==( const Type &val ) const 
	{ 
		return this->m_Value == (Type)val; 
	}

	[[gnu::always_inline]] inline bool operator!=( const Type &val ) const 
	{
		return this->m_Value != (Type)val; 
	}

	[[gnu::always_inline]] inline const Type operator+( const Type &val ) const 
	{
		return this->m_Value + val; 
	}

	[[gnu::always_inline]] inline const Type operator-( const Type &val ) const
	{ 
		return this->m_Value - val; 
	}

	[[gnu::always_inline]] inline const Type operator*( const Type &val ) const
	{
		return this->m_Value * val; 
	}

	[[gnu::always_inline]] inline const Type& operator*=( float val )
	{
		return base::Set( this->m_Value * val );
	}

	[[gnu::always_inline]] inline const Type operator*( float val ) const
	{
		return this->m_Value * val; 
	}

	[[gnu::always_inline]] inline const Type operator/( const Type &val ) const
	{
		return this->m_Value / val; 
	}

protected:
	[[gnu::always_inline]] inline void DetectChange( float &out, float in ) 
	{
		if ( out != in ) 
		{
			this->NetworkStateChanged();
			out = in;
		}
	}
};



//
// This is for a CNetworkVector that only generates one change offset.
// It should only ever be used with SendPropVector/QAngle.
//
// Single-component things like SendPropFloat should never refer to it because
// they require the network var to report an offset for each component.
//
template< class Type, class Changer >
class CNetworkVectorBase : public CNetworkVectorCommonBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:
	static constexpr int GetNetworkVarFlags() { return NETWORKVAR_IS_A_VECTOR; }

	[[gnu::always_inline]] inline const Type& operator=( const Type &val ) 
	{ 
		return base::Set( val ); 
	}

	[[gnu::always_inline]] inline const Type& operator=( const CNetworkVectorBase<Type,Changer> &val ) 
	{ 
		return base::Set( val.m_Value );
	}
	[[gnu::always_inline]] inline void SetX( float val ) { this->DetectChange( this->m_Value.x, val ); }
	[[gnu::always_inline]] inline void SetY( float val ) { this->DetectChange( this->m_Value.y, val ); }
	[[gnu::always_inline]] inline void SetZ( float val ) { this->DetectChange( this->m_Value.z, val ); }
	[[gnu::always_inline]] inline void Set( int i, float val ) { this->DetectChange( this->m_Value[i], val ); }

	[[gnu::always_inline]] inline const Type& operator*=( float val )
	{
		return base::Set( this->m_Value * val );
	}
};


//
// This variant of a CNetworkVector should be used if you want to use SendPropFloat
// on each individual component of the vector.
//
template< class Type, class Changer >
class CNetworkVectorXYZBase : public CNetworkVectorCommonBase< Type, Changer >
{
	typedef CNetworkVectorCommonBase< Type, Changer > base;
public:
	
	static constexpr int GetNetworkVarFlags() { return NETWORKVAR_IS_A_VECTOR | NETWORKVAR_VECTOR_XYZ_FLAG; }

	[[gnu::always_inline]] inline const Type& operator=( const Type &val ) 
	{ 
		return Set( val ); 
	}

	[[gnu::always_inline]] inline const Type& operator=( const CNetworkVectorBase<Type,Changer> &val ) 
	{ 
		return Set( val.m_Value );
	}

	[[gnu::always_inline]] inline const Type& Set( const Type &val )
	{
		SetX( val.x );
		SetY( val.y );
		SetZ( val.z );

		return this->m_Value;
	}
	
	[[gnu::always_inline]] inline Type& GetForModify()
	{
		this->NetworkStateChanged( &((float*)this)[0] );
		this->NetworkStateChanged( &((float*)this)[1] );
		this->NetworkStateChanged( &((float*)this)[2] );
		return this->m_Value;
	}

	[[gnu::always_inline]] inline const Type& SetDirect( const Type &val )
	{
		GetForModify() = val;
		return this->m_Value;
	}

	[[gnu::always_inline]] inline void SetX( float val ) { DetectChange( 0, val ); }
	[[gnu::always_inline]] inline void SetY( float val ) { DetectChange( 1, val ); }
	[[gnu::always_inline]] inline void SetZ( float val ) { DetectChange( 2, val ); }
	[[gnu::always_inline]] inline void Set( int i, float val ) { DetectChange( i, val ); }

	[[gnu::always_inline]] inline const Type& operator+=( const Type &val )
	{
		return Set( this->m_Value + val );
	}

	[[gnu::always_inline]] inline const Type& operator-=( const Type &val )
	{
		return Set( this->m_Value - val );
	}

	[[gnu::always_inline]] inline const Type& operator*=( float val )
	{
		return Set( this->m_Value * val );
	}

	[[gnu::always_inline]] inline const Type& operator/=( float val )
	{
		return Set( this->m_Value / val );
	}

private:
	[[gnu::always_inline]] inline void DetectChange( int nComponent, float in ) 
	{
		float *pVar = &((float*)this)[nComponent];
		if ( *pVar != in ) 
		{
			if ( pVar != &((float*)this)[0] )
			{
				this->NetworkStateChanged( &((float*)this)[0] ); // Always mark the start of the vector as changed
			}
			this->NetworkStateChanged( pVar );
			*pVar = in;
		}
	}
};



//
// This variant of a CNetworkVector should be used if you want to use SendPropVectorXY
// for the XY components and SendPropFloat for the Z component.
//
template< class Type, class Changer >
class CNetworkVectorXY_SeparateZBase : public CNetworkVectorCommonBase< Type, Changer >
{
	typedef CNetworkVectorCommonBase< Type, Changer > base;
public:
	
	static constexpr int GetNetworkVarFlags() { return NETWORKVAR_IS_A_VECTOR | NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG; }

	[[gnu::always_inline]] inline const Type& operator=( const Type &val ) 
	{ 
		return Set( val ); 
	}

	[[gnu::always_inline]] inline const Type& operator=( const CNetworkVectorBase<Type,Changer> &val ) 
	{ 
		return Set( val.m_Value );
	}

	[[gnu::always_inline]] inline const Type& Set( const Type &val )
	{
		SetX( val.x );
		SetY( val.y );
		SetZ( val.z );

		return this->m_Value;
	}
	
	[[gnu::always_inline]] inline Type& GetForModify()
	{
		this->NetworkStateChanged( &((float*)this)[0] );	// Mark the offset of our XY SendProp as changed.
		this->NetworkStateChanged( &((float*)this)[2] );	// Mark the offset of our Z SendProp as changed.
		return this->m_Value;
	}

	[[gnu::always_inline]] inline const Type& SetDirect( const Type &val )
	{
		GetForModify() = val;
		return this->m_Value;
	}

	[[gnu::always_inline]] inline void SetX( float val ) { DetectChange( 0, val ); }
	[[gnu::always_inline]] inline void SetY( float val ) { DetectChange( 1, val ); }
	[[gnu::always_inline]] inline void SetZ( float val ) { DetectChange( 2, val ); }
	[[gnu::always_inline]] inline void Set( int i, float val ) { DetectChange( i, val ); }

	[[gnu::always_inline]] inline const Type& operator+=( const Type &val )
	{
		return Set( this->m_Value + val );
	}

	[[gnu::always_inline]] inline const Type& operator-=( const Type &val )
	{
		return Set( this->m_Value - val );
	}

	[[gnu::always_inline]] inline const Type& operator*=( float val )
	{
		return Set( this->m_Value * val );
	}

	[[gnu::always_inline]] inline const Type& operator/=( float val )
	{
		return Set( this->m_Value / val );
	}

private:
	[[gnu::always_inline]] inline void DetectChange( int nComponent, float in ) 
	{
		float *pVar = &((float*)this)[nComponent];
		if ( *pVar != in ) 
		{
			this->NetworkStateChanged( &((float*)this)[0] );	// Mark the offset of our XY SendProp as changed.
			this->NetworkStateChanged( &((float*)this)[2] );	// Mark the offset of our Z SendProp as changed.

			*pVar = in;
		}
	}
};
