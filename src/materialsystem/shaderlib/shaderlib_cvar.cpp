//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "icvar.h"
#include "tier1/tier1.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void InitShaderLibCVars( CreateInterfaceFn cvarFactory )
{
	if ( g_pCVar )
	{
		ConVar_Register();
	}
}
