#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Provides access to cvars that are bounded in the client DLL.

import std;
import ks.tier1;

extern ConVar_ServerBounded *cl_predict;
extern ConVar_ServerBounded *cl_interp;

// Returns cl_interp_ratio / cl_updaterate.
float GetClientInterpAmount();


extern bool g_bForceCLPredictOff;	// If this is set to true, then prediction is forced off. Used temporarily for kill cam.
