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
#include "baseentity_shared.h"
#include "gamerules.h"
#include "vphysics_interface.h"
#include "physics.h"
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
#include "entitylist.h"
#include "physics.h"
#endif
#include "precache_register.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem.h"

static const char *s_pPrecacheResourceTypeName[] =
{
	"vgui_resource",
	"material",		
	"model",			
	"scriptsound",		// NOTE: This is named this way for backward compat in reading from kv files	
	"particle_system",	
	"entity",
	"decal",
	"pmaterial",
	"dependency_file",
	"game_material_decals",
	"physics_gamesounds",
	"shared",
};

//-----------------------------------------------------------------------------
// Provides callback to do actual precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheHandler : public IPrecacheHandler
{
public:
	void CacheResource( PrecacheResourceType_t nType, const char *pName, bool bPrecache, ResourceList_t hResourceList, int *pIndex = nullptr );

private:
	void CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList );
	void PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList );
	void PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList );
};

//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
static CPrecacheRegister	s_PrecacheRegister;
CPrecacheRegister *g_pPrecacheRegister = &s_PrecacheRegister;

static CPrecacheHandler		s_PrecacheHandler;
IPrecacheHandler *g_pPrecacheHandler = &s_PrecacheHandler;

bool CPrecacheRegister::Init()
{ 
	return true; 
}

//-----------------------------------------------------------------------------
// Level startup, shutdown
//-----------------------------------------------------------------------------
void CPrecacheRegister::LevelInitPreEntity()
{
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Start" );
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, GLOBAL, nullptr, true, RESOURCE_LIST_INVALID, false );
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Finish" );

#ifdef CLIENT_DLL
	//FIXME: Double check this
	//Finally, force the cache of these materials
	COM_TimestampedLog( "LevelInitPreEntity - CacheUsedMaterials - Start" );
	materials->CacheUsedMaterials();
	COM_TimestampedLog( "LevelInitPreEntity - CacheUsedMaterials - Finish" );
#endif
}

void CPrecacheRegister::LevelShutdownPostEntity()
{
	// FIXME: How to uncache all resources cached during the course of the level?
	g_pPrecacheSystem->UncacheAll( g_pPrecacheHandler );

	if ( g_pResourceAccessControl )
	{
		g_pResourceAccessControl->DestroyAllResourceLists();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList )
{
	static_assert( std::size( s_pPrecacheResourceTypeName ) == PRECACHE_RESOURCE_TYPE_COUNT );

	KeyValues *pValues = new KeyValues( "ResourceFile" );

	if ( !pValues->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" ) )
	{
		Warning( "Can't open %s for client precache info.", pFilename );
		pValues->deleteThis();
		return;
	}

	for ( KeyValues *pData = pValues->GetFirstSubKey(); pData != nullptr; pData = pData->GetNextKey() )
	{
		const char *pszType = pData->GetName();
		const char *pszFile = pData->GetString();

		if ( V_strlen( pszType ) == 0 || V_strlen( pszFile ) == 0 )
			continue;

		bool bFoundMatch = false;
		for ( int i = 0; i < PRECACHE_RESOURCE_TYPE_COUNT; ++i )
		{
			if ( !V_stricmp( pData->GetName(), s_pPrecacheResourceTypeName[i] ) )
			{
				CacheResource( (PrecacheResourceType_t)i, pszFile, bPrecache, hResourceList );
				bFoundMatch = true;
				break;
			}
		}

		if ( !bFoundMatch )
		{
			Warning( "Error in precache file \"%s\":\n", pFilename );
			Warning( "\tUnknown resource type specified \"%s\", value \"%s\"\n", pszType, pszFile );
		}
	}

	pValues->deleteThis();
}


//-----------------------------------------------------------------------------
// Precaches game material decals
//-----------------------------------------------------------------------------
void CPrecacheHandler::PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList )
{
}

void CPrecacheHandler::PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList )
{
	if ( !bPrecache )
		return;

	// precache the surface prop sounds
	bool bBulletSounds = !V_stricmp( pName, "BulletSounds" );
	bool bStepSounds = !V_stricmp( pName, "StepSounds" );
	bool bPhysicsImpactSounds = !V_stricmp( pName, "PhysicsImpactSounds" );

	for ( int i = 0; i < physprops->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = physprops->GetSurfaceData( i );
		contract_assert( pprop );

		if ( bBulletSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.bulletImpact );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}

		if ( bStepSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.walkStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.walkStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.runStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.runStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}

		if ( bPhysicsImpactSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.impactSoft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.impactHard );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.scrapeSmooth );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.scrapeRough );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.rolling );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.breakSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.strainSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}
	}
}


//-----------------------------------------------------------------------------
// Caches/uncaches resources
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResource( PrecacheResourceType_t nType, const char *pName, 
	bool bPrecache, ResourceList_t hResourceList, int *pIndex )
{
	if ( bPrecache )
	{
		if ( pIndex )
		{
			*pIndex = 0;
		}

		switch( nType )
		{
		case VGUI_RESOURCE:
			break;

		case MATERIAL:
			PrecacheMaterial( pName ); 
			if ( pIndex )
			{
				*pIndex = GetMaterialIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MATERIAL, pName );
			}
			break;

		case PARTICLE_MATERIAL:
			{
#ifdef CLIENT_DLL
				void* nIndex = ParticleMgr()->GetPMaterial( pName );
#else
				void* nIndex = 0;
#endif
				if ( pIndex )
				{
					void** pIndexMaterial = (void**)pIndex;

					*pIndexMaterial = nIndex;
				}
			}
			break;

		case GAME_MATERIAL_DECALS:
			PrecacheGameMaterialDecals( bPrecache, hResourceList );
			break;

		case PHYSICS_GAMESOUNDS:
			PrecachePhysicsSounds( pName, bPrecache, hResourceList );
			break;

		case DECAL:
			{
				int nIndex = UTIL_PrecacheDecal( pName, true );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
			}
			break;

		case MODEL:
			{
				int nIndex = CBaseEntity::PrecacheModel( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MODEL, pName );
				}
			}
			break;

		case GAMESOUND:
			{
				int nIndex = CBaseEntity::PrecacheScriptSound( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_GAMESOUND, pName );
				}
			}
			break;

		case PARTICLE_SYSTEM:
			PrecacheParticleSystem( pName );
			if ( pIndex )
			{
				*pIndex = GetParticleSystemIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_PARTICLE_SYSTEM, pName );
			}
			break;

		case ENTITY:
			UTIL_PrecacheOther( pName );
			break;

		case SHARED:
			g_pPrecacheSystem->Cache( this, SHARED_SYSTEM, pName, bPrecache, hResourceList, false );
			break;

		case KV_DEP_FILE:
			CacheResourceFile( pName, bPrecache, hResourceList );
			break;
		}
		return;
	}

	// Blat out value
	if ( pIndex )
	{
		*pIndex = 0;
	}

	switch( nType )
	{
	case VGUI_RESOURCE:
		break;
	case MATERIAL:
		break;
	case MODEL:
		break;
	case GAMESOUND:
		break;
	case PARTICLE_SYSTEM:
		break;
	case ENTITY:
		break;
	case DECAL:
		break;
	case KV_DEP_FILE:
		break;
	}
}
