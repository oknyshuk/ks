#ifndef TIER1_STRTOOLS_INLINES_H
#define TIER1_STRTOOLS_INLINES_H

#include "platform.h"

inline int	_V_strlen_inline( const char *str )
{
	if ( !str )
		return 0;
	return strlen( str );
}

inline char *_V_strrchr_inline( const char *s, char c )
{
	int len = _V_strlen_inline(s);
	s += len;
	while (len--)
		if (*--s == c) return (char *)s;
	return 0;
}

inline int _V_wcscmp_inline( const wchar_t *s1, const wchar_t *s2 )
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;              // strings not equal    
		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}

	return -1;
}

#define STRTOOLS_TOLOWERC( x )  (( ( x >= 'A' ) && ( x <= 'Z' ) )?( x + 32 ) : x )
inline int	_V_stricmp_inline( const char *s1, const char *s2 )
{
	if ( s1 == NULL && s2 == NULL )
		return 0;
	if ( s1 == NULL )
		return -1;
	if ( s2 == NULL )
		return 1;

	return stricmp( s1, s2 );
}

inline char *_V_strstr_inline( const char *s1, const char *search )
{
	return (char *)strstr( s1, search );
}

#endif // TIER1_STRTOOLS_INLINES_H
