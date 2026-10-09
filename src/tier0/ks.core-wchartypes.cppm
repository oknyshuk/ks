// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;

#include <stdio.h>

export module ks.core:wchartypes;

export extern "C++" {

// char8
// char8 is equivalent to char, and should be used when you really need a char
// (for example, when calling an external function that's declared to take
// chars).
typedef char char8;

// uint8 is owned by :types; byte and BYTE are the same type.
typedef unsigned char BYTE;
typedef unsigned char byte;

// wchar
// wchar is a single character of text (currently 16 bits, as all of our text is
// Unicode).  Use this whenever you mean a piece of text (for example, in a string).
typedef wchar_t wchar;

typedef char tchar;

typedef unsigned short uchar16;
typedef wchar_t uchar32;

typedef unsigned short ucs2; // wchar_t is 4 bytes on sane os's, specially define a ucs2 type so we can read out localization files and the list saved as 2 byte wchar (or ucs16 Matt tells me)

// __WFILE__
// This is a Unicode version of __FILE__

inline char *_tcsstr( const char *s, const char *sub ) { return __builtin_strstr( s, sub ); }
inline char *_tcscpy( char *dst, const char *src ) { return __builtin_strcpy( dst, src ); }
inline size_t _tcslen( const char *s ) { return __builtin_strlen( s ); }
inline int _sntprintf( char *dst, size_t n, const char *fmt, ... ) { va_list ap; __builtin_va_start( ap, fmt ); int r = vsnprintf( dst, n, fmt, ap ); __builtin_va_end( ap ); return r; }

}
