#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.math.compressed_vector;
import ks.math.lightdesc;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interfaces;

// Purpose: Export functions from l_studio.cpp

#include "engine/ivmodelrender.h"
#include "datacache/imdlcache.h"
#include "bspfile.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IStudioRender;
struct vcollide_t;

void UpdateStudioRenderConfig( void );
void InitStudioRender( void );
void ShutdownStudioRender( void );
unsigned short& FirstShadowOnModelInstance( ModelInstanceHandle_t handle );

//-----------------------------------------------------------------------------
// Converts world lights to materialsystem lights
//-----------------------------------------------------------------------------
bool WorldLightToMaterialLight( dworldlight_t* worldlight, LightDesc_t& light );

//-----------------------------------------------------------------------------
// Computes the center of the studio model for illumination purposes
//-----------------------------------------------------------------------------
void R_ComputeLightingOrigin( IClientRenderable *pRenderable, studiohdr_t* pStudioHdr, const matrix3x4_t &matrix, Vector& center );

void DrawSavedModelDebugOverlays( void );

// Used to force the value of r_rootlod depending on if sv_cheats is 0 and if we're on a server.
bool CheckVarRange_r_rootlod();
bool CheckVarRange_r_lod();

extern ConVar r_drawmodelstatsoverlay;
extern ConVar r_drawmodelstatsoverlaydistance;
extern ConVar r_lod;
extern ConVar r_drawmodellightorigin;
