// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include <stddef.h>
#include "studio.h"
#include "animation.h"
#include "bone_setup.h"
#include "datacache/idatacache.h"
#include "eiface.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "vscript/ivscript.h"
#include "baseentity.h"
#include "predictable_entity.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "igameevents.h"
#include "string_t.h"
#include "declare_class.h"
#include "gamestringpool.h"
#include "vphysics/constraints.h"
#include "bspfile.h"
#include "const.h"
#include "vphysics_interface.h"
#include "props_shared.h"
#include "bone_accessor.h"
#include "vcollide_parse.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
module ks.server:vscript;

import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import std;
import ks.core.basetypes;
import ks.core.threadtools;
import ks.tier1.datamanager;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.game.ai_activity;
import :baseentity;
import ks.game.shareddefs;
import ks.math.ssequaternion;
import ks.tier1.createinterface;
import ks.game.igamemovement;
import ks.tier1.convar;
import ks.core.types;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.kvpacker;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import ks.core.platform;
import ks.core.wchartypes;
import ks.core.vprof;
import ks.tier1.envelope;
import ks.core.tslist;
import ks.core.logging;
import :econ.econ_item_schema_minimal;

struct animevent_t;


int ExtractBbox( CStudioHdr *pstudiohdr, int sequence, Vector& mins, Vector& maxs );

void IndexModelSequences( CStudioHdr *pstudiohdr );
void ResetActivityIndexes( CStudioHdr *pstudiohdr );
void VerifySequenceIndex( CStudioHdr *pstudiohdr );
int SelectWeightedSequence( CStudioHdr *pstudiohdr, int activity, int curSequence = -1 );

// Picks the sequence with the most matching modifiers.
int SelectWeightedSequenceFromModifiers( CStudioHdr *pstudiohdr, int activity, const ActivityModifierSym_t *pActivityModifiers, int iModifierCount );
int SelectHeaviestSequence( CStudioHdr *pstudiohdr, int activity );
void SetEventIndexForSequence( mstudioseqdesc_t &seqdesc );
void BuildAllAnimationEventIndexes( CStudioHdr *pstudiohdr );
void ResetEventIndexes( CStudioHdr *pstudiohdr );
float GetSequenceLinearMotionAndDuration( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec );

void GetEyePosition( CStudioHdr *pstudiohdr, Vector &vecEyePosition );

int LookupActivity( CStudioHdr *pstudiohdr, const char *label );
int LookupSequence( CStudioHdr *pstudiohdr, const char *label );

inline constexpr auto NOMOTION = 99999;
void GetSequenceLinearMotion( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec );

const char *GetSequenceName( CStudioHdr *pstudiohdr, int sequence );
const char *GetSequenceActivityName( CStudioHdr *pstudiohdr, int iSequence );

int GetSequenceFlags( CStudioHdr *pstudiohdr, int sequence );
int GetAnimationEvent( CStudioHdr *pstudiohdr, int sequence, animevent_t *pNPCEvent, float flStart, float flEnd, int index );
bool HasAnimationEventOfType( CStudioHdr *pstudiohdr, int sequence, int type );

int FindTransitionSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, int iGoalSequence, int *piDir );
bool GotoSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, float flCurrentCycle, float flCurrentRate, int iGoalSequence, int &nNextSequence, float &flNextCycle, int &iNextDir );

void SetBodygroup( CStudioHdr *pstudiohdr, int& body, int iGroup, int iValue );
int GetBodygroup( CStudioHdr *pstudiohdr, int body, int iGroup );
void SetBodygroupPreset( CStudioHdr *pstudiohdr, int& body, char const *szName );

const char *GetBodygroupName( CStudioHdr *pstudiohdr, int iGroup );
int FindBodygroupByName( CStudioHdr *pstudiohdr, const char *name );
const char *GetBodygroupPartName( CStudioHdr *pstudiohdr, int iGroup, int iPart );
int GetBodygroupCount( CStudioHdr *pstudiohdr, int iGroup );
int GetNumBodyGroups( CStudioHdr *pstudiohdr );

int GetSequenceActivity( CStudioHdr *pstudiohdr, int sequence, int *pweight = nullptr );

void GetAttachmentLocalSpace( CStudioHdr *pstudiohdr, int attachIndex, matrix3x4_t &pLocalToWorld );

float SetBlending( CStudioHdr *pstudiohdr, int sequence, int *pblendings, int iBlender, float flValue );

int FindHitboxSetByName( CStudioHdr *pstudiohdr, const char *name );
const char *GetHitboxSetName( CStudioHdr *pstudiohdr, int setnumber );
int GetHitboxSetCount( CStudioHdr *pstudiohdr );

enum animtag_indices
{
	ANIMTAG_INVALID = -1,
	ANIMTAG_UNINITIALIZED = 0,
	ANIMTAG_STARTCYCLE_N,
	ANIMTAG_STARTCYCLE_NE,
	ANIMTAG_STARTCYCLE_E,
	ANIMTAG_STARTCYCLE_SE,
	ANIMTAG_STARTCYCLE_S,
	ANIMTAG_STARTCYCLE_SW,
	ANIMTAG_STARTCYCLE_W,
	ANIMTAG_STARTCYCLE_NW,
	ANIMTAG_AIMLIMIT_YAWMIN_IDLE,
	ANIMTAG_AIMLIMIT_YAWMAX_IDLE,
	ANIMTAG_AIMLIMIT_YAWMIN_WALK,
	ANIMTAG_AIMLIMIT_YAWMAX_WALK,
	ANIMTAG_AIMLIMIT_YAWMIN_RUN,
	ANIMTAG_AIMLIMIT_YAWMAX_RUN,
	ANIMTAG_AIMLIMIT_YAWMIN_CROUCHIDLE,
	ANIMTAG_AIMLIMIT_YAWMAX_CROUCHIDLE,
	ANIMTAG_AIMLIMIT_YAWMIN_CROUCHWALK,
	ANIMTAG_AIMLIMIT_YAWMAX_CROUCHWALK,
	ANIMTAG_AIMLIMIT_PITCHMIN_IDLE,
	ANIMTAG_AIMLIMIT_PITCHMAX_IDLE,
	ANIMTAG_AIMLIMIT_PITCHMIN_WALKRUN,
	ANIMTAG_AIMLIMIT_PITCHMAX_WALKRUN,
	ANIMTAG_AIMLIMIT_PITCHMIN_CROUCH,
	ANIMTAG_AIMLIMIT_PITCHMAX_CROUCH,
	ANIMTAG_AIMLIMIT_PITCHMIN_CROUCHWALK,
	ANIMTAG_AIMLIMIT_PITCHMAX_CROUCHWALK,
	ANIMTAG_WEAPON_POSTLAYER,
	ANIMTAG_FLASHBANG_PASSABLE,
	ANIMTAG_COUNT
};

float GetFirstSequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flStart = 0, float flEnd = 1 );

float GetAnySequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flDefault );

class CAnimationLayer;
class CBaseAnimatingOverlay;
class CBaseEntity;
class CBoneMergeCache;
class CCycler;
class CFlexCycler;
struct animevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

inline constexpr auto BCF_NO_ANIMATION_SKIP = ( 1 << 0 ); // Do not allow PVS animation skipping (mostly for attachments being critical to an entity)
inline constexpr auto BCF_IS_IN_SPAWN = ( 1 << 1 ); // Is currently inside of spawn, always evaluate animations

extern IDataCache *datacache;

namespace DT_ServerAnimationData { extern SendTable g_SendTable; }
void *SendProxy_ClientSideAnimation( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients,
    int objectID );

class CBaseAnimating : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ServerAnimationData", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_BaseAnimating" } ]]
	[[= ks::reflect::From<"m_bClientSideRagdoll", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::SubTable<"serveranimdata", &DT_ServerAnimationData::g_SendTable, SendProxy_ClientSideAnimation, false>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseAnimating, CBaseEntity );

	CBaseAnimating();
	~CBaseAnimating();

	DECLARE_PREDICTABLE();

	enum
	{
		NUM_POSEPAREMETERS = 24,
		NUM_BONECTRLS = 4
	};

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	DECLARE_ENT_SCRIPTDESC();

	void SetModel_Impl( const char *szModelName );
	void Activate_Impl();
	void Spawn_Impl();
	void Precache_Impl();
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	void OnRestore_Impl();

	CStudioHdr *GetModelPtr( void );
	virtual void InvalidateMdlCache() { UnlockStudioHdr(); delete m_pStudioHdr; m_pStudioHdr = nullptr; }

	CBaseAnimating*	GetBaseAnimating_Impl() { return this; }

	// Cycle access
	void ForceCycle( float flCycle );
	void SetCycle( float flCycle );
	float GetCycle() const;

	float	GetAnimTimeInterval( void ) const;

	// Call this in your constructor to tell it that you will not use animtime. Then the
	// interpolation will be done correctly on the client.
	// This defaults to off.
	void	UseClientSideAnimation();

	// Tells whether or not we're using client-side animation. Used for controlling
	// the transmission of animtime.
	bool	IsUsingClientSideAnimation()	{ return m_bClientSideAnimation; }

	// Basic NPC Animation functions
	virtual float	GetIdealSpeed( ) const;
	virtual float	GetIdealAccel( ) const;
	virtual void	StudioFrameAdvance(); // advance animation frame to some time in the future
	void StudioFrameAdvanceManual( float flInterval );
	bool	IsValidSequence( int iSequence );
	virtual void	ReachedEndOfSequence() { return; }

	inline float					GetPlaybackRate() const;
	inline void						SetPlaybackRate( float rate );

	inline int GetSequence() { return m_nSequence; }
	// inline void SetSequence(int nSequence) { Assert( GetModelPtr( ) && nSequence >= 0 && nSequence < GetModelPtr( )->GetNumSeq() );  m_nSequence = nSequence; }
	void SetSequence(int nSequence);
	virtual void OnSequenceSet( int nOldSequence ) {}
	/* inline */ void ResetSequence(int nSequence);
	// FIXME: push transitions support down into CBaseAnimating?
	virtual bool IsActivityFinished( void ) { return m_bSequenceFinished; }
	inline bool IsSequenceFinished( void ) { return m_bSequenceFinished; }
	inline bool SequenceLoops( void ) { return m_bSequenceLoops; }
	bool		 IsSequenceLooping( CStudioHdr *pStudioHdr, int iSequence );
	inline bool	 IsSequenceLooping( int iSequence ) { return IsSequenceLooping(GetModelPtr(),iSequence); }
	inline float SequenceDuration( void ) { return SequenceDuration( m_nSequence ); }
	float	SequenceDuration( CStudioHdr *pStudioHdr, int iSequence );
	inline float SequenceDuration( int iSequence ) { return SequenceDuration(GetModelPtr(), iSequence); }
	float	GetSequenceCycleRate( CStudioHdr *pStudioHdr, int iSequence );
	inline float	GetSequenceCycleRate( int iSequence ) { return GetSequenceCycleRate(GetModelPtr(),iSequence); }
	float	GetLastVisibleCycle( CStudioHdr *pStudioHdr, int iSequence );
	virtual float	GetSequenceGroundSpeed( CStudioHdr *pStudioHdr, int iSequence );
	inline float GetSequenceGroundSpeed( int iSequence ) { return GetSequenceGroundSpeed(GetModelPtr(), iSequence); }
	virtual float	GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence ) { return GetSequenceCycleRate(GetModelPtr(),iSequence); }
	void	ResetActivityIndexes ( void );
	void    ResetEventIndexes ( void );
	int		SelectWeightedSequence ( Activity activity );
	int		SelectWeightedSequence ( Activity activity, int curSequence );
	int		SelectWeightedSequenceFromModifiers( Activity activity, const ActivityModifierSym_t *pActivityModifiers, int iModifierCount );
	int		SelectHeaviestSequence ( Activity activity );
	int		LookupActivity( const char *label );
	int		LookupSequence ( const char *label );
	int		LookupSequence ( CStudioHdr* pHdr, const char *label );
	float	GetFirstSequenceAnimTag( int sequence, int nDesiredTag, float flStart = 0, float flEnd = 1 );
	float	GetAnySequenceAnimTag( int sequence, int nDesiredTag, float flDefault );
	KeyValues *GetSequenceKeyValues( int iSequence );

	float GetSequenceMoveYaw( int iSequence );
	float GetSequenceMoveDist( CStudioHdr *pStudioHdr, int iSequence );
	inline float GetSequenceMoveDist( int iSequence ) { return GetSequenceMoveDist(GetModelPtr(),iSequence);}
	void  GetSequenceLinearMotion( int iSequence, Vector *pVec );
	const char *GetSequenceName( int iSequence );
	const char *GetSequenceActivityName( int iSequence );
	Activity GetSequenceActivity( int iSequence );

	void ResetSequenceInfo ( );
	// This will stop animation until you call ResetSequenceInfo() at some point in the future
	inline void StopAnimation( void ) { m_flPlaybackRate = 0; }

	virtual void ClampRagdollForce( const Vector &vecForceIn, Vector *vecForceOut ) { *vecForceOut = vecForceIn; } // Base class does nothing.
	virtual bool BecomeRagdollOnClient( const Vector &force );
	virtual bool IsRagdoll();
	virtual bool CanBecomeRagdoll( void ); //Check if this entity will ragdoll when dead.

	virtual	void GetSkeleton( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternionAligned q[], int boneMask );

	virtual void GetBoneTransform( int iBone, matrix3x4_t &pBoneToWorld );
	virtual void SetupBones( matrix3x4a_t *pBoneToWorld, int boneMask );
	virtual void CalculateIKLocks( float currentTime );
	void Teleport_Impl( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts = true );

	bool HasAnimEvent( int nSequence, int nEvent );
	virtual	void DispatchAnimEvents ( CBaseAnimating *eventHandler ); // Handle events that have happend since last time called up until X seconds into the future
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual bool HandleScriptedAnimEvent( animevent_t *pEvent ) { return false; }
	virtual bool HandleBehaviorAnimEvent( animevent_t *pEvent ) { return false; }

	int		LookupPoseParameter( CStudioHdr *pStudioHdr, const char *szName );
	inline int	LookupPoseParameter( const char *szName ) { return LookupPoseParameter(GetModelPtr(), szName); }

	float	SetPoseParameter( CStudioHdr *pStudioHdr, const char *szName, float flValue );
	inline float SetPoseParameter( const char *szName, float flValue ) { return SetPoseParameter( GetModelPtr(), szName, flValue ); }
	float	SetPoseParameter( CStudioHdr *pStudioHdr, int iParameter, float flValue );
	inline float SetPoseParameter( int iParameter, float flValue ) { return SetPoseParameter( GetModelPtr(), iParameter, flValue ); }

	float	GetPoseParameter( const char *szName );
	float	GetPoseParameter( int iParameter );
	bool	GetPoseParameterRange( int index, float &minValue, float &maxValue );
	bool	HasPoseParameter( int iSequence, const char *szName );
	bool	HasPoseParameter( int iSequence, int iParameter );
	float	EdgeLimitPoseParameter( int iParameter, float flValue, float flBase = 0.0f );

protected:
	// The modus operandi for pose parameters is that you should not use the const char * version of the functions
	// in general code -- it causes many many string comparisons, which is slower than you think. Better is to 
	// save off your pose parameters in member variables in your derivation of this function:
	virtual void	PopulatePoseParameters( void );

private:
	CBoneMergeCache					*m_pBoneMergeCache;

public:

	CBaseAnimating*	FindFollowedEntity();

	int  LookupBone( const char *szName );
	void GetBonePosition( const char *szName, Vector &origin, QAngle &angles );
	void GetBonePosition( int iBone, Vector &origin, QAngle &angles );
	
	void GetHitboxBonePosition( int iBone, Vector &origin, QAngle &angles, QAngle hitboxOrientation );
	void GetHitboxBoneTransform( int iBone, QAngle hitboxOrientation, matrix3x4_t &pOut );

	int	GetPhysicsBone( int boneIndex );

	int GetNumBones ( void );

	int  FindTransitionSequence( int iCurrentSequence, int iGoalSequence, int *piDir );
	bool GotoSequence( int iCurrentSequence, float flCurrentCycle, float flCurrentRate,  int iGoalSequence, int &iNextSequence, float &flCycle, int &iDir );
	int  GetEntryNode( int iSequence );
	int  GetExitNode( int iSequence );
	
	void GetEyeballs( Vector &origin, QAngle &angles ); // ?? remove ??

	int LookupAttachment( const char *szName );

	// These return the attachment in world space
	bool GetAttachment( const char *szName, Vector &absOrigin, QAngle &absAngles );
	bool GetAttachment( int iAttachment, Vector &absOrigin, QAngle &absAngles );
	int GetAttachmentBone( int iAttachment );
	virtual bool GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld );
	const Vector& ScriptGetAttachmentOrigin( int iAttachment );
	const Vector& ScriptGetAttachmentAngles( int iAttachment );

	// These return the attachment in the space of the entity
	bool GetAttachmentLocal( const char *szName, Vector &origin, QAngle &angles );
	bool GetAttachmentLocal( int iAttachment, Vector &origin, QAngle &angles );
	bool GetAttachmentLocal( int iAttachment, matrix3x4_t &attachmentToLocal );
	
	// Non-angle versions of the attachments in world space
	bool GetAttachment(  const char *szName, Vector &absOrigin, Vector *forward = nullptr, Vector *right = nullptr, Vector *up = nullptr );
	bool GetAttachment( int iAttachment, Vector &absOrigin, Vector *forward = nullptr, Vector *right = nullptr, Vector *up = nullptr );

	void SetBodygroup( int iGroup, int iValue );
	int GetBodygroup( int iGroup );
	void SetBodygroupPreset( char const *szName );
	int GetSkin() { return m_nSkin; }

	const char *GetBodygroupName( int iGroup );
	int FindBodygroupByName( const char *name );
	const char *GetBodygroupPartName( int iGroup, int iPart );
	int GetBodygroupCount( int iGroup );
	int GetNumBodyGroups( void );
	int CountBodyGroupVariants( int group );
	int FindBodyGroupVariant( int group, int variant );	///< Find undamaged bodygroup part index
	int FindDamagedBodyGroupVariant( int group );		///< Find a damaged version of the current part for the given bodygroup
	void RandomizeBodygroups( std::vector< const char * >& groups );

	void					SetHitboxSet( int setnum );
	void					SetHitboxSetByName( const char *setname );
	int						GetHitboxSet( void );
	char const				*GetHitboxSetName( void );
	int						GetHitboxSetCount( void );
	int						GetHitboxBone( int hitboxIndex );
	bool					LookupHitbox( const char *szName, int& outSet, int& outBox );

	// Computes a box that surrounds all hitboxes
	bool ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );
	bool ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes a box that surrounds a single hitboxes
	bool ComputeHitboxSurroundingBox( int iHitbox, Vector *pVecWorldMins, Vector *pVecWorldMaxs );
	
	// Clone a CBaseAnimating from another (copies model & sequence data)
	void CopyAnimationDataFrom( CBaseAnimating *pSource );

	int ExtractBbox( int sequence, Vector& mins, Vector& maxs );
	void SetSequenceBox( void );
	int RegisterPrivateActivity( const char *pszActivityName );

	void	ResetClientsideFrame( void );

// Controllers.
	virtual	void			InitBoneControllers ( void );
	
	// Return's the controller's angle/position in bone space.
	float					GetBoneController ( int iController );

	// Maps the angle/position value you specify into the bone's start/end and sets the specified controller to the value.
	float					SetBoneController ( int iController, float flValue );
	
	void					GetVelocity_Impl(Vector *vVelocity, AngularImpulse *vAngVelocity);

	// these two need to move somewhere else
	LocalFlexController_t GetNumFlexControllers( void );
	const char *GetFlexDescFacs( int iFlexDesc );
	const char *GetFlexControllerName( LocalFlexController_t iFlexController );
	const char *GetFlexControllerType( LocalFlexController_t iFlexController );

	virtual	Vector GetGroundSpeedVelocity( void );

	bool GetIntervalMovement( float flIntervalUsed, bool &bMoveSeqFinished, Vector &newPosition, QAngle &newAngles );
	bool GetSequenceMovement( int nSequence, float fromCycle, float toCycle, Vector &deltaPosition, QAngle &deltaAngles );
	float GetInstantaneousVelocity( float flInterval = 0.0 );
	float GetEntryVelocity( int iSequence );
	float GetExitVelocity( int iSequence );
	float GetMovementFrame( float flDist );
	bool HasMovement( int iSequence );

	void ReportMissingActivity( int iActivity );
	bool TestCollision_Impl( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	bool TestHitboxes_Impl( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	class CBoneCache *GetBoneCache( void );
	virtual void InvalidateBoneCache( void );
	int DrawDebugTextOverlays_Impl( void );
	virtual bool IsViewModel() const { return false; }
	
	// See note in code re: bandwidth usage!!!
	void				DrawServerHitboxes( float duration = 0.0f, bool monocolor = false );
	void				DrawRawSkeleton( matrix3x4_t boneToWorld[], int boneMask, bool noDepthTest = true, float duration = 0.0f, bool monocolor = false );

	void				SetModelScale( float scale, float change_duration = 0.0f, ModelScaleType_t scaleType = HIERARCHICAL_MODEL_SCALE );
	float				GetModelScale() const;
	float				GetModelHierarchyScale();	// Get the overall scale of the entire hierarchy (model scale can be local, per-bone)

	ModelScaleType_t GetModelScaleType() const;
	void SetModelScaleType( ModelScaleType_t scaleType );

	void				UpdateModelScale();
	
	// also calculate IK on server? (always done on client)
	void EnableServerIK();
	void DisableServerIK();

	// for ragdoll vs. car
	int GetHitboxesFrontside( int *boxList, int boxMax, const Vector &normal, float dist );

	void	ModifyOrAppendCriteria_Impl( AI_CriteriaSet& set );

	// Send a muzzle flash event to the client for this entity.
	void DoMuzzleFlash();

	// Fire
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	virtual void IgniteLifetime( float flFlameLifetime );
	virtual void IgniteUseCheapEffect( bool bUseCheapEffect );
	virtual void Extinguish() { RemoveFlag( FL_ONFIRE ); }
	bool IsOnFire() { return ( (GetFlags() & FL_ONFIRE) != 0 ); }
	void Scorch( int rate, int floor );
	[[= ks::reflect::Input{ .name = "Ignite", .type = FIELD_VOID } ]]
	[[= ks::reflect::Input{ .name = "IgniteNumHitboxFires", .type = FIELD_INTEGER } ]]
	[[= ks::reflect::Input{ .name = "IgniteHitboxFireScale", .type = FIELD_FLOAT } ]] void InputIgnite( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "IgniteLifetime", .type = FIELD_FLOAT } ]] void InputIgniteLifetime( inputdata_t &inputdata );
	void InputIgniteNumHitboxFires( inputdata_t &inputdata );
	void InputIgniteHitboxFireScale( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BecomeRagdoll", .type = FIELD_VOID } ]] void InputBecomeRagdoll( inputdata_t &inputdata );

	// Ice
	virtual bool	IsFrozen( void ) { return m_flFrozen >= 1.0f; }
	float			GetFrozenAmount( void ) const { return m_flFrozen; }
	float			GetFrozenThawRate( void ) { return m_flFrozenThawRate; }
	void			Thaw( float flThawAmount );
	void			ToggleFreeze(void);
	virtual void	Freeze( float flFreezeAmount = -1.0f, CBaseEntity *pFreezer = nullptr, Ray_t *pFreezeRay = nullptr );
	virtual void	Unfreeze();

	// Dissolve, returns true if the ragdoll has been created
	virtual bool Dissolve( const char *pMaterialName, float flStartTime, bool bNPCOnly = true, int nDissolveType = 0, Vector vDissolverOrigin = vec3_origin, int iMagnitude = 0 );
	bool IsDissolving() { return ( (GetFlags() & FL_DISSOLVING) != 0 ); }
	void TransferDissolveFrom( CBaseAnimating *pAnim );

	// animation needs
	float				m_flGroundSpeed;	// computed linear movement rate for current sequence
	float				m_flLastEventCheck;	// cycle index of when events were last checked

	virtual void SetLightingOriginRelative( CBaseEntity *pLightingOriginRelative );
	void SetLightingOriginRelative( string_t strLightingOriginRelative );
	CBaseEntity *GetLightingOriginRelative();

	virtual void SetLightingOrigin( CBaseEntity *pLightingOrigin );
	void SetLightingOrigin( string_t strLightingOrigin );
	CBaseEntity *GetLightingOrigin();

	const float* GetPoseParameterArray() { return m_flPoseParameter.Base(); }
	const float* GetEncodedControllerArray() { return m_flEncodedController.Base(); }

	void	SetFadeDistance( float minFadeDist, float maxFadeDist );

	int		GetBoneCacheFlags( void ) { return m_fBoneCacheFlags; }
	inline void	SetBoneCacheFlags( unsigned short fFlag ) { m_fBoneCacheFlags |= fFlag; }
	inline void	ClearBoneCacheFlags( unsigned short fFlag ) { m_fBoneCacheFlags &= ~fFlag; }

	bool PrefetchSequence( int iSequence );

private:
	void LockStudioHdr();
	void UnlockStudioHdr();

	void StudioFrameAdvanceInternal( CStudioHdr *pStudioHdr, float flInterval );
	[[= ks::reflect::Input{ .name = "SetLightingOriginHack", .type = FIELD_STRING } ]] void InputSetLightingOriginRelative( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetLightingOrigin", .type = FIELD_STRING } ]] void InputSetLightingOrigin( inputdata_t &inputdata );

public:
	bool CanSkipAnimation( void );

public:

	CNetworkVar( int, m_nForceBone, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVector( m_vecForce, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	CNetworkVar( int, m_nSkin, [[= ks::reflect::Net{ .bits = ANIMATION_SKIN_BITS } ]] [[= ks::reflect::Key{ .name = "ModelSkin" } ]] [[= ks::reflect::Key{ .name = "skin", .input = true } ]] );
	CNetworkVar( int, m_nBody, [[= ks::reflect::Net{ .bits = ANIMATION_BODY_BITS } ]] [[= ks::reflect::Key{ .name = "body" } ]] [[= ks::reflect::Key{ .name = "SetBodyGroup", .input = true } ]] );
	CNetworkVar( int, m_nHitboxSet, [[= ks::reflect::Net{ .bits = ANIMATION_HITBOXSET_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "hitboxset" } ]] );

	// For making things thin during barnacle swallowing, e.g.
	CNetworkVar( float, m_flModelScale, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "ModelScale" } ]] );

	// was pev->framerate
	CNetworkVar( float, m_flPlaybackRate, [[= ks::reflect::Net{ .bits = ANIMATION_PLAYBACKRATE_BITS, .low = -4.0, .high = 12.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "playbackrate" } ]] );

private:
	CNetworkVar( ModelScaleType_t, m_ScaleType, [[= ks::reflect::Net{ .bits = -1 } ]] );

public:
	void InitStepHeightAdjust( void );
	void SetIKGroundContactInfo( float minHeight, float maxHeight );
	void UpdateStepOrigin( void );

protected:
	float				m_flIKGroundContactTime;
	float				m_flIKGroundMinHeight;
	float				m_flIKGroundMaxHeight;

	float				m_flEstIkFloor; // debounced
	float				m_flEstIkOffset;

  	CIKContext			*m_pIk;
	int					m_iIKCounter;

public:
	Vector	GetStepOrigin_Impl( void ) const;
	QAngle	GetStepAngles_Impl( void ) const;

protected:
	bool				m_bSequenceFinished;// flag set when StudioAdvanceFrame moves across a frame boundry
	bool				m_bSequenceLoops;	// true if the sequence loops
	float				m_flDissolveStartTime;

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_bClientSideRagdoll );

	// was pev->frame
	CNetworkVar( float, m_flCycle, [[= ks::reflect::Net{ .bits = ANIMATION_CYCLE_BITS, .low = 0.0f, .high = 1.0f, .flags = SPROP_CHANGES_OFTEN|SPROP_ROUNDDOWN, .table = "DT_ServerAnimationData" } ]] [[= ks::reflect::Key{ .name = "cycle" } ]] );
	CNetworkVar( int, m_nSequence, [[= ks::reflect::Net{ .bits = ANIMATION_SEQUENCE_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "sequence" } ]] );	
	CNetworkArray( float, m_flPoseParameter, NUM_POSEPAREMETERS, [[= ks::reflect::Net{ .bits = ANIMATION_POSEPARAMETER_BITS, .low = 0.0f, .high = 1.0f } ]] );	// must be private so manual mode works!
	CNetworkArray( float, m_flEncodedController, NUM_BONECTRLS, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 1.0f, .flags = SPROP_ROUNDDOWN } ]] );		// bone controller setting (0..1)

	// Client-side animation (useful for looping animation objects)
	CNetworkVar( bool, m_bClientSideAnimation, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bClientSideFrameReset, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( int, m_nNewSequenceParity, [[= ks::reflect::Net{ .bits = EF_PARITY_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nResetEventsParity, [[= ks::reflect::Net{ .bits = EF_PARITY_BITS, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( bool, m_bSuppressAnimSounds, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "SuppressAnimSounds" } ]] );

	// Incremented each time the entity is told to do a muzzle flash.
	// The client picks up the change and draws the flash.
	CNetworkVar( unsigned char, m_nMuzzleFlashParity, [[= ks::reflect::Net{ .bits = EF_MUZZLEFLASH_BITS, .flags = SPROP_UNSIGNED } ]] );

	CNetworkHandle( CBaseEntity, m_hLightingOrigin, [[= ks::reflect::Net{} ]] );
	CNetworkHandle( CBaseEntity, m_hLightingOriginRelative );

	[[= ks::reflect::Key{ .name = "LightingOriginHack" } ]] string_t m_iszLightingOriginRelative;	// for reading from the file only
	[[= ks::reflect::Key{ .name = "LightingOrigin" } ]] string_t m_iszLightingOrigin;			// for reading from the file only

	memhandle_t		m_boneCacheHandle;
	unsigned short	m_fBoneCacheFlags;		// Used for bone cache state on model

	CNetworkVar( float, m_flFrozen, [[= ks::reflect::Net{ .bits = 32 } ]] );		// 0 - 1 amount that the model is frozen
	float				m_flMovementFrozen;	// How frozen are the movement parts
	float				m_flAttackFrozen;	// How frozen are the attacking parts
	float				m_flFrozenThawRate;	// amount it unfreezes per second
	float				m_flFrozenMax;		// maximum amount this entitiy is allowed to freeze

public:
	[[= ks::reflect::Key{ .name = "OnIgnite" } ]] COutputEvent m_OnIgnite;

private:
	CStudioHdr			*m_pStudioHdr;
	std::recursive_mutex	m_StudioHdrInitLock;
	std::recursive_mutex	m_BoneSetupMutex;

// FIXME: necessary so that cyclers can hack m_bSequenceFinished
friend class CBaseAnimatingOverlay;
friend class CFlexCycler;
friend class CCycler;
friend class CBlendingCycler;
};

//-----------------------------------------------------------------------------
// Purpose: Serves the 90% case of calling SetSequence / ResetSequenceInfo.
//-----------------------------------------------------------------------------

/*
void CBaseAnimating::ResetSequence(int nSequence)
{
	if ( nSequence != m_nSequence )
	{
		m_nSequence = nSequence;
		InvalidatePhysicsRecursive( SEQUENCE_CHANGED );
	}
	ResetSequenceInfo();
}
*/

float CBaseAnimating::GetPlaybackRate() const
{

	// Slow the animation while partially frozen
	return m_flPlaybackRate * clamp( 1.0f - m_flFrozen, 0.0f, 1.0f );
}

void CBaseAnimating::SetPlaybackRate( float rate )
{
	// if we start going in a new direction, the current animation isn't finished anymore
	m_bSequenceFinished = false;
	m_flPlaybackRate = rate;
}

EXTERN_SEND_TABLE(DT_BaseAnimating);

class CBasePlayer;
class CUserCmd;

//-----------------------------------------------------------------------------
// Game systems are singleton objects in the client + server codebase responsible for
// various tasks
// The order in which the server systems appear in this list are the
// order in which they are initialized and updated. They are shut down in
// reverse order from which they are initialized.
//-----------------------------------------------------------------------------

// UNDONE: Do these need GameInit/GameShutdown as well?
// UNDONE: Remove the Pre/Post entity semantics and rely on system ordering?
// FIXME: Remove all ifdef CLIENT_DLL if we can...
class IGameSystem
{
public:
	// GameSystems are expected to implement these methods.
	virtual char const *Name() = 0;

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() = 0;
	virtual void PostInit() = 0;
	virtual void Shutdown() = 0;

	// Level init, shutdown
	virtual void LevelInitPreEntity() = 0;
	// entities are created / spawned / precached here
	virtual void LevelInitPostEntity() = 0;

	virtual void LevelShutdownPreEntity() = 0;
	// Entities are deleted / released here...
	virtual void LevelShutdownPostEntity() = 0;
	// end of level shutdown
	
	// Called during game save
	virtual void OnSave() = 0;

	// Called during game restore, after the local player has connected and entities have been fully restored
	virtual void OnRestore() = 0;

	// Called every frame. It's safe to remove an igamesystem from within this callback.
	virtual void SafeRemoveIfDesired() = 0;

	virtual bool	IsPerFrame() = 0;

	// destructor, cleans up automagically....
	virtual ~IGameSystem();

	// Client systems can use this to get at the map name
	static char const*	MapName();

	// These methods are used to add and remove server systems from the
	// main server loop. The systems are invoked in the order in which
	// they are added.
	static void Add ( IGameSystem* pSys );
	static void Remove ( IGameSystem* pSys );
	static void RemoveAll (  );

	// These methods are used to initialize, shutdown, etc all systems
	static bool InitAllSystems();
	static void PostInitAllSystems();
	static void ShutdownAllSystems();
	static void LevelInitPreEntityAllSystems( char const* pMapName );
	static void LevelInitPostEntityAllSystems();
	static void LevelShutdownPreEntityAllSystems();
	static void LevelShutdownPostEntityAllSystems();

	static void OnSaveAllSystems();
	static void OnRestoreAllSystems();

	static void SafeRemoveIfDesiredAllSystems();

	static void FrameUpdatePreEntityThinkAllSystems();
	static void FrameUpdatePostEntityThinkAllSystems();
	static void PreClientUpdateAllSystems();

	// Accessors for the above function
	static CBasePlayer *RunCommandPlayer();
	static CUserCmd *RunCommandUserCmd();
};

class IGameSystemPerFrame : public IGameSystem
{
public:
	// destructor, cleans up automagically....
	virtual ~IGameSystemPerFrame();

	// Called each frame before entities think
	virtual void FrameUpdatePreEntityThink() = 0;
	// called after entities think
	virtual void FrameUpdatePostEntityThink() = 0;
	virtual void PreClientUpdate() = 0;
};

// Quick and dirty server system for users who don't care about precise ordering
// and usually only want to implement a few of the callbacks
class CBaseGameSystem : public IGameSystem
{
public:

	virtual char const *Name() { return "unnamed"; }

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() { return true; }
	virtual void PostInit() {}
	virtual void Shutdown() {}

	// Level init, shutdown
	virtual void LevelInitPreEntity() {}
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	virtual bool	IsPerFrame() { return false; }
private:

	// Prevent anyone derived from CBaseGameSystem from implementing these, they need
	//  to derive from CBaseGameSystemPerFrame below!!!
	// Called each frame before entities think
	virtual void FrameUpdatePreEntityThink() {}
	// called after entities think
	virtual void FrameUpdatePostEntityThink() {}
	virtual void PreClientUpdate() {}
};

// Quick and dirty server system for users who don't care about precise ordering
// and usually only want to implement a few of the callbacks
class CBaseGameSystemPerFrame : public IGameSystemPerFrame
{
public:
	virtual char const *Name() { return "unnamed"; }

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() { return true; }
	virtual void PostInit() {}
	virtual void Shutdown() {}

	// Level init, shutdown
	virtual void LevelInitPreEntity() {}
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	virtual bool	IsPerFrame() { return true; }

	// Called each frame before entities think
	virtual void FrameUpdatePreEntityThink() { }
	// called after entities think
	virtual void FrameUpdatePostEntityThink() { }
	virtual void PreClientUpdate() { }
};

// Quick and dirty server system for users who don't care about precise ordering
// and usually only want to implement a few of the callbacks
class CAutoGameSystem : public CBaseGameSystem
{
public:
	CAutoGameSystem( char const *name = nullptr );	// hooks in at startup, no need to explicitly add
	CAutoGameSystem		*m_pNext;

	virtual char const *Name() { return m_pszName ? m_pszName : "unnamed"; }

private:
	char const *m_pszName;
};

//-----------------------------------------------------------------------------
// Purpose: This is a CAutoGameSystem which also cares about the "per frame" hooks
//-----------------------------------------------------------------------------
class CAutoGameSystemPerFrame : public CBaseGameSystemPerFrame
{
public:
	CAutoGameSystemPerFrame( char const *name = nullptr );
	CAutoGameSystemPerFrame *m_pNext;

	virtual char const *Name() { return m_pszName ? m_pszName : "unnamed"; }
	
private:
	char const *m_pszName;
};

class CBaseEntity;



//-----------------------------------------------------------------------------
// Purpose: Allows an entity to access its owner regardless of entity type 
//-----------------------------------------------------------------------------
class IHasOwner
{
public:
	virtual CBaseEntity			*GetOwnerViaInterface( void ) = 0;
};

class CBaseEntity;
class CBasePlayer;



enum LagCompensationType
{
	LAG_COMPENSATE_BOUNDS,
	LAG_COMPENSATE_HITBOXES,
	LAG_COMPENSATE_HITBOXES_ALONG_RAY,
};

//-----------------------------------------------------------------------------
// Purpose: This is also an IServerSystem
//-----------------------------------------------------------------------------
class ILagCompensationManager
{
public:
	// Called during player movement to set up/restore after lag compensation
	virtual void	StartLagCompensation(
		CBasePlayer *player,
		LagCompensationType lagCompensationType,
		const Vector& weaponPos = vec3_origin,
		const QAngle &weaponAngles = vec3_angle,
		float weaponRange = 0.0f ) = 0;
	virtual void	FinishLagCompensation( CBasePlayer *player ) = 0;

	// Mappers can flag certain additional entities to lag compensate, this handles them
	virtual void	AddAdditionalEntity( CBaseEntity *pEntity ) = 0;
	virtual void	RemoveAdditionalEntity( CBaseEntity *pEntity ) = 0;
};

extern ILagCompensationManager *lagcompensation;

// This side's move helper. srceng links both sides, so the instance cannot live in ks.game.
inline IMoveHelper *g_pMoveHelper = nullptr;

inline IMoveHelper *MoveHelper()
{
	return g_pMoveHelper;
}

extern void GameDLLInit( void );

extern ConVar	displaysoundlist;
extern ConVar	mapcyclefile;
extern ConVar	servercfgfile;
extern ConVar	lservercfgfile;

// multiplayer server rules
extern ConVar	teamplay;
extern ConVar	fraglimit;
extern ConVar	falldamage;
extern ConVar	weaponstay;
extern ConVar	forcerespawn;
extern ConVar	footsteps;
extern ConVar	flashlight;
extern ConVar	aimcrosshair;
extern ConVar	teamlist;
extern ConVar	teamoverride;
extern ConVar	defaultteam;
extern ConVar	allowNPCs;

extern ConVar	suitvolume;

// Engine Cvars
extern const ConVar *g_pDeveloper;

//=============================================================================
//
// Smoothly blends the timescale through an engine interface
//
class CGameTimescale : public CAutoGameSystemPerFrame
{
public:

	enum Interpolators_e
	{
		INTERPOLATOR_LINEAR,
		INTERPOLATOR_ACCEL,
		INTERPOLATOR_DEACCEL,
		INTERPOLATOR_EASE_IN_OUT,
	};

	// Creation/Destruction.
	CGameTimescale();
	~CGameTimescale();

	// Initialization/Shutdown.
	virtual bool Init();	
	virtual void Shutdown();

	virtual void FrameUpdatePostEntityThink();

	// Level init, shutdown
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPostEntity();

	float GetCurrentTimescale( void ) const { return m_flCurrentTimescale; }
	float GetDesiredTimescale( void ) const { return m_flDesiredTimescale; }

	// Set the timescale to an exact value without doing a ramp in/out blend
	void SetCurrentTimescale( float flTimescale );

	// Sets the desired timescale and will automatically ramp in/out
	void SetDesiredTimescaleAtTime( float flDesiredTimescale, float flDurationRealTimeSeconds = 0.0f, Interpolators_e nInterpolatorType = INTERPOLATOR_LINEAR, float flStartBlendTime = 0.0f );
	void SetDesiredTimescale( float flDesiredTimescale, float flDurationRealTimeSeconds = 0.0f, Interpolators_e nInterpolatorType = INTERPOLATOR_LINEAR, float flDelayRealtime = 0.0f );

private:

	void UpdateTimescale( void );
	void ResetTimescale( void );

private:

	float m_flDesiredTimescale;
	float m_flCurrentTimescale;

	float m_flDurationRealTimeSeconds;
	Interpolators_e m_nInterpolatorType;

	float m_flStartTimescale;
	float m_flStartBlendTime;
	float m_flStartBlendRealtime;
};

CGameTimescale *GameTimescale();

extern IGameEventManager2 *gameeventmanager;

// A safer method than registering with the event manager directly.
// Avoids requiring the user to remove themselves as listeners in 
// their deconstructor, and sets the serverside variable based on
// our dll location.
//
// Events are delivered to the FireGameEvent( IGameEvent * ) member of the type that
// ListenForGameEvent() is called on. A class that registers through a base class (so the
// call is made on the base type) must call SetGameEventHandler( this ) in its constructor
// before anything registers.
class CGameEventListener
{
public:
	CGameEventListener() : m_bRegisteredForEvents(false)
	{
	}

	~CGameEventListener()
	{
		StopListeningForAllEvents();
	}

	// Bind T::FireGameEvent as the function the event manager calls for this listener.
	template< class T >
	void SetGameEventHandler( T *self )
	{
		m_Handler.emplace( std::cw<&T::FireGameEvent>, self );
	}

	void ListenForGameEvent( this auto &self, const char *name )
	{
		CGameEventListener &listener = self;

		if ( !listener.m_Handler )
			listener.SetGameEventHandler( &self );

		listener.m_bRegisteredForEvents = true;

		bool bServerSide = true;

		gameeventmanager->AddListener( &listener, *listener.m_Handler, name, bServerSide );
	}

	void ListenForAllGameEvents( this auto &self )
	{
		CGameEventListener &listener = self;

		if ( !listener.m_Handler )
			listener.SetGameEventHandler( &self );

	bool bServerSide = true;

		gameeventmanager->AddListenerGlobal( &listener, *listener.m_Handler, bServerSide );
	}

	void StopListeningForAllEvents()
	{
		// remove me from list
		if ( m_bRegisteredForEvents )
		{
			if ( gameeventmanager )
				gameeventmanager->RemoveListener( this );

			m_bRegisteredForEvents = false;
		}
	}

private:

	// What the event manager calls
	std::optional< GameEventCallback_t > m_Handler;

	// Have we registered for any events?
	bool m_bRegisteredForEvents;
};

class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;

const int GAMESTATS_VERSION = 1;

enum StatSendType_t
{
	STATSEND_LEVELSHUTDOWN,
	STATSEND_APPSHUTDOWN
};

struct StatsBufferRecord_t
{
	float m_flFrameRate;									// fps
	float m_flServerPing;									// client ping to server
	float m_flMainThreadTimeMS;								// time in ms taken by the main thread
	float m_flMainThreadWaitTimeMS;							// time in ms the main thread is waiting for the render thread (cf EndFrame)
	float m_flRenderThreadTimeMS;							// time in ms taken by the render thread
	float m_flRenderThreadWaitTimeMS;						// time in ms waiting for the gpu (time spent in ForceHardwareSync)

	bool operator< ( const StatsBufferRecord_t &a) const
	{
		return m_flFrameRate < a.m_flFrameRate;
	}
};

class CFpsHistory
{
public:
	enum FpsEnum_t 
	{
		FPS_120,
		FPS_90,
		FPS_60,
		FPS_45,
		FPS_30,
		FPS_20,
		FPS_10,
		FPS_LOW,
		FPS_BIN_COUNT
	};

	enum HistoryBufSizeEnum_t { HISTORY_BUF_SIZE = 32 };
	struct Rec_t
	{
		float m_flFps;
		float m_flCpuWait;
	};

	CFpsHistory()
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	uint8 Update( float flFps, float flCpuWait );

	uint8 operator []( int i ) const 
	{
		return m_History[ ( m_nNext + HISTORY_BUF_SIZE - 1 - i ) % HISTORY_BUF_SIZE ];
	}

	int m_nNext;
	uint8 m_History[ HISTORY_BUF_SIZE ];

};

class CFpsHistogram
{
public:
	enum FpsEnum_t
	{
		FPS_BIN_COUNT = CFpsHistory::FPS_BIN_COUNT
	};

	CFpsHistogram()
	{
		Reset();
	}
	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
	}
	void Update( uint8 nBin );
	int64 Encode()const;
protected:
	uint32 m_nFps[ FPS_BIN_COUNT ];
	uint32 m_nCpuWaits[ FPS_BIN_COUNT ];
};

class CFpsSelectiveHistogram: public CFpsHistogram
{
public:
	CFpsSelectiveHistogram();

	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	void Fire( int nLookAhead, int nLookBack, const CFpsHistory &history );

	void Update( uint8 nBin );
	int GetInactiveCount() const { return m_nLookBack; }
	void ResetInactiveCount() { m_nLookBack = 0; }

protected:
	int m_nLookAhead; // this is how many more frames we need to collect statistics (stay active)
	int m_nLookBack; // this is how many frames we skipped (did not collect statistics)
};

enum PerfStatsEventEnum_t
{
	PERF_STATS_SMOKE,
	PERF_STATS_BULLET,
	PERF_STATS_PLAYER,
	PERF_STATS_PLAYER_SPAWN,
	PERF_STATS_EVENT_COUNT
};

inline constexpr auto STATS_WINDOW_SIZE = ( 60 * 10 ); // # of records to hold
inline constexpr auto STATS_TRAILING_WINDOW_SIZE = ( 30 ); // # of records to hold in trailing window prior to stat recording
inline constexpr auto STATS_RECORD_INTERVAL = 1; // # of seconds between data grabs. 2 * 300 = every 10 minutes

class CGameStats;

void UpdatePerfStats( void );
void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead = CFpsHistory::HISTORY_BUF_SIZE, int nLookBack = CFpsHistory::HISTORY_BUF_SIZE );
void SetGameStatsHandler( CGameStats *pGameStats );

inline constexpr auto GAMESTATS_STANDARD_NOT_SAVED = 0xFEEDBEEF;

enum GameStatsVersions_t
{
	GAMESTATS_FILE_VERSION_OLD = 001,
	GAMESTATS_FILE_VERSION_OLD2,
	GAMESTATS_FILE_VERSION_OLD3,
	GAMESTATS_FILE_VERSION_OLD4,
	GAMESTATS_FILE_VERSION_OLD5,
	GAMESTATS_FILE_VERSION
};

struct BasicGameStatsRecord_t
{
public:
	BasicGameStatsRecord_t() :
	  m_nCount( 0 ),
		  m_nSeconds( 0 ),
		  m_nCommentary( 0 ),
		  m_nHDR( 0 ),
		  m_nCaptions( 0 ),
		  m_bSteam( true ),
		  m_bCyberCafe( false ),
		  m_nDeaths( 0 )
	  {
		  V_memset( m_nSkill, 0, sizeof( m_nSkill ) );
	  }

	  void		Clear();

	  void		SaveToBuffer( ks::byte_writer& buf );
	  bool		ParseFromBuffer( ks::byte_reader& buf, int iBufferStatsVersion );

	  // Data
public:
	int			m_nCount;
	int			m_nSeconds;

	int			m_nCommentary;
	int			m_nHDR;
	int			m_nCaptions;
	int			m_nSkill[ 3 ];
	bool		m_bSteam;
	bool		m_bCyberCafe;
	int			m_nDeaths;
};

struct BasicGameStats_t
{
public:
	BasicGameStats_t() :
		  m_nSecondsToCompleteGame( 0 ),
		  m_bSteam( true ),
		  m_bCyberCafe( false ),
		  m_nDXLevel( 0 )
	  {
	  }

	  void						Clear();

	  void						SaveToBuffer( ks::byte_writer& buf );
	  bool						ParseFromBuffer( ks::byte_reader& buf, int iBufferStatsVersion );

	  BasicGameStatsRecord_t	*FindOrAddRecordForMap( char const *mapname );

	  // Data
public:
	int							m_nSecondsToCompleteGame; // 0 means they haven't finished playing yet

	BasicGameStatsRecord_t		m_Summary;			// Summary record
	ks::idict< BasicGameStatsRecord_t > m_MapTotals;
	bool						m_bSteam;
	bool						m_bCyberCafe;
	int							m_nDXLevel;
};

class CBaseGameStats 
{
public:
	CBaseGameStats();

	// override this to declare what format you want to send.  New products should use new format.
	virtual bool UseOldFormat() 
	{ 
		return true;		// servers by default send old format for backward compat
	}

	// Implement this if you support new format gamestats.
	// Return true if you added data to KeyValues, false if you have no data to report
	virtual bool AddDataForSend( KeyValues *pKV, StatSendType_t sendType ) { return false; }

	// These methods used for new format gamestats only and control when data gets sent.
	virtual bool ShouldSendDataOnLevelShutdown()
	{
		// by default, servers send data at every level change and clients don't
		return true;
	}
	virtual bool ShouldSendDataOnAppShutdown();

	virtual void Event_Init( void );
	virtual void Event_Shutdown( void );
	virtual void Event_MapChange( const char *szOldMapName, const char *szNewMapName );
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_SaveGame( void );
	virtual void Event_LoadGame( void );

	void StatsLog( char const *fmt, ... );
	
	// This is the first call made, so that we can "subclass" the CBaseGameStats based on gamedir as needed (e.g., ep2 vs. episodic)
	virtual CBaseGameStats *OnInit( CBaseGameStats *pCurrentGameStats, char const *gamedir ) { return pCurrentGameStats; }

	// Frees up data from gamestats and resets it to a clean state.
	virtual void Clear( void );

	virtual bool StatTrackingEnabledForMod( void ) { return false; } //Override this to turn on the system. Stat tracking is disabled by default and will always be disabled at the user's request
	static bool StatTrackingAllowed( void ); //query whether stat tracking is possible and warranted by the user
	virtual bool HaveValidData( void ) { return true; } // whether we currently have an interesting enough data set to upload.  Called at upload time; if false, data is not uploaded.

	virtual bool ShouldTrackStandardStats( void ) { return true; } //exactly what was tracked for EP1 release
	
	//Get mod specific strings used for tracking, defaults should work fine for most cases
	virtual const char *GetStatSaveFileName( void );
	virtual const char *GetStatUploadRegistryKeyName( void );
	const char *GetUserPseudoUniqueID( void );

	virtual bool UserPlayedAllTheMaps( void ) { return false; } //be sure to override this to determine user completion time

	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );	
	virtual void Event_PlayerConnected( CBasePlayer *pBasePlayer );
	virtual void Event_PlayerDisconnected( CBasePlayer *pBasePlayer );
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );
	virtual void Event_Credits();
	virtual void Event_Commentary();
	virtual void Event_CrateSmashed();
	virtual void Event_Punted( CBaseEntity *pObject );
	virtual void Event_PlayerTraveled( CBasePlayer *pBasePlayer, float distanceInInches, bool bInVehicle, bool bSprinting );
	virtual void Event_WeaponFired( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName );
	virtual void Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info );
	virtual void Event_PreSaveGameLoaded( char const *pSaveName, bool bInGame );
	virtual void Event_PlayerEnteredGodMode( CBasePlayer *pBasePlayer );
	virtual void Event_PlayerEnteredNoClip( CBasePlayer *pBasePlayer );
	virtual void Event_DecrementPlayerEnteredNoClip( CBasePlayer *pBasePlayer );
	virtual void Event_IncrementCountedStatistic( const Vector& vecAbsOrigin, char const *pchStatisticName, float flIncrementAmount );	
	virtual void Event_WindowShattered( CBasePlayer *pPlayer );

	//custom data to tack onto existing stats if you're not doing a complete overhaul
	virtual void AppendCustomDataToSaveBuffer( ks::byte_writer &SaveBuffer ) { } //custom data you want thrown into the default save and upload path
	virtual void LoadCustomDataFromBuffer( ks::byte_reader &LoadBuffer ) { }; //when loading the saved stats file, this will point to where you started saving data to the save buffer

	virtual void LoadingEvent_PlayerIDDifferentThanLoadedStats( void ); //Only called if you use the base SaveToFileNOW() and LoadFromFile() functions. Used in case you want to keep/invalidate data that was just loaded. 

	virtual bool LoadFromFile( void ); //called just before Event_Init()
	virtual bool SaveToFileNOW( bool bForceSyncWrite = false ); //saves buffers to their respective files now, returns success or failure
	virtual bool UploadStatsFileNOW( void ); //uploads data to the CSER now, returns success or failure

	static bool AppendLump( int nMaxLumpCount, ks::byte_writer &SaveBuffer, unsigned short iLump, unsigned short iLumpCount, std::size_t nSize, void *pData );
	static bool GetLumpHeader( int nMaxLumpCount, ks::byte_reader &LoadBuffer, unsigned short &iLump, unsigned short &iLumpCount, bool bPermissive = false );
	static void LoadLump( ks::byte_reader &LoadBuffer, unsigned short iLumpCount, std::size_t nSize, void *pData );

	//default save behavior is to save on level shutdown, and game shutdown
	virtual bool AutoSave_OnInit( void ) { return false; }
	virtual bool AutoSave_OnShutdown( void ) { return true; }
	virtual bool AutoSave_OnMapChange( void ) { return false; }
	virtual bool AutoSave_OnLevelInit( void ) { return false; }
	virtual bool AutoSave_OnLevelShutdown( void ) { return true; }

	//default upload behavior is to upload on game shutdown
	virtual bool AutoUpload_OnInit( void ) { return false; }
	virtual bool AutoUpload_OnShutdown( void ) { return true; }
	virtual bool AutoUpload_OnMapChange( void ) { return false; }
	virtual bool AutoUpload_OnLevelInit( void ) { return false; }
	virtual bool AutoUpload_OnLevelShutdown( void ) { return false; }

	// Helper for builtin stuff
	void SetCyberCafeStatistic( bool bIsCyberCafeUser );
	void SetHDRStatistic( bool bHDREnabled );
	void SetCaptionsStatistic( bool bClosedCaptionsEnabled );
	void SetSkillStatistic( int iSkillSetting );
	void SetDXLevelStatistic( int iDXLevel );
public:
	BasicGameStats_t m_BasicStats; //exposed in case you do a complete overhaul and still want to save it
	bool			m_bLogging : 1;
	bool			m_bLoggingToFile : 1;
};

extern CBaseGameStats *gamestats; //starts out pointing at a singleton of the class above, overriding this in any constructor should work for replacing it

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Color correction entity.

// Spawn Flags
inline constexpr auto SF_COLORCORRECTION_MASTER = 0x0001;
inline constexpr auto SF_COLORCORRECTION_CLIENTSIDE = 0x0002;

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
void SendProxy_Origin( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class CColorCorrection : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ColorCorrection", .base = false } ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector }, SendProxy_Origin>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CColorCorrection, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CColorCorrection();

	void Spawn_Impl( void );
	int  UpdateTransmitState_Impl();
	void Activate_Impl( void );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

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
		m_hMasterController = nullptr;
	}

	~CColorCorrectionSystem()
	{
		m_hMasterController = nullptr;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	void FireGameEvent( IGameEvent *pEvent );
	CColorCorrection *GetMasterColorCorrection( void )			{ return m_hMasterController; }

private:

	void InitMasterController( void );
	CHandle< CColorCorrection > m_hMasterController;
};

CColorCorrectionSystem *ColorCorrectionSystem( void );

//=========================================================
// Plane
//=========================================================
class CPlane 
{
public:
	CPlane ( void );

	//=========================================================
	// InitializePlane - Takes a normal for the plane and a
	// point on the plane and 
	//=========================================================
	void InitializePlane ( const Vector &vecNormal, const Vector &vecPoint );

	//=========================================================
	// PointInFront - determines whether the given vector is 
	// in front of the plane. 
	//=========================================================
	bool PointInFront ( const Vector &vecPoint );

	//=========================================================
	// How far off the plane is this point?
	//=========================================================
	float PointDist( const Vector &vecPoint );

private:
	Vector	m_vecNormal;
	float	m_flDist;
	bool	m_fInitialized;
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Used to influence the initial force for a dying NPC's ragdoll.
//			Passive entity. Just represents position in the world, radius, force

inline constexpr auto SF_RAGDOLLMAGNET_BAR = 0x00000002; // this is a bar magnet.

class CRagdollMagnet : public CPointEntity
{
public:
	DECLARE_CLASS( CRagdollMagnet, CPointEntity );
	DECLARE_DATADESC();

	Vector GetForceVector( CBaseEntity *pNPC );
	float GetRadius( void ) { return m_radius; }
	Vector GetAxisVector( void ) { return m_axis - GetAbsOrigin(); }
	float DistToPoint( const Vector &vecPoint );

	bool IsEnabled( void ) { return !m_bDisabled; }
	
	int IsBarMagnet( void ) { return (m_spawnflags & SF_RAGDOLLMAGNET_BAR); }

	static CRagdollMagnet *FindBestMagnet( CBaseEntity *pNPC );

	void Enable( bool bEnable ) { m_bDisabled = !bEnable; }

	// Inputs
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bDisabled;
	[[= ks::reflect::Key{ .name = "radius" } ]] float	m_radius;
	[[= ks::reflect::Key{ .name = "force" } ]] float	m_force;
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector	m_axis;
};

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: System to generate events as specified entities become visible to players.

// A callback which will automatically be used instead of generating the generic
// 'on_entity_visible' game event. Often used so that the entity can generate
// its own, more unique game event. 
// Return true if you want the generic event generated after the callback
// Return false if you do not want the generic event generated
typedef bool (*VisibilityMonitorCallback)( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );

// This callback tells us which object is being seen, and by which client. The callback returns 'true'
// to allow visibility monitor to acknowledge the event, or false to suppress the event.
typedef bool (*VisibilityMonitorEvaluator)( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );

extern void VisibilityMonitor_AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator );
extern void VisibilityMonitor_AddEntity_NotVisibleThroughGlass( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator );
extern void VisibilityMonitor_RemoveEntity( CBaseEntity *pEntity );

class CBaseEntity;


//-----------------------------------------------------------------------------
// Purpose: Class handling generic damage modification to & from a player
//-----------------------------------------------------------------------------
class CDamageModifier
{
public:
				CDamageModifier();

	void		AddModifierToEntity( CBaseEntity *pChar );
	void		RemoveModifier();

	void		SetModifier( float flDamageScale );
	float		GetModifier() const;

	void		SetDoneToMe( bool bDoneToMe );
	bool		IsDamageDoneToMe() const;

	CBaseEntity	*GetCharacter() const;

private:
	float							m_flModifier;
	CHandle<CBaseEntity>			m_hEnt;
	bool							m_bDoneToMe;	// True = modifies damage done to the entity, false = damage done by the entity
};

class CBaseEntity;
class CBasePlayer;


// Purpose: APIs for player pickup of physics objects

// Reasons behind a pickup
enum PhysGunPickup_t
{
	PICKED_UP_BY_CANNON,
	PUNTED_BY_CANNON,
	PICKED_UP_BY_PLAYER, // Picked up by +USE, not physgun.
};

// Reasons behind a drop
enum PhysGunDrop_t
{
	DROPPED_BY_PLAYER,
	THROWN_BY_PLAYER,
	DROPPED_BY_CANNON,
	LAUNCHED_BY_CANNON,
};

enum PhysGunForce_t
{
	PHYSGUN_FORCE_DROPPED,	// Dropped by +USE
	PHYSGUN_FORCE_THROWN,	// Thrown from +USE
	PHYSGUN_FORCE_PUNTED,	// Punted by cannon
	PHYSGUN_FORCE_LAUNCHED,	// Launched by cannon
};

void PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject );
void Pickup_ForcePlayerToDropThisObject( CBaseEntity *pTarget );

void Pickup_OnPhysGunDrop( CBaseEntity *pDroppedObject, CBasePlayer *pPlayer, PhysGunDrop_t reason );
void Pickup_OnPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
bool Pickup_OnAttemptPhysGunPickup( CBaseEntity *pPickedUpObject, CBasePlayer *pPlayer, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
bool Pickup_GetPreferredCarryAngles( CBaseEntity *pObject, CBasePlayer *pPlayer, matrix3x4_t &localToWorld, QAngle &outputAnglesWorldSpace );
bool Pickup_ForcePhysGunOpen( CBaseEntity *pObject, CBasePlayer *pPlayer );
bool Pickup_ShouldPuntUseLaunchForces( CBaseEntity *pObject, PhysGunForce_t reason );
AngularImpulse Pickup_PhysGunLaunchAngularImpulse( CBaseEntity *pObject, PhysGunForce_t reason );
Vector Pickup_DefaultPhysGunLaunchVelocity( const Vector &vecForward, float flMass );
Vector Pickup_PhysGunLaunchVelocity( CBaseEntity *pObject, const Vector &vecForward, PhysGunForce_t reason );

CBaseEntity	*Pickup_OnFailedPhysGunPickup( CBaseEntity *pPickedUpObject, Vector vPhysgunPos );

class IPlayerPickupVPhysics
{
public:
	// Callbacks for the physgun/cannon picking up an entity
	virtual bool			OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) = 0;
	virtual CBaseEntity		*OnFailedPhysGunPickup( Vector vPhysgunPos ) = 0;
	virtual void			OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) = 0;
	virtual void			OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason ) = 0;
	virtual bool			HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer = nullptr ) = 0;
	virtual QAngle			PreferredCarryAngles( void )  = 0;
	virtual bool			ForcePhysgunOpen( CBasePlayer *pPlayer ) = 0;
	virtual AngularImpulse	PhysGunLaunchAngularImpulse() = 0;
	virtual bool			ShouldPuntUseLaunchForces( PhysGunForce_t reason ) = 0;
	virtual Vector			PhysGunLaunchVelocity( const Vector &vecForward, float flMass ) = 0;
};

class CDefaultPlayerPickupVPhysics : public IPlayerPickupVPhysics
{
public:
	virtual bool			OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) { return true; }
	virtual CBaseEntity		*OnFailedPhysGunPickup( Vector vPhysgunPos ) { return nullptr; }
	virtual void			OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON ) {}
	virtual void			OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason ) {}
	virtual bool			HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return false; }
	virtual QAngle			PreferredCarryAngles( void ) { return vec3_angle; }
	virtual bool			ForcePhysgunOpen( CBasePlayer *pPlayer ) { return false; }
	virtual AngularImpulse	PhysGunLaunchAngularImpulse() { return RandomAngularImpulse( -600, 600 ); }
	virtual bool			ShouldPuntUseLaunchForces( PhysGunForce_t reason ) { return false; }
	virtual Vector			PhysGunLaunchVelocity( const Vector &vecForward, float flMass )
	{
		return Pickup_DefaultPhysGunLaunchVelocity( vecForward, flMass );
	}
};

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Entity that propagates general data needed by clients for every player.

class CPlayerResource : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PlayerResource", .base = false } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPlayerResource, CBaseEntity );
public:
	DECLARE_SERVERCLASS();

	void Spawn_Impl( void );
	int	 ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() | FCAP_DONT_SAVE; }
	virtual void ResourceThink( void );
	virtual void UpdatePlayerData( void );
	int  UpdateTransmitState_Impl(void);
	int GetPlayerSmoothPing( int iClientIndex );

protected:
	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	CNetworkArray( int, m_iPing, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iKills, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 16 } ]] );
	CNetworkArray( int, m_iAssists, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 16 } ]] );
	CNetworkArray( int, m_iDeaths, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 12 } ]] );
	CNetworkArray( int, m_bConnected, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iTeam, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 4 } ]] );
	CNetworkArray( int, m_iPendingTeam, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 4 } ]] );
	CNetworkArray( int, m_bAlive, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( int, m_iHealth, MAX_PLAYERS+1, [[= ks::reflect::Net{ .bits = 10 } ]] );

	CNetworkArray( int, m_iCoachingTeam, MAX_PLAYERS + 1, [[= ks::reflect::Net{ .bits = 4 } ]] );
		
	int	m_nUpdateCounter;
};

extern CPlayerResource *g_pPlayerResource;

class CBasePlayer;


class CPlayerVoiceListener : public CAutoGameSystem
{
public:
	CPlayerVoiceListener( void );

	// Auto-game cleanup
	virtual void LevelInitPreEntity( void );
	virtual void LevelShutdownPreEntity( void );

	bool IsPlayerSpeaking( int nPlayerIndex );
	bool IsPlayerSpeaking( CBasePlayer *pPlayer );
	bool ScriptIsPlayerSpeaking( int nPlayerIndex );

	void AddPlayerSpeakTime( int nPlayerIndex );
	void AddPlayerSpeakTime( CBasePlayer *pPlayer );

	float GetPlayerLastSpeechTime( int nPlayerIndex );
	float GetPlayerLastSpeechTime( CBasePlayer *pPlayer );

	float GetPlayerSilenceDuration( int nPlayerIndex );
	float GetPlayerSilenceDuration( CBasePlayer *pPlayer );

	float GetPlayerSpeechDuration( int nPlayerIndex );
	float GetPlayerSpeechDuration( CBasePlayer *pPlayer );
	float ScriptGetPlayerSpeechDuration( int nPlayerIndex );

private:
	void	InitData( void );

	float	m_flLastPlayerSpeechTime[MAX_PLAYERS];
	float	m_flPlayerSpeechDuration[MAX_PLAYERS];
};

extern CPlayerVoiceListener &PlayerVoiceListener( void );

// Only care about this stuff in game/client .dlls

class CPlayerState
{
	[[= ks::reflect::NetTable{ .name = "DT_PlayerState", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CPlayerState );
	DECLARE_EMBEDDED_NETWORKVAR();
	
	// This virtual method is necessary to generate a vtable in all cases
	// (DECLARE_PREDICTABLE will generate a vtable also)!
	virtual ~CPlayerState() {}

	// true if the player is dead
	CNetworkVar( bool, deadflag, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]
	                             [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );	
	// Viewing angle (player only)
	QAngle		v_angle;		
	
// The client .dll only cares about deadflag
//  the game and engine .dlls need to worry about the rest of this data
	// Player's network name
	string_t	netname;
	// 0:nothing, 1:force view angles, 2:add avelocity
	int			fixangle;
	// delta angle for fixangle == FIXANGLE_RELATIVE
	QAngle		anglechange;
	// flag to single the HLTV/Replay fake client, not transmitted
	bool		hltv;
	bool		replay;
	int			frags;
	int			assists;
	int			deaths;
	int			score;

// NOTE:  Only care about this stuff in game/client dlls
// Put at end in case it has any effect on size of structure
	DECLARE_SIMPLE_DATADESC();

};

class IGameSystem;

// Purpose: Pool of all per-level strings. Allocates memory for strings,
//			consolodating duplicates. The memory is freed on behalf of clients
//			at level transition. Strings are of type string_t.


//-----------------------------------------------------------------------------
// String allocation
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char *pszValue );
template <typename T1> [[gnu::always_inline]] inline auto AllocPooledStringConstant( T1 pszValue ) { return AllocPooledString( pszValue ); }
string_t FindPooledString( const char *pszValue );
void RemovePooledString( const char *pszValue );

//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
IGameSystem *GameStringSystem();

//-----------------------------------------------------------------------------
class CGameString
{
public:
	CGameString() : 
		m_iszString( NULL_STRING ), m_pszString( nullptr ), m_iSerial( 0 ) 
	{

	} 

	CGameString( const char *pszString, bool bCopy = false )  : 
		m_iszString( NULL_STRING ), m_pszString( nullptr ), m_iSerial( 0 ) 
	{ 
		Set( pszString, bCopy ); 
	}

	~CGameString();

	void Set( const char *pszString, bool bCopy = false )
	{
		if ( m_bCopy && m_pszString )
			std::free( (void *)m_pszString );
		m_iszString = NULL_STRING;
		m_pszString = ( !bCopy ) ? pszString : __builtin_strdup( pszString );
	}

	// This is only for temp variables!
	// If the global string pool changes this has no way to realloc the string!
	void SetFastNoCopy( string_t iszString )
	{
		m_iszString = iszString;
		m_iSerial = gm_iSerialNumber;
	}

	bool IsSerialNumberOutOfDate( void ) const
	{
		return m_iSerial != gm_iSerialNumber;
	}

	string_t Get() const
	{
		if ( m_iszString == NULL_STRING || IsSerialNumberOutOfDate() )
		{
			if ( !m_pszString )
				return NULL_STRING;

			m_iszString = AllocPooledString( m_pszString );
			m_iSerial = gm_iSerialNumber;
		}

		return m_iszString;
	}

	operator const char *() const
	{
		return ( m_pszString ) ? m_pszString : "";
	}

	operator string_t() const
	{
		return Get();
	}

	static void IncrementSerialNumber()
	{
		gm_iSerialNumber++;
	}

private:
	mutable string_t m_iszString;
	const char *m_pszString;
	mutable int m_iSerial;
	bool m_bCopy;

	static int gm_iSerialNumber;

};

extern ConVar mp_forcecamera;
extern ConVar mp_allowspectators;
extern ConVar mp_friendlyfire;
extern ConVar mp_teammates_are_enemies;

class CBaseCombatWeapon;
class CBaseEntity;



void CreateWeaponManager( const char *pWeaponName, int iMaxPieces );


void WeaponManager_AmmoMod( CBaseCombatWeapon *pWeapon );

void WeaponManager_AddManaged( CBaseEntity *pWeapon );
void WeaponManager_RemoveManaged( CBaseEntity *pWeapon );

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: A gib is a chunk of a body, or a piece of wood/metal/rocks/etc.

extern CBaseEntity *CreateRagGib( const char *szModel, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecForce, float flFadeTime = 0.0, bool bShouldIgnite = false );

inline constexpr auto GERMAN_GIB_COUNT = 4;
inline constexpr auto HUMAN_GIB_COUNT = 6;
inline constexpr auto ALIEN_GIB_COUNT = 4;

enum GibType_e
{
	GIB_HUMAN,
	GIB_ALIEN,
};

class CGib : public CBaseAnimating,
			 public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CGib, CBaseAnimating );

	virtual ~CGib( void );

	void Spawn( const char *szGibModel, float flLifetime = 25.0f );

	void InitGib( CBaseEntity *pVictim, float fMaxVelocity, float fMinVelocity );
	void BounceGibTouch ( CBaseEntity *pOther );
	void StickyGibTouch ( CBaseEntity *pOther );
	void WaitTillLand( void );
	void DieThink( void );
	void LimitVelocity( void );
	bool SUB_AllowedToFade_Impl( void );

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int	ObjectCaps_Impl( void ) { return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE | FCAP_IMPULSE_USE; }
	static	void SpawnHeadGib( CBaseEntity *pVictim );
	static	void SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, GibType_e eGibType );
	static  void SpawnStickyGibs( CBaseEntity *pVictim, Vector vecOrigin, int cGibs );
	static	void SpawnSpecificGibs( CBaseEntity *pVictim, int nNumGibs, float fMaxVelocity, float fMinVelocity, const char* cModelName, float flLifetime = 25);

	void SetPhysicsAttacker( CBasePlayer *pEntity, float flTime );
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	CBasePlayer *HasPhysicsAttacker_Impl( float dt );

	void SetSprite( CBaseEntity *pSprite )
	{
		m_hSprite = pSprite;	
	}

	CBaseEntity *GetSprite( void )
	{
		return m_hSprite.Get();
	}

	void SetFlame( CBaseEntity *pFlame )
	{
		m_hFlame = pFlame;	
	}

	CBaseEntity *GetFlame( void )
	{
		return m_hFlame.Get();
	}

public:
	void SetBloodColor( int nBloodColor );

	int		m_cBloodDecals;
	int		m_material;
	float	m_lifeTime;
	bool	m_bForceRemove;

	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

private:
	// A little piece of duplicated code
	void AdjustVelocityBasedOnHealth( int nHealth, Vector &vecVelocity );
	int		m_bloodColor;

	EHANDLE m_hSprite;
	EHANDLE m_hFlame;

	// Limit the max number of extant Gibs (mainly a memory issue):
	static const int MAX_CONCURRENT_GIBS = 75;
	static std::vector< EHANDLE > s_ExtantGibs;
};

class CRagGib : public CBaseAnimating
{
public:
	DECLARE_CLASS( CRagGib, CBaseAnimating );

	void Spawn( const char *szModel, const Vector &vecOrigin, const Vector &vecForce, float flFadeTime );
};

// Purpose: work in progress

class CGlobalEvent;

class CGlobalEventLog : public CAutoGameSystemPerFrame
{
public:
	typedef enum
	{
		GLOBAL_EVENT_NPCS = 0,

		GLOBAL_EVENT_MAX
	} EGlobalEvent;

public:
	CGlobalEventLog( );

	CGlobalEvent	*GetGlobalEvent( EGlobalEvent GlobalEvent );

	CGlobalEvent	*CreateEvent( const char *pszName, bool bIsHighLevel, CGlobalEvent *pParent = nullptr );
	CGlobalEvent	*CreateTempEvent( const char *pszName, CGlobalEvent *pParent = nullptr );
	void			RemoveEvent( CGlobalEvent *pEvent );

	void			AddKeyValue( CGlobalEvent *pEvent, bool bVarying, const char *pszKey, const char *pszValueFormat, ... );

	void			SendUpdate( );

protected:
	virtual void PostInit( );
	virtual void FrameUpdatePostEntityThink( );

private:
	unsigned int					m_nNextID;
	std::vector< CGlobalEvent * >	m_Events;
	std::vector< CGlobalEvent * >	m_TempEvents;
	std::vector< CGlobalEvent * >	m_DirtyEvents;
	CGlobalEvent					*m_pGlobalEvents[ GLOBAL_EVENT_MAX ];
};

extern CGlobalEventLog	GlobalEventLog;

extern Vector g_vecAttackDir;
extern int g_iSkillLevel;
extern bool g_fGameOver;
extern ConVar g_Language;

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Armor given by a battery
inline constexpr auto MAX_NORMAL_BATTERY = 100;

// Ammo counts given by ammo items
inline constexpr auto SIZE_AMMO_PISTOL = 20;
inline constexpr auto SIZE_AMMO_PISTOL_LARGE = 100;
inline constexpr auto SIZE_AMMO_SMG1 = 45;
inline constexpr auto SIZE_AMMO_SMG1_LARGE = 225;
inline constexpr auto SIZE_AMMO_AR2 = 20;
inline constexpr auto SIZE_AMMO_AR2_LARGE = 100;
inline constexpr auto SIZE_AMMO_RPG_ROUND = 1;
inline constexpr auto SIZE_AMMO_SMG1_GRENADE = 1;
inline constexpr auto SIZE_AMMO_BUCKSHOT = 20;
inline constexpr auto SIZE_AMMO_357 = 6;
inline constexpr auto SIZE_AMMO_357_LARGE = 20;
inline constexpr auto SIZE_AMMO_CROSSBOW = 6;
inline constexpr auto SIZE_AMMO_AR2_ALTFIRE = 1;
inline constexpr auto SIZE_AMMO_FLECHETTE = 60;
inline constexpr auto SIZE_AMMO_URANIUM = 30;

inline constexpr auto SF_ITEM_START_CONSTRAINED = 0x00000001;
inline constexpr auto SF_ITEM_MUST_EXIST = 0x00000002; // prevent the procedural population system from modifying this item

class CItem : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( CItem, CBaseAnimating );

	CItem();
	virtual ~CItem();

	void Spawn_Impl( void );
	void Precache_Impl();

	virtual bool HasBloatedCollision( void ) const { return true; } // Does this item increase its collision box to make it easier to pick up?

	CBaseEntity* Respawn_Impl( void );
	void ItemTouch( CBaseEntity *pOther );
	void ItemForceTouch( CBaseEntity *pOther );
	
	virtual void Materialize( void );
	virtual bool MyTouch( CBasePlayer *pPlayer ) { return false; };

	// Become touchable when we are at rest
	void OnEntityEvent_Impl( EntityEvent_t event, void *pEventData );

	// Activate when at rest, but don't allow pickup until then
	void ActivateWhenAtRest( float flTime = 0.5f );

	// IPlayerPickupVPhysics
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );

	int	ObjectCaps_Impl() { return BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE | FCAP_WCEDIT_POSITION; };
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector	GetOriginalSpawnOrigin( void ) { return m_vOriginalSpawnOrigin;	}
	QAngle	GetOriginalSpawnAngles( void ) { return m_vOriginalSpawnAngles;	}
	void	SetOriginalSpawnOrigin( const Vector& origin ) { m_vOriginalSpawnOrigin = origin; }
	void	SetOriginalSpawnAngles( const QAngle& angles ) { m_vOriginalSpawnAngles = angles; }
	bool	CreateItemVPhysicsObject( void );
	virtual bool	ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer );
	DECLARE_DATADESC();
private:
	void ComeToRest( void );
	void ItemTouchInternal( CBaseEntity *pOther, bool bForceTouch );

private:
	bool		m_bActivateWhenAtRest;
	[[= ks::reflect::Key{ .name = "OnPlayerTouch" } ]] COutputEvent m_OnPlayerTouch;
	[[= ks::reflect::Key{ .name = "OnCacheInteraction" } ]] COutputEvent m_OnCacheInteraction;
	
	Vector		m_vOriginalSpawnOrigin;
	QAngle		m_vOriginalSpawnAngles;

	IPhysicsConstraint		*m_pConstraint;
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLight : public CPointEntity
{
public:
	DECLARE_CLASS( CLight, CPointEntity );

	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );
	void	Spawn_Impl( void );
	void	FadeThink( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void	TurnOn( void );
	void	TurnOff( void );
	void	Toggle( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetPattern", .type = FIELD_STRING } ]] void	InputSetPattern( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeToPattern", .type = FIELD_STRING } ]] void	InputFadeToPattern( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "style" } ]] int		m_iStyle;
	[[= ks::reflect::Key{ .name = "defaultstyle" } ]] int		m_iDefaultStyle;
	[[= ks::reflect::Key{ .name = "pattern" } ]] string_t m_iszPattern;
	char	m_iCurrentFade;
	char	m_iTargetFade;
};

// Purpose: Defines a structure common to buttons and doors for playing locked
//			and unlocked sounds.

struct locksound_t					// sounds that doors and buttons make when locked/unlocked
{

	string_t	sLockedSound;		// sound a door makes when it's locked
	string_t	sLockedSentence;	// sentence group played when door is locked
	string_t	sUnlockedSound;		// sound a door makes when it's unlocked
	string_t	sUnlockedSentence;	// sentence group played when door is unlocked

	int		iLockedSentence;		// which sentence in sentence group to play next
	int		iUnlockedSentence;		// which sentence in sentence group to play next

	float	flwaitSound;			// time delay between playing consecutive 'locked/unlocked' sounds
	float	flwaitSentence;			// time delay between playing consecutive sentences
	byte	bEOFLocked;				// true if hit end of list of locked sentences
	byte	bEOFUnlocked;			// true if hit end of list of unlocked sentences
};

struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the events to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicEventListener : public CLogicalEntity, public CGameEventListener
{
	DECLARE_CLASS( CLogicEventListener, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass

	void Spawn_Impl( void );
	void FireGameEvent( IGameEvent *event );
	
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "EventName" } ]] string_t	m_iszEventName;
	[[= ks::reflect::Key{ .name = "IsEnabled" } ]] bool		m_bIsEnabled;
	[[= ks::reflect::Key{ .name = "TeamNum" } ]] int			m_nTeam;
	[[= ks::reflect::Key{ .name = "FetchEventData" } ]] bool		m_bFetchEventData;

	[[= ks::reflect::Key{ .name = "OnEventFired" } ]] COutputEvent m_OnEventFired;

};

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the events to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicEventListenerItemEquip : public CLogicEventListener
{
	DECLARE_CLASS( CLogicEventListenerItemEquip, CLogicEventListener );
	DECLARE_DATADESC();

public:
	CLogicEventListenerItemEquip() { SetGameEventHandler( this ); }

	void Spawn_Impl( void );
	void FireGameEvent( IGameEvent *event );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "IsEnabled" } ]] bool		m_bIsEnabled;
	[[= ks::reflect::Key{ .name = "TeamNum" } ]] int			m_nTeam;
	[[= ks::reflect::Key{ .name = "WeaponClassname" } ]] string_t	m_szWeaponClassname;
	[[= ks::reflect::Key{ .name = "WeaponType" } ]] int			m_nWeaponType;

	[[= ks::reflect::Key{ .name = "OnEventFired" } ]] COutputEvent m_OnEventFired;
};

class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the player to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicPlayerProxy : public CLogicalEntity
{
	DECLARE_CLASS( CLogicPlayerProxy, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass

	COutputEvent m_PlayerHasAmmo;
	COutputEvent m_PlayerHasNoAmmo;
	[[= ks::reflect::Key{ .name = "PlayerDied" } ]] COutputEvent m_PlayerDied;

	[[= ks::reflect::Key{ .name = "OnDuck" } ]] COutputEvent m_OnDuck;
	[[= ks::reflect::Key{ .name = "OnUnDuck" } ]] COutputEvent m_OnUnDuck;
	[[= ks::reflect::Key{ .name = "OnJump" } ]] COutputEvent m_OnJump;

	[[= ks::reflect::Key{ .name = "PlayerHealth" } ]] COutputInt m_RequestedPlayerHealth;

	void InputRequestPlayerHealth( inputdata_t &inputdata );
	void InputSetPlayerHealth( inputdata_t &inputdata );
	void InputRequestAmmoState( inputdata_t &inputdata );
	void InputEnableCappedPhysicsDamage( inputdata_t &inputdata );
	void InputDisableCappedPhysicsDamage( inputdata_t &inputdata );

	void Activate_Impl( void );

	bool PassesDamageFilter_Impl( const CTakeDamageInfo &info );

	EHANDLE m_hPlayer;
};

struct inputdata_t;

using namespace ::ks::reflect;

inline constexpr auto NUM_RANDOM_OUTPUTS = 8;

class CLogicRandomOutputs : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicRandomOutputs, CLogicalEntity );

	CLogicRandomOutputs();

	void Activate_Impl();
	void Think_Impl();
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableRefire", .type = FIELD_VOID } ]] void InputEnableRefire( inputdata_t &inputdata );  // Private input handler, not in FGD
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Trigger", .type = FIELD_VOID } ]] void InputTrigger( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CancelPending", .type = FIELD_VOID } ]] void InputCancelPending( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrigger1", .index = 0 } ]] [[= ks::reflect::Key{ .name = "OnTrigger2", .index = 1 } ]] [[= ks::reflect::Key{ .name = "OnTrigger3", .index = 2 } ]] [[= ks::reflect::Key{ .name = "OnTrigger4", .index = 3 } ]] [[= ks::reflect::Key{ .name = "OnTrigger5", .index = 4 } ]] [[= ks::reflect::Key{ .name = "OnTrigger6", .index = 5 } ]] [[= ks::reflect::Key{ .name = "OnTrigger7", .index = 6 } ]] [[= ks::reflect::Key{ .name = "OnTrigger8", .index = 7 } ]] COutputEvent m_Output[ NUM_RANDOM_OUTPUTS ];
	[[= ks::reflect::Key{ .name = "OnSpawn" } ]] COutputEvent m_OnSpawn;

	float m_flOnTriggerChance[ NUM_RANDOM_OUTPUTS ];
	
private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	bool m_bWaitForRefire;			// Set to disallow a refire while we are waiting for our outputs to finish firing.
};

struct inputdata_t;

using namespace ::ks::reflect;

class CLogicRelay : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicRelay, CLogicalEntity );

	CLogicRelay();

	void Activate_Impl();
	void Think_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableRefire", .type = FIELD_VOID } ]] void InputEnableRefire( inputdata_t &inputdata );  // Private input handler, not in FGD
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Trigger", .type = FIELD_VOID } ]] void InputTrigger( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CancelPending", .type = FIELD_VOID } ]] void InputCancelPending( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrigger" } ]] COutputEvent m_OnTrigger;
	[[= ks::reflect::Key{ .name = "OnSpawn" } ]] COutputEvent m_OnSpawn;
	
private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	bool m_bWaitForRefire;			// Set to disallow a refire while we are waiting for our outputs to finish firing.
};

class CBaseEntity;
class CPointTemplate;



// This class provides hooks into the map-entity loading process that allows CS to do some tricks
// when restarting the round. The main trick it tries to do is recreate all 
class IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname ) = 0;
	virtual CBaseEntity* CreateNextEntity( const char *pClassname ) = 0;
};

// Use the filter so you can prevent certain entities from being created out of the map.
// CSPort does this when restarting rounds. It wants to reload most entities from the map, but certain
// entities like the world entity need to be left intact.
void MapEntity_ParseAllEntities( const char *pMapData, IMapEntityFilter *pFilter=nullptr, bool bActivateEntities=false );

const char *MapEntity_ParseEntity( CBaseEntity *&pEntity, const char *pEntData, IMapEntityFilter *pFilter );
void MapEntity_PrecacheEntity( const char *pEntData, int &nStringSize );

//-----------------------------------------------------------------------------
// Hierarchical spawn 
//-----------------------------------------------------------------------------
struct HierarchicalSpawn_t
{
	CBaseEntity *m_pEntity;
	int			m_nDepth;
	CBaseEntity	*m_pDeferredParent;			// attachment parents can't be set until the parents are spawned
	const char	*m_pDeferredParentAttachment; // so defer setting them up until the second pass
};

struct HierarchicalSpawnMapData_t
{
	const char	*m_pMapData;
	int			m_iMapDataLength;
};

// Shared by mapentities.cpp and Foundry for spawning entities.
class CMapEntitySpawner
{
public:
	CMapEntitySpawner();
	~CMapEntitySpawner();
	
	void AddEntity( CBaseEntity *pEntity, const char *pMapData, int iMapDataLength );
	void HandleTemplates();
	void SpawnAndActivate( bool bActivateEntities );
	void PurgeRemovedEntities();

private:

	HierarchicalSpawnMapData_t *m_pSpawnMapData;
	HierarchicalSpawn_t *m_pSpawnList;
	std::vector< CPointTemplate* > m_PointTemplates;
	int m_nEntities;
};

void SpawnHierarchicalList( int nEntities, HierarchicalSpawn_t *pSpawnList, bool bActivateEntities );
void MapEntity_ParseAllEntites_SpawnTemplates( CPointTemplate **pTemplates, int iTemplateCount, CBaseEntity **pSpawnedEntities, HierarchicalSpawnMapData_t *pSpawnMapData, int iSpawnedEntityCount );

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;


class CRuleEntity : public CBaseEntity
{
	public:
	DECLARE_CLASS( CRuleEntity, CBaseEntity );

	void	Spawn_Impl( void );

	DECLARE_DATADESC();

	void	SetMaster( string_t iszMaster ) { m_iszMaster = iszMaster; }

	protected:
	bool	CanFireForActivator( CBaseEntity *pActivator );

	private:
	[[= ks::reflect::Key{ .name = "master" } ]] string_t	m_iszMaster;
};

// 
// CRulePointEntity -- base class for all rule "point" entities (not brushes)
//
class CRulePointEntity : public CRuleEntity
{
	public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CRulePointEntity, CRuleEntity );

	int		m_Score;
	void		Spawn_Impl( void );
};

class CGameCoopMissionManager : public CRulePointEntity
{
	public:
	DECLARE_CLASS( CGameCoopMissionManager, CRulePointEntity );
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_DATADESC();

	void	Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	int		GetWaveNumber( void );
	void	SetWaveCompleted( void );
	void	SetRoundReset( void );
	void	SetSpawnsReset( void ); // used for spawns
	void	SetRoundLostKilled( void );
	void	SetRoundLostTime( void );
	void	SetMissionCompleted( void );

	//void InputGetWaveNumber( inputdata_t &inputdata );

	private:
	[[= ks::reflect::Key{ .name = "OnWaveCompleted" } ]] COutputEvent	m_OnWaveCompleted;
	[[= ks::reflect::Key{ .name = "OnRoundReset" } ]] COutputEvent	m_OnRoundReset;
	[[= ks::reflect::Key{ .name = "OnSpawnsReset" } ]] COutputEvent	m_OnSpawnsReset;
	[[= ks::reflect::Key{ .name = "OnRoundLostKilled" } ]] COutputEvent	m_OnRoundLostKilled;
	[[= ks::reflect::Key{ .name = "OnRoundLostTime" } ]] COutputEvent	m_OnRoundLostTime;
	[[= ks::reflect::Key{ .name = "OnMissionCompleted" } ]] COutputEvent	m_OnMissionCompleted;
};

inline constexpr auto SF_PLAYEREQUIP_USEONLY = 0x0001;
inline constexpr auto SF_PLAYEREQUIP_STRIPFIRST = 0x0002;
inline constexpr auto SF_PLAYEREQUIP_ONLYSTRIPSAME = 0x0004;
inline constexpr auto MAX_EQUIP = 32;

class CGamePlayerEquip : public CRulePointEntity
{
	DECLARE_DATADESC();

	public:
	DECLARE_CLASS( CGamePlayerEquip, CRulePointEntity );

	//inputs
	[[= ks::reflect::Input{ .name = "TriggerForAllPlayers", .type = FIELD_VOID } ]] void		InputTriggerForAllPlayers( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TriggerForActivatedPlayer", .type = FIELD_STRING } ]] void		InputTriggerForActivatedPlayer( inputdata_t &inputdata );

	using BaseClass::KeyValue_Impl;
	bool		KeyValue_Impl( const char *szKeyName, const char *szValue );
	void		TriggerForAllPlayers( void );
	void		TriggerForActivatedPlayer( CBasePlayer *pPlayer, const char *szWeapon );
	void		Touch_Impl( CBaseEntity *pOther );
	void		Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	inline bool	UseOnly( void ) { return ( m_spawnflags & SF_PLAYEREQUIP_USEONLY ) ? true : false; }
	inline bool	StripFirst( void ) { return ( m_spawnflags & SF_PLAYEREQUIP_STRIPFIRST ) ? true : false; }
	inline bool	OnlyStripSameWeaponType( void ) { return ( m_spawnflags & SF_PLAYEREQUIP_ONLYSTRIPSAME ) ? true : false; }

	private:

	void		EquipPlayer( CBaseEntity *pPlayer, const char *szWeapon = nullptr );

	string_t	m_weaponNames[MAX_EQUIP];
	int			m_weaponCount[MAX_EQUIP];
};

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

// Phys prop spawnflags
inline constexpr auto SF_PHYSPROP_START_ASLEEP = 0x000001;
inline constexpr auto SF_PHYSPROP_DONT_TAKE_PHYSICS_DAMAGE = 0x000002; // this prop can't be damaged by physics collisions
inline constexpr auto SF_PHYSPROP_DEBRIS = 0x000004;
inline constexpr auto SF_PHYSPROP_MOTIONDISABLED = 0x000008; // motion disabled at startup (flag only valid in spawn - motion can be enabled via input)
inline constexpr auto SF_PHYSPROP_TOUCH = 0x000010; // can be 'crashed through' by running player (plate glass)
inline constexpr auto SF_PHYSPROP_PRESSURE = 0x000020; // can be broken by a player standing on it
inline constexpr auto SF_PHYSPROP_ENABLE_ON_PHYSCANNON = 0x000040; // enable motion only if the player grabs it with the physcannon
inline constexpr auto SF_PHYSPROP_NO_ROTORWASH_PUSH = 0x000080; // The rotorwash doesn't push these
inline constexpr auto SF_PHYSPROP_ENABLE_PICKUP_OUTPUT = 0x000100; // If set, allow the player to +USE this for the purposes of generating an output
inline constexpr auto SF_PHYSPROP_PREVENT_PICKUP = 0x000200; // If set, prevent +USE/Physcannon pickup of this prop
inline constexpr auto SF_PHYSPROP_PREVENT_PLAYER_TOUCH_ENABLE = 0x000400; // If set, the player will not cause the object to enable its motion when bumped into
inline constexpr auto SF_PHYSPROP_HAS_ATTACHED_RAGDOLLS = 0x000800; // Need to remove attached ragdolls on enable motion/etc
inline constexpr auto SF_PHYSPROP_FORCE_TOUCH_TRIGGERS = 0x001000; // Override normal debris behavior and respond to triggers anyway
inline constexpr auto SF_PHYSPROP_FORCE_SERVER_SIDE = 0x002000; // Force multiplayer physics object to be serverside
inline constexpr auto SF_PHYSPROP_RADIUS_PICKUP = 0x004000; // For Xbox, makes small objects easier to pick up by allowing them to be found
inline constexpr auto SF_PHYSPROP_DISABLE_MOTION_ON_FREEZE = 0x010000; // Phys prop disables motion the first time it comes to rest
inline constexpr auto SF_PHYSPROP_ALWAYS_PICK_UP = 0x100000; // Physcannon can always pick this up, no matter what mass or constraints may apply.
inline constexpr auto SF_PHYSPROP_NO_COLLISIONS = 0x200000; // Don't enable collisions on spawn
inline constexpr auto SF_PHYSPROP_IS_GIB = 0x400000; // Limit # of active gibs

// Any barrel farther away than this is ignited rather than exploded.

// ParsePropData returns
enum
{
	PARSE_SUCCEEDED,					// Parsed propdata. Prop must be a prop_physics.
	PARSE_SUCCEEDED_ALLOWED_STATIC,		// Parsed propdata. Prop allowed to be prop_physics or prop_dynamic/prop_static.
	PARSE_FAILED_NO_DATA,				// Parse failed, no propdata in model. Prop must be prop_dynamic/prop_static.
	PARSE_FAILED_BAD_DATA,				// Parse failed, found propdata but it had bad data.
};

// Propdata defined interactions
enum propdata_interactions_t
{
	PROPINTER_PHYSGUN_WORLD_STICK,		// "onworldimpact"	"stick"
	PROPINTER_PHYSGUN_FIRST_BREAK,		// "onfirstimpact"	"break"
	PROPINTER_PHYSGUN_FIRST_PAINT,		// "onfirstimpact"	"paintsplat"
	PROPINTER_PHYSGUN_FIRST_IMPALE,		// "onfirstimpact"	"impale"
	PROPINTER_PHYSGUN_LAUNCH_SPIN_NONE,	// "onlaunch"		"spin_none"
	PROPINTER_PHYSGUN_LAUNCH_SPIN_Z,	// "onlaunch"		"spin_zaxis"
	PROPINTER_PHYSGUN_BREAK_EXPLODE,	// "onbreak"		"explode_fire"
	PROPINTER_PHYSGUN_BREAK_EXPLODE_ICE,	// "onbreak"	"explode_ice"
	PROPINTER_PHYSGUN_DAMAGE_NONE,		// "damage"			"none"

	PROPINTER_FIRE_FLAMMABLE,			// "flammable"			"yes"
	PROPINTER_FIRE_EXPLOSIVE_RESIST,	// "explosive_resist"	"yes"
	PROPINTER_FIRE_IGNITE_HALFHEALTH,	// "ignite"				"halfhealth"

	PROPINTER_PHYSGUN_CREATE_FLARE,		// "onpickup"		"create_flare"

	PROPINTER_PHYSGUN_ALLOW_OVERHEAD,	// "allow_overhead"	"yes"

	PROPINTER_WORLD_BLOODSPLAT,			// "onworldimpact", "bloodsplat"
	
	PROPINTER_PHYSGUN_NOTIFY_CHILDREN,	// "onfirstimpact" cause attached flechettes to explode
	PROPINTER_MELEE_IMMUNE,				// "melee_immune"	"yes"

	// If we get more than 32 of these, we'll need a different system

	PROPINTER_NUM_INTERACTIONS,
};

// Entities using COLLISION_GROUP_SPECIAL_PHYSICS should support this interface.
class IMultiplayerPhysics
{
public:
	virtual int		GetMultiplayerPhysicsMode() = 0;
	virtual float	GetMass() = 0;
	virtual bool	IsAsleep() = 0;
};

inline constexpr auto PHYSICS_MULTIPLAYER_AUTODETECT = 0; // use multiplayer physics mode as defined in model prop data
inline constexpr auto PHYSICS_MULTIPLAYER_SOLID = 1; // soild, pushes player away
inline constexpr auto PHYSICS_MULTIPLAYER_NON_SOLID = 2; // nonsolid, but pushed by player
inline constexpr auto PHYSICS_MULTIPLAYER_CLIENTSIDE = 3; // Clientside only, nonsolid

enum mp_break_t
{
	MULTIPLAYER_BREAK_DEFAULT,
	MULTIPLAYER_BREAK_SERVERSIDE,
	MULTIPLAYER_BREAK_CLIENTSIDE,
	MULTIPLAYER_BREAK_BOTH
};

enum PerformanceMode_t
{
	PM_NORMAL,
	PM_NO_GIBS,
	PM_FULL_GIBS,
	PM_REDUCED_GIBS,
};

//=============================================================================================================
// PROP DATA
//=============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: Derive your entity from this if you want your entity to parse propdata
//-----------------------------------------------------------------------------
class IBreakableWithPropData
{
public:
	// Damage modifiers
	virtual void		SetDmgModBullet( float flDmgMod ) = 0;
	virtual void		SetDmgModClub( float flDmgMod ) = 0;
	virtual void		SetDmgModExplosive( float flDmgMod ) = 0;
	virtual float		GetDmgModBullet( void ) = 0;
	virtual float		GetDmgModClub( void ) = 0;
	virtual float		GetDmgModExplosive( void ) = 0;
	virtual float		GetDmgModFire( void ) = 0;

	// Explosive
	virtual void		SetExplosiveRadius( float flRadius ) = 0;
	virtual void		SetExplosiveDamage( float flDamage ) = 0;
	virtual float		GetExplosiveRadius( void ) = 0;
	virtual float		GetExplosiveDamage( void ) = 0;

	// Physics damage tables
	virtual void		SetPhysicsDamageTable( string_t iszTableName ) = 0;
	virtual string_t	GetPhysicsDamageTable( void ) = 0;

	// Breakable chunks
	virtual void		SetBreakableModel( string_t iszModel ) = 0;
	virtual string_t 	GetBreakableModel( void ) = 0;
	virtual void		SetBreakableSkin( int iSkin ) = 0;
	virtual int			GetBreakableSkin( void ) = 0;
	virtual void		SetBreakableCount( int iCount ) = 0;
	virtual int			GetBreakableCount( void ) = 0;
	virtual void		SetMaxBreakableSize( int iSize ) = 0;
	virtual int			GetMaxBreakableSize( void ) = 0;

	// LOS blocking
	virtual void		SetPropDataBlocksLOS( bool bBlocksLOS ) = 0;
	virtual void		SetPropDataIsAIWalkable( bool bBlocksLOS ) = 0;

	// Interactions
	virtual void		SetInteraction( propdata_interactions_t Interaction ) = 0;
	virtual bool		HasInteraction( propdata_interactions_t Interaction ) = 0;

	// Multiplayer physics mode
	virtual void		SetPhysicsMode(int iMode) = 0;
	virtual int			GetPhysicsMode() = 0;

	// Multiplayer breakable spawn behavior
	virtual void		SetMultiplayerBreakMode( mp_break_t mode ) = 0;
	virtual mp_break_t	GetMultiplayerBreakMode( void ) const = 0;

	// Used for debugging
	virtual void		SetBasePropData( string_t iszBase ) = 0;
	virtual string_t	GetBasePropData( void ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Gamesystem that parses the prop data file
//-----------------------------------------------------------------------------
class CPropData : public CAutoGameSystem
{
public:
	CPropData( void );

	// Inherited from IAutoServerSystem
	virtual void LevelInitPreEntity( void );
	virtual void LevelShutdownPostEntity( void );

	// Read in the data from the prop data file
	void ParsePropDataFile( void );

	// Parse a keyvalues section into the prop
	int ParsePropFromKV( CBaseEntity *pProp, IBreakableWithPropData *pBreakableInterface, KeyValues *pSection, KeyValues *pInteractionSection );

	// Fill out a prop's with base data parsed from the propdata file
	int ParsePropFromBase( CBaseEntity *pProp, IBreakableWithPropData *pBreakableInterface, const char *pszPropData );

	// Get a random chunk in the specified breakable section
	const char *GetRandomChunkModel( const char *pszBreakableSection, int iMaxSize = -1 );

protected:
	KeyValues	*m_pKVPropData;
	bool		m_bPropDataLoaded;

	struct propdata_breakablechunk_t
	{
		string_t				iszChunkType;
		std::vector< string_t >	iszChunkModels;
	};
	std::vector< propdata_breakablechunk_t >	m_BreakableChunks;
};

extern CPropData g_PropDataSystem;

struct breakmodel_t
{
	Vector		offset;
	char		modelName[512];
	char		placementName[512];
	float		fadeTime;
	float		fadeMinDist;
	float		fadeMaxDist;
	float		health;
	float		burstScale;
	int			collisionGroup;
	bool		isRagdoll;
	bool		placementIsBone;
	bool		isMotionDisabled;
	mp_break_t	mpBreakMode;
};

struct breakablepropparams_t
{
	breakablepropparams_t( const Vector &_origin, const QAngle &_angles, const Vector &_velocity, const AngularImpulse &_angularVelocity );

	const Vector &origin;
	const QAngle &angles;
	const Vector &velocity;
	const AngularImpulse &angularVelocity;
	float impactEnergyScale;
	float defBurstScale;
	int defCollisionGroup;
};

const char *GetMassEquivalent(float flMass);
int  GetAutoMultiplayerPhysicsMode( Vector size, float mass );
void BreakModelList( std::vector< breakmodel_t > &list, int modelindex, float defBurstScale, int defCollisionGroup );
void PropBreakableCreateAll( int modelindex, IPhysicsObject *pPhysics, const breakablepropparams_t &params, CBaseEntity *pEntity, int iPrecomputedBreakableCount, bool bIgnoreGibLImit, bool defaultLocation = true );
void PropBreakableCreateAll( int modelindex, IPhysicsObject *pPhysics, const Vector &origin, const QAngle &angles, const Vector &velocity, const AngularImpulse &angularVelocity, float impactEnergyScale, float burstScale, int collisionGroup, CBaseEntity *pEntity = nullptr, bool defaultLocation = true );

// Player gibs.
void PrecacheGibsForModel( int iModel );
void BuildGibList( std::vector< breakmodel_t > &list, int modelindex, float defBurstScale, int defCollisionGroup );
CBaseEntity *CreateGibsFromList( std::vector< breakmodel_t > &list, int modelindex, IPhysicsObject *pPhysics, const breakablepropparams_t &params, CBaseEntity *pEntity, int iPrecomputedBreakableCount, bool bIgnoreGibLImit, bool defaultLocation = true, std::vector< EHANDLE > *pGibList = nullptr, bool bBurning = false );

class CBaseEntity;
struct physicspushlist_t;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: Keeps track of original positions of any entities that are being possibly pushed
//  and handles restoring positions for those objects if the push is aborted
//-----------------------------------------------------------------------------
class CPhysicsPushedEntities
{
public:

	DECLARE_CLASS_NOBASE( CPhysicsPushedEntities );

	CPhysicsPushedEntities( void );

	// Purpose: Tries to rotate an entity hierarchy, returns the blocker if any
	CBaseEntity *PerformRotatePush( CBaseEntity *pRoot, float movetime );

	// Purpose: Tries to linearly push an entity hierarchy, returns the blocker if any
	CBaseEntity *PerformLinearPush( CBaseEntity *pRoot, float movetime );

	int			CountMovedEntities() { return std::ssize( m_rgMoved ); }
	void		StoreMovedEntities( physicspushlist_t &list );
	void		BeginPush( CBaseEntity *pRootEntity );
	// updates physics for all pushers that moved this tick
	void		UpdatePusherPhysicsEndOfTick();
	void		QueueChildUpdate( CBaseEntity *pChild ) { m_rgUpdatedChildren.push_back( pChild ); }

protected:

	// describes the per-frame incremental motion of a rotating MOVETYPE_PUSH
	struct RotatingPushMove_t
	{
		Vector		origin;
		matrix3x4_t	startLocalToWorld;
		matrix3x4_t	endLocalToWorld;
		QAngle		amove;		// delta orientation
	};

	// Pushers + their original positions also (for touching triggers)
	struct PhysicsPusherInfo_t
	{
		CBaseEntity			*m_pEntity;
		Vector				m_vecStartAbsOrigin;
	};

	// Pushed entities + various state related to them being pushed
	struct PhysicsPushedInfo_t
	{
		CBaseEntity			*m_pEntity;
		Vector				m_vecStartAbsOrigin;
		trace_t				m_Trace;
		bool				m_bBlocked;
		bool				m_bPusherIsGround;
	};

	// Adds the specified entity to the list
	void	AddEntity( CBaseEntity *ent );

	// If a move fails, restores all entities to their original positions
	void	RestoreEntities( );

	// Compute the direction to move the rotation blocker
	void	ComputeRotationalPushDirection( CBaseEntity *pBlocker, const RotatingPushMove_t &rotPushMove, Vector *pMove, CBaseEntity *pRoot );

	// Speculatively checks to see if all entities in this list can be pushed
	bool SpeculativelyCheckPush( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush, CBaseEntity *pRoot, bool bIgnoreTeammates = false );

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot = nullptr );

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool	SpeculativelyCheckLinearPush( const Vector &vecAbsPush );

	// Registers a blockage
	CBaseEntity *RegisterBlockage();

	// Some fixup for objects pushed by rotating objects
	virtual void	FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove );

	// Commits the speculative movement
	void	FinishPush( bool bIsRotPush = false, const RotatingPushMove_t *pRotPushMove = nullptr );

	// Generates a list of all entities potentially blocking all pushers
	void	GenerateBlockingEntityList();
	void	GenerateBlockingEntityListAddBox( const Vector &vecMoved );

	// Purpose: Gets a list of all entities hierarchically attached to the root 
	void	SetupAllInHierarchy( CBaseEntity *pParent );

	// Unlink + relink the pusher list so we can actually do the push
	void	UnlinkPusherList( int *pPusherHandles );
	void	RelinkPusherList( int *pPusherHandles );

	// Causes all entities in the list to touch triggers from their prev position
	void	FinishPushers();

	// Purpose: Rotates the root entity, fills in the pushmove structure
	void	RotateRootEntity( CBaseEntity *pRoot, float movetime, RotatingPushMove_t &rotation );

	// Purpose: Linearly moves the root entity
	void	LinearlyMoveRootEntity( CBaseEntity *pRoot, float movetime, Vector *pAbsPushVector );

	bool	IsPushedPositionValid( CBaseEntity *pBlocker, bool bIgnoreTeammates );
	void	TraceBlockerEntity( CBaseEntity *pBlocker, const Vector& absStart, const Vector& absEnd, bool bIgnoreTeammates, trace_t *pOutTrace );

	bool	FindValidLocationUpwards( float *pOutLengthUp, CBaseEntity *pBlocker, float maxDist, float slop );
	bool	FindValidLocationAlongVector( Vector *pOutDelta, CBaseEntity *pBlocker, const Vector &vEndPoint, float slop );

protected:

	std::vector< PhysicsPusherInfo_t >	m_rgPusher;
	std::vector< PhysicsPushedInfo_t >	m_rgMoved;
	int								m_nBlocker;
	bool							m_bIsUnblockableByPlayer;
	Vector							m_rootPusherStartLocalOrigin;
	QAngle							m_rootPusherStartLocalAngles;
	float							m_rootPusherStartLocaltime;
	float							m_flMoveTime;

	std::vector< PhysicsPusherInfo_t >	m_rgUpdatedPushers;
	std::vector< CBaseEntity * >	m_rgUpdatedChildren;
	friend class CPushBlockerEnum;
};

class CTraceFilterPushMove : public CTraceFilterSimple
{
	DECLARE_CLASS( CTraceFilterPushMove, CTraceFilterSimple );

public:
	CTraceFilterPushMove( CBaseEntity *pEntity, int nCollisionGroup ) 
		: CTraceFilterSimple( pEntity, nCollisionGroup )
	{
		m_pRootParent = pEntity->GetRootMoveParent();
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		contract_assert( dynamic_cast<CBaseEntity*>(pHandleEntity) );
		CBaseEntity *pTestEntity = static_cast<CBaseEntity*>(pHandleEntity);

		if ( UTIL_EntityHasMatchingRootParent( m_pRootParent, pTestEntity ) )
			return false;

		if ( pTestEntity->GetMoveType() == MOVETYPE_VPHYSICS && 
			pTestEntity->VPhysicsGetObject() && pTestEntity->VPhysicsGetObject()->IsMoveable() )
			return false;

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:

	CBaseEntity *m_pRootParent;
};

extern CPhysicsPushedEntities *g_pPushedEntities;

class CBaseEntity;

using namespace ::ks::reflect;

typedef CHandle<CBaseEntity> EHANDLE;

// this system provides several piece of functionality to ai or other systems which wish to do
// traces and other trace-like queries. 

// a. By maintaining a set of incrementally updated trace results, it makes it simple to have ai
// code use hyteresis on traces as an optimization method.

// b. By updating the cache entries outside of the entity think functions, the update is done in a
// fully multi-threaded fashion

enum EQueryType_t
{
	EQUERY_INVALID = 0,									// an invalid or unused entry
	EQUERY_TRACELINE,
	EQUERY_ENTITY_LOS_CHECK,

};

enum EEntityOffsetMode_t
{
	EOFFSET_MODE_WORLDSPACE_CENTER,
	EOFFSET_MODE_EYEPOSITION,
	EOFFSET_MODE_NONE,										// nop
};

inline constexpr auto QCACHE_MAXPNTS = 3; // maximum number of points/entities
															// involved in a query

struct QueryCacheKey_t
{
	EQueryType_t m_Type;
	int m_nNumValidPoints;
	Vector m_Points[QCACHE_MAXPNTS];
	EHANDLE m_pEntities[QCACHE_MAXPNTS];
	EEntityOffsetMode_t m_nOffsetMode[QCACHE_MAXPNTS];
	unsigned int m_nTraceMask;
	unsigned int m_nHashIdx;
	int m_nCollisionGroup;
	ShouldHitFunc_t m_pTraceFilterFunction;

	float m_flMinimumUpdateInterval;

	void ComputeHashIndex( void );

	bool Matches( QueryCacheKey_t const *pNode ) const ;
};

struct QueryCacheEntry_t
{
	QueryCacheEntry_t *m_pNext;
	QueryCacheEntry_t *m_pPrev;
	QueryCacheKey_t m_QueryParams;
	float m_flLastUpdateTime;
	bool m_bUsedSinceUpdated;								// was this cell referenced?
	bool m_bSpeculativelyDone;
	bool m_bResult;											// for queries with a boolean result

	void IssueQuery( void );

};

bool IsLineOfSightBetweenTwoEntitiesClear( CBaseEntity *pSrcEntity,
										   EEntityOffsetMode_t nSrcOffsetMode,
										   CBaseEntity *pDestEntity,
										   EEntityOffsetMode_t nDestOffsetMode,
										   CBaseEntity *pSkipEntity,
										   int nCollisionGroup,
										   unsigned int nTraceMask,
										   ShouldHitFunc_t pTraceFilterCallback,
										   float flMinimumUpdateInterval = 0.2
	);

// call during main loop for threaded update of the query cache
void UpdateQueryCache( void );

// call on level transition or other significant step-functions
void InvalidateQueryCache( void );

class CBaseAnimating;
class CBaseEntity;


// UNDONE: Remove and make dynamic?
inline constexpr auto RAGDOLL_MAX_ELEMENTS = 24;
inline constexpr auto RAGDOLL_INDEX_BITS = 5; // NOTE 1<<RAGDOLL_INDEX_BITS >= RAGDOLL_MAX_ELEMENTS

inline constexpr auto CORE_DISSOLVE_FADE_START = 0.2f;
inline constexpr auto CORE_DISSOLVE_MODEL_FADE_START = 0.1f;
inline constexpr auto CORE_DISSOLVE_MODEL_FADE_LENGTH = 0.05f;
inline constexpr auto CORE_DISSOLVE_FADEIN_LENGTH = 0.1f;

struct ragdollelement_t
{
	Vector				originParentSpace;
	IPhysicsObject		*pObject;		// all valid elements have an object
	IPhysicsConstraint	*pConstraint;	// all valid elements have a constraint (except the root)
	int					parentIndex;
};

struct ragdoll_t
{
	int						listCount;
	bool					allowStretch;
	bool					unused;
	IPhysicsConstraintGroup *pGroup;
	// store these in separate arrays for save/load
	ragdollelement_t 	list[RAGDOLL_MAX_ELEMENTS];
	int					boneIndex[RAGDOLL_MAX_ELEMENTS];
	ragdollanimatedfriction_t animfriction;
};

struct ragdollparams_t
{
	void		*pGameData;
	vcollide_t	*pCollide;
	CStudioHdr	*pStudioHdr;
	int			modelIndex;
	Vector		forcePosition;
	Vector		forceVector;
	int			forceBoneIndex;
	const matrix3x4_t *pCurrentBones;
	float		jointFrictionScale;
	bool		allowStretch;
	bool		fixedConstraints;
};

class CRagdollEntry
{
public:
	CRagdollEntry( CBaseAnimating *pRagdoll, float flForcedRetireTime ) : m_hRagdoll( pRagdoll ), m_flForcedRetireTime( flForcedRetireTime )
	{
	}
	CBaseAnimating* Get() { return m_hRagdoll.Get(); }
	float GetForcedRetireTime() { return m_flForcedRetireTime; }

private:
	CHandle<CBaseAnimating> m_hRagdoll;
	float m_flForcedRetireTime;
};

//-----------------------------------------------------------------------------
// This hooks the main game systems callbacks to allow the AI system to manage memory
//-----------------------------------------------------------------------------
class CRagdollLRURetirement : public CAutoGameSystemPerFrame
{
public:
	CRagdollLRURetirement( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	// Methods of IGameSystem
	virtual void Update( float frametime );
	virtual void FrameUpdatePostEntityThink( void );

	// Move it to the top of the LRU
	void MoveToTopOfLRU( CBaseAnimating *pRagdoll, bool bImportant = false, float flForcedRetireTime = 0.0f );
	void SetMaxRagdollCount( int iMaxCount ){ m_iMaxRagdolls = iMaxCount; }

	virtual void LevelInitPreEntity( void );
	int CountRagdolls( bool bOnlySimulatingRagdolls ) { return bOnlySimulatingRagdolls ? m_iSimulatedRagdollCount : m_iRagdollCount; }

private:
	typedef CHandle<CBaseAnimating> CRagdollHandle;
	std::vector< CRagdollEntry > m_LRU; 
	std::vector< CRagdollEntry > m_LRUImportantRagdolls; 

	int m_iMaxRagdolls;
	int m_iSimulatedRagdollCount;
	int m_iRagdollCount;
};

extern CRagdollLRURetirement s_RagdollLRU;

// Manages ragdolls fading for the low violence versions
class CRagdollLowViolenceManager
{
public:
	CRagdollLowViolenceManager(){ m_bLowViolence = false; }
	// Turn the low violence ragdoll stuff off if we're in the HL2 Citadel maps because
	// the player has the super gravity gun and fading ragdolls will break things.
	void SetLowViolence( const char *pMapName );
	bool IsLowViolence( void ){ return m_bLowViolence; }

private:
	bool m_bLowViolence;
};

extern CRagdollLowViolenceManager g_RagdollLVManager;

bool RagdollCreate( ragdoll_t &ragdoll, const ragdollparams_t &params, IPhysicsEnvironment *pPhysEnv );

void RagdollActivate( ragdoll_t &ragdoll, vcollide_t *pCollide, int modelIndex, bool bForceWake = true );
void RagdollSetupCollisions( ragdoll_t &ragdoll, vcollide_t *pCollide, int modelIndex );
void RagdollDestroy( ragdoll_t &ragdoll );

// Gets the bone matrix for a ragdoll object
// NOTE: This is different than the object's position because it is
// forced to be rigidly attached in parent space
bool RagdollGetBoneMatrix( const ragdoll_t &ragdoll, CBoneAccessor &pBoneToWorld, int objectIndex );

// Parse the ragdoll and obtain the mapping from each physics element index to a bone index
// returns num phys elements
int RagdollExtractBoneIndices( int *boneIndexOut, CStudioHdr *pStudioHdr, vcollide_t *pCollide );

// computes an exact bbox of the ragdoll's physics objects
void RagdollComputeExactBbox( const ragdoll_t &ragdoll, const Vector &origin, Vector &outMins, Vector &outMaxs );
void RagdollComputeApproximateBbox( const ragdoll_t &ragdoll, const Vector &origin, Vector &outMins, Vector &outMaxs );
bool RagdollIsAsleep( const ragdoll_t &ragdoll );
void RagdollSetupAnimatedFriction( IPhysicsEnvironment *pPhysEnv, ragdoll_t *ragdoll, int iModelIndex );

void RagdollApplyAnimationAsVelocity( ragdoll_t &ragdoll, const matrix3x4_t *pBoneToWorld );
void RagdollApplyAnimationAsVelocity( ragdoll_t &ragdoll, const matrix3x4_t *pPrevBones, const matrix3x4_t *pCurrentBones, float dt );

void RagdollSolveSeparation( ragdoll_t &ragdoll, CBaseEntity *pEntity );

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Set this spawnflag before calling Spawn to get electrical effects
//-----------------------------------------------------------------------------
inline constexpr auto SF_RAGDOLL_BOOGIE_ELECTRICAL = 0x10000;
inline constexpr auto SF_RAGDOLL_BOOGIE_ELECTRICAL_NARROW_BEAM = 0x20000;

//-----------------------------------------------------------------------------
// Makes ragdolls DANCE!
//-----------------------------------------------------------------------------
class CRagdollBoogie : public CBaseEntity 
{
	DECLARE_CLASS( CRagdollBoogie, CBaseEntity );

public:
	static CRagdollBoogie	*Create( CBaseEntity *pTarget, float flMagnitude, float flStartTime, float flLengthTime = 0.0f, int nSpawnFlags = 0 );
	static void IncrementSuppressionCount( CBaseEntity *pTarget );
	static void DecrementSuppressionCount( CBaseEntity *pTarget );

	void Precache_Impl();
	void Spawn_Impl();

private:
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetBoogieTime( float flStartTime, float flLengthTime );
	void	SetMagnitude( float flMagnitude );
	void	BoogieThink( void );
	void	ZapThink();
	
	float m_flStartTime;
	float m_flBoogieLength;
	float m_flMagnitude;
	int	m_nSuppressionCount;
};

bool VScriptServerReplaceClosures( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing = false );

class CBaseEntityScriptInstanceHelper : public IScriptInstanceHelper
{
	bool ToString( void *p, char *pBuf, int bufSize );
	void *BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId );
};

extern CBaseEntityScriptInstanceHelper g_BaseEntityScriptInstanceHelper;

// Only allow scripts to create entities during map initialization
bool IsEntityCreationAllowedInScripts( void );

// ----------------------------------------------------------------------------
// KeyValues access
// ----------------------------------------------------------------------------
class CScriptKeyValues
{
public:
	CScriptKeyValues( KeyValues *pKeyValues = nullptr );
	~CScriptKeyValues( );

	HSCRIPT ScriptFindKey( const char *pszName );
	HSCRIPT ScriptGetFirstSubKey( void );
	HSCRIPT ScriptGetNextKey( void );
	int ScriptGetKeyValueInt( const char *pszName );
	float ScriptGetKeyValueFloat( const char *pszName );
	const char *ScriptGetKeyValueString( const char *pszName );
	bool ScriptIsKeyValueEmpty( const char *pszName );
	bool ScriptGetKeyValueBool( const char *pszName );
	void ScriptReleaseKeyValues( );

	KeyValues *m_pKeyValues;	// actual KeyValue entity
};

extern LoggingChannelID_t LOG_VScript;;

extern IScriptVM * g_pScriptVM;

HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing = false );
bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing = false );
inline bool VScriptRunScript( const char *pszScriptName, bool bWarnMissing = false ) { return VScriptRunScript( pszScriptName, nullptr, bWarnMissing ); }

#define BEGIN_ENT_SCRIPTDESC_NAMED( className, baseClass, scriptName, description )	_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_NAMED( className, baseClass, scriptName, description )
#define BEGIN_ENT_SCRIPTDESC_ROOT_NAMED( className, scriptName, description )		_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_ROOT_NAMED( className, scriptName, description )

// Only allow scripts to create entities during map initialization
bool IsEntityCreationAllowedInScripts( void );

#undef BEGIN_ENT_SCRIPTDESC_NAMED
#undef BEGIN_ENT_SCRIPTDESC_ROOT_NAMED

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;


// Purpose: Entity that simulates bullets that are underwater.

inline constexpr auto WATER_BULLET_BUBBLES_PER_INCH = 0.05f;

//=========================================================
//=========================================================
class CWaterBullet : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_WaterBullet" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CWaterBullet, CBaseAnimating );

public:
	void Precache_Impl();
	void Spawn( const Vector &vecOrigin, const Vector &vecDir );
	void Touch_Impl( CBaseEntity *pOther );
	void BulletThink();

	DECLARE_SERVERCLASS();
};

class CBaseEntity;

// Purpose: Namespace for functions having to do with WC Edit mode



//=============================================================================
//	>> NWCEdit
//=============================================================================
namespace NWCEdit
{
	void	RememberEntityPosition( CBaseEntity *pEntity );
	void	UpdateEntityPosition( CBaseEntity *pEntity );
};

class CEconItemView;
class CEconPersonaDataPublic;

class CBaseEntity;
class CEconItemDefinition;
class CEconItemSchema;


// Purpose: Weapon data file parsing, shared by game & client dlls.

//=============================================================================
// Econ item system stubs (econ system removed)
//=============================================================================

// CEconPersonaDataPublic stub
class CEconPersonaDataPublic
{
public:
	struct Commendation_t {
		int cmd_leader() const { return 0; }
		int cmd_teaching() const { return 0; }
		int cmd_friendly() const { return 0; }
	};
	struct Data_t {
		int player_level() const { return 0; }
		Commendation_t commendation() const { return Commendation_t(); }
	};
	virtual ~CEconPersonaDataPublic() {}
	const Data_t& Obj() const { static Data_t s; return s; }
};

// Item quality enums stub
enum
{
	AE_NORMAL = 0,
	AE_UNIQUE = 1,
};

// Operator enums stub
enum
{
	k_EOperator_String_EQ = 0,
};

// Item generation stub
class CItemSelectionCriteria
{
public:
	void SetQuality( int ) {}
	void BAddCondition( const char*, int, const char*, bool ) {}
};

class CItemGeneration
{
public:
	CBaseEntity* GenerateItemFromScriptData( void*, const Vector&, const QAngle&, const char* ) { return nullptr; }
	CBaseEntity* GenerateRandomItem( void*, const Vector&, const QAngle& ) { return nullptr; }
};
inline CItemGeneration* ItemGeneration() { static CItemGeneration s; return &s; }

// Combined item ID stub
inline uint64 CombinedItemIdMakeFromDefIndexAndPaint( uint16, int ) { return 0; }

// Unacknowledged item reasons stub
enum unacknowledged_item_inventory_positions_t
{
	UNACK_ITEM_DROPPED = 1,
	UNACK_ITEM_CRAFTED,
	UNACK_ITEM_TRADED,
	UNACK_ITEM_PURCHASED,
	UNACK_ITEM_FOUND_IN_CRATE,
	UNACK_ITEM_GIFTED,
	UNACK_ITEM_SUPPORT,
	UNACK_ITEM_PROMOTION,
	UNACK_ITEM_EARNED,
	UNACK_ITEM_REFUNDED,
};

// Sticker attribute IDs stub
enum EStickerAttributeType
{
	k_EStickerAttribute_ID = 0,
	k_EStickerAttribute_Wear,
	k_EStickerAttribute_Scale,
	k_EStickerAttribute_Rotation,
};

// Locchar type
typedef wchar_t locchar_t;

// Get unacknowledged reason stub
inline int GetUnacknowledgedReason( uint32 ) { return 0; }

// Schema attribute handle stub
class CSchemaAttributeDefHandle
{
public:
	CSchemaAttributeDefHandle( const char* ) {}
	operator bool() const { return false; }
	bool operator!() const { return true; }
};

// Attribute definition stub
class CEconItemAttributeDefinition
{
public:
	const char *GetDefinitionName() const { return ""; }
	uint16 GetDefinitionIndex() const { return 0; }
};

// Kill eater attribute pair stubs
inline const CEconItemAttributeDefinition* GetKillEaterAttrPair_Score( int ) { return nullptr; }
inline const CEconItemAttributeDefinition* GetKillEaterAttrPair_Type( int ) { return nullptr; }

// Music definition stub
class CEconMusicDefinition
{
public:
	const char *GetName() const { return ""; }
};

// Forward declaration

// Quest definition stub
class CEconQuestDefinition
{
public:
	uint32 GetQuestID() const { return 0; }
	const char* GetQuestExpression() const { return ""; }
	const char* GetQuestConVars() const { return ""; }
	const char* GetGameMode() const { return ""; }
	const char* GetMapGroup() const { return ""; }
	const char* GetMap() const { return ""; }
	int GetTargetTeam() const { return 0; }
	KeyValues* GetStringTokens() const { return nullptr; }
	const char* GetShortNameLocToken() const { return ""; }
	const std::vector< int >& GetQuestPoints() const { static std::vector< int > empty; return empty; }
};

// Taunt definition stub
class CEconTauntDefinition
{
public:
	const char* GetName() const { return ""; }
	const char* GetSequenceName() const { return ""; }
};

// Paint kit stub
class CPaintKit
{
public:
	const char* GetName() const { return ""; }
	int nID;
};

// Sticker kit stub
class CStickerKit
{
public:
	std::string sMaterialPath;
	std::string sMaterialPathNoDrips;
	int nRarity;
};

// Graffiti tint definition stub
class CEconGraffitiTintDefinition
{
public:
	uint32 GetHexColorRGB() const { return 0xFFFFFF; }
};

// Combined tint ID helper stub
inline uint8 CombinedTintIDGetHSVID( uint32 ) { return 0; }

// Viewmodel hand activity translation stub
inline int TranslateViewmodelHandActivity( int activity ) { return activity; }

// Pro player data stub
class CProPlayerData
{
public:
	const char* GetName() const { return ""; }
};

// GetItemSchema() is defined in cs_shareddefs.cpp
CEconItemSchema *GetItemSchema();
// GEconItemSchema returns the same as GetItemSchema
inline CEconItemSchema& GEconItemSchema() { return *GetItemSchema(); }

// CEconItemView - Wrapper for item data with pointer to real definition
class CEconItemView
{
public:
	CEconItemView() : m_pItemDef( nullptr ), m_nItemDefIndex( 0 ) {}

	// Initialize from a definition name (looks up in schema)
	void Init( const char *pszDefName )
	{
		m_pItemDef = GetItemSchema()->GetItemDefinitionByName( pszDefName );
		if ( m_pItemDef )
			m_nItemDefIndex = m_pItemDef->GetDefinitionIndex();
	}

	// Initialize from a definition index
	void Init( uint16 nDefIndex )
	{
		m_nItemDefIndex = nDefIndex;
		m_pItemDef = GetItemSchema()->GetItemDefinition( nDefIndex );
	}

	// Initialize from an existing definition pointer
	void Init( CEconItemDefinition *pDef )
	{
		m_pItemDef = pDef;
		if ( pDef )
			m_nItemDefIndex = pDef->GetDefinitionIndex();
	}

	uint16 GetItemIndex() const { return m_nItemDefIndex; }
	uint64 GetItemID() const { return m_nItemDefIndex; }
	// Always returns non-NULL (fallback to static default for backwards compatibility with stubs)
	CEconItemDefinition *GetItemDefinition() const;
	CEconItemDefinition *GetStaticData() const;
	const char *GetCustomDesc() const { return nullptr; }
	bool IsValid() const { return m_pItemDef != nullptr; }
	uint32 GetTauntID() const { return 0; }
	const CPaintKit *GetCustomPaintKit() const { return nullptr; }
	int GetCustomPaintKitIndex() const { return 0; }
	int GetRarity() const { return 0; }
	const wchar_t *GetItemName() const { return L""; }
	uint32 GetStickerAttributeBySlotIndexInt( int, int, uint32 defVal ) const { return defVal; }
	const char *GetMagazineModel() const { return nullptr; }
	const char *GetScopeLensMaskModel() const { return nullptr; }
	const char *GetUidModel() const { return nullptr; }
	const char *GetStatTrakModelByType( int ) const { return nullptr; }
	const char *GetCustomName() const { return nullptr; }
	void GetKillEaterTypes( std::flat_multiset<uint32>& ) const {}
	int GetKillEaterValueByType( int ) const { return -1; }
	uint32 GetAccountID() const { return 0; }
	int GetNumSupportedStickerSlots() const { return 0; }
	bool ItemHasAnyStickersApplied() const { return false; }
	IMaterial *GetStickerIMaterialBySlotIndex( int, bool ) const { return nullptr; }
	const char *GetStickerWorldModelBoneParentNameBySlotIndex( int ) const { return nullptr; }
	Vector GetStickerSlotWorldProjectionStartBySlotIndex( int ) const { return vec3_origin; }
	Vector GetStickerSlotWorldProjectionEndBySlotIndex( int ) const { return vec3_origin; }
	uint64 GetFauxItemIDFromDefinitionIndex() const { return m_nItemDefIndex; }
	template<typename T>
	bool FindAttribute( const CSchemaAttributeDefHandle&, T* ) const { return false; }
	template<typename T>
	bool FindAttribute( const CEconItemAttributeDefinition*, T* ) const { return false; }
	// Single-argument FindAttribute (for existence check)
	bool FindAttribute( const CSchemaAttributeDefHandle& ) const { return false; }
	bool FindAttribute( const CEconItemAttributeDefinition* ) const { return false; }
	// Update networked attributes stub
	void UpdateNetworkedDynamicAttributesForDemos( uint16, float ) {}

private:
	CEconItemDefinition *m_pItemDef;
	uint16 m_nItemDefIndex;
};

// Stub for FindAttribute_UnsafeBitwiseCast
template<typename T, typename U, typename V>
inline bool FindAttribute_UnsafeBitwiseCast( U*, const CSchemaAttributeDefHandle&, V* ) { return false; }
template<typename T, typename U, typename V>
inline bool FindAttribute_UnsafeBitwiseCast( U*, const CEconItemAttributeDefinition*, V* ) { return false; }

typedef unsigned short WEAPON_FILE_INFO_HANDLE;

// -----------------------------------------------------------
// Weapon sound types
// Used to play sounds defined in the weapon's classname.txt file
// This needs to match pWeaponSoundCategories in weapon_parse.cpp
// ------------------------------------------------------------
enum WeaponSound_t {
	EMPTY,
	SINGLE,
	SINGLE_ACCURATE,
	SINGLE_NPC,
	WPN_DOUBLE, // Can't be "DOUBLE" because windows.h uses it.
	DOUBLE_NPC,
	BURST,
	RELOAD,
	RELOAD_NPC,
	MELEE_MISS,
	MELEE_HIT,
	MELEE_HIT_WORLD,
	SPECIAL1,
	SPECIAL2,
	SPECIAL3,
	TAUNT,
	NEARLYEMPTY,
	FAST_RELOAD,

	// Add new shoot sound types here

	NUM_SHOOT_SOUND_TYPES,
}; 

int GetWeaponSoundFromString( const char *pszString );

inline constexpr auto MAX_SHOOT_SOUNDS = 16; // Maximum number of shoot sounds per shoot type

inline constexpr auto MAX_WEAPON_STRING = 80;
inline constexpr auto MAX_WEAPON_PREFIX = 16;
inline constexpr auto MAX_WEAPON_AMMO_NAME = 32;

inline constexpr const char *WEAPON_PRINTNAME_MISSING = "!!! Missing printname on weapon";

class CHudTexture;

struct WeaponInfoLookup
{
	std::size_t m_nWeaponParseDataOffset;
	_fieldtypes m_fieldType;
	CGameString m_iszAttribClassName;

	WeaponInfoLookup( void ) {}
	WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char* szAttribClassName );
	WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup );
};

struct CWeaponInfoLookupListLess
{
	bool operator()( const WeaponInfoLookup *src1, const WeaponInfoLookup *src2 ) const
	{
		return src1->m_iszAttribClassName.Get() < src2->m_iszAttribClassName.Get();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Contains the data read from the weapon's script file. 
// It's cached so we only read each weapon's script file once.
// Each game provides a CreateWeaponInfo function so it can have game-specific
// data (like CS move speeds) in the weapon script.
//-----------------------------------------------------------------------------
class FileWeaponInfo_t
{
public:

	FileWeaponInfo_t();
	virtual ~FileWeaponInfo_t() {}
	
	// Each game can override this to get whatever values it wants from the script.
	virtual void Parse( KeyValues *pKeyValuesData, const char *szWeaponName );

	virtual void RefreshDynamicParameters() {};
	
public:	
	bool					bParsedScript;
	bool					bLoadedHudElements;

// SHARED
	char					szClassName[MAX_WEAPON_STRING];
	char					szPrintName[MAX_WEAPON_STRING];			// Name for showing in HUD, etc.

	int GetIndexofAttribute( string_t iszAttribClassName ) const;
	static std::vector< WeaponInfoLookup* > ms_vecWeaponInfoLookup;	// sorted by CWeaponInfoLookupListLess

protected:
	char					szViewModel[MAX_WEAPON_STRING];			// View model of this weapon
	char					szWorldModel[MAX_WEAPON_STRING];		// Model of this weapon seen carried by the player
	char					szAmmo1[MAX_WEAPON_AMMO_NAME];			// "primary" ammo type
	char					szWorldDroppedModel[MAX_WEAPON_STRING];

	static bool				ms_bWeaponInfoLookupInitialized;

public:
	char					szAnimationPrefix[MAX_WEAPON_PREFIX];	// Prefix of the animations that should be used by the player carrying this weapon
	int						iSlot;									// inventory slot.
	int						iPosition;								// position in the inventory slot.
	int						iMaxClip1;								// max primary clip size (-1 if no clip)
	int						iMaxClip2;								// max secondary clip size (-1 if no clip)
	int						iDefaultClip1;							// amount of primary ammo in the gun when it's created
	int						iDefaultClip2;							// amount of secondary ammo in the gun when it's created
	int						iWeight;								// this value used to determine this weapon's importance in autoselection.
	int						iRumbleEffect;							// Which rumble effect to use when fired? (xbox)
	bool					bAutoSwitchTo;							// whether this weapon should be considered for autoswitching to
	bool					bAutoSwitchFrom;						// whether this weapon can be autoswitched away from when picking up another weapon or ammo
	int						iFlags;									// miscellaneous weapon flags

	char					szAmmo2[MAX_WEAPON_AMMO_NAME];			// "secondary" ammo type
	char					szAIAddOn[MAX_WEAPON_STRING];			// addon that this weapon can become

	// Sound blocks
	char					aShootSounds[NUM_SHOOT_SOUND_TYPES][MAX_WEAPON_STRING];	

private:
	int						iAmmoType;
	int						iAmmo2Type;	

public:

	bool					m_bMeleeWeapon;		// Melee weapons can always "fire" regardless of ammo.

	// This tells if the weapon was built right-handed (defaults to true).
	// This helps cl_righthand make the decision about whether to flip the model or not.
	bool					m_bBuiltRightHanded;
	bool					m_bAllowFlipping;	// False to disallow flipping the model, regardless of whether
												// it is built left or right handed.

	virtual int		GetPrimaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetSecondaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetDefaultPrimaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetDefaultSecondaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }
	virtual int		GetPrimaryReserveAmmoMax( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }
	virtual int		GetSecondaryReserveAmmoMax( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }

	const char* GetWorldModel( const CEconItemView* pWepView = nullptr, int iTeam = 0 ) const;
	const char* GetViewModel( const CEconItemView* pWepView = nullptr, int iTeam = 0 ) const;
	const char* GetWorldDroppedModel( const CEconItemView* pWepView = nullptr, int iTeam = 0 ) const;
	const char* GetPrimaryAmmo( const CEconItemView* pWepView = nullptr ) const;
	
	int GetPrimaryAmmoType( const CEconItemView* pWepView = nullptr ) const;

// CLIENT DLL
	// Sprite data, read from the data file
	int						iSpriteCount;
	CHudTexture				*iconActive;
	CHudTexture	 			*iconInactive;
	CHudTexture 			*iconAmmo;
	CHudTexture 			*iconAmmo2;
	CHudTexture 			*iconCrosshair;
	CHudTexture 			*iconAutoaim;
	CHudTexture 			*iconZoomedCrosshair;
	CHudTexture 			*iconZoomedAutoaim;
	CHudTexture				*iconSmall;

// TF2 specific
	bool					bShowUsageHint;							// if true, then when you receive the weapon, show a hint about it

// SERVER DLL

};

WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name );
FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle );
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void );

void PrecacheFileWeaponInfoDatabase();

// 
// Read a possibly-encrypted KeyValues file in. 
// If pICEKey is NULL, then it appends .txt to the filename and loads it as an unencrypted file.
// If pICEKey is non-NULL, then it appends .ctx to the filename and loads it as an encrypted file.
//
// (This should be moved into a more appropriate place).
//
KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile = false );

// Each game implements this. It can return a derived class and override Parse() if it wants.
extern FileWeaponInfo_t* CreateWeaponInfo();

extern void LoadEquipmentData();

class CWeaponDatabase : public CAutoGameSystem, public CGameEventListener
{
public:
	CWeaponDatabase();

	void Reset();
	bool LoadManifest();
	void PrecacheAllWeapons();
	void RefreshAllWeapons();

	WEAPON_FILE_INFO_HANDLE FindWeaponInfo( const char *name );
	FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle );

protected:
	friend void LoadEquipmentData();

	virtual bool Init();

	WEAPON_FILE_INFO_HANDLE FindOrCreateWeaponInfo( const char *name );
	bool LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey );

public:
	void FireGameEvent( IGameEvent *event );

private:
	// Handles are dense indices into m_WeaponInfos, held by weapons; the dict maps names to them.
	std::vector< std::unique_ptr< FileWeaponInfo_t > > m_WeaponInfos;
	ks::idict< WEAPON_FILE_INFO_HANDLE > m_WeaponInfoDatabase;
	bool m_bPreCached;
};

extern CWeaponDatabase g_WeaponDatabase;

struct WeaponProficiencyInfo_t
{
	float	spreadscale;
	float	bias;
};

enum WeaponProficiency_t
{
	WEAPON_PROFICIENCY_POOR = 0,
	WEAPON_PROFICIENCY_AVERAGE,
	WEAPON_PROFICIENCY_GOOD,
	WEAPON_PROFICIENCY_VERY_GOOD,
	WEAPON_PROFICIENCY_PERFECT,
};

const char *GetWeaponProficiencyName( WeaponProficiency_t proficiency );

class CBaseEntity;
struct gamevcollisionevent_t;

using namespace ::ks::reflect;


// Purpose: The worldspawn entity. This spawns first when each level begins.

enum
{
	TIME_MIDNIGHT	= 0,
	TIME_DAWN,
	TIME_MORNING,
	TIME_AFTERNOON,
	TIME_DUSK,
	TIME_EVENING,
};

class CWorld : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_WORLD" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWorld, CBaseEntity );

	CWorld();
	~CWorld();

	DECLARE_SERVERCLASS();

	int RequiredEdictIndex_Impl( void ) { return 0; }   // the world always needs to be in slot 0
	
	static void RegisterSharedActivities( void );
	static void RegisterSharedEvents( void );
	void Spawn_Impl( void );
	void Precache_Impl( void );
	void UpdateOnRemove_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void DecalTrace_Impl( trace_t *pTrace, char const *decalName );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent ) {}
	void VPhysicsFriction_Impl( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit ) {}

	inline void GetWorldBounds( Vector &vecMins, Vector &vecMaxs )
	{
		VectorCopy( m_WorldMins, vecMins );
		VectorCopy( m_WorldMaxs, vecMaxs );
	}

	inline float GetWaveHeight() const
	{
		return (float)m_flWaveHeight;
	}

	bool GetDisplayTitle() const;
	bool GetStartDark() const;

	void SetDisplayTitle( bool display );
	void SetStartDark( bool startdark );

	int GetTimeOfDay() const;
	void SetTimeOfDay( int iTimeOfDay );

	bool IsColdWorld( void );

	int GetTimeOfDay()	{ return m_iTimeOfDay; }

private:
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "chaptertitle" } ]] string_t m_iszChapterTitle;

	CNetworkVar( float, m_flWaveHeight, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 8.0f, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVector( m_WorldMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_WorldMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_flMaxOccludeeArea, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxoccludeearea" } ]] );
	CNetworkVar( float, m_flMinOccluderArea, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minoccluderarea" } ]] );
	CNetworkVar( float, m_flMinPropScreenSpaceWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minpropscreenwidth" } ]] );
	CNetworkVar( float, m_flMaxPropScreenSpaceWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxpropscreenwidth" } ]] );
	CNetworkVar( string_t, m_iszDetailSpriteMaterial, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "detailmaterial" } ]] );

	// start flags
	CNetworkVar( bool, m_bStartDark, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "startdark" } ]] );
	CNetworkVar( bool, m_bColdWorld, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "coldworld" } ]] );
	CNetworkVar( int, m_iTimeOfDay );
	[[= ks::reflect::Key{ .name = "gametitle" } ]] bool m_bDisplayTitle;

};

CWorld* GetWorldEntity();
extern const char *GetDefaultLightstyleString( int styleIndex );
