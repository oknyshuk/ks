//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Resource collection entity
//
// $NoKeywords: $
//=============================================================================//

#ifndef SKYCAMERA_H
#define SKYCAMERA_H

#include "reflect_annotations.h"

#ifdef _WIN32
#pragma once
#endif

class CSkyCamera;

//=============================================================================
//
// Sky Camera Class
//
class
      [[= ks::reflect::KeyFrom<"m_skyboxData.scale", ks::reflect::Key{ .name = "scale" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.enable", ks::reflect::Key{ .name = "fogenable" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.blend", ks::reflect::Key{ .name = "fogblend" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.dirPrimary", ks::reflect::Key{ .name = "fogdir" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.colorPrimary", ks::reflect::Key{ .name = "fogcolor" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.colorSecondary", ks::reflect::Key{ .name = "fogcolor2" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.start", ks::reflect::Key{ .name = "fogstart" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.end", ks::reflect::Key{ .name = "fogend" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.maxdensity", ks::reflect::Key{ .name = "fogmaxdensity" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_skyboxData.fog.HDRColorScale", ks::reflect::Key{ .name = "HDRColorScale" } >{} ]]
      CSkyCamera : public CLogicalEntity
{
	DECLARE_CLASS( CSkyCamera, CLogicalEntity );

public:

	DECLARE_DATADESC();
	CSkyCamera();
	~CSkyCamera();
	virtual void Spawn( void );
	virtual void Activate();

	[[= ks::reflect::Input{ .name = "ActivateSkybox", .type = FIELD_VOID } ]] void InputActivateSkybox( inputdata_t &inputdata );

public:
	sky3dparams_t	m_skyboxData;
	[[= ks::reflect::Key{ .name = "use_angles" } ]] bool			m_bUseAngles;
	CSkyCamera		*m_pNext;
};


//-----------------------------------------------------------------------------
// Retrives the current skycamera
//-----------------------------------------------------------------------------
CSkyCamera*		GetCurrentSkyCamera();
CSkyCamera*		GetSkyCameraList();


#endif // SKYCAMERA_H
