// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib:cvar;

import std;
import ks.tier1;

export extern "C++" {

//-----------------------------------------------------------------------------
// Returns a CVar dictionary for tool usage
//-----------------------------------------------------------------------------
CreateInterfaceFn VStdLib_GetICVarFactory();

}
