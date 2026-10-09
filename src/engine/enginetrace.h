#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "engine/IEngineTrace.h"

extern IEngineTrace *g_pEngineTraceServer;
extern IEngineTrace *g_pEngineTraceClient;


//-----------------------------------------------------------------------------
// Debugging code to render all ray casts since the last time this call was made
//-----------------------------------------------------------------------------
void EngineTraceRenderRayCasts();
