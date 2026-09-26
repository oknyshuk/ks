//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "render_pch.h"
#include "draw.h"
#include "server.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "tier1/text_reader.h"
#include "tier1/utlvector.h"
#include "tier2/renderutils.h"

std::vector< Vector >	g_Points;
//-----------------------------------------------------------------------------
// Purpose: Draw the currently loaded line file
// Input  : g_Points - list of points
//-----------------------------------------------------------------------------
void Linefile_Draw( void )
{
	Vector *points = g_Points.data();
	int pointCount = std::ssize( g_Points );

	for ( int i = 0; i < pointCount-1; i++ )
	{
		RenderLine( points[i], points[i+1], Color( 255, 255, 0, 255 ), true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: parse the map.lin file from disk
//			this file contains a list of line segments illustrating a leak in
//			the map
//-----------------------------------------------------------------------------
void Linefile_Read_f( void )
{
	Vector	org;
	int		r;
	int		c;
	char	name[MAX_OSPATH];

	g_Points.clear(); g_Points.shrink_to_fit();

	Q_snprintf( name, sizeof( name ), "maps/%s.lin", sv.GetMapName() );

	auto data = g_pFileSystem->ReadFile( name, nullptr );
	if ( !data )
	{
		ConMsg ("couldn't open %s\n", name);
		return;
	}
	ks::text_reader buf( ks::text_view( *data ) );
	
	ConMsg ("Reading %s...\n", name);
	c = 0;

	for ( ;; )
	{
		r = buf.scanf ("%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		g_Points.push_back( org );
	}

	ConMsg ("%i lines read\n", c);
}
