// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

import ks.tier1;
#include "compat/better_winlite.h"
import std;

#define VJOLT_RETURN_ADDRESS() __builtin_return_address(0)

[[gnu::always_inline]] inline void GetCallingFunctionModulePath( void *pReturnAddress, char *pszModulePath, size_t len )
{
    V_strncpy( pszModulePath, "Unknown", len);
}
