#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation


import std;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;

class IClientNetworkable;
class CStudioHdr;
class CIKContext;

class IPoseDebugger
{
public:
	virtual void StartBlending( IClientNetworkable *pEntity, const CStudioHdr *pStudioHdr ) = 0;
	
	virtual void AccumulatePose(
		const CStudioHdr *pStudioHdr,
		CIKContext *pIKContext,
		Vector pos[], 
		Quaternion q[], 
		int sequence, 
		float cycle,
		const float poseParameter[],
		int boneMask,
		float flWeight,
		float flTime
		) = 0;
};
