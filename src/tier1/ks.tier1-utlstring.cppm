// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.utlstring;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;
export import ks.tier1.string_ops;
export import ks.tier1.strtools;

export extern "C++" {

//-----------------------------------------------------------------------------
// Purpose: Implementation of low-level string functionality for character types.
//-----------------------------------------------------------------------------

template < typename T >
class StringFuncs
{
public:
	static T		*Duplicate( const T *pValue );
	static void		 Copy( T *out_pOut, const T *pIn, int iLength );
	static int		 Compare( const T *pLhs, const T *pRhs );
	static int		 Length( const T *pValue );
	static const T  *FindChar( const T *pStr, const T cSearch );
	static const T	*EmptyString();
};

template < >
class StringFuncs<char>
{
public:
	static char		  *Duplicate( const char *pValue ) { return __builtin_strdup( pValue ); }
	static void		   Copy( char *out_pOut, const char *pIn, int iLength ) { std::strncpy( out_pOut, pIn, iLength ); }
	static int		   Compare( const char *pLhs, const char *pRhs ) { return std::strcmp( pLhs, pRhs ); }
	static int		   Length( const char *pValue ) { return (int)std::strlen( pValue ); }
	static const char *FindChar( const char *pStr, const char cSearch ) { return std::strchr( pStr, cSearch ); }
	static const char *EmptyString() { return ""; }
};

template < >
class StringFuncs<wchar_t>
{
public:
	static wchar_t		 *Duplicate( const wchar_t *pValue ) {
		const std::size_t length = std::wcslen( pValue ) + 1;
		return static_cast<wchar_t *>( std::wmemcpy( static_cast<wchar_t *>( std::malloc( length * sizeof( wchar_t ) ) ), pValue, length ) );
	}
	static void			  Copy( wchar_t *out_pOut, const wchar_t  *pIn, int iLength ) { std::wcsncpy( out_pOut, pIn, iLength ); }
	static int			  Compare( const wchar_t *pLhs, const wchar_t *pRhs ) { return std::wcscmp( pLhs, pRhs ); }
	static int			  Length( const wchar_t *pValue ) { return (int) std::wcslen( pValue ); }
	static const wchar_t *FindChar( const wchar_t *pStr, const wchar_t cSearch ) { return std::wcschr( pStr, cSearch ); }
	static const wchar_t *EmptyString() { return L""; }
};

}
