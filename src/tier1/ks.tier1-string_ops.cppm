// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.tier1:string_ops;

import std;
import ks.math;

export extern "C++" {

namespace ks
{
	// For const char * that may be null, where std::string would not accept it.
	constexpr const char *nz( const char *p ) { return p ? p : ""; }

	std::string format_printfv( const char *pFormat, std::va_list args );
	std::string format_printf( const char *pFormat, ... ) [[gnu::format(printf, 1, 2)]];
}

}
