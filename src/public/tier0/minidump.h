//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MINIDUMP_H
#define MINIDUMP_H

#include "tier0/platform.h"

// Set prefix to use for minidump files.  If you don't set one, it is defaulted for you,
// using the current module name
PLATFORM_INTERFACE void SetMinidumpFilenamePrefix( const char *pszPrefix );

// Set comment to put into minidump file upon next call of WriteMiniDump.  (Most common use is the assert text.)
PLATFORM_INTERFACE void SetMinidumpComment( const char *pszComment );

// writes out a minidump of the current stack trace with a unique filename
PLATFORM_INTERFACE void WriteMiniDump( const char *pszFilenameSuffix = NULL );

typedef void( *FnWMain )(int, tchar *[]);
typedef void( *FnVoidPtrFn )(void *);


#endif // MINIDUMP_H
