#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Should we draw light sprites over visible lights?
#include "bspfile.h"

bool ActivateLightSprites( bool bActive );

// Draws sprites over all visible lights
void DrawLightSprites( void );

// Draws lighting debugging information
void DrawLightDebuggingInfo( void );

float ComputeLightRadius( dworldlight_t *pLight, bool bIsHDR );
