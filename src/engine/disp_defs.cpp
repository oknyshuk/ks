// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "disp_defs.h"
import ks.tier1.convar;
import ks.tier1.iconvar;

// ---------------------------------------------------------------------- //
// Global tables.
// ---------------------------------------------------------------------- //

// These map CCoreDispSurface neighbor orientations (which are actually edge indices)
// into our 'degrees of rotation' representation.
int g_CoreDispNeighborOrientationMap[4][4] =
{
	{ORIENTATION_CCW_180, ORIENTATION_CCW_270, ORIENTATION_CCW_0,   ORIENTATION_CCW_90},
	{ORIENTATION_CCW_90,  ORIENTATION_CCW_180, ORIENTATION_CCW_270, ORIENTATION_CCW_0},
	{ORIENTATION_CCW_0,   ORIENTATION_CCW_90,  ORIENTATION_CCW_180, ORIENTATION_CCW_270},
	{ORIENTATION_CCW_270, ORIENTATION_CCW_0,   ORIENTATION_CCW_90,  ORIENTATION_CCW_180}
};



// ---------------------------------------------------------------------- //
// Global variables.
// ---------------------------------------------------------------------- //

std::vector< unsigned char >	g_DispLightmapSamplePositions;
std::vector< std::unique_ptr< CDispGroup > >	g_DispGroups;

bool						g_bDispOrthoRender = false;

ConVar						r_DrawDisp( "r_DrawDisp", "1", FCVAR_CHEAT, "Toggles rendering of displacment maps" );
ConVar						r_DispWalkable( "r_DispWalkable", "0", FCVAR_CHEAT );
ConVar						r_DispBuildable( "r_DispBuildable", "0", FCVAR_CHEAT );
