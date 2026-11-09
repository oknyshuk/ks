#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation


#include "networkvar.h"


//-----------------------------------------------------------------------------
// Purpose: Implements the debugging elements of view rendering
//-----------------------------------------------------------------------------
class CDebugViewRender
{
	DECLARE_CLASS_NOBASE( CDebugViewRender );
public:
	// Draws all the debugging info
	static void	Draw3DDebuggingInfo( const CViewSetup &view );
	static void	Draw2DDebuggingInfo( const CViewSetup &view );
	static void GenerateOverdrawForTesting();
};
