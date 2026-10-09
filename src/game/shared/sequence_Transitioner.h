#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#ifdef CLIENT_DLL
#include "animationlayer.h"
#else
#include "BaseAnimatingOverlay.h"
#endif

class CStudioHdr;

// ------------------------------------------------------------------------------------------------ //
// CSequenceTransitioner declaration.
// ------------------------------------------------------------------------------------------------ //
class CSequenceTransitioner
{
public:
	void CheckForSequenceChange( 
		// Describe the current animation state with these parameters.
		CStudioHdr *hdr,
		int nCurSequence, 

		// Even if the sequence hasn't changed, you can force it to interpolate from the previous
		// spot in the same sequence to the current spot in the same sequence by setting this to true.
		bool bForceNewSequence,

		// Follows EF_NOINTERP.
		bool bInterpolate
		);

	void UpdateCurrent( 
		// Describe the current animation state with these parameters.
		CStudioHdr *hdr,
		int nCurSequence, 
		float flCurCycle,
		float flCurPlaybackRate,
		float flCurTime
		);

	void RemoveAll( void ) { m_animationQueue.clear(); };

public:
	std::vector< CAnimationLayer >	m_animationQueue;
};
