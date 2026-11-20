// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "bone_accessor.h"
#include "bone_setup.h"
#include "client_class.h"
#include "icliententity.h"
#include "materialsystem/custommaterialowner.h"
#include "mouthinfo.h"
#include "networkvar.h"
#include "posedebugger.h"
#include "reflect_annotations.h"
#include "shaderapi/ishaderapi.h"
#include "soundinfo.h"
#include "tools/bonelist.h"
#include "utils/common/scriplib.h"
#include "vphysics_interface.h"
#include "vscript/ivscript.h"
#include "predictable_entity.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "declare_class.h"
#include "basehandle.h"
#include "choreoactor.h"
#include "choreoscene.h"
#include "ichoreoeventcallback.h"
#include "soundflags.h"
#include "iscenetokenprocessor.h"
#include "studio.h"
#include "ScreenSpaceEffects.h"
#include "sentence.h"
#include "game/client/iclientrendertargets.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "edict.h"
#include "view_shared.h"
#include "dt_send.h"
module ks.client:baseanimatedtextureproxy;

import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import std;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.interpolatedvar;
import ks.tier1.rangecheckedvar;
import ks.tier1.reflect_wirevar;
import ks.tier1.tracked_ref;
import ks.game.ai_activity;
import :baseentity;
import :particlemgr;
import ks.game.shareddefs;
import ks.core.dbg;
import ks.core.types;
import ks.tier1.createinterface;
import ks.core.color;
import :util;
import ks.tier1.bitbuf;
import ks.tier1.intrusive_list;
import ks.tier1.intrusive_ptr;
import ks.tier1.map_ops;
import :cstrike15.cs_shareddefs;

class CBaseAnimatedTextureProxy : public IMaterialProxy
{
public:
	CBaseAnimatedTextureProxy();
	virtual ~CBaseAnimatedTextureProxy();

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

protected:
	// derived classes must implement this; it returns the time
	// that the animation began
	virtual float  GetAnimationStartTime( void* pBaseEntity ) = 0;

	// Derived classes may implement this if they choose;
	// this method is called whenever the animation wraps...
	virtual void   AnimationWrapped( void* pBaseEntity ) {}

protected:
	void Cleanup();
	
	IMaterialVar *m_AnimatedTextureVar;
	IMaterialVar *m_AnimatedTextureFrameNumVar;
	float m_FrameRate;
	bool m_WrapAnimation;
};

class CClientRagdoll;

class CBoneMergeCache;
class CJiggleBones;
class CNewParticleEffect;
class CRagdoll;
class CAnimationLayer;
class CBaseAnimatingOverlay;
class CBaseEntity;
class CRopeKeyframe;

using namespace ::ks::reflect;

// Shared activities

inline constexpr const char *LIPSYNC_POSEPARAM_NAME = "mouth";
inline constexpr auto NUM_HITBOX_FIRES = 10;

/*
class CBaseClientShader
{
	virtual void RenderMaterial( CBaseEntity *pEntity, int count, const vec4_t *verts, const vec4_t *normals, const vec2_t *texcoords, vec4_t *lightvalues );
};
*/

class CIKState;

extern ConVar vcollide_wireframe;
extern IDataCache *datacache;
extern void DevMsgRT( char const* pMsg, ... );
extern IPoseDebugger *g_pPoseDebugger;

struct ClientModelRenderInfo_t : public ModelRenderInfo_t
{
	// Added space for lighting origin override. Just allocated space, need to set base pointer
	matrix3x4_t lightingOffset;

	// Added space for model to world matrix. Just allocated space, need to set base pointer
	matrix3x4_t modelToWorld;
};

struct RagdollInfo_t
{
	bool		m_bActive;
	float		m_flSaveTime;
	int			m_nNumBones;
	BoneVector		m_rgBonePos[MAXSTUDIOBONES];
	BoneQuaternion	m_rgBoneQuaternion[MAXSTUDIOBONES];
};

enum {
		ANIMLODFLAG_DISTANT					= 0x01,
		ANIMLODFLAG_OUTSIDEVIEWFRUSTUM		= 0x02,
		ANIMLODFLAG_INVISIBLELOCALPLAYER	= 0x04,
		ANIMLODFLAG_DORMANT					= 0x08,
		//ANIMLODFLAG_UNUSED			= 0x10,
		//ANIMLODFLAG_UNUSED			= 0x20,
	};

class CAttachmentData
{
public:
	matrix3x4_t	m_AttachmentToWorld;
	QAngle	m_angRotation;
	Vector	m_vOriginVelocity;
	int		m_nLastFramecount : 31;
	bool	m_bAnglesComputed : 1;
};

typedef unsigned int			ClientSideAnimationListHandle_t;

inline constexpr auto INVALID_CLIENTSIDEANIMATION_LIST_HANDLE = (ClientSideAnimationListHandle_t)~0;

namespace DT_ServerAnimationData { extern RecvTable g_RecvTable; }
void RecvProxy_Sequence( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CBaseAnimating : public CBaseEntity, public CCustomMaterialOwner
{
	[[= ks::reflect::NetTable{ .name = "DT_ServerAnimationData", .base = false } ]]
	[[= ks::reflect::From<"m_flCycle", ks::reflect::Net{ .table = "DT_ServerAnimationData" }>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_BaseAnimating" } ]]
	[[= ks::reflect::From<"m_bClientSideRagdoll", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::SubTable<"serveranimdata", &DT_ServerAnimationData::g_RecvTable, nullptr, false>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseAnimating, CBaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_ENT_SCRIPTDESC();

	enum
	{
		NUM_POSEPAREMETERS = 24,
		NUM_BONECTRLS = 4
	};

	// Inherited from IClientUnknown
public:
	virtual IClientModelRenderable*	GetClientModelRenderable();

	// Inherited from IClientModelRenderable
public:
	virtual bool GetRenderData( void *pData, ModelDataCategory_t nCategory );

public:
	CBaseAnimating();
	~CBaseAnimating();

#ifdef DEBUG
	std::vector< float > m_flBoneSetupPerfHistory;
#endif

	virtual CBaseAnimating*		GetBaseAnimating() { return this; }

	bool UsesPowerOfTwoFrameBufferTexture( void );

	int GetRenderFlags( void );

	virtual bool	Interpolate( float currentTime );
	virtual bool	Simulate();	
	virtual void	Release();	

	float	GetAnimTimeInterval( void ) const;

	// Get bone controller values.
	virtual void	GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS]);
	virtual float	SetBoneController ( int iController, float flValue );

	LocalFlexController_t GetNumFlexControllers( void );
	const char *GetFlexDescFacs( int iFlexDesc );
	const char *GetFlexControllerName( LocalFlexController_t iFlexController );
	const char *GetFlexControllerType( LocalFlexController_t iFlexController );

	virtual void	GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

	// Computes a box that surrounds all hitboxes
	bool ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );
	bool ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Gets the hitbox-to-world transforms, returns false if there was a problem
	bool HitboxToWorldTransforms( matrix3x4_t *pHitboxToWorld[MAXSTUDIOBONES] );

	// base model functionality
	float		  ClampCycle( float cycle, bool isLooping );
	virtual void GetPoseParameters( CStudioHdr *pStudioHdr, float poseParameter[MAXSTUDIOPOSEPARAM] );
	void CalcBoneMerge( int boneMask );
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	void BuildJiggleTransformations( int boneIndex, const mstudiojigglebone_t *jiggleParams, const matrix3x4_t &goalMX, bool coordSystemIsFlipped );
	virtual void ApplyBoneMatrixTransform( matrix3x4_t& transform );
 	virtual int	VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );

	// model specific
	virtual bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	virtual void UpdateIKLocks( float currentTime );
	virtual void CalculateIKLocks( float currentTime );
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual int	InternalDrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	virtual bool OnPostInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	void		DoInternalDrawModel( class IMatRenderContext *pRenderContext, ClientModelRenderInfo_t *pInfo, DrawModelState_t *pState, matrix3x4_t *pBoneToWorldArray = nullptr );

	//
	virtual CMouthInfo *GetMouth();
	virtual void	ControlMouth( CStudioHdr *pStudioHdr );
	
	virtual void DoExtraBoneProcessing( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternion q[], matrix3x4a_t boneToWorld[], CBoneBitList &boneComputed, CIKContext *pIKContext ) { contract_assert( false ); }

	// override in sub-classes
	virtual void DoAnimationEvents( CStudioHdr *pStudio );
	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual void FireObsoleteEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual const char* ModifyEventParticles( const char* token ) { return token; }

	// Parses and distributes muzzle flash events
	virtual bool DispatchMuzzleEffect( const char *options, bool isFirstPerson );
	virtual void EjectParticleBrass( const char *pEffectName, const int iAttachment );

	// virtual	void AllocateMaterials( void );
	// virtual	void FreeMaterials( void );

	virtual CStudioHdr *OnNewModel( void );
	CStudioHdr	*GetModelPtr() const;
	virtual void InvalidateMdlCache() { UnlockStudioHdr(); delete m_pStudioHdr; m_pStudioHdr = nullptr; }
	
	virtual void SetPredictable( bool state );
	void UseClientSideAnimation();
	bool	IsUsingClientSideAnimation()	{ return m_bClientSideAnimation; }

	// CBaseClientShader **p_ClientShaders;

	virtual	void StandardBlendingRules( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternionAligned q[], float currentTime, int boneMask );
	void UnragdollBlend( CStudioHdr *hdr, BoneVector pos[], BoneQuaternion q[], float currentTime );

	bool m_bMaintainSequenceTransitions; // kill-switch so entities can opt out of automatic transitions
	void MaintainSequenceTransitions( IBoneSetup &boneSetup, float flCycle, BoneVector pos[], BoneQuaternion q[] );
	virtual void AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );

	// Attachments
	virtual int	LookupAttachment( const char *pAttachmentName );
	int		LookupRandomAttachment( const char *pAttachmentNameSubstring );

	int		LookupPoseParameter( CStudioHdr *pStudioHdr, const char *szName );
	inline int LookupPoseParameter( const char *szName ) { return LookupPoseParameter(GetModelPtr(), szName); }

	float	SetPoseParameter( CStudioHdr *pStudioHdr, const char *szName, float flValue );
	inline float SetPoseParameter( const char *szName, float flValue ) { return SetPoseParameter( GetModelPtr(), szName, flValue ); }
	float	SetPoseParameter( CStudioHdr *pStudioHdr, int iParameter, float flValue );
	inline float SetPoseParameter( int iParameter, float flValue ) { return SetPoseParameter( GetModelPtr(), iParameter, flValue ); }
	float	GetPoseParameter( int iParameter );

	float	GetPoseParameterRaw( int iPoseParameter );  // returns raw 0..1 value
	bool	GetPoseParameterRange( int iPoseParameter, float &minValue, float &maxValue );

	int		LookupBone( const char *szName );
	void	GetBonePosition( int iBone, Vector &origin, QAngle &angles );
	void	GetBonePosition( int iBone, Vector &origin );
	void	GetBoneTransform( int iBone, matrix3x4_t &pBoneToWorld );
	int		GetHitboxBone( int hitboxIndex );
	
	void	GetHitboxBonePosition( int iBone, Vector &origin, QAngle &angles, QAngle hitboxOrientation );
	void	GetHitboxBoneTransform( int iBone, QAngle hitboxOrientation, matrix3x4_t &pOut );

	void	CopySequenceTransitions( CBaseAnimating *pCopyFrom );
	//bool solveIK(float a, float b, const Vector &Foot, const Vector &Knee1, Vector &Knee2);
	//void DebugIK( mstudioikchain_t *pikchain );

	// Bone attachments
	virtual void		AttachEntityToBone( CBaseAnimating* attachTarget, int boneIndexAttached=-1, Vector bonePosition=Vector(0,0,0), QAngle boneAngles=QAngle(0,0,0) );
	void				AddBoneAttachment( CBaseAnimating* newBoneAttachment );
	void				RemoveBoneAttachment( CBaseAnimating* boneAttachment );
	void				RemoveBoneAttachments();
	void				DestroyBoneAttachments();
	void				MoveBoneAttachments( CBaseAnimating* attachTarget );
	int					GetNumBoneAttachments();
	CBaseAnimating*	GetBoneAttachment( int i );
	virtual void		NotifyBoneAttached( CBaseAnimating* attachTarget );

	virtual void		PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] ) {}

private:
	virtual void		UpdateBoneAttachments( void );

public:

	virtual void					PreDataUpdate( DataUpdateType_t updateType );
	virtual void					PostDataUpdate( DataUpdateType_t updateType );
	
	virtual void					NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void					OnPreDataChanged( DataUpdateType_t updateType );
	virtual void					OnDataChanged( DataUpdateType_t updateType );

	// This can be used to force client side animation to be on. Only use if you know what you're doing!
	// Normally, the server entity should set this.
	void							ForceClientSideAnimationOn();
	
	void							AddToClientSideAnimationList();
	void							RemoveFromClientSideAnimationList();

	virtual bool					IsSelfAnimating();
	virtual void					ResetLatched();

	// implements these so ragdolls can handle frustum culling & leaf visibility
	virtual Vector					GetThirdPersonViewPosition( void );
	virtual void					GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual const Vector&			GetRenderOrigin( void );
	virtual const QAngle&			GetRenderAngles( void );

	virtual bool					GetSoundSpatialization( SpatializationInfo_t& info );

	// Attachments.
	bool							GetAttachment( const char *szName, Vector &absOrigin );
	bool							GetAttachment( const char *szName, Vector &absOrigin, QAngle &absAngles );

	// Inherited from CBaseEntity
	virtual bool					GetAttachment( int number, Vector &origin );
	virtual bool					GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool					GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool					GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );
	virtual bool					ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter );

	virtual void					InvalidateAttachments();
	
	// Returns the attachment in local space
	bool							GetAttachmentLocal( int iAttachment, matrix3x4_t &attachmentToLocal );
	bool							GetAttachmentLocal( int iAttachment, Vector &origin, QAngle &angles );
	bool                            GetAttachmentLocal( int iAttachment, Vector &origin );

	// Should this object cast render-to-texture shadows?
	virtual ShadowType_t			ShadowCastType();

	// Should we collide?
	virtual CollideType_t			GetCollideType( void );

	virtual bool					TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual bool					TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	// returns true if we are of type CClientRagdoll
	virtual bool					IsClientRagdoll() const { return false; }

	// returns true if we're currently being ragdolled
	bool							IsRagdoll() const;
	virtual CBaseAnimating			*BecomeRagdollOnClient();
	virtual CClientRagdoll			*CreateClientRagdoll( bool bRestoring = false );
	CBaseAnimating					*CreateRagdollCopy();
	bool							InitAsClientRagdoll( const matrix3x4_t *pDeltaBones0, const matrix3x4_t *pDeltaBones1, const matrix3x4_t *pCurrentBonePosition, float boneDt, Vector vecForceOverride, bool bleedOut=true );
	bool							InitAsClientRagdoll( const matrix3x4_t *pDeltaBones0, const matrix3x4_t *pDeltaBones1, const matrix3x4_t *pCurrentBonePosition, float boneDt, bool bleedOut=true );
	void							IgniteRagdoll( CBaseAnimating *pSource );
	void							TransferDissolveFrom( CBaseAnimating *pSource );
	virtual void					SaveRagdollInfo( int numbones, const matrix3x4_t &cameraTransform, CBoneAccessor &pBoneToWorld );
	virtual bool					RetrieveRagdollInfo( BoneVector *pos, BoneQuaternion *q );
	virtual void					Clear( void );
	void							ClearRagdoll();
	void							CreateUnragdollInfo( CBaseAnimating *pRagdoll );
	void							ForceSetupBonesAtTime( matrix3x4a_t *pBonesOut, float flTime );
	virtual void					GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );

	// For shadows rendering the correct body + sequence...
	virtual int GetBody()			{ return m_nBody; }
	virtual int GetSkin()			{ return m_nSkin; }

	bool							IsOnFire() { return ( (GetFlags() & FL_ONFIRE) != 0 ); }
	float							GetFrozenAmount() { return m_flFrozen; }

	inline float					GetPlaybackRate() const;
	inline void						SetPlaybackRate( float rate );

	void							SetModelScale( float scale, ModelScaleType_t scaleType = HIERARCHICAL_MODEL_SCALE );
	inline float					GetModelScale() const { return m_flModelScale; }
	float							GetModelHierarchyScale() const;	// Get the overall scale of the entire hierarchy (model scale can be local, per-bone)

	ModelScaleType_t GetModelScaleType() const;
	void SetModelScaleType( ModelScaleType_t scaleType );

	inline bool						IsModelScaleFractional() const;  /// very fast way to ask if the model scale is < 1.0f  (faster than if (GetModelScale() < 1.0f) )

	int								GetSequence();
	virtual void					SetSequence(int nSequence);
	inline void						ResetSequence(int nSequence);
	void							OnNewSequence( );
	float							GetSequenceGroundSpeed( CStudioHdr *pStudioHdr, int iSequence );
	inline float					GetSequenceGroundSpeed( int iSequence ) { return GetSequenceGroundSpeed(GetModelPtr(), iSequence); }
	bool							IsSequenceLooping( CStudioHdr *pStudioHdr, int iSequence );
	inline bool						IsSequenceLooping( int iSequence ) { return IsSequenceLooping(GetModelPtr(),iSequence); }
	float							GetSequenceMoveDist( CStudioHdr *pStudioHdr, int iSequence );
	void							GetSequenceLinearMotion( int iSequence, Vector *pVec );
	float							GetSequenceLinearMotionAndDuration( int iSequence, Vector *pVec );
	bool							GetSequenceMovement( int nSequence, float fromCycle, float toCycle, Vector &deltaPosition, QAngle &deltaAngles );
	void							GetBlendedLinearVelocity( Vector *pVec );
	void							SetMovementPoseParams( const Vector &vecLocalVelocity, int iMoveX, int iMoveY, int iXSign = 1, int iYSign = 1 );
	int								LookupSequence ( const char *label );
	int								LookupSequence ( CStudioHdr* pHdr, const char *label );
	int								LookupActivity( const char *label );
	float							GetFirstSequenceAnimTag( int sequence, int nDesiredTag, float flStart = 0, float flEnd = 1 );
	float							GetAnySequenceAnimTag( int sequence, int nDesiredTag, float flDefault );
	char const						*GetSequenceName( int iSequence ); 
	char const						*GetSequenceActivityName( int iSequence );
	Activity						GetSequenceActivity( int iSequence );
	virtual void					StudioFrameAdvance(); // advance animation frame to some time in the future
	void							ExtractBbox( int nSequence, Vector &mins, Vector &maxs );

	// Clientside animation
	virtual float					FrameAdvance( float flInterval = 0.0f );
	virtual float					GetSequenceCycleRate( CStudioHdr *pStudioHdr, int iSequence );
	virtual float					GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence ) { return GetSequenceCycleRate(GetModelPtr(),iSequence); }
	virtual void					UpdateClientSideAnimation();
	void							ClientSideAnimationChanged();
	virtual unsigned int			ComputeClientSideAnimationFlags();
	float							GetGroundSpeed( void ) { return m_flGroundSpeed; }
	virtual void					ReachedEndOfSequence() { return; }

	void SetCycle( float flCycle );
	float GetCycle() const;

	void SetBodygroup( int iGroup, int iValue );
	int GetBodygroup( int iGroup );
	void SetBodygroupPreset( char const *szName );

	void SetSkin( int iSkin );
	void SetBody( int iBody );

	const char *GetBodygroupName( int iGroup );
	int FindBodygroupByName( const char *name );
	int GetBodygroupCount( int iGroup );
	int GetNumBodyGroups( void );

	void							SetHitboxSet( int setnum );
	void							SetHitboxSetByName( const char *setname );
	int								GetHitboxSet( void );
	char const						*GetHitboxSetName( void );
	int								GetHitboxSetCount( void );
	void							DrawClientHitboxes( float duration = 0.0f, bool monocolor = false );
	void							DrawSkeleton( CStudioHdr const* pHdr, int iBoneMask ) const;

	CBaseAnimating*				FindFollowedEntity();

	virtual bool					IsActivityFinished( void ) { return m_bSequenceFinished; }
	inline bool						IsSequenceFinished( void );
	inline bool						SequenceLoops( void ) { return m_bSequenceLoops; }

	// All view model attachments origins are stretched so you can place entities at them and
	// they will match up with where the attachment winds up being drawn on the view model, since
	// the view models are drawn with a different FOV.
	//
	// If you're drawing something inside of a view model's DrawModel() function, then you want the
	// original attachment origin instead of the adjusted one. To get that, call this on the 
	// adjusted attachment origin.
	virtual void					UncorrectViewModelAttachment( Vector &vOrigin ) {}

	// Call this if SetupBones() has already been called this frame but you need to move the
	// entity and rerender.
	void							InvalidateBoneCache();
	bool							IsBoneCacheValid() const;	// Returns true if the bone cache is considered good for this frame.
	void							GetCachedBoneMatrix( int boneIndex, matrix3x4_t &out );

	// Wrappers for CBoneAccessor.
	const matrix3x4a_t&				GetBone( int iBone ) const;
	matrix3x4a_t&					GetBoneForWrite( int iBone );
	matrix3x4a_t*					GetBoneArrayForWrite();

	bool							isBoneAvailableForRead( int iBone ) const;
	bool							isBoneAvailableForWrite( int iBone ) const;

	// Used for debugging. Will produce asserts if someone tries to setup bones or
	// attachments before it's allowed.
	// Use the "AutoAllowBoneAccess" class to auto push/pop bone access.
	// Use a distinct "tag" when pushing/popping - asserts when push/pop tags do not match.
	struct AutoAllowBoneAccess
	{
		AutoAllowBoneAccess( bool bAllowForNormalModels, bool bAllowForViewModels );
		~AutoAllowBoneAccess( void );
	};
	static void						PushAllowBoneAccess( bool bAllowForNormalModels, bool bAllowForViewModels, char const *tagPush );
	static void						PopBoneAccess( char const *tagPop );
	static void						ThreadedBoneSetup();
	static bool						InThreadedBoneSetup();
	static void						InitBoneSetupThreadPool();
	static void						ShutdownBoneSetupThreadPool();
	void							MarkForThreadedBoneSetup();
	static void						SetupBonesOnBaseAnimating( CBaseAnimating *&pBaseAnimating );

	// Invalidate bone caches so all SetupBones() calls force bone transforms to be regenerated.
	static void						InvalidateBoneCaches();
	// Enable/Disable use of stale data, instead of updating contents of bonecache
	static void						EnableNewBoneSetupRequest( bool bEnable ) { s_bEnableNewBoneSetupRequest = bEnable; };
	// Enable/Disable Invalidation of Bone Caches
	static void						EnableInvalidateBoneCache( bool bEnable ) { s_bEnableInvalidateBoneCache = bEnable; };

	// Purpose: My physics object has been updated, react or extract data
	virtual void					VPhysicsUpdate( IPhysicsObject *pPhysics );

	void DisableMuzzleFlash();		// Turn off the muzzle flash (ie: signal that we handled the server's event).
	virtual void DoMuzzleFlash();	// Force a muzzle flash event. Note: this only QUEUES an event, so
									// ProcessMuzzleFlashEvent will get called later.
	bool ShouldMuzzleFlash() const;	// Is the muzzle flash event on?

	// This is called to do the actual muzzle flash effect.
	virtual void ProcessMuzzleFlashEvent();
	
	// Update client side animations
	static void UpdateClientSideAnimations();

	// Load the model's keyvalues section and create effects listed inside it
	void InitModelEffects( void );

	// Sometimes the server wants to update the client's cycle to get the two to run in sync (for proper hit detection)
	virtual void SetServerIntendedCycle( float intended ) { intended; }
	virtual float GetServerIntendedCycle( void ) { return -1.0f; }

	// For prediction
	int								SelectWeightedSequence ( int activity );
	int								SelectWeightedSequenceFromModifiers( Activity activity, const ActivityModifierSym_t *pActivityModifiers, int iModifierCount );
	void							ResetSequenceInfo( void );
	float							SequenceDuration( void );
	float							SequenceDuration( CStudioHdr *pStudioHdr, int iSequence );
	inline float					SequenceDuration( int iSequence ) { return SequenceDuration(GetModelPtr(), iSequence); }
	int								FindTransitionSequence( int iCurrentSequence, int iGoalSequence, int *piDir );

	void							RagdollMoved( void );

	virtual void					GetToolRecordingState( KeyValues *msg );
	virtual void					CleanupToolRecordingState( KeyValues *msg );

	void							SetReceivedSequence( void );
	virtual bool					ShouldResetSequenceOnNewModel( void );

	// View models say yes to this.
	virtual bool					IsViewModel( void ) const;
	virtual bool					ShouldFlipModel( void ) { return false; }

	// viewmodel or viewmodelattachmentmodel or lowerbody
	virtual bool					IsViewModelOrAttachment( void ) const;

	void							EnableJiggleBones( void );
	void							DisableJiggleBones( void );

	void							ScriptSetPoseParameter( const char *szName, float fValue );

	void							SetRenderOriginOverride( const Vector &vec );
	void							DisableRenderOriginOverride( void );
	bool							IsUsingRenderOriginOverride( void ) { return m_vecRenderOriginOverride != vec3_invalid; }

	virtual CBaseAnimating *		GetBoneSetupDependancy( void ) { return GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : nullptr; }
	
	bool							GetRootBone( matrix3x4_t &rootBone );
	inline void						SetUseParentLightingOrigin( bool value ){ m_bUseParentLightingOrigin = value; }

	virtual void					SetCustomMaterial( ICustomMaterial *pCustomMaterial, int nIndex = 0 ) override;

	virtual void					SetAllowFastPath( bool bAllow ) { m_bCanUseFastPath = bAllow; }

protected:
	// View models scale their attachment positions to account for FOV. To get the unmodified
	// attachment position (like if you're rendering something else during the view model's DrawModel call),
	// use TransformViewModelAttachmentToWorld.
	virtual void					FormatViewModelAttachment( int nAttachment, matrix3x4_t &attachmentToWorld ) {}

	bool							IsBoneAccessAllowed() const;
	CMouthInfo&						MouthInfo();

	// Models used in a ModelPanel say yes to this
	virtual bool					IsMenuModel() const;

	// Allow studio models to tell CBaseEntity what their m_nBody value is
	virtual int						GetStudioBody( void ) { return m_nBody; }

	virtual bool					CalcAttachments();

	virtual bool					ComputeStencilState( ShaderStencilState_t *pStencilState );

	virtual bool					WantsInterpolatedVars() { return true; }

	virtual void					ResetSequenceLooping() { m_bSequenceFinished = false; }

private:
	// This method should return true if the bones have changed + SetupBones needs to be called
	virtual float					LastBoneChangedTime() { return std::numeric_limits<float>::max(); }

	CBoneList*						RecordBones( CStudioHdr *hdr, matrix3x4_t *pBoneState );

	bool							PutAttachment( int number, const matrix3x4_t &attachmentToWorld );
	void							TermRopes();

	void							DelayedInitModelEffects( void );
	void							ParseModelEffects( KeyValues *modelKeyValues );

	void							UpdateRelevantInterpolatedVars();
	void							AddBaseAnimatingInterpolatedVars();
	void							RemoveBaseAnimatingInterpolatedVars();

	void							LockStudioHdr();
	void							UnlockStudioHdr();

public:
	CRagdoll						*m_pRagdoll;
	CBaseAnimating					*m_pClientsideRagdoll;

	// Hitbox set to use (default 0)
	[[= ks::reflect::Net{} ]] int								m_nHitboxSet;

	CSequenceTransitioner			m_SequenceTransitioner;
private:

// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
// FTYPEDESC_INSENDTABLE STUFF
protected:

	//float							m_flCycle;
	// This needs to be ranged checked because some interpolation edge cases
	// can assign it to values far out of range. Interpolation vars will only
	// clamp range checked vars.
	CRangeCheckedVar<float, -2, 2, 0>		m_flCycle;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK } ]] [[= ks::reflect::Net{} ]] float							m_flPlaybackRate;// Animation playback framerate

// FTYPEDESC_INSENDTABLE STUFF (end)
public:
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] int								m_nSkin;// Texture group to use
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] int								m_nBody;// Object bodygroup

	int								m_nCustomBlendingRuleMask;

	unsigned int					m_nAnimLODflags;
	unsigned int					m_nAnimLODflagsOld;

	inline void SetAnimLODflag( unsigned int nNewFlag )		{ m_nAnimLODflags |= (nNewFlag); }
	inline void UnSetAnimLODflag( unsigned int nNewFlag )	{ m_nAnimLODflags &= (~nNewFlag); }
	inline bool IsAnimLODflagSet( unsigned int nFlag )		{ return (m_nAnimLODflags & (nFlag)) != 0; }
	inline void ClearAnimLODflags( void )					{ m_nAnimLODflags = 0; }

	int								m_nComputedLODframe;
	float							m_flDistanceFromCamera;

protected:
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK } ]] [[= ks::reflect::Net{} ]] int								m_nNewSequenceParity;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK } ]] [[= ks::reflect::Net{} ]] int								m_nResetEventsParity;
	int								m_nPrevNewSequenceParity;
	int								m_nPrevResetEventsParity;

	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.02f } ]] [[= ks::reflect::Net{} ]] float							m_flEncodedController[MAXSTUDIOBONECTRLS];	
private:
	// This is compared against m_nOldMuzzleFlashParity to determine if the entity should muzzle flash.
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] unsigned char					m_nMuzzleFlashParity;
// END PREDICTION DATA COMPACTION

	bool							ShouldSkipAnimationFrame( float currentTime );
	int								m_nLastNonSkippedFrame;

	BoneVector						m_pos_cached[MAXSTUDIOBONES];
	BoneQuaternionAligned			m_q_cached[MAXSTUDIOBONES];

protected:
	CIKContext						*m_pIk;

	int								m_iEyeAttachment;

	// Decomposed ragdoll info
	bool							m_bStoreRagdollInfo;
	RagdollInfo_t					*m_pRagdollInfo;
	[[= ks::reflect::Net{} ]] Vector							m_vecForce;
	[[= ks::reflect::Net{} ]] int								m_nForceBone;

	// Is bone cache valid
	// bone transformation matrix
	unsigned long					m_iMostRecentModelBoneCounter;
	unsigned long					m_iMostRecentBoneSetupRequest;
	CBaseAnimating *				m_pNextForThreadedBoneSetup;
	int								m_iPrevBoneMask;
	int								m_iAccumulatedBoneMask;

	static bool						s_bEnableInvalidateBoneCache;
	static bool						s_bEnableNewBoneSetupRequest;

	CBoneAccessor					m_BoneAccessor;
	std::recursive_mutex				m_BoneSetupLock;

	ClientSideAnimationListHandle_t	m_ClientSideAnimationListHandle;

	// Client-side animation
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool							m_bClientSideFrameReset;

	// Bone attachments. Used for attaching one BaseAnimating to another's bones.
	// Client side only.
	std::vector< CHandle<CBaseAnimating> > m_BoneAttachments;
	int								m_boneIndexAttached;
	Vector							m_bonePosition;
	QAngle							m_boneAngles;
	CHandle<CBaseAnimating>		m_pAttachedTo;

protected:

	[[= ks::reflect::Net{} ]] float							m_flFrozen;

	// Can we use the fast rendering path?
	bool							m_bCanUseFastPath;

private:
	float							m_flGroundSpeed;	// computed linear movement rate for current sequence
	float							m_flLastEventCheck;	// cycle index of when events were last checked
	bool							m_bSequenceFinished;// flag set when StudioAdvanceFrame moves across a frame boundry
	bool							m_bSequenceLoops;	// true if the sequence loops

	bool							m_bIsUsingRelativeLighting;

	// Mouth lipsync/envelope following values
	CMouthInfo						m_mouth;

	CNetworkVar( float, m_flModelScale, [[= ks::reflect::Net{} ]] );
	CNetworkVar( ModelScaleType_t, m_ScaleType, [[= ks::reflect::Net{} ]] );

	// Ropes that got spawned when the model was created.
	std::vector<CRopeKeyframe*> m_Ropes;

	// event processing info
	float							m_flPrevEventCycle;
	int								m_nEventSequence;

	// Animation blending factors
	[[= ks::reflect::Net{} ]] float							m_flPoseParameter[MAXSTUDIOPOSEPARAM];
	CInterpolatedVarArray< float, MAXSTUDIOPOSEPARAM >		m_iv_flPoseParameter;
	float							m_flOldPoseParameters[MAXSTUDIOPOSEPARAM];

	CInterpolatedVarArray< float, MAXSTUDIOBONECTRLS >		m_iv_flEncodedController;
	float							m_flOldEncodedController[MAXSTUDIOBONECTRLS];

	// Clientside animation
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool							m_bClientSideAnimation;
	bool							m_bLastClientSideFrameReset;

	Vector							m_vecPreRagdollMins;
	Vector							m_vecPreRagdollMaxs;
	bool							m_builtRagdoll;
	bool							m_bReceivedSequence;
	bool							m_bIsStaticProp;

	// Current animation sequence
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Sequence, ks::reflect::WireSide::Recv>{} ]] int								m_nSequence;

	// Current cycle location from server
protected:
	CInterpolatedVar< CRangeCheckedVar<float, -2, 2, 0> >		m_iv_flCycle;
	//CInterpolatedVar< float >		m_iv_flCycle;
	float							m_flOldCycle;
	float							m_prevClientCycle;
	float							m_prevClientAnimTime;

	// True if bone setup should latch bones for demo polish subsystem
	bool							m_bBonePolishSetup;

	virtual bool UpdateBlending( int flags, const RenderableInstance_t &instance );
	
	void CheckIfEntityShouldForceRTTShadows( void );
	ShadowType_t GetShadowCastTypeForStudio( CStudioHdr *pStudioHdr );
	bool m_bForceRTTShadows;

private:
	int m_nPrevBody;
	int m_nPrevSkin;

	float							m_flOldModelScale = 0.0f;
	int								m_nOldSequence = 0;
	CBoneMergeCache					*m_pBoneMergeCache = nullptr;	// This caches the strcmp lookups that it has to do
														// when merg

	std::vector< matrix3x4a_t >		m_CachedBoneData; // never access this directly. Use m_BoneAccessor.
	float							m_flLastBoneSetupTime = 0.0f;
	CJiggleBones					*m_pJiggleBones = nullptr;
	bool							m_isJiggleBonesEnabled;

	// Calculated attachment points
	std::vector< CAttachmentData >		m_Attachments;

	void							SetupBones_AttachmentHelper( CStudioHdr *pStudioHdr );
	[[= ks::reflect::Net{} ]] EHANDLE							m_hLightingOrigin;

	unsigned char					m_nOldMuzzleFlashParity;

	bool							m_bInitModelEffects;

	static bool						m_bBoneListInUse;
	static CBoneList				m_recordingBoneList;

	[[= ks::reflect::Net{} ]] bool							m_bSuppressAnimSounds;

private:
	mutable CStudioHdr				*m_pStudioHdr;
	mutable MDLHandle_t				m_hStudioHdr;
	std::recursive_mutex				m_StudioHdrInitLock;

	ks::tracked_ref<CNewParticleEffect>	m_ejectBrassEffect;
	int								m_iEjectBrassAttachment;

	Vector							m_vecRenderOriginOverride;
	bool							m_bUseParentLightingOrigin;

	friend class CBaseAnimatingOverlay;
};

enum 
{
	RAGDOLL_FRICTION_OFF = -2,
	RAGDOLL_FRICTION_NONE,
	RAGDOLL_FRICTION_IN,
	RAGDOLL_FRICTION_HOLD,
	RAGDOLL_FRICTION_OUT,
};

class CClientRagdoll : public CBaseAnimating, public IPVSNotify
{
public:
	CClientRagdoll( bool bRestoring = true , bool fullInit = true);
public:
	DECLARE_CLASS( CClientRagdoll, CBaseAnimating );

	// inherited from IClientUnknown
	virtual IClientModelRenderable*	GetClientModelRenderable();

	// inherited from IPVSNotify
	virtual void OnPVSStatusChanged( bool bInPVS );

	virtual void Release( void );
	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	virtual void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	void ClientThink( void );
	void ReleaseRagdoll( void ) { m_bReleaseRagdoll = true;	}
	bool ShouldSavePhysics( void ) { return true; }
	virtual void	OnSave();
	virtual void	OnRestore();
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_SAVE_NON_NETWORKABLE; }
	virtual IPVSNotify*				GetPVSNotifyInterface() { return this; }

	void	HandleAnimatedFriction( void );
	virtual void SUB_Remove( void );

	void	FadeOut( void );
	virtual float LastBoneChangedTime();

	inline bool IsFadingOut() { return m_bFadingOut; }

	bool m_bFadeOut;
	bool m_bImportant;
	float m_flEffectTime;

	// returns true if we are of type CClientRagdoll
	virtual bool					IsClientRagdoll() const { return true; }

private:
	int m_iCurrentFriction;
	int m_iMinFriction;
	int m_iMaxFriction;
	float m_flFrictionModTime;
	float m_flFrictionTime;

	int  m_iFrictionAnimState;
	bool m_bReleaseRagdoll;

	bool m_bFadingOut;

	float m_flScaleEnd[NUM_HITBOX_FIRES];
	float m_flScaleTimeStart[NUM_HITBOX_FIRES];
	float m_flScaleTimeEnd[NUM_HITBOX_FIRES];
};

//-----------------------------------------------------------------------------
// Purpose: Serves the 90% case of calling SetSequence / ResetSequenceInfo.
//-----------------------------------------------------------------------------
void CBaseAnimating::ResetSequence(int nSequence)
{
	SetSequence( nSequence );
	ResetSequenceInfo();
}

float CBaseAnimating::GetPlaybackRate() const
{
	return m_flPlaybackRate * clamp( 1.0f - m_flFrozen, 0.0f, 1.0f );
}

void CBaseAnimating::SetPlaybackRate( float rate )
{
	m_flPlaybackRate = rate;
}

//-----------------------------------------------------------------------------
// Purpose: return a pointer to an updated studiomdl cache cache
//-----------------------------------------------------------------------------

bool CBaseAnimating::IsModelScaleFractional() const   /// very fast way to ask if the model scale is < 1.0f
{
	static_assert( sizeof( m_flModelScale ) == sizeof( int ) );
	return *((const int *) &m_flModelScale) < 0x3f800000;
}

bool CBaseAnimating::IsSequenceFinished( void ) 
{ 
	return m_bSequenceFinished; 
}

float CBaseAnimating::SequenceDuration( void )
{ 
	return SequenceDuration( GetSequence() ); 
}

// FIXME: move these to somewhere that makes sense
void GetColumn( matrix3x4_t& src, int column, Vector &dest );
void SetColumn( Vector &src, int column, matrix3x4_t& dest );

EXTERN_RECV_TABLE(DT_BaseAnimating);

class CAnimationLayer;
class CBaseAnimating;

using namespace ::ks::reflect;

// For shared code.

namespace DT_OverlayVars { extern RecvTable g_RecvTable; }

namespace DT_Animationlayer { extern RecvTable g_RecvTable; }
void ResizeAnimationLayerCallback( void *pStruct, int offsetToUtlVector, int len );

class CBaseAnimatingOverlay : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseAnimatingOverlay" } ]]
	[[= ks::reflect::SubTable<"overlay_vars", &DT_OverlayVars::g_RecvTable, nullptr, true>{} ]]
	[[= ks::reflect::NetTable{ .name = "DT_OverlayVars", .base = false } ]]
	// 15, i.e. CBaseAnimatingOverlay::MAX_OVERLAYS -- the class is incomplete here
	[[= ks::reflect::UtlVec<"m_AnimOverlay", 15,
	&DT_Animationlayer::g_RecvTable,
	ResizeAnimationLayerCallback, "DT_OverlayVars">{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseAnimatingOverlay, CBaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	// Inherited from CBaseAnimating
public:
	virtual CBaseAnimatingOverlay *GetBaseAnimatingOverlay() { return this; }

	virtual void	AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	virtual void	DoAnimationEvents( CStudioHdr *pStudioHdr );
	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual CStudioHdr *OnNewModel();

	virtual bool	Interpolate( float flCurrentTime );

public:
	enum
	{
		MAX_OVERLAYS = 15,
	};

	CBaseAnimatingOverlay();
	CAnimationLayer* GetAnimOverlay( int i, bool bUseOrder = true );
	void			SetNumAnimOverlays( int num );	// This makes sure there is space for this # of layers.
	int				GetNumAnimOverlays() const;
	void			SetOverlayPrevEventCycle( int nSlot, float flValue );

	void			CheckInterpChanges( void );
	void			CheckForLayerPhysicsInvalidate( void );

	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence );
	void AccumulateDispatchedLayers( CBaseAnimatingOverlay *pWeapon, CStudioHdr *pWeaponStudioHdr, IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	void RegenerateDispatchedLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );

	void AccumulateInterleavedDispatchedLayers( CBaseAnimatingOverlay *pWeapon, IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime, bool bSetupInvisibleWeapon = false );

	virtual void	NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence ) {};
	virtual void	NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight ) {};
	virtual void	NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewCycle ) {};

private:
	void CheckForLayerChanges( CStudioHdr *hdr, float currentTime );

	std::vector< CAnimationLayer >	m_AnimOverlay;
	std::vector< CInterpolatedVar< CAnimationLayer > >	m_iv_AnimOverlay;

	float m_flOverlayPrevEventCycle[ MAX_OVERLAYS ];

	CBaseAnimatingOverlay( const CBaseAnimatingOverlay & ); // not defined, not accessible

	friend void ResizeAnimationLayerCallback( void *pStruct, int offsetToUtlVector, int len );
};

EXTERN_RECV_TABLE(DT_BaseAnimatingOverlay);

class CBaseEntity;
class CBaseFlex;

using namespace ::ks::reflect;


// named by the Proxy<> annotation below; defined in c_sceneentity.cpp
void RecvProxy_ForcedClientTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

// IMPLEMENT_CLIENTCLASS_DT, so this half carries a baseclass prop; the server half is _NOBASE
class CSceneEntity : public CBaseEntity, public IChoreoEventCallback
{
	[[= ks::reflect::NetTable{ .name = "DT_SceneEntity" } ]]
	[[= ks::reflect::UtlVec<"m_hActorList", MAX_ACTORS_IN_SCENE, nullptr>{} ]]
	static constexpr bool ks_class = true;

	friend class CChoreoEventCallback;

public:
	DECLARE_CLASS( CSceneEntity, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CSceneEntity( void );
	~CSceneEntity( void );

	// From IChoreoEventCallback
	virtual void			StartEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual void			EndEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual void			ProcessEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );
	virtual bool			CheckEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void PreDataUpdate( DataUpdateType_t updateType );

	virtual void StopClientOnlyScene();
	virtual void SetupClientOnlyScene( const char *pszFilename, CBaseFlex *pOwner = nullptr , bool bMultiplayer = false );

	virtual void ClientThink();

	void					OnResetClientTime();

	CHandle< CBaseFlex >	GetActor( int i ){ return ( i < std::ssize( m_hActorList ) ) ? m_hActorList[i] : INVALID_EHANDLE; }

	virtual	void			DispatchStartSpeak( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event, soundlevel_t iSoundlevel );
	virtual void			DispatchEndSpeak( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );

	bool IsClientOnly( void ){ return m_bClientOnly; }
	bool IsMultiplayer() const	{ return m_bMultiplayer; }

private:

	void					ResetActorFlexesForScene();

	// Scene load/unload
	CChoreoScene			*LoadScene( const char *filename );
	void					LoadSceneFromFile( const char *filename );
	void					UnloadScene( void );
	void					PrefetchAnimBlocks( CChoreoScene *pScene );

	CBaseFlex				*FindNamedActor( CChoreoActor *pChoreoActor );

	virtual void			DispatchStartFlexAnimation( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndFlexAnimation( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartExpression( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndExpression( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartGesture( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndGesture( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchStartSequence( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	virtual void			DispatchEndSequence( CChoreoScene *scene, CBaseFlex *actor, CChoreoEvent *event );
	void					DispatchProcessLoop( CChoreoScene *scene, CChoreoEvent *event );

	char const				*GetSceneFileName();

	void					DoThink( float frametime );

	void					ClearSceneEvents( CChoreoScene *scene, bool canceled );
	void					SetCurrentTime( float t, bool forceClientSync );

	bool					GetHWMorphSceneFileName( const char *pFilename, char *pHWMFilename );

private:

	void					CheckQueuedEvents();
	void					WipeQueuedEvents();
	void					QueueStartEvent( float starttime, CChoreoScene *scene, CChoreoEvent *event );

	[[= ks::reflect::Net{} ]] bool		m_bIsPlayingBack;
	[[= ks::reflect::Net{} ]] bool		m_bPaused;
	[[= ks::reflect::Net{} ]] bool		m_bMultiplayer;
	float		m_flCurrentTime;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_ForcedClientTime, ks::reflect::WireSide::Recv>{} ]] float		m_flForceClientTime;
	[[= ks::reflect::Net{} ]] int			m_nSceneStringIndex;
	bool		m_bClientOnly;

	CHandle< CBaseFlex >	m_hOwner; // if set, this overrides the m_hActorList in FindNamedActor()

	std::vector< CHandle< CBaseFlex > > m_hActorList;

private:
	bool		m_bWasPlaying;

	CChoreoScene *m_pScene;

	struct QueuedEvents_t
	{
		float			starttime;
		CChoreoScene	*scene;
		CChoreoEvent	*event;
	};

	std::vector< QueuedEvents_t > m_QueuedEvents;
};

class CBaseEntity;
class CBaseFlex;
class CSceneEntity;

typedef CHandle<CBaseEntity> EHANDLE;



//-----------------------------------------------------------------------------
// Purpose: One of a number of currently playing scene events for this actor
//-----------------------------------------------------------------------------
// FIXME: move this, it's only used in in baseflex and baseactor
class CSceneEventInfo
{
public:
	CSceneEventInfo();
	~CSceneEventInfo(); // out-of-line default destructor to deal with template on forward-declared class

	// The event handle of the current scene event
	CChoreoEvent	*m_pEvent;

	// Current Scene
	CChoreoScene	*m_pScene;

	// Current actor
	CChoreoActor	*m_pActor;

	CHandle< CSceneEntity >	m_hSceneEntity;

	// Set after the first time the event has been configured ( allows
	//  bumping markov index only at start of event playback, not every frame )
	bool			m_bStarted;

public:
	//	EVENT local data...
	// FIXME: Evil, make accessors or figure out better place
	// FIXME: This won't work, scenes don't save and restore...
	int						m_iLayer;
	int						m_iPriority;
	int						m_nSequence;
	bool					m_bIsGesture;
	float					m_flWeight; // used for suppressions of posture while moving

	// movement, faceto targets?
	EHANDLE					m_hTarget;
	bool					m_bIsMoving;
	bool					m_bHasArrived;
	float					m_flInitialYaw;
	float					m_flTargetYaw;
	float					m_flFacingYaw;

	// generic AI events
	int						m_nType;
	float					m_flNext;

	// is this event only client side?
	bool					m_bClientSide; 

	// cached flex file
	const flexsettinghdr_t *m_pExpHdr; 

	void					InitWeight( CBaseFlex *pActor );
	float					UpdateWeight( CBaseFlex *pActor );
};

//-----------------------------------------------------------------------------
// Purpose: Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	const char	*CurrentToken( void );
	bool		GetToken( bool crossline );
	bool		TokenAvailable( void );
	void		Error( const char *fmt, ... );
	void		SetBuffer( char *buffer );
private:
	const char	*m_pBuffer;
	char		m_szToken[ 1024 ];
};

extern CSceneTokenProcessor g_TokenProcessor;

void Scene_Printf( const char *pFormat, ... );
extern ConVar scene_clientflex;

//------------------------------------------------------------------------------
// Simple base class for screen space post-processing effects
//------------------------------------------------------------------------------
class IScreenSpaceEffect
{
public:

	virtual void Init( ) = 0;
	virtual void Shutdown( ) = 0;

	virtual void SetParameters( KeyValues *params ) = 0;

	virtual void Render( int x, int y, int w, int h ) = 0;

	virtual void Enable( bool bEnable ) = 0;
	virtual bool IsEnabled( ) = 0;
};

//------------------------------------------------------------------------------
// Interface class for managing screen space post-processing effects
//------------------------------------------------------------------------------
class IScreenSpaceEffectManager
{
public:

	virtual void InitScreenSpaceEffects( ) = 0;
	virtual void ShutdownScreenSpaceEffects( ) = 0;

	virtual IScreenSpaceEffect *GetScreenSpaceEffect( const char *pEffectName ) = 0;

	virtual void SetScreenSpaceEffectParams( const char *pEffectName, KeyValues *params ) = 0;
	virtual void SetScreenSpaceEffectParams( IScreenSpaceEffect *pEffect, KeyValues *params ) = 0;
    
	virtual void EnableScreenSpaceEffect( const char *pEffectName ) = 0;
	virtual void EnableScreenSpaceEffect( IScreenSpaceEffect *pEffect ) = 0;

	virtual void DisableScreenSpaceEffect( const char *pEffectName ) = 0;
	virtual void DisableScreenSpaceEffect( IScreenSpaceEffect *pEffect ) = 0;

	virtual void DisableAllScreenSpaceEffects( ) = 0;

	virtual void RenderEffects( int x, int y, int w, int h ) = 0;
};

extern IScreenSpaceEffectManager *g_pScreenSpaceEffects;

//-------------------------------------------------------------------------------------
// Registration class for adding screen space effects to the IScreenSpaceEffectManager
//-------------------------------------------------------------------------------------
class CScreenSpaceEffectRegistration
{
public:
	CScreenSpaceEffectRegistration( const char *pName, IScreenSpaceEffect *pEffect );

	const char			*m_pEffectName;
	IScreenSpaceEffect	*m_pEffect;

	CScreenSpaceEffectRegistration *m_pNext;

	static CScreenSpaceEffectRegistration *s_pHead;
};

class CBaseAnimatingOverlay;
class CBaseEntity;
class CSceneEntity;
struct Emphasized_Phoneme;

using namespace ::ks::reflect;

// Client-side CBasePlayer

//-----------------------------------------------------------------------------
// Purpose: Item in list of loaded scene files
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

// For phoneme emphasis track

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseFlex : public CBaseAnimatingOverlay
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseFlex" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseFlex, CBaseAnimatingOverlay );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					CBaseFlex();
	virtual			~CBaseFlex();

	virtual void Spawn();

	virtual IClientModelRenderable*	GetClientModelRenderable();

	virtual void InitPhonemeMappings();

	void		SetupMappings( char const *pchFileRoot );

	virtual CStudioHdr *OnNewModel( void );

	virtual void	StandardBlendingRules( CStudioHdr *hdr, BoneVector pos[], BoneQuaternionAligned q[], float currentTime, int boneMask );

	virtual void OnThreadedDrawSetup();

	// model specific
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	virtual	void	SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	virtual bool	UsesFlexDelayedWeights();

	virtual bool	GetSoundSpatialization( SpatializationInfo_t& info );

	virtual void	GetToolRecordingState( KeyValues *msg );

	// Called at the lowest level to actually apply a flex animation
	void			AddFlexAnimation( CSceneEventInfo *info );

	void			SetFlexWeight( LocalFlexController_t index, float value );
	float			GetFlexWeight( LocalFlexController_t index );

	// Look up flex controller index by global name
	LocalFlexController_t				FindFlexController( const char *szName );

public:
	[[= ks::reflect::Net{} ]] Vector			m_viewtarget;
	CInterpolatedVar< Vector >	m_iv_viewtarget;
	// indexed by model local flexcontroller
	[[= ks::reflect::Net{} ]] float			m_flexWeight[MAXSTUDIOFLEXCTRL];
	CInterpolatedVarArray< float, MAXSTUDIOFLEXCTRL >	m_iv_flexWeight;

	[[= ks::reflect::Net{} ]] int				m_blinktoggle;

	static int		AddGlobalFlexController( const char *szName );
	static char const *GetGlobalFlexControllerName( int idx );

	// bah, this should be unified with all prev/current stuff.

public:

	// Keep track of what scenes are being played
	void				StartChoreoScene( CChoreoScene *scene );
	void				RemoveChoreoScene( CChoreoScene *scene );

	// Start the specifics of an scene event
	virtual bool		StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );

	virtual bool		ProcessSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );

	// Remove all playing events
	void				ClearSceneEvents( CChoreoScene *scene, bool canceled );

	// Stop specifics of event
	virtual	bool		ClearSceneEvent( CSceneEventInfo *info, bool fastKill, bool canceled );

	// Add the event to the queue for this actor
	void				AddSceneEvent( CChoreoScene *scene, CChoreoEvent *event, CBaseEntity *pTarget = nullptr, bool bClientSide = false, CSceneEntity *pSceneEntity = nullptr );

	// Remove the event from the queue for this actor
	void				RemoveSceneEvent( CChoreoScene *scene, CChoreoEvent *event, bool fastKill );

	// Checks to see if the event should be considered "completed"
	bool				CheckSceneEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	// Checks to see if a event should be considered "completed"
	virtual bool		CheckSceneEventCompletion( CSceneEventInfo *info, float currenttime, CChoreoScene *scene, CChoreoEvent *event );

	int					FlexControllerLocalToGlobal( const flexsettinghdr_t *pSettinghdr, int key );
	void				EnsureTranslations( const flexsettinghdr_t *pSettinghdr );

	// For handling scene files
	const void			*FindSceneFile( const char *filename );

	static void			InvalidateFlexCaches();
	bool				IsFlexCacheValid() const;

private:
	Vector	SetViewTarget( CStudioHdr *pStudioHdr, const float *pGlobalFlexWeight );
	void	RunFlexRules( CStudioHdr *pStudioHdr, const float *pGlobalFlexWeight, float *dest );

	// Manipulation of events for the object
	// Should be called by think function to process all scene events
	// The default implementation resets m_flexWeight array and calls
	//  AddSceneEvents
	void ProcessSceneEvents( bool bFlexEvents, float *pGlobalFlexWeight );

	// Assumes m_flexWeight array has been set up, this adds the actual currently playing
	//  expressions to the flex weights and adds other scene events as needed
	bool ProcessSceneEvent( float *pGlobalFlexWeight, bool bFlexEvents, CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );

	bool RequestStartSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );

	bool ProcessFlexAnimationSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	bool ProcessFlexSettingSceneEvent( float *pGlobalFlexWeight, CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event );
	void AddFlexSetting( float *pGlobalFlexWeight, const char *expr, float scale, 
		const flexsettinghdr_t *pSettinghdr, bool newexpression );

	// Array of active SceneEvents, in order oldest to newest
	std::vector< CSceneEventInfo >		m_SceneEvents;
	std::vector< CChoreoScene * >		m_ActiveChoreoScenes;

	bool				HasSceneEvents() const;

private:
	// Mapping for each loaded scene file used by this actor: local flex controller index -> global
	std::map< const flexsettinghdr_t *, std::vector< int > > m_LocalToGlobal;

	float			m_blinktime;
	int				m_prevblinktoggle;

	int						m_iBlink;
	LocalFlexController_t	m_iEyeUpdown;
	LocalFlexController_t	m_iEyeRightleft;
	int				m_iMouthAttachment;

	float			*m_flFlexDelayedWeight;

	int					m_iMostRecentFlexCounter;
	Vector				m_CachedViewTarget;
	std::vector< float > m_CachedFlexWeights;
	std::vector< float > m_CachedDelayedFlexWeights;

	// shared flex controllers
	static int		g_numflexcontrollers;
	static char		*g_flexcontroller[MAXSTUDIOFLEXCTRL*4]; // room for global set of flexcontrollers
	static float	s_pGlobalFlexWeight[MAXSTUDIOFLEXCTRL*4];

protected:

	enum
	{
		PHONEME_CLASS_WEAK = 0,
		PHONEME_CLASS_NORMAL,
		PHONEME_CLASS_STRONG,

		NUM_PHONEME_CLASSES
	};

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	struct Emphasized_Phoneme
	{
		// Global fields, setup at start
		char			classname[ 64 ];
		bool			required;
		// Global fields setup first time tracks played
		bool			basechecked;
		const flexsettinghdr_t *base;
		const flexsetting_t *exp;

		// Local fields, processed for each sentence
		bool			valid;
		float			amount;
	};

	Emphasized_Phoneme m_PhonemeClasses[ NUM_PHONEME_CLASSES ];

private:

	CBaseFlex( const CBaseFlex & ); // not defined, not accessible

	const flexsetting_t *FindNamedSetting( const flexsettinghdr_t *pSettinghdr, const char *expr );

	void			ProcessVisemes( Emphasized_Phoneme *classes, float *pGlobalFlexWeight );
	void			AddVisemesForSentence( float *pGlobalFlexWeight, Emphasized_Phoneme *classes, float emphasis_intensity, CSentence *sentence, float t, float dt, bool juststarted );
	void			AddViseme( float *pGlobalFlexWeight, Emphasized_Phoneme *classes, float emphasis_intensity, int phoneme, float scale, bool newexpression );
	bool			SetupEmphasisBlend( Emphasized_Phoneme *classes, int phoneme );
	void			ComputeBlendedSetting( Emphasized_Phoneme *classes, float emphasis_intensity );

};

EXTERN_RECV_TABLE(DT_BaseFlex);

float *GetVisemeWeights( int phoneme );

// Purpose: Has init functions for all the standard render targets used by most games.
//			Mods who wish to make their own render targets can inherit from this class
//			and in the 'InitClientRenderTargets' interface called by the engine, set up
//			their own render targets as well as calling the init functions for various
//			common render targets provided by this class.
//			Note: Unless the client defines a singleton interface by inheriting from
//			this class and exposing the singleton instance, these init and shutdown
//			functions WILL NOT be called by the engine.

// Externs

class CBaseClientRenderTargets : public IClientRenderTargets
{
	// no networked vars
	DECLARE_CLASS_GAMEROOT( CBaseClientRenderTargets, IClientRenderTargets );
public:
	// Interface called by engine during material system startup.
	virtual void InitClientRenderTargets ( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig );
	// Shutdown all custom render targets here.
	virtual void ShutdownClientRenderTargets ( void );

protected:
	void SetupClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig, int iWaterTextureSize = 1024, int iCameraTextureSize = 256 );

	// Standard render textures used by most mods-- Classes inheriting from
	// this can choose to init these or not depending on their needs.

	// For reflective and refracting water
	CTextureReference		m_WaterReflectionTexture;
	CTextureReference		m_WaterRefractionTexture;

	// Used for monitors
	CTextureReference		m_CameraTexture;

	CTextureReference		m_RenderToRTHelperTexture;

	// Init functions for the common render targets
	ITexture* CreateWaterReflectionTexture( IMaterialSystem* pMaterialSystem, int iSize = 1024 );
	ITexture* CreateWaterRefractionTexture( IMaterialSystem* pMaterialSystem, int iSize = 1024 );
	ITexture* CreateCameraTexture( IMaterialSystem* pMaterialSystem, int iSize = 256 );

};

class CBaseCombatWeapon;
class CBaseEntity;
class CBaseFlex;

using namespace ::ks::reflect;

// Purpose: Defines the client-side representation of CBaseCombatCharacter.

inline constexpr auto BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE = 0.9f;

class C_WeaponCombatShield;

namespace DT_BCCLocalPlayerExclusive { extern RecvTable g_RecvTable; }
namespace DT_BCCNonLocalPlayerExclusive { extern RecvTable g_RecvTable; }

class CBaseCombatCharacter : public CBaseFlex
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseCombatCharacter" } ]]
	[[= ks::reflect::NetTable{ .name = "DT_BCCLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_BCCNonLocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::SubTable<"bcc_localdata", &DT_BCCLocalPlayerExclusive::g_RecvTable,
	nullptr, true>{} ]]
	[[= ks::reflect::SubTable<"bcc_nonlocaldata", &DT_BCCNonLocalPlayerExclusive::g_RecvTable,
	nullptr, true>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseCombatCharacter, CBaseFlex );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					CBaseCombatCharacter( void );
	virtual			~CBaseCombatCharacter( void );

	virtual bool	IsBaseCombatCharacter( void ) { return true; };
	virtual CBaseCombatCharacter *MyCombatCharacterPointer( void ) { return this; }

	virtual void DropPhysicsMag( const char *options ) {}

	// -----------------------
	// Vision
	// -----------------------
	enum FieldOfViewCheckType { USE_FOV, DISREGARD_FOV };
	bool IsAbleToSee( const CBaseEntity *entity, FieldOfViewCheckType checkFOV );	// Visible starts with line of sight, and adds all the extra game checks like fog, smoke, camo...
	bool IsAbleToSee( CBaseCombatCharacter *pBCC, FieldOfViewCheckType checkFOV );	// Visible starts with line of sight, and adds all the extra game checks like fog, smoke, camo...

	virtual bool IsLookingTowards( const CBaseEntity *target, float cosTolerance = BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE ) const;	// return true if our view direction is pointing at the given target, within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
	virtual bool IsLookingTowards( const Vector &target, float cosTolerance = BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE ) const;	// return true if our view direction is pointing at the given target, within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.

	virtual bool IsInFieldOfView( CBaseEntity *entity ) const;	// Calls IsLookingAt with the current field of view.  
	virtual bool IsInFieldOfView( const Vector &pos ) const;

	enum LineOfSightCheckType
	{
		IGNORE_NOTHING,
		IGNORE_ACTORS
	};
	virtual bool IsLineOfSightClear( CBaseEntity *entity, LineOfSightCheckType checkType = IGNORE_NOTHING ) const;// strictly LOS check with no other considerations
	virtual bool IsLineOfSightClear( const Vector &pos, LineOfSightCheckType checkType = IGNORE_NOTHING, CBaseEntity *entityToIgnore = nullptr ) const;

	int	LastHitGroup() const { return m_LastHitGroup; }

	// -----------------------
	// Ammo
	// -----------------------
	void				RemoveAmmo( int iCount, int iAmmoIndex );
	void				RemoveAmmo( int iCount, const char *szName );
	void				RemoveAllAmmo( );
	int					GetAmmoCount( int iAmmoIndex ) const;
	int					GetAmmoCount( char *szName ) const;

	virtual CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual CBaseCombatWeapon *Weapon_GetSlot( int slot ) const;
	virtual CBaseCombatWeapon *Weapon_GetPosition( int pos ) const;

	virtual	bool		Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual bool		Weapon_CanSwitchTo(CBaseCombatWeapon *pWeapon);
	
	// I can't use my current weapon anymore. Switch me to the next best weapon.
	bool SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent);

	virtual CBaseCombatWeapon	*GetActiveWeapon( void ) const;
	int							WeaponCount() const;
	virtual CBaseCombatWeapon	*GetWeapon( int i ) const;

	// This is a sort of hack back-door only used by physgun!
	void SetAmmoCount( int iCount, int iAmmoIndex );

	bool HasEverBeenInjured( void ) const;
	float GetTimeSinceLastInjury( void ) const;
	RelativeDamagedDirection_t GetLastInjuryRelativeDirection( void ) { return m_nRelativeDirectionOfLastInjury; }

	float				GetNextAttack() const { return m_flNextAttack; }
	void				SetNextAttack( float flWait ) { m_flNextAttack = flWait; }

	virtual int			BloodColor();

	// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
	void SetBloodColor( int nBloodColor );

	virtual void DoMuzzleFlash();

public:

// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
	[[= ks::reflect::Net{ .table = "DT_BCCLocalPlayerExclusive" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] float			m_flNextAttack;

private:
	bool ComputeLOS( const Vector &vecEyePosition, const Vector &vecTarget ) const;

public:
	[[= ks::reflect::Net{} ]] int m_LastHitGroup;

private:
	CNetworkArray( int, m_iAmmo, MAX_AMMO_TYPES, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] CHandle<CBaseCombatWeapon>		m_hMyWeapons[MAX_WEAPONS];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] CHandle< CBaseCombatWeapon > m_hActiveWeapon;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::As{ FIELD_TIME } ]] float m_flTimeOfLastInjury;
	[[= ks::reflect::Net{} ]] RelativeDamagedDirection_t m_nRelativeDirectionOfLastInjury;

// END PREDICTION DATA COMPACTION

protected:

	int			m_bloodColor;			// color of blood particless

private:

private:
	CBaseCombatCharacter( const CBaseCombatCharacter & ); // not defined, not accessible

//-----------------------

public:
	virtual void		OnDataChanged( DataUpdateType_t updateType );
};

inline CBaseCombatCharacter *ToBaseCombatCharacter( CBaseEntity *pEntity )
{
	return pEntity ? pEntity->MyCombatCharacterPointer() : nullptr;
}

EXTERN_RECV_TABLE(DT_BaseCombatCharacter);

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Functionality to render a glowing outline around client renderable objects.

inline const int GLOW_FOR_ALL_SPLIT_SCREEN_SLOTS = -1;

enum GlowRenderStyle_t
{
	GLOWRENDERSTYLE_DEFAULT = 0,
	GLOWRENDERSTYLE_RIMGLOW3D,
	GLOWRENDERSTYLE_EDGE_HIGHLIGHT,
	GLOWRENDERSTYLE_EDGE_HIGHLIGHT_PULSE,
	GLOWRENDERSTYLE_COUNT,
};

class CGlowObjectManager
{
public:
	CGlowObjectManager() :
	  m_nFirstFreeSlot( GlowObjectDefinition_t::END_OF_FREE_LIST )
	  {
	  }

	  int RegisterGlowObject( CBaseEntity *pEntity, const Vector &vGlowColor, float flGlowAlpha, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded, int nSplitScreenSlot );

	  int AddGlowBox( Vector vecOrigin, QAngle angOrientation, Vector mins, Vector maxs, Color colColor, float flLifetime );

	  void UnregisterGlowObject( int nGlowObjectHandle );

	  void UnregisterGlowObject( CBaseEntity *pEntity );

	  void SetGlowAlphaPulseOverdrive( int nGlowObjectHandle, float flAdditionalAlpha );

	  void UnregisterAllGlowBoxes()
	  {
		  m_GlowBoxDefinitions.clear();
	  }

	  void SetGlowAlphaCappedByRenderAlpha( int nGlowObjectHandle, bool bGlowAlphaCappedByRenderAlpha );

	  void SetGlowAlphaFunctionOfMaxVelocity( int nGlowObjectHandle, float flMaxVelocity );

	  void SetGlowAlphaMax( int nGlowObjectHandle, float flGlowAlphaMax );

	  void SetEntity( int nGlowObjectHandle, CBaseEntity *pEntity );

	  void SetColor( int nGlowObjectHandle, const Vector &vGlowColor ) 
	  { 
		  contract_assert( !m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused() );
		  m_GlowObjectDefinitions[nGlowObjectHandle].m_vGlowColor = vGlowColor;
	  }

	  void SetAlpha( int nGlowObjectHandle, float flAlpha );

	  void SetRenderFlags( int nGlowObjectHandle, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded );

	  void SetRenderStyle( int nGlowObjectHandle, GlowRenderStyle_t nNewRenderStyle );

	  void SetFullBloomRender( int nGlowObjectHandle, bool bFullBloomRender, int nStencilTestValue );

	  bool IsRenderingWhenOccluded( int nGlowObjectHandle ) const;

	  bool IsRenderingWhenUnoccluded( int nGlowObjectHandle ) const;

	  void RenderGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot );

private:

	void ApplyEntityGlowEffects( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, float flBloomScale, int x, int y, int w, int h );

	void RenderGlowBoxes( int iPass, CMatRenderContextPtr &pRenderContext );

	struct GlowObjectDefinition_t
	{
		bool ShouldDraw( int nSlot ) const { 
			return m_pEntity && ( m_nSplitScreenSlot == GLOW_FOR_ALL_SPLIT_SCREEN_SLOTS || m_nSplitScreenSlot == nSlot ) && ( m_bRenderWhenOccluded || m_bRenderWhenUnoccluded ) && m_pEntity->ShouldDraw(); 
		}
		bool IsUnused() const { 
			return m_nNextFreeSlot != GlowObjectDefinition_t::ENTRY_IN_USE; 
		}
		void DrawModel();

		CBaseEntity* m_pEntity;
		Vector m_vGlowColor;
		float m_flGlowAlpha;

		bool m_bGlowAlphaCappedByRenderAlpha;
		float m_flGlowAlphaFunctionOfMaxVelocity;
		float m_flGlowAlphaMax;
		float m_flGlowPulseOverdrive;
		bool m_bRenderWhenOccluded;
		bool m_bRenderWhenUnoccluded;
		bool m_bFullBloomRender;
		int m_nFullBloomStencilTestValue; // only render full bloom objects if stencil is equal to this value (value of -1 implies no stencil test)
		int m_nRenderStyle;
		int m_nSplitScreenSlot;

		// Linked list of free slots
		int m_nNextFreeSlot;

		// Special values for GlowObjectDefinition_t::m_nNextFreeSlot
		static const int END_OF_FREE_LIST = -1;
		static const int ENTRY_IN_USE = -2;
	};

	void RenderGlowModels( const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext, std::vector< GlowObjectDefinition_t > &vecGlowObjects );

	void DownSampleAndBlurRT( const CViewSetup *pSetup, CMatRenderContextPtr &pRenderContext, float flBloomScale, ITexture *pRtFullFrame, ITexture *pRtQuarterSize0, ITexture *pRtQuarterSize1 );

	std::vector< GlowObjectDefinition_t > m_GlowObjectDefinitions;
	int m_nFirstFreeSlot;

	struct GlowBoxDefinition_t
	{
		Vector m_vPosition;
		QAngle m_angOrientation;
		Vector m_vMins;
		Vector m_vMaxs;
		float m_flBirthTimeIndex;
		float m_flTerminationTimeIndex; //when to die
		Color m_colColor;
	};

	std::vector< GlowBoxDefinition_t > m_GlowBoxDefinitions;

};

CGlowObjectManager &GlowObjectManager();

class CGlowObject
{
public:
	CGlowObject( CBaseEntity *pEntity, const Vector &vGlowColor = Vector( 1.0f, 1.0f, 1.0f ), float flGlowAlpha = 1.0f, bool bRenderWhenOccluded = false, bool bRenderWhenUnoccluded = false, int nSplitScreenSlot = GLOW_FOR_ALL_SPLIT_SCREEN_SLOTS )
	{
		m_nGlowObjectHandle = GlowObjectManager().RegisterGlowObject( pEntity, vGlowColor, flGlowAlpha, bRenderWhenOccluded, bRenderWhenUnoccluded, nSplitScreenSlot );
	}

	~CGlowObject()
	{
		GlowObjectManager().UnregisterGlowObject( m_nGlowObjectHandle );
	}

	void SetGlowAlphaPulseOverdrive( float flAdditionalAlpha );

	void SetGlowAlphaCappedByRenderAlpha( bool bGlowAlphaCappedByRenderAlpha )
	{
		GlowObjectManager().SetGlowAlphaCappedByRenderAlpha( m_nGlowObjectHandle, bGlowAlphaCappedByRenderAlpha );
	}

	void SetGlowAlphaFunctionOfMaxVelocity( float flMaxVelocity )
	{
		GlowObjectManager().SetGlowAlphaFunctionOfMaxVelocity( m_nGlowObjectHandle, flMaxVelocity );
	}

	void SetGlowAlphaMax( float flGlowAlphaMax )
	{
		GlowObjectManager().SetGlowAlphaMax( m_nGlowObjectHandle, flGlowAlphaMax );
	}

	void SetEntity( CBaseEntity *pEntity )
	{
		GlowObjectManager().SetEntity( m_nGlowObjectHandle, pEntity );
	}

	void SetColor( const Vector &vGlowColor )
	{
		GlowObjectManager().SetColor( m_nGlowObjectHandle, vGlowColor );
	}

	void SetAlpha( float flAlpha )
	{
		GlowObjectManager().SetAlpha( m_nGlowObjectHandle, flAlpha );
	}

	void SetRenderFlags( bool bRenderWhenOccluded, bool bRenderWhenUnoccluded )
	{
		GlowObjectManager().SetRenderFlags( m_nGlowObjectHandle, bRenderWhenOccluded, bRenderWhenUnoccluded );
	}

	void SetRenderStyle( GlowRenderStyle_t nNewRenderStyle )
	{
		GlowObjectManager().SetRenderStyle( m_nGlowObjectHandle, nNewRenderStyle );
	}

	void SetFullBloomRender( bool bFullBloomRender, int nStencilTestValue = -1 )
	{
		return GlowObjectManager().SetFullBloomRender( m_nGlowObjectHandle, bFullBloomRender, nStencilTestValue );
	}

	bool IsRenderingWhenOccluded() const
	{
		return GlowObjectManager().IsRenderingWhenOccluded( m_nGlowObjectHandle );
	}

	bool IsRenderingWhenUnoccluded() const
	{
		return GlowObjectManager().IsRenderingWhenUnoccluded( m_nGlowObjectHandle );
	}

	bool IsRendering() const
	{
		return IsRenderingWhenOccluded() || IsRenderingWhenUnoccluded();
	}

	// Add more accessors/mutators here as needed

private:
	int m_nGlowObjectHandle;

	// Assignment & copy-construction disallowed
	CGlowObject( const CGlowObject &other );
	CGlowObject& operator=( const CGlowObject &other );
};

class CTakeDamageInfo;
class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseEntity;

using namespace ::ks::reflect;


inline constexpr auto BASEGRENADE_EXPLOSION_VOLUME = 1024;

void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void RecvProxy_LocalVelocity( const CRecvProxyData *pData, void *pStruct, void *pOut );


class CBaseGrenade : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseGrenade" } ]]
	[[= ks::reflect::From<"m_flDamage", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_DmgRadius", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_bIsLive", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_hThrower", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{}, RecvProxy_LocalVelocity>{} ]]
	[[= ks::reflect::From<"m_fFlags", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_vecVelocity", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.5f }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseGrenade, CBaseAnimating );
public:

	CBaseGrenade(void);
	~CBaseGrenade(void);

	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	virtual void		Precache( void );

	virtual void		Explode( trace_t *pTrace, int bitsDamageType );
	void				Smoke( void );

	void				BounceTouch( CBaseEntity *pOther );
	void				SlideTouch( CBaseEntity *pOther );
	void				ExplodeTouch( CBaseEntity *pOther );
	void				DangerSoundThink( void );
	void				PreDetonate( void );
	virtual void		Detonate( void );
	void				DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void				TumbleThink( void );

	virtual Vector		GetBlastForce() { return vec3_origin; }

	virtual void		BounceSound( void );
	virtual int			BloodColor( void ) { return DONT_BLEED; }
	virtual void		Event_Killed( const CTakeDamageInfo &info );

	virtual float		GetShakeAmplitude( void ) { return 25.0; }
	virtual float		GetShakeRadius( void ) { return 750.0; }

	virtual const char *GetParticleSystemName( int pointContents, surfacedata_t *pdata = nullptr ) { return nullptr; }

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

	// Damage accessors.
	virtual float GetDamage()
	{
		return m_flDamage;
	}
	virtual float GetDamageRadius()
	{
		return m_DmgRadius;
	}

	virtual void SetDamage(float flDamage)
	{
		m_flDamage = flDamage;
	}

	virtual void SetDamageRadius(float flDamageRadius)
	{
		m_DmgRadius = flDamageRadius;
	}

	// Bounce sound accessors.
	void SetBounceSound( const char *pszBounceSound ) 
	{
		m_iszBounceSound = MAKE_STRING( pszBounceSound );
	}

	CBaseCombatCharacter *GetThrower( void );
	void				  SetThrower( CBaseCombatCharacter *pThrower );
	CBaseEntity *GetOriginalThrower() { return m_hOriginalThrower; }

public:
	
	bool				m_bHasWarnedAI;				// whether or not this grenade has issued its DANGER sound to the world sound list yet.
	CNetworkVar( bool, m_bIsLive, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );					// Is this grenade live, or can it be picked up?
	CNetworkVar( float, m_DmgRadius, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// How far do I do damage?
	CNetworkVar( float, m_flNextAttack, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );
	float				m_flDetonateTime;			// Time at which to detonate.
	float				m_flWarnAITime;				// Time at which to warn the AI

	CGlowObject			m_GlowObject;

protected:

	CNetworkVar( float, m_flDamage, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );		// Damage to inflict.
	string_t m_iszBounceSound;	// The sound to make on bouncing.  If not NULL, overrides the BounceSound() function.

private:
	CNetworkHandle( CBaseEntity, m_hThrower, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );					// Who threw this grenade
	EHANDLE			m_hOriginalThrower;							// Who was the original thrower of this grenade

	CBaseGrenade( const CBaseGrenade & ); // not defined, not accessible

};
