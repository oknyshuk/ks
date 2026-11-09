// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// NavMesh.cpp
// Implementation of Navigation Mesh interface
// Author: Michael S. Booth, 2003-2004

#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "filesystem.h"
#include "cs_nav_mesh.h"
#include "cs_nav_node.h"
#include "cs_nav_area.h"
#include "cs_nav_pathfind.h"
#include "cs_shareddefs.h"
#include "cs_gamerules.h"
#include "bot_util.h"
#include "cs_bot_manager.h"

#ifndef CLIENT_DLL
#include "mapinfo.h"
#endif

extern ConVar showtriggers;
class CFuncBlockDMSpawns : public CFuncBrush
{
	DECLARE_CLASS( CFuncBlockDMSpawns, CFuncBrush );

	public: void Spawn_Impl( void )
	{
		SetSolid( SOLID_BSP );
		AddSolidFlags( FSOLID_NOT_SOLID );

		SetModel( STRING( GetModelName() ) );    // set size and link into world


		if ( !showtriggers.GetInt() )
		{
			AddEffects( EF_NODRAW );
		}
	}
	private:
};

LINK_ENTITY_TO_CLASS( func_block_dm_spawns, CFuncBlockDMSpawns );




extern ConVar mp_randomspawn;
ConVar mp_guardian_target_site( "mp_guardian_target_site", "-1", FCVAR_RELEASE | FCVAR_GAMEDLL, "If set to the index of a bombsite, will cause random spawns to be only created near that site." );

//--------------------------------------------------------------------------------------------------------------
// Counter-Strike extensions of CNavMesh

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the Navigation Mesh to initial values
 */
void CNavMesh::Reset( void )
{
	m_refreshChickenTimer.Start( 5.0f );
	m_refreshDMSpawnTimer.Start( 1.0f );

	ResetBase();
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Zero player counts in all areas
 */
void CNavMesh::ClearPlayerCounts( void )
{
	for ( CNavArea *pNavArea : TheNavAreas )
	{
		CNavArea *area = pNavArea;
		area->ClearPlayerCount();
	}
}


//--------------------------------------------------------------------------------------------------------------
// Keep desired number of chickens alive in map
void CNavMesh::MaintainChickenPopulation( void )
{
	if ( m_desiredChickenCount > 0 && m_refreshChickenTimer.IsElapsed() )
	{
		m_refreshChickenTimer.Start( RandomFloat( 10.0f, 20.0f ) );

		int actualCount = 0;
		for( int i=0; i<std::ssize( m_chickenVector ); ++i )
		{
			if ( m_chickenVector[i] != nullptr )
			{
				++actualCount;
			}
		}

		if ( actualCount < m_desiredChickenCount )
		{
			int need = m_desiredChickenCount - actualCount;

			for( int k=0; k<need; ++k )
			{
				// find a good spot to spawn a chicken
				CBaseEntity *chicken = nullptr;

				for( int attempts=0; attempts<10; ++attempts )
				{
					int which = RandomInt( 0, std::ssize( TheNavAreas )-1 );

					CNavArea *testArea = TheNavAreas[ which ];

					const float tooSmall = 50.0f;

					if ( testArea && testArea->GetSizeX() > tooSmall && testArea->GetSizeY() > tooSmall )
					{
						if ( !UTIL_IsVisibleToTeam( testArea->GetCenter(), TEAM_CT ) &&
							 !UTIL_IsVisibleToTeam( testArea->GetCenter(), TEAM_TERRORIST ) )
						{
							// don't spawn a chicken on top of another chicken
							int n;
							for( n=0; n<std::ssize( m_chickenVector ); ++n )
							{
								if ( m_chickenVector[n] == nullptr )
									continue;

								const float tooClose = 50.0f;
								Vector between = m_chickenVector[n]->GetAbsOrigin() - testArea->GetCenter();
								if ( between.IsLengthLessThan( tooClose ) )
									break;
							}

							if ( n >= std::ssize( m_chickenVector ) )
							{
								// found a good spot - spawn a chicken here
								chicken = CreateEntityByName( "chicken" );
								if ( chicken )
								{
									chicken->SetAbsOrigin( testArea->GetCenter() );

									DispatchSpawn( chicken );
									m_chickenVector.push_back( chicken );
								}

								break;
							}
						}
					}
				}

				if ( !chicken )
				{
					// couldn't spawn a chicken - try again later
					return;
				}
			}
		}
	}
}


NavErrorType CNavMesh::PostLoad( unsigned int version )
{
	if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
		m_desiredChickenCount = 10;
	else
		m_desiredChickenCount = g_pMapInfo ? g_pMapInfo->m_iPetPopulation : 0;
	m_chickenVector.clear();

	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
	{
		// in Guardian mode, remove all of the mapper placed DM spawn points
		CBaseEntity *pEntity = nullptr;
		while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "info_deathmatch_spawn" ) ) != nullptr )
		{
			UTIL_Remove( pEntity );
		}
	}

	ResetDMSpawns();

	return PostLoadBase(version);
}

static void dm_reset_spawns( const CCommand &args );
static ConCommand dm_reset_spawns_command( "dm_reset_spawns", dm_reset_spawns, "" );
static void dm_reset_spawns( const CCommand &args )
{
	TheNavMesh->ResetDMSpawns();
	DevMsg( "Manually resetting deathmatch spawns.\n");

}

void CNavMesh::ResetDMSpawns( void )
{
	for ( auto &hDMSpawn : m_DMSpawnVector )
	{
		UTIL_Remove( hDMSpawn );
	}

	m_desiredDMSpawns = 50;
	m_consecutiveFailedAttempts = 0;
	m_DMSpawnVector.clear();
	MaintainDMSpawnPopulation();
}

void CNavMesh::MaintainDMSpawnPopulation( void )
{
	if ( mp_randomspawn.GetInt() == 0 )
		return;

	bool bDisableAutoGeneratedDMSpawns = ( g_pMapInfo ? g_pMapInfo->m_bDisableAutoGeneratedDMSpawns : false );

	// HACK: Force this to run in guardian, even if the map explicitly placed dm spawns
	if ( bDisableAutoGeneratedDMSpawns && !CSGameRules()->IsPlayingCoopGuardian() )
		return;

	if ( m_desiredDMSpawns > 0 && m_refreshDMSpawnTimer.IsElapsed() )
	{
		m_refreshDMSpawnTimer.Start( 1.0f );

		int actualCount = 0;
		for( int i=0; i<std::ssize( m_DMSpawnVector ); ++i )
		{
			if ( m_DMSpawnVector[i] != nullptr )
			{
				++actualCount;
			}
		}

		// If the map isn't large enough to support as many spawns as m_desiredDMSpawns then shrink m_desiredDMSpawns to whatever we can get.
		if ( m_consecutiveFailedAttempts >= 100 )
		{
			m_desiredDMSpawns = actualCount;

			m_consecutiveFailedAttempts = 0;

			Warning( "Giving up attempts to make more random spawn points. Current count: %i.\n", actualCount );

		}

		if ( actualCount < m_desiredDMSpawns )
		{
//			float calctime = Plat_FloatTime(); MEASURE PERF 1/2

			int need = m_desiredDMSpawns - actualCount;

			int spawnsToTry = std::max( 10, need );

			for( int k=0; k < spawnsToTry; k++ )
			{
				// find a good spot to spawn a Deathmatch spawnpoint
				CBaseEntity *DMSpawn = nullptr;

				m_consecutiveFailedAttempts++;

				for( int attempts = 0; attempts < 10; attempts++ )
				{

					CNavArea *testArea = nullptr;
					if ( mp_guardian_target_site.GetInt() >= 0 )
					{
						const CCSBotManager::Zone *zone = TheCSBots()->GetZone( mp_guardian_target_site.GetInt() );
						if ( zone )
						{
							testArea = zone->m_area[ RandomInt( 0, zone->m_areaCount - 1 ) ];
						}
					}
					else
					{
						testArea = TheNavAreas[ RandomInt( 0, std::ssize( TheNavAreas ) - 1 ) ];
					}

					const float tooSmall = ( mp_guardian_target_site.GetInt() >= 0 ) ? 35.0f : 100.0f;

					if ( testArea && testArea->GetSizeX() > tooSmall && testArea->GetSizeY() > tooSmall )
					{
						bool bBadNavArea = false;

						// don't spawn a DMSpawn too close to another DMSpawn
						for( int n = 0; n < std::ssize( m_DMSpawnVector ); n++ )
						{
							if ( m_DMSpawnVector[n] == nullptr )
								continue;

							float tooClose = ( mp_guardian_target_site.GetInt() >= 0 ) ? 50.0f : 300.0f;
							Vector between = m_DMSpawnVector[n]->GetAbsOrigin() - testArea->GetCenter();
							if ( between.IsLengthLessThan( tooClose ) )
							{
								bBadNavArea = true;
								break;
							}
						}

						if ( bBadNavArea )
							continue;

						if ( IsSpawnBlockedByTrigger( testArea->GetCenter() + Vector( 0, 0, 16 ) ) )
							continue;

						// check that we can path from the nav area to a ct spawner to confirm it isn't orphaned.
						CBaseEntity *CTSpawn = gEntList.FindEntityByClassname( nullptr, "info_player_counterterrorist" );

						if ( CTSpawn )
						{
							CNavArea *CTSpawnArea = GetNearestNavArea( CTSpawn->GetAbsOrigin() );

							ShortestPathCost cost;

							bool bNotOrphaned = NavAreaBuildPath( testArea, CTSpawnArea, nullptr, cost );

							if ( !bNotOrphaned )
							{
								const char* szTSpawnEntName = "info_player_terrorist";
								if ( CSGameRules()->IsPlayingCoopMission() )
									szTSpawnEntName = "info_enemy_terrorist_spawn";

								// double check that we can path from the nav area to a t spawner to confirm it isn't orphaned.
								CBaseEntity *TSpawn = gEntList.FindEntityByClassname( nullptr, szTSpawnEntName );
								if ( TSpawn )
								{
									CNavArea *TSpawnArea = GetNearestNavArea( TSpawn->GetAbsOrigin() );
									ShortestPathCost cost2;
									bNotOrphaned = NavAreaBuildPath( testArea, TSpawnArea, nullptr, cost2 );

									if ( !bNotOrphaned )
										continue;
								}							
							}
						}

						// found a good spot - spawn a DMSpawn here
						DMSpawn = CreateEntityByName( "info_deathmatch_spawn" );
						if ( DMSpawn )
						{
							DMSpawn->SetAbsOrigin( testArea->GetCenter() + Vector( 0, 0, 16 ));

							DispatchSpawn( DMSpawn );
							m_DMSpawnVector.push_back( DMSpawn );

							m_consecutiveFailedAttempts = 0;

//							Msg( " %f :CREATED DM SPAWN %i.\n", gpGlobals->curtime, m_DMSpawnVector.Count() );
						}

						break;
					}
				}
			}

//			MEASURE PERF 2/2
// 			calctime = Plat_FloatTime() - calctime;
// 			Msg( "DM SPAWN SEARCH CALC TIME: %f\n", calctime );
		}
	}


}

bool CNavMesh::IsSpawnBlockedByTrigger( Vector pos )
{
	CBaseEntity *list[32];

	Ray_t ray;
	ray.Init( pos, pos + Vector( 1,1,1 ) );

	int nCount = AllEdictsAlongRay( list, 1024, ray, 0 );

	for ( int i = 0; i < nCount; i++ )
	{
		if ( FClassnameIs( list[i], "func_block_dm_spawns" ) )
			return true;
	}

	return false;
}

int CNavMesh::AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );

	::partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, &rayEnum );

	return rayEnum.GetCount();
}
