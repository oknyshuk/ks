#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2008 Valve Corporation

import ks.core;
#include "vscript/ivscript.h"

import std;

//-----------------------------------------------------------------------------
// Signature deduction: the parameter list comes from the function's own type.
//-----------------------------------------------------------------------------
template <typename... ARGS>
inline void ScriptAppendParams( ScriptFuncDescriptor_t *pDesc )
{
	pDesc->m_Parameters.reserve( sizeof...( ARGS ) );
	( pDesc->m_Parameters.push_back( ScriptDeduceType<ARGS> ), ... );
}

template <typename RET_TYPE, typename... ARGS>
inline void ScriptDeduceFunctionSignature( ScriptFuncDescriptor_t *pDesc, RET_TYPE (*pfnProxied)( ARGS... ) )
{
	pDesc->m_ReturnType = ScriptDeduceType<RET_TYPE>;
	ScriptAppendParams<ARGS...>( pDesc );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... ARGS>
inline void ScriptDeduceFunctionSignature( ScriptFuncDescriptor_t *pDesc, OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( ARGS... ) )
{
	pDesc->m_ReturnType = ScriptDeduceType<RET_TYPE>;
	ScriptAppendParams<ARGS...>( pDesc );
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... ARGS>
inline void ScriptDeduceFunctionSignature( ScriptFuncDescriptor_t *pDesc, OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( ARGS... ) const )
{
	pDesc->m_ReturnType = ScriptDeduceType<RET_TYPE>;
	ScriptAppendParams<ARGS...>( pDesc );
}

#define ScriptInitMemberFuncDescriptor_( pDesc, class, func, scriptName )	if ( 0 ) {} else { (pDesc)->m_pszScriptName = scriptName; (pDesc)->m_pszFunction = #func; ScriptDeduceFunctionSignature( pDesc, (class *)(0), &class::func ); }

#define ScriptInitFuncDescriptorNamed( pDesc, func, scriptName )						if ( 0 ) {} else { (pDesc)->m_pszScriptName = scriptName; (pDesc)->m_pszFunction = #func; ScriptDeduceFunctionSignature( pDesc, &func ); }
#define ScriptInitFuncDescriptor( pDesc, func )											ScriptInitFuncDescriptorNamed( pDesc, func, #func )
#define ScriptInitMemberFuncDescriptorNamed( pDesc, class, func, scriptName )			ScriptInitMemberFuncDescriptor_( pDesc, class, func, scriptName )
#define ScriptInitMemberFuncDescriptor( pDesc, class, func )							ScriptInitMemberFuncDescriptorNamed( pDesc, class, func, #func )

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

template <typename FUNCPTR_TYPE>
inline ScriptFunctionBindingStorageType_t ScriptConvertFreeFuncPtrToVoid( FUNCPTR_TYPE pFunc )
{
	static_assert( sizeof( FUNCPTR_TYPE ) == sizeof( void* ) * 2 || sizeof( FUNCPTR_TYPE ) == sizeof( void* ) );
	
	if ( sizeof( FUNCPTR_TYPE ) == 4 )
	{
		union FuncPtrConvertMI
		{
			FUNCPTR_TYPE pFunc;
			ScriptFunctionBindingStorageType_t stype;
		};

		FuncPtrConvertMI convert;
		convert.pFunc = pFunc;
		return convert.stype;
	}
	else
	{
		union FuncPtrConvertMI
		{
			FUNCPTR_TYPE pFunc;
			struct
			{
				ScriptFunctionBindingStorageType_t stype;
				std::intptr_t iToc;
			} fn8;
		};

		FuncPtrConvertMI convert;
		convert.fn8.iToc = 0;
		convert.pFunc = pFunc;
		if ( !convert.fn8.iToc )
			return convert.fn8.stype;
		
		contract_assert( 0 );
		DebuggerBreak();
		return 0;
	}
}

template <typename FUNCPTR_TYPE>
inline FUNCPTR_TYPE ScriptConvertFreeFuncPtrFromVoid( ScriptFunctionBindingStorageType_t p )
{
	static_assert( sizeof( FUNCPTR_TYPE ) == sizeof(void*)*2 || sizeof( FUNCPTR_TYPE ) == sizeof(void*) );

	if ( sizeof( FUNCPTR_TYPE ) == 4 )
	{
		union FuncPtrConvertMI
		{
			FUNCPTR_TYPE pFunc;
			ScriptFunctionBindingStorageType_t stype;
		};

		FuncPtrConvertMI convert;
		convert.pFunc = 0;
		convert.stype = p;
		return convert.pFunc;
	}
	else
	{
		union FuncPtrConvertMI
		{
			FUNCPTR_TYPE pFunc;
			struct
			{
				ScriptFunctionBindingStorageType_t stype;
				std::intptr_t iToc;
			} fn8;
		};

		FuncPtrConvertMI convert;
		convert.pFunc = 0;
		convert.fn8.stype = p;
		convert.fn8.iToc = 0;
		return convert.pFunc;
	}


}

template <typename FUNCPTR_TYPE>
inline ScriptFunctionBindingStorageType_t ScriptConvertFuncPtrToVoid( FUNCPTR_TYPE pFunc )
{
	typedef FUNCPTR_TYPE FuncPtr_t;
	std::size_t funcPtrSize = sizeof( FuncPtr_t ); funcPtrSize;

	return ScriptConvertFreeFuncPtrToVoid<FUNCPTR_TYPE>( pFunc );
}

template <typename FUNCPTR_TYPE>
inline FUNCPTR_TYPE ScriptConvertFuncPtrFromVoid( ScriptFunctionBindingStorageType_t p )
{
	return ScriptConvertFreeFuncPtrFromVoid<FUNCPTR_TYPE>( p );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Script bindings: turn a typed function pointer into a ScriptBindingFunc_t.
//-----------------------------------------------------------------------------
template <typename FUNC_TYPE, typename RET_TYPE, typename... ARGS>
class CNonMemberScriptBinding
{
	static constexpr int kArity = sizeof...( ARGS );

	template <std::size_t... INDICES>
	static void Invoke( ScriptFunctionBindingStorageType_t pFunction, ScriptVariant_t *pArguments, ScriptVariant_t *pReturn, std::index_sequence<INDICES...> )
	{
		if constexpr ( std::is_void_v<RET_TYPE> )
		{
			ScriptConvertFreeFuncPtrFromVoid<FUNC_TYPE>( pFunction )( pArguments[INDICES]... );
		}
		else
		{
			*pReturn = ScriptConvertFreeFuncPtrFromVoid<FUNC_TYPE>( pFunction )( pArguments[INDICES]... );
			if ( pReturn->m_type == FIELD_VECTOR )
				pReturn->m_pVector = new Vector( *pReturn->m_pVector );
		}
	}

public:
	static bool Call( ScriptFunctionBindingStorageType_t pFunction, void *pContext, ScriptVariant_t *pArguments, int nArguments, ScriptVariant_t *pReturn )
	{
		contract_assert( nArguments == kArity );
		contract_assert( pContext == nullptr );

		if ( nArguments != kArity || pContext )
			return false;

		if constexpr ( std::is_void_v<RET_TYPE> )
		{
			contract_assert( pReturn == nullptr );
			if ( pReturn )
				return false;
		}
		else
		{
			contract_assert( pReturn != nullptr );
			if ( !pReturn )
				return false;
		}

		Invoke( pFunction, pArguments, pReturn, std::index_sequence_for<ARGS...>{} );
		return true;
	}
};

template <typename OBJECT_TYPE_PTR, typename FUNC_TYPE, typename RET_TYPE, typename... ARGS>
class CMemberScriptBinding
{
	static constexpr int kArity = sizeof...( ARGS );

	template <std::size_t... INDICES>
	static void Invoke( ScriptFunctionBindingStorageType_t pFunction, void *pContext, ScriptVariant_t *pArguments, ScriptVariant_t *pReturn, std::index_sequence<INDICES...> )
	{
		if constexpr ( std::is_void_v<RET_TYPE> )
		{
			( ( (OBJECT_TYPE_PTR)(pContext) )->*ScriptConvertFuncPtrFromVoid<FUNC_TYPE>( pFunction ) )( pArguments[INDICES]... );
		}
		else
		{
			*pReturn = ( ( (OBJECT_TYPE_PTR)(pContext) )->*ScriptConvertFuncPtrFromVoid<FUNC_TYPE>( pFunction ) )( pArguments[INDICES]... );
			if ( pReturn->m_type == FIELD_VECTOR )
				pReturn->m_pVector = new Vector( *pReturn->m_pVector );
		}
	}

public:
	static bool Call( ScriptFunctionBindingStorageType_t pFunction, void *pContext, ScriptVariant_t *pArguments, int nArguments, ScriptVariant_t *pReturn )
	{
		contract_assert( nArguments == kArity );
		contract_assert( pContext != nullptr );

		if ( nArguments != kArity || !pContext )
			return false;

		if constexpr ( std::is_void_v<RET_TYPE> )
		{
			contract_assert( pReturn == nullptr );
			if ( pReturn )
				return false;
		}
		else
		{
			contract_assert( pReturn != nullptr );
			if ( !pReturn )
				return false;
		}

		Invoke( pFunction, pContext, pArguments, pReturn, std::index_sequence_for<ARGS...>{} );
		return true;
	}
};

template <typename RET_TYPE, typename... ARGS>
inline ScriptBindingFunc_t ScriptCreateBinding( RET_TYPE (*pfnProxied)( ARGS... ) )
{
	typedef RET_TYPE (*Func_t)( ARGS... );
	return &CNonMemberScriptBinding< Func_t, RET_TYPE, ARGS... >::Call;
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... ARGS>
inline ScriptBindingFunc_t ScriptCreateBinding( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( ARGS... ) )
{
	typedef RET_TYPE ( FUNCTION_CLASS::*Func_t )( ARGS... );
	return &CMemberScriptBinding< OBJECT_TYPE_PTR, Func_t, RET_TYPE, ARGS... >::Call;
}

template <typename OBJECT_TYPE_PTR, typename RET_TYPE, typename FUNCTION_CLASS, typename... ARGS>
inline ScriptBindingFunc_t ScriptCreateBinding( OBJECT_TYPE_PTR pObject, RET_TYPE ( FUNCTION_CLASS::*pfnProxied )( ARGS... ) const )
{
	typedef RET_TYPE ( FUNCTION_CLASS::*Func_t )( ARGS... ) const;
	return &CMemberScriptBinding< OBJECT_TYPE_PTR, Func_t, RET_TYPE, ARGS... >::Call;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
