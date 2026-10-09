// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
import ks.vstdlib;
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "itempents.h"
#include "cdll_client_int.h"
#elif defined(GAME_DLL)
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#endif
#include "decals.h"
#include "igamesystem.h"
#include "filesystem.h"

#ifdef CLIENT_DLL
#include "iefx.h"
#endif

#define DECAL_LIST_FILE "scripts/decals_subrect.txt"
//#define DECAL_LIST_FILE "scripts/decals.txt"
inline constexpr const char *TRANSLATION_DATA_SECTION = "TranslationData";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDecalEmitterSystem : public IDecalEmitterSystem, public CAutoGameSystem
{
public:
	CDecalEmitterSystem( char const *name ) : CAutoGameSystem( name )
	{
	}

	virtual bool		Init();
	virtual void		Shutdown();
	virtual void		LevelInitPreEntity();

	// Public interface
	virtual int			GetDecalIndexForName( char const *decalname );
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial );

private:
	char const		*ImpactDecalForGameMaterial( int gamematerial );
	void			LoadDecalsFromScript( char const *filename );
	void			Clear();

	struct DecalListEntry
	{
		DecalListEntry()
		{
			precache_index	= -1;
			weight			= 1.0f;
		}

		std::string_view	name;	// view into m_DecalFileNames
		int			precache_index;
		float		weight;
	};

	struct DecalEntry
	{
		DecalEntry()
		{
		}

		DecalEntry( const DecalEntry& src )
		{
			int c = std::ssize( src.indices );
			for ( int i = 0; i < c; i++ )
			{
				indices.push_back( src.indices[ i ] );
			}
		}

		DecalEntry& operator = ( const DecalEntry& src )
		{
			if ( this == &src )
				return *this;

			int c = std::ssize( src.indices );
			for ( int i = 0; i < c; i++ )
			{
				indices.push_back( src.indices[ i ] );
			}

			return *this;
		}

		std::vector< int >	indices;
	};

	std::vector< DecalListEntry >	m_AllDecals;
	ks::idict< DecalEntry >			m_Decals;
	ks::intern_pool					m_DecalFileNames;
	// Game material -> its decal in m_Decals; both are only cleared together.
	ks::idict< ks::idict< DecalEntry >::const_iterator >	m_GameMaterialTranslation;
};

static CDecalEmitterSystem g_DecalSystem( "CDecalEmitterSystem" );
IDecalEmitterSystem *decalsystem = &g_DecalSystem;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *decalname - 
// Output : int
//-----------------------------------------------------------------------------
int CDecalEmitterSystem::GetDecalIndexForName( char const *decalname )
{
	if ( !decalname  || !decalname[ 0 ] )
		return -1;

	DecalEntry *e = ks::find_ptr( m_Decals, decalname );
	if ( !e )
		return -1;

	contract_assert( e );
	int count = std::ssize( e->indices );
	if ( count <= 0 )
		return -1;

	float totalweight = 0.0f;
	int slot = 0;

	for ( int i = 0; i < count; i++ )
	{
		int idx = e->indices[ i ];
		DecalListEntry *item = &m_AllDecals[ idx ];
		contract_assert( item );
		
		if ( !totalweight )
		{
			slot = idx;
		}

		// Always assume very first slot will match
		totalweight += item->weight;
		if ( !totalweight || random->RandomFloat(0,totalweight) < item->weight )
		{
			slot = idx;
		}
	}

	return m_AllDecals[ slot ].precache_index;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDecalEmitterSystem::Init()
{
	LoadDecalsFromScript( DECAL_LIST_FILE );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::LevelInitPreEntity()
{
	// Precache all entries
	int c = std::ssize( m_AllDecals );
	for ( int i = 0 ; i < c; i++ )
	{
		DecalListEntry& e = m_AllDecals[ i ];
#if defined( CLIENT_DLL )
		e.precache_index = effects->Draw_DecalIndexFromName( (char *)e.name.data() );
#else
		e.precache_index = engine->PrecacheDecal( e.name.data() );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::LoadDecalsFromScript( char const *filename )
{
	KeyValues *kv = new KeyValues( filename );
	contract_assert( kv );
	if ( kv )
	{
		KeyValues *translation = nullptr;
		if ( kv->LoadFromFile( filesystem, filename ) )
		{
			KeyValues *p = kv;
			while ( p )
			{
				if ( p->GetFirstSubKey() )
				{
					char const *keyname = p->GetName();

					if ( !V_stricmp( keyname, TRANSLATION_DATA_SECTION ) )
					{
						translation = p;
					}
					else
					{
						DecalEntry entry;

						for ( KeyValues *sub = p->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
						{

							DecalListEntry decal;
							decal.precache_index = -1;
							decal.name = m_DecalFileNames.intern( sub->GetName() );
							decal.weight = sub->GetFloat();

							// Add to global list
							int idx = std::ssize( m_AllDecals ); m_AllDecals.push_back( decal );

							// Add index only to local list
							entry.indices.push_back( idx );
						}

						// Add entry to main dictionary; a repeated name keeps the first
						m_Decals.emplace( keyname, entry );
					}
				}
				p = p->GetNextKey();
			}
		}
		else
		{
			Msg( "CDecalEmitterSystem::LoadDecalsFromScript:  Unable to load '%s'\n", filename );
		}

		if ( !translation )
		{
			Msg( "CDecalEmitterSystem::LoadDecalsFromScript:  Script '%s' missing section '%s'\n",
				filename,
				TRANSLATION_DATA_SECTION );
		}
		else
		{
			// Now parse game material to entry translation table
			for ( KeyValues *sub = translation->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
			{
				// Don't add NULL string to list
				if ( !V_stricmp( sub->GetString(), "" ) )
					continue;

				auto itDecal = m_Decals.find( sub->GetString() );
				if ( itDecal != m_Decals.end() )
				{
					const char *value = sub->GetName();
					int gameMaterial;
					if ( !V_isdigit( value[0]) )
					{
						gameMaterial = std::toupper( value[0] );
					}
					else
					{
						gameMaterial = std::atoi( value );
					}

					char gm[ 2 ];
					gm[0] = gameMaterial;
					gm[1] = 0;
					m_GameMaterialTranslation.emplace( gm, itDecal );
				}
				else
				{
					Msg( "CDecalEmitterSystem::LoadDecalsFromScript:  Translation for game material type '%s' references unknown decal '%s'\n",
						sub->GetName(), sub->GetString() );
				}
			}
		}

		kv->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : gamematerial - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CDecalEmitterSystem::ImpactDecalForGameMaterial( int gamematerial )
{
	char gm[ 2 ];
	gm[0] = (char)gamematerial;
	gm[1] = 0;

	auto it = m_GameMaterialTranslation.find( gm );
	if ( it == m_GameMaterialTranslation.end() )
		return nullptr;

	return it->second->first.c_str();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::Shutdown()
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDecalEmitterSystem::Clear()
{
	m_DecalFileNames.clear();
	m_GameMaterialTranslation.clear();
	m_Decals.clear();
	m_AllDecals.clear(); m_AllDecals.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *decalName - 
//			gamematerial - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CDecalEmitterSystem::TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial )
{
	if ( gamematerial == CHAR_TEX_CONCRETE )
		return decalName;

	if ( !V_stricmp( decalName, "Impact.Concrete" ) )
	{
		if ( gamematerial == '-' )
			return "";

		char const *d = ImpactDecalForGameMaterial( gamematerial );
		if ( d )
		{
			return d;
		}
	}

	return decalName;
}
