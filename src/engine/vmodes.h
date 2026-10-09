#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct viddef_t
{
	unsigned int	width;		
	unsigned int	height;
	int				recalc_refdef;	// if non-zero, recalc vid-based stuff
	int				bits;
};
