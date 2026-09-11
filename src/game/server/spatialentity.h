//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Spatial entity.
//
// $NoKeywords: $
//===========================================================================//

#ifndef SPATIALENTITY_H
#define SPATIALENTITY_H

#include "reflect_annotations.h"
#include "dt_common.h"

#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------
// Purpose : Spatial entity
//------------------------------------------------------------------------------
void SendProxy_Origin( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class [[= ks::reflect::NetTable{ .name = "DT_SpatialEntity", .base = false } ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = HIGH_DEFAULT, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR }, SendProxy_Origin>{} ]]
      CSpatialEntity : public CBaseEntity
{
	DECLARE_CLASS( CSpatialEntity, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CSpatialEntity();

	void Spawn( void );
	int  UpdateTransmitState();

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	// Inputs
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFadeInDuration", .type = FIELD_FLOAT } ]] void	InputSetFadeInDuration ( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFadeOutDuration", .type = FIELD_FLOAT } ]] void	InputSetFadeOutDuration ( inputdata_t &inputdata );

private:
	void	FadeIn ( void );
	void	FadeOut ( void );

	void FadeInThink( void );	// Fades lookup weight from Cur->MaxWeight 
	void FadeOutThink( void );	// Fades lookup weight from CurWeight->0.0



	[[= ks::reflect::Key{ .name = "fadeInDuration" } ]] float	m_flFadeInDuration;		// Duration for a full 0->MaxWeight transition
	[[= ks::reflect::Key{ .name = "fadeOutDuration" } ]] float	m_flFadeOutDuration;	// Duration for a full Max->0 transition
	float	m_flStartFadeInWeight;
	float	m_flStartFadeOutWeight;
	float	m_flTimeStartFadeIn;
	float	m_flTimeStartFadeOut;

	[[= ks::reflect::Key{ .name = "maxweight" } ]] float	m_flMaxWeight;

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bStartDisabled;
	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );

	CNetworkVar( float, m_MinFalloff, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "minfalloff" } ]] );
	CNetworkVar( float, m_MaxFalloff, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "maxfalloff" } ]] );
	CNetworkVar( float, m_flCurWeight, [[= ks::reflect::Net{ .bits = 32 } ]] );

	[[= ks::reflect::Key{ .name = "filename" } ]] string_t	m_lookupFilename;
};

#endif // SPATIALENTITY_H
