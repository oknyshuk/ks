#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

#ifdef USE_BLOBULATOR

#include "../../common/blobulator/Implicit/ImpDefines.h"
#include "../../common/blobulator/Implicit/ImpRenderer.h"
#include "../../common/blobulator/Implicit/ImpTiler.h"
#include "../../common/blobulator/Implicit/UserFunctions.h"

void Surface_Draw( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth, bool bSurfaceNoParticleCull = false );

void Surface_SafeLightCubeUpdate( const Vector &vecRenderOrigin, Vector4D *cachedCubeColours );


extern ks::aligned_vector< ImpParticleWithFourInterpolants, 16 > g_SurfaceRenderParticles;
extern const QAngle g_SurfaceRenderAnglesAngles;

#endif
