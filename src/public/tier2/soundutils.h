#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: Helper methods + classes for sound

#include "tier2/riff.h"

//-----------------------------------------------------------------------------
// RIFF reader/writers that use the file system
//-----------------------------------------------------------------------------
extern IFileReadBinary *g_pFSIOReadBinary;
extern IFileWriteBinary *g_pFSIOWriteBinary;


//-----------------------------------------------------------------------------
// Returns the duration of a wav file
//-----------------------------------------------------------------------------
float GetWavSoundDuration( const char *pWavFile );
