// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

export module ks.game.rope_helpers;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;

export
{
//
// This function can help you choose starting conditions for your rope. It is fairly
// expensive (slightly less than 0.5ms for a 10-node rope), but it's within reason for the 
// frequency we create ropes at.
//
// Input: 
//    - rope endpoints
//    - the number of nodes the client will be simulating (CRopeKeyframe::m_nSegments)
//    - how low you want the rope to hang (below the lowest of the two endpoints)
//
// Output: 
//	  - pOutputLength = length of the rope
//    - pOutputSlack  = slack you should set to produce the desired hang
//
void CalcRopeStartingConditions( 
	const Vector &vStartPos,
	const Vector &vEndPos,
	int const nNodes,
	float const desiredHang,
	float *pOutputLength,
	float *pOutputSlack
	);
}
