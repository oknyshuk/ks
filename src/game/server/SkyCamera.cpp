//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "igamesystem.h"
#include "entitylist.h"
#include "SkyCamera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// automatically hooks in the system's callbacks
CEntityClassList<CSkyCamera> g_SkyList;
template <> CSkyCamera *CEntityClassList<CSkyCamera>::m_pClassList = NULL;

CHandle<CSkyCamera> g_hActiveSkybox = INVALID_EHANDLE;

#ifdef PORTAL2

//------------------------------------------------------------------------------
// Purpose: NPC step trough AI
//------------------------------------------------------------------------------
void CC_SkyboxSwap( void )
{
	// Make this cyclical for now!
	if ( g_SkyList.m_pClassList->m_pNext )
	{
		g_SkyList.m_pClassList->m_pNext->m_pNext = g_SkyList.m_pClassList;
	}
	
	g_SkyList.m_pClassList = g_SkyList.m_pClassList->m_pNext;
}
static ConCommand skybox_swap("skybox_swap", CC_SkyboxSwap, "Swap through the skyboxes in our queue", FCVAR_CHEAT );

#endif // PORTAL2

//-----------------------------------------------------------------------------
// Retrives the current skycamera
//-----------------------------------------------------------------------------
CSkyCamera*	GetCurrentSkyCamera()
{
	if (g_hActiveSkybox.Get() == NULL)
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
	NULL
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

#ifdef HL2_DLL
	// NOTE! This is a hack. There was a bug in the skybox fog computation
	// on the client DLL that caused it to use the average of the primary and
	// secondary fog color when blending was enabled. The bug is fixed, but to make
	// the maps look the same as before the bug fix without having to download new maps,
	// I have to cheat here and slam the primary and secondary colors to be the average of 
	// the primary and secondary colors.
	if ( m_skyboxData.fog.blend )
	{
		for ( int i = 0; s_pBogusFogMaps[i]; ++i )
		{
			if ( !Q_stricmp( s_pBogusFogMaps[i], STRING(gpGlobals->mapname) ) )
			{
				m_skyboxData.fog.colorPrimary.SetR( ( m_skyboxData.fog.colorPrimary.GetR() + m_skyboxData.fog.colorSecondary.GetR() ) * 0.5f );
				m_skyboxData.fog.colorPrimary.SetG( ( m_skyboxData.fog.colorPrimary.GetG() + m_skyboxData.fog.colorSecondary.GetG() ) * 0.5f );
				m_skyboxData.fog.colorPrimary.SetB( ( m_skyboxData.fog.colorPrimary.GetB() + m_skyboxData.fog.colorSecondary.GetB() ) * 0.5f );
				m_skyboxData.fog.colorPrimary.SetA( ( m_skyboxData.fog.colorPrimary.GetA() + m_skyboxData.fog.colorSecondary.GetA() ) * 0.5f );
				m_skyboxData.fog.colorSecondary = m_skyboxData.fog.colorPrimary;
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CSkyCamera::InputActivateSkybox( inputdata_t &inputdata )
{
	g_hActiveSkybox = this;
}
