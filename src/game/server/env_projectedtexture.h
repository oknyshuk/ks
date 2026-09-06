#ifndef ENV_PROJECTEDTEXTURE_H
#define ENV_PROJECTEDTEXTURE_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#define ENV_PROJECTEDTEXTURE_STARTON			(1<<0)
#define ENV_PROJECTEDTEXTURE_ALWAYSUPDATE		(1<<1)

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvProjectedTexture" } ]]
      CEnvProjectedTexture : public CPointEntity
{
	DECLARE_CLASS( CEnvProjectedTexture, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvProjectedTexture();
	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );

	// Always transmit to clients
	virtual int UpdateTransmitState();
	virtual void Activate( void );
	virtual void Spawn( void );

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AlwaysUpdateOn", .type = FIELD_VOID } ]] void InputAlwaysUpdateOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AlwaysUpdateOff", .type = FIELD_VOID } ]] void InputAlwaysUpdateOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FOV", .type = FIELD_FLOAT } ]] void InputSetFOV( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Target", .type = FIELD_EHANDLE } ]] void InputSetTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CameraSpace", .type = FIELD_BOOLEAN } ]] void InputSetCameraSpace( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightOnlyTarget", .type = FIELD_BOOLEAN } ]] void InputSetLightOnlyTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightWorld", .type = FIELD_BOOLEAN } ]] void InputSetLightWorld( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableShadows", .type = FIELD_BOOLEAN } ]] void InputSetEnableShadows( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColor", .type = FIELD_COLOR32 } ]] void InputSetLightColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SpotlightTexture", .type = FIELD_STRING } ]] void InputSetSpotlightTexture( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Ambient", .type = FIELD_FLOAT } ]] void InputSetAmbient( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetLightStyle", .type = FIELD_INTEGER } ]] void InputSetLightStyle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPattern", .type = FIELD_STRING } ]] void InputSetPattern( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNearZ", .type = FIELD_FLOAT } ]] void InputSetNearZ( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarZ", .type = FIELD_FLOAT } ]] void InputSetFarZ( inputdata_t &inputdata );

	void InitialThink( void );

	CNetworkHandle( CBaseEntity, m_hTargetEntity, [[= ks::reflect::Net{} ]] );

private:

	void EnforceSingleProjectionRules( bool bWarnOnEnforcement = false );

	CNetworkVar( bool, m_bState, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bAlwaysUpdate, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flLightFOV, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "lightfov" } ]] );
	CNetworkVar( bool, m_bEnableShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enableshadows" } ]] );
	CNetworkVar( bool, m_bSimpleProjection, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "simpleprojection" } ]] );
	CNetworkVar( bool, m_bLightOnlyTarget, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "lightonlytarget" } ]] );
	CNetworkVar( bool, m_bLightWorld, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "lightworld" } ]] );
	CNetworkVar( bool, m_bCameraSpace, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "cameraspace" } ]] );
	CNetworkVar( float, m_flBrightnessScale, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "brightnessscale" } ]] );
	CNetworkColor32( m_LightColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] );
	CNetworkVar( float, m_flColorTransitionTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "colortransitiontime" } ]] );
	CNetworkVar( float, m_flAmbient, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "ambient" } ]] );
	CNetworkString( m_SpotlightTextureName, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_nSpotlightTextureFrame, [[= ks::reflect::Net{ .bits = -1 } ]] [[= ks::reflect::Key{ .name = "textureframe" } ]] );
	CNetworkVar( float, m_flNearZ, [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 500.0f, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Key{ .name = "nearz" } ]] );
	CNetworkVar( float, m_flFarZ, [[= ks::reflect::Net{ .bits = 18, .low = 0.0f, .high = 2500.0f, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Key{ .name = "farz" } ]] );
	CNetworkVar( int, m_nShadowQuality, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "shadowquality" } ]] );
	CNetworkVar( float, m_flProjectionSize, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "projection_size" } ]] );
	CNetworkVar( float, m_flRotation, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "projection_rotation" } ]] );
	
	// Light style
	CNetworkVar( int, m_iStyle, [[= ks::reflect::Net{ .bits = -1 } ]] [[= ks::reflect::Key{ .name = "style" } ]] );
	[[= ks::reflect::Key{ .name = "defaultstyle" } ]] int			m_iDefaultStyle;
	[[= ks::reflect::Key{ .name = "pattern" } ]] string_t	m_iszPattern;

};


#endif	// ENV_PROJECTEDTEXTURE_H
