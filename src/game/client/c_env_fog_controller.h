//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ENV_FOG_CONTROLLER_H
#define C_ENV_FOG_CONTROLLER_H

#define CFogController C_FogController

//=============================================================================
//
// Class Fog Controller:
// Compares a set of integer inputs to the one main input
// Outputs true if they are all equivalant, false otherwise
//
class [[= ks::reflect::NetTable{ .name = "DT_FogController", .base = false } ]]
      [[= ks::reflect::From<"m_fog.enable", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.blend", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.dirPrimary", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.colorPrimary", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
      [[= ks::reflect::From<"m_fog.colorSecondary", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
      [[= ks::reflect::From<"m_fog.start", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.end", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.farz", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.maxdensity", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.colorPrimaryLerpTo", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
      [[= ks::reflect::From<"m_fog.colorSecondaryLerpTo", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
      [[= ks::reflect::From<"m_fog.startLerpTo", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.endLerpTo", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.maxdensityLerpTo", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.lerptime", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.duration", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.HDRColorScale", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_fog.ZoomFogScale", ks::reflect::Net{}>{} ]]
      C_FogController : public C_BaseEntity
{
public:
	DECLARE_NETWORKCLASS();
	DECLARE_CLASS( C_FogController, C_BaseEntity );

	C_FogController();

public:

	fogparams_t				m_fog;
};


#endif // C_ENV_FOG_CONTROLLER_H
