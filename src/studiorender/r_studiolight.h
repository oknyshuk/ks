#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Stateless light computation routines

import ks.math;



//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct lightpos_t;


//-----------------------------------------------------------------------------
// Stateless light computation routines
//-----------------------------------------------------------------------------

// Computes the ambient term
void R_LightAmbient_4D( const Vector& normal, Vector4D* pLightBoxColor, Vector &lv );
void R_LightStrengthWorld( const Vector& vert, int lightcount, LightDesc_t* pLightDesc, lightpos_t *light );
float R_WorldLightDistanceFalloff( const LightDesc_t *wl, const Vector& delta );

// Copies lighting state into a buffer, returns number of lights copied
int CopyLocalLightingState( int nMaxLights, LightDesc_t *pDest, int nLightCount, const LightDesc_t *pSrc );
