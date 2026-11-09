// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2008 Valve Corporation

// Purpose: VScript
// Overview
// VScript is an abstract binding layer that allows code to expose itself to
// multiple scripting languages in a uniform format. Code can expose
// functions, classes, and data to the scripting languages, and can also
// call functions that reside in scripts.
// Initializing
// To create a script virtual machine (VM), grab the global instance of
// IScriptManager, call CreateVM, then call Init on the returned VM. Right
// now you can have multiple VMs, but only VMs for a specific language.
// Exposing functions and classes
// To expose a C++ function to the scripting system, you just need to fill out a
// description block. Using templates, the system will automatically deduce
// all of the binding requirements (parameters and return values). Functions
// are limited as to what the types of the parameters can be. See ScriptVariant_t.
//		extern IScriptVM *pScriptVM;
//		bool Foo( int );
//		void Bar();
//		float FooBar( int, const char * );
//		float OverlyTechnicalName( bool );
//		void RegisterFuncs()
//		{
//			ScriptRegisterFunction( pScriptVM, Foo );
//			ScriptRegisterFunction( pScriptVM, Bar );
//			ScriptRegisterFunction( pScriptVM, FooBar );
//			ScriptRegisterFunctionNamed( pScriptVM, OverlyTechnicalName, "SimpleName" );
//		}
//		class CMyClass
//		public:
//			bool Foo( int );
//			void Bar();
//			float FooBar( int, const char * );
//			float OverlyTechnicalName( bool );
//		};
//		BEGIN_SCRIPTDESC_ROOT( CMyClass )
//			DEFINE_SCRIPTFUNC( Foo )
//			DEFINE_SCRIPTFUNC( Bar )
//			DEFINE_SCRIPTFUNC( FooBar )
//			DEFINE_SCRIPTFUNC_NAMED( OverlyTechnicalName, "SimpleMemberName" )
//		END_SCRIPTDESC();
//		class CMyDerivedClass : public CMyClass
//			float DerivedFunc() const;
//		BEGIN_SCRIPTDESC( CMyDerivedClass, CMyClass )
//			DEFINE_SCRIPTFUNC( DerivedFunc )
//		CMyDerivedClass derivedInstance;
//		void AnotherFunction()
//			// Manual class exposure
//			pScriptVM->RegisterClass( GetScriptDescForClass( CMyClass ) );
//			// Auto registration by instance
//			pScriptVM->RegisterInstance( &derivedInstance, "theInstance" );
// Classes with "DEFINE_SCRIPT_CONSTRUCTOR()" in their description can be instanced within scripts
// Scopes

// ------
// Scripts can either be run at the global scope, or in a user defined scope. In the latter case,
// all "globals" within the script are actually in the scope. This can be used to bind private
// data spaces with C++ objects.
//
// Calling a function on a script
// ------------------------------
// Generally, use the "Call" functions. This example is the equivalent of DoIt("Har", 6.0, 99).
//
// 		hFunction = pScriptVM->LookupFunction( "DoIt", hScope );
// 		pScriptVM->Call( hFunction, hScope, true, NULL, "Har", 6.0, 99 );
// 
//=============================================================================

#ifndef IVSCRIPT_H
#define IVSCRIPT_H

import ks.core;
#include "datamap.h"
import ks.tier1;
import std;

namespace ks { class byte_writer; class byte_reader; }

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

#define VSCRIPT_INTERFACE_VERSION		"VScriptManager009"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class IScriptVM;

enum ScriptLanguage_t
{
	SL_NONE,
	SL_GAMEMONKEY,
	SL_SQUIRREL,
	SL_LUA,
	SL_PYTHON,

	SL_DEFAULT = SL_SQUIRREL
};

class IScriptManager
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	IScriptVM *CreateVM( ScriptLanguage_t language = SL_DEFAULT );
	void DestroyVM( IScriptVM * );
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

struct HSCRIPT__ { int unused; };
using HSCRIPT = HSCRIPT__ *;
#define INVALID_HSCRIPT	((HSCRIPT)-1)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

enum ExtendedFieldType
{
	FIELD_TYPEUNKNOWN = FIELD_TYPECOUNT,
	FIELD_CSTRING,
	FIELD_HSCRIPT,
	FIELD_VARIANT,
};

typedef int ScriptDataType_t;
struct ScriptVariant_t;

template <typename T> struct ScriptDeducer { /*enum { FIELD_TYPE = FIELD_TYPEUNKNOWN };*/ };
#define DECLARE_DEDUCE_FIELDTYPE( fieldType, type ) template<> struct ScriptDeducer<type> { enum { FIELD_TYPE = fieldType }; };

DECLARE_DEDUCE_FIELDTYPE( FIELD_VOID,		void );
DECLARE_DEDUCE_FIELDTYPE( FIELD_FLOAT,		float );
DECLARE_DEDUCE_FIELDTYPE( FIELD_CSTRING,	const char * );
DECLARE_DEDUCE_FIELDTYPE( FIELD_CSTRING,	char * );
DECLARE_DEDUCE_FIELDTYPE( FIELD_VECTOR,		Vector );
DECLARE_DEDUCE_FIELDTYPE( FIELD_VECTOR,		const Vector &);
DECLARE_DEDUCE_FIELDTYPE( FIELD_INTEGER,	int );
DECLARE_DEDUCE_FIELDTYPE( FIELD_BOOLEAN,	bool );
DECLARE_DEDUCE_FIELDTYPE( FIELD_CHARACTER,	char );
DECLARE_DEDUCE_FIELDTYPE( FIELD_HSCRIPT,	HSCRIPT );
DECLARE_DEDUCE_FIELDTYPE( FIELD_VARIANT,	ScriptVariant_t );

template <typename T> inline constexpr ScriptDataType_t ScriptDeduceType = ScriptDeducer<T>::FIELD_TYPE;

template <typename T>
inline const char * ScriptFieldTypeName() 
{
	T::using_unknown_script_type(); 
	return nullptr;
}

#define DECLARE_NAMED_FIELDTYPE( fieldType, strName ) template <> inline const char * ScriptFieldTypeName<fieldType>() { return strName; }
DECLARE_NAMED_FIELDTYPE( void,	"void" );
DECLARE_NAMED_FIELDTYPE( float,	"float" );
DECLARE_NAMED_FIELDTYPE( const char *,	"cstring" );
DECLARE_NAMED_FIELDTYPE( char *,	"cstring" );
DECLARE_NAMED_FIELDTYPE( Vector,	"vector" );
DECLARE_NAMED_FIELDTYPE( const Vector&,	"vector" );
DECLARE_NAMED_FIELDTYPE( int,	"integer" );
DECLARE_NAMED_FIELDTYPE( bool,	"boolean" );
DECLARE_NAMED_FIELDTYPE( char,	"character" );
DECLARE_NAMED_FIELDTYPE( HSCRIPT,	"hscript" );
DECLARE_NAMED_FIELDTYPE( ScriptVariant_t,	"variant" );

inline const char * ScriptFieldTypeName( int16 eType)
{
	switch( eType )
	{
	case FIELD_VOID:	return "void";
	case FIELD_FLOAT:	return "float";
	case FIELD_CSTRING:	return "cstring";
	case FIELD_VECTOR:	return "vector";
	case FIELD_INTEGER:	return "integer";
	case FIELD_BOOLEAN:	return "boolean";
	case FIELD_CHARACTER:	return "character";
	case FIELD_HSCRIPT:	return "hscript";
	case FIELD_VARIANT:	return "variant";
	default:	return "unknown_script_type";
	}
}

//---------------------------------------------------------

struct ScriptFuncDescriptor_t
{
	ScriptFuncDescriptor_t()
	{
		m_pszFunction = nullptr;
		m_ReturnType = FIELD_TYPEUNKNOWN;
		m_pszDescription = nullptr;
	}

	const char *m_pszScriptName;
	const char *m_pszFunction;
	const char *m_pszDescription;
	ScriptDataType_t m_ReturnType;
	std::vector< ScriptDataType_t > m_Parameters;
};

//---------------------------------------------------------

// Prefix a script description with this in order to not show the function or class in help
#define SCRIPT_HIDE "@"

// Prefix a script description of a class to indicate it is a singleton and the single instance should be in the help
#define SCRIPT_SINGLETON "!"

// Prefix a script description with this to indicate it should be represented using an alternate name
#define SCRIPT_ALIAS( alias, description ) "#" alias ":" description

//---------------------------------------------------------

enum ScriptFuncBindingFlags_t
{
	SF_MEMBER_FUNC	= 0x01,
};

typedef void* ScriptFunctionBindingStorageType_t;

typedef bool (*ScriptBindingFunc_t)( ScriptFunctionBindingStorageType_t pFunction, void *pContext, ScriptVariant_t *pArguments, int nArguments, ScriptVariant_t *pReturn );

struct ScriptFunctionBinding_t
{
	ScriptFuncDescriptor_t	m_desc;
	ScriptBindingFunc_t		m_pfnBinding;
	ScriptFunctionBindingStorageType_t	m_pFunction;
	unsigned				m_flags;
};

//---------------------------------------------------------
class IScriptInstanceHelper
{
public:
	virtual void *GetProxied( void *p )												{ return p; }
	virtual bool ToString( void *p, char *pBuf, int bufSize )						{ return false; }
	virtual void *BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId )	{ return nullptr; }
};

//---------------------------------------------------------

struct ScriptClassDesc_t
{
	ScriptClassDesc_t( void (*pfnInitializer)() ) : m_pszScriptName( 0 ), m_pszClassname( 0 ), m_pszDescription( 0 ), m_pBaseDesc( 0 ), m_pfnConstruct( 0 ), m_pfnDestruct( 0 ), pHelper(nullptr) 
	{
		(*pfnInitializer)();
		ScriptClassDesc_t **ppHead = GetDescList();
		m_pNextDesc = *ppHead;
		*ppHead = this;
	}

	const char *						m_pszScriptName;
	const char *						m_pszClassname;
	const char *						m_pszDescription;
	ScriptClassDesc_t *					m_pBaseDesc;
	std::vector< ScriptFunctionBinding_t > m_FunctionBindings;

	void *(*m_pfnConstruct)();
	void (*m_pfnDestruct)( void *);
	IScriptInstanceHelper *				pHelper; // optional helper

	ScriptClassDesc_t *					m_pNextDesc;

	static ScriptClassDesc_t **GetDescList()
	{
		static ScriptClassDesc_t *pHead;
		return &pHead;
	}
};

//---------------------------------------------------------
// A simple variant type. Intentionally not full featured (no implicit conversion, no memory management)
//---------------------------------------------------------

enum SVFlags_t
{
	SV_FREE = 0x01,
};

struct ScriptVariant_t
{
	ScriptVariant_t() :						m_flags( 0 ), m_type( FIELD_VOID )		{ m_pVector = 0; }
	ScriptVariant_t( int val ) :			m_flags( 0 ), m_type( FIELD_INTEGER )	{ m_int = val;}
	ScriptVariant_t( float val ) :			m_flags( 0 ), m_type( FIELD_FLOAT )		{ m_float = val; }
	ScriptVariant_t( double val ) :			m_flags( 0 ), m_type( FIELD_FLOAT )		{ m_float = (float)val; }
	ScriptVariant_t( char val ) :			m_flags( 0 ), m_type( FIELD_CHARACTER )	{ m_char = val; }
	ScriptVariant_t( bool val ) :			m_flags( 0 ), m_type( FIELD_BOOLEAN )	{ m_bool = val; }
	ScriptVariant_t( HSCRIPT val ) :		m_flags( 0 ), m_type( FIELD_HSCRIPT )	{ m_hScript = val; }

	ScriptVariant_t( const Vector &val, bool bCopy = false ) :	m_flags( 0 ), m_type( FIELD_VECTOR )	{ if ( !bCopy ) { m_pVector = &val; } else { m_pVector = new Vector( val ); m_flags |= SV_FREE; } }
	ScriptVariant_t( const Vector *val, bool bCopy = false ) :	m_flags( 0 ), m_type( FIELD_VECTOR )	{ if ( !bCopy ) { m_pVector = val; } else { m_pVector = new Vector( *val ); m_flags |= SV_FREE; } }
	ScriptVariant_t( const char *val , bool bCopy = false ) :	m_flags( 0 ), m_type( FIELD_CSTRING )	{ if ( !bCopy ) { m_pszString = val; } else { m_pszString = V_strdup( val ); m_flags |= SV_FREE; } }

	bool IsNull() const						{ return (m_type == FIELD_VOID ); }

	operator int() const					{ contract_assert( m_type == FIELD_INTEGER );	return m_int; }
	operator int64() const					{ contract_assert( m_type == FIELD_INTEGER );	return static_cast<int64>(m_int); }
	operator float() const					{ contract_assert( m_type == FIELD_FLOAT );		return m_float; }
	operator const char *() const			{ contract_assert( m_type == FIELD_CSTRING );	return ( m_pszString ) ? m_pszString : ""; }
	operator const Vector &() const			{ contract_assert( m_type == FIELD_VECTOR );		static Vector vecNull(0, 0, 0); return (m_pVector) ? *m_pVector : vecNull; }
	operator char() const					{ contract_assert( m_type == FIELD_CHARACTER );	return m_char; }
	operator bool() const					{ contract_assert( m_type == FIELD_BOOLEAN );	return m_bool; }
	operator HSCRIPT() const				{ contract_assert( m_type == FIELD_HSCRIPT );	return m_hScript; }

	void operator=( int i ) 				{ m_type = FIELD_INTEGER; m_int = i; }
	void operator=( int64 i ) 				{ m_type = FIELD_INTEGER; m_int = i; }
	void operator=( float f ) 				{ m_type = FIELD_FLOAT; m_float = f; }
	void operator=( double f ) 				{ m_type = FIELD_FLOAT; m_float = (float)f; }
	void operator=( const Vector &vec )		{ m_type = FIELD_VECTOR; m_pVector = &vec; }
	void operator=( const Vector *vec )		{ m_type = FIELD_VECTOR; m_pVector = vec; }
	void operator=( const char *psz )		{ m_type = FIELD_CSTRING; m_pszString = psz; }
	void operator=( char c )				{ m_type = FIELD_CHARACTER; m_char = c; }
	void operator=( bool b ) 				{ m_type = FIELD_BOOLEAN; m_bool = b; }
	void operator=( HSCRIPT h ) 			{ m_type = FIELD_HSCRIPT; m_hScript = h; }

	void Free()
	{
		if ( !( m_flags & SV_FREE ) )
			return;

		if ( m_type == FIELD_VECTOR )
		{
			delete m_pVector;
		}
		else if ( m_type == FIELD_CSTRING )
		{
			delete[] m_pszString;
		}
	} // Generally only needed for return results

	template <typename T>
	T Get()
	{
		T value;
		AssignTo( &value );
		return value;
	}

	template <typename T>
	bool AssignTo( T *pDest )
	{
		ScriptDataType_t destType = ScriptDeduceType<T>;
		if ( destType == FIELD_TYPEUNKNOWN )
		{
			DevWarning( "Unable to convert script variant to unknown type\n" );
		}
		if ( destType == m_type )
		{
			*pDest = *this;
			return true;
		}

		if ( m_type != FIELD_VECTOR && m_type != FIELD_CSTRING && destType != FIELD_VECTOR && destType != FIELD_CSTRING )
		{
			switch ( m_type )
			{
			case FIELD_VOID:		*pDest = 0; break;
			case FIELD_INTEGER:		*pDest = m_int; return true;
			case FIELD_FLOAT:		*pDest = m_float; return true;
			case FIELD_CHARACTER:	*pDest = m_char; return true;
			case FIELD_BOOLEAN:		*pDest = m_bool; return true;
			case FIELD_HSCRIPT:		*pDest = m_hScript; return true;
			}
		}
		else
		{
			DevWarning( "No free conversion of %s script variant to %s right now\n",
				ScriptFieldTypeName( m_type ), ScriptFieldTypeName<T>() );
			if ( destType != FIELD_VECTOR )
			{
				*pDest = 0;
			}
		}
		return false;
	}

	bool AssignTo( float *pDest )
	{
		switch( m_type )
		{
		case FIELD_VOID:		*pDest = 0; return false;
		case FIELD_INTEGER:		*pDest = m_int; return true;
		case FIELD_FLOAT:		*pDest = m_float; return true;
		case FIELD_BOOLEAN:		*pDest = m_bool; return true;
		default:
			DevWarning( "No conversion from %s to float now\n", ScriptFieldTypeName( m_type ) );
			return false;
		}
	}

	bool AssignTo( int *pDest )
	{
		switch( m_type )
		{
		case FIELD_VOID:		*pDest = 0; return false;
		case FIELD_INTEGER:		*pDest = m_int; return true;
		case FIELD_FLOAT:		*pDest = ( int )m_float; return true;
		case FIELD_BOOLEAN:		*pDest = m_bool; return true;
		default:
			DevWarning( "No conversion from %s to int now\n", ScriptFieldTypeName( m_type ) );
			return false;
		}
	}

	bool AssignTo( bool *pDest )
	{
		switch( m_type )
		{
		case FIELD_VOID:		*pDest = 0; return false;
		case FIELD_INTEGER:		*pDest = m_int; return true;
		case FIELD_FLOAT:		*pDest = m_float; return true;
		case FIELD_BOOLEAN:		*pDest = m_bool; return true;
		default:
			DevWarning( "No conversion from %s to bool now\n", ScriptFieldTypeName( m_type ) );
			return false;
		}
	}

	bool AssignTo( const char **pDest )
	{
		DevWarning( "No free conversion of string or vector script variant right now\n" );
		// If want to support this, probably need to malloc string and require free on other side [3/24/2008 tom]
		*pDest = "";
		return false;
	}

	bool AssignTo( ScriptVariant_t *pDest )
	{
		pDest->m_type = m_type;
		if ( m_type == FIELD_VECTOR ) 
		{
			pDest->m_pVector = new Vector;
			((Vector *)(pDest->m_pVector))->Init( m_pVector->x, m_pVector->y, m_pVector->z );
			pDest->m_flags |= SV_FREE;
		}
		else if ( m_type == FIELD_CSTRING ) 
		{
			pDest->m_pszString = V_strdup( m_pszString );
			pDest->m_flags |= SV_FREE;
		}
		else
		{
			pDest->m_int = m_int;
		}
		return false;
	}

	union
	{
		int				m_int;
		float			m_float;
		const char *	m_pszString;
		const Vector *	m_pVector;
		char			m_char;
		bool			m_bool;
		HSCRIPT			m_hScript;
	};

	int16				m_type;
	int16				m_flags;

private:
};

#define SCRIPT_VARIANT_NULL ScriptVariant_t()

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#include "vscript_templates.h"

// Lower level macro primitives
#define ScriptInitFunctionBinding( pScriptFunction, func )									ScriptInitFunctionBindingNamed( pScriptFunction, func, #func )
#define ScriptInitFunctionBindingNamed( pScriptFunction, func, scriptName )					do { ScriptInitFuncDescriptorNamed( (&(pScriptFunction)->m_desc), func, scriptName ); (pScriptFunction)->m_pfnBinding = ScriptCreateBinding( &func ); (pScriptFunction)->m_pFunction = ScriptConvertFreeFuncPtrToVoid( &func ); } while (0)

#define ScriptInitMemberFunctionBinding( pScriptFunction, class, func )						ScriptInitMemberFunctionBinding_( pScriptFunction, class, func, #func )
#define ScriptInitMemberFunctionBindingNamed( pScriptFunction, class, func, scriptName )	ScriptInitMemberFunctionBinding_( pScriptFunction, class, func, scriptName )
#define ScriptInitMemberFunctionBinding_( pScriptFunction, class, func, scriptName ) 		do { ScriptInitMemberFuncDescriptor_( (&(pScriptFunction)->m_desc), class, func, scriptName ); (pScriptFunction)->m_pfnBinding = ScriptCreateBinding( ((class *)0), &class::func ); 	(pScriptFunction)->m_pFunction = ScriptConvertFuncPtrToVoid( &class::func ); (pScriptFunction)->m_flags = SF_MEMBER_FUNC;  } while (0)

#define ScriptInitClassDesc( pClassDesc, class, pBaseClassDesc )							ScriptInitClassDescNamed( pClassDesc, class, pBaseClassDesc, #class )
#define ScriptInitClassDescNamed( pClassDesc, class, pBaseClassDesc, scriptName )			ScriptInitClassDescNamed_( pClassDesc, class, pBaseClassDesc, scriptName )
#define ScriptInitClassDescNoBase( pClassDesc, class )										ScriptInitClassDescNoBaseNamed( pClassDesc, class, #class )
#define ScriptInitClassDescNoBaseNamed( pClassDesc, class, scriptName )						ScriptInitClassDescNamed_( pClassDesc, class, nullptr, scriptName )
#define ScriptInitClassDescNamed_( pClassDesc, class, pBaseClassDesc, scriptName )			do { (pClassDesc)->m_pszScriptName = scriptName; (pClassDesc)->m_pszClassname = #class; (pClassDesc)->m_pBaseDesc = pBaseClassDesc; } while ( 0 )

#define ScriptAddFunctionToClassDesc( pClassDesc, class, func, description  )				ScriptAddFunctionToClassDescNamed( pClassDesc, class, func, #func, description )
#define ScriptAddFunctionToClassDescNamed( pClassDesc, class, func, scriptName, description ) do { ScriptFunctionBinding_t *pBinding = &(pClassDesc)->m_FunctionBindings.emplace_back(); pBinding->m_desc.m_pszDescription = description; ScriptInitMemberFunctionBindingNamed( pBinding, class, func, scriptName );  } while (0)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#define ScriptRegisterFunction( pVM, func, description )									ScriptRegisterFunctionNamed( pVM, func, #func, description )
#define ScriptRegisterFunctionNamed( pVM, func, scriptName, description )					do { static ScriptFunctionBinding_t binding; binding.m_desc.m_pszDescription = description; binding.m_desc.m_Parameters.clear(); ScriptInitFunctionBindingNamed( &binding, func, scriptName ); pVM->RegisterFunction( &binding ); } while (0)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

// The script class descriptor macros (BEGIN_SCRIPTDESC, ...) are in vscript_game.h.

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

template <typename T>
class CScriptConstructor
{
public:
	static void *Construct()		{ return new T; }
	static void Destruct( void *p )	{ delete (T *)p; }
};

#define ScriptAddConstructorToClassDesc( pClassDesc, class )								do { (pClassDesc)->m_pfnConstruct = &CScriptConstructor<class>::Construct; (pClassDesc)->m_pfnDestruct = &CScriptConstructor<class>::Destruct; } while (0)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

enum ScriptErrorLevel_t
{
	SCRIPT_LEVEL_WARNING	= 0,
	SCRIPT_LEVEL_ERROR,
};

typedef void ( *ScriptOutputFunc_t )( const char *pszText );
typedef bool ( *ScriptErrorFunc_t )( ScriptErrorLevel_t eLevel, const char *pszText );

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#ifdef RegisterClass
#undef RegisterClass
#endif

enum ScriptStatus_t
{
	SCRIPT_ERROR = -1,
	SCRIPT_DONE,
	SCRIPT_RUNNING,
};

class IScriptVM
{
public:
	bool Init();
	void Shutdown();

	bool ConnectDebugger();
	void DisconnectDebugger();

	ScriptLanguage_t GetLanguage();
	const char *GetLanguageName();

	void AddSearchPath( const char *pszSearchPath );

	//--------------------------------------------------------
 
 	bool Frame( float simTime );

	//--------------------------------------------------------
	// Simple script usage
	//--------------------------------------------------------
	ScriptStatus_t Run( const char *pszScript, bool bWait = true );
	inline ScriptStatus_t Run( const unsigned char *pszScript, bool bWait = true ) { return Run( (char *)pszScript, bWait ); }

	//--------------------------------------------------------
	// Compilation
	//--------------------------------------------------------
 	HSCRIPT CompileScript( const char *pszScript, const char *pszId = nullptr );
	inline HSCRIPT CompileScript( const unsigned char *pszScript, const char *pszId = nullptr ) { return CompileScript( (char *)pszScript, pszId ); }
	void ReleaseScript( HSCRIPT );

	//--------------------------------------------------------
	// Execution of compiled
	//--------------------------------------------------------
	ScriptStatus_t Run( HSCRIPT hScript, HSCRIPT hScope = nullptr, bool bWait = true );
	ScriptStatus_t Run( HSCRIPT hScript, bool bWait );

	//--------------------------------------------------------
	// Scope
	//--------------------------------------------------------
	HSCRIPT CreateScope( const char *pszScope, HSCRIPT hParent = nullptr );
	void ReleaseScope( HSCRIPT hScript );

	//--------------------------------------------------------
	// Script functions
	//--------------------------------------------------------
	HSCRIPT LookupFunction( const char *pszFunction, HSCRIPT hScope = nullptr );
	void ReleaseFunction( HSCRIPT hScript );

	//--------------------------------------------------------
	// Script functions (raw, use Call())
	//--------------------------------------------------------
	ScriptStatus_t ExecuteFunction( HSCRIPT hFunction, ScriptVariant_t *pArgs, int nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope, bool bWait );

	//--------------------------------------------------------
	// External functions
	//--------------------------------------------------------
	void RegisterFunction( ScriptFunctionBinding_t *pScriptFunction );

	//--------------------------------------------------------
	// External classes
	//--------------------------------------------------------
	bool RegisterClass( ScriptClassDesc_t *pClassDesc );

	void RegisterAllClasses()
	{
		ScriptClassDesc_t *pCurrent = *ScriptClassDesc_t::GetDescList();
		while ( pCurrent )
		{
			RegisterClass( pCurrent );
			pCurrent = pCurrent->m_pNextDesc;
		}
	}

	//--------------------------------------------------------
	// External instances. Note class will be auto-registered.
	//--------------------------------------------------------

	HSCRIPT RegisterInstance( ScriptClassDesc_t *pDesc, void *pInstance );
	void SetInstanceUniqeId( HSCRIPT hInstance, const char *pszId );
	template <typename T> HSCRIPT RegisterInstance( T *pInstance )																	{ return RegisterInstance( GetScriptDesc( pInstance ), pInstance );	}
	template <typename T> HSCRIPT RegisterInstance( T *pInstance, const char *pszInstance, HSCRIPT hScope = nullptr)					{ HSCRIPT hInstance = RegisterInstance( GetScriptDesc( pInstance ), pInstance ); SetValue( hScope, pszInstance, hInstance ); return hInstance; }
	void RemoveInstance( HSCRIPT );
	void RemoveInstance( HSCRIPT hInstance, const char *pszInstance, HSCRIPT hScope = nullptr )										{ ClearValue( hScope, pszInstance ); RemoveInstance( hInstance ); }
	void RemoveInstance( const char *pszInstance, HSCRIPT hScope = nullptr )															{ ScriptVariant_t val; if ( GetValue( hScope, pszInstance, &val ) ) { if ( val.m_type == FIELD_HSCRIPT ) { RemoveInstance( val, pszInstance, hScope ); } ReleaseValue( val ); } }

	void *GetInstanceValue( HSCRIPT hInstance, ScriptClassDesc_t *pExpectedType = nullptr );

	//----------------------------------------------------------------------------

	bool GenerateUniqueKey( const char *pszRoot, char *pBuf, int nBufSize );

	//----------------------------------------------------------------------------

	bool ValueExists( HSCRIPT hScope, const char *pszKey );
	bool ValueExists( const char *pszKey )																							{ return ValueExists( nullptr, pszKey ); }

	bool SetValue( HSCRIPT hScope, const char *pszKey, const char *pszValue );
	bool SetValue( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value );
	bool SetValue( const char *pszKey, const ScriptVariant_t &value )																{ return SetValue(nullptr, pszKey, value ); }

	void CreateTable( ScriptVariant_t &Table );
	int	GetNumTableEntries( HSCRIPT hScope );
	int GetKeyValue( HSCRIPT hScope, int nIterator, ScriptVariant_t *pKey, ScriptVariant_t *pValue );

	bool GetValue( HSCRIPT hScope, const char *pszKey, ScriptVariant_t *pValue );
	bool GetValue( const char *pszKey, ScriptVariant_t *pValue )																	{ return GetValue(nullptr, pszKey, pValue ); }
	void ReleaseValue( ScriptVariant_t &value );

	bool ClearValue( HSCRIPT hScope, const char *pszKey );
	bool ClearValue( const char *pszKey)																							{ return ClearValue( nullptr, pszKey ); }

	//----------------------------------------------------------------------------

	void WriteState( ks::byte_writer *pBuffer );
	void ReadState( ks::byte_reader *pBuffer );
	void RemoveOrphanInstances();

	void DumpState();

	void SetOutputCallback( ScriptOutputFunc_t pFunc );
	void SetErrorCallback( ScriptErrorFunc_t pFunc );

	//----------------------------------------------------------------------------

	bool RaiseException( const char *pszExceptionText );

	//----------------------------------------------------------------------------
	// Call API
	//
	// Note for string and vector return types, the caller must delete the pointed to memory
	//----------------------------------------------------------------------------
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope = nullptr, bool bWait = true, ScriptVariant_t *pReturn = nullptr )
	{
		return ExecuteFunction( hFunction, nullptr, 0, pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1 )
	{
		ScriptVariant_t args[1]; args[0] = arg1;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2 )
	{
		ScriptVariant_t args[2]; args[0] = arg1; args[1] = arg2;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3 )
	{
		ScriptVariant_t args[3]; args[0] = arg1; args[1] = arg2; args[2] = arg3;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4 )
	{
		ScriptVariant_t args[4]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5 )
	{
		ScriptVariant_t args[5]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6 )
	{
		ScriptVariant_t args[6]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7 )
	{
		ScriptVariant_t args[7]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8 )
	{
		ScriptVariant_t args[8]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9 )
	{
		ScriptVariant_t args[9]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10 )
	{
		ScriptVariant_t args[10]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11 )
	{
		ScriptVariant_t args[11]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12 )
	{
		ScriptVariant_t args[12]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13 )
	{
		ScriptVariant_t args[13]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13;
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13, ARG_TYPE_14 arg14 )
	{
		ScriptVariant_t args[14]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13; args[13] = arg14; 
		return ExecuteFunction( hFunction, args, std::size( args ), pReturn, hScope, bWait );
	}

};

// The script scope helper (CScriptScope) is in vscript_game.h.

//-----------------------------------------------------------------------------
// Script function proxy support
//-----------------------------------------------------------------------------

class CScriptFuncHolder
{
public:
	CScriptFuncHolder() : hFunction( INVALID_HSCRIPT ) {}
	bool IsValid()	{ return ( hFunction != INVALID_HSCRIPT ); }
	bool IsNull()	{ return ( !hFunction ); }
	HSCRIPT hFunction;
};

//-----------------------------------------------------------------------------

#endif // IVSCRIPT_H
