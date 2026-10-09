#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Provides access to cvar values that are bounded by the server.

import std;
import ks.tier1;

extern ConVar_ServerBounded *cl_rate;
extern ConVar_ServerBounded *cl_cmdrate;
extern ConVar_ServerBounded *cl_updaterate;
