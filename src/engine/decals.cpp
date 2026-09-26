//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// Functionality common to wad and decal code in gl_draw.c and draw.c
//
//===========================================================================//
#include "render_pch.h"
#include "decal.h"
#include "decal_private.h"
#include "zone.h"
#include "sys.h"
#include "gl_matsysiface.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "materialsystem/imaterial.h"
#include <map>

struct DecalEntry
{
#ifdef _DEBUG
	char		*m_pDebugName;	// only used in debug builds
#endif
	IMaterial	*material;
	int			index;
};

// This stores the list of all decals
std::map< FileNameHandle_t, DecalEntry >	g_DecalDictionary;

// This list is indexed by network id, so it maps network ids to decal dictionary entries (null if unset)
std::vector< DecalEntry * > g_DecalLookup;

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int Draw_DecalMax( void )
{
	return g_nMaxDecals;
}

static bool BIsPlayerLogoDecal( int index )
{
	return ( ( ( index >> 24 ) & 0x7F ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Sets the name of the bitmap from decals.wad to be used in a specific slot #
// called from cl_parse.cpp twice
// This sets the name of a decal prototype texture
// Input  : decal - 
//			*name - 
//-----------------------------------------------------------------------------
// called from gl_rsurf.cpp
IMaterial *Draw_DecalMaterial( int index )
{
	if ( BIsPlayerLogoDecal( index ) )
		return materials->FindMaterial( "decals/playerlogo01", TEXTURE_GROUP_DECAL, true );

	if ( index < 0 || index >= std::ssize( g_DecalLookup ) )
		return nullptr;

	DecalEntry * entry = g_DecalLookup[index];
	
	if ( entry )
	{
		return entry->material;
	}
	else
	{
		return nullptr;
	}
}

#ifndef DEDICATED
void Draw_DecalSetName( int decal, char *name )
{
	while ( decal >= std::ssize( g_DecalLookup ) )
	{
		MEM_ALLOC_CREDIT();
		g_DecalLookup.push_back( nullptr );
	}

	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( name );
	auto lookup = g_DecalDictionary.find( fnHandle );
	if ( lookup == g_DecalDictionary.end() )
	{
		DecalEntry entry;
#ifdef _DEBUG
		int len = strlen(name) + 1;
		entry.m_pDebugName = new char[len];
		memcpy( entry.m_pDebugName, name, len );
#endif
		// fully precache the decal
		entry.material = GL_LoadMaterial( name, TEXTURE_GROUP_DECAL, true );
		entry.index = decal;

		lookup = g_DecalDictionary.emplace( fnHandle, entry ).first;
	}
	else
	{
		lookup->second.index = decal;
	}

	g_DecalLookup[decal] = &lookup->second;
}



// called from cl_parse.cpp
// find the server side decal id given it's name.
// used for save/restore
int Draw_DecalIndexFromName( char *name, bool *found )
{
	Assert( found );

	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( name );
	auto lookup = g_DecalDictionary.find( fnHandle );
	if ( lookup == g_DecalDictionary.end() )
	{
		if ( found )
		{
			*found = false;
		}
		return 0;
	}

	if ( found )
	{
		*found = true;
	}
	return lookup->second.index;
}
#endif


const char *Draw_DecalNameFromIndex( int index )
{
#if !defined(DEDICATED)
	// Callers pass the network id, which g_DecalLookup maps to the entry.
	if ( index < 0 || index >= std::ssize( g_DecalLookup ) || !g_DecalLookup[index] )
		return "";
	IMaterial *mat = g_DecalLookup[index]->material;
	return mat ? mat->GetName() : "";
#else
	return "";
#endif
}

// This is called to reset all loaded decals
// called from cl_parse.cpp and host.cpp
void Decal_Init( void )
{
	Decal_Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Decal_Shutdown( void )
{
	for ( auto &[fnHandle, entry] : g_DecalDictionary )
	{
		IMaterial *mat = entry.material;
		if ( mat )
		{
			GL_UnloadMaterial( mat );
		}
#ifdef _DEBUG
		delete[] entry.m_pDebugName;
#endif
	}
	g_DecalLookup.clear(); g_DecalLookup.shrink_to_fit();
	g_DecalDictionary.clear();
}

