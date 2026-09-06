//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PLAYERLOCALDATA_H
#define PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif


#include "playernet_vars.h"
#include "networkvar.h"
#include "fogcontroller.h"
#include "postprocesscontroller.h"
#include "colorcorrection.h"

//-----------------------------------------------------------------------------
// Purpose: Player specific data ( sent only to local player, too )
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_Local", .base = false } ]]
      [[= ks::reflect::From<"m_skybox3d.scale", ks::reflect::Net{ .bits = 12 }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.origin", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.area", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.enable", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.blend", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.dirPrimary", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.colorPrimary", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.colorSecondary", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.start", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.end", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.maxdensity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_skybox3d.fog.HDRColorScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[0]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[1]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[2]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[3]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[4]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[5]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[6]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.localSound[7]", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD }>{} ]]
      [[= ks::reflect::From<"m_audio.soundscapeIndex", ks::reflect::Net{ .bits = 17, .flags = 0 }>{} ]]
      [[= ks::reflect::From<"m_audio.localBits", ks::reflect::Net{ .bits = NUM_AUDIO_LOCAL_SOUNDS, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_audio.entIndex", ks::reflect::Net{}>{} ]]
      CPlayerLocalData
{
public:
	// Save/restore
	DECLARE_SIMPLE_DATADESC();
	// Prediction data copying
	DECLARE_CLASS_NOBASE( CPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	CPlayerLocalData();

	void UpdateAreaBits( CBasePlayer *pl, unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES] );

public:
	CNetworkArray( unsigned char, m_chAreaBits, MAX_AREA_STATE_BYTES , [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );					// Which areas are potentially visible to the client?
	CNetworkArray( unsigned char, m_chAreaPortalBits, MAX_AREA_PORTAL_STATE_BYTES , [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );	// Which area portals are open?

	CNetworkVar( int,	m_iHideHUD , [[= ks::reflect::Net{ .bits = HIDEHUD_BITCOUNT, .flags = SPROP_UNSIGNED } ]] );		// bitfields containing sections of the HUD to hide
	CNetworkVar( float, m_flFOVRate , [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );		// rate at which the FOV changes (defaults to 0)
		
	Vector				m_vecOverViewpoint;			// Viewpoint overriding the real player's viewpoint
	
	// Fully ducked
	CNetworkVar( bool, m_bDucked , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::ENC_INT } ]] );
	// In process of ducking
	CNetworkVar( bool, m_bDucking , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::ENC_INT } ]] );
	// Last time the user pressed duck (to handle duck-spam)
	CNetworkVar( float, m_flLastDuckTime , [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	// In process of duck-jumping
	CNetworkVar( bool, m_bInDuckJump , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::ENC_INT } ]] );
	// During ducking process, amount of time before full duc
	CNetworkVar( int, m_nDuckTimeMsecs , [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN } ]] );
	CNetworkVar( int, m_nDuckJumpTimeMsecs , [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] );
	// Jump time, time to auto unduck (since we auto crouch jump now).
	CNetworkVar( int, m_nJumpTimeMsecs , [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] );
	// Step sound side flip/flip
	int m_nStepside;
	// Velocity at time when we hit ground
	CNetworkVar( float, m_flFallVelocity , [[= ks::reflect::Net{ .bits = 17, .low = -4096.0f, .high = 4096.0f, .flags = SPROP_CHANGES_OFTEN } ]] );
	// Previous button state
	int m_nOldButtons;
	class CSkyCamera *m_pOldSkyCamera;
	// Base velocity that was passed in to server physics so 
	//  client can predict conveyors correctly.  Server zeroes it, so we need to store here, too.
	// auto-decaying view angle adjustment
#if PREDICTION_ERROR_CHECK_LEVEL > 1
	CNetworkQAngleXYZ( m_viewPunchAngle );
	CNetworkQAngleXYZ( m_aimPunchAngle );
	CNetworkQAngleXYZ( m_aimPunchAngleVel );
#else
	CNetworkQAngle( m_viewPunchAngle, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkQAngle( m_aimPunchAngle, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkQAngle( m_aimPunchAngleVel, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::ENC_VECTOR } ]] );
#endif
	// Draw view model for the player
	CNetworkVar( bool, m_bDrawViewmodel , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::ENC_INT } ]] );

	// Is the player wearing the HEV suit
	CNetworkVar( bool, m_bWearingSuit , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::ENC_INT } ]] );
	CNetworkVar( bool, m_bPoisoned , [[= ks::reflect::Net{  } ]] );
	CNetworkVar( float, m_flStepSize , [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 128.0f, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( bool, m_bAllowAutoMovement , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::ENC_INT } ]] );

	// Autoaim
	CNetworkVar( bool,	m_bAutoAimTarget );

	// 3d skybox
	CNetworkVarEmbedded( sky3dparams_t, m_skybox3d );
	// world fog
	CNetworkVarEmbedded( fogplayerparams_t, m_PlayerFog );
	fogparams_t			m_fog;
	// audio environment
	CNetworkVarEmbedded( audioparams_t, m_audio );

	CNetworkVar( bool, m_bSlowMovement );
	CNetworkVar( float, m_fTBeamEndTime );
};

EXTERN_SEND_TABLE(DT_Local);


#endif // PLAYERLOCALDATA_H
