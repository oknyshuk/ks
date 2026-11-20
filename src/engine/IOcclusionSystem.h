#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vmatrix;
import ks.math.vplane;
import ks.platform;
import ks.simd;

struct model_t;


//-----------------------------------------------------------------------------
// Occlusion system interface
//-----------------------------------------------------------------------------
class IOcclusionSystem
{
public:
	// Activate/deactivate an occluder brush model
	void ActivateOccluder( int nOccluderIndex, bool bActive );

	// Sets the view transform
	void SetView( const Vector &vecCameraPos, float flFOV, const VMatrix &worldToCamera, 
		const VMatrix &cameraToProjection, const VPlane &nearClipPlane );
	int GetViewId() const;

	// Test for occlusion (bounds specified in abs space)
	bool IsOccluded( int occlusionViewId, const Vector &vecAbsMins, const Vector &vecAbsMaxs );

	// Sets global occlusion parameters
	void SetOcclusionParameters( float flMaxOccludeeArea, float flMinOccluderArea );
	float MinOccluderArea() const;

	// Render debugging overlay
	void DrawDebugOverlays();
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
IOcclusionSystem *OcclusionSystem();
