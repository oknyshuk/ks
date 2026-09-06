//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef POSTPROCESSCONTROLLER_H
#define POSTPROCESSCONTROLLER_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include "postprocess_shared.h"

// Spawn Flags
#define SF_POSTPROCESS_MASTER		0x0001

//=============================================================================
//
// Class Postprocess Controller:
//
class [[= ks::reflect::NetTable{ .name = "DT_PostProcessController" } ]]
      CPostProcessController : public CBaseEntity
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_CLASS( CPostProcessController, CBaseEntity );

	CPostProcessController();
	virtual ~CPostProcessController();

	// Parse data from a map file
	virtual void Activate();
	virtual int UpdateTransmitState();

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetFadeTime", .type = FIELD_FLOAT } ]] void InputSetFadeTime(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetLocalContrastStrength", .type = FIELD_FLOAT } ]] void InputSetLocalContrastStrength(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetLocalContrastEdgeStrength", .type = FIELD_FLOAT } ]] void InputSetLocalContrastEdgeStrength(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetVignetteStart", .type = FIELD_FLOAT } ]] void InputSetVignetteStart(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetVignetteEnd", .type = FIELD_FLOAT } ]] void InputSetVignetteEnd(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetVignetteBlurStrength", .type = FIELD_FLOAT } ]] void InputSetVignetteBlurStrength(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetFadeToBlackStrength", .type = FIELD_FLOAT } ]] void InputSetFadeToBlackStrength(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetDepthBlurFocalDistance", .type = FIELD_FLOAT } ]] void InputSetDepthBlurFocalDistance(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetDepthBlurStrength", .type = FIELD_FLOAT } ]] void InputSetDepthBlurStrength(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetScreenBlurStrength", .type = FIELD_FLOAT } ]] void InputSetScreenBlurStrength(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetFilmGrainStrength", .type = FIELD_FLOAT } ]] void InputSetFilmGrainStrength(inputdata_t &data);

	void InputTurnOn(inputdata_t &data);
	void InputTurnOff(inputdata_t &data);

	void Spawn( void );

	bool IsMaster( void ) const { return HasSpawnFlags( SF_FOG_MASTER ); }

public:
	CNetworkArray( float, m_flPostProcessParameters, POST_PROCESS_PARAMETER_COUNT, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "filmgrainstrength", .index = PPPN_FILM_GRAIN_STRENGTH } ]] [[= ks::reflect::Key{ .name = "screenblurstrength", .index = PPPN_SCREEN_BLUR_STRENGTH } ]] [[= ks::reflect::Key{ .name = "depthblurstrength", .index = PPPN_DEPTH_BLUR_STRENGTH } ]] [[= ks::reflect::Key{ .name = "depthblurfocaldistance", .index = PPPN_DEPTH_BLUR_FOCAL_DISTANCE } ]] [[= ks::reflect::Key{ .name = "fadetoblackstrength", .index = PPPN_FADE_TO_BLACK_STRENGTH } ]] [[= ks::reflect::Key{ .name = "vignetteblurstrength", .index = PPPN_VIGNETTE_BLUR_STRENGTH } ]] [[= ks::reflect::Key{ .name = "vignetteend", .index = PPPN_VIGNETTE_END, .as = FIELD_TIME } ]] [[= ks::reflect::Key{ .name = "vignettestart", .index = PPPN_VIGNETTE_START, .as = FIELD_TIME } ]] [[= ks::reflect::Key{ .name = "localcontrastedgestrength", .index = PPPN_LOCAL_CONTRAST_EDGE_STRENGTH } ]] [[= ks::reflect::Key{ .name = "localcontraststrength", .index = PPPN_LOCAL_CONTRAST_STRENGTH } ]] [[= ks::reflect::Key{ .name = "fadetime", .index = PPPN_FADE_TIME } ]] );

	CNetworkVar( bool, m_bMaster, [[= ks::reflect::Net{} ]] );
};

//=============================================================================
//
// Postprocess Controller System.
//
class CPostProcessSystem : public CAutoGameSystem, public CGameEventListener
{
public:

	// Creation/Init.
	CPostProcessSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = NULL;
	}

	~CPostProcessSystem()
	{
		m_hMasterController = NULL;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void FireGameEvent( IGameEvent *pEvent );
	CPostProcessController *GetMasterPostProcessController( void )			{ return m_hMasterController; }

private:

	void InitMasterController( void );
	CHandle< CPostProcessController > m_hMasterController;
};

CPostProcessSystem *PostProcessSystem( void );


#endif // POSTPROCESSCONTROLLER_H
