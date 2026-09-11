//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_POINTCAMERA_H
#define C_POINTCAMERA_H

#include "reflect_annotations.h"
#include "dt_recv.h"
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"
#include "basetypes.h"

class [[= ks::reflect::NetTable{ .name = "DT_PointCamera" } ]]
      C_PointCamera : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PointCamera, C_BaseEntity );
	DECLARE_CLIENTCLASS();

public:
	C_PointCamera();
	~C_PointCamera();

	bool IsActive();
	
	// C_BaseEntity.
	virtual bool	ShouldDraw();

	float			GetFOV();
	float			GetResolution();
	bool			IsFogEnabled();
	void			GetFogColor( unsigned char &r, unsigned char &g, unsigned char &b );
	float			GetFogStart();
	float			GetFogMaxDensity();
	float			GetFogEnd();
	bool			UseScreenAspectRatio() const { return m_bUseScreenAspectRatio; }

	virtual void	GetToolRecordingState( KeyValues *msg );

protected:
	[[= ks::reflect::Net{} ]] float m_FOV;
	[[= ks::reflect::Net{} ]] float m_Resolution;
	[[= ks::reflect::Net{} ]] bool m_bFogEnable;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WIRE_RECV>{} ]] color32 m_FogColor;
	[[= ks::reflect::Net{} ]] float m_flFogStart;
	[[= ks::reflect::Net{} ]] float m_flFogEnd;
	[[= ks::reflect::Net{} ]] float m_flFogMaxDensity;
	[[= ks::reflect::Net{} ]] bool m_bActive;
	[[= ks::reflect::Net{} ]] bool m_bUseScreenAspectRatio;

public:
	C_PointCamera	*m_pNext;
};

C_PointCamera *GetPointCameraList();

#endif // C_POINTCAMERA_H
