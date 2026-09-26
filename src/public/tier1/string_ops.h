// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// std::string helpers with no one-line std spelling.

#ifndef STRING_OPS_H
#define STRING_OPS_H

#include <cstdarg>
#include <cstdio>
#include <string>

#include "tier0/platform.h"

namespace ks
{
	// For const char * that may be null, where std::string would not accept it.
	constexpr const char *nz( const char *p ) { return p ? p : ""; }

	std::string format_printfv( PRINTF_FORMAT_STRING const char *pFormat, va_list args );
	std::string format_printf( PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 1, 2 );
}

#endif // STRING_OPS_H
