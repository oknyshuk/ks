#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "toolframework/itoolentity.h"

//-----------------------------------------------------------------------------
// No tool DLLs ship with the game, so tool recording is never enabled.
//-----------------------------------------------------------------------------
inline constexpr bool ToolsEnabled() { return false; }
inline void ToolFramework_PostToolMessage( HTOOLHANDLE, KeyValues * ) {}
inline constexpr bool ToolFramework_IsThirdPersonCamera() { return false; }



//-----------------------------------------------------------------------------
// Standard messages
//-----------------------------------------------------------------------------
struct EffectRecordingState_t
{
	bool m_bVisible : 1;
	bool m_bThirdPerson : 1;
	Color m_Color;
	float m_Scale;
	const char *m_pMaterialName;
	int m_nAttachment;
	Vector m_vecAttachment; // only used if m_nAttachment is -1
};

struct BaseEntityRecordingState_t
{
	BaseEntityRecordingState_t() :	
		m_flTime( 0.0f ),
		m_pModelName( 0 ),
		m_nOwner( -1 ),
		m_fEffects( 0 ),
		m_bVisible( false ),
		m_bRecordFinalVisibleSample( false ),
		m_numEffects( 0 ),
		m_pEffects( nullptr ),
		m_nFollowEntity( -1 )
	{
		m_vecRenderOrigin.Init();
		m_vecRenderAngles.Init();
	}

	float m_flTime;
	const char *m_pModelName;
	int m_nOwner;
	int m_fEffects;
	bool m_bVisible : 1;
	bool m_bRecordFinalVisibleSample : 1;
	Vector m_vecRenderOrigin;
	QAngle m_vecRenderAngles;
	int m_nFollowEntity;

	int m_numEffects;
	EffectRecordingState_t *m_pEffects;
};

struct SpriteRecordingState_t
{
	float m_flRenderScale;
	float m_flFrame;
	int m_nRenderMode;
	bool m_nRenderFX;
	Color m_Color;
	float m_flProxyRadius;
};

struct BaseAnimatingHighLevelRecordingState_t
{
	BaseAnimatingHighLevelRecordingState_t()
	:	m_bClearIkTargets( false ),
		m_bIsRagdoll( false ),
		m_bShouldCreateIkContext( false ),
		m_nNumPoseParams( 0 ),
		m_flCycle( 0.0f ),
		m_flPlaybackRate( 1.0f ),
		m_flCycleRate( 0.0f ),
		m_nFrameCount( 0 ),
		m_bInterpEffectActive( false )
	{
	}

	bool m_bClearIkTargets;
	bool m_bIsRagdoll;
	bool m_bShouldCreateIkContext;
	int m_nNumPoseParams;

	float m_flCycle;
	float m_flPlaybackRate;
	float m_flCycleRate;
	int m_nFrameCount;

	float m_flPoseParameter[MAXSTUDIOPOSEPARAM];

	bool m_bInterpEffectActive;
};

struct BaseAnimatingRecordingState_t
{
	BaseAnimatingHighLevelRecordingState_t m_highLevelState;

	int m_nSkin;
	int m_nBody;
	int m_nSequence;
	CBoneList *m_pBoneList;
};

struct BaseFlexRecordingState_t
{
	int m_nFlexCount;
	float *m_pDestWeight;
	Vector m_vecViewTarget;
};

struct CameraRecordingState_t
{
	bool m_bThirdPerson;
	float m_flFOV;
	Vector m_vecEyePosition;
	QAngle m_vecEyeAngles;
	bool m_bPlayerEyeIsPortalled; //for portal. Need to reverse some portal recursion draw logic when the player eye and body straddle a portal
};

struct MonitorRecordingState_t
{
	bool	m_bActive;
	float	m_flFOV;
	bool	m_bFogEnabled;
	float	m_flFogStart;
	float	m_flFogEnd;
	Color	m_FogColor;
};


//-----------------------------------------------------------------------------
// Recorded temp entity structures
//-----------------------------------------------------------------------------
enum TERecordingType_t
{
	TE_DYNAMIC_LIGHT = 0,
	TE_WORLD_DECAL,
	TE_DISPATCH_EFFECT,
	TE_MUZZLE_FLASH,
	TE_ARMOR_RICOCHET,
	TE_METAL_SPARKS,
	TE_SMOKE,
	TE_SPARKS,
	TE_BLOOD_SPRITE,
	TE_BREAK_MODEL,
	TE_GLOW_SPRITE,
	TE_PHYSICS_PROP,
	TE_SPRITE_SINGLE,
	TE_SPRITE_SPRAY,
	TE_CONCUSSIVE_EXPLOSION,
	TE_BLOOD_STREAM,
	TE_SHATTER_SURFACE,
	TE_DECAL,
	TE_PROJECT_DECAL,
	TE_EXPLOSION,

	TE_RECORDING_TYPE_COUNT,
};
