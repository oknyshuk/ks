// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A camera entity that's used by the -makedevshots system to take
//			dev screenshots everytime the map is checked into source control.

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
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "igamesystem.h"
#include "filesystem.h"

int g_iDevShotCameraCount = 0;
constexpr auto DEVSHOT_INITIAL_WAIT = 5; // Time after the level spawn before the first devshot camera takes it's shot
constexpr auto DEVSHOT_INTERVAL = 5; // Time between each devshot camera taking it's shot

//-----------------------------------------------------------------------------
// Purpose: A camera entity that's used by the -makedevshots system to take
//			dev screenshots everytime the map is checked into source control.
//-----------------------------------------------------------------------------
class CPointDevShotCamera : public CBaseEntity
{
	DECLARE_CLASS( CPointDevShotCamera, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void DevShotThink_Setup( void );
	void DevShotThink_TakeShot( void );
	void DevShotThink_PostShot( void );

	// Always transmit to clients so they know where to move the view to
	int UpdateTransmitState_Impl();

private:
	[[= ks::reflect::Key{ .name = "cameraname" } ]] string_t	m_iszCameraName;
	[[= ks::reflect::Key{ .name = "FOV" } ]] int			m_iFOV;
};

IMPLEMENT_REFLECT_DATAMAP( CPointDevShotCamera )

LINK_ENTITY_TO_CLASS( point_devshot_camera, CPointDevShotCamera );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	// Remove this entity immediately if we're not making devshots
	if ( !CommandLine()->FindParm("-makedevshots") )
	{
		UTIL_Remove( this );
		return;
	}

	// Take a screenshot when it's my turn
	SetThink( &CPointDevShotCamera::DevShotThink_Setup );
	SetNextThink( gpGlobals->curtime + DEVSHOT_INITIAL_WAIT + (g_iDevShotCameraCount * DEVSHOT_INTERVAL) );

	g_iDevShotCameraCount++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPointDevShotCamera::UpdateTransmitState_Impl()
{
	// always transmit if currently used by a monitor
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::DevShotThink_Setup( void )
{
	// Move the player to the devshot camera
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
		return;

	// Hide stuff
	engine->ClientCommand( pPlayer->edict(), "developer 0" );
	engine->ClientCommand( pPlayer->edict(), "cl_drawhud 0" );
	engine->ClientCommand( pPlayer->edict(), "sv_cheats 1" );
	engine->ClientCommand( pPlayer->edict(), "god" );
	engine->ClientCommand( pPlayer->edict(), "notarget" );

	pPlayer->AddSolidFlags( FSOLID_NOT_SOLID );
	pPlayer->EnableControl(false);
	pPlayer->SetViewEntity( this );
	pPlayer->SetFOV( this, m_iFOV );

	// Hide the player's viewmodel
	if ( pPlayer->GetActiveWeapon() )
	{
		pPlayer->GetActiveWeapon()->AddEffects( EF_NODRAW );
	}

	DispatchUpdateTransmitState();

	// Now take the shot next frame
	SetThink( &CPointDevShotCamera::DevShotThink_TakeShot );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::DevShotThink_TakeShot( void )
{
	// Take the screenshot
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
		return;

	engine->ClientCommand( pPlayer->edict(), "devshots_screenshot \"%s\"", STRING(m_iszCameraName) );

	// Now take the shot next frame
	SetThink( &CPointDevShotCamera::DevShotThink_PostShot );
	SetNextThink( gpGlobals->curtime + (DEVSHOT_INTERVAL - 1) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::DevShotThink_PostShot( void )
{
	// Take the screenshot
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
		return;

	pPlayer->SetFOV( this, 0 );

	// If all cameras have taken their shots, move to the next map
	g_iDevShotCameraCount--;
	if ( !g_iDevShotCameraCount )
	{
		engine->ClientCommand( pPlayer->edict(), "devshots_nextmap" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Game system to detect maps without cameras in them, and move on
//-----------------------------------------------------------------------------
class CDevShotSystem : public CAutoGameSystemPerFrame
{
public:

	CDevShotSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	virtual void LevelInitPreEntity()
	{
		m_bIssuedNextMapCommand = false;
		g_iDevShotCameraCount = 0;
		m_bParsedMapFile = false;
	}

	virtual void SafeRemoveIfDesired( void )
	{
		// If we're not making devshots, remove this system immediately
		if ( !CommandLine()->FindParm("-makedevshots") )
		{
			Remove( this );
			return;
		}
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		// Wait until we're all spawned in
		if ( gpGlobals->curtime < 5 )
			return;

		if ( m_bIssuedNextMapCommand )
			return;

		if ( !m_bParsedMapFile )
		{
			m_bParsedMapFile = true;

			// See if we've got a camera file to import cameras from
			char szFullName[512];
			V_snprintf(szFullName,sizeof(szFullName), "maps/%s.txt", STRING( gpGlobals->mapname ));
			KeyValues *pkvMapCameras = new KeyValues( "MapCameras" );
			if ( pkvMapCameras->LoadFromFile( filesystem, szFullName, "MOD" ) )
			{
				Warning( "Devshots: Loading point_devshot_camera positions from %s. \n", szFullName );

				// Get each camera, and add it to our list
				KeyValues *pkvCamera = pkvMapCameras->GetFirstSubKey();
				while ( pkvCamera )
				{
					// Get camera name
					const char *pCameraName = pkvCamera->GetName();

					// Make a camera, and move it to the position specified
					CPointDevShotCamera	*pCamera = (CPointDevShotCamera*)CreateEntityByName( "point_devshot_camera" );
					contract_assert( pCamera );
					pCamera->KeyValue( "cameraname", pCameraName );
					pCamera->KeyValue( "origin", pkvCamera->GetString( "origin", "0 0 0" ) );
					pCamera->KeyValue( "angles", pkvCamera->GetString( "angles", "0 0 0" ) );
					pCamera->KeyValue( "FOV", pkvCamera->GetString( "FOV", "75" ) );
					DispatchSpawn( pCamera );
					pCamera->Activate();

					// Move to next camera
					pkvCamera = pkvCamera->GetNextKey();
				}
			}

			if ( !g_iDevShotCameraCount )
			{
				Warning( "Devshots: No point_devshot_camera in %s. Moving to next map.\n", STRING( gpGlobals->mapname ) );

				CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
				if ( pPlayer )
				{
					engine->ClientCommand( pPlayer->edict(), "devshots_nextmap" );
					m_bIssuedNextMapCommand = true;
					return;
				}
			}
		}
	}

private:
	bool	m_bIssuedNextMapCommand;
	bool	m_bParsedMapFile;
};

CDevShotSystem	DevShotSystem( "CDevShotSystem" );
