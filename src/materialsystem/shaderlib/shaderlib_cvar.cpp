// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

void InitShaderLibCVars( CreateInterfaceFn cvarFactory )
{
	if ( g_pCVar )
	{
		ConVar_Register();
	}
}
