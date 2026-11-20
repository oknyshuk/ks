import ks.core.basetypes;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#ifndef NETWORKVAR_H
#define NETWORKVAR_H

import std;


#if defined( CLIENT_DLL ) || defined( GAME_DLL )
	#include "basehandle.h"
#endif



#define MyOffsetOf( type, var ) ( (int)(intp)&((type*)0)->var )

#ifdef _DEBUG
	#undef new
	extern bool g_bUseNetworkVars;
	#define CHECK_USENETWORKVARS if(g_bUseNetworkVars)

	// Use START_ and END_CHECK_USENETWORKVARS if your code is longer than one line.
	#define START_CHECK_USENETWORKVARS if(g_bUseNetworkVars) {
	#define END_CHECK_USENETWORKVARS }
#else
	#define CHECK_USENETWORKVARS // don't check for g_bUseNetworkVars
	#define START_CHECK_USENETWORKVARS
	#define END_CHECK_USENETWORKVARS
#endif



//
// Networkvar flags.
//
inline constexpr auto NETWORKVAR_IS_A_VECTOR = 0x0001; // Is it any type of network vector?
inline constexpr auto NETWORKVAR_VECTOR_XYZ_FLAG = 0x0002; // Is it a CNetworkVectorXYZ?
inline constexpr auto NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG = 0x0004; // Is it a CNetworkVectorXY_SeparateZ?

#define NETWORKVAR_ALL_FLAGS ( NETWORKVAR_IS_A_VECTOR | NETWORKVAR_VECTOR_XYZ_FLAG | NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG )



#include "declare_class.h"




// All classes that contain CNetworkVars need a NetworkStateChanged() function. If the class is not an entity,
// it needs to forward the call to the entity it's in. These macros can help.
	
	// These macros setup an entity pointer in your class. Use IMPLEMENT_NETWORKVAR_CHAIN before you do
	// anything inside the class itself.
	// The entity type is the user's: game entities belong to the side module that defines them.
	template< class Entity >
	class CAutoInitEntPtr
	{
	public:
		Entity *m_pEnt = nullptr;
	};

	//TODO: Currently, these don't get the benefit of tracking changes to individual vars.
	// Would be nice if they did.
	#define DECLARE_NETWORKVAR_CHAIN() \
		CAutoInitEntPtr< CBaseEntity > __m_pChainEntity; \
		void NetworkStateChanged() { CHECK_USENETWORKVARS __m_pChainEntity.m_pEnt->NetworkStateChanged(); } \
		void NetworkStateChanged( void *pVar ) { CHECK_USENETWORKVARS __m_pChainEntity.m_pEnt->NetworkStateChanged(); }

	#define IMPLEMENT_NETWORKVAR_CHAIN( varName ) \
		(varName)->__m_pChainEntity.m_pEnt = this;



// Use this macro when you want to embed a structure inside your entity and have CNetworkVars in it.
template< class T >
inline void DispatchNetworkStateChanged( T *pObj )
{
	CHECK_USENETWORKVARS pObj->NetworkStateChanged();
}
template< class T >
inline void DispatchNetworkStateChanged( T *pObj, void *pVar )
{
	CHECK_USENETWORKVARS pObj->NetworkStateChanged( pVar );
}

#define DECLARE_EMBEDDED_NETWORKVAR() \
	template <typename T> friend int ServerClassInit( T * ); \
	template <typename T> friend int ClientClassInit( T * ); \
	virtual void NetworkStateChanged() {} virtual void NetworkStateChanged( void *pProp ) {}

template < typename T, typename ContainingClass, typename GetOffset >
class NetworkVarEmbedded : public T
{
private:
	// NOTE: Assignment operator is disabled because it doesn't call copy constructors of scalar types within the aggregate, so they are not marked changed
	template< class U > NetworkVarEmbedded& operator=( U &&val ) = delete;

	T& Data() { return *this; }
	const T& Data() const { return *this; }

public:
	operator const T& () const { return Data(); }
	
	// $$$REI TODO: Why doesn't this implementation work as an assignment operator?
	template< typename U >
	void CopyFrom( U&& src ) {
		Data() = std::forward<U>( src );
		NetworkStateChanged();
	}

	const T* Get( void ) const { return &Data(); }
	T* Get( void ) { return &Data(); }

	ContainingClass* GetEmbeddedVarOuterPtr() { return ( ContainingClass* )( ( ( char* )this ) - GetOffset::Get() ); }
	virtual void NetworkStateChanged() 
	{
		ContainingClass *pOuter = GetEmbeddedVarOuterPtr();
		DispatchNetworkStateChanged( pOuter );
	}

	virtual void NetworkStateChanged( void *pVar )
	{
		ContainingClass *pOuter = GetEmbeddedVarOuterPtr();
		DispatchNetworkStateChanged( pOuter, pVar );
	}
};

// The trailing ... is the annotation pass-through every other wrapper in this family has; an
// embedded member needs one too, because DEFINE_PRED_TYPEDESCRIPTION names one (C_BasePlayer's
// m_Local).
#define CNetworkVarEmbedded( type, name, ... ) \
    struct GetOffset_##name{ [[gnu::always_inline]] static inline int Get() { return MyOffsetOf( ThisClass, name ); } }; \
	typedef NetworkVarEmbedded< type, ThisClass, GetOffset_##name > NetworkVar_##name; \
	friend class NetworkVarEmbedded< type, ThisClass, GetOffset_##name >; \
	__VA_ARGS__ NetworkVar_##name name;

// Zero the object -- necessary for CNetworkVar and possibly other cases.
template<typename T>
[[gnu::always_inline]] inline void EnsureValidValue( T &x ) { x = T(0); }
// For types that cannot compile the line above (such as QAngle, Vector, etc.)
// define a EnsureValidValue overload in the appropriate header files.

class CNetworkVarFlagsBase
{
public:
	// A combination of NETWORKVAR_ flags. SendProps store these for additional information
	// about what kind of CNetworkVar the SendProp is linked to.
	static constexpr int GetNetworkVarFlags() { return 0; }
};


// when we box ints into floats, the comparison of floats doesn't work

template <typename T>
inline bool NetworkParanoidUnequal( const T &a, const T &b )
{
	return a != b;
}

template <>
inline bool NetworkParanoidUnequal( const float &a, const float &b )
{
	//return 0 != memcmp( &a, &b, sizeof( a ) );
	union
	{
		float f32;
		uint32 u32;
	} p, q;
	p.f32 = a;
	q.f32 = b;
	return p.u32 != q.u32;
}

template <>
inline bool NetworkParanoidUnequal( const double &a, const double &b )
{
	// return 0 != memcmp( &a, &b, sizeof( a ) );
	union
	{
		double f64;
		uint64 u64;
	} p, q;
	p.f64 = a;
	q.f64 = b;
	return p.u64 != q.u64;
}

// The wire payload is the single data member this class declares; see reflect_wirevar.h.
template< class Type, class Changer >
class CNetworkVarBase : public CNetworkVarFlagsBase
{
	[[= ks::reflect::WireVar{} ]] static constexpr bool ks_class = true;

public:
	CNetworkVarBase()
	{
		// Always initialize m_Value. Not doing this means that the initial behavior
		// is random. For float types this random behavior can persist arbitrarily
		// long because of our non-IEEE comparisons which may make NaN be equal to
		// everything.
		EnsureValidValue( m_Value );
	}

	[[gnu::always_inline]] inline explicit CNetworkVarBase( Type val )
	: m_Value( val )
	{
		NetworkStateChanged();
	}

	[[gnu::always_inline]] inline const Type& SetDirect( const Type &val )
	{
		NetworkStateChanged();
		m_Value = val;
		return m_Value;
	}

	[[gnu::always_inline]] inline const Type& Set( const Type &val )
	{
		if ( NetworkParanoidUnequal( m_Value, val ) )
		{
			NetworkStateChanged();
			m_Value = val;
		}
		return m_Value;
	}
	
	template< class C >
	[[gnu::always_inline]] inline const Type& operator=( const C &val ) 
	{ 
		return Set( ( const Type )val ); 
	}

	template< class C >
	[[gnu::always_inline]] inline const Type& operator=( const CNetworkVarBase< C, Changer > &val ) 
	{ 
		return Set( ( const Type )val.m_Value ); 
	}

	[[gnu::always_inline]] inline Type& GetForModify()
	{
		NetworkStateChanged();
		return m_Value;
	}

	template< class C >
	[[gnu::always_inline]] inline const Type& operator+=( const C &val ) 
	{
		return Set( m_Value + ( const Type )val ); 
	}

	template< class C >
	[[gnu::always_inline]] inline const Type& operator-=( const C &val ) 
	{
		return Set( m_Value - ( const Type )val ); 
	}
	
	template< class C >
	[[gnu::always_inline]] inline const Type& operator/=( const C &val ) 
	{
		return Set( m_Value / ( const Type )val ); 
	}
	
	template< class C >
	[[gnu::always_inline]] inline const Type& operator*=( const C &val ) 
	{
		return Set( m_Value * ( const Type )val ); 
	}
	
	template< class C >
	[[gnu::always_inline]] inline const Type& operator^=( const C &val ) 
	{
		return Set( m_Value ^ ( const Type )val ); 
	}

	template< class C >
	[[gnu::always_inline]] inline const Type& operator|=( const C &val ) 
	{
		return Set( m_Value | ( const Type )val ); 
	}

	[[gnu::always_inline]] inline const Type& operator++()
	{
		return (*this += 1);
	}

	[[gnu::always_inline]] inline Type operator--()
	{
		return (*this -= 1);
	}
	
	[[gnu::always_inline]] inline Type operator++( int ) // postfix version..
	{
		Type val = m_Value;
		(*this += 1);
		return val;
	}

	[[gnu::always_inline]] inline Type operator--( int ) // postfix version..
	{
		Type val = m_Value;
		(*this -= 1);
		return val;
	}
	
	// For some reason the compiler only generates type conversion warnings for this operator when used like 
	// CNetworkVarBase<unsigned char> = 0x1
	// (it warns about converting from an int to an unsigned char).
	template< class C >
	[[gnu::always_inline]] inline const Type& operator&=( const C &val ) 
	{	
		return Set( m_Value & ( const Type )val ); 
	}

	[[gnu::always_inline]] inline operator const Type&() const 
	{
		return m_Value; 
	}
	
	[[gnu::always_inline]] inline const Type& Get() const 
	{
		return m_Value; 
	}
	
	[[gnu::always_inline]] inline const Type* operator->() const 
	{
		return &m_Value; 
	}

	Type m_Value;

protected:
	[[gnu::always_inline]] inline void NetworkStateChanged()
	{
		Changer::NetworkStateChanged( this );
	}
	
	[[gnu::always_inline]] inline void NetworkStateChanged( void *pVar )
	{
		Changer::NetworkStateChanged( this, pVar );
	}
};



#include "networkvar_vector.h"



template< class Type, class Changer >
class CNetworkColor32Base : public CNetworkVarBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:
	inline void Init( byte rVal, byte gVal, byte bVal )
	{
		SetR( rVal );
		SetG( gVal );
		SetB( bVal );
	}
	inline void Init( byte rVal, byte gVal, byte bVal, byte aVal )
	{
		SetR( rVal );
		SetG( gVal );
		SetB( bVal );
		SetA( aVal );
	}

	const Type& operator=( const Type &val ) 
	{ 
		return this->Set( val );
	}

	const Type& operator=( const CNetworkColor32Base<Type,Changer> &val ) 
	{ 
		return base::Set( val.m_Value );
	}
	
	inline byte GetR() const { return this->m_Value.r; }
	inline byte GetG() const { return this->m_Value.g; }
	inline byte GetB() const { return this->m_Value.b; }
	inline byte GetA() const { return this->m_Value.a; }
	inline void SetR( byte val ) { SetVal( this->m_Value.r, val ); }
	inline void SetG( byte val ) { SetVal( this->m_Value.g, val ); }
	inline void SetB( byte val ) { SetVal( this->m_Value.b, val ); }
	inline void SetA( byte val ) { SetVal( this->m_Value.a, val ); }

protected:
	inline void SetVal( byte &out, const byte &in )
	{
		if ( out != in )
		{
			this->NetworkStateChanged();
			out = in;
		}
	}
};


// Network quaternion wrapper.
template< class Type, class Changer >
class CNetworkQuaternionBase : public CNetworkVarBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:
	inline void Init( float ix=0, float iy=0, float iz=0, float iw = 0 ) 
	{
		base::Set( Quaternion( ix, iy, iz, iw ) );
	}
	
	const Type& operator=( const Type &val ) 
	{ 
		return Set( val ); 
	}

	const Type& operator=( const CNetworkQuaternionBase<Type,Changer> &val ) 
	{ 
		return Set( val.m_Value );
	}

	inline float GetX() const { return this->m_Value.x; }
	inline float GetY() const { return this->m_Value.y; }
	inline float GetZ() const { return this->m_Value.z; }
	inline float GetW() const { return this->m_Value.w; }
	inline float operator[]( int i ) const { return this->m_Value[i]; }

	inline void SetX( float val ) { DetectChange( this->m_Value.x, val ); }
	inline void SetY( float val ) { DetectChange( this->m_Value.y, val ); }
	inline void SetZ( float val ) { DetectChange( this->m_Value.z, val ); }
	inline void SetW( float val ) { DetectChange( this->m_Value.w, val ); }
	inline void Set( int i, float val ) { DetectChange( this->m_Value[i], val ); }

	bool operator==( const Type &val ) const 
	{ 
		return !NetworkParanoidUnequal( this->m_Value, ( Type )val );
	}

	bool operator!=( const Type &val ) const 
	{
		return NetworkParanoidUnequal( this->m_Value, val );
	}

	const Type operator+( const Type &val ) const 
	{
		return this->m_Value + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return this->m_Value - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return this->m_Value * val; 
	}

	const Type& operator*=( float val )
	{
		return Set( this->m_Value * val );
	}

	const Type operator*( float val ) const
	{
		return this->m_Value * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return this->m_Value / val; 
	}

private:
	inline void DetectChange( float &out, float in ) 
	{
		if ( NetworkParanoidUnequal( out, in ) )
		{
			this->NetworkStateChanged();
			out = in;
		}
	}
};


// Network ehandle wrapper.
#if defined( CLIENT_DLL ) || defined( GAME_DLL )
	// EntityFromHandle is the game's (ehandle.cppm of each side): every call below depends on the
	// template, so it is found where the template is instantiated.

	template< class Type, class Changer >
	class CNetworkHandleBase : public CNetworkVarBase< CBaseHandle, Changer >
	{
		typedef CNetworkVarBase< CBaseHandle, Changer > base;
	public:
		const Type* operator=( const Type *val ) 
		{ 
			return Set( val ); 
		}
			
		const Type& operator=( const CNetworkHandleBase<Type,Changer> &val ) 
		{ 
			return *(const Type*)EntityFromHandle( CNetworkVarBase<CBaseHandle,Changer>::Set( val.m_Value ) );
		}

		bool operator !() const 
		{ 
			return !EntityFromHandle( this->m_Value ); 
		}
		
		operator Type*() const 
		{ 
			return static_cast< Type* >( EntityFromHandle( this->m_Value ) );
		}

		const Type* Set( const Type *val )
		{
			if ( CNetworkHandleBase<Type,Changer>::m_Value != val )
			{
				this->NetworkStateChanged();
				CNetworkHandleBase<Type,Changer>::m_Value = val;
			}
			return val;
		}
		
		Type* Get() const 
		{ 
			return static_cast< Type* >( EntityFromHandle( CNetworkHandleBase<Type,Changer>::m_Value ) );
		}

		Type* operator->() const 
		{ 
			return static_cast< Type* >( EntityFromHandle( CNetworkHandleBase<Type,Changer>::m_Value ) );
		}

		bool operator==( const Type *val ) const 
		{
			return CNetworkHandleBase<Type,Changer>::m_Value == val; 
		}

		bool operator!=( const Type *val ) const 
		{
			return CNetworkHandleBase<Type,Changer>::m_Value != val;
		}
	};



	#define CNetworkHandle( type, name, ... ) CNetworkHandleInternal( type, name, NetworkStateChanged __VA_OPT__(,) __VA_ARGS__ )

	#define CNetworkHandleInternal( type, name, stateChangedFn, ... ) \
		NETWORK_VAR_START( type, name ) \
		NETWORK_VAR_END( type, name, CNetworkHandleBase, stateChangedFn __VA_OPT__(,) __VA_ARGS__ )
#endif


// Use this macro to define a network variable.
#define CNetworkVar( type, name, ... ) \
	NETWORK_VAR_START( type, name ) \
	NETWORK_VAR_END( type, name, CNetworkVarBase, NetworkStateChanged __VA_OPT__(,) __VA_ARGS__ )


// A base class's variable that only some derived classes send. Changes mark the entity only once a
// derived class has said it sends the variable, with IMPLEMENT_NETWORK_VAR_FOR_DERIVED. The switch is
// a flag on the entity rather than a virtual override, so a base without a vtable (CEntityCore) can
// declare one too.
#define NETWORK_VAR_FOR_DERIVED_HOOK( name ) \
	bool m_bNetworkVarForDerived_##name = false; \
	void NetworkStateChanged_##name( void *pVar ) { CHECK_USENETWORKVARS if ( m_bNetworkVarForDerived_##name ) NetworkStateChanged( pVar ); }

#define CNetworkVarForDerived( type, name, ... ) \
	NETWORK_VAR_FOR_DERIVED_HOOK( name ) \
	NETWORK_VAR_START( type, name ) \
	NETWORK_VAR_END( type, name, CNetworkVarBase, NetworkStateChanged_##name __VA_OPT__(,) __VA_ARGS__ )

#define CNetworkHandleForDerived( type, name, ... ) \
	NETWORK_VAR_FOR_DERIVED_HOOK( name ) \
	CNetworkHandleInternal( type, name, NetworkStateChanged_##name __VA_OPT__(,) __VA_ARGS__ )

#define CNetworkArrayForDerived( type, name, count, ... ) \
	NETWORK_VAR_FOR_DERIVED_HOOK( name ) \
	CNetworkArrayInternal( type, name, count, NetworkStateChanged_##name __VA_OPT__(,) __VA_ARGS__ )

// The flag a ForDerived variable's hook checks, found in the class or a base. A splice is not
// access-checked, so a derived class can switch on a variable its base declares private.
consteval std::meta::info find_network_var_for_derived_flag( std::meta::info t, std::string_view name )
{
	for ( auto m : std::meta::nonstatic_data_members_of( t, std::meta::access_context::unchecked() ) )
		if ( std::meta::has_identifier( m ) && std::meta::identifier_of( m ) == name ) return m;
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
	{
		const std::meta::info r = find_network_var_for_derived_flag( std::meta::type_of( b ), name );
		if ( r != std::meta::info{} ) return r;
	}
	return {};
}

consteval std::meta::info network_var_for_derived_flag( std::meta::info t, std::string_view name )
{
	const std::meta::info r = find_network_var_for_derived_flag( t, name );
	if ( r == std::meta::info{} )
		throw std::meta::exception( "no ForDerived network var of that name", t );
	return r;
}

// Set during construction, from a default member initialiser, once the bases exist. The member is
// empty and of its own type, so it takes no space.
#define IMPLEMENT_NETWORK_VAR_FOR_DERIVED( name ) \
	struct NetworkVarForDerivedOn_##name {}; \
	[[no_unique_address]] NetworkVarForDerivedOn_##name m_NetworkVarForDerivedOn_##name = \
		( this->[: network_var_for_derived_flag( ^^ThisClass, "m_bNetworkVarForDerived_" #name ) :] = true, \
		  NetworkVarForDerivedOn_##name{} );



#define CNetworkQuaternion( name, ... ) \
	NETWORK_VAR_START( Quaternion, name ) \
	NETWORK_VAR_END( Quaternion, name, CNetworkQuaternionBase, NetworkStateChanged __VA_OPT__(,) __VA_ARGS__ )

// Helper for color32's. Contains GetR(), SetR(), etc.. functions.
#define CNetworkColor32( name, ... ) \
	NETWORK_VAR_START( color32, name ) \
	NETWORK_VAR_END( color32, name, CNetworkColor32Base, NetworkStateChanged __VA_OPT__(,) __VA_ARGS__ )


#define CNetworkString( name, length, ... ) \
	class NetworkVar_##name; \
	friend class NetworkVar_##name; \
	typedef ThisClass MakeANetworkVar_##name; \
	class NetworkVar_##name : public CNetworkVarFlagsBase\
	{ \
		[[= ks::reflect::WireVar{} ]] static constexpr bool ks_class = true; \
	public: \
		operator const char*() const { return m_Value; } \
		const char* Get() const { return m_Value; } \
		char* GetForModify() \
		{ \
			NetworkStateChanged(); \
			return m_Value; \
		} \
	protected: \
		inline void NetworkStateChanged() \
		{ \
			START_CHECK_USENETWORKVARS \
			ThisClass *pThis = ((ThisClass*)(((char*)this) - MyOffsetOf(ThisClass,name))); \
			pThis->NetworkStateChanged(); \
			END_CHECK_USENETWORKVARS \
		} \
	private: \
		char m_Value[length]; \
	}; \
	typedef NetworkVar_##name NetworkVarType_##name; \
	__VA_ARGS__ NetworkVar_##name name;




// Use this to define networked arrays.
// You can access elements for reading with operator[], and you can set elements with the Set() function.
#define CNetworkArrayInternal( type, name, count, stateChangedFn, ... ) \
	class NetworkVar_##name; \
	friend class NetworkVar_##name; \
	typedef ThisClass MakeANetworkVar_##name; \
	class NetworkVar_##name \
	{ \
		[[= ks::reflect::WireVar{} ]] static constexpr bool ks_class = true; \
	public: \
		template <typename T> friend int ServerClassInit(T *);	\
		const type& operator[]( int i ) const \
		{ \
			return Get( i ); \
		} \
		\
		const type& Get( int i ) const \
		{ \
			contract_assert( i >= 0 && i < count ); \
			return m_Value[i]; \
		} \
		\
		type& GetForModify( int i ) \
		{ \
			contract_assert( i >= 0 && i < count ); \
			NetworkStateChanged( i ); \
			return m_Value[i]; \
		} \
		\
		void Set( int i, const type &val ) \
		{ \
			contract_assert( i >= 0 && i < count ); \
			if( std::memcmp( &m_Value[i], &val, sizeof(type) ) ) \
			{ \
				NetworkStateChanged( i ); \
			       	m_Value[i] = val; \
			} \
		} \
		const type* Base() const { return m_Value; } \
		int Count() const { return count; } \
		type m_Value[count]; \
	protected: \
		inline void NetworkStateChanged( int index ) \
		{ \
			START_CHECK_USENETWORKVARS \
			ThisClass *pThis = ((ThisClass*)(((char*)this) - MyOffsetOf(ThisClass,name))); \
			pThis->stateChangedFn( &m_Value[index] ); \
			END_CHECK_USENETWORKVARS \
		} \
	}; \
	__VA_ARGS__ NetworkVar_##name name;


#define CNetworkArray( type, name, count, ... )  CNetworkArrayInternal( type, name, count, NetworkStateChanged __VA_OPT__(,) __VA_ARGS__ )


// Internal macros used in definitions of network vars.
#define NETWORK_VAR_START( networkVarType, networkVarName ) \
	class NetworkVar_##networkVarName; \
	friend class NetworkVar_##networkVarName; \
	typedef ThisClass MakeANetworkVar_##networkVarName; \
	class NetworkVar_##networkVarName \
	{ \
	public: \
		template <typename T> friend int ServerClassInit(T *);


// pChangedPtr could be the Y or Z component of a vector, so possibly != to pNetworkVar.
// Trailing ... carries annotations onto the member declared on the last line; an attribute
// before CNetworkVar(...) would silently attach to the NetworkVar_##name forward decl.
#define NETWORK_VAR_END( networkVarType, networkVarName, baseClassName, stateChangedFn, ... ) \
	public: \
		static inline void NetworkStateChanged( void *pNetworkVar, void *pChangedPtr ) \
		{ \
			START_CHECK_USENETWORKVARS \
			ThisClass *pThis = ( (ThisClass*)(((char*)pNetworkVar) - MyOffsetOf(ThisClass,networkVarName)) ); \
			pThis->stateChangedFn( pChangedPtr ); \
			END_CHECK_USENETWORKVARS \
		} \
		static inline void NetworkStateChanged( void *pNetworkVar ) \
		{ \
			NetworkStateChanged( pNetworkVar, pNetworkVar ); \
		} \
	}; \
	typedef baseClassName< networkVarType, NetworkVar_##networkVarName > NetworkVarType_##networkVarName; \
	__VA_ARGS__ baseClassName< networkVarType, NetworkVar_##networkVarName > networkVarName;



#endif // NETWORKVAR_H
