// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.core;

// First include standard libraries
import std;


// This function is marked in the VPC file as always being optimized, even in debug
// builds, because this gives a ~7% speedup on debug builds because this function is
// used by the debug allocator.


static constexpr int ToLowerAscii( int c ) { return ( c >= 'A' && c <= 'Z' ) ? c + 32 : c; }

int V_tier0_stricmp(const char *s1, const char *s2 )
{
	// A string is always equal to itself. This optimization is
	// surprisingly valuable.
	if ( s1 == s2 )
		return 0;

	uint8 const *pS1 = ( uint8 const * ) s1;
	uint8 const *pS2 = ( uint8 const * ) s2;
	for(;;)
	{
		int c1 = *( pS1++ );
		int c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = ToLowerAscii( c1 );
			c2 = ToLowerAscii( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
		c1 = *( pS1++ );
		c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = ToLowerAscii( c1 );
			c2 = ToLowerAscii( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
	}
}

void V_tier0_strncpy( char *a, const char *b, int n )
{
	contract_assert( n >= sizeof( *a ) );

	// NOTE: Never never use strncpy! Here's what it actually does, which is not what we want!

	// (from MSDN)
	// The strncpy function copies the initial count characters of strSource to strDest
	// and returns strDest. If count is less than or equal to the length of strSource, 
	// a null character is not appended automatically to the copied string. If count 
	// is greater than the length of strSource, the destination string is padded with 
	// null characters up to length count. The behavior of strncpy is undefined 
	// if the source and destination strings overlap.
	// strncpy( pDest, pSrc, maxLen );

	char *pLast = a + n - 1;
	while ( (a < pLast) && (*b != 0) )
	{
		*a = *b;
		++a; ++b;
	}
	*a = 0;
}

char *V_tier0_strncat( char *pDest, const char *pSrc, int destBufferSize, int max_chars_to_copy )
{
	int charstocopy = 0;

	contract_assert( destBufferSize >= 0 );
	
	int len = (int)std::strlen(pDest);
	int srclen = (int)std::strlen( pSrc );
	if ( max_chars_to_copy <= -1 )
	{
		charstocopy = srclen;
	}
	else
	{
		charstocopy = Min( max_chars_to_copy, (int)srclen );
	}

	if ( len + charstocopy >= destBufferSize )
	{
		charstocopy = destBufferSize - len - 1;
	}

	// charstocopy can end up negative if you fill a buffer and then pass in a smaller
	// buffer size. Yes, this actually happens.
	// Cast to ptrdiff_t is necessary in order to check for negative (size_t is unsigned)
	if ( charstocopy <= 0 )
	{
		return pDest;
	}

	; // warning C6059: : Incorrect length parameter in call to 'strncat'. Pass the number of remaining characters, not the buffer size of 'argument 1'
	char *pOut = std::strncat( pDest, pSrc, charstocopy );
	return pOut;
}

int V_tier0_vsnprintf( char *a, int n, const char *f, std::va_list l )
{
	int len = std::vsnprintf( a, n, f, l );

	if ( ( len < 0 ) ||
		 ( n > 0 && len >= n ) )
	{
		len = n - 1;
		a[n - 1] = 0;
	}

	return len;
}

int V_tier0_snprintf( char *a, int n, const char *f, ... )
{
    std::va_list l;

    __builtin_va_start( l, f );
    int len = V_tier0_vsnprintf( a, n, f, l );
    __builtin_va_end( l );
    return len;
}
