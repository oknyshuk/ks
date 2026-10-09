// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
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
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "recipientfilter.h"
#include "const.h"
#include "toolframework/itoolentity.h"
#include "entitylist.h"
#include "toolframework/itoolsystem.h"
#include "icliententity.h"
#include "iserverentity.h"
#include "sceneentity.h"
#include "particles/particles.h"
#include "mapentities_shared.h"
#include "TemplateEntities.h"
#include "mapentities.h"
#include "point_template.h"

class CFoundryEntitySpawnRecord
{
public:
	std::vector< char > m_VMFText;
	int m_iEntityIndex;
	int m_iHammerID;
	int m_iSerialNumber;
	int m_debugOverlays;
};

static std::vector<CFoundryEntitySpawnRecord> g_FoundryEntitySpawnRecords;


//-----------------------------------------------------------------------------
// Interface from engine to tools for manipulating entities
//-----------------------------------------------------------------------------
class CServerTools : public IServerTools
{
public:
	// Inherited from IServerTools
	virtual IServerEntity *GetIServerEntity( IClientEntity *pClientEntity );
	virtual bool GetPlayerPosition( Vector &org, QAngle &ang, IClientEntity *pClientPlayer = nullptr );
	virtual bool SnapPlayerToPosition( const Vector &org, const QAngle &ang, IClientEntity *pClientPlayer = nullptr );
	virtual int GetPlayerFOV( IClientEntity *pClientPlayer = nullptr );
	virtual bool SetPlayerFOV( int fov, IClientEntity *pClientPlayer = nullptr );
	virtual bool IsInNoClipMode( IClientEntity *pClientPlayer = nullptr );
	virtual void *FirstEntity( void );
	virtual void *NextEntity( void *pEntity );
	virtual void *FindEntityByHammerID( int iHammerID );
	virtual bool GetKeyValue( void *pEntity, const char *szField, char *szValue, int iMaxLen );
	virtual bool SetKeyValue( void *pEntity, const char *szField, const char *szValue );
	virtual bool SetKeyValue( void *pEntity, const char *szField, float flValue );
	virtual bool SetKeyValue( void *pEntity, const char *szField, const Vector &vecValue );
	virtual void *CreateEntityByName( const char *szClassName );
	virtual void DispatchSpawn( void *pEntity );
	virtual void ReloadParticleDefintions( const char *pFileName, const void *pBufData, int nLen );
	virtual void AddOriginToPVS( const Vector &org );
	virtual bool DestroyEntityByHammerId( int iHammerID );
	virtual bool RespawnEntitiesWithEdits( CEntityRespawnInfo *pInfos, int nInfos );
	virtual void MoveEngineViewTo( const Vector &vPos, const QAngle &vAngles );
	virtual void RemoveEntity( int nHammerID );
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CServerTools g_ServerTools;
IServerTools *g_pServerTools = &g_ServerTools;



IServerEntity *CServerTools::GetIServerEntity( IClientEntity *pClientEntity )
{
	if ( pClientEntity == nullptr )
		return nullptr;

	CBaseHandle ehandle = pClientEntity->GetRefEHandle();
	if ( ehandle.GetEntryIndex() >= MAX_EDICTS )
		return nullptr; // the first MAX_EDICTS entities are networked, the rest are client or server only

	IHandleEntity *pEnt = gEntList.LookupEntityByNetworkIndex( ehandle.GetEntryIndex() );
	if ( pEnt == nullptr )
		return nullptr;

	CBaseHandle h = gEntList.GetNetworkableHandle( ehandle.GetEntryIndex() );
	const int mask = ( 1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS ) - 1;
	if ( !h.IsValid() || ( ( h.GetSerialNumber() & mask ) != ( ehandle.GetSerialNumber() & mask ) ) )
		return nullptr;

	IServerUnknown *pUnk = static_cast< IServerUnknown* >( pEnt );
	return pUnk->GetBaseEntity();
}

bool CServerTools::GetPlayerPosition( Vector &org, QAngle &ang, IClientEntity *pClientPlayer )
{
	IServerEntity *pServerPlayer = GetIServerEntity( pClientPlayer );
	CBasePlayer *pPlayer = pServerPlayer ? ( CBasePlayer* )pServerPlayer : UTIL_GetLocalPlayer();
	if ( pPlayer == nullptr )
		return false;

	org = pPlayer->EyePosition();
	ang = pPlayer->EyeAngles();
	return true;
}

bool CServerTools::SnapPlayerToPosition( const Vector &org, const QAngle &ang, IClientEntity *pClientPlayer )
{
	IServerEntity *pServerPlayer = GetIServerEntity( pClientPlayer );
	CBasePlayer *pPlayer = pServerPlayer ? ( CBasePlayer* )pServerPlayer : UTIL_GetLocalPlayer();
	if ( pPlayer == nullptr )
		return false;

	pPlayer->SetAbsOrigin( org - pPlayer->GetViewOffset() );
	pPlayer->SnapEyeAngles( ang );

	// Disengage from hierarchy
	pPlayer->SetParent( nullptr );

	return true;
}

int CServerTools::GetPlayerFOV( IClientEntity *pClientPlayer )
{
	IServerEntity *pServerPlayer = GetIServerEntity( pClientPlayer );
	CBasePlayer *pPlayer = pServerPlayer ? ( CBasePlayer* )pServerPlayer : UTIL_GetLocalPlayer();
	if ( pPlayer == nullptr )
		return 0;

	return pPlayer->GetFOV();
}

bool CServerTools::SetPlayerFOV( int fov, IClientEntity *pClientPlayer )
{
	IServerEntity *pServerPlayer = GetIServerEntity( pClientPlayer );
	CBasePlayer *pPlayer = pServerPlayer ? ( CBasePlayer* )pServerPlayer : UTIL_GetLocalPlayer();
	if ( pPlayer == nullptr )
		return false;

	pPlayer->SetDefaultFOV( fov );
	CBaseEntity *pFOVOwner = pPlayer->GetFOVOwner();
	return pPlayer->SetFOV( pFOVOwner ? pFOVOwner : pPlayer, fov );
}

bool CServerTools::IsInNoClipMode( IClientEntity *pClientPlayer )
{
	IServerEntity *pServerPlayer = GetIServerEntity( pClientPlayer );
	CBasePlayer *pPlayer = pServerPlayer ? ( CBasePlayer* )pServerPlayer : UTIL_GetLocalPlayer();
	if ( pPlayer == nullptr )
		return true;

	return pPlayer->GetMoveType() == MOVETYPE_NOCLIP;
}

void *CServerTools::FirstEntity( void )
{
	return (void *)gEntList.FirstEnt();
}

void *CServerTools::NextEntity( void *pEntity )
{
	CBaseEntity *pEnt;

	if ( pEntity == nullptr )
	{
		pEnt = gEntList.FirstEnt();
	}
	else
	{
		pEnt = gEntList.NextEnt( (CBaseEntity *)pEntity );
	}
	return (void *)pEnt;
}

void *CServerTools::FindEntityByHammerID( int iHammerID )
{
	CBaseEntity *pEntity = gEntList.FirstEnt();

	while (pEntity)
	{
		if (pEntity->m_iHammerID == iHammerID)
			return (void *)pEntity;
		pEntity = gEntList.NextEnt( pEntity );
	}
	return nullptr;
}

bool CServerTools::GetKeyValue( void *pEntity, const char *szField, char *szValue, int iMaxLen )
{
	CBaseEntity *pEnt = (CBaseEntity *)pEntity;

	return pEnt->GetKeyValue( szField, szValue, iMaxLen );
}

bool CServerTools::SetKeyValue( void *pEntity, const char *szField, const char *szValue )
{
	CBaseEntity *pEnt = (CBaseEntity *)pEntity;

	return pEnt->KeyValue( szField, szValue );
}

bool CServerTools::SetKeyValue( void *pEntity, const char *szField, float flValue )
{
	CBaseEntity *pEnt = (CBaseEntity *)pEntity;

	return pEnt->KeyValue( szField, flValue );
}

bool CServerTools::SetKeyValue( void *pEntity, const char *szField, const Vector &vecValue )
{
	CBaseEntity *pEnt = (CBaseEntity *)pEntity;

	return pEnt->KeyValue( szField, vecValue );
}


//-----------------------------------------------------------------------------
// entity spawning
//-----------------------------------------------------------------------------
void *CServerTools::CreateEntityByName( const char *szClassName )
{
	return ::CreateEntityByName( szClassName );
}

void CServerTools::DispatchSpawn( void *pEntity )
{
	::DispatchSpawn( (CBaseEntity *)pEntity );
}

bool CServerTools::DestroyEntityByHammerId( int iHammerID )
{
	CBaseEntity *pEntity = (CBaseEntity*)FindEntityByHammerID( iHammerID );
	if ( !pEntity )
		return false;

	UTIL_Remove( pEntity );
	return true;
}


void HandleFoundryEntitySpawnRecords()
{
	if ( g_FoundryEntitySpawnRecords.empty() )
		return;

	CVProfScope _( "HandleFoundryEntitySpawnRecords", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Create all the entities.
	std::vector< CBaseEntity* > newEnts;
	
	CMapEntitySpawner spawner;
	spawner.m_bFoundryMode = true;

	for ( CFoundryEntitySpawnRecord &record : g_FoundryEntitySpawnRecords )
	{
		CFoundryEntitySpawnRecord *pRecord = &record;

		if ( pRecord->m_iEntityIndex > 0 )
		{
			gEntList.ForceEntSerialNumber( pRecord->m_iEntityIndex, pRecord->m_iSerialNumber );
			engine->ForceFlushEntity( pRecord->m_iEntityIndex );
		}

		// Figure out the class name.
		CEntityMapData entData( pRecord->m_VMFText.data() );
		char szClassName[MAPKEY_MAXLENGTH];
		if ( !entData.ExtractValue( "classname", szClassName ) )
		{
			contract_assert( false );
			continue;
		}

		// Respawn it in the same slot.
		int nIndexToSpawn = pRecord->m_iEntityIndex;
		if ( nIndexToSpawn == 0 )
			nIndexToSpawn = -1;

		CBaseEntity *pNewEntity = ::CreateEntityByName( szClassName, nIndexToSpawn );
		if ( !pNewEntity )
		{
			Warning( "HandleFoundryEntitySpawnRecords - CreateEntityByName( %s, %d ) failed\n", szClassName, pRecord->m_iEntityIndex );
			continue;
		}

		const char *pBaseMapDataForThisEntity = entData.CurrentBufferPosition();
		pNewEntity->ParseMapData( &entData );

		if ( pRecord->m_debugOverlays != -1 )
			pNewEntity->m_debugOverlays = pRecord->m_debugOverlays;

		pNewEntity->m_iHammerID = pRecord->m_iHammerID;

		spawner.AddEntity( pNewEntity, pBaseMapDataForThisEntity, (entData.CurrentBufferPosition() - pBaseMapDataForThisEntity) + 2 );
	}

	spawner.HandleTemplates();
	spawner.SpawnAndActivate( true );

	// Now that all of the active entities have been loaded in, precache any entities who need point_template parameters
	//  to be parsed (the above code has loaded all point_template entities)
	PrecachePointTemplates();

	// Sometimes an ent will Remove() itself during its precache, so RemoveImmediate won't happen.
	// This makes sure those ents get cleaned up.
	gEntList.CleanupDeleteList();

	g_FoundryEntitySpawnRecords = {};
}


bool CServerTools::RespawnEntitiesWithEdits( CEntityRespawnInfo *pInfos, int nInfos )
{
	// Create a spawn record so it'll respawn the entity next frame.
	for ( int i=0; i < nInfos; i++ )
	{
		CFoundryEntitySpawnRecord *pRecord = &g_FoundryEntitySpawnRecords.emplace_back();
		CEntityRespawnInfo *pInfo = &pInfos[i];
		
		pRecord->m_VMFText.assign( V_strlen( pInfo->m_pEntText ) + 1, {} );
		V_strncpy( pRecord->m_VMFText.data(), pInfo->m_pEntText, std::ssize( pRecord->m_VMFText ) );
		pRecord->m_iHammerID = pInfo->m_nHammerID;

		CBaseEntity *pOldEntity = (CBaseEntity*)FindEntityByHammerID( pInfo->m_nHammerID );
		if ( pOldEntity )
		{
			// This is a respawn.
			pRecord->m_iEntityIndex = pOldEntity->entindex();
			pRecord->m_iSerialNumber = pOldEntity->GetRefEHandle().GetSerialNumber();
			pRecord->m_debugOverlays = pOldEntity->m_debugOverlays;
			UTIL_Remove( pOldEntity );
		}
		else
		{
			// This is a new spawn.
			pRecord->m_iEntityIndex = -1;
			pRecord->m_iSerialNumber = -1;
			pRecord->m_debugOverlays = -1;
		}
	}
	
return true;
}

//-----------------------------------------------------------------------------
// Reload particle definitions
//-----------------------------------------------------------------------------
void CServerTools::ReloadParticleDefintions( const char *pFileName, const void *pBufData, int nLen )
{
	// FIXME: Use file name to determine if we care about this data
	ks::byte_reader buf( pBufData, nLen );
	g_pParticleSystemMgr->ReadParticleConfigFile( buf, true );
}

void CServerTools::AddOriginToPVS( const Vector &org )
{
	engine->AddOriginToPVS( org );
}


void CServerTools::MoveEngineViewTo( const Vector &vPos, const QAngle &vAngles )
{
	CBasePlayer *pPlayer = UTIL_GetListenServerHost();
	if ( !pPlayer )
		return;

	extern void EnableNoClip( CBasePlayer *pPlayer );
	EnableNoClip( pPlayer );

	Vector zOffset = pPlayer->EyePosition() - pPlayer->GetAbsOrigin();

	pPlayer->SetAbsOrigin( vPos - zOffset );
	pPlayer->SnapEyeAngles( vAngles );
}


void CServerTools::RemoveEntity( int nHammerID )
{
	CBaseEntity *pOldEntity = (CBaseEntity*)FindEntityByHammerID( nHammerID );
	if ( pOldEntity )
		UTIL_Remove( pOldEntity );
}


//------------------------------------------------------------------------------
// Applies keyvalues to the entity by hammer ID.
//------------------------------------------------------------------------------
void CC_Ent_Keyvalue( const CCommand &args )
{
	// Must have an odd number of arguments.
	if ( ( args.ArgC() < 4 ) || ( args.ArgC() & 1 ) )
	{
		Msg( "Format: ent_keyvalue <entity id> \"key1\"=\"value1\" \"key2\" \"value2\" ... \"keyN\" \"valueN\"\n" );
		return;
	}

	int nID = std::atoi( args[1] );

	void *pEnt = g_ServerTools.FindEntityByHammerID( nID );
	if ( !pEnt )
	{
		Msg( "Entity ID %d not found.\n", nID );
		return;
	}

	int nArg = 2;
	while ( nArg < args.ArgC() )
	{
		const char *pszKey = args[ nArg ];
		const char *pszValue = args[ nArg + 1 ];
		nArg += 2;

		g_ServerTools.SetKeyValue( pEnt, pszKey, pszValue );
	}
} 

static ConCommand ent_keyvalue("ent_keyvalue", CC_Ent_Keyvalue, "Applies the comma delimited key=value pairs to the entity with the given Hammer ID.\n\tFormat: ent_keyvalue <entity id> <key1>=<value1>,<key2>=<value2>,...,<keyN>=<valueN>\n", FCVAR_CHEAT);
