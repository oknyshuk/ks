//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FOGCONTROLLER_H
#define FOGCONTROLLER_H

#include "reflect_annotations.h"

#include "playernet_vars.h"
#include "igamesystem.h"
#include "GameEventListener.h"

bool GetWorldFogParams( CBaseCombatCharacter *character, fogparams_t &fog );

// Spawn Flags
#define SF_FOG_MASTER		0x0001

//=============================================================================
//
// Class Fog Controller:
// Compares a set of integer inputs to the one main input
// Outputs true if they are all equivalant, false otherwise
//
class
      [[= ks::reflect::NetTable{ .name = "DT_FogController", .base = false } ]]
      [[= ks::reflect::From<"m_fog.enable", ks::reflect::Net{ .bits = 1,  .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_fog.blend", ks::reflect::Net{ .bits = 1,  .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_fog.dirPrimary", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_fog.colorPrimary", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_fog.colorSecondary", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_fog.start", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.end", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.maxdensity", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.farz", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.colorPrimaryLerpTo", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_fog.colorSecondaryLerpTo", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_fog.startLerpTo", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.endLerpTo", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.maxdensityLerpTo", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.lerptime", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.duration", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.HDRColorScale", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_fog.ZoomFogScale", ks::reflect::Net{ .bits = 0,  .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.colorPrimary", ks::reflect::Key{ .name = "fogcolor" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.colorSecondary", ks::reflect::Key{ .name = "fogcolor2" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.dirPrimary", ks::reflect::Key{ .name = "fogdir" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.enable", ks::reflect::Key{ .name = "fogenable" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.blend", ks::reflect::Key{ .name = "fogblend" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.start", ks::reflect::Key{ .name = "fogstart" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.end", ks::reflect::Key{ .name = "fogend" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.maxdensity", ks::reflect::Key{ .name = "fogmaxdensity" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.farz", ks::reflect::Key{ .name = "farz" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.duration", ks::reflect::Key{ .name = "foglerptime" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.HDRColorScale", ks::reflect::Key{ .name = "HDRColorScale" } >{} ]]
      [[= ks::reflect::KeyFrom<"m_fog.ZoomFogScale", ks::reflect::Key{ .name = "ZoomFogScale" } >{} ]]
      CFogController : public CBaseEntity
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_CLASS( CFogController, CBaseEntity );

	CFogController();
	~CFogController();

	// Parse data from a map file
	virtual void Activate();
	virtual int UpdateTransmitState();

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetStartDist", .type = FIELD_FLOAT } ]] void InputSetStartDist(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetEndDist", .type = FIELD_FLOAT } ]] void InputSetEndDist(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetColor", .type = FIELD_COLOR32 } ]] void InputSetColor(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetColorSecondary", .type = FIELD_COLOR32 } ]] void InputSetColorSecondary(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetFarZ", .type = FIELD_INTEGER } ]] void InputSetFarZ( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void InputSetAngles( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxDensity", .type = FIELD_FLOAT } ]] void InputSetMaxDensity( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetZoomFogScale", .type = FIELD_STRING } ]] void InputSetZoomFogScale( inputdata_t &inputdata );	

	[[= ks::reflect::Input{ .name = "SetColorLerpTo", .type = FIELD_COLOR32 } ]] void InputSetColorLerpTo(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetColorSecondaryLerpTo", .type = FIELD_COLOR32 } ]] void InputSetColorSecondaryLerpTo(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetStartDistLerpTo", .type = FIELD_FLOAT } ]] void InputSetStartDistLerpTo(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetEndDistLerpTo", .type = FIELD_FLOAT } ]] void InputSetEndDistLerpTo(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "SetMaxDensityLerpTo", .type = FIELD_FLOAT } ]] void InputSetMaxDensityLerpTo(inputdata_t &data);

	[[= ks::reflect::Input{ .name = "StartFogTransition", .type = FIELD_VOID } ]] void InputStartFogTransition(inputdata_t &data);

	int DrawDebugTextOverlays(void);

	void SetLerpValues( void );
	void Spawn( void );

	bool IsMaster( void )					{ return HasSpawnFlags( SF_FOG_MASTER ); }

public:

	CNetworkVarEmbedded( fogparams_t, m_fog );
	[[= ks::reflect::Key{ .name = "use_angles" } ]] bool					m_bUseAngles;
	int						m_iChangedVariables;
};

//=============================================================================
//
// Fog Controller System.
//
class CFogSystem : public CAutoGameSystem, public CGameEventListener
{
public:

	// Creation/Init.
	CFogSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = nullptr;
	}

	~CFogSystem()
	{
		m_hMasterController = nullptr;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void FireGameEvent( IGameEvent *pEvent );
	CFogController *GetMasterFogController( void )			{ return m_hMasterController; }
	void SetMasterController( CFogController *pFogController );


private:

	void InitMasterController( void );
	CHandle< CFogController > m_hMasterController;
};

CFogSystem *FogSystem( void );

#endif // FOGCONTROLLER_H
