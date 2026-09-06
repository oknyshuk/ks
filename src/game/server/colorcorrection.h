//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Color correction entity.
//
// $NoKeywords: $
//===========================================================================//

#ifndef COLOR_CORRECTION_H
#define COLOR_CORRECTION_H

#include "reflect_annotations.h"
#include "dt_common.h"
#ifdef _WIN32
#pragma once
#endif

#include <string.h>
#include "cbase.h"
#include "GameEventListener.h"

// Spawn Flags
#define SF_COLORCORRECTION_MASTER		0x0001
#define SF_COLORCORRECTION_CLIENTSIDE	0x0002

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
void SendProxy_Origin( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class [[= ks::reflect::NetTable{ .name = "DT_ColorCorrection", .base = false } ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = HIGH_DEFAULT, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR }, SendProxy_Origin>{} ]]
      CColorCorrection : public CBaseEntity
{
	DECLARE_CLASS( CColorCorrection, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CColorCorrection();

	void Spawn( void );
	int  UpdateTransmitState();
	void Activate( void );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	bool IsMaster( void ) const { return HasSpawnFlags( SF_COLORCORRECTION_MASTER ); }

	bool IsClientSide( void ) const { return HasSpawnFlags( SF_COLORCORRECTION_CLIENTSIDE ); }

	bool IsExclusive( void ) const { return m_bExclusive; }

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

	
	
	CNetworkVar( float, m_flFadeInDuration, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "fadeInDuration" } ]] );	// Duration for a full 0->MaxWeight transition
	CNetworkVar( float, m_flFadeOutDuration, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "fadeOutDuration" } ]] );	// Duration for a full Max->0 transition
	float	m_flStartFadeInWeight;
	float	m_flStartFadeOutWeight;
	float	m_flTimeStartFadeIn;
	float	m_flTimeStartFadeOut;
	
	CNetworkVar( float, m_flMaxWeight, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "maxweight" } ]] );

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bStartDisabled;
	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	CNetworkVar( bool, m_bMaster, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bClientSide, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bExclusive, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "exclusive" } ]] );

	CNetworkVar( float, m_MinFalloff, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "minfalloff" } ]] );
	CNetworkVar( float, m_MaxFalloff, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "maxfalloff" } ]] );
	CNetworkVar( float, m_flCurWeight, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkString( m_netlookupFilename, MAX_PATH, [[= ks::reflect::Net{} ]] );

	[[= ks::reflect::Key{ .name = "filename" } ]] string_t	m_lookupFilename;
};

//=============================================================================
//
// ColorCorrection Controller System. Just a place to store a master controller
//
class CColorCorrectionSystem : public CAutoGameSystem, public CGameEventListener
{
public:

	// Creation/Init.
	CColorCorrectionSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = NULL;
	}

	~CColorCorrectionSystem()
	{
		m_hMasterController = NULL;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void FireGameEvent( IGameEvent *pEvent );
	CColorCorrection *GetMasterColorCorrection( void )			{ return m_hMasterController; }

private:

	void InitMasterController( void );
	CHandle< CColorCorrection > m_hMasterController;
};

CColorCorrectionSystem *ColorCorrectionSystem( void );

#endif // COLOR_CORRECTION_H
