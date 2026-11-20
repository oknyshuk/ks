// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib.cvar;

import std;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;

export extern "C++" {

//-----------------------------------------------------------------------------
// Returns a CVar dictionary for tool usage
//-----------------------------------------------------------------------------
CreateInterfaceFn VStdLib_GetICVarFactory();

}
