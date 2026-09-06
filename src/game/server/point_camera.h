//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CAMERA_H
#define CAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "reflect_annotations.h"
#include "sendproxy.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_PointCamera" } ]] CPointCamera : public CBaseEntity
{
public:
	DECLARE_CLASS( CPointCamera, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CPointCamera();
	~CPointCamera();

	void Spawn( void );

	// Tell the client that this camera needs to be rendered
	void SetActive( bool bActive );
	bool IsActive( void ) { return m_bActive; }
	int  UpdateTransmitState(void);

	void ChangeFOVThink( void );
	float GetFOV() { return m_FOV; }

	[[= ks::reflect::Input{ .name = "ChangeFOV", .type = FIELD_STRING } ]] void InputChangeFOV( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetOnAndTurnOthersOff", .type = FIELD_VOID } ]] void InputSetOnAndTurnOthersOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetOn", .type = FIELD_VOID } ]] void InputSetOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetOff", .type = FIELD_VOID } ]] void InputSetOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputForceActive( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputForceInactive( inputdata_t &inputdata );

private:
	float m_TargetFOV;
	float m_DegreesPerSecond;

	CNetworkVar( float, m_FOV, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FOV" } ]] );
	CNetworkVar( float, m_Resolution, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "resolution" } ]] );
	CNetworkVar( bool, m_bFogEnable, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "fogEnable" } ]] );
	CNetworkColor32( m_FogColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]]
	                             [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] [[= ks::reflect::Key{ .name = "fogColor" } ]] );
	CNetworkVar( float, m_flFogStart, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fogStart" } ]] );
	CNetworkVar( float, m_flFogEnd, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fogEnd" } ]] );
	CNetworkVar( float, m_flFogMaxDensity, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fogMaxDensity" } ]] );
	CNetworkVar( bool, m_bActive, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bUseScreenAspectRatio, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "UseScreenAspectRatio" } ]] );

	// Allows the mapmaker to control whether a camera is active or not
	bool	m_bIsOn;

public:
	CPointCamera	*m_pNext;
};

CPointCamera *GetPointCameraList();
#endif // CAMERA_H
