// SPDX-License-Identifier: None
// Copyright (c) 2007 Turtle Rock Studios, Inc. - All Rights Reserved
// Copyright 2005 Turtle Rock Studios, Inc.

module;
#include "collisionutils.h"
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "edict.h"
#include "filesystem.h"
#include "declare_class.h"
module ks.server;

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :entityutil;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :nav_4;
import ks.core.dbg;
import ks.core.platform;
import ks.math.mathlib;
import ks.tier1.fmtstr;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.core.random;

class CColorCorrection;
class CFogController;
class CPostProcessController;
struct inputdata_t;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------

std::vector< CFogVolume * > TheFogVolumes;

ConVar fog_volume_debug( side_registry(), "fog_volume_debug", "0", 0, "If enabled, prints diagnostic information about the current fog volume" );

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS(fog_volume, CFogVolume);

IMPLEMENT_REFLECT_DATAMAP( CFogVolume )

//--------------------------------------------------------------------------------------------------------
CFogVolume *CFogVolume::FindFogVolumeForPosition( const Vector &position )
{
	CFogVolume *fogVolume = nullptr;
	for ( int i=0; i<std::ssize( TheFogVolumes ); ++i )
	{
		fogVolume = TheFogVolumes[i];

		Vector vecRelativeCenter;
		fogVolume->CollisionProp()->WorldToCollisionSpace( position, &vecRelativeCenter );
		if ( IsBoxIntersectingSphere( fogVolume->CollisionProp()->OBBMins(), fogVolume->CollisionProp()->OBBMaxs(), vecRelativeCenter, 1.0f ) )
		{
			break;
		}
		fogVolume = nullptr;
	}

	// This doesn't work well if there are multiple players or multiple fog volume queries per frame; might want to relocate this if that's the case
	if ( fog_volume_debug.GetBool() )
	{
		if ( fogVolume )
		{
			char fogVolumeName[256];
			fogVolume->GetKeyValue( "targetname", fogVolumeName, 256 );
			engine->Con_NPrintf( 0, "Fog Volume ""%s"" found at position (%f %f %f)", fogVolumeName, position.x, position.y, position.z );
			engine->Con_NPrintf( 1, "Fog: %s, post process: %s, color correct: %s", fogVolume->m_fogName.ToCStr(), fogVolume->m_postProcessName.ToCStr(), fogVolume->m_colorCorrectionName.ToCStr() );
		}
		else
		{
			engine->Con_NPrintf( 0, "No Fog Volume found at given position (%f %f %f)", position.x, position.y, position.z );
		}
	}
	
	return fogVolume;
}

//--------------------------------------------------------------------------------------------------------
CFogVolume::CFogVolume() :
	BaseClass(),
	m_bDisabled( false ),
	m_bInFogVolumesList( false )
{
}

//--------------------------------------------------------------------------------------------------------
CFogVolume::~CFogVolume()
{
	RemoveFromGlobalList();
}

//--------------------------------------------------------------------------------------------------------
void CFogVolume::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	SetSolid( SOLID_BSP );
	SetSolidFlags( FSOLID_NOT_SOLID );
	SetModel( STRING( GetModelName() ) );
}

//--------------------------------------------------------------------------------------------------------
void CFogVolume::AddToGlobalList()
{
	if ( !m_bInFogVolumesList )
	{
		TheFogVolumes.push_back( this );
		m_bInFogVolumesList = true;
	}
}

//--------------------------------------------------------------------------------------------------------
void CFogVolume::RemoveFromGlobalList()
{
	if ( m_bInFogVolumesList )
	{
		ks::erase_first( TheFogVolumes, this );
		m_bInFogVolumesList = false;
	}
}

//----------------------------------------------------------------------------
void CFogVolume::InputEnable( inputdata_t &data )
{
	m_bDisabled = false;
	AddToGlobalList();
}

//----------------------------------------------------------------------------
void CFogVolume::InputDisable( inputdata_t &data )
{
	m_bDisabled = true;
	RemoveFromGlobalList();
}

//----------------------------------------------------------------------------
// Called when the level loads or is restored
//----------------------------------------------------------------------------
void CFogVolume::Activate_Impl()
{
	BaseClass::Activate_Impl();

	m_hFogController = dynamic_cast< CFogController* >( gEntList.FindEntityByName( nullptr, m_fogName ) );
	m_hPostProcessController = dynamic_cast< CPostProcessController* >( gEntList.FindEntityByName( nullptr, m_postProcessName ) );
	m_hColorCorrectionController = dynamic_cast< CColorCorrection* >( gEntList.FindEntityByName( nullptr, m_colorCorrectionName ) );

	if ( !m_bDisabled )
	{
		AddToGlobalList();
	}
}

class CNavArea;
class CNavMesh;

using namespace ::ks::reflect;

// nav_merge.cpp
// Save/merge for partial nav meshes

//--------------------------------------------------------------------------------------------------------
void CNavArea::SaveToSelectedSet( KeyValues *areaKey ) const
{
	const char *placeName = TheNavMesh->PlaceToName( GetPlace() );
	areaKey->SetString( "Place", (placeName)?placeName:"" );

	areaKey->SetInt( "Attributes", GetAttributes() );
}

//--------------------------------------------------------------------------------------------------------
void CNavArea::RestoreFromSelectedSet( KeyValues *areaKey )
{
	SetPlace( TheNavMesh->NameToPlace( areaKey->GetString( "Place" ) ) );

	SetAttributes( areaKey->GetInt( "Attributes" ) );
}

//--------------------------------------------------------------------------------------------------------
class BuildSelectedSet
{
public:
	BuildSelectedSet( KeyValues *kv )
	{
		m_kv = kv;
		m_areaCount = 0;
	}

	bool operator() ( CNavArea *area )
	{
		CFmtStrN<32> name( "%d", area->GetID() );
		KeyValues *areaKey = m_kv->FindKey( name.Access(), true );
		if ( areaKey )
		{
			++m_areaCount;

			WriteCorner( area, areaKey, NORTH_WEST, "NorthWest" );
			WriteCorner( area, areaKey, NORTH_EAST, "NorthEast" );
			WriteCorner( area, areaKey, SOUTH_WEST, "SouthWest" );
			WriteCorner( area, areaKey, SOUTH_EAST, "SouthEast" );

			WriteConnections( area, areaKey, NORTH, "North" );
			WriteConnections( area, areaKey, SOUTH, "South" );
			WriteConnections( area, areaKey, EAST, "East" );
			WriteConnections( area, areaKey, WEST, "West" );

			area->SaveToSelectedSet( areaKey );
		}
		return true;
	}

	int Count( void ) const
	{
		return m_areaCount;
	}

private:
	void WriteCorner( CNavArea *area, KeyValues *areaKey, NavCornerType corner, const char *cornerName )
	{
		KeyValues *cornerKey = areaKey->FindKey( cornerName, true );
		if ( cornerKey )
		{
			Vector pos = area->GetCorner( corner );
			cornerKey->SetFloat( "x", pos.x );
			cornerKey->SetFloat( "y", pos.y );
			cornerKey->SetFloat( "z", pos.z );
		}
	}

	void WriteConnections( CNavArea *area, KeyValues *areaKey, NavDirType dir, const char *dirName )
	{
		KeyValues *dirKey = areaKey->FindKey( dirName, true );
		if ( dirKey )
		{
			for ( int i=0; i<area->GetAdjacentCount( dir ); ++i )
			{
				CNavArea *other = area->GetAdjacentArea( dir, i );
				if ( other && TheNavMesh->IsInSelectedSet( other ) )
				{
					CFmtStrN<32> name( "%d", i );
					dirKey->SetInt( name.Access(), other->GetID() );
				}
			}
		}
	}

	int m_areaCount;
	KeyValues *m_kv;
};

//--------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavSaveSelected( const CCommand &args )
{
	KeyValues *data = new KeyValues( "Selected Nav Areas" );
	data->SetInt( "version", 1 );

	BuildSelectedSet setBuilder( data );
	TheNavMesh->ForAllSelectedAreas( setBuilder );

	if ( !setBuilder.Count() )
	{
		Msg( "Not saving empty selected set to disk.\n" );
		data->deleteThis();
		return;
	}

	char fname[32];
	char path[MAX_PATH];
	if ( args.ArgC() == 2 )
	{
		V_FileBase( args[0], fname, sizeof( fname ) );
	}
	else
	{
		V_strncpy( fname, STRING( gpGlobals->mapname ), sizeof( fname ) );
	}

	int i;
	for ( i=0; i<1000; ++i )
	{
		V_snprintf( path, sizeof( path ), "maps/%s_selected_%4.4d.txt", fname, i );
		if ( !filesystem->FileExists( path ) )
		{
			break;
		}
	}

	if ( i == 1000 )
	{
		Msg( "Unable to find a filename to save the selected set to disk.\n" );
		data->deleteThis();
		return;
	}

	if ( !data->SaveToFile( filesystem, path ) )
	{
		Msg( "Unable to save the selected set to disk.\n" );
	}

	Msg( "Selected set saved to %s.  Use 'nav_merge_mesh %s_selected_%4.4d' to merge it into another mesh.\n", path, fname, i );
	data->deleteThis();
}

//--------------------------------------------------------------------------------------------------------
static void nav_save_selected( const CCommand &args );
static ConCommand nav_save_selected_command( side_registry(), "nav_save_selected", nav_save_selected, "Writes the selected set to disk for merging into another mesh via nav_merge_mesh.", FCVAR_GAMEDLL | FCVAR_CHEAT );
static void nav_save_selected( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	TheNavMesh->CommandNavSaveSelected( args );
}

//--------------------------------------------------------------------------------------------------------
Vector ReadCorner( KeyValues *areaKey, const char *cornerName )
{
	Vector pos( vec3_origin );
	KeyValues *cornerKey = areaKey->FindKey( cornerName, false );
	if ( cornerKey )
	{
		pos.x = cornerKey->GetFloat( "x" );
		pos.y = cornerKey->GetFloat( "y" );
		pos.z = cornerKey->GetFloat( "z" );
	}

	return pos;
}

//--------------------------------------------------------------------------------------------------------
void ReconnectMergedArea( ks::idict< CNavArea * > &newAreas, KeyValues *areaKey, NavDirType dir, const char *dirName )
{
	CNavArea **ppArea = ks::find_ptr( newAreas, areaKey->GetName() );
	if ( !ppArea )
	{
		contract_assert( false );
		return;
	}

	CNavArea *area = *ppArea;

	KeyValues *dirKey = areaKey->FindKey( dirName, true );
	if ( dirKey )
	{
		KeyValues *connection = dirKey->GetFirstValue();
		while ( connection )
		{
			const char *otherID = connection->GetString();
			CNavArea **ppOther = ks::find_ptr( newAreas, otherID );
			contract_assert( ppOther );
			if ( ppOther )
			{
				CNavArea *other = *ppOther;

				area->ConnectTo( other, dir );	// only a 1-way connection.  the other area will connect back to us.
			}

			connection = connection->GetNextValue();
		}
	}
}

//--------------------------------------------------------------------------------------------------------
void CNavMesh::CommandNavMergeMesh( const CCommand &args )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "Usage: nav_merge_mesh filename\n" );
		return;
	}

	char fname[64];
	char path[MAX_PATH];
	V_FileBase( args[1], fname, sizeof( fname ) );
	V_snprintf( path, sizeof( path ), "maps/%s.txt", fname );

	KeyValues *data = new KeyValues( "Nav Selected Set" );
	if ( !data->LoadFromFile( filesystem, path ) )
	{
		Msg( "Unable to load %s.\n", path );
	}
	else
	{
		// Loaded the data - plug it into the existing mesh!

		// First add the areas, and put them in the correct places.  We can save off the new area ID
		// at the same time.
		ks::idict< CNavArea * > newAreas;
		std::vector< CNavArea * > areaVector;
		KeyValues *areaKey = data->GetFirstSubKey();
		while ( areaKey )
		{
			Vector northWest = ReadCorner( areaKey, "NorthWest" );
			Vector northEast = ReadCorner( areaKey, "NorthEast" );
			Vector southWest = ReadCorner( areaKey, "SouthWest" );
			Vector southEast = ReadCorner( areaKey, "SouthEast" );

			CNavArea *newArea = TheNavMesh->CreateArea();
			if (newArea == nullptr)
			{
				Warning( "nav_merge_mesh: Out of memory\n" );
				return;
			}

			newArea->Build( northWest, northEast, southEast, southWest );
			TheNavAreas.push_back( newArea );
			TheNavMesh->AddNavArea( newArea );
			areaVector.push_back( newArea );

			// save the new ID for connections
			[[maybe_unused]] bool bInserted = newAreas.emplace( areaKey->GetName(), newArea ).second;
			contract_assert( bInserted );

			// Restore additional data
			newArea->RestoreFromSelectedSet( areaKey );

			areaKey = areaKey->GetNextKey();
		}

		// Go back and reconnect the new areas to each other
		areaKey = data->GetFirstSubKey();
		while ( areaKey )
		{
			ReconnectMergedArea( newAreas, areaKey, NORTH, "North" );
			ReconnectMergedArea( newAreas, areaKey, SOUTH, "South" );
			ReconnectMergedArea( newAreas, areaKey, EAST, "East" );
			ReconnectMergedArea( newAreas, areaKey, WEST, "West" );

			areaKey = areaKey->GetNextKey();
		}

		// Connect selected areas with pre-existing areas
		StitchAreaSet( &areaVector );
	}

	data->deleteThis();
}

//--------------------------------------------------------------------------------------------------------
int NavMeshMergeAutocomplete( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	const char *commandName = "nav_merge_mesh";
	int numMatches = 0;
	partial += V_strlen( commandName ) + 1;
	int partialLength = V_strlen( partial );

	FileFindHandle_t findHandle;
	char txtFilenameNoExtension[ MAX_PATH ];
	const char *txtFilename = filesystem->FindFirstEx( "maps/*_selected_*.txt", "MOD", &findHandle );
	while ( txtFilename )
	{
		V_FileBase( txtFilename, txtFilenameNoExtension, sizeof( txtFilenameNoExtension ) );
		if ( !V_strnicmp( txtFilenameNoExtension, partial, partialLength ) && V_stristr( txtFilenameNoExtension, "_selected_" ) )
		{
			// Add the place name to the autocomplete array
			V_snprintf( commands[ numMatches++ ], COMMAND_COMPLETION_ITEM_LENGTH, "%s %s", commandName, txtFilenameNoExtension );

			// Make sure we don't try to return too many place names
			if ( numMatches == COMMAND_COMPLETION_MAXITEMS )
				return numMatches;
		}

		txtFilename = filesystem->FindNext( findHandle );
	}
	filesystem->FindClose( findHandle );

	return numMatches;
}

//--------------------------------------------------------------------------------------------------------
static void nav_merge_mesh( const CCommand &args );
static ConCommand nav_merge_mesh_command( side_registry(), "nav_merge_mesh", nav_merge_mesh, "Merges a saved selected set into the current mesh.", FCVAR_GAMEDLL | FCVAR_CHEAT, NavMeshMergeAutocomplete );
static void nav_merge_mesh( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	TheNavMesh->CommandNavMergeMesh( args );
}

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

class CBaseEntity;
class CBaseFilter;
struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// class CPointEntityFinder
//
// Purpose: Finds an entity using a specified heuristic and outputs it as !caller
//			with the OnFoundEntity output.
//-----------------------------------------------------------------------------

enum EntFinderMethod_t
{
	ENT_FIND_METHOD_NEAREST = 0,
	ENT_FIND_METHOD_FARTHEST,
	ENT_FIND_METHOD_RANDOM,
};

class CPointEntityFinder : public CBaseEntity
{
	public: void Activate_Impl( void );
	private:

	DECLARE_CLASS( CPointEntityFinder, CBaseEntity );

private:

	EHANDLE						m_hEntity;
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t					m_iFilterName;
	CHandle<class CBaseFilter>	m_hFilter;
	[[= ks::reflect::Key{ .name = "referencename" } ]] string_t					m_iRefName;
	EHANDLE						m_hReference;

	[[= ks::reflect::Key{ .name = "method" } ]] EntFinderMethod_t			m_FindMethod;

	void FindEntity( void );
	void FindByDistance( void );
	void FindByRandom( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "FindEntity", .type = FIELD_VOID } ]] void InputFindEntity( inputdata_t &inputdata );

	// Output handlers
	[[= ks::reflect::Key{ .name = "OnFoundEntity" } ]] COutputEvent m_OnFoundEntity;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_entity_finder, CPointEntityFinder );

IMPLEMENT_REFLECT_DATAMAP( CPointEntityFinder )

void CPointEntityFinder::Activate_Impl( void )
{
	// Get the filter, if it exists.
	if (m_iFilterName != NULL_STRING)
	{
		m_hFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( nullptr, m_iFilterName ));
	}

	BaseClass::Activate_Impl();
}

void CPointEntityFinder::FindEntity( void )
{
	// Get the reference entity, if it exists.
	if (m_iRefName != NULL_STRING)
	{
		m_hReference = gEntList.FindEntityByName( nullptr, m_iRefName );
	}

	switch ( m_FindMethod )
	{

	case ( ENT_FIND_METHOD_NEAREST ):
		FindByDistance();
		break;
	case ( ENT_FIND_METHOD_FARTHEST ):
		FindByDistance();
		break;
	case ( ENT_FIND_METHOD_RANDOM ):
		FindByRandom();
		break;
	}
}

void CPointEntityFinder::FindByDistance( void )
{
	m_hEntity = nullptr;
	CBaseFilter *pFilter = m_hFilter.Get();

// go through each entity and determine whether it's closer or farther from the current entity.  Pick according to Method selected.

	float flBestDist = 0;
	CBaseEntity *pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		if ( FStrEq( STRING( pEntity->m_iClassname ), "worldspawn" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "soundent" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "player_manager" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "bodyque" ) 
			|| FStrEq( STRING( pEntity->m_iClassname ), "ai_network" ) 
			|| pEntity == this
			|| ( pFilter && !( pFilter->PassesFilter( this, pEntity ) ) ) )	   
		{
			pEntity = gEntList.NextEnt( pEntity );
			continue;
		}

		// if we have a reference entity, use that, otherwise, check against 'this'
		Vector vecStart;
		if ( m_hReference )
		{
			vecStart = m_hReference->GetAbsOrigin();
		}
		else
		{
			vecStart = GetAbsOrigin();
		}

		// init m_hEntity with a valid entity.
		if (m_hEntity == nullptr )
		{
			m_hEntity = pEntity;
			flBestDist = ( pEntity->GetAbsOrigin() - vecStart ).LengthSqr();
		}

		float flNewDist = ( pEntity->GetAbsOrigin() - vecStart ).LengthSqr();

		switch ( m_FindMethod )
		{

		case ( ENT_FIND_METHOD_NEAREST ):
			if ( flNewDist < flBestDist )
			{
				m_hEntity = pEntity;
				flBestDist = flNewDist;
			}
			break;

		case ( ENT_FIND_METHOD_FARTHEST ):
			if ( flNewDist > flBestDist )
			{
				m_hEntity = pEntity;
				flBestDist = flNewDist;
			}
			break;

		default:
			contract_assert( false );
			break;
		}

		pEntity = gEntList.NextEnt( pEntity );
	}
}

void CPointEntityFinder::FindByRandom( void )
{
	// TODO: optimize the case where there is no filter
	m_hEntity = nullptr;
	CBaseFilter *pFilter = m_hFilter.Get();
	std::vector< CBaseEntity * > ValidEnts;

	CBaseEntity *pEntity = gEntList.FirstEnt();
	do	   // note all valid entities.
	{
		if ( pFilter &&  pFilter->PassesFilter( this, pEntity ) )
		{
			ValidEnts.push_back( pEntity );
		}

		pEntity = gEntList.NextEnt( pEntity );

	} while ( pEntity );

	// pick one at random
	if ( !ValidEnts.empty() )
	{
		m_hEntity = ValidEnts[ RandomInt( 0, std::ssize( ValidEnts ) - 1 )];
	}
}

void CPointEntityFinder::InputFindEntity( inputdata_t &inputdata )
{
	FindEntity();

	m_OnFoundEntity.FireOutput( inputdata.pActivator, m_hEntity );
}
