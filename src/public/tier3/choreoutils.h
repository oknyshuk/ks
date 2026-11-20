#pragma once
import ks.core.threadtools;
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: Helper methods + classes for choreo

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CChoreoScene;
class CChoreoEvent;
class CStudioHdr;


//-----------------------------------------------------------------------------
// Finds sound files associated with events
//-----------------------------------------------------------------------------
const char *GetSoundForEvent( CChoreoEvent *pEvent, CStudioHdr *pStudioHdr );


//-----------------------------------------------------------------------------
// Fixes up the duration of a choreo scene based on wav files + animations
// Returns true if a change needed to be made
//-----------------------------------------------------------------------------
bool AutoAddGestureKeys( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly );
bool UpdateGestureLength( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly );
bool UpdateSequenceLength( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly, bool bVerbose );
