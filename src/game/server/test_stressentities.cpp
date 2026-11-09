// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "test_stressentities.h"
#include "world.h"

CStressEntityReg	*CStressEntityReg::s_pHead = nullptr;


// CStressEntityReg::s_pHead in array form for convenient access.
std::vector< CStressEntityReg* > g_StressEntityRegs;

std::vector< EHANDLE > g_StressEntities;


CBaseEntity* MoveToRandomSpot( CBaseEntity *pEnt )
{
	if ( pEnt )
	{
		CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
		if ( pLocalPlayer )
		{			
			Vector vForward;
			pLocalPlayer->EyeVectors(&vForward );

			UTIL_SetOrigin( pEnt, GetRandomSpot() );
		}
	}

	return pEnt;
}


Vector GetRandomSpot()
{
	CWorld *pEnt = GetWorldEntity();
	if ( pEnt )
	{
		Vector vMin, vMax;
		pEnt->GetWorldBounds( vMin, vMax );
		return Vector(
			RandomFloat( vMin.x, vMax.x ),
			RandomFloat( vMin.y, vMax.y ),
			RandomFloat( vMin.z, vMax.z ) );
	}
	else
	{
		return Vector( 0, 0, 0 );
	}
}


void Test_InitRandomEntitySpawner( const CCommand &args )
{
	// Put the list of registered functions into array form for convenience.
	g_StressEntityRegs.clear(); g_StressEntityRegs.shrink_to_fit();
	for ( CStressEntityReg *pCur=CStressEntityReg::GetListHead(); pCur; pCur=pCur->GetNext() )
		g_StressEntityRegs.push_back( pCur );

	// Create slots for all the entities..
	int nSlots = 100;
	if ( args.ArgC() >= 2 )
		nSlots = std::atoi( args[ 1 ] );

	g_StressEntities.clear(); g_StressEntities.shrink_to_fit();
	g_StressEntities.assign( nSlots, {} );

	Msg( "Test_InitRandomEntitySpawner: created %d slots.\n", nSlots );
}


void Test_SpawnRandomEntities( const CCommand &args )
{
	if ( args.ArgC() < 3 )
	{
		Error( "Test_SpawnRandomEntities <min # entities> <max # entities> missing arguments." );
	}

	if ( g_StressEntities.empty() )
	{
		Error( "Test_SpawnRandomEntities: not initialized (call Test_InitRandomEntitySpawner frst)." );
	} 

	int nMin = std::atoi( args[ 1 ] );
	int nMax = atoi( args[ 2 ] );
	int count = RandomInt( nMin, nMax );

	for ( int i=0; i < count; i++ )
	{
		int iSlot = RandomInt( 0, std::ssize( g_StressEntities ) - 1 );

		// Remove any old entity in this slot.
		if ( g_StressEntities[iSlot].Get() )
			UTIL_RemoveImmediate( g_StressEntities[iSlot] );

		// Create a new one in this slot.
		int iType = RandomInt( 0, std::ssize( g_StressEntityRegs ) - 1 );
		g_StressEntities[iSlot] = g_StressEntityRegs[iType]->GetFn()();
	}
}


void Test_RandomizeInPVS( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_RandomizeInPVS <percentage chance to change>" );
	}

	int percent = std::atoi( args[ 1 ] );
	for ( int i=0; i < std::ssize( g_StressEntities ); i++ )
	{
		CBaseEntity *pEnt = g_StressEntities[i];

		if ( pEnt )
		{
			if ( RandomInt( 0, 100 ) < percent )
			{
				if ( pEnt->IsEffectActive( EF_NODRAW ) )
					pEnt->RemoveEffects( EF_NODRAW );
				else
					pEnt->AddEffects( EF_NODRAW );
			}
		}
	}
}


void Test_RemoveAllRandomEntities()
{
	for ( int i=0; i < std::ssize( g_StressEntities ); i++ )
	{
		if ( g_StressEntities[i].Get() )
			UTIL_Remove( g_StressEntities[i] );
	}
}


ConCommand cc_Test_InitRandomEntitySpawner( "Test_InitRandomEntitySpawner", Test_InitRandomEntitySpawner, 0, FCVAR_CHEAT );
ConCommand cc_Test_SpawnRandomEntities( "Test_SpawnRandomEntities", Test_SpawnRandomEntities, 0, FCVAR_CHEAT );
ConCommand cc_Test_RandomizeInPVS( "Test_RandomizeInPVS", Test_RandomizeInPVS, 0, FCVAR_CHEAT );
ConCommand cc_Test_RemoveAllRandomEntities( "Test_RemoveAllRandomEntities", Test_RemoveAllRandomEntities, 0, FCVAR_CHEAT );
