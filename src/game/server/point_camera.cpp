// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
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
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "point_camera.h"

constexpr auto CAM_THINK_INTERVAL = 0.05;

// Spawnflags
constexpr auto SF_CAMERA_START_OFF = 0x01;

// UNDONE: Share properly with the client code!!!
constexpr auto POINT_CAMERA_MSG_SETACTIVE = 1;

CEntityClassList<CPointCamera> g_PointCameraList;
template <> CPointCamera *CEntityClassList<CPointCamera>::m_pClassList = nullptr;

CPointCamera* GetPointCameraList()
{
	return g_PointCameraList.m_pClassList;
}

// These are already built into CBaseEntity
//	DEFINE_KEYFIELD( m_iName, FIELD_STRING, "targetname" ),
//	DEFINE_KEYFIELD( m_iParent, FIELD_STRING, "parentname" ),
//	DEFINE_KEYFIELD( m_target, FIELD_STRING, "target" ),

LINK_ENTITY_TO_CLASS( point_camera, CPointCamera );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPointCamera::~CPointCamera()
{
	g_PointCameraList.Remove( this );
}

CPointCamera::CPointCamera()
{
	// Set these to opposites so that it'll be sent the first time around.
	m_bActive = false;
	m_bIsOn = false;
	
	m_bFogEnable = false;

	g_PointCameraList.Insert( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	if ( m_spawnflags & SF_CAMERA_START_OFF )
	{
		m_bIsOn = false;
	}
	else
	{
		m_bIsOn = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override ShouldTransmit since we want to be sent even though we don't have a model, etc.
//			All that matters is if we are in the pvs.
//-----------------------------------------------------------------------------
int CPointCamera::UpdateTransmitState_Impl()
{
	if ( m_bActive )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
	else
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::SetActive( bool bActive )
{
	// If the mapmaker's told the camera it's off, it enforces inactive state
	if ( !m_bIsOn )
	{
		bActive = false;
	}

	if ( m_bActive != bActive )
	{
		m_bActive = bActive;
		DispatchUpdateTransmitState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputChangeFOV( inputdata_t &inputdata )
{
	// Parse the keyvalue data
	char parseString[255];

	V_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// Get FOV
	char *pszParam = std::strtok(parseString," ");
	if(pszParam)
	{
		m_TargetFOV = std::atof( pszParam );
	}
	else
	{
		// Assume no change
		m_TargetFOV = m_FOV;
	}

	// Get Time
	float flChangeTime;
	pszParam = strtok(nullptr," ");
	if(pszParam)
	{
		flChangeTime = std::atof( pszParam );
	}
	else
	{
		// Assume 1 second.
		flChangeTime = 1.0;
	}

	m_DegreesPerSecond = ( m_TargetFOV - m_FOV ) / flChangeTime;

	SetThink( &CPointCamera::ChangeFOVThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::ChangeFOVThink( void )
{
	SetNextThink( gpGlobals->curtime + CAM_THINK_INTERVAL );

	float newFOV = m_FOV;

	newFOV += m_DegreesPerSecond * CAM_THINK_INTERVAL;

	if( m_DegreesPerSecond < 0 )
	{
		if( newFOV <= m_TargetFOV )
		{
			newFOV = m_TargetFOV;
			SetThink( nullptr );
		}
	}
	else
	{
		if( newFOV >= m_TargetFOV )
		{
			newFOV = m_TargetFOV;
			SetThink( nullptr );
		}
	}

	m_FOV = newFOV;
}

//-----------------------------------------------------------------------------
// Purpose: Turn this camera on, and turn all other cameras off
//-----------------------------------------------------------------------------
void CPointCamera::InputSetOnAndTurnOthersOff( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = nullptr;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "point_camera" )) != nullptr)
	{
		CPointCamera *pCamera = (CPointCamera*)pEntity;
		pCamera->InputSetOff( inputdata );
	}

	// Now turn myself on
	InputSetOn( inputdata );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputSetOn( inputdata_t &inputdata )
{
	m_bIsOn = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputSetOff( inputdata_t &inputdata )
{
	m_bIsOn = false;
	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputForceActive( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = nullptr;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "point_camera" )) != nullptr)
	{
		CPointCamera *pCamera = (CPointCamera*)pEntity;
		pCamera->m_bActive = false;
	}

	// Now turn myself on
	InputSetOn( inputdata );

	m_bActive = true;

	UpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputForceInactive( inputdata_t &inputdata )
{
	m_bIsOn = false;
	SetActive( false );
	UpdateTransmitState();
}

IMPLEMENT_REFLECT_DATAMAP( CPointCamera )

IMPLEMENT_REFLECT_SERVERCLASS( CPointCamera, DT_PointCamera )
