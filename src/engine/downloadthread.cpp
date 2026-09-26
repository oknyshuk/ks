//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//--------------------------------------------------------------------------------------------------------------
// downloadthread.cpp
// 
// Implementation file for optional HTTP asset downloading thread
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------------------------------

#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen

#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "download_internal.h"
#include "tier1/strtools.h"


uintp DownloadThread( void *voidPtr )
{
	Assert( !"Impl me" );
	return 0;
}

