//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ENVPROJECTED_TEXTURE_H
#define C_ENVPROJECTED_TEXTURE_H

#include "reflect_annotations.h"
#include "dt_recv.h"
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"
#include "basetypes.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvProjectedTexture" } ]]
      C_EnvProjectedTexture : public C_BaseEntity
{
	DECLARE_CLASS( C_EnvProjectedTexture, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	void SetMaterial( IMaterial *pMaterial );
	void SetLightColor( byte r, byte g, byte b, byte a );
	void SetSize( float flSize );
	void SetRotation( float flRotation );

	virtual void OnDataChanged( DataUpdateType_t updateType );
	void	ShutDownLightHandle( void );

	virtual bool Simulate();

	bool	ShouldUpdate( void );
	void	UpdateLight( void );

	C_EnvProjectedTexture();
	~C_EnvProjectedTexture();

	static void SetVisibleBBoxMinHeight( float flVisibleBBoxMinHeight ) { m_flVisibleBBoxMinHeight = flVisibleBBoxMinHeight; }
	static float GetVisibleBBoxMinHeight( void ) { return m_flVisibleBBoxMinHeight; }
	static C_EnvProjectedTexture *Create( );

private:

	inline bool IsBBoxVisible( void );
	bool IsBBoxVisible( Vector vecExtentsMin,
						Vector vecExtentsMax );

	ClientShadowHandle_t m_LightHandle;
	bool m_bForceUpdate;

	[[= ks::reflect::Net{} ]] EHANDLE	m_hTargetEntity;

	[[= ks::reflect::Net{} ]] bool		m_bState;
	[[= ks::reflect::Net{} ]] bool		m_bAlwaysUpdate;
	[[= ks::reflect::Net{} ]] float		m_flLightFOV;
	[[= ks::reflect::Net{} ]] bool		m_bEnableShadows;
	[[= ks::reflect::Net{} ]] bool		m_bSimpleProjection;
	[[= ks::reflect::Net{} ]] bool		m_bLightOnlyTarget;
	[[= ks::reflect::Net{} ]] bool		m_bLightWorld;
	[[= ks::reflect::Net{} ]] bool		m_bCameraSpace;
	[[= ks::reflect::Net{} ]] float		m_flBrightnessScale;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WIRE_RECV>{} ]] color32		m_LightColor;
	Vector		m_CurrentLinearFloatLightColor;
	float		m_flCurrentLinearFloatLightAlpha;
	[[= ks::reflect::Net{} ]] float		m_flColorTransitionTime;
	[[= ks::reflect::Net{} ]] float		m_flAmbient;
	[[= ks::reflect::Net{} ]] float		m_flNearZ;
	[[= ks::reflect::Net{} ]] float		m_flFarZ;
	[[= ks::reflect::Net{} ]] char		m_SpotlightTextureName[ MAX_PATH ];
	CTextureReference m_SpotlightTexture;
	CMaterialReference m_ProjectedMaterial;
	[[= ks::reflect::Net{} ]] int			m_nSpotlightTextureFrame;
	[[= ks::reflect::Net{} ]] int			m_nShadowQuality;
	[[= ks::reflect::Net{} ]] int			m_iStyle;
	bool		m_bIsCurrentlyProjected;

	// simple projection
	IMaterial	*m_pMaterial;
	[[= ks::reflect::Net{} ]] float		m_flProjectionSize;
	[[= ks::reflect::Net{} ]] float		m_flRotation;

	Vector	m_vecExtentsMin;
	Vector	m_vecExtentsMax;

	static float m_flVisibleBBoxMinHeight;
};



bool C_EnvProjectedTexture::IsBBoxVisible( void )
{
	return IsBBoxVisible( GetAbsOrigin() + m_vecExtentsMin, GetAbsOrigin() + m_vecExtentsMax );
}

#endif // C_ENV_PROJECTED_TEXTURE_H
