
#pragma once

#include "compat/better_winlite.h"

#define VJOLT_RETURN_ADDRESS() __builtin_return_address(0)

FORCEINLINE void GetCallingFunctionModulePath( void *pReturnAddress, char *pszModulePath, size_t len )
{
    V_strncpy( pszModulePath, "Unknown", len);
}
