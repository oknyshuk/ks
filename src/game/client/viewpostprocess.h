#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

#include "postprocess_shared.h"
#include "iclientrenderable.h"

class CViewSetup;

struct RenderableInstance_t;

bool DoEnginePostProcessing( int x, int y, int w, int h, bool bFlashlightIsOn, bool bPostUI = false );
bool DoImageSpaceMotionBlur( const CViewSetup &view );
bool IsDepthOfFieldEnabled();
void DoDepthOfField( const CViewSetup &view );
void BlurEntity( IClientRenderable *pRenderable, bool bPreDraw, int drawFlags, const RenderableInstance_t &instance, const CViewSetup &view, int x, int y, int w, int h );

void UpdateMaterialSystemTonemapScalar();

float GetCurrentTonemapScale();

void SetOverrideTonemapScale( bool bEnableOverride, float flTonemapScale );

void SetOverridePostProcessingDisable( bool bForceOff );

void DoBlurFade( float flStrength, float flDesaturate, int x, int y, int w, int h );

void SetPostProcessParams( const PostProcessParameters_t *pPostProcessParameters );

void SetViewFadeParams( byte r, byte g, byte b, byte a, bool bModulate );

#ifdef IRONSIGHT
bool ApplyIronSightScopeEffect( int x, int y, int w, int h, CViewSetup *viewSetup, bool bPreparationStage );
#endif
