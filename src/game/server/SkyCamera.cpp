// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "edict.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "reflect_datamap.h"
#include "entitylist.h"
#include "SkyCamera.h"

// automatically hooks in the system's callbacks
CEntityClassList<CSkyCamera> g_SkyList;
template <> CSkyCamera *CEntityClassList<CSkyCamera>::m_pClassList = nullptr;

CHandle<CSkyCamera> g_hActiveSkybox = INVALID_EHANDLE;


//-----------------------------------------------------------------------------
// Retrives the current skycamera
//-----------------------------------------------------------------------------
CSkyCamera*	GetCurrentSkyCamera()
{
	if (g_hActiveSkybox.Get() == nullptr)
	{
		g_hActiveSkybox = GetSkyCameraList();
	}
	return g_hActiveSkybox.Get();
}

CSkyCamera*	GetSkyCameraList()
{
	return g_SkyList.m_pClassList;
}

//=============================================================================

LINK_ENTITY_TO_CLASS( sky_camera, CSkyCamera );

IMPLEMENT_REFLECT_DATAMAP( CSkyCamera )


//-----------------------------------------------------------------------------
// List of maps in HL2 that we must apply our skybox fog fixup hack to
//-----------------------------------------------------------------------------
static const char *s_pBogusFogMaps[] =
{
	"d1_canals_01",
	"d1_canals_01a",
	"d1_canals_02",
	"d1_canals_03",
	"d1_canals_09",
	"d1_canals_10",
	"d1_canals_11",
	"d1_canals_12",
	"d1_canals_13",
	"d1_eli_01",
	"d1_trainstation_01",
	"d1_trainstation_03",
	"d1_trainstation_04",
	"d1_trainstation_05",
	"d1_trainstation_06",
	"d3_c17_04",
	"d3_c17_11",
	"d3_c17_12",
	"d3_citadel_01",
	nullptr
};

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CSkyCamera::CSkyCamera()
{
	g_SkyList.Insert( this );
	m_skyboxData.fog.maxdensity = 1.0f;
	m_skyboxData.fog.HDRColorScale = 1.0f;
}

CSkyCamera::~CSkyCamera()
{
	g_SkyList.Remove( this );
}

void CSkyCamera::Spawn( void ) 
{ 
	m_skyboxData.origin = GetLocalOrigin();
	m_skyboxData.area = engine->GetArea( m_skyboxData.origin );
	
	Precache();
}


//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CSkyCamera::Activate( ) 
{
	BaseClass::Activate();

	if ( m_bUseAngles )
	{
		AngleVectors( GetAbsAngles(), &m_skyboxData.fog.dirPrimary.GetForModify() );
		m_skyboxData.fog.dirPrimary.GetForModify() *= -1.0f; 
	}

}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CSkyCamera::InputActivateSkybox( inputdata_t &inputdata )
{
	g_hActiveSkybox = this;
}
