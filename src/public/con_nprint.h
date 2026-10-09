#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Debug overlay / notfication printing

//-----------------------------------------------------------------------------
// Purpose:  Debug overlay / notfication printing 
// NOTE:  Structure cannot be changed by mods
//-----------------------------------------------------------------------------
typedef struct con_nprint_s
{
	int		index;			// Row #
	float	time_to_live;	// # of seconds before it disappears. -1 means to display for 1 frame then go away.
	float	color[ 3 ];		// RGB colors ( 0.0 -> 1.0 scale )
	bool	fixed_width_font;
} con_nprint_t;

// Print string on line idx
void Con_NPrintf( int idx, const char *fmt, ... );
// Customized printout
void Con_NXPrintf( const con_nprint_t *info, const char *fmt, ... );
