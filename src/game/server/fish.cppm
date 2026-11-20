// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include "bone_setup.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "BaseAnimatingOverlay.h"
#include "predictable_entity.h"
#include "studio.h"
#include "declare_class.h"
#include "choreoactor.h"
#include "choreoscene.h"
#include "irecipientfilter.h"
#include "responserules/response_types.h"
#include "vscript/ivscript.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "engine/IEngineTrace.h"
#include "iservernetworkable.h"
#include "string_t.h"
#include "vphysics_interface.h"
#include "baseplayer_shared.h"
#include "edict.h"
#include "datamap.h"
#include "dt_send.h"
#include "igameevents.h"
#include "localflexcontroller.h"
#include "eiface.h"
#include "shattersurfacetypes.h"
#include "gamerules_register.h"
#include "modelentities.h"
#include "cached_file_data.h"
#include "ihandleentity.h"
#include "datamodel/dmattributetypes.h"
#include "vote_controller.h"
module ks.server:fish;

import std;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.game.ai_activity;
import :vscript;
import ks.game.shareddefs;
import ks.game.iplayeranimstate;
import ks.math.compressed_vector;
import ks.math.vector2d;
import :baseentity;
import ks.core.threadtools;
import ks.core.types;
import :cstrike15.cs_playeranimstate;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.math.mathlib;
import ks.game.entitycore;
import ks.tier1.keyvalues;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.game.igamemovement;
import ks.core.vprof;
import ks.math.vmatrix;
import ks.game.postprocess_shared;
import ks.core.platform;

class CBaseAnimatingOverlay;

class CBaseAnimating;

using namespace ::ks::reflect;

// #include "BaseAnimating.h"

inline constexpr auto ORDER_BITS = 4;

inline constexpr auto WEIGHT_BITS = 8;

class CAnimationLayer : public CMemZeroOnNew
{
	[[= ks::reflect::NetTable{ .name = "DT_Animationlayer", .base = false } ]]
	static constexpr bool ks_class = true;

public:	
	DECLARE_CLASS_NOBASE( CAnimationLayer );
	
	CAnimationLayer( void );
	void	Init( CBaseAnimatingOverlay *pOverlay );

	// float	SetBlending( int iBlender, float flValue, CBaseAnimating *pOwner );
	void	StudioFrameAdvance( float flInterval, CBaseAnimating *pOwner );
	void	DispatchAnimEvents( CBaseAnimating *eventHandler, CBaseAnimating *pOwner );
	void	SetOrder( int nOrder );

	float GetFadeout( float flCurTime );

	// For CNetworkVars.
	void NetworkStateChanged();
	void NetworkStateChanged( void *pVar );

public:	

#define ANIM_LAYER_KILLME		0x0004
#define ANIM_LAYER_CHECKACCESS	0x0010
#define ANIM_LAYER_DYING		0x0020

	int		m_fFlags;

	bool	m_bSequenceFinished;
	bool	m_bLooping;
	
	CNetworkVar( int, m_nSequence, [[= ks::reflect::Net{ .bits = ANIMATION_SEQUENCE_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flCycle, [[= ks::reflect::Net{ .bits = ANIMATION_CYCLE_BITS, .low = 0.0f, .high = 1.0f, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( float, m_flPlaybackRate, [[= ks::reflect::Net{ .bits = WEIGHT_BITS, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flPrevCycle, [[= ks::reflect::Net{ .bits = ANIMATION_CYCLE_BITS, .low = 0.0f, .high = 1.0f, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( float, m_flWeight, [[= ks::reflect::Net{ .bits = WEIGHT_BITS, .low = 0.0f, .high = 1.0f } ]] );
	CNetworkVar( float, m_flWeightDeltaRate, [[= ks::reflect::Net{ .bits = WEIGHT_BITS, .flags = SPROP_NOSCALE } ]] );

	float	m_flBlendIn; // start and end blend frac (0.0 for now blend)
	float	m_flBlendOut; 

	float	m_flKillRate;
	float	m_flKillDelay;

	float	m_flLayerAnimtime;
	float	m_flLayerFadeOuttime;

	// dispatch flags
	CStudioHdr *m_pDispatchedStudioHdr;
	int		m_nDispatchedSrc;
	int		m_nDispatchedDst;

	// For checking for duplicates
	Activity	m_nActivity;

	// order of layering on client
	int		m_nPriority;
	CNetworkVar( int, m_nOrder, [[= ks::reflect::Net{ .bits = ORDER_BITS, .flags = SPROP_UNSIGNED } ]] );
	int		GetOrder( void ) { return m_nOrder; }

	bool	IsActive( void ) { return ((m_fFlags & ANIM_LAYER_ACTIVE) != 0); }
	bool	IsAutokill( void ) { return ((m_fFlags & ANIM_LAYER_AUTOKILL) != 0); }
	bool	IsKillMe( void ) { return ((m_fFlags & ANIM_LAYER_KILLME) != 0); }
	bool	IsAutoramp( void ) { return (m_flBlendIn != 0.0 || m_flBlendOut != 0.0); }
	void	KillMe( void ) { m_fFlags |= ANIM_LAYER_KILLME; }
	void	Dying( void ) { m_fFlags |= ANIM_LAYER_DYING; }
	bool	IsDying( void ) { return ((m_fFlags & ANIM_LAYER_DYING) != 0); }
	void	Dead( void ) { m_fFlags &= ~ANIM_LAYER_DYING; }
	bool	NoEvents( void ) { return ((m_fFlags & ANIM_LAYER_NOEVENTS) != 0); }

	void	SetSequence( int nSequence );
	void	SetCycle( float flCycle );
	void	SetPrevCycle( float flCycle );
	void	SetPlaybackRate( float flPlaybackRate );
	void	SetWeight( float flWeight );
	void	SetWeightDeltaRate( float flDelta );

	int		GetSequence( ) const;
	float	GetCycle( ) const;
	float	GetPrevCycle( ) const;
	float	GetPlaybackRate( ) const;
	float	GetWeight( ) const;
	float	GetWeightDeltaRate( ) const;

	bool	IsAbandoned( void );
	void	MarkActive( void );

	float	m_flLastEventCheck;

	float	m_flLastAccess;

	// Network state changes get forwarded here.
	CBaseAnimatingOverlay *m_pOwnerEntity;
	
	DECLARE_SIMPLE_DATADESC();
};

[[gnu::always_inline]] inline void CAnimationLayer::SetSequence( int nSequence )
{
	m_nSequence = nSequence;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetCycle( float flCycle )
{
	m_flCycle = flCycle;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetWeight( float flWeight )
{
	m_flWeight = flWeight;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetWeightDeltaRate( float flDelta )
{
	m_flWeightDeltaRate = flDelta;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetPrevCycle( float flPrevCycle )
{
	m_flPrevCycle = flPrevCycle;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetPlaybackRate( float flPlaybackRate )
{
	m_flPlaybackRate = flPlaybackRate;
}

[[gnu::always_inline]] inline int	CAnimationLayer::GetSequence( ) const
{
	return m_nSequence;
}

[[gnu::always_inline]] inline float CAnimationLayer::GetCycle( ) const
{
	return m_flCycle;
}

[[gnu::always_inline]] inline float CAnimationLayer::GetPrevCycle( ) const
{
	return m_flPrevCycle;
}

[[gnu::always_inline]] inline float CAnimationLayer::GetPlaybackRate( ) const
{
	return m_flPlaybackRate;
}

[[gnu::always_inline]] inline float CAnimationLayer::GetWeight( ) const
{
	return m_flWeight;
}

[[gnu::always_inline]] inline float CAnimationLayer::GetWeightDeltaRate( ) const
{
	return m_flWeightDeltaRate;
}

namespace DT_OverlayVars { extern SendTable g_SendTable; }

namespace DT_Animationlayer { extern SendTable g_SendTable; }

class CBaseAnimatingOverlay : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseAnimatingOverlay" } ]]
	[[= ks::reflect::SubTable<"overlay_vars", &DT_OverlayVars::g_SendTable, nullptr, true>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_OverlayVars", .base = false } ]]
	// 15, i.e. CBaseAnimatingOverlay::MAX_OVERLAYS -- the class is incomplete here
	[[= ks::reflect::UtlVec<"m_AnimOverlay", 15, &DT_Animationlayer::g_SendTable, nullptr, "DT_OverlayVars">{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseAnimatingOverlay, CBaseAnimating );

public:
	enum 
	{
		MAX_OVERLAYS = 15,
	};

private:
	std::vector< CAnimationLayer > m_AnimOverlay;
	//int				m_nActiveLayers;
	//int				m_nActiveBaseLayers;

public:
	
	CBaseAnimatingOverlay *	GetBaseAnimatingOverlay_Impl() { return this; }
	
	void	OnRestore_Impl();
	void	SetModel_Impl( const char *szModelName );

	virtual void	StudioFrameAdvance();
	virtual	void	DispatchAnimEvents ( CBaseAnimating *eventHandler );
	virtual void	GetSkeleton( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternionAligned q[], int boneMask );

	int		AddGestureSequence( int sequence, bool autokill = true );
	int		AddGestureSequence( int sequence, float flDuration, bool autokill = true );
	int		AddGesture( Activity activity, bool autokill = true );
	int		AddGesture( Activity activity, float flDuration, bool autokill = true );
	bool	IsPlayingGesture( Activity activity );
	void	RestartGesture( Activity activity, bool addifmissing = true, bool autokill = true );
	void	RemoveGesture( Activity activity );
	void	RemoveAllGestures( void );

	int		AddLayeredSequence( int sequence, int iPriority );

	void	SetLayerPriority( int iLayer, int iPriority );

	bool	IsValidLayer( int iLayer );

	void	SetLayerDuration( int iLayer, float flDuration );
	float	GetLayerDuration( int iLayer );

	void	SetLayerCycle( int iLayer, float flCycle );
	void	SetLayerCycle( int iLayer, float flCycle, float flPrevCycle );
	float	GetLayerCycle( int iLayer );

	void	SetLayerPlaybackRate( int iLayer, float flPlaybackRate );
	void	SetLayerWeight( int iLayer, float flWeight );
	float	GetLayerWeight( int iLayer );
	void	SetLayerBlendIn( int iLayer, float flBlendIn );
	void	SetLayerBlendOut( int iLayer, float flBlendOut );
	void	SetLayerAutokill( int iLayer, bool bAutokill );
	void	SetLayerLooping( int iLayer, bool bLooping );
	void	SetLayerNoRestore( int iLayer, bool bNoRestore );
	void	SetLayerNoEvents( int iLayer, bool bNoEvents );

	Activity	GetLayerActivity( int iLayer );
	int			GetLayerSequence( int iLayer );

	int		FindGestureLayer( Activity activity );

	void	RemoveLayer( int iLayer, float flKillRate = 0.2, float flKillDelay = 0.0 );
	void	FastRemoveLayer( int iLayer );

	CAnimationLayer *GetAnimOverlay( int iIndex, bool bUseOrder = true );
	int GetNumAnimOverlays() const;
	void SetNumAnimOverlays( int num );

	void VerifyOrder( void );

	bool	HasActiveLayer( void );

	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence );
	void AccumulateDispatchedLayers( CBaseAnimatingOverlay *pWeapon, CStudioHdr *pWeaponStudioHdr, IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	void RegenerateDispatchedLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );

private:
	int		AllocateLayer( int iPriority = 0 ); // lower priorities are processed first

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();
};

EXTERN_SEND_TABLE(DT_BaseAnimatingOverlay);

// ------------------------------------------------------------------------------------------ //
// CAnimationLayer inlines.
// ------------------------------------------------------------------------------------------ //

#undef ANIM_LAYER_KILLME
#undef ANIM_LAYER_CHECKACCESS
#undef ANIM_LAYER_DYING

class CAnimationLayer;

using namespace ::ks::reflect;

// ------------------------------------------------------------------------------------------------ //
// CSequenceTransitioner declaration.
// ------------------------------------------------------------------------------------------------ //
class CSequenceTransitioner
{
public:
	void CheckForSequenceChange( 
		// Describe the current animation state with these parameters.
		CStudioHdr *hdr,
		int nCurSequence, 

		// Even if the sequence hasn't changed, you can force it to interpolate from the previous
		// spot in the same sequence to the current spot in the same sequence by setting this to true.
		bool bForceNewSequence,

		// Follows EF_NOINTERP.
		bool bInterpolate
		);

	void UpdateCurrent( 
		// Describe the current animation state with these parameters.
		CStudioHdr *hdr,
		int nCurSequence, 
		float flCurCycle,
		float flCurPlaybackRate,
		float flCurTime
		);

	void RemoveAll( void ) { m_animationQueue.clear(); };

public:
	std::vector< CAnimationLayer >	m_animationQueue;
};

class CBaseAnimatingOverlay;

using namespace ::ks::reflect;

	class CBaseAnimatingOverlay;

// If a guy is moving slower than this, then he's considered to not be moving
// (so he goes to his idle animation at full playback rate rather than his walk 
// animation at low playback rate).
inline constexpr auto MOVING_MINIMUM_SPEED = 0.5f;

inline constexpr auto FOOTPLANT_MINIMUM_SPEED = 0.1f;

inline constexpr auto MAIN_IDLE_SEQUENCE_LAYER = 0; // For 8-way blended models, this layer blends an idle on top of the run/walk animation to simulate a 9-way blend.
									// For 9-way blended models, we don't use this layer.

inline constexpr auto AIMSEQUENCE_LAYER = 1; // Aim sequence uses layers 0 and 1 for the weapon idle animation (needs 2 layers so it can blend).

inline constexpr auto NUM_AIMSEQUENCE_LAYERS = 2; // Then it uses layers 2 and 3 to blend in the weapon run/walk/crouchwalk animation.

// Everyone who derives from CBasePlayerAnimState gets to fill in this info
// to drive how the animation state is generated.
class CModAnimConfig
{
public:
	// This tells how far the upper body can rotate left and right. If he begins to rotate
	// past this, it'll turn his feet to face his upper body.
	float	m_flMaxBodyYawDegrees;

	float	m_flMaxBodyYawDegreesCorrectionAmount;
	float	m_flIdleFootPlantMaxYaw;
	float	m_flIdleFootPlantFootLiftDelta;

	// How do the legs animate?
	LegAnimType_t m_LegAnimType;

	// Use aim sequences? (CS hostages don't).
	bool m_bUseAimSequences;
};

// ------------------------------------------------------------------------------------------------ //
// CBasePlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //

class CBasePlayerAnimState : public IPlayerAnimState
{
public:
	DECLARE_CLASS_NOBASE( CBasePlayerAnimState );

	enum
	{
		TURN_NONE = 0,
		TURN_LEFT,
		TURN_RIGHT
	};

						CBasePlayerAnimState();
	virtual ~CBasePlayerAnimState();

	void Init( CBaseAnimatingOverlay *pPlayer, const CModAnimConfig &config );
	virtual void Release();

	// Update() and DoAnimationEvent() together maintain the entire player's animation state.
	//
	// Update() maintains the the lower body animation (the player's m_nSequence)
	// and the upper body overlay based on the player's velocity and look direction.
	//
	// It also modulates these based on events triggered by DoAnimationEvent.
	virtual void Update( float eyeYaw, float eyePitch );

	// This is called by the client when a new player enters the PVS to clear any events
	// the dormant version of the entity may have been playing.
	virtual void ClearAnimationState();

	// This is called every frame to prepare the animation layers to be filled with data
	// since we reconstruct them every frame (in case they get stomped by the networking
	// or anything else).
	virtual void ClearAnimationLayers();

	// The client uses this to figure out what angles to render the entity with (since as the guy turns,
	// it will change his body_yaw pose parameter before changing his rendered angle).
	virtual const QAngle& GetRenderAngles();

	virtual void SetForceAimYaw( bool bForce );

// Overrideables.
public:

	virtual bool ShouldUpdateAnimState();

	// This is called near the start of each frame.
	// The base class figures out the main sequence and the aim sequence, and derived
	// classes can overlay whatever other animations they want.
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );

	// This is called to figure out what the main activity is. The mod-specific class 
	// overrides this to handle events like jumping, firing, etc.
	virtual Activity CalcMainActivity() = 0;

	// This is called to calculate the aim layer sequence. It usually figures out the 
	// animation prefixes and suffixes and calls CalcSequenceIndex().
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle ) = 0;

	// This lets server-controlled idle sequences to play unchanged on the client
	virtual bool ShouldChangeSequences( void ) const;

	// If this returns true, then it will blend the current aim layer sequence with an idle aim layer
	// sequence based on how fast the character is moving, so it doesn't play the upper-body run at
	// full speed if he's moving really slowly.
	//
	// We return false on this for animations that don't have blends, like reloads.
	virtual bool ShouldBlendAimSequenceToIdle();

	// For the body left/right rotation, some models use a pose parameter and some use a bone controller.
	virtual float SetOuterBodyYaw( float flValue );

	// Return true if the player is allowed to move.
	virtual bool CanThePlayerMove();

	// This is called every frame to see what the maximum speed the player can move is.
	// It is used to determine where to put the move_x/move_y pose parameters or to
	// determine the animation playback rate, based on the player's movement speed.
	// The return value from here is interpolated so the playback rate or pose params don't move sharply.
	virtual float GetCurrentMaxGroundSpeed() = 0;

	// Display Con_NPrint output about the animation state. This is called if
	// we're on the client and if cl_showanimstate holds the current entity's index.
	void DebugShowAnimStateFull( int iStartLine );

	virtual void DebugShowAnimState( int iStartLine );
	void AnimStatePrintf( int iLine, const char *pMsg, ... );
	void AnimStateLog( const char *pMsg, ... );

	// Calculate the playback rate for movement layer
	virtual float CalcMovementPlaybackRate( bool *bIsMoving );

	// Allow inheriting classes to translate their desired activity, while keeping all
	// internal ACT comparisons using the base activity
	virtual Activity TranslateActivity( Activity actDesired ) { return actDesired; }

	// Allow inheriting classes to override SelectWeightedSequence
	virtual int SelectWeightedSequence( Activity activity );

public:
	
	void				GetPoseParameters( CStudioHdr *pStudioHdr, float poseParameter[MAXSTUDIOPOSEPARAM] );

	CBaseAnimatingOverlay	*GetOuter() const;

	void				RestartMainSequence();

	virtual	float		GetFeetYawRate( void );

// Helpers for the derived classes to use.
protected:

	// Sets up the string you specify, looks for that sequence and returns the index. 
	// Complains in the console and returns 0 if it can't find it.
	virtual int CalcSequenceIndex( const char *pBaseName, ... );

	Activity GetCurrentMainSequenceActivity() const;

	void				GetOuterAbsVelocity( Vector& vel ) const;
	float				GetOuterXYSpeed() const;

	// How long has it been since we cleared the animation state?
	float				TimeSinceLastAnimationStateClear() const;

	float				GetEyeYaw() const { return m_flEyeYaw; }

	void				SetOuterPoseParameter( int iParam, float flValue );

protected:
	
	CModAnimConfig		m_AnimConfig;
	CBaseAnimatingOverlay	*m_pOuter;

protected:
	virtual int			ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current );
	virtual void		ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_BodyYaw();

	virtual void		ResetGroundSpeed( void );
	virtual bool		ShouldResetGroundSpeed( Activity oldActivity, Activity idealActivity );

protected:
	// The player's eye yaw and pitch angles.
	float m_flEyeYaw;
	float m_flEyePitch;

	// The following variables are used for tweaking the yaw of the upper body when standing still and
	//  making sure that it smoothly blends in and out once the player starts moving
	// Direction feet were facing when we stopped moving
	float				m_flGoalFeetYaw;

	float				m_flCurrentFeetYaw;
	bool				m_bCurrentFeetYawInitialized;

	bool				m_bForceAimYaw;

	float				m_flCurrentTorsoYaw;

	// To check if they are rotating in place
	float				m_flLastYaw;

	// Time when we stopped moving
	float				m_flLastTurnTime;

	// One of the above enums
	int					m_nTurningInPlace;

	QAngle				m_angRender;

	Vector2D			m_vLastMovePose;

	bool				m_bInFootPlantIdleTurn;
	float				m_flFootPlantIdleTurnCycle;
	bool				m_bFootPlantIdleNeedToLiftFeet;

	float				m_flPoseParamTargetDampenedScaleIdeal;

private:

	// Update the prone state machine.
	void		UpdateProneState();

	// Get the string that's appended to animation names for the player's current weapon.
	const char* GetWeaponSuffix();

	Activity			BodyYawTranslateActivity( Activity activity );

	void				EstimateYaw();

	virtual bool		ShouldResetMainSequence( int iCurrentSequence, int iNewSequence );
	void				ComputeMainSequence();
	void				ComputeAimSequence();

	void				ComputePlaybackRate();

	void				ResetCycleAcrossCustomActivityChange( Activity iCurrent, Activity iNew );

	void UpdateInterpolators();
	float GetInterpolatedGroundSpeed();

private:
	
	float m_flMaxGroundSpeed;

	float m_flLastAnimationStateClearTime;

	// If he's using 8-way blending, then we blend to this idle 
	int m_iCurrent8WayIdleSequence;
	int m_iCurrent8WayCrouchIdleSequence;

	// Last activity we've used on the lower body. Used to determine if animations should restart.
	Activity m_eCurrentMainSequenceActivity;	
												
	float				m_flGaitYaw;
	float				m_flStoredCycle;

	void UpdateAimSequenceLayers(
		float flCycle,
		int iFirstLayer,
		bool bForceIdle,
		CSequenceTransitioner *pTransitioner,
		float flWeightScale
		);

	void OptimizeLayerWeights( int iFirstLayer, int nLayers );

protected:

	// This gives us smooth transitions between aim anim sequences on the client.
	CSequenceTransitioner	m_HighAimSequenceTransitioner;
	CSequenceTransitioner	m_LowAimSequenceTransitioner;
};

extern float g_flLastBodyPitch, g_flLastBodyYaw, m_flLastMoveYaw;

class CBaseAnimatingOverlay;
class CBaseEntity;
class CSceneEventInfo;

using namespace ::ks::reflect;

typedef ResponseRules::CRR_Response		AI_Response;

//-----------------------------------------------------------------------------
// Purpose:  A .vfe referenced by a scene during .vcd playback
//-----------------------------------------------------------------------------
class CFlexSceneFile
{
public:
	enum
	{
		MAX_FLEX_FILENAME = 128,
	};

	char			filename[ MAX_FLEX_FILENAME ];
	void			*buffer;
};

//-----------------------------------------------------------------------------
// Purpose: Animated characters who have vertex flex capability (e.g., facial expressions)
//-----------------------------------------------------------------------------
class CBaseFlex : public CBaseAnimatingOverlay
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseFlex" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseFlex, CBaseAnimatingOverlay );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();
	// script description
	DECLARE_ENT_SCRIPTDESC();

	// Construction
						CBaseFlex( void );
						~CBaseFlex( void );

	void		SetModel_Impl( const char *szModelName );
	CStudioHdr *OnNewModel_Impl();

	void Blink( );

	virtual	void		SetViewtarget( const Vector &viewtarget );
	const Vector		&GetViewtarget( void ) const;

	void				SetFlexWeight( char *szName, float value );
	void				SetFlexWeight( LocalFlexController_t index, float value );
	float				GetFlexWeight( char *szName );
	float				GetFlexWeight( LocalFlexController_t index );

	// Look up flex controller index by global name
	LocalFlexController_t	FindFlexController( const char *szName );
	void				EnsureTranslations( const flexsettinghdr_t *pSettinghdr );

	// Keep track of what scenes are being played
	void				StartChoreoScene( CChoreoScene *scene );
	void				RemoveChoreoScene( CChoreoScene *scene, bool canceled = false );

	// Start the specifics of an scene event
	virtual bool		StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );

	// Manipulation of events for the object
	// Should be called by think function to process all scene events
	// The default implementation resets m_flexWeight array and calls
	//  AddSceneEvents
	virtual void		ProcessSceneEvents( void );

	// Assumes m_flexWeight array has been set up, this adds the actual currently playing
	//  expressions to the flex weights and adds other scene events as needed
	virtual	bool		ProcessSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );

	// Remove all playing events
	void				ClearSceneEvents( CChoreoScene *scene, bool canceled );

	// Stop specifics of event
	virtual	bool		ClearSceneEvent( CSceneEventInfo *info, bool fastKill, bool canceled );

	// Add the event to the queue for this actor
	void				AddSceneEvent( CChoreoScene *scene, CChoreoEvent *event, CBaseEntity *pTarget = nullptr, CSceneEntity* pSceneEntity = nullptr );

	// Remove the event from the queue for this actor
	void				RemoveSceneEvent( CChoreoScene *scene, CChoreoEvent *event, bool fastKill );

	// Checks to see if the event should be considered "completed"
	bool				CheckSceneEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	// Checks to see if a event should be considered "completed"
	virtual bool		CheckSceneEventCompletion( CSceneEventInfo *info, float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	// Finds the layer priority of the current scene
	int					GetScenePriority( CChoreoScene *scene );

	// Returns true if the actor is not currently in a scene OR if the actor
	//  is in a scene, but a PERMIT_RESPONSES event is active and the permit time
	//  period has enough time remaining to handle the response in full.
	bool				PermitResponse( float response_length );

	// Set response end time (0 to clear response blocking)
	void				SetPermitResponse( float endtime );

	void				SentenceStop( void ) { EmitSound( "Default.Null" ); }

	virtual float		PlayScene( const char *pszScene, float flDelay = 0.0f, AI_Response *response = nullptr, IRecipientFilter *filter = nullptr );
	virtual float		ScriptPlayScene( const char *pszScene, float flDelay = 0.0f );
	virtual float		PlayAutoGeneratedSoundScene( const char *soundname );
	// Returns the script instance of the scene entity associated with our oldest ("top level") scene event
	virtual HSCRIPT		ScriptGetOldestScene( void );
	virtual HSCRIPT		ScriptGetSceneByIndex( int index );

protected:
	// For handling .vfe files
	// Search list, or add if not in list
	const void			*FindSceneFile( const char *filename );

	// Find setting by name
	const flexsetting_t *FindNamedSetting( const flexsettinghdr_t *pSettinghdr, const char *expr );

	// Called at the lowest level to actually apply an expression
	void				AddFlexSetting( const char *expr, float scale, const flexsettinghdr_t *pSettinghdr, bool newexpression );

	// Called at the lowest level to actually apply a flex animation
	void				AddFlexAnimation( CSceneEventInfo *info );

	bool				HasSceneEvents() const;
	bool				IsRunningSceneMoveToEvent();

	LocalFlexController_t	FlexControllerLocalToGlobal( const flexsettinghdr_t *pSettinghdr, int key );

private:
	// Starting various expression types 

	bool RequestStartSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );
	bool RequestStartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );

	bool HandleStartSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor );
	bool HandleStartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor );
	bool StartFacingSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );
	bool StartMoveToSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );

	// Processing various expression types
	bool ProcessFlexAnimationSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessFlexSettingSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessFacingSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessMoveToSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessLookAtSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );

	// Set playing the scene sequence
public:
	bool EnterSceneSequence( CChoreoScene *scene, CChoreoEvent *event, bool bRestart = false );
private:
	bool ExitSceneSequence( void );

private:
	CNetworkArray( float, m_flexWeight, MAXSTUDIOFLEXCTRL, [[= ks::reflect::Net{ .bits = 12, .low = 0.0f, .high = 1.0f, .flags = SPROP_ROUNDDOWN } ]] );	// indexed by model local flexcontroller

	// Vector from actor to eye target
	CNetworkVector( m_viewtarget, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );

	// Blink state
	CNetworkVar( int, m_blinktoggle, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );

	// Array of active SceneEvents, in order oldest to newest
	std::vector< CSceneEventInfo >		m_SceneEvents;

	// Mapping for each loaded scene file used by this actor: local flex controller index -> global
	std::map< const flexsettinghdr_t *, std::vector< LocalFlexController_t > > m_LocalToGlobal;

	// The NPC is in a scene, but another .vcd (such as a short wave to say in response to the player doing something )
	//  can be layered on top of this actor (assuming duration matches, etc.
	float				m_flAllowResponsesEndTime;

	// List of actively playing scenes
	std::vector< CChoreoScene * >		m_ActiveChoreoScenes;
	bool				m_bUpdateLayerPriorities;

public:
	bool				IsSuppressedFlexAnimation( CSceneEventInfo *info );

private:
	// last time a foreground flex animation was played
	float				m_flLastFlexAnimationTime;

public:
	void DoBodyLean( void );

	void Teleport_Impl( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts = true );

};

EXTERN_SEND_TABLE(DT_BaseFlex);

class CBaseCombatWeapon;
class CBaseEntity;
class CBaseFlex;
class CNavArea;
class CScriptedTarget;
class CTakeDamageInfo;
struct animevent_t;
struct fogparams_t;
struct gamevcollisionevent_t;
struct impactdamagetable_t;
struct inputdata_t;
struct vphysics_objectstress_t;

using namespace ::ks::reflect;

// Purpose: Base combat character with no AI

typedef CHandle<CBaseCombatWeapon> CBaseCombatWeaponHandle;

// -------------------------------------
//  Capability Bits
// -------------------------------------

enum Capability_t 
{
	bits_CAP_MOVE_GROUND			= 0x00000001, // walk/run
	bits_CAP_MOVE_JUMP				= 0x00000002, // jump/leap
	bits_CAP_MOVE_FLY				= 0x00000004, // can fly, move all around
	bits_CAP_MOVE_CLIMB				= 0x00000008, // climb ladders
	bits_CAP_MOVE_CRAWL				= 0x00000010, // crawl
	bits_CAP_MOVE_SWIM				= 0x00000020, // navigate in water			// UNDONE - not yet implemented
	bits_CAP_MOVE_SHOOT				= 0x00000040, // tries to shoot weapon while moving
	bits_CAP_SKIP_NAV_GROUND_CHECK	= 0x00000080, // optimization - skips ground tests while computing navigation
	bits_CAP_USE					= 0x00000100, // open doors/push buttons/pull levers
	bits_CAP_NO_LOCAL_NAV_CRAWL		= 0x00000200, // Don't try to crawl when doing local navigation
	bits_CAP_AUTO_DOORS				= 0x00000400, // can trigger auto doors
	bits_CAP_OPEN_DOORS				= 0x00000800, // can open manual doors
	bits_CAP_TURN_HEAD				= 0x00001000, // can turn head, always bone controller 0
	bits_CAP_WEAPON_RANGE_ATTACK1	= 0x00002000, // can do a weapon range attack 1
	bits_CAP_WEAPON_RANGE_ATTACK2	= 0x00004000, // can do a weapon range attack 2
	bits_CAP_WEAPON_MELEE_ATTACK1	= 0x00008000, // can do a weapon melee attack 1
	bits_CAP_WEAPON_MELEE_ATTACK2	= 0x00010000, // can do a weapon melee attack 2
	bits_CAP_INNATE_RANGE_ATTACK1	= 0x00020000, // can do a innate range attack 1
	bits_CAP_INNATE_RANGE_ATTACK2	= 0x00040000, // can do a innate range attack 1
	bits_CAP_INNATE_MELEE_ATTACK1	= 0x00080000, // can do a innate melee attack 1
	bits_CAP_INNATE_MELEE_ATTACK2	= 0x00100000, // can do a innate melee attack 1
	bits_CAP_USE_WEAPONS			= 0x00200000, // can use weapons (non-innate attacks)
	//bits_CAP_STRAFE					= 0x00400000, // strafe ( walk/run sideways)
	bits_CAP_ANIMATEDFACE			= 0x00800000, // has animated eyes/face
	bits_CAP_USE_SHOT_REGULATOR		= 0x01000000, // Uses the shot regulator for range attack1
	bits_CAP_FRIENDLY_DMG_IMMUNE	= 0x02000000, // don't take damage from npc's that are D_LI
	bits_CAP_SQUAD					= 0x04000000, // can form squads
	bits_CAP_DUCK					= 0x08000000, // cover and reload ducking
	bits_CAP_NO_HIT_PLAYER			= 0x10000000, // don't hit players
	bits_CAP_AIM_GUN				= 0x20000000, // Use arms to aim gun, not just body
	bits_CAP_NO_HIT_SQUADMATES		= 0x40000000, // none
	bits_CAP_SIMPLE_RADIUS_DAMAGE	= 0x80000000, // Do not use robust radius damage model on this character.
};

inline constexpr auto bits_CAP_DOORS_GROUP = (bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS);
inline constexpr auto bits_CAP_RANGE_ATTACK_GROUP = (bits_CAP_WEAPON_RANGE_ATTACK1 | bits_CAP_WEAPON_RANGE_ATTACK2);
inline constexpr auto bits_CAP_MELEE_ATTACK_GROUP = (bits_CAP_WEAPON_MELEE_ATTACK1 | bits_CAP_WEAPON_MELEE_ATTACK2);
inline constexpr auto bits_CAP_MOVE_GROUP = (bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_MOVE_FLY | bits_CAP_MOVE_CLIMB | bits_CAP_MOVE_CRAWL | bits_CAP_MOVE_SWIM);

inline constexpr auto BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE = 0.9f;

enum Disposition_t 
{
	D_ER,		// Undefined - error
	D_HT,		// Hate
	D_FR,		// Fear
	D_LI,		// Like
	D_NU,		// Neutral

	// The following are duplicates of the above, only with friendlier names
	D_ERROR = D_ER,		// Undefined - error
	D_HATE = D_HT,		// Hate
	D_FEAR = D_FR,		// Fear
	D_LIKE = D_LI,		// Like
	D_NEUTRAL = D_NU,	// Neutral
};

const int DEF_RELATIONSHIP_PRIORITY = std::numeric_limits<int>::min();

struct Relationship_t
{
	EHANDLE			entity;			// Relationship to a particular entity
	Class_T			classType;		// Relationship to a class  CLASS_NONE = not class based (Def. in baseentity.h)
	int				faction;		// Relationship to a faction FACTION_NONE = not faction based

	Disposition_t	disposition;	// D_HT (Hate), D_FR (Fear), D_LI (Like), D_NT (Neutral)
	int				priority;		// Relative importance of this relationship (higher numbers mean more important)

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: This should contain all of the combat entry points / functionality 
// that are common between NPCs and players
//-----------------------------------------------------------------------------
// The two exclusive sub-tables are named as props of DT_BaseCombatCharacter, so both the tables
// and the proxies that gate them have to be visible where the annotation sits.
namespace DT_BCCLocalPlayerExclusive { extern SendTable g_SendTable; }
namespace DT_BCCNonLocalPlayerExclusive { extern SendTable g_SendTable; }
void *SendProxy_SendBaseCombatCharacterLocalDataTable( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
void *SendProxy_SendBaseCombatCharacterNonLocalDataTable( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );

class CBaseCombatCharacter : public CBaseFlex
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseCombatCharacter" } ]]
	[[= ks::reflect::SubTable<"bcc_localdata", &DT_BCCLocalPlayerExclusive::g_SendTable,
	SendProxy_SendBaseCombatCharacterLocalDataTable, true>{} ]]
	[[= ks::reflect::SubTable<"bcc_nonlocaldata", &DT_BCCNonLocalPlayerExclusive::g_SendTable,
	SendProxy_SendBaseCombatCharacterNonLocalDataTable, true>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_BCCLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_BCCNonLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::From<"m_flTimeOfLastInjury", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nRelativeDirectionOfLastInjury", ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseCombatCharacter, CBaseFlex );

public:
	CBaseCombatCharacter(void);
	~CBaseCombatCharacter(void);

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();

public:

	void		Spawn_Impl( void );

	void		Precache_Impl();

	virtual const impactdamagetable_t	&GetPhysicsImpactDamageTable( void );

	int					TakeHealth_Impl( float flHealth, int bitsDamageType );
	void				CauseDeath( const CTakeDamageInfo &info );

	bool		FVisible_Impl ( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = nullptr ); // true iff the parameter can be seen by me.
	bool		FVisible_Impl( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = nullptr )	{ return BaseClass::FVisible_Impl( vecTarget, traceMask, ppBlocker ); }
	static void			ResetVisibilityCache( CBaseCombatCharacter *pBCC = nullptr );

	virtual bool		FInViewCone( CBaseEntity *pEntity );
	virtual bool		FInViewCone( const Vector &vecSpot );

	virtual bool		FInAimCone( CBaseEntity *pEntity );
	virtual bool		FInAimCone( const Vector &vecSpot );
	
	virtual bool		ShouldShootMissTarget( CBaseCombatCharacter *pAttacker );
	virtual CBaseEntity *FindMissTarget( void );

	// Do not call HandleInteraction directly, use DispatchInteraction
	bool				DispatchInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )	{ return ( interactionType > 0 ) ? HandleInteraction( interactionType, data, sourceEnt ) : false; }
	virtual bool		HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt );

	virtual QAngle		BodyAngles();
	virtual Vector		BodyDirection2D( void );
	virtual Vector		BodyDirection3D( void );
	virtual Vector		HeadDirection2D( void )	{ return BodyDirection2D( ); }; // No head motion so just return body dir
	virtual Vector		HeadDirection3D( void )	{ return BodyDirection2D( ); }; // No head motion so just return body dir
	virtual Vector		EyeDirection2D( void ) 	{ return HeadDirection2D( );  }; // No eye motion so just return head dir
	virtual Vector		EyeDirection3D( void ) 	{ return HeadDirection3D( );  }; // No eye motion so just return head dir

	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	// -----------------------
	// Fog
	// -----------------------
	void				OnFogTriggerStartTouch( CBaseEntity *fogTrigger );
	void				OnFogTriggerEndTouch( CBaseEntity *fogTrigger );
	CBaseEntity *		GetFogTrigger( void );

	virtual bool		IsHiddenByFog( const Vector &target ) const;	///< return true if given target cant be seen because of fog
	virtual bool		IsHiddenByFog( CBaseEntity *target ) const;		///< return true if given target cant be seen because of fog
	virtual bool		IsHiddenByFog( float range ) const;				///< return true if given distance is too far to see through the fog
	virtual float		GetFogObscuredRatio( const Vector &target ) const;///< return 0-1 ratio where zero is not obscured, and 1 is completely obscured
	virtual float		GetFogObscuredRatio( CBaseEntity *target ) const;	///< return 0-1 ratio where zero is not obscured, and 1 is completely obscured
	virtual float		GetFogObscuredRatio( float range ) const;		///< return 0-1 ratio where zero is not obscured, and 1 is completely obscured
	virtual bool		GetFogParams( fogparams_t *fog ) const;			///< return the current fog parameters

	// -----------------------
	// Vision
	// -----------------------
	enum FieldOfViewCheckType { USE_FOV, DISREGARD_FOV };

	// Visible starts with line of sight, and adds all the extra game checks like fog, smoke, camo...
	bool IsAbleToSee( const CBaseEntity *entity, FieldOfViewCheckType checkFOV );
	bool IsAbleToSee( CBaseCombatCharacter *pBCC, FieldOfViewCheckType checkFOV );

	virtual bool IsLookingTowards( const CBaseEntity *target, float cosTolerance = BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE ) const;	// return true if our view direction is pointing at the given target, within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
	virtual bool IsLookingTowards( const Vector &target, float cosTolerance = BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE ) const;	// return true if our view direction is pointing at the given target, within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.

	virtual bool IsInFieldOfView( CBaseEntity *entity ) const;	// Calls IsLookingTowards with the current field of view.  
	virtual bool IsInFieldOfView( const Vector &pos ) const;

	enum LineOfSightCheckType
	{
		IGNORE_NOTHING,
		IGNORE_ACTORS
	};
	virtual bool IsLineOfSightClear( CBaseEntity *entity, LineOfSightCheckType checkType = IGNORE_NOTHING ) const;// strictly LOS check with no other considerations
	virtual bool IsLineOfSightClear( const Vector &pos, LineOfSightCheckType checkType = IGNORE_NOTHING, CBaseEntity *entityToIgnore = nullptr ) const;

	// -----------------------
	// Footsteps
	// -----------------------
	void PlayFootstepSound( const Vector& origin, bool leftFoot, bool feetInWater, bool kneesInWater, bool jumping = false );
	virtual void OnFootstep( const Vector& origin, bool leftFoot, bool feetInWater, bool kneesInWater, bool jumping ) {}

	// -----------------------
	// Ammo
	// -----------------------
	virtual int			GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound = false );
	int					GiveAmmo( int iCount, const char *szName, bool bSuppressSound = false );
	void				RemoveAmmo( int iCount, int iAmmoIndex );
	void				RemoveAmmo( int iCount, const char *szName );
	void				RemoveAllAmmo( );
	int					GetAmmoCount( int iAmmoIndex ) const;
	int					GetAmmoCount( char *szName ) const;

	virtual Activity	NPC_TranslateActivity( Activity baseAct );

	// -----------------------
	// Weapons
	// -----------------------
	CBaseCombatWeapon*	Weapon_Create( const char *pWeaponName );
	virtual Activity	Weapon_TranslateActivity( Activity baseAct, bool *pRequired = nullptr );
	void				Weapon_SetActivity( Activity newActivity, float duration );
	virtual void		Weapon_FrameUpdate( void );
	virtual void		Weapon_HandleAnimEvent( animevent_t *pEvent );
	virtual CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual bool		Weapon_CanUse( CBaseCombatWeapon *pWeapon );		// True is allowed to use this class of weapon
	virtual void		Weapon_Equip( CBaseCombatWeapon *pWeapon );			// Adds weapon to player
	virtual bool		Weapon_EquipAmmoOnly( CBaseCombatWeapon *pWeapon );	// Adds weapon ammo to player, leaves weapon
	bool				Weapon_Detach( CBaseCombatWeapon *pWeapon );		// Clear any pointers to the weapon.
	virtual void		Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = nullptr, const Vector *pVelocity = nullptr );
	virtual	bool		Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	virtual	Vector		Weapon_ShootPosition( );		// gun position at current position/orientation
	bool				Weapon_IsOnGround( CBaseCombatWeapon *pWeapon );
	CBaseEntity*		Weapon_FindUsable( const Vector &range );			// search for a usable weapon in this range
	virtual	bool		Weapon_CanSwitchTo(CBaseCombatWeapon *pWeapon);
	virtual bool		Weapon_SlotOccupied( CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon *Weapon_GetSlot( int slot ) const;
	virtual CBaseCombatWeapon *Weapon_GetPosition( int pos ) const;

	CBaseCombatWeapon	*Weapon_GetWpnForAmmo( int iAmmoIndex );
	virtual bool		ShouldPickupItemSilently( CBaseCombatCharacter *pNewOwner ) { return false; }

	// For weapon strip
	void				Weapon_DropAll( bool bDisallowWeaponPickup = false );

	virtual bool			AddPlayerItem( CBaseCombatWeapon *pItem ) { return false; }
	virtual bool			RemovePlayerItem( CBaseCombatWeapon *pItem ) { return false; }

	virtual bool			CanBecomeServerRagdoll( void ) { return true; }

	// -----------------------
	// Damage
	// -----------------------
	// Don't override this for characters, override the per-life-state versions below
	int				OnTakeDamage_Impl( const CTakeDamageInfo &info );

	// Override these to control how your character takes damage in different states
	virtual int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual int				OnTakeDamage_Dying( const CTakeDamageInfo &info );
	virtual int				OnTakeDamage_Dead( const CTakeDamageInfo &info );

	virtual float			GetAliveDuration( void ) const;			// return time we have been alive (only valid when alive)

	virtual void 			OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttacker ) {}
	virtual void 			NotifyFriendsOfDamage( CBaseEntity *pAttackerEntity ) {}
	virtual bool			HasEverBeenInjured( int team = TEAM_ANY ) const;			// return true if we have ever been injured by a member of the given team
	virtual float			GetTimeSinceLastInjury( int team = TEAM_ANY ) const;		// return time since we were hurt by a member of the given team
	RelativeDamagedDirection_t GetLastInjuryRelativeDirection( void ) { return m_nRelativeDirectionOfLastInjury; }

	uint32					GetLastDamageTypeFlags( void ) const { return m_uiLastDamageTypeFlags; }

	virtual void			OnPlayerKilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info ) {}

	// utility function to calc damage force
	virtual Vector			CalcDeathForceVector( const CTakeDamageInfo &info );

	int				BloodColor_Impl();
	virtual Activity		GetDeathActivity( void );

	virtual bool			CorpseGib( const CTakeDamageInfo &info );
	virtual void			CorpseFade( void );	// Called instead of GibNPC() when gibs are disabled
	virtual bool			HasHumanGibs( void );
	virtual bool			HasAlienGibs( void );
	virtual bool			ShouldGib( const CTakeDamageInfo &info ) { return false; }	// Always ragdoll, unless specified by the leaf class

	float GetDamageAccumulator() { return m_flDamageAccumulator; }
	int	  GetDamageCount( void ) { return m_iDamageCount; }	// # of times NPC has been damaged.  used for tracking 1-shot kills.

	// Character killed (only fired once)
	void			Event_Killed_Impl( const CTakeDamageInfo &info );
	virtual bool			ShouldDropActiveWeaponWhenKilled() { return true; }

	// Killed a character
	[[= ks::reflect::Input{ .name = "KilledNPC", .type = FIELD_VOID } ]] void InputKilledNPC( inputdata_t &inputdata );
	virtual void OnKilledNPC( CBaseCombatCharacter *pKilled ) {}; 

	// Exactly one of these happens immediately after killed (gibbed may happen later when the corpse gibs)
	// Character gibbed or faded out (violence controls) (only fired once)
	// returns true if gibs were spawned
	virtual bool			Event_Gibbed( const CTakeDamageInfo &info );
	// Character entered the dying state without being gibbed (only fired once)
	virtual void			Event_Dying( void );
	// character died and should become a ragdoll now
	// return true if converted to a ragdoll, false to use AI death
	virtual bool			BecomeRagdoll( const CTakeDamageInfo &info, const Vector &forceVector );
	virtual void			FixupBurningServerRagdoll( CBaseEntity *pRagdoll );

	virtual bool			BecomeRagdollBoogie( CBaseEntity *pKiller, const Vector &forceVector, float duration, int flags );

	CBaseEntity				*FindHealthItem( const Vector &vecPosition, const Vector &range );

	virtual CBaseEntity		*CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float forceScale = 1.0f, bool bDamageAnyNPC = false );
	virtual CBaseEntity		*CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float flForceScale = 1.0f, bool bDamageAnyNPC = false );

	CBaseCombatCharacter *MyCombatCharacterPointer_Impl( void ) { return this; }

	// VPHYSICS
	void			VPhysicsShadowCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	void			VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	float					CalculatePhysicsStressDamage( vphysics_objectstress_t *pStressOut, IPhysicsObject *pPhysics );
	void					ApplyStressDamage( IPhysicsObject *pPhysics, bool bRequireLargeObject );

	virtual void			PushawayTouch( CBaseEntity *pOther ) {}

	void SetImpactEnergyScale( float fScale ) { m_impactEnergyScale = fScale; }

	void			UpdateOnRemove_Impl( void );

	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );
	virtual int				IRelationPriority( CBaseEntity *pTarget );

	virtual void			SetLightingOriginRelative( CBaseEntity *pLightingOrigin );

protected:
	Relationship_t			*FindEntityRelationship( CBaseEntity *pTarget );

public:
	
	// Vehicle queries
	virtual bool ExitVehicle( void ) { return false; }

	// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
	void SetBloodColor( int nBloodColor );

	// Weapons..
	CBaseCombatWeapon*	GetActiveWeapon() const;
	int					WeaponCount() const;
	CBaseCombatWeapon*	GetWeapon( int i ) const;
	bool				RemoveWeapon( CBaseCombatWeapon *pWeapon );
	virtual void		RemoveAllWeapons();
	virtual void		RemoveAllWearables( void );
	virtual void		RemoveWeaponOnPlayer( CBaseCombatWeapon *pWeapon );
	WeaponProficiency_t GetCurrentWeaponProficiency() { return m_CurrentWeaponProficiency; }
	void				SetCurrentWeaponProficiency( WeaponProficiency_t iProficiency ) { m_CurrentWeaponProficiency = iProficiency; }
	virtual WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );
	virtual	Vector		GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = nullptr );
	virtual	float		GetSpreadBias(  CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget );
	virtual void		DoMuzzleFlash();

	// Interactions
	static void			InitInteractionSystem();

	// Relationships
	static void			SetDefaultFactionRelationship(int nFaction, int nFactionTarget, Disposition_t nDisposition, int nPriority);
	Disposition_t		GetFactionRelationshipDisposition( int nFaction );
	static void			AllocateDefaultRelationships( );
	static void			AllocateDefaultFactionRelationships( );
	static void			SetDefaultRelationship( Class_T nClass, Class_T nClassTarget,  Disposition_t nDisposition, int nPriority );
	Disposition_t		GetDefaultRelationshipDisposition( Class_T nClassTarget );
	virtual void		AddEntityRelationship( CBaseEntity *pEntity, Disposition_t nDisposition, int nPriority );
	virtual bool		RemoveEntityRelationship( CBaseEntity *pEntity );
	virtual void		AddClassRelationship( Class_T nClass, Disposition_t nDisposition, int nPriority );
	virtual void		AddFactionRelationship(int nFaction, Disposition_t nDisposition, int nPriority);

	// Factions
	static int			GetNumFactions( void );
	static std::vector< EHANDLE > *GetEntitiesInFaction( int nFaction );
	int					GetFaction( void ) const { return m_nFaction; }
	virtual void		ChangeFaction( int nNewFaction );

	void		ChangeTeam_Impl( int iTeamNum );

	// Nav hull type

	// FIXME: The following 3 methods are backdoor hack methods
	
	// This is a sort of hack back-door only used by physgun!
	void SetAmmoCount( int iCount, int iAmmoIndex );

	// This is a hack to blat out the current active weapon...
	// Used by weapon_slam + game_ui
	void SetActiveWeapon( CBaseCombatWeapon *pNewWeapon );
	void ClearActiveWeapon() { SetActiveWeapon( nullptr ); }
	virtual void OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon ) {}

	// I can't use my current weapon anymore. Switch me to the next best weapon.
	bool SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent);

	// This is a hack to copy the relationship strings used by monstermaker
	void SetRelationshipString( string_t theString ) { m_RelationshipString = theString; }

	float				GetNextAttack() const { return m_flNextAttack; }
	void				SetNextAttack( float flWait ) { m_flNextAttack = flWait; }

	bool				m_bForceServerRagdoll;

	// Pickup prevention
	bool				IsAllowedToPickupWeapons( void ) { return !m_bPreventWeaponPickup; }
	void				SetPreventWeaponPickup( bool bPrevent ) { m_bPreventWeaponPickup = bPrevent; }
	bool				m_bPreventWeaponPickup;

	virtual CNavArea *GetLastKnownArea( void ) const		{ return m_lastNavArea; }		// return the last nav area the player occupied - NULL if unknown
	virtual bool IsAreaTraversable( const CNavArea *area ) const;							// return true if we can use the given area 
	virtual void ClearLastKnownArea( void );
	virtual void UpdateLastKnownArea( void );										// invoke this to update our last known nav area (since there is no think method chained to CBaseCombatCharacter)
	virtual void OnNavAreaChanged( CNavArea *enteredArea, CNavArea *leftArea ) { }	// invoked (by UpdateLastKnownArea) when we enter a new nav area (or it is reset to NULL)
	virtual void OnNavAreaRemoved( CNavArea *removedArea );

	// -----------------------
	// Notification from INextBots.
	// -----------------------
	virtual void OnPursuedBy( INextBot * __restrict__ pPursuer ){} // called every frame while pursued by a bot in DirectChase.

public:
	// returns the last body region that took damage
	int	LastHitGroup() const				{ return m_LastHitGroup; }
protected:
	void SetLastHitGroup( int nHitGroup )	{ m_LastHitGroup = nHitGroup; }

public:
	CNetworkVar( float, m_flNextAttack, [[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Net{ .table = "DT_BCCLocalPlayerExclusive" } ]] );			// cannot attack again until this time

private:

protected:
	[[= ks::reflect::Key{ .name = "BloodColor" } ]] int			m_bloodColor;			// color of blood particless

	// -------------------
	// combat ability data
	// -------------------
	float		m_flFieldOfView;		// cosine of field of view for this character
	Vector		m_HackedGunPos;			// HACK until we can query end of gun
	[[= ks::reflect::Key{ .name = "Relationship" } ]] string_t	m_RelationshipString;	// Used to load up relationship keyvalues
	[[= ks::reflect::Key{ .name = "physdamagescale", .input = true } ]] float		m_impactEnergyScale;// scale the amount of energy used to calculate damage this ent takes due to physics

	byte		m_weaponIDToIndex[WEAPON_MAX];

public:
	static int					GetInteractionID();	// Returns the next interaction #
	
	float		GetTimeOfLastInjury( void ) const {  return m_flTimeOfLastInjury; };

protected:
	// Visibility-related stuff
	bool ComputeLOS( const Vector &vecEyePosition, const Vector &vecTarget ) const;
private:
	bool ComputeTargetIsInDarkness( const Vector &vecEyePosition, CNavArea *pTargetNavArea, const Vector &vecTargetPos ) const;

protected:
	// For weapon strip
	void ThrowDirForWeaponStrip( CBaseCombatWeapon *pWeapon, const Vector &vecForward, Vector *pVecThrowDir );
	void DropWeaponForWeaponStrip( CBaseCombatWeapon *pWeapon, const Vector &vecForward, const QAngle &vecAngles, float flDiameter );

protected:
	friend class CScriptedTarget; // needs to access GetInteractionID()
	
	static int					m_lastInteraction;	// Last registered interaction #
	static Relationship_t**		m_DefaultRelationship;
	static Relationship_t**		m_FactionRelationship;
	static std::vector< std::vector< EHANDLE > > m_aFactions;

public:
	// attack/damage
	CNetworkVar( int, m_LastHitGroup, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );			// the last body region that took damage

private:
	float				m_flDamageAccumulator;	// so very small amounts of damage do not get lost.
	int					m_iDamageCount;			// # of times NPC has been damaged.  used for tracking 1-shot kills.
	
	// Weapon proficiency gets calculated each time an NPC changes his weapon, and then
	// cached off as the CurrentWeaponProficiency.
	WeaponProficiency_t m_CurrentWeaponProficiency;

	// ---------------
	//  Relationships
	// ---------------
	std::vector< Relationship_t >		m_Relationship;						// Array of relationships
	int								m_nFaction;

	// Used by trigger_fog to manage when the character is touching multiple fog triggers simultaneously.
	// The one at the HEAD of the list is always the current fog trigger for the character.
	std::vector< EHANDLE > m_hTriggerFogList;
	EHANDLE m_hLastFogTrigger;
protected:
	// shared ammo slots
	CNetworkArrayForDerived( int, m_iAmmo, MAX_AMMO_TYPES );

	// Usable character items 
	CNetworkArray( CBaseCombatWeaponHandle, m_hMyWeapons, MAX_WEAPONS, [[= ks::reflect::Net{} ]] );
	CNetworkHandle( CBaseCombatWeapon, m_hActiveWeapon, [[= ks::reflect::Net{} ]] );

	IntervalTimer m_aliveTimer;

	unsigned int m_hasBeenInjured;							// bitfield corresponding to team ID that did the injury	

	// we do this because MAX_TEAMS is 32, which is wasteful for most games
	enum { MAX_DAMAGE_TEAMS = 4 };
	struct DamageHistory
	{
		int team;					// which team hurt us (TEAM_INVALID means slot unused)
		IntervalTimer interval;		// how long has it been
	};
	DamageHistory m_damageHistory[ MAX_DAMAGE_TEAMS ];

	CNetworkVar( float, m_flTimeOfLastInjury );
	CNetworkVar( RelativeDamagedDirection_t, m_nRelativeDirectionOfLastInjury );

	// CNetworkVar( uint32, m_uiLastDamageTypeFlags );
	uint32 m_uiLastDamageTypeFlags;

	// last known navigation area of player - NULL if unknown
	CNavArea *m_lastNavArea;
	int m_registeredNavTeam;	// ugly, but needed to clean up player team counts in nav mesh
	
	friend class CCleanupDefaultRelationShips;
};

EXTERN_SEND_TABLE(DT_BaseCombatCharacter);

void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTraceFilterMelee : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CTraceFilterMelee );
	
	CTraceFilterMelee( const IHandleEntity *passentity, int collisionGroup, CTakeDamageInfo *dmgInfo, float flForceScale, bool bDamageAnyNPC )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_dmgInfo(dmgInfo), m_pHit(nullptr), m_flForceScale(flForceScale), m_bDamageAnyNPC(bDamageAnyNPC)
	{
	}
	
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

public:
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CTakeDamageInfo		*m_dmgInfo;
	CBaseEntity			*m_pHit;
	float				m_flForceScale;
	bool				m_bDamageAnyNPC;
};

class CBaseEntity;

using namespace ::ks::reflect;

// Particle system entities can derive from this to handle some of the mundane
// functionality of hooking into the engine's entity system.

class CBaseParticleEntity : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseParticleEntity" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseParticleEntity, CBaseEntity );
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	CBaseParticleEntity();
	virtual ~CBaseParticleEntity();

	// CBaseEntity overrides.
public:
	 int		UpdateTransmitState_Impl( void );	

	 void	Activate_Impl();
	 void	Think_Impl();	

public:
	void			FollowEntity(CBaseEntity *pEntity);
	
	// UTIL_Remove will be called after the specified amount of time.
	// If you pass in -1, the entity will never go away automatically.
	void			SetLifetime(float lifetime);

private:
	CBaseParticleEntity( const CBaseParticleEntity & ); // not defined, not accessible
};

class CBaseEntity;

// PlayerUse defines

typedef CHandle<CBaseEntity> EHANDLE;
inline constexpr auto PLAYER_USE_RADIUS = 80.f;
inline constexpr auto PLAYER_USE_BOT_RADIUS = 140.f;

inline constexpr auto CONE_45_DEGREES = 0.707f;
inline constexpr auto CONE_15_DEGREES = 0.9659258f;
inline constexpr auto CONE_90_DEGREES = 0;

// entity messages
inline constexpr auto UPDATE_PLAYER_RADAR = 2;

inline constexpr auto DEATH_ANIMATION_TIME = 3.0f;

// multiplayer only
inline constexpr auto NOINTERP_PARITY_MAX = 4;
inline constexpr auto NOINTERP_PARITY_MAX_BITS = 2;

struct autoaim_params_t
{
	autoaim_params_t();

	Vector		m_vecAutoAimDir;		// Output: The direction autoaim wishes to point.
	Vector		m_vecAutoAimPoint;		// Output: The point (world space) that autoaim is aiming at.
	EHANDLE		m_hAutoAimEntity;		// Output: The entity that autoaim is aiming at.
	float		m_fScale;				// Input:
	float		m_fMaxDist;				// Input:
	float		m_fMaxDeflection;		// Input:
	bool		m_bOnTargetQueryOnly;	// Input: Don't do expensive assistance, just resolve m_bOnTargetNatural
	bool		m_bAutoAimAssisting;	// Output: If this is true, autoaim is aiming at the target.
	bool		m_bOnTargetNatural;		// Output: If true, the player is on target without assistance.
};

enum stepsoundtimes_t
{
	STEPSOUNDTIME_NORMAL = 0,
	STEPSOUNDTIME_ON_LADDER,
	STEPSOUNDTIME_WATER_KNEE,
	STEPSOUNDTIME_WATER_FOOT,
};

//
// Player PHYSICS FLAGS bits
//
enum PlayerPhysFlag_e
{
	PFLAG_DIROVERRIDE	= ( 1<<0 ),		// override the player's directional control (trains, physics gun, etc.)
	PFLAG_DUCKING		= ( 1<<1 ),		// In the process of ducking, but totally squatted yet
	PFLAG_USING			= ( 1<<2 ),		// Using a continuous entity
	PFLAG_OBSERVER		= ( 1<<3 ),		// player is locked in stationary cam mode. Spectators can move, observers can't.
	PFLAG_VPHYSICS_MOTIONCONTROLLER = ( 1<<4 ),	// player is physically attached to a motion controller
	PFLAG_GAMEPHYSICS_ROTPUSH = (1<<5), // game physics did a rotating push that we may want to override with vphysics

	// If you add another flag here check that you aren't 
	// overwriting phys flags in the HL2 of TF2 player classes
};

enum
{
	VPHYS_WALK = 0,
	VPHYS_CROUCH,
	VPHYS_NOCLIP,
};

// useful cosines
inline constexpr auto DOT_1DEGREE = 0.9998476951564;
inline constexpr auto DOT_2DEGREE = 0.9993908270191;
inline constexpr auto DOT_3DEGREE = 0.9986295347546;
inline constexpr auto DOT_4DEGREE = 0.9975640502598;
inline constexpr auto DOT_5DEGREE = 0.9961946980917;
inline constexpr auto DOT_6DEGREE = 0.9945218953683;
inline constexpr auto DOT_7DEGREE = 0.9925461516413;
inline constexpr auto DOT_8DEGREE = 0.9902680687416;
inline constexpr auto DOT_9DEGREE = 0.9876883405951;
inline constexpr auto DOT_10DEGREE = 0.9848077530122;
inline constexpr auto DOT_15DEGREE = 0.9659258262891;
inline constexpr auto DOT_20DEGREE = 0.9396926207859;
inline constexpr auto DOT_25DEGREE = 0.9063077870367;
inline constexpr auto DOT_30DEGREE = 0.866025403784;
inline constexpr auto DOT_45DEGREE = 0.707106781187;

//#define DEBUG_MOTION_CONTROLLERS  //uncomment to spew debug data while in a motion controller

enum HltvUiType_t
{
	HLTV_UI_XRAY_ON = 0,
	HLTV_UI_XRAY_OFF,
	HLTV_UI_SCOREBOARD_ON,
	HLTV_UI_SCOREBOARD_OFF,
	HLTV_UI_OVERVIEW_ON,
	HLTV_UI_OVERVIEW_OFF,
	HLTV_UI_GRAPHS_ON,
	HLTV_UI_GRAPHS_OFF
};

// Shared header file for players

// This is the base class for TEMP ENTITIES that use the 
//  event system to propagate
class CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseTempEntity", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CBaseTempEntity );
	DECLARE_SERVERCLASS();

						CBaseTempEntity( const char *name );
	virtual				~CBaseTempEntity( void );

	const char			*GetName( void );

	// Force all derived classes to implement a test
	virtual void		Test( const Vector& current_origin, const QAngle& current_angles );

	virtual	void		Create( IRecipientFilter& filter, float delay = 0.0 );

	virtual void		Precache( void );

	CBaseTempEntity		*GetNext( void );

	// Get list of tempentities
	static CBaseTempEntity *GetList( void );

	// Called at startup to allow temp entities to precache any models/sounds that they need
	static void			PrecacheTempEnts( void );

	void NetworkStateChanged() {}	// TE's are sent out right away so we don't track whether state changes or not,
									// but we want to allow CNetworkVars.
	void NetworkStateChanged( void *pVar ) {}

private:
	// Descriptive name, for when running tests
	const char			*m_pszName;

	// Next in chain
	CBaseTempEntity		*m_pNext;

	// ConVars add themselves to this list for the executable. Then ConVarMgr::Init() runs through 
	// all the console variables and registers them.
	static CBaseTempEntity	*s_pTempEntities;
};

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: For the slow removing of the CBaseToggle entity
//			only old entities that need it for backwards-compatibility should
//			include this file


class CBaseToggle : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseToggle" } ]]
	[[= ks::reflect::From<"m_vecFinalDest", ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_movementType", ks::reflect::Net{ .bits = -1 }>{} ]]
	[[= ks::reflect::From<"m_flMoveTargetTime", ks::reflect::Net{ .bits = 32 }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseToggle, CBaseEntity );
	DECLARE_SERVERCLASS();

public:
	CBaseToggle();

	using BaseClass::KeyValue_Impl;
	bool		KeyValue_Impl( const char *szKeyName, const char *szValue );
	bool		KeyValue_Impl( const char *szKeyName, float flValue ) { return BaseClass::KeyValue_Impl( szKeyName, flValue ); };

	TOGGLE_STATE		m_toggle_state;
	float				m_flMoveDistance;// how far a door should slide or rotate
	float				m_flWait;
	float				m_flLip;

	Vector				m_vecPosition1;
	Vector				m_vecPosition2;

	QAngle				m_vecMoveAng;
	QAngle				m_vecAngle1;
	QAngle				m_vecAngle2;

	float				m_flHeight;
	EHANDLE				m_hActivator;
	CNetworkVector(	m_vecFinalDest );
	QAngle				m_vecFinalAngle;

	CNetworkVar( int, m_movementType );

	CNetworkVar( float, m_flMoveTargetTime ); //absolute time, not local time

	DECLARE_DATADESC();

	float	GetDelay_Impl( void ) { return m_flWait; }

	// common member functions
	void LinearMove( const Vector &vecDest, float flSpeed );
	void LinearMoveDone( void );
	void AngularMove( const QAngle &vecDestAngle, float flSpeed );
	void AngularMoveDone( void );
	bool IsLockedByMaster_Impl( void );
	void MoveDone_Impl( void );

	void GetGroundVelocityToApply_Impl( Vector &vecGroundVel );

	static float AxisValue( int flags, const QAngle &angles );
	void AxisDir( void );
	static float AxisDelta( int flags, const QAngle &angle1, const QAngle &angle2 );

	string_t m_sMaster;		// If this button has a master switch, this is the targetname.
							// A master switch must be of the multisource type. If all 
							// of the switches in the multisource have been triggered, then
							// the button will be allowed to operate. Otherwise, it will be
							// deactivated.
};

// Purpose: Arbitrary length bit string
//				** NOTE: This class does NOT override the bitwise operators
//						 as doing so would require overriding the operators
//						 to allocate memory for the returned bitstring.  This method
//						 would be prone to memory leaks as the calling party
//						 would have to remember to delete the memory.  Funtions
//						 are used instead to require the calling party to allocate
//						 and destroy their own memory

class CBaseAnimating;

class CBoneMergeCache
{
public:

	CBoneMergeCache();
	
	void Init( CBaseAnimating *pOwner );

	// Updates the lookups that let it merge bones quickly.
	void UpdateCache();
	
	// This copies the transform from all bones in the followed entity that have 
	// names that match our bones.
	void BuildMatricesWithBoneMerge( const CStudioHdr *pStudioHdr, const QAngle& angles, 
		const Vector& origin, const Vector pos[MAXSTUDIOBONES],
		const Quaternion q[MAXSTUDIOBONES], matrix3x4_t bonetoworld[MAXSTUDIOBONES],
		CBaseAnimating *pParent, CBoneCache *pParentCache, int boneMask );
	
	void MergeMatchingPoseParams( void );

	void CopyFromFollow( const BoneVector followPos[], const BoneQuaternion followQ[], int boneMask, BoneVector myPos[], BoneQuaternion myQ[] );
	void CopyToFollow( const BoneVector myPos[], const BoneQuaternion myQ[], int boneMask, BoneVector followPos[], BoneQuaternion followQ[] );
	bool IsCopied( );

	// Returns true if the specified bone is one that gets merged in MergeMatchingBones.
	int IsBoneMerged( int iBone ) const;

	// Gets the origin for the first merge bone on the parent.
	bool GetAimEntOrigin( Vector *pAbsOrigin, QAngle *pAbsAngles );
	bool GetRootBone( matrix3x4_t &rootBone );

	void ForceCacheClear( void ) { m_bForceCacheClear = true; UpdateCache(); }

	const unsigned short *GetRawIndexMapping( void ) { return &m_iRawIndexMapping[0]; }

protected:

	// This is the entity that we're keeping the cache updated for.
	CBaseAnimating *m_pOwner;

	// All the cache data is based off these. When they change, the cache data is regenerated.
	// These are either all valid pointers or all NULL.
	CBaseAnimating *m_pFollow;
	CStudioHdr		*m_pFollowHdr;
	const studiohdr_t *m_pFollowRenderHdr;
	CStudioHdr		*m_pOwnerHdr;
	const studiohdr_t *m_pOwnerRenderHdr;

	// keeps track if this entity is part of a reverse bonemerge
	int				m_nCopiedFramecount;

	// This is the mask we need to use to set up bones on the followed entity to do the bone merge
	int				m_nFollowBoneSetupMask;

	// Cache data.
	class CMergedBone
	{
	public:
		unsigned short m_iMyBone;		// index of MergeCache's owner's bone
		unsigned short m_iParentBone;	// index of m_pFollow's matching bone
	};

	// This is an array of pose param indices on the follower to pose param indices on the owner
	int				m_nOwnerToFollowPoseParamMapping[MAXSTUDIOPOSEPARAM];

	std::vector< CMergedBone > m_MergedBones;
	std::vector< bool > m_BoneMergeBits;	// One bit for each bone. The bit is set if the bone gets merged.

	unsigned short m_iRawIndexMapping[ MAXSTUDIOBONES ];

	bool m_bForceCacheClear;
};

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;


class CBaseButton : public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseButton" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CBaseButton, CBaseToggle );
	DECLARE_SERVERCLASS();

	void Spawn_Impl( void );
	void Precache_Impl( void );
	bool CreateVPhysics_Impl();
	void RotSpawn( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	int DrawDebugTextOverlays_Impl();

protected:

	void ButtonActivate( );
	void SparkSoundCache( void );

	void ButtonTouch( ::CBaseEntity *pOther );
	void ButtonSpark ( void );
	void TriggerAndWait( void );
	void ButtonReturn( void );
	void ButtonBackHome( void );
	void ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	bool OnUseLocked( CBaseEntity *pActivator );

	virtual void Lock();
	virtual void Unlock();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Lock", .type = FIELD_VOID } ]] void InputLock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Press", .type = FIELD_VOID } ]] void InputPress( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PressIn", .type = FIELD_VOID } ]] void InputPressIn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "PressOut", .type = FIELD_VOID } ]] void InputPressOut( inputdata_t &inputdata );

	public: int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	protected:
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN, BUTTON_PRESS };

	BUTTON_CODE	ButtonResponseToTouch( void );
	void Press( CBaseEntity *pActivator, BUTTON_CODE eCode );
	
	DECLARE_DATADESC();

	public: int	ObjectCaps_Impl(void);
	protected:

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector m_vecMoveDir;

	bool	m_fStayPushed;		// button stays pushed in until touched again?
	bool	m_fRotating;		// a rotating button?  default is a sliding button.

	locksound_t m_ls;			// door lock sounds
	
	byte	m_bLockedSound;		// ordinals from entity selection
	byte	m_bLockedSentence;	
	byte	m_bUnlockedSound;	
	byte	m_bUnlockedSentence;
	bool	m_bLocked;
	[[= ks::reflect::Key{ .name = "sounds" } ]] int		m_sounds;
	float	m_flUseLockedTime;		// Controls how often we fire the OnUseLocked output.

	bool	m_bSolidBsp;

	string_t	m_sNoise;			// The actual WAV file name of the sound.

	[[= ks::reflect::Key{ .name = "OnDamaged" } ]] COutputEvent m_OnDamaged;
	[[= ks::reflect::Key{ .name = "OnPressed" } ]] COutputEvent m_OnPressed;
	[[= ks::reflect::Key{ .name = "OnUseLocked" } ]] COutputEvent m_OnUseLocked;
	[[= ks::reflect::Key{ .name = "OnIn" } ]] COutputEvent m_OnIn;
	[[= ks::reflect::Key{ .name = "OnOut" } ]] COutputEvent m_OnOut;

	int		m_nState;
};

//
// Rotating button (aka "lever")
//
class CRotButton : public CBaseButton
{
public:
	DECLARE_CLASS( CRotButton, CBaseButton );

	void Spawn_Impl( void );
	bool CreateVPhysics_Impl( void );

};

class CMomentaryRotButton : public CRotButton
{
	[[= ks::reflect::KeyFrom<"m_bSolidBsp", ks::reflect::Key{ .name = "solidbsp" } >{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CMomentaryRotButton, CRotButton );

public:
	void	Spawn_Impl ( void );
	bool	CreateVPhysics_Impl( void );
	int	ObjectCaps_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UseMoveDone( void );
	void	ReturnMoveDone( void );
	void	OutputMovementComplete(void);
	void	SetPositionMoveDone(void);
	void	UpdateSelf( float value, bool bPlaySound );

	void	PlaySound( void );
	void	UpdateTarget( float value, CBaseEntity *pActivator );

	int		DrawDebugTextOverlays_Impl(void);

	static CMomentaryRotButton *Instance( edict_t *pent ) { return (CMomentaryRotButton *)GetContainingEntity(pent); }

	float GetPos(const QAngle &vecAngles);

	DECLARE_DATADESC();

	virtual void Lock();
	virtual void Unlock();

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetPosition", .type = FIELD_FLOAT } ]] void InputSetPosition( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPositionImmediately", .type = FIELD_FLOAT } ]] void InputSetPositionImmediately( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "_DisableUpdateTarget", .type = FIELD_VOID } ]] void InputDisableUpdateTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "_EnableUpdateTarget", .type = FIELD_VOID } ]] void InputEnableUpdateTarget( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	virtual void Enable( void );
	virtual void Disable( void );

	bool	m_bDisabled;

	[[= ks::reflect::Key{ .name = "Position" } ]] COutputFloat m_Position;
	[[= ks::reflect::Key{ .name = "OnUnpressed" } ]] COutputEvent m_OnUnpressed;
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;
	[[= ks::reflect::Key{ .name = "OnReachedPosition" } ]] COutputEvent m_OnReachedPosition;

	int			m_lastUsed;
	QAngle		m_start;
	QAngle		m_end;
	float		m_IdealYaw;
	string_t	m_sNoise;

	bool		m_bUpdateTarget;		// Used when jiggling so that we don't jiggle the target (door, etc)

	[[= ks::reflect::Key{ .name = "StartDirection" } ]] int			m_direction;
	[[= ks::reflect::Key{ .name = "returnspeed" } ]] float		m_returnSpeed;
	[[= ks::reflect::Key{ .name = "StartPosition" } ]] float		m_flStartPosition;

protected:

	void UpdateThink( void );
};

//--------------------------------------------------------------------------------------------------------
//
//	CButtonTimed - func_button_timed
//
//--------------------------------------------------------------------------------------------------------

class CButtonTimed;   // declared only: it was never implemented

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;


// Purpose: Base class for simple projectiles

//=============================================================================
//=============================================================================
class CBaseProjectile : public CBaseAnimating
{
	DECLARE_CLASS( CBaseProjectile, CBaseAnimating );

public:
	void Touch_Impl( CBaseEntity *pOther );
	virtual void HandleTouch( CBaseEntity *pOther );

	void Think_Impl();
	virtual void HandleThink();

	void Spawn(	char *pszModel,
		const Vector &vecOrigin,
		const Vector &vecVelocity,
		edict_t *pOwner,
		MoveType_t	iMovetype,
		MoveCollide_t nMoveCollide,
		int	iDamage,
		int iDamageType,
		CBaseEntity *pIntendedTarget = nullptr );

	void Precache_Impl( void ) {};

	int	m_iDmg;
	int m_iDmgType;
	EHANDLE m_hIntendedTarget;
};

class CBasePlayer;


void ClientActive( edict_t *pEdict, bool bLoadGame );
void ClientFullyConnect( edict_t *pEdict );
void ClientPutInServer( edict_t *pEdict, const char *playername );
void ClientCommand( CBasePlayer *pSender, const CCommand &args );
void ClientPrecache( void );
// Game specific precaches
void ClientGamePrecache( void );
const char *GetGameDescription( void );
void Host_Say( edict_t *pEdict, bool teamonly );

class CBaseEntity;


//
// A variant class for use in response rule contexts.
// It provides implicit conversion between types.
// So, you can construct it as a string, but fetch it
// as an int, and so on.
// It caches these conversions to make them faster afterwards.
// If you construct from a string, it will make a copy of 
// that string, so it's okay to pass in extemporaneous stack strings.
//
class coerciblevariant_t
{
	// each of the types we store
	bool bVal;
	int iVal;
	float flVal;
	char * szVal; // generally this one is only computed on demand (because it eats memory)
	CHandle<CBaseEntity> eVal; // this can't be in the union because it has a constructor.

	// my native type -- but I can be implicitly converted to others.
	fieldtype_t fieldType;

	// store which types have been initialized
	enum 
	{
		kINIT_BOOLEAN = ( 1 << 0 ),
		kINIT_INT	  = ( 1 << 1 ),
		kINIT_FLOAT	  = ( 1 << 2 ),
		kINIT_STRING  = ( 1 << 3 ),
		kINIT_EHANDLE = ( 1 << 4 ),
	};
	unsigned int m_bvInitFields; 

	enum
	{
		DEFAULT_VARIANT_STRING_SIZE = 64 // how big my allocated strings are, when I make them.
	};

public:

	// constructor
	coerciblevariant_t() : fieldType(FIELD_VOID), iVal(0), bVal(false), szVal(nullptr), flVal(0), m_bvInitFields(0) {}
	~coerciblevariant_t();
	// assignment ctors (private because not used yet, please don't use them
private:
	coerciblevariant_t(const coerciblevariant_t &src);
	coerciblevariant_t& operator=(const coerciblevariant_t &src);
public:

	// convenience constructors
	coerciblevariant_t( bool b );
	coerciblevariant_t( const char * str );
	coerciblevariant_t( int i );
	coerciblevariant_t( float f );
	coerciblevariant_t( const EHANDLE &handle );
	coerciblevariant_t( CBaseEntity *ent );

	inline bool Bool( void ) ;// const						{ return( fieldType == FIELD_BOOLEAN ) ? bVal : false; }
	const char *String( void ) ;// const				{ return( fieldType == FIELD_STRING ) ? STRING(iszVal) : ToString(); }
	inline int Int( void ) ;// const						{ return( fieldType == FIELD_INTEGER ) ? iVal : 0; }
	inline float Float( void ) ;// const					{ return( fieldType == FIELD_FLOAT ) ? flVal : 0; }
	inline const CHandle<CBaseEntity> &Entity(void) ;// const;

	fieldtype_t FieldType( void ) { return fieldType; }

	void SetBool( bool b );
	void SetString( const char * str );
	void SetInt( int val );
	void SetFloat( float val );
	void SetEntity( CBaseEntity *val );

protected:

	// from my native type, make a float value if possible
	float ConvertFloat() const;

	// from my native type, make an int value if possible
	int ConvertInt() const;

	// from my native type, make a bool value if possible
	bool ConvertBool() const;

	// from my native type, make an entity if possible, NULL otherwise
	CBaseEntity * ConvertEntity() const;

private: 
	// zero out my contents -- called at top of each Set
	inline void Void( void );
};

// mark me as having no conversions, dump the string
// if there is one
void coerciblevariant_t::Void( void )
{
	m_bvInitFields = 0;

	if (szVal)
	{
		delete[] szVal;
		szVal = nullptr;
	}
}

// get my bool contents
bool coerciblevariant_t::Bool( void )
{
	if ( (m_bvInitFields & kINIT_BOOLEAN) == 0 )
	{
		// we need to convert
		bVal = ConvertBool();
		m_bvInitFields |= kINIT_BOOLEAN ;
	}

	return bVal;
}

// get my int contents
int coerciblevariant_t::Int( void )
{
	if ( (m_bvInitFields & kINIT_INT) == 0 )
	{
		// we need to convert
		iVal = ConvertInt();
		m_bvInitFields |= kINIT_INT ;
	}

	return iVal;
}

// get my float contents
float coerciblevariant_t::Float( void )
{
	if ( (m_bvInitFields & kINIT_FLOAT) == 0 )
	{
		// we need to convert
		flVal = ConvertFloat();
		m_bvInitFields |= kINIT_FLOAT ;
	}

	return flVal;
}

// get me as an entity
const CHandle<CBaseEntity> &coerciblevariant_t::Entity( void )
{
	if ( (m_bvInitFields & kINIT_EHANDLE) == 0 )
	{
		// we need to convert
		eVal = ConvertEntity();
		m_bvInitFields |= kINIT_EHANDLE ;
	}

	return eVal;
}

typedef coerciblevariant_t cvariant_t; // easier typing!

class CFogController;

inline constexpr auto NUM_AUDIO_LOCAL_SOUNDS = 8;

// These structs are contained in each player's local data and shared by the client & server

struct fogparams_t
{
	DECLARE_CLASS_NOBASE( fogparams_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	DECLARE_SIMPLE_DATADESC();

	bool operator !=( const fogparams_t& other ) const;

	CNetworkVector( dirPrimary );
	CNetworkColor32( colorPrimary );
	CNetworkColor32( colorSecondary );
	CNetworkColor32( colorPrimaryLerpTo );
	CNetworkColor32( colorSecondaryLerpTo );
	CNetworkVar( float, start );
	CNetworkVar( float, end );
	CNetworkVar( float, farz );
	CNetworkVar( float, maxdensity );

	CNetworkVar( float, startLerpTo );
	CNetworkVar( float, endLerpTo );
	CNetworkVar( float, maxdensityLerpTo );
	CNetworkVar( float, lerptime );
	CNetworkVar( float, duration );
	CNetworkVar( bool, enable );
	CNetworkVar( bool, blend );

	CNetworkVar( float, ZoomFogScale );

	CNetworkVar( float, HDRColorScale );
};

// Crappy. Needs to be here because it wants to use 


struct fogplayerparams_t
{
	DECLARE_CLASS_NOBASE( fogplayerparams_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	DECLARE_SIMPLE_DATADESC();

	CNetworkHandle( CFogController, m_hCtrl );
	float					m_flTransitionTime;

	color32					m_OldColor;
	float					m_flOldStart;
	float					m_flOldEnd;
	float					m_flOldMaxDensity;
	float					m_flOldHDRColorScale;
	float					m_flOldFarZ;

	color32					m_NewColor;
	float					m_flNewStart;
	float					m_flNewEnd;
	float					m_flNewMaxDensity;
	float					m_flNewHDRColorScale;
	float					m_flNewFarZ;

	float					m_flZoomFogScale;

	fogplayerparams_t()
	{
		m_flTransitionTime = -1.0f;
		m_OldColor.r = m_OldColor.g = m_OldColor.g = m_OldColor.a = 0.0f;
		m_flOldStart = 0.0f;
		m_flOldEnd = 0.0f;
		m_flOldMaxDensity = 1.0f;
		m_flOldHDRColorScale = 1.0f;
		m_flOldFarZ = 0;
		m_NewColor.r = m_NewColor.g = m_NewColor.g = m_NewColor.a = 0.0f;
		m_flNewStart = 0.0f;
		m_flNewEnd = 0.0f;
		m_flNewMaxDensity = 1.0f;
		m_flNewHDRColorScale = 1.0f;
		m_flNewFarZ = 0;
		m_flZoomFogScale = 1.0f;
	}
};

struct sky3dparams_t
{
	DECLARE_CLASS_NOBASE( sky3dparams_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	DECLARE_SIMPLE_DATADESC();

	// 3d skybox camera data
	CNetworkVar( int, scale );
	CNetworkVector( origin );
	CNetworkVar( int, area );

	// 3d skybox fog data
	CNetworkVarEmbedded( fogparams_t, fog );
};

struct audioparams_t
{
	DECLARE_CLASS_NOBASE( audioparams_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	DECLARE_SIMPLE_DATADESC();

	CNetworkArray( Vector, localSound, NUM_AUDIO_LOCAL_SOUNDS )
	CNetworkVar( int, soundscapeIndex );	// index of the current soundscape from soundscape.txt
	CNetworkVar( int, localBits );			// if bits 0,1,2,3 are set then position 0,1,2,3 are valid/used
	CNetworkVar( int, entIndex );		// the entity setting the soundscape
};

class CBasePlayer;
class CSkyCamera;
struct fogparams_t;

using namespace ::ks::reflect;



//-----------------------------------------------------------------------------
// Purpose: Player specific data ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CPlayerLocalData : public CPlayerLocalMoveCore
{
	[[= ks::reflect::NetTable{ .name = "DT_Local", .base = false } ]]
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
	[[= ks::reflect::From<"m_nDuckTimeMsecs", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN }>{} ]]
	[[= ks::reflect::From<"m_nDuckJumpTimeMsecs", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nJumpTimeMsecs", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_flFallVelocity", ks::reflect::Net{ .bits = 17, .low = -4096.0f, .high = 4096.0f, .flags = SPROP_CHANGES_OFTEN }>{} ]]
	[[= ks::reflect::From<"m_flStepSize", ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 128.0f, .flags = SPROP_ROUNDUP }>{} ]]
	[[= ks::reflect::From<"m_viewPunchAngle", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_aimPunchAngle", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_aimPunchAngleVel", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_bDucked", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::From<"m_bDucking", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::From<"m_flLastDuckTime", ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_bInDuckJump", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::From<"m_bAllowAutoMovement", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::WireEnc::Int }>{} ]]
	static constexpr bool ks_class = true;

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
	// In process of ducking
	// Last time the user pressed duck (to handle duck-spam)
	// In process of duck-jumping
	// During ducking process, amount of time before full duc
	// Jump time, time to auto unduck (since we auto crouch jump now).
	// Step sound side flip/flip
	// Velocity at time when we hit ground
	// Previous button state
	class CSkyCamera *m_pOldSkyCamera;
	// Base velocity that was passed in to server physics so 
	//  client can predict conveyors correctly.  Server zeroes it, so we need to store here, too.
	// auto-decaying view angle adjustment
	// Draw view model for the player
	CNetworkVar( bool, m_bDrawViewmodel , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::WireEnc::Int } ]] );

	// Is the player wearing the HEV suit
	CNetworkVar( bool, m_bWearingSuit , [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED, .enc = ks::reflect::WireEnc::Int } ]] );
	CNetworkVar( bool, m_bPoisoned , [[= ks::reflect::Net{  } ]] );

	// Autoaim
	CNetworkVar( bool,	m_bAutoAimTarget );

	// 3d skybox
	CNetworkVarEmbedded( sky3dparams_t, m_skybox3d );
	// world fog
	CNetworkVarEmbedded( fogplayerparams_t, m_PlayerFog );
	fogparams_t			m_fog;
	// audio environment
	CNetworkVarEmbedded( audioparams_t, m_audio );

	CNetworkVar( float, m_fTBeamEndTime );
};

EXTERN_SEND_TABLE(DT_Local);

// Included last: these pull in baseentity.h and then player.h, which needs CPlayerLocalData above.

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointCamera : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PointCamera" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPointCamera, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CPointCamera();
	~CPointCamera();

	void Spawn_Impl( void );

	// Tell the client that this camera needs to be rendered
	void SetActive( bool bActive );
	bool IsActive( void ) { return m_bActive; }
	int  UpdateTransmitState_Impl(void);

	void ChangeFOVThink( void );
	float GetFOV() { return m_FOV; }

	[[= ks::reflect::Input{ .name = "ChangeFOV", .type = FIELD_STRING } ]] void InputChangeFOV( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetOnAndTurnOthersOff", .type = FIELD_VOID } ]] void InputSetOnAndTurnOthersOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetOn", .type = FIELD_VOID } ]] void InputSetOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetOff", .type = FIELD_VOID } ]] void InputSetOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputForceActive( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputForceInactive( inputdata_t &inputdata );

private:
	float m_TargetFOV;
	float m_DegreesPerSecond;

	CNetworkVar( float, m_FOV, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FOV" } ]] );
	CNetworkVar( float, m_Resolution, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "resolution" } ]] );
	CNetworkVar( bool, m_bFogEnable, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "fogEnable" } ]] );
	CNetworkColor32( m_FogColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]]
	                             [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "fogColor" } ]] );
	CNetworkVar( float, m_flFogStart, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fogStart" } ]] );
	CNetworkVar( float, m_flFogEnd, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fogEnd" } ]] );
	CNetworkVar( float, m_flFogMaxDensity, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fogMaxDensity" } ]] );
	CNetworkVar( bool, m_bActive, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bUseScreenAspectRatio, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "UseScreenAspectRatio" } ]] );

	// Allows the mapmaker to control whether a camera is active or not
	bool	m_bIsOn;

public:
	CPointCamera	*m_pNext;
};

CPointCamera *GetPointCameraList();

class CFishPool;

class CBaseAnimating;
class CBaseEntity;
class CTakeDamageInfo;

using namespace ::ks::reflect;

// fish.h
// Simple fish behavior
// Author: Michael S. Booth, April 2005

void SendProxy_FishAngle( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

//----------------------------------------------------------------------------------------------
/**
 * Simple ambient fish
 */
class CFish : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_CFish", .base = false } ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex }>{} ]]
	[[= ks::reflect::From<"m_lifeState", ks::reflect::Net{ .bits = -1 }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFish, CBaseAnimating );
	DECLARE_SERVERCLASS();

	CFish( void );
	virtual ~CFish();

	void Initialize( CFishPool *pool, unsigned int id );
	
	void Spawn_Impl( void );

	void Event_Killed_Impl( const CTakeDamageInfo &info );
	void Touch_Impl( CBaseEntity *other );			///< in contact with "other"

	void Update( float deltaT );						///< invoked each server tick

	void FlockTo( CFish *other, float amount );			///< influence my motion to flock with other nearby fish
	float Avoid( void );
	void Panic( void );									///< panic for awhile

	void ResetVisible( void );							///< zero the visible vector
	void AddVisible( CFish *fish );						///< add this fish to our visible vector

private:
	friend void SendProxy_FishOriginX( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
	friend void SendProxy_FishOriginY( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

	CHandle<CFishPool> m_pool;							///< the pool we are in
	unsigned int m_id;									///< our unique ID

	CNetworkVar( float, m_x, [[= ks::reflect::Net{ .bits = 7, .low = -255.0f, .high = 255.0f } ]] );							///< have to send position coordinates separately since Z is unused
	CNetworkVar( float, m_y, [[= ks::reflect::Net{ .bits = 7, .low = -255.0f, .high = 255.0f } ]] );							///< have to send position coordinates separately since Z is unused
	CNetworkVar( float, m_z, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );							///< only sent once since fish always swim at the same depth

	CNetworkVar( float, m_angle, [[= ks::reflect::Net{ .bits = 7, .low = 0.0f, .high = 360.0f } ]] [[= ks::reflect::Proxy<SendProxy_FishAngle, ks::reflect::WireSide::Send>{} ]] );						///< only yaw changes
	float m_angleChange;
	Vector m_forward;
	Vector m_perp;

	CNetworkVector( m_poolOrigin, [[= ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );				///< used to efficiently network our relative position
	CNetworkVar( float, m_waterLevel, [[= ks::reflect::Net{ .bits = 32 } ]] );

	float m_speed;
	float m_desiredSpeed;

	float m_calmSpeed;									///< speed the fish moves when calm
	float m_panicSpeed;									///< speed the fish moves when panicked

	float m_avoidRange;									///< range to avoid obstacles

	CountdownTimer m_turnTimer;							///< every so often our turn preference changes
	bool m_turnClockwise;								///< if true this fish prefers to turn clockwise, else CCW
	
	CountdownTimer m_goTimer;							///< start the fish moving when timer elapses
	CountdownTimer m_moveTimer;							///< dont decay speed while we are moving
	CountdownTimer m_panicTimer;						///< if active, fish is panicked
	CountdownTimer m_disperseTimer;						///< initial non-flocking time

	std::vector< CFish * > m_visible;					///< vector of fish that we can see
};

//----------------------------------------------------------------------------------------------
/**
 * This class defines a volume of water where a number of CFish swim
 */
class CFishPool : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CFishPool, CBaseEntity );

	CFishPool( void );

	void Spawn_Impl();

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	void FireGameEvent( IGameEvent *event );

	void Update( void );					///< invoked each server tick

	float GetWaterLevel( void ) const;		///< return Z coordinate of water in world coords
	float GetMaxRange( void ) const;		///< return how far a fish is allowed to wander

private:
	int m_fishCount;						///< number of fish in the pool
	float m_maxRange;						///< how far a fish is allowed to wander
	float m_swimDepth;						///< the depth the fish swim below the water surface

	float m_waterLevel;						///< Z of water surface

	bool m_isDormant;

	std::vector< CHandle<CFish> > m_fishes;	///< vector of all fish in this pool

	CountdownTimer m_visTimer;				///< for throttling line of sight checks between all fish
};

class CBaseFlex;
class CTakeDamageInfo;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

class CFlexCycler : public CBaseFlex
{
private:
	DECLARE_CLASS( CFlexCycler, CBaseFlex );
public:
	DECLARE_DATADESC();

	CFlexCycler() { m_iszSentence = NULL_STRING; m_sentence = 0; }
	void GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax);
	int	ObjectCaps_Impl( void ) { return (BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE); }
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Spawn_Impl( void );
	void Think_Impl( void );

	virtual void ProcessSceneEvents( void );

	// Don't treat as a live target
	virtual bool IsAlive( void ) { return false; }

	float m_flextime;
	LocalFlexController_t m_flexnum;
	float m_flextarget[64];
	float m_blinktime;
	float m_looktime;
	Vector m_lookTarget;
	float m_speaktime;
	int	m_istalking;
	int	m_phoneme;

	[[= ks::reflect::Key{ .name = "Sentence" } ]] string_t m_iszSentence;
	int m_sentence;

	void SetFlexTarget( LocalFlexController_t flexnum );
	LocalFlexController_t LookupFlex( const char *szTarget );
};

//
// we should get rid of all the other cyclers and replace them with this.
//
class CGenericFlexCycler : public CFlexCycler
{
public:
	DECLARE_CLASS( CGenericFlexCycler, CFlexCycler );

	void Spawn_Impl( void ) { GenericCyclerSpawn( (char *)STRING( GetModelName() ), Vector(-16, -16, 0), Vector(16, 16, 72) ); }
};

class CBaseCombatCharacter;
class CBaseEntity;
struct fogparams_t;
struct inputdata_t;

using namespace ::ks::reflect;


bool GetWorldFogParams( CBaseCombatCharacter *character, fogparams_t &fog );

// Spawn Flags
inline constexpr auto SF_FOG_MASTER = 0x0001;

//=============================================================================
//
// Class Fog Controller:
// Compares a set of integer inputs to the one main input
// Outputs true if they are all equivalant, false otherwise
//
class CFogController : public CBaseEntity
{
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
	static constexpr bool ks_class = true;

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_CLASS( CFogController, CBaseEntity );

	CFogController();
	~CFogController();

	// Parse data from a map file
	void Activate_Impl();
	int UpdateTransmitState_Impl();

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

	int DrawDebugTextOverlays_Impl(void);

	void SetLerpValues( void );
	void Spawn_Impl( void );

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
	void FireGameEvent( IGameEvent *pEvent );
	CFogController *GetMasterFogController( void )			{ return m_hMasterController; }
	void SetMasterController( CFogController *pFogController );

private:

	void InitMasterController( void );
	CHandle< CFogController > m_hMasterController;
};

CFogSystem *FogSystem( void );

class CBaseEntity;

using namespace ::ks::reflect;

// Shared stuff between door portals and window portals.
class CFuncAreaPortalBase : public CBaseEntity
{
	DECLARE_CLASS( CFuncAreaPortalBase, CBaseEntity );
public:
	DECLARE_DATADESC();

					CFuncAreaPortalBase();
	virtual			~CFuncAreaPortalBase();

	// Areaportals must be placed in each map for preprocess, they can't use transitions
	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }
	
	// This is called each frame for each client to all portals to close 
	// when the viewer is far enough away, or on the backside.
	//
	// The default implementation closes the portal if the viewer (plus some padding)
	// is on the backside of the portal. Return false if you close the portal.
	//
	// Returns whether or not the (server part of) the engine was told to shut the 
	// portal off for purposes of flowing through portals to determine which areas to
	// send to the client.
	//
	//
	// bIsOpenOnClient is usually the same as the return value but NOT always. Here's why (explained
	// here in depth because this case was discovered in a lengthy debugging session):
	//
	// - Each CFuncAreaPortalBase represents two dareaportal_t's (matched by CFuncAreaPortalBase::m_portalNumber
	//   and dareaportal_t::m_PortalKey). Each dareaportal_t leads into one of the connected areas
	//   and the dareaportal_t's have opposite-facing planes.
	//
	// - The engine's SetAreaPortalState function takes a portal key and closes BOTH dareaportal_t's associated with it.
	//
	// - UpdateVisibility may decide a portal leading out of the _area you're sitting in_ can be closed
	//   for purposes of flowing through areas because you're on the backside of the dareaportal_t that 
	//   you would flow out of. 
	//
	// - At the same time, you might be able to look through the other dareaportal_t attached to
	//   that portal key (right back into the area you're standing in). 
	//
	// - An illustration:
	//
	//	 --------------------
	//	 |					|
	//	 |	|--------|aaaaaa|
	//	 |	|		 |bbbbbb| <---- aaaa and bbbb area both for PortalKey1
	//	 |**|   	 |     	|
	//	 |	|		 | area	|
	//	 |**|		 |	2   |
	//	 |	|		 |------|
	//	 |	|		 |		|
	//	 |	---------- area |
	//	 |				1   |
	//	 |------------------|
	//
	// "aaaa" and "bbbb" each represent a different dareaportal_t, (aaa leads into area 2 and
	// bbb leads into area 1). They both represent the same portal key though (call it PortalKey1).
	//
	// When standing in area 1 (where the "area 1" text is), the engine will check aaaa and it'll notice
	// that you're on the wrong side of aaaa to be looking through it, so it'll say that PortalKey1 is closed
	// for purposes of flowing through areas on the server (it turns out you can get to area 2 through
	// the portal right above the "area 1" text).
	//
	// If you told the client that PortalKey1 was closed, then you'd get a pop when moving from area 1
	// to area 2 because the client would think you couldn't see through PortalKey1 into area 1 UNTIL
	// the server transmitted the new updated PortalKey bits, which can be lagged. 
	//
	// That's why we have bIsOpenOnClient which doesn't include this backfacing test.
	//
	// .. and they all lived happily ever after.
	// The End
	// 
	//
	//
	// Note: when you're standing in the space between the **'s, then the server would stop transmitting
	// the contents of area 2 because there would be no portal you were on the correct side of to
	// see into area 2.
	virtual bool	UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient );

public:

	// This matches two dareaportal_t::m_PortalKeys.	
	int				m_portalNumber;
	
	[[= ks::reflect::Key{ .name = "PortalVersion" } ]] int				m_iPortalVersion;

};

extern std::vector<CFuncAreaPortalBase*> g_AreaPortals;

struct inputdata_t;

using namespace ::ks::reflect;


class CFuncAreaPortalWindow : public CFuncAreaPortalBase
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncAreaPortalWindow" } ]]
	[[= ks::reflect::KeyFrom<"m_portalNumber", ks::reflect::Key{ .name = "portalnumber" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncAreaPortalWindow, CFuncAreaPortalBase );	
	
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CFuncAreaPortalWindow();
					~CFuncAreaPortalWindow();

// Overrides.
public:

	void	Spawn_Impl();
	void	Activate_Impl();

// CFuncAreaPortalBase stuff.
public:

	virtual bool	UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient );

public:
	// Returns false if the viewer is past the fadeout distance.
	bool IsWindowOpen( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor );

public:
	
	CNetworkVar( float, m_flFadeStartDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FadeStartDist" } ]] );	// Distance at which it starts fading (when <= this, alpha=m_flTranslucencyLimit).
	CNetworkVar( float, m_flFadeDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FadeDist" } ]] );		// Distance at which it becomes solid.

	// 0-1 value - minimum translucency it's allowed to get to.
	CNetworkVar( float, m_flTranslucencyLimit, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "TranslucencyLimit" } ]] );

	[[= ks::reflect::Key{ .name = "BackgroundBModel" } ]] string_t 		m_iBackgroundBModelName;	// string name of background bmodel
	CNetworkVar( int, m_iBackgroundModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );

	//Input handlers
	[[= ks::reflect::Input{ .name = "SetFadeStartDistance", .type = FIELD_FLOAT } ]] void InputSetFadeStartDistance( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFadeEndDistance", .type = FIELD_FLOAT } ]] void InputSetFadeEndDistance( inputdata_t &inputdata );
};

class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: Defines a class for objects that break after taking a certain amount
//			of damage.

typedef enum { expRandom = 0, expDirected, expUsePrecise} Explosions;
typedef enum { matGlass = 0, matWood, matMetal, matFlesh, matCinderBlock, matCeilingTile, matComputer, matUnbreakableGlass, matRocks, matMetalPanel, matNone, matLastMaterial } Materials;

inline constexpr auto NUM_SHARDS = 6; // this many shards spawned when breakable objects break;

// Spawnflags for func breakable
inline constexpr auto SF_BREAK_TRIGGER_ONLY = 0x0001; // may only be broken by trigger
inline constexpr auto SF_BREAK_TOUCH = 0x0002; // can be 'crashed through' by running player (plate glass)
inline constexpr auto SF_BREAK_PRESSURE = 0x0004; // can be broken by a player standing on it
inline constexpr auto SF_BREAK_PHYSICS_BREAK_IMMEDIATELY = 0x0200; // the first physics collision this breakable has will immediately break it
inline constexpr auto SF_BREAK_DONT_TAKE_PHYSICS_DAMAGE = 0x0400; // this breakable doesn't take damage from physics collisions
inline constexpr auto SF_BREAK_NO_BULLET_PENETRATION = 0x0800; // don't allow bullets to penetrate

// Spawnflags for func_pushable (it's also func_breakable, so don't collide with those flags)
inline constexpr auto SF_PUSH_BREAKABLE = 0x0080;
inline constexpr auto SF_PUSH_NO_USE = 0x0100; // player cannot +use pickup this ent

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBreakable : public CBaseEntity, public IBreakableWithPropData, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CBreakable, CBaseEntity );

	// basic functions
	void Spawn_Impl( void );
	void ParsePropData( void );
	bool CreateVPhysics_Impl( void );
	void Precache_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );

	void BreakTouch( CBaseEntity *pOther );
	void DamageSound( void );
	void Break( CBaseEntity *pBreaker );

	// Input handlers
	[[= ks::reflect::Input{ .name = "AddHealth", .type = FIELD_INTEGER } ]] void InputAddHealth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Break", .type = FIELD_VOID } ]] void InputBreak( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RemoveHealth", .type = FIELD_INTEGER } ]] void InputRemoveHealth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHealth", .type = FIELD_INTEGER } ]] void InputSetHealth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMass", .type = FIELD_FLOAT } ]] void InputSetMass( inputdata_t &inputdata );

	// breakables use an overridden takedamage
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );

	// To spark when hit
	void TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	bool IsBreakable( void );
	bool SparkWhenHit( void );

	char const		*DamageDecal_Impl( int bitsDamageType, int gameMaterial );

	virtual void	Die( void );
	void			ResetOnGroundFlags(void);

	inline bool		Explodable( void ) { return GetExplosiveRadius() > 0; }

	Materials		GetMaterialType( void ) { return m_Material; }
	static void MaterialSoundRandom( int entindex, Materials soundMaterial, float volume );
	static const char *MaterialSound( Materials precacheMaterial );

	static const char *pSpawnObjects[];

	int DrawDebugTextOverlays_Impl(void);

	DECLARE_DATADESC();

public:
// IBreakableWithPropData
	void			SetDmgModBullet( float flDmgMod ) { m_flDmgModBullet = flDmgMod; }
	void			SetDmgModClub( float flDmgMod ) { m_flDmgModClub = flDmgMod; }
	void			SetDmgModExplosive( float flDmgMod ) { m_flDmgModExplosive = flDmgMod; }
	void			SetDmgModFire( float flDmgMod ) { m_flDmgModFire = flDmgMod; }
	float			GetDmgModBullet( void ) { return m_flDmgModBullet; }
	float			GetDmgModClub( void ) { return m_flDmgModClub; }
	float			GetDmgModExplosive( void ) { return m_flDmgModExplosive; }
	float			GetDmgModFire( void ) { return m_flDmgModFire; }
	void			SetExplosiveRadius( float flRadius ) { m_explodeRadius = flRadius; }
	void			SetExplosiveDamage( float flDamage ) { m_ExplosionMagnitude = flDamage; }
	float			GetExplosiveRadius( void ) { return m_explodeRadius; }
	float			GetExplosiveDamage( void ) { return m_ExplosionMagnitude; }
	void			SetPhysicsDamageTable( string_t iszTableName ) { m_iszPhysicsDamageTableName = iszTableName; }
	string_t		GetPhysicsDamageTable( void ) { return m_iszPhysicsDamageTableName; }
	void			SetBreakableModel( string_t iszModel ) { m_iszBreakableModel = iszModel; }
	string_t 		GetBreakableModel( void ) { return m_iszBreakableModel; }
	void			SetBreakableSkin( int iSkin ) { m_iBreakableSkin = iSkin; }
	int				GetBreakableSkin( void ) { return m_iBreakableSkin; }
	void			SetBreakableCount( int iCount ) { m_iBreakableCount = iCount; }
	int				GetBreakableCount( void ) { return m_iBreakableCount; }
	void			SetMaxBreakableSize( int iSize ) { m_iMaxBreakableSize = iSize; }
	int				GetMaxBreakableSize( void ) { return m_iMaxBreakableSize; }
	void			SetPropDataBlocksLOS( bool bBlocksLOS ) { SetBlocksLOS( bBlocksLOS ); }
	void			SetPropDataIsAIWalkable( bool bBlocksLOS ) { SetAIWalkable( bBlocksLOS ); }
	void			SetBasePropData( string_t iszBase ) { m_iszBasePropData = iszBase; }
	string_t		GetBasePropData( void ) { return m_iszBasePropData; }
	void			SetInteraction( propdata_interactions_t Interaction ) { m_iInteractions |= (1 << Interaction); }
	bool			HasInteraction( propdata_interactions_t Interaction ) { return ( m_iInteractions & (1 << Interaction) ) != 0; }
	void			SetPhysicsMode(int iMode){}
	int				GetPhysicsMode() { return PHYSICS_MULTIPLAYER_SOLID; }
	void			SetMultiplayerBreakMode( mp_break_t mode ) {}
	mp_break_t		GetMultiplayerBreakMode( void ) const { return MULTIPLAYER_BREAK_DEFAULT; }

protected:
	Materials	m_Material;
	EHANDLE m_hBreaker;			// The entity that broke us. Held as a data member because sometimes breaking is delayed.

private:

	[[= ks::reflect::Key{ .name = "explosion" } ]] Explosions	m_Explosion;
	[[= ks::reflect::Key{ .name = "gibdir" } ]] QAngle		m_GibDir;
	string_t 	m_iszGibModel;
	string_t 	m_iszSpawnObject;
	[[= ks::reflect::Key{ .name = "explodemagnitude" } ]] int			m_ExplosionMagnitude;
	[[= ks::reflect::Key{ .name = "PressureDelay" } ]] float		m_flPressureDelay;		// Delay before breaking when destoyed by pressure
	[[= ks::reflect::Key{ .name = "minhealthdmg" } ]] int			m_iMinHealthDmg;		// minimum damage attacker must have to cause damage
	bool		m_bTookPhysicsDamage;

	string_t	m_iszPropData;
	string_t	m_iszModelName;

protected:

	bool		UpdateHealth( int iNewHealth, CBaseEntity *pActivator );

	[[= ks::reflect::Key{ .name = "physdamagescale", .input = true } ]] float		m_impactEnergyScale;

	[[= ks::reflect::Key{ .name = "OnBreak" } ]] COutputEvent m_OnBreak;
	[[= ks::reflect::Key{ .name = "OnHealthChanged" } ]] COutputFloat m_OnHealthChanged;

	// Prop data storage
	float			m_flDmgModBullet;
	float			m_flDmgModClub;
	float			m_flDmgModExplosive;
	float			m_flDmgModFire;
	string_t		m_iszPhysicsDamageTableName;
	string_t		m_iszBreakableModel;
	int				m_iBreakableSkin;
	int				m_iBreakableCount;
	int				m_iMaxBreakableSize;
	string_t		m_iszBasePropData;	
	int				m_iInteractions;
	[[= ks::reflect::Key{ .name = "PerformanceMode" } ]] PerformanceMode_t m_PerformanceMode;

	float			m_explodeRadius;

public:
	// IPlayerPickupVPhysics
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );
	CBasePlayer *HasPhysicsAttacker_Impl( float dt );
private:
	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;
};

class CBaseAnimating;
class CBaseEntity;
class CBreakable;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


inline constexpr auto MAX_NUM_PANELS = 16; //Must match client

//#############################################################################
//  > CWindowPane
//
//  A piece that falls out of the window
//#############################################################################
class CWindowPane : public CBaseAnimating
{
public:
	DECLARE_CLASS( CWindowPane, CBaseAnimating );

	static CWindowPane* CreateWindowPane(  const Vector &vecOrigin, const QAngle &vecAngles );

	void			Spawn_Impl( void );
	void			Precache_Impl( void );
	void			PaneTouch( CBaseEntity *pOther );
	void			Die( void );
};

//#############################################################################
//  > CBreakableSurface
//
//  A breakable surface
//#############################################################################
class CBreakableSurface : public CBreakable
{
	[[= ks::reflect::NetTable{ .name = "DT_BreakableSurface" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBreakableSurface, CBreakable );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nNumWide, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nNumHigh, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flPanelWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flPanelHeight, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVector( m_vNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vCorner, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( bool, m_bIsBroken, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( ShatterSurface_t, m_nSurfaceType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "surfacetype" } ]] );
	int					m_nNumBrokenPanes;
	float				m_flSupport[MAX_NUM_PANELS][MAX_NUM_PANELS]; //UNDONE: allocate dynamically?

	[[= ks::reflect::Key{ .name = "fragility" } ]] int					m_nFragility;
	[[= ks::reflect::Key{ .name = "lowerleft" } ]] Vector				m_vLLVertex;
	[[= ks::reflect::Key{ .name = "upperleft" } ]] Vector				m_vULVertex;
	[[= ks::reflect::Key{ .name = "lowerright" } ]] Vector				m_vLRVertex;
	[[= ks::reflect::Key{ .name = "upperright" } ]] Vector				m_vURVertex;
	[[= ks::reflect::Key{ .name = "error" } ]] int					m_nQuadError;

	void			SurfaceTouch( CBaseEntity *pOther );
	void			PanePos(const Vector &vPos, float *flWidth, float *flHeight);

	bool			IsBroken(int nWidth, int nHeight);
	void			SetSupport(int w, int h, float support);

	float			GetSupport(int nWidth, int nHeight);
	float			RecalcSupport(int nWidth, int nHeight);

	void			BreakPane(int nWidth, int nHeight);
	void			DropPane(int nWidth, int nHeight);
	bool			ShatterPane(int nWidth, int nHeight, const Vector &force, const Vector &vForcePos);
	void			BreakAllPanes(void);

	void			CreateShards(const Vector &vBreakPos, const QAngle &vAngles,
								 const Vector &vForce,	  const Vector &vForcePos,
								 float flWidth,			  float flHeight,
								 int   nShardSize);

	void			Spawn_Impl(void);
	void			Precache_Impl(void);
	void			Die( CBaseEntity *pBreaker, const Vector &vAttackDir );
	void			BreakThink(void);
	void			Event_Killed( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType );
	void			TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	int				OnTakeDamage_Impl( const CTakeDamageInfo &info );
	[[= ks::reflect::Input{ .name = "Shatter", .type = FIELD_VECTOR } ]] void			InputShatter( inputdata_t &inputdata );
	void			VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
private:
	// One bit per pane
	CNetworkArray( bool, m_RawPanelBitVec, MAX_NUM_PANELS * MAX_NUM_PANELS, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

class CInfoLadderDismount : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_InfoLadderDismount" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CInfoLadderDismount, CBaseEntity );
	DECLARE_NETWORKCLASS();

	 void DrawDebugGeometryOverlays_Impl();
};

typedef CHandle< CInfoLadderDismount > CInfoLadderDismountHandle;

// Spawnflags
inline constexpr auto SF_LADDER_DONTGETON = 1; // Set for ladders that are acting as automount points, but not really ladders

//-----------------------------------------------------------------------------
// Purpose: A player-climbable ladder
//-----------------------------------------------------------------------------
class CFuncLadder : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncLadder" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CFuncLadder, CBaseEntity );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	CFuncLadder();
	~CFuncLadder();

	 void Spawn_Impl();

	 void DrawDebugGeometryOverlays_Impl(void);

	int					GetDismountCount() const;
	CInfoLadderDismount	*GetDismount( int index );

	void	GetTopPosition( Vector& org );
	void	GetBottomPosition( Vector& org );
	void	ComputeLadderDir( Vector& bottomToTopVec );

	void	SetEndPoints( const Vector& p1, const Vector& p2 );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

	bool	IsEnabled() const;

	void	PlayerGotOn( CBasePlayer *pPlayer );
	void	PlayerGotOff( CBasePlayer *pPlayer );

	 void Activate_Impl();

	bool	DontGetOnLadder( void ) const;

	static int GetLadderCount();
	static CFuncLadder *GetLadder( int index );
	static std::vector< CFuncLadder * >	s_Ladders;
public:

	void FindNearbyDismountPoints( const Vector& origin, float radius, std::vector< CInfoLadderDismountHandle >& list );
	const char *GetSurfacePropName();

private:

	void	SearchForDismountPoints();

	// Movement vector from "bottom" to "top" of ladder
	CNetworkVector( m_vecLadderDir, [[= ks::reflect::Net{ .bits = SPROP_COORD, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	// Dismount points near top/bottom of ladder, precomputed
	std::vector< CInfoLadderDismountHandle > m_Dismounts;

	// Endpoints for checking for mount/dismount
	CNetworkVector( m_vecPlayerMountPositionTop, [[= ks::reflect::Net{ .bits = SPROP_COORD, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Key{ .name = "point0" } ]] );
	CNetworkVector( m_vecPlayerMountPositionBottom, [[= ks::reflect::Net{ .bits = SPROP_COORD, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Key{ .name = "point1" } ]] );

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool		m_bDisabled;
	CNetworkVar( bool,	m_bFakeLadder, [[= ks::reflect::Net{} ]] );

	[[= ks::reflect::Key{ .name = "ladderSurfaceProperties" } ]] string_t	m_surfacePropName;
	//-----------------------------------------------------
	//	Outputs
	//-----------------------------------------------------
	[[= ks::reflect::Key{ .name = "OnPlayerGotOnLadder" } ]] COutputEvent	m_OnPlayerGotOnLadder;
	[[= ks::reflect::Key{ .name = "OnPlayerGotOffLadder" } ]] COutputEvent	m_OnPlayerGotOffLadder;

	public:  int UpdateTransmitState_Impl();
	private:
};

const char *FuncLadder_GetSurfaceprops(CBaseEntity *pLadderEntity);

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;


class CFuncMoveLinear : public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncMoveLinear" } ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_fFlags", ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncMoveLinear, CBaseToggle );
	DECLARE_SERVERCLASS();

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	void		Spawn_Impl( void );
	void		Precache_Impl( void );
	bool		CreateVPhysics_Impl( void );
	bool		ShouldSavePhysics_Impl( void );

	void		MoveTo(Vector vPosition, float flSpeed);
	void		Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		MoveDone_Impl( void );
	void		StopMoveSound( void );
	void		Blocked_Impl( CBaseEntity *pOther );
	void		SetPosition( float flPosition );

	int			DrawDebugTextOverlays_Impl(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] void InputOpen( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] void InputClose( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPosition", .type = FIELD_FLOAT } ]] void InputSetPosition( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetSpeed( inputdata_t &inputdata );
	
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector		m_vecMoveDir;			// Move direction.

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "StartSound" } ]] string_t	m_soundStart;			// start and looping sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "StopSound" } ]] string_t	m_soundStop;			// stop sound
	string_t	m_currentSound;			// sound I'm playing

	[[= ks::reflect::Key{ .name = "BlockDamage" } ]] float		m_flBlockDamage;		// Damage inflicted when blocked.
	[[= ks::reflect::Key{ .name = "StartPosition" } ]] float		m_flStartPosition;		// Position of brush when spawned
	[[= ks::reflect::Key{ .name = "MoveDistance" } ]] float		m_flMoveDistance;		// Total distance the brush can move

	IPhysicsFluidController *m_pFluidController;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;
};

class CGameRules;

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CItem;
class CTakeDamageInfo;

using namespace ::ks::reflect;


// Debug history should be disabled in release builds
//#define DISABLE_DEBUG_HISTORY

//#include "items.h"

extern ConVar sk_autoaim_mode;

// Autoaiming modes
enum
{
	AUTOAIM_NONE = 0,		// No autoaim at all.
	AUTOAIM_ON,				// Autoaim is on.
	AUTOAIM_ON_CONSOLE,		// Autoaim is on, including enhanced features for Console gaming (more assistance, etc)
};

// weapon respawning return codes
enum
{	
	GR_NONE = 0,
	
	GR_WEAPON_RESPAWN_YES,
	GR_WEAPON_RESPAWN_NO,
	
	GR_AMMO_RESPAWN_YES,
	GR_AMMO_RESPAWN_NO,
	
	GR_ITEM_RESPAWN_YES,
	GR_ITEM_RESPAWN_NO,

	GR_PLR_DROP_GUN_ALL,
	GR_PLR_DROP_GUN_ACTIVE,
	GR_PLR_DROP_GUN_NO,

	GR_PLR_DROP_AMMO_ALL,
	GR_PLR_DROP_AMMO_ACTIVE,
	GR_PLR_DROP_AMMO_NO,
};

// Player relationship return codes
enum
{
	GR_NOTTEAMMATE = 0,
	GR_TEAMMATE,
	GR_ENEMY,
	GR_ALLY,
	GR_NEUTRAL,
};

// This class has the data tables and gets the CGameRules data to the client.
class CGameRulesProxy : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_GameRulesProxy", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CGameRulesProxy, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CGameRulesProxy();
	~CGameRulesProxy();

	// UNDONE: Is this correct, Mike?
	// Don't carry these across a transition, they are recreated.
	 int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	// ALWAYS transmit to all clients.
	 int UpdateTransmitState_Impl( void );

	// CGameRules chains its NetworkStateChanged calls to here, since this
	// is the actual entity that will send the data.
	static void NotifyNetworkStateChanged();

private:
	
	static CGameRulesProxy *s_pGameRulesProxy;
};

class CGameRules : public CMemZeroOnNew, public CAutoGameSystemPerFrame
{
public:
	DECLARE_CLASS_GAMEROOT( CGameRules, CAutoGameSystemPerFrame );

	virtual char const *Name() { return "CGameRules"; }

	// Stuff shared between client and server.

	CGameRules(void);

	virtual ~CGameRules( void );

	virtual	bool	Init();

	// Damage Queries - these need to be implemented by the various subclasses (single-player, multi-player, etc).
	// The queries represent queries against damage types and properties.
	virtual bool	Damage_IsTimeBased( int iDmgType ) = 0;			// Damage types that are time-based.
	virtual bool	Damage_ShouldGibCorpse( int iDmgType ) = 0;		// Damage types that gib the corpse.
	virtual bool	Damage_ShowOnHUD( int iDmgType ) = 0;			// Damage types that have client HUD art.
	virtual bool	Damage_NoPhysicsForce( int iDmgType ) = 0;		// Damage types that don't have to supply a physics force & position.
	virtual bool	Damage_ShouldNotBleed( int iDmgType ) = 0;		// Damage types that don't make the player bleed.
	//Temp: These will go away once DamageTypes become enums.
	virtual int		Damage_GetTimeBased( void ) = 0;				// Actual bit-fields.
	virtual int		Damage_GetShouldGibCorpse( void ) = 0;
	virtual int		Damage_GetShowOnHud( void ) = 0;					
	virtual int		Damage_GetNoPhysicsForce( void )= 0;
	virtual int		Damage_GetShouldNotBleed( void ) = 0;

// Ammo Definitions
	//CAmmoDef* GetAmmoDef();

	virtual bool SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon ); // Switch to the next best weapon
	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon ); // I can't use this weapon anymore, get me the next best one.
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual int DefaultFOV( void ) { return 90; }

	// This function is here for our CNetworkVars.
	inline void NetworkStateChanged()
	{
		// Forward the call to the entity that will send the data.
		CGameRulesProxy::NotifyNetworkStateChanged();
	}

	inline void NetworkStateChanged( void *pVar )
	{
		// Forward the call to the entity that will send the data.
		CGameRulesProxy::NotifyNetworkStateChanged();
	}

	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const;

// Damage rules for ammo types
	virtual float GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );
    virtual float GetDamageMultiplier( void ) { return 1.0f; }    

// Functions to verify the single/multiplayer status of a game
	virtual bool IsMultiplayer( void ) = 0;// is this a multiplayer game? (either coop or deathmatch)

	virtual const unsigned char *GetEncryptionKey() { return nullptr; }

	virtual bool InRoundRestart( void ) { return false; }

	virtual bool CheckAchievementsEnabled( int iAchievementID ) { return true; }

	virtual void RegisterScriptFunctions( void ){ };

	virtual void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) {} 

	// IsConnectedUserInfoChangeAllowed allows to override FCVAR_NOT_CONNECTED rule when
	// player is on team spectator or team unassigned for example
	// Default and previous engine implementation will never allow FCVAR_NOT_CONNECTED cvars
	// to be changed while connected to a game server
	virtual bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer ) { return false; }

	virtual void GetTaggedConVarList( KeyValues *pCvarTagList ) {}

// CBaseEntity overrides.
public:

// Setup
	virtual void OnBeginChangeLevel( const char *nextMapName, KeyValues *saveData ) {}	///< called just before a trigger_changelevel starts a changelevel
	
	// Called when game rules are destroyed by CWorld
	virtual void LevelShutdown( void ) { return; };

	virtual void Precache( void );

	virtual void RefreshSkillData( bool forceUpdate );// fill skill data struct with proper values
	
	// Called each frame. This just forwards the call to Think().
	virtual void FrameUpdatePostEntityThink();

	virtual void Think( void );		// GR_Think - runs every server frame, should handle any timer tasks, periodic events, etc.
	virtual bool IsAllowedToSpawn( CBaseEntity *pEntity ) = 0;  // Can this item spawn (eg NPCs don't spawn in deathmatch).

	// Called at the end of GameFrame (i.e. after all game logic has run this frame)
	virtual void EndGameFrame( void );

	virtual bool IsSkillLevel( int iLevel ) { return GetSkillLevel() == iLevel; }
	virtual int	GetSkillLevel() { return g_iSkillLevel; }
	virtual void OnSkillLevelChanged( int iNewLevel ) {};
	virtual void SetSkillLevel( int iLevel );

	virtual bool FAllowFlashlight( void ) = 0;// Are players allowed to switch on their flashlight?
	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon ) = 0;// should the player switch to this weapon?

// Functions to verify the single/multiplayer status of a game
	virtual bool IsDeathmatch( void ) = 0;//is this a deathmatch game?
	virtual bool IsTeamplay( void ) { return false; };// is this deathmatch game being played with team rules?
	virtual bool IsCoOp( void ) = 0;// is this a coop game?
	virtual const char *GetGameDescription( void ) { return "Half-Life 2"; }  // this is the game name that gets seen in the server browser
	
// Client connection/disconnection
	virtual bool ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) = 0;// a client just connected to the server (player hasn't spawned yet)
	virtual void InitHUD( CBasePlayer *pl ) = 0;		// the client dll is ready for updating
	virtual void ClientDisconnected( edict_t *pClient ) = 0;// a client just disconnected from the server
	virtual bool ShouldTimeoutClient( int nUserID, float flTimeSinceLastReceived ) { return false; } // return true to disconnect client due to timeout (used to do stricter timeouts when the game is sure the client isn't loading a map)
	
// Client damage rules
	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer ) = 0;// this client just hit the ground after a fall. How much damage?
	virtual bool  FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker ) {return true;};// can this player take damage from this attacker?
	virtual bool ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target ) { return true; }
	virtual float GetAutoAimScale( CBasePlayer *pPlayer ) { return 1.0f; }
	virtual int	GetAutoAimMode()	{ return AUTOAIM_ON; }

	virtual bool ShouldUseRobustRadiusDamage(CBaseEntity *pEntity) { return false; }
	virtual void  RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	// Let the game rules specify if fall death should fade screen to black
	virtual bool  FlPlayerFallDeathDoesScreenFade( CBasePlayer *pl ) { return true; }

	virtual bool AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info ) = 0;

// Client spawn/respawn control
	virtual void PlayerSpawn( CBasePlayer *pPlayer ) = 0;// called by CBasePlayer::Spawn just before releasing player into the game
	virtual void PlayerThink( CBasePlayer *pPlayer ) = 0; // called by CBasePlayer::PreThink every frame, before physics are run and after keys are accepted
	virtual bool FPlayerCanRespawn( CBasePlayer *pPlayer ) = 0;// is this player allowed to respawn now?
	virtual float FlPlayerSpawnTime( CBasePlayer *pPlayer ) = 0;// When in the future will this player be able to spawn?
	virtual CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );// Place this player on their spawnspot and face them the proper direction.
	virtual bool IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer );

	virtual bool AllowAutoTargetCrosshair( void ) { return true; };
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );  // handles the user commands;  returns true if command handled properly
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );		 // the player has changed cvars

	virtual bool CanClientCustomizeOwnIdentity() { return true; }

// Client kills/scoring
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) = 0;// how many points do I award whoever kills this player?
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info ) = 0;// Called each time a player dies
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )=  0;// Call this from within a GameRules class to report an obituary.
	virtual const char *GetDamageCustomString( const CTakeDamageInfo &info ) { return nullptr; }

	virtual bool IgnorePlayerKillCommand( void ) const { return false; }

// Weapon Damage
	// Determines how much damage Player's attacks inflict, based on skill level.
	virtual float AdjustPlayerDamageInflicted( float damage ) { return damage; }
	virtual void  AdjustPlayerDamageTaken( CTakeDamageInfo *pInfo ) {}; // Base class does nothing.

// Weapon retrieval
	virtual bool CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );// The player is touching an CBaseCombatWeapon, do I give it to him?

// Weapon spawn/respawn control
	virtual int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon ) = 0;// should this weapon respawn?
	virtual float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon ) = 0;// when may this weapon respawn?
	virtual float FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon ) = 0; // can i respawn now,  and if not, when should i try again?
	virtual Vector VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon ) = 0;// where in the world should this weapon respawn?

// Item retrieval
	virtual bool CanHaveItem( CBasePlayer *pPlayer, CItem *pItem ) = 0;// is this player allowed to take this item?
	virtual void PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem ) = 0;// call each time a player picks up an item (battery, healthkit)

// Item spawn/respawn control
	virtual int ItemShouldRespawn( CItem *pItem ) = 0;// Should this item respawn?
	virtual float FlItemRespawnTime( CItem *pItem ) = 0;// when may this item respawn?
	virtual Vector VecItemRespawnSpot( CItem *pItem ) = 0;// where in the world should this item respawn?
	virtual QAngle VecItemRespawnAngles( CItem *pItem ) = 0;// what angles should this item use when respawing?

// Ammo retrieval
	virtual bool CanHaveAmmo( CBaseCombatCharacter *pPlayer, int iAmmoIndex ); // can this player take more of this ammo?
	virtual bool CanHaveAmmo( CBaseCombatCharacter *pPlayer, const char *szName );
	virtual void PlayerGotAmmo( CBaseCombatCharacter *pPlayer, char *szName, int iCount ) = 0;// called each time a player picks up some ammo in the world
	virtual float GetAmmoQuantityScale( int iAmmoIndex ) { return 1.0f; }

// AI Definitions
	virtual void			InitDefaultAIRelationships( void ) { return; }
	virtual const char*		AIClassText(int classType) { return nullptr; }
	virtual int				NumEntityClasses() const	{ return LAST_SHARED_ENTITY_CLASS; }
	virtual int				NumFactions() const	{ return LAST_SHARED_FACTION; }

// Healthcharger respawn control
	virtual float FlHealthChargerRechargeTime( void ) = 0;// how long until a depleted HealthCharger recharges itself?
	virtual float FlHEVChargerRechargeTime( void ) { return 0; }// how long until a depleted HealthCharger recharges itself?

// What happens to a dead player's weapons
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer ) = 0;// what do I do with a player's weapons when he's killed?

// What happens to a dead player's ammo	
	virtual int DeadPlayerAmmo( CBasePlayer *pPlayer ) = 0;// Do I drop ammo when the player dies? How much?

// Teamplay stuff
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget ) = 0;// What is the player's relationship with this entity?
	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly ) = 0;
	virtual void CheckChatText( CBasePlayer *pPlayer, char *pText ) { return; }

	virtual int GetTeamIndex( const char *pTeamName ) { return -1; }
	virtual const char *GetIndexedTeamName( int teamIndex ) { return ""; }
	virtual bool IsValidTeam( const char *pTeamName ) { return true; }
	virtual void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, bool bKill, bool bGib ) {}
	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer ) { return ""; }
	virtual void UpdateClientData( CBasePlayer *pPlayer ) { };
	virtual bool IsTeamChangeSilent( CBasePlayer *pPlayer, int iTeamNum, bool bAutoTeam, bool bSilent ) { return bSilent; }

// Sounds
	virtual bool PlayTextureSounds( void ) { return true; }
	virtual bool PlayFootstepSounds( CBasePlayer *pl ) { return true; }
	virtual bool AllowSoundscapes( void ) { return true; }

// NPCs
	virtual bool FAllowNPCs( void ) = 0;//are NPCs allowed

	// Immediately end a multiplayer game
	virtual void EndMultiplayerGame( void ) {}
				    
	// trace line rules
	virtual float WeaponTraceEntity( CBaseEntity *pEntity, const Vector &vecStart, const Vector &vecEnd, unsigned int mask, trace_t *ptr );

	// Setup g_pPlayerResource (some mods use a different entity type here).
	virtual void CreateStandardEntities();

	// Team name, etc shown in chat and dedicated server console
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );

	// Location name shown in chat
	virtual const char *GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer ) { return nullptr; }

	// VGUI format string for chat, if desired
	virtual const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer ) { return nullptr; }

	// Whether props that are on fire should get a DLIGHT.
	virtual bool ShouldBurningPropsEmitLight() { return false; }

	virtual bool CanEntityBeUsePushed( CBaseEntity *pEnt ) { return true; }

	virtual void CreateCustomNetworkStringTables( void ) { }

	// Game Achievements (server version)
	virtual void MarkAchievement ( IRecipientFilter& filter, char const *pchAchievementName );

	virtual void ResetMapCycleTimeStamp( void ){ return; }

	virtual void OnNavMeshLoad( void ) { return; }

	virtual edict_t *DoFindClientInPVS( edict_t *pEdict, unsigned char *pvs, unsigned pvssize );

	virtual const char *GetGameTypeName( void ){ return nullptr; }
	virtual int GetGameType( void ){ return 0; }

	virtual bool ForceSplitScreenPlayersOnToSameTeam() { return true; }

	virtual bool IsTopDown() { return false; }
	virtual const QAngle& GetTopDownMovementAxis() { return vec3_angle; }

	// Assume the game doesn't care
	virtual int GetMaxHumanPlayers() const { return -1; }
};

	void InstallGameRules();

	// Create user messages for game here, calls into static player class creation functions
	void RegisterUserMessages( void );

extern ConVar g_Language;

//-----------------------------------------------------------------------------
// Gets us at the game rules
//-----------------------------------------------------------------------------

extern CGameRules *g_pGameRules;

inline CGameRules* GameRules()
{
	return g_pGameRules;
}

// Each game rules class must register using this in it's .cpp file.

class CGameRulesRegister
{
public:
	typedef void (*CreateGameRulesFn)();

	CGameRulesRegister( const char *pClassName, CreateGameRulesFn fn );

	// Allocates the gamerules object associated with this class.
	void CreateGameRules();

	static CGameRulesRegister* FindByName( const char *pName );

private:
	const char *m_pClassName;
	CreateGameRulesFn m_pFn;
	CGameRulesRegister *m_pNext;	// Links it into the global list.
	
	static CGameRulesRegister *s_pHead;

};

	// Server calls this at startup.
	void CreateNetworkStringTables_GameRules();
	
	// Server calls this to install a specific game rules object. The class should have been registered
	// with REGISTER_GAMERULES_CLASS.
	void CreateGameRulesObject( const char *pClassName );

// Purpose: Server-side counterpart to C_MemoryLog (see c_memorylog.h)

class CMemoryLog : public CAutoGameSystemPerFrame
{
public:
	// Methods of IGameSystem
	virtual void LevelInitPostEntity();

private:
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

//!! replace this with generic start enabled/disabled
inline constexpr auto SF_IGNORE_PLAYERUSE = 0x0002;

//-----------------------------------------------------------------------------
// Purpose: basic solid geometry
// enabled state:	brush is visible
// disabled staute:	brush not visible
//-----------------------------------------------------------------------------
class CFuncBrush : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncBrush" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncBrush, CBaseEntity );

	void Spawn_Impl( void );
	void Activate_Impl( void );
	bool CreateVPhysics_Impl( void );

	int	ObjectCaps_Impl( void ) { return HasSpawnFlags(SF_IGNORE_PLAYERUSE) ? BaseClass::ObjectCaps_Impl() : BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE; }

	int DrawDebugTextOverlays_Impl( void );

	void TurnOff( void );
	void TurnOn( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetExcluded", .type = FIELD_STRING } ]] void InputSetExcluded( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetInvert", .type = FIELD_BOOLEAN } ]] void InputSetInvert( inputdata_t &inputdata );

	enum BrushSolidities_e {
		BRUSHSOLID_TOGGLE = 0,
		BRUSHSOLID_NEVER  = 1,
		BRUSHSOLID_ALWAYS = 2,
	};

	[[= ks::reflect::Key{ .name = "Solidity" } ]] BrushSolidities_e m_iSolidity;
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int m_iDisabled;
	[[= ks::reflect::Key{ .name = "excludednpc" } ]] string_t m_iszExcludedClass;
	[[= ks::reflect::Key{ .name = "solidbsp" } ]] bool m_bSolidBsp;
	[[= ks::reflect::Key{ .name = "invert_exclusion" } ]] bool m_bInvertExclusion;

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual bool IsOn( void );
};

inline constexpr auto MODELSOUNDSCACHE_VERSION = 5;

#pragma pack(1)
class CModelSoundsCache : public IBaseCacheInfo
{
public:
	std::flat_set< int > sounds;

	CModelSoundsCache();
	CModelSoundsCache( const CModelSoundsCache& src );

	void PrecacheSoundList();

	virtual void Save( ks::byte_writer& buf );
	virtual void Restore( ks::byte_reader& buf );
	virtual void Rebuild( char const *filename );

	static void FindOrAddScriptSound( std::flat_set< int >& sounds, char const *soundname );
	static void BuildAnimationEventSoundList( CStudioHdr *hdr, std::flat_set< int >& sounds );
private:
	char const *GetSoundName( int index );
};
#pragma pack()

IMoveHelper* MoveHelperServer();

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

const int MAX_PUSHED_ENTITIES = 32;
struct physicspushlist_t
{
	float	localMoveTime;
	Vector	localOrigin;
	QAngle	localAngles;
	int		pushedCount;
	EHANDLE	pushedEnts[MAX_PUSHED_ENTITIES];
	Vector	pushVec[MAX_PUSHED_ENTITIES];
};

extern ConVar sv_gravity;
extern ConVar sv_stopspeed;
extern ConVar sv_noclipaccelerate;
extern ConVar sv_noclipspeed;
extern ConVar sv_maxspeed;
extern ConVar sv_accelerate;
extern ConVar sv_airaccelerate;
extern ConVar sv_wateraccelerate;
extern ConVar sv_waterfriction;
extern ConVar sv_footsteps;
extern ConVar sv_rollspeed;
extern ConVar sv_rollangle;
extern ConVar sv_friction;
extern ConVar sv_bounce;
extern ConVar sv_maxvelocity;
extern ConVar sv_stepsize;
extern ConVar sv_skyname;
extern ConVar sv_backspeed;
extern ConVar sv_waterdist;
extern ConVar sv_specaccelerate;
extern ConVar sv_specspeed;
extern ConVar sv_specnoclip;

// Vehicle convars
extern ConVar r_VehicleViewDampen;

// Jeep convars
extern ConVar r_JeepViewDampenFreq;
extern ConVar r_JeepViewDampenDamp;
extern ConVar r_JeepViewZHeight;

// Airboat convars
extern ConVar r_AirboatViewDampenFreq;
extern ConVar r_AirboatViewDampenDamp;
extern ConVar r_AirboatViewZHeight;

using namespace ::ks::reflect;

class MovieExplosion : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_MovieExplosion" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( MovieExplosion, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	static MovieExplosion* CreateMovieExplosion(const Vector &pos);
};

//-----------------------------------------------------------------------------
// TF Concepts
//-----------------------------------------------------------------------------
inline constexpr auto MP_CONCEPT_NONE = -1;

enum
{
	MP_CONCEPT_FIREWEAPON = 0,				// "TLK_FIREWEAPON"
	MP_CONCEPT_HURT,						// "TLK_HURT"
	MP_CONCEPT_PLAYER_EXPRESSION,			// "TLK_PLAYER_EXPRESSION"
	MP_CONCEPT_WINDMINIGUN,					// "TLK_WINDMINIGUN"
	MP_CONCEPT_FIREMINIGUN,					// "TLK_FIREMINIGUN"
	MP_CONCEPT_PLAYER_MEDIC,				// "TLK_PLAYER_MEDIC"
	MP_CONCEPT_DETONATED_OBJECT,			// "TLK_DETONATED_OBJECT"
	MP_CONCEPT_KILLED_PLAYER,				// "TLK_KILLED_PLAYER"
	MP_CONCEPT_KILLED_OBJECT,				// "TLK_KILLED_OBJECT"
	MP_CONCEPT_PLAYER_PAIN,					// "TLK_PLAYER_PAIN"
	MP_CONCEPT_PLAYER_ATTACKER_PAIN,		// "TLK_PLAYER_ATTACKER_PAIN"
	MP_CONCEPT_PLAYER_TAUNT,				// "TLK_PLAYER_TAUNT"
	MP_CONCEPT_PLAYER_HELP,					// "TLK_PLAYER_HELP"
	MP_CONCEPT_PLAYER_GO,					// "TLK_PLAYER_GO"
	MP_CONCEPT_PLAYER_MOVEUP,				// "TLK_PLAYER_MOVEUP"
	MP_CONCEPT_PLAYER_LEFT,					// "TLK_PLAYER_LEFT"
	MP_CONCEPT_PLAYER_RIGHT,				// "TLK_PLAYER_RIGHT"
	MP_CONCEPT_PLAYER_YES,					// "TLK_PLAYER_YES"
	MP_CONCEPT_PLAYER_NO,					// "TLK_PLAYER_NO"
	MP_CONCEPT_PLAYER_INCOMING,				// "TLK_PLAYER_INCOMING"
	MP_CONCEPT_PLAYER_CLOAKEDSPY,			// "TLK_PLAYER_CLOAKEDSPY"
	MP_CONCEPT_PLAYER_SENTRYAHEAD,			// "TLK_PLAYER_SENTRYAHEAD"
	MP_CONCEPT_PLAYER_TELEPORTERHERE,		// "TLK_PLAYER_TELEPORTERHERE"
	MP_CONCEPT_PLAYER_DISPENSERHERE,		// "TLK_PLAYER_DISPENSERHERE"
	MP_CONCEPT_PLAYER_SENTRYHERE,			// "TLK_PLAYER_SENTRYHERE"
	MP_CONCEPT_PLAYER_ACTIVATECHARGE,		// "TLK_PLAYER_ACTIVATECHARGE"
	MP_CONCEPT_PLAYER_CHARGEREADY,			// "TLK_PLAYER_CHARGEREADY"
	MP_CONCEPT_PLAYER_TAUNTS,				// "TLK_PLAYER_TAUNTS"	
	MP_CONCEPT_PLAYER_BATTLECRY,			// "TLK_PLAYER_BATTLECRY"
	MP_CONCEPT_PLAYER_CHEERS,				// "TLK_PLAYER_CHEERS"
	MP_CONCEPT_PLAYER_JEERS,				// "TLK_PLAYER_JEERS"
	MP_CONCEPT_PLAYER_POSITIVE,				// "TLK_PLAYER_POSITIVE"
	MP_CONCEPT_PLAYER_NEGATIVE,				// "TLK_PLAYER_NEGATIVE"
	MP_CONCEPT_PLAYER_NICESHOT,				// "TLK_PLAYER_NICESHOT"
	MP_CONCEPT_PLAYER_GOODJOB,				// "TLK_PLAYER_GOODJOB"
	MP_CONCEPT_MEDIC_STARTEDHEALING,		// "TLK_MEDIC_STARTEDHEALING"
	MP_CONCEPT_MEDIC_CHARGEREADY,			// "TLK_MEDIC_CHARGEREADY"
	MP_CONCEPT_MEDIC_STOPPEDHEALING,		// "TLK_MEDIC_STOPPEDHEALING"
	MP_CONCEPT_MEDIC_CHARGEDEPLOYED,		// "TLK_MEDIC_CHARGEDEPLOYED"
	MP_CONCEPT_FLAGPICKUP,					// "TLK_FLAGPICKUP"
	MP_CONCEPT_FLAGCAPTURED,				// "TLK_FLAGCAPTURED"
	MP_CONCEPT_ROUND_START,					// "TLK_ROUND_START"
	MP_CONCEPT_SUDDENDEATH_START,			// "TLK_SUDDENDEATH_START"
	MP_CONCEPT_ONFIRE,						// "TLK_ONFIRE"
	MP_CONCEPT_STALEMATE,					// "TLK_STALEMATE"
	MP_CONCEPT_BUILDING_OBJECT,				// "TLK_BUILDING_OBJECT"
	MP_CONCEPT_LOST_OBJECT,					// "TLK_LOST_OBJECT"
	MP_CONCEPT_SPY_SAPPER,					// "TLK_SPY_SAPPER"
	MP_CONCEPT_TELEPORTED,					// "TLK_TELEPORTED"
	MP_CONCEPT_LOST_CONTROL_POINT,			// "TLK_LOST_CONTROL_POINT"
	MP_CONCEPT_CAPTURED_POINT,				// "TLK_CAPTURED_POINT"
	MP_CONCEPT_CAPTURE_BLOCKED,				// "TLK_CAPTURE_BLOCKED"
	MP_CONCEPT_HEALTARGET_STARTEDHEALING,	// "TLK_HEALTARGET_STARTEDHEALING"
	MP_CONCEPT_HEALTARGET_CHARGEREADY,		// "TLK_HEALTARGET_CHARGEREADY"
	MP_CONCEPT_HEALTARGET_STOPPEDHEALING,	// "TLK_HEALTARGET_STOPPEDHEALING"
	MP_CONCEPT_HEALTARGET_CHARGEDEPLOYED,	// "TLK_HEALTARGET_CHARGEDEPLOYED"
	MP_CONCEPT_MINIGUN_FIREWEAPON,			// "TLK_MINIGUN_FIREWEAPON"
	MP_CONCEPT_DIED,						// "TLK_DIED"
	MP_CONCEPT_PLAYER_THANKS,				// "TLK_PLAYER_THANKS"
	MP_CONCEPT_CART_MOVING_FORWARD,			// "TLK_CART_MOVING_FORWARD"
	MP_CONCEPT_CART_MOVING_BACKWARD,		// "TLK_CART_MOVING_BACKWARD"
	MP_CONCEPT_CART_STOP,					// "TLK_CART_STOP"
	MP_CONCEPT_ATE_FOOD,					// "TLK_ATE_FOOD"
	MP_TF_CONCEPT_COUNT

	// Other MP_CONCEPT_* start he using MP_TF_CONCEPT_COUNT + 1 as start.
};

extern const char *g_pszMPConcepts[];
int GetMPConceptIndexFromString( const char *pszConcept );

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseMultiplayerPlayer;
class CBasePlayer;
class CGameRules;
class CItem;
class CTakeDamageInfo;

using namespace ::ks::reflect;


extern ConVar mp_restartgame;
extern ConVar mp_waitingforplayers_time;
extern ConVar mp_waitingforplayers_restart;
extern ConVar mp_waitingforplayers_cancel;
extern ConVar mp_clan_readyrestart;
extern ConVar mp_clan_ready_signal;
extern ConVar nextlevel;

inline constexpr auto VOICE_COMMAND_MAX_SUBTITLE_DIST = 1900;


inline constexpr auto MAX_SPEAK_CONCEPT_LEN = 64;
inline constexpr auto MAX_VOICE_COMMAND_SUBTITLE = 256;

typedef struct
{
	// concept to speak
	int	 m_iConcept;

	// play subtitle?
	bool m_bShowSubtitle;
	bool m_bDistanceBasedSubtitle;

	char m_szGestureActivity[64];

} VoiceCommandMenuItem_t;

extern ConVar mp_timelimit;


//=========================================================
// CMultiplayRules - rules for the basic half life multiplayer
// competition
//=========================================================
class CMultiplayRules : public CGameRules
{
public:
	DECLARE_CLASS( CMultiplayRules, CGameRules );

// Functions to verify the single/multiplayer status of a game
	virtual bool IsMultiplayer( void );

	// Damage query implementations.
	virtual bool	Damage_IsTimeBased( int iDmgType );			// Damage types that are time-based.
	virtual bool	Damage_ShouldGibCorpse( int iDmgType );		// Damage types that gib the corpse.
	virtual bool	Damage_ShowOnHUD( int iDmgType );				// Damage types that have client HUD art.
	virtual bool	Damage_NoPhysicsForce( int iDmgType );		// Damage types that don't have to supply a physics force & position.
	virtual bool	Damage_ShouldNotBleed( int iDmgType );			// Damage types that don't make the player bleed.
	// TEMP: These will go away once DamageTypes become enums.
	virtual int		Damage_GetTimeBased( void );
	virtual int		Damage_GetShouldGibCorpse( void );
	virtual int		Damage_GetShowOnHud( void );
	virtual int		Damage_GetNoPhysicsForce( void );
	virtual int		Damage_GetShouldNotBleed( void );

	CMultiplayRules();
	virtual ~CMultiplayRules() {}

	void LoadVoiceCommandScript( void );

// GR_Think
	virtual void Think( void );
	virtual void RefreshSkillData( bool forceUpdate );
	virtual bool IsAllowedToSpawn( CBaseEntity *pEntity );
	virtual bool FAllowFlashlight( void );

	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );
	bool SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );

// Functions to verify the single/multiplayer status of a game
	virtual bool IsDeathmatch( void );
	virtual bool IsCoOp( void );

// Client connection/disconnection
	// If ClientConnected returns false, the connection is rejected and the user is provided the reason specified in
	//  svRejectReason
	// Only the client's name and remote address are provided to the dll for verification.
	virtual bool ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual void InitHUD( CBasePlayer *pl );		// the client dll is ready for updating
	virtual void ClientDisconnected( edict_t *pClient );

// Client damage rules
	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );
	virtual bool  FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual bool AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info );

// Client spawn/respawn control
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual bool FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual float FlPlayerSpawnTime( CBasePlayer *pPlayer );
	virtual CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );

	virtual bool AllowAutoTargetCrosshair( void );

// Client kills/scoring
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	CBasePlayer *GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor );									// old version of method - kept for backward compat
	virtual CBasePlayer *GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor, CBaseEntity *pVictim );		// new version of method

// Weapon retrieval
	virtual bool CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );// The player is touching an CBaseCombatWeapon, do I give it to him?

// Weapon spawn/respawn control
	virtual int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon );
	virtual float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon );
	virtual float FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon );
	virtual Vector VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon );

// Item retrieval
	virtual bool CanHaveItem( CBasePlayer *pPlayer, CItem *pItem );
	virtual void PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem );

// Item spawn/respawn control
	virtual int ItemShouldRespawn( CItem *pItem );
	virtual float FlItemRespawnTime( CItem *pItem );
	virtual Vector VecItemRespawnSpot( CItem *pItem );
	virtual QAngle VecItemRespawnAngles( CItem *pItem );

// Ammo retrieval
	virtual void PlayerGotAmmo( CBaseCombatCharacter *pPlayer, char *szName, int iCount );

// Healthcharger respawn control
	virtual float FlHealthChargerRechargeTime( void );
	virtual float FlHEVChargerRechargeTime( void );

// What happens to a dead player's weapons
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer );

// What happens to a dead player's ammo	
	virtual int DeadPlayerAmmo( CBasePlayer *pPlayer );

// Teamplay stuff	
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly );

	virtual bool PlayTextureSounds( void ) { return false; }
	virtual bool PlayFootstepSounds( CBasePlayer *pl );

// NPCs
	virtual bool FAllowNPCs( void );
	
	// Immediately end a multiplayer game
	virtual void EndMultiplayerGame( void ) { GoToIntermission(); }

// Voice commands
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual VoiceCommandMenuItem_t *VoiceCommand( CBaseMultiplayerPlayer *pPlayer, int iMenu, int iItem );
	
// Bugbait report	
	bool IsLoadingBugBaitReport( void );

	virtual void ResetMapCycleTimeStamp( void ){ m_nMapCycleTimeStamp = 0; }

	virtual void HandleTimeLimitChange( void ){ return; }

	void IncrementMapCycleIndex();

	void HaveAllPlayersSpeakConceptIfAllowed( int iConcept );

	virtual void GetTaggedConVarList( KeyValues *pCvarTagList );

	void SkipNextMapInCycle();

	virtual float GetIntermissionStartTime() const { return m_flIntermissionStartTime; };
	
	virtual bool OnReplayPrompt( CBasePlayer *pVictim, CBasePlayer *pScorer );
protected:
	virtual bool UseSuicidePenalty() { return true; }		// apply point penalty for suicide?
	virtual void GetNextLevelName( char *szNextMap, int bufsize, bool bRandom = false );
	virtual void ChangeLevel( void );
	virtual void GoToIntermission( void );
	float GetIntermissionDuration() const;
	float m_flIntermissionStartTime;
	static int m_nMapCycleTimeStamp;
	static int m_nMapCycleindex;
	static std::vector< std::string > m_MapList;
	const char* m_szNextLevelName;
	
	private:
		std::vector< std::vector< VoiceCommandMenuItem_t > > m_VoiceCommandMenus;
		
};

inline CMultiplayRules* MultiplayRules()
{
	return static_cast<CMultiplayRules*>(g_pGameRules);
}

class CBaseEntity;

using namespace ::ks::reflect;

// nav.h
// Data structures and constants for the Navigation Mesh system
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

/**
 * Below are several constants used by the navigation system.
 * @todo Move these into TheNavMesh singleton.
 */
const float GenerationStepSize = 25.0f;			// (30) was 20, but bots can't fit always fit
const float JumpHeight = 41.8f;					// if delta Z is less than this, we can jump up on it

const float JumpCrouchHeight = 58.0f;			// (48) if delta Z is less than or equal to this, we can jumpcrouch up on it

// There are 3 different definitions of StepHeight throughout the code, waiting to produce bugs if the 18.0 is ever changed.
const float StepHeight = 18.0f;					// if delta Z is greater than this, we have to jump to get up

// TERROR: Increased DeathDrop from 200, since zombies don't take falling damage
const float DeathDrop = 200.0f;					// (300) distance at which we will die if we fall - should be about 600, and pay attention to fall damage during pathfind

const float ClimbUpHeight = JumpCrouchHeight;	// CSBots assume all jump up links are reachable

const float CliffHeight = 300.0f;				// height which we consider a significant cliff which we would not want to fall off of

// TERROR: Converted these values to use the same numbers as the player bounding boxes etc
inline constexpr auto HalfHumanWidth = 16;
inline constexpr auto HalfHumanHeight = 35.5;
inline constexpr auto HumanHeight = 71;
inline constexpr auto HumanEyeHeight = 62;
inline constexpr auto HumanCrouchHeight = 55;
inline constexpr auto HumanCrouchEyeHeight = 37;

inline constexpr auto NAV_MAGIC_NUMBER = 0xFEEDFACE; // to help identify nav files

/**
 * A place is a named group of navigation areas
 */
typedef unsigned int Place;
inline constexpr auto UNDEFINED_PLACE = 0; // ie: "no place"
inline constexpr auto ANY_PLACE = 0xFFFF;

enum NavErrorType
{
	NAV_OK,
	NAV_CANT_ACCESS_FILE,
	NAV_INVALID_FILE,
	NAV_BAD_FILE_VERSION,
	NAV_FILE_OUT_OF_DATE,
	NAV_CORRUPT_DATA,
	NAV_OUT_OF_MEMORY,
};

enum NavAttributeType
{
	NAV_MESH_INVALID		= 0,
	NAV_MESH_CROUCH			= 0x0000001,				// must crouch to use this node/area
	NAV_MESH_JUMP			= 0x0000002,				// must jump to traverse this area (only used during generation)
	NAV_MESH_PRECISE		= 0x0000004,				// do not adjust for obstacles, just move along area
	NAV_MESH_NO_JUMP		= 0x0000008,				// inhibit discontinuity jumping
	NAV_MESH_STOP			= 0x0000010,				// must stop when entering this area
	NAV_MESH_RUN			= 0x0000020,				// must run to traverse this area
	NAV_MESH_WALK			= 0x0000040,				// must walk to traverse this area
	NAV_MESH_AVOID			= 0x0000080,				// avoid this area unless alternatives are too dangerous
	NAV_MESH_TRANSIENT		= 0x0000100,				// area may become blocked, and should be periodically checked
	NAV_MESH_DONT_HIDE		= 0x0000200,				// area should not be considered for hiding spot generation
	NAV_MESH_STAND			= 0x0000400,				// bots hiding in this area should stand
	NAV_MESH_NO_HOSTAGES	= 0x0000800,				// hostages shouldn't use this area
	NAV_MESH_STAIRS			= 0x0001000,				// this area represents stairs, do not attempt to climb or jump them - just walk up
	NAV_MESH_NO_MERGE		= 0x0002000,				// don't merge this area with adjacent areas
	NAV_MESH_OBSTACLE_TOP	= 0x0004000,				// this nav area is the climb point on the tip of an obstacle
	NAV_MESH_CLIFF			= 0x0008000,				// this nav area is adjacent to a drop of at least CliffHeight

	NAV_MESH_FIRST_CUSTOM	= 0x00010000,				// apps may define custom app-specific bits starting with this value
	NAV_MESH_LAST_CUSTOM	= 0x04000000,				// apps must not define custom app-specific bits higher than with this value

	NAV_MESH_BLOCKED_PROPDOOR	= 0x10000000,				// area is blocked by prop_door_rotating 

	NAV_MESH_HAS_ELEVATOR	= 0x40000000,				// area is in an elevator's path
	NAV_MESH_NAV_BLOCKER	= 0x80000000				// area is blocked by nav blocker ( Alas, needed to hijack a bit in the attributes to get within a cache line [7/24/2008 tom])
};

extern NavAttributeType NameToNavAttribute( const char *name );

enum NavDirType
{
	NORTH = 0,
	EAST = 1,
	SOUTH = 2,
	WEST = 3,

	NUM_DIRECTIONS
};

/**
 * Defines possible ways to move from one area to another
 */
enum NavTraverseType
{
	// NOTE: First 4 directions MUST match NavDirType
	GO_NORTH = 0,
	GO_EAST,
	GO_SOUTH,
	GO_WEST,

	GO_LADDER_UP,
	GO_LADDER_DOWN,
	GO_JUMP,
	GO_ELEVATOR_UP,
	GO_ELEVATOR_DOWN,

	NUM_TRAVERSE_TYPES
};

enum NavCornerType
{
	NORTH_WEST = 0,
	NORTH_EAST = 1,
	SOUTH_EAST = 2,
	SOUTH_WEST = 3,

	NUM_CORNERS
};

enum NavRelativeDirType
{
	FORWARD = 0,
	RIGHT,
	BACKWARD,
	LEFT,
	UP,
	DOWN,

	NUM_RELATIVE_DIRECTIONS
};

struct Extent
{
	Vector lo, hi;

	void Init( void )
	{
		lo.Init();
		hi.Init();
	}

	void Init( CBaseEntity *entity )
	{
		entity->CollisionProp()->WorldSpaceSurroundingBounds( &lo, &hi );
	}

	float SizeX( void ) const	{ return hi.x - lo.x; }
	float SizeY( void ) const	{ return hi.y - lo.y; }
	float SizeZ( void ) const	{ return hi.z - lo.z; }
	float Area( void ) const	{ return SizeX() * SizeY(); }

	// Increase bounds to contain the given point
	void Encompass( const Vector &pos )
	{
		for ( int i=0; i<3; ++i )
		{
			if ( pos[i] < lo[i] )
			{
				lo[i] = pos[i];
			}
			else if ( pos[i] > hi[i] )
			{
				hi[i] = pos[i];
			}
		}
	}

	// Increase bounds to contain the given extent
	void Encompass( const Extent &extent )
	{
		Encompass( extent.lo );
		Encompass( extent.hi );
	}

	/// return true if 'pos' is inside of this extent
	bool Contains( const Vector &pos ) const
	{
		return (pos.x >= lo.x && pos.x <= hi.x &&
				pos.y >= lo.y && pos.y <= hi.y &&
				pos.z >= lo.z && pos.z <= hi.z);
	}
	
	/// return true if this extent overlaps the given one
	bool IsOverlapping( const Extent &other ) const
	{
		return (lo.x <= other.hi.x && hi.x >= other.lo.x &&
				lo.y <= other.hi.y && hi.y >= other.lo.y &&
				lo.z <= other.hi.z && hi.z >= other.lo.z);
	}
};

struct Ray
{
	Vector from, to;
};


//--------------------------------------------------------------------------------------------------------------
inline NavDirType OppositeDirection( NavDirType dir )
{
	switch( dir )
	{
		case NORTH: return SOUTH;
		case SOUTH: return NORTH;
		case EAST:	return WEST;
		case WEST:	return EAST;
	}

	return NORTH;
}

//--------------------------------------------------------------------------------------------------------------
inline NavDirType DirectionLeft( NavDirType dir )
{
	switch( dir )
	{
		case NORTH: return WEST;
		case SOUTH: return EAST;
		case EAST:	return NORTH;
		case WEST:	return SOUTH;
	}

	return NORTH;
}

//--------------------------------------------------------------------------------------------------------------
inline NavDirType DirectionRight( NavDirType dir )
{
	switch( dir )
	{
		case NORTH: return EAST;
		case SOUTH: return WEST;
		case EAST:	return SOUTH;
		case WEST:	return NORTH;
	}

	return NORTH;
}

//--------------------------------------------------------------------------------------------------------------
inline void AddDirectionVector( Vector *v, NavDirType dir, float amount )
{
	switch( dir )
	{
		case NORTH: v->y -= amount; return;
		case SOUTH: v->y += amount; return;
		case EAST:  v->x += amount; return;
		case WEST:  v->x -= amount; return;
	}
}

//--------------------------------------------------------------------------------------------------------------
inline float DirectionToAngle( NavDirType dir )
{
	switch( dir )
	{
		case NORTH:	return 270.0f;
		case SOUTH:	return 90.0f;
		case EAST:	return 0.0f;
		case WEST:	return 180.0f;
	}

	return 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
inline NavDirType AngleToDirection( float angle )
{
	while( angle < 0.0f )
		angle += 360.0f;

	while( angle > 360.0f )
		angle -= 360.0f;

	if (angle < 45 || angle > 315)
		return EAST;

	if (angle >= 45 && angle < 135)
		return SOUTH;

	if (angle >= 135 && angle < 225)
		return WEST;

	return NORTH;
}

//--------------------------------------------------------------------------------------------------------------
inline void DirectionToVector2D( NavDirType dir, Vector2D *v )
{
	switch( dir )
	{
		case NORTH: v->x =  0.0f; v->y = -1.0f; break;
		case SOUTH: v->x =  0.0f; v->y =  1.0f; break;
		case EAST:  v->x =  1.0f; v->y =  0.0f; break;
		case WEST:  v->x = -1.0f; v->y =  0.0f; break;
	}
}

//--------------------------------------------------------------------------------------------------------------
inline void CornerToVector2D( NavCornerType dir, Vector2D *v )
{
	switch( dir )
	{
		case NORTH_WEST: v->x = -1.0f; v->y = -1.0f; break;
		case NORTH_EAST: v->x =  1.0f; v->y = -1.0f; break;
		case SOUTH_EAST: v->x =  1.0f; v->y =  1.0f; break;
		case SOUTH_WEST: v->x = -1.0f; v->y =  1.0f; break;
	}

	v->NormalizeInPlace();
}

//--------------------------------------------------------------------------------------------------------------
// Gets the corner types that surround the given direction
inline void GetCornerTypesInDirection( NavDirType dir, NavCornerType *first, NavCornerType *second )
{
	switch ( dir )
	{
	case NORTH:
		*first = NORTH_WEST;
		*second = NORTH_EAST;
		break;
	case SOUTH:
		*first = SOUTH_WEST;
		*second = SOUTH_EAST;
		break;
	case EAST:
		*first = NORTH_EAST;
		*second = SOUTH_EAST;
		break;
	case WEST:
		*first = NORTH_WEST;
		*second = SOUTH_WEST;
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
inline float RoundToUnits( float val, float unit )
{
	val = val + ((val < 0.0f) ? -unit*0.5f : unit*0.5f);
	return (float)( unit * ( ((int)val) / (int)unit ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given entity can be ignored when moving
 */
inline constexpr auto WALK_THRU_PROP_DOORS = 0x01;
inline constexpr auto WALK_THRU_FUNC_DOORS = 0x02;
inline constexpr auto WALK_THRU_DOORS = (WALK_THRU_PROP_DOORS | WALK_THRU_FUNC_DOORS);
inline constexpr auto WALK_THRU_BREAKABLES = 0x04;
inline constexpr auto WALK_THRU_TOGGLE_BRUSHES = 0x08;
inline constexpr auto WALK_THRU_EVERYTHING = (WALK_THRU_DOORS | WALK_THRU_BREAKABLES | WALK_THRU_TOGGLE_BRUSHES);
extern ConVar nav_solid_props;
inline bool IsEntityWalkable( CBaseEntity *entity, unsigned int flags )
{
	if (FClassnameIs( entity, "worldspawn" ))
		return false;

	if (FClassnameIs( entity, "player" ))
		return false;

	// if we hit a door, assume its walkable because it will open when we touch it
	if (FClassnameIs( entity, "func_door*" ))
		return (flags & WALK_THRU_FUNC_DOORS) ? true : false;

	if (FClassnameIs( entity, "prop_door*" ))
		return (flags & WALK_THRU_PROP_DOORS) ? true : false;

	// if we hit a clip brush, ignore it if it is not BRUSHSOLID_ALWAYS
	if (FClassnameIs( entity, "func_brush" ))
	{
		CFuncBrush *brush = (CFuncBrush *)entity;
		switch ( brush->m_iSolidity )
		{
		case CFuncBrush::BRUSHSOLID_ALWAYS:
			return false;
		case CFuncBrush::BRUSHSOLID_NEVER:
			return true;
		case CFuncBrush::BRUSHSOLID_TOGGLE:
			return (flags & WALK_THRU_TOGGLE_BRUSHES) ? true : false;
		}
	}

	// if we hit a breakable object, assume its walkable because we will shoot it when we touch it
	if (FClassnameIs( entity, "func_breakable" ) && entity->GetHealth() && entity->m_takedamage == DAMAGE_YES)
		return (flags & WALK_THRU_BREAKABLES) ? true : false;

	if (FClassnameIs( entity, "func_breakable_surf" ) && entity->m_takedamage == DAMAGE_YES)
		return (flags & WALK_THRU_BREAKABLES) ? true : false;

	if ( FClassnameIs( entity, "func_playerinfected_clip" ) == true )
		return true;

	if ( nav_solid_props.GetBool() && FClassnameIs( entity, "prop_*" ) )
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Trace filter that ignores players, NPCs, and objects that can be walked through
 */
class CTraceFilterWalkableEntities : public CTraceFilterNoNPCsOrPlayer
{
public:
	CTraceFilterWalkableEntities( const IHandleEntity *passentity, int collisionGroup, unsigned int flags )
		: CTraceFilterNoNPCsOrPlayer( passentity, collisionGroup ), m_flags( flags )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		if ( CTraceFilterNoNPCsOrPlayer::ShouldHitEntity(pServerEntity, contentsMask) )
		{
			CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
			return ( !IsEntityWalkable( pEntity, m_flags ) );
		}
		return false;
	}

private:
	unsigned int m_flags;
};

extern bool IsWalkableTraceLineClear( const Vector &from, const Vector &to, unsigned int flags = 0 );
extern bool IsWalkableTraceHullClear( const Vector &from, const Vector &to, const Vector &mins, const Vector &maxs, unsigned int flags );

class CBaseAnimating;
class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: Controls the pose parameters of a model

inline constexpr auto MAX_POSE_CONTROLLED_PROPS = 4; // Number of entities by the same name that can be controlled

// Type of frequency modulations
enum PoseController_FModType_t
{
	POSECONTROLLER_FMODTYPE_NONE = 0,
	POSECONTROLLER_FMODTYPE_SINE,
	POSECONTROLLER_FMODTYPE_SQUARE,
	POSECONTROLLER_FMODTYPE_TRIANGLE,
	POSECONTROLLER_FMODTYPE_SAWTOOTH,
	POSECONTROLLER_FMODTYPE_NOISE,

	POSECONTROLLER_FMODTYPE_TOTAL,
};

//-----------------------------------------------------------------------------
// SERVER CLASS
//-----------------------------------------------------------------------------

inline constexpr auto MAX_POSE_CYCLE_FREQUENCY = 10.0f;

inline constexpr auto MAX_POSE_FMOD_AMPLITUDE = 10.0f;

inline constexpr auto MAX_POSE_FMOD_RATE = 10.0f;

inline constexpr auto MAX_POSE_INTERPOLATION_TIME = 10.0f;

class CPoseController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PoseController" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPoseController, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	 void Spawn_Impl( void );

	void Think_Impl( void );

	void BuildPropList( void );
	void BuildPoseIndexList( void );
	void SetPoseIndex( int i, int iValue );
	void SetCurrentPose( float fCurrentPoseValue );

	float GetPoseValue( void );

	void SetProp( CBaseAnimating *pProp );
	void SetPropName( const char *pName );
	void SetPoseParameterName( const char *pName );
	void SetPoseValue( float fValue );
	void SetInterpolationTime( float fValue );
	void SetInterpolationWrap( bool bWrap );
	void SetCycleFrequency( float fValue );
	void SetFModType( int nType );
	void SetFModTimeOffset( float fValue );
	void SetFModRate( float fValue );
	void SetFModAmplitude( float fValue );
	void RandomizeFMod( float fExtremeness );

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetPoseParameterName", .type = FIELD_STRING } ]] void InputSetPoseParameterName( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPoseValue", .type = FIELD_FLOAT } ]] void InputSetPoseValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetInterpolationTime", .type = FIELD_FLOAT } ]] void InputSetInterpolationTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCycleFrequency", .type = FIELD_FLOAT } ]] void InputSetCycleFrequency( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModType", .type = FIELD_INTEGER } ]] void InputSetFModType( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModTimeOffset", .type = FIELD_FLOAT } ]] void InputSetFModTimeOffset( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModRate", .type = FIELD_FLOAT } ]] void InputSetFModRate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFModAmplitude", .type = FIELD_FLOAT } ]] void InputSetFModAmplitude( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RandomizeFMod", .type = FIELD_FLOAT } ]] void InputRandomizeFMod( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetFMod", .type = FIELD_VOID } ]] void InputGetFMod( inputdata_t &inputdata );

private:

	CNetworkArray( EHANDLE, m_hProps, MAX_POSE_CONTROLLED_PROPS, [[= ks::reflect::Net{} ]] );				// Handles to controlled models
	CNetworkArray( unsigned char, m_chPoseIndex, MAX_POSE_CONTROLLED_PROPS, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] );	// Pose parameter indices for each model

	bool		m_bDisablePropLookup;

	CNetworkVar( bool, m_bPoseValueParity, [[= ks::reflect::Net{} ]] );

	[[= ks::reflect::Key{ .name = "PropName" } ]] string_t	m_iszPropName;				// Targetname of the models to control
	[[= ks::reflect::Key{ .name = "PoseParameterName" } ]] string_t	m_iszPoseParameterName;		// Pose parameter name to control

	CNetworkVar( float, m_fPoseValue, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 1.0f } ]] [[= ks::reflect::Key{ .name = "PoseValue" } ]] );			// Normalized pose parameter value (maps to each pose parameter's min and max range)
	CNetworkVar( float, m_fInterpolationTime, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = MAX_POSE_INTERPOLATION_TIME } ]] [[= ks::reflect::Key{ .name = "InterpolationTime" } ]] );	// Interpolation speed for client matching absolute pose values
	CNetworkVar( bool, m_bInterpolationWrap, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "InterpolationWrap" } ]] );	// Interpolation for the client wraps 0 to 1.

	CNetworkVar( float, m_fCycleFrequency, [[= ks::reflect::Net{ .bits = 11, .low = -MAX_POSE_CYCLE_FREQUENCY, .high = MAX_POSE_CYCLE_FREQUENCY } ]] [[= ks::reflect::Key{ .name = "CycleFrequency" } ]] );	// Cycles per second

	// Frequency modulation variables
	CNetworkVar( PoseController_FModType_t, m_nFModType, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "FModType" } ]] );
	CNetworkVar( float, m_fFModTimeOffset, [[= ks::reflect::Net{ .bits = 11, .low = -1.0f, .high = 1.0f } ]] [[= ks::reflect::Key{ .name = "FModTimeOffset" } ]] );
	CNetworkVar( float, m_fFModRate, [[= ks::reflect::Net{ .bits = 11, .low = -MAX_POSE_FMOD_RATE, .high = MAX_POSE_FMOD_RATE } ]] [[= ks::reflect::Key{ .name = "FModRate" } ]] );
	CNetworkVar( float, m_fFModAmplitude, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = MAX_POSE_FMOD_AMPLITUDE } ]] [[= ks::reflect::Key{ .name = "FModAmplitude" } ]] );
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Point entity used to create templates out of other entities or groups of entities

inline constexpr auto MAX_NUM_TEMPLATES = 16;

struct template_t
{
	int			iTemplateIndex;
	VMatrix		matEntityToTemplate;

	DECLARE_SIMPLE_DATADESC();
};

void PrecachePointTemplates();

//-----------------------------------------------------------------------------

void ScriptInstallPreSpawnHook();
bool ScriptPreInstanceSpawn( CScriptScope *pScriptScope, CBaseEntity *pChild, string_t iszKeyValueData );
void ScriptPostSpawn( CScriptScope *pScriptScope, CBaseEntity **ppEntities, int nEntities );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointTemplate : public CLogicalEntity
{
	DECLARE_CLASS( CPointTemplate, CLogicalEntity );
public:
	DECLARE_DATADESC();

	void	Spawn_Impl( void );
	void	Precache_Impl();

	// Template initialization
	void			StartBuildingTemplates( void );
	void			FinishBuildingTemplates( void );

	// Template Entity accessors
	int				GetNumTemplateEntities( void );
	CBaseEntity		*GetTemplateEntity( int iTemplateNumber );
	void			AddTemplate( CBaseEntity *pEntity, const char *pszMapData, int nLen );
	bool			ShouldRemoveTemplateEntities( void );
	bool			AllowNameFixup();

	// Templates accessors
	int				GetNumTemplates( void );
	int				GetTemplateIndexForTemplate( int iTemplate );

	// Template instancing
	bool			CreateInstance( const Vector &vecOrigin, const QAngle &vecAngles, std::vector< CBaseEntity* > *pEntities, CBaseEntity *pEntityMaker = nullptr, bool bCreateTime = false );
	void			CreationComplete( const std::vector< CBaseEntity* > &entities );

	// Inputs
	[[= ks::reflect::Input{ .name = "ForceSpawn", .type = FIELD_VOID } ]] void			InputForceSpawn( inputdata_t &inputdata );

	virtual void	PerformPrecache();

private:
	[[= ks::reflect::Key{ .name = "Template16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "Template15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "Template14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "Template13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "Template12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "Template11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "Template10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "Template09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "Template08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "Template07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "Template06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "Template05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "Template04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "Template03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "Template02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "Template01", .index = 0 } ]] string_t						m_iszTemplateEntityNames[MAX_NUM_TEMPLATES];

	// List of map entities this template targets. Built inside our Spawn().
	// It's only valid between Spawn() & Activate(), because the map entity parsing
	// code removes all the entities in it once it finishes turning them into templates.
	std::vector< CBaseEntity * >		m_hTemplateEntities;

	// List of templates, generated from our template entities.
	std::vector< template_t >		m_hTemplates;

	[[= ks::reflect::Key{ .name = "OnEntitySpawned" } ]] COutputEvent					m_pOutputOnSpawned;
};

class CBaseEntity;



// inherit from this interface to be able to call WatchPositionChanges
class IWatcherCallback
{
public:
	virtual ~IWatcherCallback() {}
};

class IPositionWatcher : public IWatcherCallback
{
public:
	virtual void NotifyPositionChanged( CBaseEntity *pEntity ) = 0;
};

// NOTE: The table of watchers is NOT saved/loaded!  Recreate these links on restore
void ReportPositionChanged( CBaseEntity *pMovedEntity );
void WatchPositionChanges( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity );
void RemovePositionWatcher( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity );

// inherit from this interface to be able to call WatchPositionChanges
class IVPhysicsWatcher : public IWatcherCallback
{
public:
	virtual void NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake ) = 0;
};

// NOTE: The table of watchers is NOT saved/loaded!  Recreate these links on restore
void ReportVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake );
void WatchVPhysicsStateChanges( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity );
void RemoveVPhysicsStateWatcher( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity );

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Spawn Flags
inline constexpr auto SF_POSTPROCESS_MASTER = 0x0001;

//=============================================================================
//
// Class Postprocess Controller:
//
class CPostProcessController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PostProcessController" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_CLASS( CPostProcessController, CBaseEntity );

	CPostProcessController();
	virtual ~CPostProcessController();

	// Parse data from a map file
	void Activate_Impl();
	int UpdateTransmitState_Impl();

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

	void Spawn_Impl( void );

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
		m_hMasterController = nullptr;
	}

	~CPostProcessSystem()
	{
		m_hMasterController = nullptr;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	void FireGameEvent( IGameEvent *pEvent );
	CPostProcessController *GetMasterPostProcessController( void )			{ return m_hMasterController; }

private:

	void InitMasterController( void );
	CHandle< CPostProcessController > m_hMasterController;
};

CPostProcessSystem *PostProcessSystem( void );

using namespace ::ks::reflect;

class CBaseTempEntity;

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------

class CTEBaseBeam : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseBeam", .base = false } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CTEBaseBeam, CBaseTempEntity );
	DECLARE_SERVERCLASS();

public:
					CTEBaseBeam( const char *name );
	virtual			~CTEBaseBeam( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles ) = 0;
	
public:
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nHaloIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nStartFrame, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nFrameRate, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fLife, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6 } ]] );
	CNetworkVar( float, m_fWidth, [[= ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 128.0 } ]] );
	CNetworkVar( float, m_fEndWidth, [[= ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 128.0 } ]] );
	CNetworkVar( int, m_nFadeLength, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fAmplitude, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 64.0 } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, a, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nSpeed, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

EXTERN_SEND_TABLE(DT_BaseBeam);

class CEffectData;



void DispatchEffect( const char *pName, const CEffectData &data );
void DispatchEffect( IRecipientFilter& filter, float flDelay, const char *pName, const CEffectData &data );

class CBaseTempEntity;

class CTEParticleSystem : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEParticleSystem" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEParticleSystem, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEParticleSystem(const char *pName) : BaseClass(pName)
	{
		m_vecOrigin.GetForModify().Init();
	}

	CNetworkVectorXYZ( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .index = 2 } ]]  [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .index = 1 } ]]  [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .index = 0 } ]] );
};

class CBaseEntity;
class CBasePlayer;
class CCSBot;
class CCSPlayer;
class CTeamSpawnPoint;

using namespace ::ks::reflect;


// Purpose: Team management class. Contains all the details for a specific team


// named by the BareArray<> annotation below; both live in team.cpp
void SendProxy_PlayerList( const SendProp *pProp, const void *pStruct, const void *pData,
                           DVariant *pOut, int iElement, int objectID );
int  SendProxyArrayLength_PlayerArray( const void *pStruct, int objectID );

class CTeam : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Team", .base = false } ]]
	[[= ks::reflect::BareArray<"player_array_element", "\"player_array\"", MAX_PLAYERS, 0, 4,
	ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED },
	SendProxy_PlayerList, SendProxyArrayLength_PlayerArray>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CTeam, CBaseEntity );
public:
	CTeam( void );
	virtual ~CTeam( void );

	DECLARE_SERVERCLASS();

	void Precache_Impl( void ) { return; };

	void Think_Impl( void );
	int  UpdateTransmitState_Impl( void );

	//-----------------------------------------------------------------------------
	// Initialization
	//-----------------------------------------------------------------------------
	virtual void		Init( const char *pName, int iNumber );

	//-----------------------------------------------------------------------------
	// Data Handling
	//-----------------------------------------------------------------------------
	virtual int			GetTeamNumber( void ) const;
	virtual void		SetName( const char *pName );
	virtual const char *GetName( void );
	virtual void		UpdateClientData( CBasePlayer *pPlayer );
	virtual int		ShouldTransmitToPlayer( CBasePlayer* pRecipient, CBaseEntity* pEntity );
	virtual void		SetClanName( const char *pName );
	virtual const char *GetClanName( void );
	virtual void		SetClanID( uint32 iClanID );
	virtual uint32		GetClanID( void );
	virtual void		SetFlagImageString( const char *pName );
	virtual const char *GetFlagImageString( void );
	virtual void		SetLogoImageString( const char *pName );
	virtual const char *GetLogoImageString( void );
	virtual void		SetNumMapVictories( int numVictories );

	//-----------------------------------------------------------------------------
	// Spawnpoints
	//-----------------------------------------------------------------------------
	virtual void InitializeSpawnpoints( void );
	virtual void AddSpawnpoint( CTeamSpawnPoint *pSpawnpoint );
	virtual void RemoveSpawnpoint( CTeamSpawnPoint *pSpawnpoint );
	virtual CBaseEntity *SpawnPlayer( CBasePlayer *pPlayer );

	//-----------------------------------------------------------------------------
	// Players
	//-----------------------------------------------------------------------------
	virtual void InitializePlayers( void );
	virtual void AddPlayer( CBasePlayer *pPlayer );
	virtual void RemovePlayer( CBasePlayer *pPlayer );
	virtual int  GetNumPlayers( void );
	virtual CBasePlayer *GetPlayer( int iIndex );
	static int TeamGGSortFunction( CCSPlayer* const *entry1, CCSPlayer* const *entry2 );
	virtual void DetermineGGLeaderAndSort( void );
	virtual int GetGGLeader( int nTeam );

	//-----------------------------------------------------------------------------
	// Scoring
	//-----------------------------------------------------------------------------
	virtual void AddScore( int score )					{ m_scoreTotal += score; }
	virtual void AddScoreFirstHalf( int score )			{ m_scoreFirstHalf += score; }
	virtual void AddScoreSecondHalf( int score )		{ m_scoreSecondHalf += score; }
	virtual void AddScoreOvertime( int score )			{ m_scoreOvertime += score; }

	virtual void SetScore( int score )					{ m_scoreTotal = score; }
	virtual void SetScoreFirstHalf( int score )			{ m_scoreFirstHalf = score; }
	virtual void SetScoreSecondHalf( int score )		{ m_scoreSecondHalf = score; }
	virtual void SetScoreOvertime( int score )			{ m_scoreOvertime = score; }

	virtual int  GetScore( void )						{ return m_scoreTotal; }
	virtual int  GetScoreFirstHalf( void )				{ return m_scoreFirstHalf; }
	virtual int  GetScoreSecondHalf( void )				{ return m_scoreSecondHalf; }
	virtual int  GetScoreOvertime( void )				{ return m_scoreOvertime; }

	virtual void ResetScores( void );
	virtual void ResetTeamLeaders( void );

	void MarkSurrendered() { m_bSurrendered = 1; }

	void AwardAchievement( int iAchievement );

	virtual int GetAliveMembers( void );

	virtual int GetBotMembers( std::vector< class CCSBot* > *pOutVecBots = nullptr );
	virtual int GetHumanMembers( std::vector< class CCSPlayer* > *pOutVecPlayers = nullptr );

	float m_flLastPlayerSortTime;
	static int m_nStaticGGLeader_CT;
	static int m_nStaticGGLeader_T;

	int m_nLastGGLeader_CT;
	int m_nLastGGLeader_T;

public:
	std::vector< CTeamSpawnPoint * > m_aSpawnPoints;
	std::vector< CBasePlayer * >		m_aPlayers;

	// Data
	CNetworkString( m_szTeamname, MAX_TEAM_NAME_LENGTH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szClanTeamname, MAX_TEAM_NAME_LENGTH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szTeamFlagImage, MAX_TEAM_FLAG_ICON_LENGTH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szTeamLogoImage, MAX_TEAM_LOGO_ICON_LENGTH, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szTeamMatchStat, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_numMapVictories, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( uint32, m_iClanID, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_bSurrendered, [[= ks::reflect::Net{ .bits = 0 } ]] );
	CNetworkVar( int, m_scoreTotal, [[= ks::reflect::Net{ .bits = 0 } ]] );
	CNetworkVar( int, m_scoreFirstHalf, [[= ks::reflect::Net{ .bits = 0 } ]] );
	CNetworkVar( int, m_scoreSecondHalf, [[= ks::reflect::Net{ .bits = 0 } ]] );	
	CNetworkVar( int, m_scoreOvertime, [[= ks::reflect::Net{ .bits = 0 } ]] );
	int		m_iDeaths;

	CNetworkVar( int, m_nGGLeaderEntIndex_CT, [[= ks::reflect::Net{ .bits = 0 } ]] );
	CNetworkVar( int, m_nGGLeaderEntIndex_T, [[= ks::reflect::Net{ .bits = 0 } ]] );
	bool	m_bGGHasLeader_CT;
	bool	m_bGGHasLeader_T;

	// Spawnpoints
	int		m_iLastSpawn;		// Index of the last spawnpoint used

	CNetworkVar( int, m_iTeamNum, [[= ks::reflect::Net{ .bits = 5 } ]] );			// Which team is this?
};

extern std::vector< CTeam * > g_Teams;
extern CTeam *GetGlobalTeam( int iIndex );
extern int GetNumberOfTeams( void );
extern const char* GetTeamName( int iTeam );

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: Team spawnpoint entity


//-----------------------------------------------------------------------------
// Purpose: points at which the player can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamSpawnPoint : public CPointEntity
{
public:
	DECLARE_CLASS( CTeamSpawnPoint, CPointEntity );

	void	Activate_Impl( void );
	virtual bool	IsValid( CBasePlayer *pPlayer );

	[[= ks::reflect::Key{ .name = "OnPlayerSpawn" } ]] COutputEvent m_OnPlayerSpawn;

protected:	
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int		m_iDisabled;

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: points at which vehicles can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamVehicleSpawnPoint : public CTeamSpawnPoint
{
	DECLARE_CLASS( CTeamVehicleSpawnPoint, CTeamSpawnPoint );
public:
	void	Activate_Impl( void );
	bool	IsValid( void );

	[[= ks::reflect::Key{ .name = "OnVehicleSpawn" } ]] COutputEvent m_OnVehicleSpawn;

	DECLARE_DATADESC();
};

class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;

using namespace ::ks::reflect;


//
// teamplay_gamerules.h
//

inline constexpr auto MAX_TEAMNAME_LENGTH = 16;

inline constexpr auto TEAMPLAY_TEAMLISTLENGTH = MAX_TEAMS*MAX_TEAMNAME_LENGTH;

class CTeamplayRules : public CMultiplayRules
{
public:
	DECLARE_CLASS( CTeamplayRules, CMultiplayRules );

	// Return the value of this player towards capturing a point
	virtual int	 GetCaptureValueForPlayer( CBasePlayer *pPlayer ) { return 1; }
	virtual bool TeamMayCapturePoint( int iTeam, int iPointIndex ) { return true; }
	virtual bool PlayerMayCapturePoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = nullptr, int iMaxReasonLength = 0 ) { return true; }
	virtual bool PlayerMayBlockPoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = nullptr, int iMaxReasonLength = 0 ) { return false; }

	// Return false if players aren't allowed to cap points at this time (i.e. in WaitingForPlayers)
	virtual bool PointsMayBeCaptured( void ) { return true; }
	virtual void SetLastCapPointChanged( int iIndex ) { return; }

	CTeamplayRules();
	virtual ~CTeamplayRules() {};

	virtual void Precache( void );

	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual bool IsTeamplay( void );
	virtual bool FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly );
	virtual bool ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void InitHUD( CBasePlayer *pl );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual const char *GetGameDescription( void ) { return "Teamplay"; }  // this is the game name that gets seen in the server browser
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void Think ( void );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual const char *GetIndexedTeamName( int teamIndex );
	virtual bool IsValidTeam( const char *pTeamName );
	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );
	virtual void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, bool bKill, bool bGib );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual bool TimerMayExpire( void ) { return true; }

	// A game has been won by the specified team
	virtual void SetWinningTeam( int team, int iWinReason, bool bForceMapReset = true, bool bSwitchTeams = false, bool bDontAddScore = false ) { return; }
	virtual void SetStalemate( int iReason, bool bForceMapReset = true, bool bSwitchTeams = false ) { return; }

	// Used to determine if all players should switch teams
	virtual void SetSwitchTeams( bool bSwitch ){ m_bSwitchTeams = bSwitch; }
	virtual bool ShouldSwitchTeams( void ){ return m_bSwitchTeams; }
	virtual void HandleSwitchTeams( void ){ return; }

	// Used to determine if we should scramble the teams
	virtual void SetScrambleTeams( bool bScramble ){ m_bScrambleTeams = bScramble; }
	virtual bool ShouldScrambleTeams( void ){ return m_bScrambleTeams; }
	virtual void HandleScrambleTeams( void ){ return; }

protected:
	bool m_DisableDeathMessages;

private:
	void RecountTeams( void );
	const char *TeamWithFewestPlayers( void );

	bool m_DisableDeathPenalty;
	bool m_teamLimit;				// This means the server set only some teams as valid
	char m_szTeamList[TEAMPLAY_TEAMLISTLENGTH];
	bool m_bSwitchTeams;
	bool m_bScrambleTeams;

};

inline CTeamplayRules* TeamplayGameRules()
{
	return static_cast<CTeamplayRules*>(g_pGameRules);
}

using namespace ::ks::reflect;

// Purpose: round based game rule convars shared by the multiplayer rules.

extern ConVar mp_respawnwavetime;

	extern ConVar mp_showroundtransitions;
	extern ConVar mp_enableroundwaittime;
	extern ConVar mp_showcleanedupents;
	extern ConVar mp_bonusroundtime;
	extern ConVar mp_restartround;
	extern ConVar mp_winlimit;
	extern ConVar mp_stalemate_timelimit;
	extern ConVar mp_stalemate_enable;

// Utility function
bool FindInList( const char **pStrings, const char *pToFind );

class CBaseEntity;
class CPointTemplate;


// Purpose: Template entities are used by spawners to create copies of entities
//			that were configured by the level designer. This allows us to spawn
//			entities with arbitrary sets of key/value data and entity I/O
//			connections.


int			Templates_Add(CBaseEntity *pEntity, const char *pszMapData, int nLen, int nHammerID=-1);
string_t	Templates_FindByIndex( int iIndex );
int			Templates_GetStringSize( int iIndex );
string_t	Templates_FindByTargetName(const char *pszName);
void		Templates_ReconnectIOForGroup( CPointTemplate *pGroup );

// Some templates have Entity I/O connecting the entities within the template.
// Unique versions of these templates need to be created whenever they're instanced.
void		Templates_StartUniqueInstance( void );
bool		Templates_IndexRequiresEntityIOFixup( int iIndex );
char		*Templates_GetEntityIOFixedMapData( int iIndex );

// Used by Foundry.
void		Templates_RemoveByHammerID( int nHammerID );

// Save / Restore

inline constexpr auto VOICE_MAX_PLAYERS = MAX_PLAYERS;
inline constexpr auto VOICE_MAX_PLAYERS_DW = ((VOICE_MAX_PLAYERS / 32) + !!(VOICE_MAX_PLAYERS & 31));
inline constexpr auto VOICE_DEFAULT_PROXIMITY_RANGE = 1200; //100 feet

class CBasePlayer;


class IVoiceGameMgrHelper
{
public:
	virtual				~IVoiceGameMgrHelper() {}

	// Called each frame to determine which players are allowed to hear each other.	This overrides
	// whatever squelch settings players have.
	virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity ) = 0;
};

// CVoiceGameMgr manages which clients can hear which other clients.
class CVoiceGameMgr
{
public:
						CVoiceGameMgr();
	virtual				~CVoiceGameMgr();
	
	bool				Init(
		IVoiceGameMgrHelper *m_pHelper,
		int maxClients
		);

	void				SetHelper(IVoiceGameMgrHelper *pHelper);

	// Updates which players can hear which other players.
	// If gameplay mode is DM, then only players within the PVS can hear each other.
	// If gameplay mode is teamplay, then only players on the same team can hear each other.
	// Player masks are always applied.
	void				Update(double frametime);

	// Called when a new client connects (unsquelches its entity for everyone).
	void				ClientConnected(struct edict_t *pEdict);

	// Called on ClientCommand. Checks for the squelch and unsquelch commands.
	// Returns true if it handled the command.
	bool				ClientCommand(CBasePlayer *pPlayer, const CCommand &args );

	bool				CheckProximity( int iDistance );
	void				SetProximityDistance( int iDistance );

	bool				IsPlayerIgnoringPlayer( int iTalker, int iListener );

private:

	// Force it to update the client masks.
	void				UpdateMasks();

private:
	IVoiceGameMgrHelper	*m_pHelper;
	int					m_nMaxPlayers;
	double				m_UpdateInterval;						// How long since the last update.
	int					m_iProximityDistance;
};

// Use this to access CVoiceGameMgr.
CVoiceGameMgr* GetVoiceGameMgr();

class CVoteController;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: Player-driven Voting System for Multiplayer Source games (currently implemented for TF2)

inline constexpr auto MAX_CREATE_ERROR_STRING = 96;

// TODO: look into doing enum instead of string compares here - mtw
inline constexpr const char *VOTEISSUE_NAME_KICK = "Kick";
inline constexpr const char *VOTEISSUE_NAME_CHANGELEVEL = "ChangeLevel";
inline constexpr const char *VOTEISSUE_NAME_NEXTLEVEL = "NextLevel";
inline constexpr const char *VOTEISSUE_NAME_SWAPTEAMS = "SwapTeams";
inline constexpr const char *VOTEISSUE_NAME_SCRAMBLE = "ScrambleTeams";
inline constexpr const char *VOTEISSUE_NAME_RESTARTGAME = "RestartGame";
inline constexpr const char *VOTEISSUE_NAME_SURRENDER = "Surrender";
inline constexpr const char *VOTEISSUE_NAME_REMATCH = "Rematch";
inline constexpr const char *VOTEISSUE_NAME_CONTINUE = "ContinueGame";
inline constexpr const char *VOTEISSUE_NAME_PAUSEMATCH = "PauseMatch";
inline constexpr const char *VOTEISSUE_NAME_UNPAUSEMATCH = "UnpauseMatch";
inline constexpr const char *VOTEISSUE_NAME_LOADBACKUP = "LoadBackup";
inline constexpr const char *VOTEISSUE_NAME_READYFORMATCH = "ReadyForMatch";
inline constexpr const char *VOTEISSUE_NAME_NOTREADYFORMATCH = "NotReadyForMatch";
inline constexpr const char *VOTEISSUE_NAME_STARTTIMEOUT = "StartTimeOut";


extern CVoteController *g_voteControllerGlobal;
extern CVoteController *g_voteControllerCT;
extern CVoteController *g_voteControllerT;

class CBaseIssue	// Abstract base class for all things-that-can-be-voted-on.  
{
public:
	CBaseIssue( const char *typeString, CVoteController *pVoteController );
	virtual ~CBaseIssue();
	const char			*GetTypeString( void );						// Connection between console command and specific type of issue
	virtual const char	*GetDetailsString();
	virtual void		SetIssueDetails( const char *pszDetails );	// We need to know the details part of the con command for later
	virtual void		OnVoteFailed( void );						// The moment the vote fails, also has some time for feedback before the window goes away
	virtual void		OnVoteStarted( void ) {}					// Called as soon as the vote starts
	virtual bool		IsEnabled( void ) { return false; }			// Query the issue to see if it's enabled
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return false; } // Query if the issue is supported in queued matchmaking mode
	virtual bool		IsEnabledDuringWarmup( void )		{ return false; } // Can this vote be called during warmup?
	virtual float		GetCommandDelay( void );	
	virtual bool		ShouldIgnoreCreationTimer( void ) { return false; }	// should this issue ignore sv_vote_creation_timer that prevents spamming callvotes?
	virtual bool		CanTeamCallVote( int iTeam ) const;			// Can someone on the given team call this vote?
	virtual bool		CanCallVote( int iEntIndex, const char *pszCommand, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime ); // Can this guy hold a vote on this issue?
	virtual bool		IsAllyRestrictedVote( void );				// Can only members of the same team vote on this?
	virtual bool		IsUnanimousVoteToPass( void ) {return false; }	// Requires all potential voters to pass
	virtual bool		IsVoteCallExclusiveToSpectators( void ) { return false; }		// Whether only spectators can call the vote
	virtual int			GetVotesRequiredToPass( void );					// how many votes are required to pass
	virtual const char *GetDisplayString( void ) = 0;				// The string that will be passed to the client for display
	virtual const char *GetOtherTeamDisplayString( void ) = 0;				// The string that will be passed to the client for a vote being cast by the other team
	virtual void		ExecuteCommand( void ) = 0;					// Where the magic happens.  Do your thing.
	virtual void		ListIssueDetails( CBasePlayer *pForWhom ) = 0;	// Someone would like to know all your valid details
	virtual const char *GetVotePassedString( void );				// Get the string an issue would like to display when it passes.
	virtual int			CountPotentialVoters( void );
	virtual int			GetNumberVoteOptions( void );				// How many choices this vote will have.  i.e. Returns 2 on a Yes/No issue (the default).
	virtual bool		IsYesNoVote( void );
	virtual void		SetYesNoVoteCount( int iNumYesVotes, int iNumNoVotes, int iNumPotentialVotes );
	virtual bool		GetVoteOptions( std::vector< const char* > &vecNames );	// We use this to generate options for voting
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_UNDEFINED; }
	virtual float		GetFailedVoteLockOutTime( void );			// How long to wait before a failed vote can be resubmitted.
	virtual vote_create_failed_t MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode ) { return eDefaultFailCode; }

protected:
	static void			ListStandardNoArgCommand( CBasePlayer *forWhom, const char *issueString );		// List a Yes vote command

	struct FailedVote
	{
		char	szFailedVoteCommand[MAX_COMMAND_LENGTH];
		char	szFailedVoteParameter[MAX_VOTE_DETAILS_LENGTH];
		float	flLockoutTime;					
	};

	std::vector< FailedVote * > m_FailedVotes;

	char				m_szTypeString[MAX_COMMAND_LENGTH];
	char				m_szDetailsString[MAX_PATH];

	int m_iNumYesVotes;
	int m_iNumNoVotes;
	int m_iNumPotentialVotes;

	CVoteController *m_pVoteController;

};

class CVoteController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_VoteController" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CVoteController, CBaseEntity );
	
public:
	DECLARE_SERVERCLASS();

	virtual ~CVoteController();

	enum TryCastVoteResult
	{
		CAST_OK,
		CAST_FAIL_SERVER_DISABLE,
		CAST_FAIL_NO_ACTIVE_ISSUE,
		CAST_FAIL_TEAM_RESTRICTED,
		CAST_FAIL_NO_CHANGES,
		CAST_FAIL_DUPLICATE,
		CAST_FAIL_VOTE_CLOSED,
		CAST_FAIL_SYSTEM_ERROR
	};

	void	Spawn_Impl( void );
	int		UpdateTransmitState_Impl( void );

	bool			SetupVote( int iEntIndex );	// This creates a list of issues for the UI
	bool			CreateVote( int iEntIndex, const char *pszTypeString, const char *pszDetailString );	// This is what the UI passes in
	TryCastVoteResult TryCastVote( int iEntIndex, const char *pszVoteString );
	float			GetAcceptingVotesTimeLeft() { return m_acceptingVotesTimer.GetRemainingTime(); }
	void			RegisterIssue( CBaseIssue *pNewIssue );
	void			ListIssues( CBasePlayer *pForWhom );
	bool			IsValidVoter( CBasePlayer *pWhom );
	bool			CanTeamCastVote( int iTeam ) const;
	void			SendVoteFailedMessage( vote_create_failed_t nReason = VOTE_FAILED_GENERIC, CBasePlayer *pVoteCaller = nullptr, int nTime = -1 );
	void			VoteChoice_Increment( int nVoteChoice );
	void			VoteChoice_Decrement( int nVoteChoice );
	int				GetWinningVoteOption( void );
	int				GetCallingEntity( void )  { return m_iEntityHoldingVote; }
	int				GetPotentialVotes( void ) { return m_nPotentialVotes.Get(); }
	bool			IsAVoteInProgress( void ) { return ( m_iActiveIssueIndex != INVALID_ISSUE ); }
	bool			HasIssue( const char *pszIssue );
	void			EndVoteImmediately( void );

protected:
	void			ResetData( void );
	void			VoteControllerThink( void );
	void			CheckForEarlyVoteClose( void ); // If everyone has voted (and changing votes is not allowed) then end early

	CNetworkVar( int, m_iActiveIssueIndex, [[= ks::reflect::Net{ .bits = -1 } ]] );					// Type of thing being voted on
	CNetworkVar( int, m_iOnlyTeamToVote, [[= ks::reflect::Net{ .bits = -1 } ]] );						// If an Ally restricted vote, the team number that is allowed to vote
	CNetworkArray( int, m_nVoteOptionCount, MAX_VOTE_OPTIONS, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );	// Vote options counter
	CNetworkVar( int, m_nPotentialVotes, [[= ks::reflect::Net{ .bits = -1 } ]] );						// How many votes could come in, so we can close ballot early
	CNetworkVar( bool, m_bIsYesNoVote, [[= ks::reflect::Net{} ]] );						// Is the current issue Yes/No?
	CountdownTimer	m_acceptingVotesTimer;						// How long from vote start until we count the ballots
	CountdownTimer	m_executeCommandTimer;						// How long after end of vote time until we execute a passed vote
	CountdownTimer	m_resetVoteTimer;							// when the current vote will end 
	std::vector< uint64 > m_arrVotedUsers;						// SteamIDs of users who voted already
	int				m_nVotesCast[MAX_PLAYERS];					// votes cast by each entity
	int				m_iEntityHoldingVote;
	int				m_nHighestCountIndex;
	
	std::vector< CBaseIssue * > m_potentialIssues;
	std::vector< const char * > m_VoteOptions;
};
