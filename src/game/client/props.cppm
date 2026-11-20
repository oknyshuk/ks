// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include "studio.h"
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "filesystem.h"
#include "vcollide_parse.h"
#include "vphysics/object_hash.h"
#include "mathlib/softbodyenvironment.h"
#include "cmodel.h"
#include "vphysics_interface.h"
#include "predictable_entity.h"
#include "const.h"
#include "string_t.h"
#include "materialsystem/imaterialproxy.h"
#include "bone_accessor.h"
#include "vphysics/constraints.h"
#include "ihandleentity.h"
#include "ispatialpartition.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "irecipientfilter.h"
#include "vscript/ivscript.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "weapon__stubs.h"
#include "baseentity_shared.h"
#include "engine/ivdebugoverlay.h"
#include "engine/ivmodelinfo.h"
#include "icliententity.h"
#include "ivrenderview.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "networkvar.h"
#include "view_shared.h"
#include "materialsystem/imaterial.h"
module ks.client:props;

import std;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.core.basetypes;
import ks.tier1.reflect_wirevar;
import :baseanimatedtextureproxy;
import :cstrike15.cs_player;
import :particlemgr;
import ks.math.pfns;
import ks.tier1.interfaces;
import :baseentity;
import ks.game.shareddefs;
import ks.tier1.convar;
import ks.tier1.keyvalues;
import ks.core.vprof;
import ks.tier1.createinterface;
import ks.core.logging;
import ks.core.numeric;
import ks.core.types;
import ks.core.wchartypes;
import :iinput;

void BlackBox_Record(const char * type, const char *fmt, ...);

class CBaseAnimating;

#undef CBaseAnimating

class CBoneMergeCache
{
public:

	CBoneMergeCache();
	
	void Init( CBaseAnimating *pOwner );

	// Updates the lookups that let it merge bones quickly.
	void UpdateCache();
	
	// This copies the transform from all bones in the followed entity that have 
	// names that match our bones.
	void MergeMatchingBones( int boneMask );
	
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

class CBaseAnimating;
class CBasePlayer;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RecvProxy_UnmodifiedQAngles( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CBreakableProp : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
	[[= ks::reflect::NetTable{ .name = "DT_BreakableProp" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBreakableProp, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	CBreakableProp();

	virtual bool IsProp( void ) const
	{
		return true;
	};

	//IPlayerPickupVPhysics
	virtual bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle PreferredCarryAngles( void );

	virtual bool	ShouldPredict( void );
	virtual CBasePlayer *GetPredictionOwner( void );
	virtual bool PredictionErrorShouldResetLatchedForAllPredictables( void ) { return false; }

	// Copy fade from another breakable prop
	void CopyFadeFrom( CBreakableProp *pSource );
	virtual void OnDataChanged( DataUpdateType_t type );

	const QAngle &GetNetworkedPreferredPlayerCarryAngles( void ) { return m_qPreferredPlayerCarryAngles; }

protected:
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::QAngles } ]] [[= ks::reflect::Proxy<RecvProxy_UnmodifiedQAngles, ks::reflect::WireSide::Recv>{} ]] QAngle m_qPreferredPlayerCarryAngles;

private:
	[[= ks::reflect::Net{} ]] bool m_bClientPhysics;
};

struct ClientModelRenderInfo_t;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPhysicsProp : public CBreakableProp
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysicsProp" } ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

	typedef CBreakableProp BaseClass;
public:
	DECLARE_CLIENTCLASS();

	// Inherited from IClientUnknown
public:
	virtual IClientModelRenderable*	GetClientModelRenderable();

	// Inherited from IClientModelRenderable
public:
	virtual bool GetRenderData( void *pData, ModelDataCategory_t nCategory );

	// Other public methods
public:
	CPhysicsProp();
	virtual ~CPhysicsProp();

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	bool GetPropDataAngles( const char *pKeyName, QAngle &vecAngles );
	float GetCarryDistanceOffset( void );
	void OnPreDataChanged( DataUpdateType_t updateType );
protected:
	// Networked vars.
	[[= ks::reflect::Net{} ]] bool m_bAwake;
	bool m_bAwakeLastTime;
	bool m_bCanUseStaticLighting;

	Vector m_vecClientOrigin;
	QAngle m_vecClientAngles;
};

class CBaseAnimating;
class CPhysicsProp;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CStatueProp : public CPhysicsProp
{
	[[= ks::reflect::NetTable{ .name = "DT_StatueProp" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CStatueProp, CPhysicsProp );
	DECLARE_CLIENTCLASS();

	CStatueProp();
	virtual			~CStatueProp();

	virtual void Spawn( void );

	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:

	[[= ks::reflect::Net{} ]] CHandle<CBaseAnimating>		m_hInitBaseAnimating;

	[[= ks::reflect::Net{} ]] bool	m_bShatter;
	[[= ks::reflect::Net{} ]] int		m_nShatterFlags;
	[[= ks::reflect::Net{} ]] Vector	m_vShatterPosition;
	[[= ks::reflect::Net{} ]] Vector	m_vShatterForce;
};

class CBaseEntity;
class IGameSystem;


typedef CHandle<CBaseEntity> EHANDLE;

class CSoundPatch;

extern IPhysicsObject		*g_PhysWorldObject;
extern IPhysics				*physics;
extern IPhysicsCollision	*physcollision;
extern IPhysicsEnvironment	*physenv;
extern IPhysicsSurfaceProps *physprops;
extern IPhysicsObjectPairHash *g_EntityCollisionHash;

extern const objectparams_t g_PhysDefaultObjectParams;

// Compute enough energy of a reference mass travelling at speed
// makes numbers more intuitive
template <typename T1, typename T2> [[gnu::always_inline]] inline constexpr auto MASS_SPEED2ENERGY( T1 mass, T2 speed ) { return ((speed)*(speed)*(mass)); }

// energy of a 10kg mass moving at speed
template <typename T1> [[gnu::always_inline]] inline auto MASS10_SPEED2ENERGY( T1 speed ) { return MASS_SPEED2ENERGY(10,speed); }

template <typename T1, typename T2> [[gnu::always_inline]] inline auto MASS_ENERGY2SPEED( T1 mass, T2 energy ) { return (FastSqrt((energy)/mass)); }

inline constexpr auto ENERGY_VOLUME_SCALE = (1.0f / 15500.0f);

inline constexpr auto FLUID_TIME_MAX = 2.0f; // keep track of last time hitting fluid for up to 2 seconds

//-----------------------------------------------------------------------------
// Purpose: A little cache of current objects making noises
//-----------------------------------------------------------------------------
struct friction_t
{
	CSoundPatch	*patch;
	CBaseEntity	*pObject;
	float		flLastUpdateTime;
	float		flLastEffectTime;
};

enum
{
	TOUCH_START=0,
	TOUCH_END,
};

struct touchevent_t
{
	CBaseEntity *pEntity0;
	CBaseEntity *pEntity1;
	int			touchType;
	Vector		endPoint;	//sv
	Vector		normal;		//sv
};

struct fluidevent_t
{
	EHANDLE			hEntity;
	float			impactTime;
};

void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit );
void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, const char *pSoundName, HSOUNDSCRIPTHASH& handle, float flVolume );
void PhysCleanupFrictionSounds( CBaseEntity *pEntity );
void PhysFrictionEffect( Vector &vecPos, Vector vecVel, float energy, int surfaceProps, int surfacePropsHit );
float PhysGetEntityMass( CBaseEntity *pEntity );

// Convenience routine
// ORs gameFlags with the physics object's current game flags
inline unsigned short PhysSetGameFlags( IPhysicsObject *pPhys, unsigned short gameFlags )
{
	unsigned short flags = pPhys->GetGameFlags();
	flags |= gameFlags;
	pPhys->SetGameFlags( flags );
	
	return flags;
}
// mask off gameFlags
inline unsigned short PhysClearGameFlags( IPhysicsObject *pPhys, unsigned short gameFlags )
{
	unsigned short flags = pPhys->GetGameFlags();
	flags &= ~gameFlags;
	pPhys->SetGameFlags( flags );
	
	return flags;
}

// Create a vphysics object based on a model
IPhysicsObject *PhysModelCreate( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles, solid_t *pSolid = nullptr );

IPhysicsObject *PhysModelCreateBox( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, bool isStatic );
IPhysicsObject *PhysModelCreateOBB( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, const QAngle &angle, bool isStatic );

// Create a vphysics object based on a BSP model (unmoveable)
IPhysicsObject *PhysModelCreateUnmoveable( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles );

// Create a vphysics object based on an existing collision model
IPhysicsObject *PhysModelCreateCustom( CBaseEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *pName, bool isStatic, solid_t *pSolid = nullptr );

// Create a bbox collision model (these may be shared among entities, they are auto-deleted at end of level. do not manage)
CPhysCollide *PhysCreateBbox( const Vector &mins, const Vector &maxs );

// Create a vphysics sphere object
IPhysicsObject *PhysSphereCreate( CBaseEntity *pEntity, float radius, const Vector &origin, solid_t &solid );

// Destroy a physics object created using PhysModelCreate...()
void PhysDestroyObject( IPhysicsObject *pObject, CBaseEntity *pEntity = nullptr );

void PhysDisableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysDisableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysDisableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
void PhysEnableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysEnableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysEnableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
bool PhysEntityCollisionsAreDisabled( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );

// create the world physics objects
IPhysicsObject *PhysCreateWorld_Shared( CBaseEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams );

// parse the parameters for a single solid from the model's collision data
bool PhysModelParseSolid( solid_t &solid, CBaseEntity *pEntity, int modelIndex );
// parse the parameters for a solid matching a particular index
bool PhysModelParseSolidByIndex( solid_t &solid, CBaseEntity *pEntity, int modelIndex, int solidIndex );

void PhysParseSurfaceData( class IPhysicsSurfaceProps *pProps, class IFileSystem *pFileSystem );

// fill out this solid_t with the AABB defaults (high inertia/no rotation)
void PhysGetDefaultAABBSolid( solid_t &solid );

// Compute an output velocity based on sliding along the current contact points 
// in the closest direction toward inputVelocity.
void PhysComputeSlideDirection( IPhysicsObject *pPhysics, const Vector &inputVelocity, const AngularImpulse &inputAngularVelocity, 
							   Vector *pOutputVelocity, Vector *pOutputAngularVelocity, float minMass );

void PhysForceClearVelocity( IPhysicsObject *pPhys );
bool PhysHasContactWithOtherInDirection( IPhysicsObject *pPhysics, const Vector &dir );

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* PhysicsGameSystem();

extern CSoftbodyEnvironment g_SoftbodyEnvironment; // in physics_softbody.cpp
extern ConVar softbody_debug; // in studio.cpp

class CSoftbodyProcess : public CAutoGameSystemPerFrame
{
public:
	CSoftbodyProcess() : CAutoGameSystemPerFrame( "SoftbodyProcess" ) {}
	virtual void OnRestore() override;
	virtual void Update( float frametime ) override;

	virtual void PostRender() override;
};

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDynamicProp : public CBreakableProp
{
	[[= ks::reflect::NetTable{ .name = "DT_DynamicProp" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CDynamicProp, CBreakableProp );
public:
	DECLARE_NETWORKCLASS();

	// constructor, destructor
	CDynamicProp( void );
	~CDynamicProp( void );

	virtual void	ClientThink( void );
	virtual void OnDataChanged( DataUpdateType_t type );

	void GetRenderBounds( Vector& theMins, Vector& theMaxs );
	unsigned int ComputeClientSideAnimationFlags();
	bool TestBoneFollowers( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	bool TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	void ForceTurnOffGlow( void );
	void UpdateGlow( void );
	CGlowObject m_GlowObject;

private:
	CDynamicProp( const CDynamicProp & );

	[[= ks::reflect::Net{} ]] bool	m_bUseHitboxesForRenderBox;
	int		m_iCachedFrameCount;
	Vector	m_vecCachedRenderMins;
	Vector	m_vecCachedRenderMaxs;

	[[= ks::reflect::Net{} ]] float		m_flGlowMaxDist;
	[[= ks::reflect::Net{} ]] bool		m_bShouldGlow;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32		m_clrGlow;
	[[= ks::reflect::Net{} ]] int			m_nGlowStyle;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBasePropDoor : public CDynamicProp
{
	[[= ks::reflect::NetTable{ .name = "DT_BasePropDoor" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBasePropDoor, CDynamicProp );
public:
	DECLARE_CLIENTCLASS();

	// constructor, destructor
	CBasePropDoor( void );
	virtual ~CBasePropDoor( void );

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void PostDataUpdate( DataUpdateType_t updateType );

	virtual bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );

private:
	CBasePropDoor( const CBasePropDoor & );
	bool m_modelChanged;
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
inline constexpr auto PROP_EXPLOSION_IGNITE_RADIUS = 32.0f;

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


// Purpose: A base class for all material proxies in the client dll


//-----------------------------------------------------------------------------
// Base class all material proxies should inherit from
//-----------------------------------------------------------------------------
class CEntityMaterialProxy : public IMaterialProxy
{
public:
	virtual void Release( void );
	void OnBind( void *pC_BaseEntity );

protected:
	// base classes should implement these
	virtual void OnBind( CBaseEntity *pBaseEntity ) = 0;
};

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

#define RAGDOLL_VISUALIZE	0


class IRagdoll
{
public:
	virtual ~IRagdoll() {}

	virtual void RagdollBone( CBaseEntity *ent, const mstudiobone_t *pbones, int boneCount, bool *boneSimulated, CBoneAccessor &pBoneToWorld ) = 0;
	virtual const Vector& GetRagdollOrigin( ) = 0;
	virtual void GetRagdollBounds( Vector &mins, Vector &maxs ) = 0;
	virtual int RagdollBoneCount() const = 0;
	virtual IPhysicsObject *GetElement( int elementNum ) = 0;
	virtual void DrawWireframe( void ) = 0;
	virtual void VPhysicsUpdate( IPhysicsObject *pObject ) = 0;
	virtual bool TransformVectorToWorld( int boneIndex, const Vector *vTemp, Vector *vOut ) = 0;
};

class CRagdoll : public IRagdoll
{
public:
	CRagdoll();
	~CRagdoll( void );

	DECLARE_SIMPLE_DATADESC();
	
	void Init( 
		CBaseEntity *ent, 
		CStudioHdr *pstudiohdr, 
		const Vector &forceVector, 
		int forceBone, 
		const matrix3x4_t *pDeltaBones0, 
		const matrix3x4_t *pDeltaBones1, 
		const matrix3x4_t *pCurrentBonePosition, 
		float boneDt,
		bool bFixedConstraints=false,
		bool bBleedOutOnSleep=true);

	virtual void RagdollBone( CBaseEntity *ent, const mstudiobone_t *pbones, int boneCount, bool *boneSimulated, CBoneAccessor &pBoneToWorld );
	virtual const Vector& GetRagdollOrigin( );
	virtual void GetRagdollBounds( Vector &theMins, Vector &theMaxs );
	void	BuildRagdollBounds( CBaseEntity *ent );
	
	virtual IPhysicsObject *GetElement( int elementNum );
	virtual IPhysicsConstraintGroup *GetConstraintGroup() { return m_ragdoll.pGroup; }
	virtual void DrawWireframe();
	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual int RagdollBoneCount() const { return m_ragdoll.listCount; }
	virtual bool TransformVectorToWorld( int iBoneIndex, const Vector *vTemp, Vector *vOut );
	
	void	SetInitialBonePosition( CStudioHdr *pstudiohdr, const CBoneAccessor &pDesiredBonePosition );

	bool IsValid() { return m_ragdoll.listCount > 0; }
	bool IsAsleep( void ) const { return m_allAsleep; }

	void ResetRagdollSleepAfterTime( void );
	float GetLastVPhysicsUpdateTime() const { return m_lastUpdate; }
	IPhysicsObject *RagdollPhysicsObject( int i ) { return m_ragdoll.list[ i ].pObject; }

public:

	void			CheckSettleStationaryRagdoll();
	void			PhysForceRagdollToSleep();
	void			CreateBloodPool();

private:

	ragdoll_t	m_ragdoll;
	Vector		m_mins, m_maxs;
	Vector		m_origin;
	float		m_lastUpdate;
	bool		m_allAsleep;
	Vector		m_vecLastOrigin;
	float		m_flLastOriginChangeTime;
	float		m_flAwakeTime;
	bool		m_doBleedOut;

#if RAGDOLL_VISUALIZE
	matrix3x4_t			m_savedBone1[MAXSTUDIOBONES];
	matrix3x4_t			m_savedBone2[MAXSTUDIOBONES];
	matrix3x4_t			m_savedBone3[MAXSTUDIOBONES];
#endif

public:
	
	ragdoll_t *GetRagdoll( void ){ return &m_ragdoll; }
};

CRagdoll *CreateRagdoll( 
	CBaseEntity *ent, 
	CStudioHdr *pstudiohdr, 
	const Vector &forceVector, 
	int forceBone, 
	const matrix3x4_t *pDeltaBones0, 
	const matrix3x4_t *pDeltaBones1, 
	const matrix3x4_t *pCurrentBonePosition, 
	float boneDt,
	bool bFixedConstraints=false,
	bool bBleedOutOnSleep=true);

// save this ragdoll's creation as the current tick
void NoteRagdollCreationTick( CBaseEntity *pRagdoll );
// returns true if the ragdoll was created on this tick
bool WasRagdollCreatedOnCurrentTick( CBaseEntity *pRagdoll );

#undef RAGDOLL_VISUALIZE

class CBaseEntity;


//Enumator class for ragdolls being affected by explosive forces
class CRagdollExplosionEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_GAMEROOT( CRagdollExplosionEnumerator, IPartitionEnumerator );
public:
	//Forced constructor
	CRagdollExplosionEnumerator( Vector origin, float radius, float magnitude );
	~CRagdollExplosionEnumerator();

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

public:
	//Data members
	std::vector< CBaseEntity* > m_Entities;
	Vector	m_vecOrigin;
	float	m_flMagnitude;
	float	m_flRadius;
};

class CBaseEntity;
class CBasePlayer;
class CTeam;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CRecipientFilter : public IRecipientFilter
{
public:
					CRecipientFilter();
	virtual			~CRecipientFilter();

	virtual bool	IsReliable( void ) const;

	virtual int		GetRecipientCount( void ) const;
	virtual int		GetRecipientIndex( int slot ) const;

	virtual bool	IsInitMessage( void ) const { return false; };

public:

	void			CopyFrom( const CRecipientFilter& src );

	void			Reset( void );

	void			MakeReliable( void );
		
	void			AddAllPlayers( void );
	void			AddRecipientsByPVS( const Vector& origin );
	void			AddRecipientsByPAS( const Vector& origin );
	void			AddRecipient( CBasePlayer *player );
	void			RemoveRecipient( CBasePlayer *player );
	void			AddRecipientsByTeam( CTeam *team );
	void			RemoveRecipientsByTeam( CTeam *team );

	void			UsePredictionRules( void );
	bool			IsUsingPredictionRules( void ) const;

	bool			IgnorePredictionCull( void ) const;
	void			SetIgnorePredictionCull( bool ignore );

	void			AddPlayersFromBitMask( CPlayerBitVec& playerbits );

	void			RemoveSplitScreenPlayers();
	void			ReplaceSplitScreenPlayersWithOwners();

	void			RemoveDuplicateRecipients();

private:

	bool				m_bReliable;
	bool				m_bInitMessage;
	std::vector< int >	m_Recipients;
	// If using prediction rules, the filter itself suppresses local player
	bool				m_bUsingPredictionRules;
	// If ignoring prediction cull, then external systems can determine
	//  whether this is a special case where culling should not occur
	bool				m_bIgnorePredictionCull;
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player
//-----------------------------------------------------------------------------
class CSingleUserRecipientFilter : public CRecipientFilter
{
public:
	explicit CSingleUserRecipientFilter( CBasePlayer *player )
	{
		AddRecipient( player );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players, unreliable
//-----------------------------------------------------------------------------
class CBroadcastRecipientFilter : public CRecipientFilter
{
public:
	CBroadcastRecipientFilter( void )
	{
		AddAllPlayers();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players, reliable
//-----------------------------------------------------------------------------
class CReliableBroadcastRecipientFilter : public CBroadcastRecipientFilter
{
public:
	CReliableBroadcastRecipientFilter( void )
	{
		MakeReliable();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player
//-----------------------------------------------------------------------------
class CPASFilter : public CRecipientFilter
{
public:
	explicit CPASFilter( const Vector& origin )
	{
		AddRecipientsByPAS( origin );
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPASAttenuationFilter : public CPASFilter
{
public:
	CPASAttenuationFilter( CBaseEntity *entity, float attenuation = ATTN_NORM ) :
		CPASFilter( entity->GetAbsOrigin() )
	{
	}

	CPASAttenuationFilter( const Vector& origin, float attenuation = ATTN_NORM ) :
		CPASFilter( origin )
	{
	}

	CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound ) :
		CPASFilter( entity->GetAbsOrigin() )
	{
	}

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound ) :
		CPASFilter( origin )
	{
	}

	CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound, HSOUNDSCRIPTHASH& handle ) :
		CPASFilter( entity->GetAbsOrigin() )
	{
	}

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound, HSOUNDSCRIPTHASH& handle ) :
		CPASFilter( origin )
	{
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player
//-----------------------------------------------------------------------------
class CPVSFilter : public CRecipientFilter
{
public:
	explicit CPVSFilter( const Vector& origin )
	{
		AddRecipientsByPVS( origin );
	}
};

class CLocalPlayerFilter : public CRecipientFilter
{
public:
	CLocalPlayerFilter( void );
};

class CBaseEntity;

using namespace ::ks::reflect;

namespace physicssound
{
	struct impactsound_t
	{
		void			*pGameData;
		int				entityIndex;
		int				soundChannel;
		float			volume;
		float			impactSpeed;
		unsigned short	surfaceProps;
		unsigned short	surfacePropsHit;
		Vector			origin;
	};

	// UNDONE: Use a sorted container and sort by volume/distance?
	struct soundlist_t
	{
		std::vector< impactsound_t >	elements;
		impactsound_t	&GetElement(int index) { return elements[index]; }
		impactsound_t	&AddElement() { return elements.emplace_back(); }
		int Count() { return std::ssize( elements ); }
		void RemoveAll() { elements.clear(); }
	};

	void PlayImpactSounds( soundlist_t &list )
	{
		for ( int i = list.Count()-1; i >= 0; --i )
		{
			impactsound_t &sound = list.GetElement(i);
			const surfacedata_t *psurf = physprops->GetSurfaceData( sound.surfaceProps );
			if ( psurf->sounds.impactHard )
			{
				const surfacedata_t *pHit = physprops->GetSurfaceData( sound.surfacePropsHit );
				unsigned short soundName = psurf->sounds.impactHard;
				if ( pHit && psurf->sounds.impactSoft )
				{
					if ( pHit->audio.hardnessFactor < psurf->audio.hardThreshold || 
						(psurf->audio.hardVelocityThreshold > 0 && psurf->audio.hardVelocityThreshold > sound.impactSpeed) )
					{
						soundName = psurf->sounds.impactSoft;
					}
				}
				const char *pSound = physprops->GetString( soundName );

				CSoundParameters params;
				if ( !CBaseEntity::GetParametersForSound( pSound, params, nullptr ) )
					break;

				if ( sound.volume > 1 )
					sound.volume = 1;
				CBroadcastRecipientFilter filter;
				// JAY: If this entity gets deleted, the sound comes out at the world origin
				// this sounds bad!  Play on ent 0 for now.
				EmitSound_t ep;
				ep.m_nChannel = sound.soundChannel;
				ep.m_pSoundName = params.soundname;
				ep.m_flVolume = params.volume * sound.volume;
				ep.m_SoundLevel = params.soundlevel;
				ep.m_nPitch = params.pitch;
				ep.m_pOrigin = &sound.origin;
				ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
				ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

				CBaseEntity::EmitSound( filter, 0 /*sound.entityIndex*/, ep );
			}
		}
		list.RemoveAll();
	}
	void AddImpactSound( soundlist_t &list, void *pGameData, int entityIndex, int soundChannel, IPhysicsObject *pObject, int surfaceProps, int surfacePropsHit, float volume, float impactSpeed )
	{
		impactSpeed += 1e-4;
		for ( int i = list.Count()-1; i >= 0; --i )
		{
			impactsound_t &sound = list.GetElement(i);
			// UNDONE: Compare entity or channel somehow?
			// UNDONE: Doing one slot per entity is too noisy.  So now we use one slot per material

			// heuristic - after 4 impacts sounds in one frame, start merging everything
			if ( surfaceProps == sound.surfaceProps || list.Count() > 4 )
			{
				// UNDONE: Store instance volume separate from aggregate volume and compare that?
				if ( volume > sound.volume )
				{
					pObject->GetPosition( &sound.origin, nullptr );
					sound.pGameData = pGameData;
					sound.entityIndex = entityIndex;
					sound.soundChannel = soundChannel;
					sound.surfacePropsHit = surfacePropsHit;
				}
				sound.volume += volume;
				sound.impactSpeed = std::max(impactSpeed,sound.impactSpeed);
				return;
			}
		}

		impactsound_t &sound = list.AddElement();
		sound.pGameData = pGameData;
		sound.entityIndex = entityIndex;
		sound.soundChannel = soundChannel;
		pObject->GetPosition( &sound.origin, nullptr );
		sound.surfaceProps = surfaceProps;
		sound.surfacePropsHit = surfacePropsHit;
		sound.volume = volume;
		sound.impactSpeed = impactSpeed;
	}

	struct breaksound_t
	{
		Vector			origin;
		int				surfacePropsBreak;
	};

	void AddBreakSound( std::vector< breaksound_t > &list, const Vector &origin, unsigned short surfaceProps )
	{
		const surfacedata_t *psurf = physprops->GetSurfaceData( surfaceProps );
		if ( !psurf->sounds.breakSound )
			return;

		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			breaksound_t &sound = list[ i ];
			// Allow 3 break sounds before you start merging anything.
			if ( std::ssize( list ) > 2 && surfaceProps == sound.surfacePropsBreak )
			{
				sound.origin = (sound.origin + origin) * 0.5f;
				return;
			}
		}
		breaksound_t sound;
		sound.origin = origin;
		sound.surfacePropsBreak = surfaceProps;
		list.push_back( sound );

	}

	void PlayBreakSounds( std::vector< breaksound_t > &list )
	{
		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			breaksound_t &sound = list[ i ];

			const surfacedata_t *psurf = physprops->GetSurfaceData( sound.surfacePropsBreak );
			const char *pSound = physprops->GetString( psurf->sounds.breakSound );
			CSoundParameters params;
			if ( !CBaseEntity::GetParametersForSound( pSound, params, nullptr ) )
				return;

			// Play from the world, because the entity is breaking, so it'll be destroyed soon
			CBroadcastRecipientFilter filter;
			EmitSound_t ep;
			ep.m_nChannel = CHAN_STATIC;
			ep.m_pSoundName = params.soundname;
			ep.m_flVolume = params.volume;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = params.pitch;
			ep.m_pOrigin = &sound.origin;
			ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
			ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

			CBaseEntity::EmitSound( filter, 0 /*sound.entityIndex*/, ep );
		}
		list.clear();
	}
};

extern IScriptVM * g_pScriptVM;

// Only allow scripts to create entities during map initialization
bool IsEntityCreationAllowedInScripts( void );

// The parts of the script interface that are bound to one game side: GetScriptDesc<T> is specialised
// for the side's classes and CScriptScope runs on the side's VM (g_pScriptVM). They are not part
// of ivscript.h, which is shared with the engine.

struct ScriptNoBase_t;

// We use template specialization to allow classes to optionally override this function.
// For a given class, if this function is NOT overridden, the class' descriptor will use the base class'
// IScriptInstanceHelper object.
// If this function IS overridden, it will use the return value of the overridden function
template < typename TScriptClass > 
IScriptInstanceHelper *GetScriptInstanceHelperOverride( IScriptInstanceHelper *pBaseClassHelper )
{
	return pBaseClassHelper;
}

inline IScriptInstanceHelper *GetScriptInstanceHelper_ScriptNoBase_t()
{
	return nullptr;
}

// A root class has no base helper: the global GetScriptInstanceHelper_ScriptNoBase_t is not
// redeclared, which a module could not do.

#define DEFINE_SCRIPT_CONSTRUCTOR()															ScriptAddConstructorToClassDesc( pDesc, _className );
#define DEFINE_SCRIPT_INSTANCE_HELPER( className, p )										template <> IScriptInstanceHelper *GetScriptInstanceHelperOverride< className >( IScriptInstanceHelper * ) { return p; }
								
template <typename T> ScriptClassDesc_t *GetScriptDesc(T *);

template <>
inline ScriptClassDesc_t *GetScriptDesc<ScriptNoBase_t>( ScriptNoBase_t *) { return nullptr; }

//-----------------------------------------------------------------------------
// Script scope helper class
//-----------------------------------------------------------------------------

extern IScriptVM *g_pScriptVM;
class CDefScriptScopeBase
{
public:
	static IScriptVM *GetVM()
	{
		return g_pScriptVM;
	}
};

template <class BASE_CLASS = CDefScriptScopeBase>
class CScriptScopeT : public CDefScriptScopeBase
{
public:
	CScriptScopeT() :
		m_hScope( INVALID_HSCRIPT ),
		m_flags( 0 )
	{
	}

	~CScriptScopeT()
	{
		Term();
	}

	bool IsInitialized()
	{
		return m_hScope != INVALID_HSCRIPT;
	}

	bool Init( const char *pszName )
	{
		m_hScope = GetVM()->CreateScope( pszName );
		return ( m_hScope != nullptr );
	}

	bool Init( HSCRIPT hScope, bool bExternal = true )
	{
		if ( bExternal )
		{
			m_flags |= EXTERNAL;
		}
		m_hScope = hScope;
		return ( m_hScope != nullptr );
	}

	bool InitGlobal()
	{
		contract_assert( 0 ); // todo [3/24/2008 tom]
		m_hScope = GetVM()->CreateScope( "" );
		return ( m_hScope != nullptr );
	}

	void Term()
	{
		if ( m_hScope != INVALID_HSCRIPT )
		{
			IScriptVM *pVM = GetVM();
			if ( pVM )
			{
				for ( HSCRIPT *pFunc : m_FuncHandles )
					pVM->ReleaseFunction( *pFunc );
			}
			m_FuncHandles.clear();
			if ( m_hScope && pVM && !(m_flags & EXTERNAL) )
			{
				pVM->ReleaseScope( m_hScope );
			}
			m_hScope = INVALID_HSCRIPT;
		}
		m_flags = 0;
	}

	void InvalidateCachedValues()
	{
		IScriptVM *pVM = GetVM();
		for ( HSCRIPT *pFunc : m_FuncHandles )
		{
			if ( *pFunc )
				pVM->ReleaseFunction( *pFunc );
			*pFunc = INVALID_HSCRIPT;
		}
		m_FuncHandles.clear();
	}

	operator HSCRIPT()
	{
		return ( m_hScope != INVALID_HSCRIPT ) ? m_hScope : nullptr;
	}

	bool ValueExists( const char *pszKey )																							{ return GetVM()->ValueExists( m_hScope, pszKey ); }
	bool SetValue( const char *pszKey, const ScriptVariant_t &value )																{ return GetVM()->SetValue(m_hScope, pszKey, value ); }
	bool GetValue( const char *pszKey, ScriptVariant_t *pValue )																	{ return GetVM()->GetValue(m_hScope, pszKey, pValue ); }
	void ReleaseValue( ScriptVariant_t &value )																						{ GetVM()->ReleaseValue( value ); }
	bool ClearValue( const char *pszKey)																							{ return GetVM()->ClearValue( m_hScope, pszKey ); }

	ScriptStatus_t Run( HSCRIPT hScript )
	{
		InvalidateCachedValues();
		return GetVM()->Run( hScript, m_hScope );
	}

	ScriptStatus_t Run( const char *pszScriptText, const char *pszScriptName = nullptr )
	{
		InvalidateCachedValues();
		HSCRIPT hScript = GetVM()->CompileScript( pszScriptText, pszScriptName );
		if ( hScript )
		{
			ScriptStatus_t result = GetVM()->Run( hScript, m_hScope );
			GetVM()->ReleaseScript( hScript );
			return result; 
		}
		return SCRIPT_ERROR;
	}

	ScriptStatus_t Run( const unsigned char *pszScriptText, const char *pszScriptName = nullptr )
	{
		return Run( (const char *)pszScriptText, pszScriptName);
	}

	HSCRIPT LookupFunction( const char *pszFunction )
	{
		return GetVM()->LookupFunction( pszFunction, m_hScope );
	}

	void ReleaseFunction( HSCRIPT hScript )
	{
		GetVM()->ReleaseFunction( hScript );
	}

	bool FunctionExists( const char *pszFunction )
	{
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		GetVM()->ReleaseFunction( hFunction );
		return ( hFunction != nullptr ) ;
	}

	//-----------------------------------------------------

	enum Flags_t
	{
		EXTERNAL = 0x01,
	};

	//-----------------------------------------------------

	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn = nullptr )
	{
		return GetVM()->ExecuteFunction( hFunction, nullptr, 0, pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1 )
	{
		ScriptVariant_t args[1]; args[0] = arg1;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2 )
	{
		ScriptVariant_t args[2]; args[0] = arg1; args[1] = arg2;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3 )
	{
		ScriptVariant_t args[3]; args[0] = arg1; args[1] = arg2; args[2] = arg3;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4 )
	{
		ScriptVariant_t args[4]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5 )
	{
		ScriptVariant_t args[5]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6 )
	{
		ScriptVariant_t args[6]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7 )
	{
		ScriptVariant_t args[7]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8 )
	{
		ScriptVariant_t args[8]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9 )
	{
		ScriptVariant_t args[9]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10 )
	{
		ScriptVariant_t args[10]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11 )
	{
		ScriptVariant_t args[11]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12 )
	{
		ScriptVariant_t args[12]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13 )
	{
		ScriptVariant_t args[13]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13;
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13, ARG_TYPE_14 arg14 )
	{
		ScriptVariant_t args[14]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13; args[13] = arg14; 
		return GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
	}

	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn = nullptr )
	{
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, nullptr, 0, pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1 )
	{
		ScriptVariant_t args[1]; args[0] = arg1;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2 )
	{
		ScriptVariant_t args[2]; args[0] = arg1; args[1] = arg2;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3 )
	{
		ScriptVariant_t args[3]; args[0] = arg1; args[1] = arg2; args[2] = arg3;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4 )
	{
		ScriptVariant_t args[4]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5 )
	{
		ScriptVariant_t args[5]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6 )
	{
		ScriptVariant_t args[6]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7 )
	{
		ScriptVariant_t args[7]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8 )
	{
		ScriptVariant_t args[8]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9 )
	{
		ScriptVariant_t args[9]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10 )
	{
		ScriptVariant_t args[10]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11 )
	{
		ScriptVariant_t args[11]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12 )
	{
		ScriptVariant_t args[12]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13 )
	{
		ScriptVariant_t args[13]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13;
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

	template <typename ARG_TYPE_1, typename ARG_TYPE_2, typename	ARG_TYPE_3,	typename ARG_TYPE_4, typename ARG_TYPE_5, typename ARG_TYPE_6, typename ARG_TYPE_7, typename ARG_TYPE_8, typename ARG_TYPE_9, typename ARG_TYPE_10, typename ARG_TYPE_11, typename ARG_TYPE_12, typename ARG_TYPE_13, typename ARG_TYPE_14>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, ARG_TYPE_1 arg1, ARG_TYPE_2 arg2, ARG_TYPE_3 arg3, ARG_TYPE_4 arg4, ARG_TYPE_5 arg5, ARG_TYPE_6 arg6, ARG_TYPE_7 arg7, ARG_TYPE_8 arg8, ARG_TYPE_9 arg9, ARG_TYPE_10 arg10, ARG_TYPE_11 arg11, ARG_TYPE_12 arg12, ARG_TYPE_13 arg13, ARG_TYPE_14 arg14 )
	{
		ScriptVariant_t args[14]; args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4; args[4] = arg5; args[5] = arg6; args[6] = arg7; args[7] = arg8; args[8] = arg9; args[9] = arg10; args[10] = arg11; args[11] = arg12; args[12] = arg13; args[13] = arg14; 
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, args, std::size( args ), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

protected:
	HSCRIPT m_hScope;
	int m_flags;
	std::vector<HSCRIPT *> m_FuncHandles;
};

typedef CScriptScopeT<> CScriptScope;

#define VScriptAddEnumToScope_( scope, enumVal, scriptName )	(scope).SetValue( scriptName, (int)enumVal )
#define VScriptAddEnumToScope( scope, enumVal )					VScriptAddEnumToScope_( scope, enumVal, #enumVal )

#define VScriptAddEnumToRoot( enumVal )					g_pScriptVM->SetValue( #enumVal, (int)enumVal )

#undef DEFINE_SCRIPT_CONSTRUCTOR
#undef DEFINE_SCRIPT_INSTANCE_HELPER
#undef VScriptAddEnumToScope_
#undef VScriptAddEnumToScope
#undef VScriptAddEnumToRoot

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

using namespace ::ks::reflect;

// This is an ugly hack to link client classes to weapons for now
//  these will be removed once we predict all weapons, especially TF2 weapons
// STUB_WEAPON_CLASS builds its table by reflection

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

extern ConVar hl2_episodic;

// Simple shared header file for common base entities

// How many bits are used to transmit parent attachment indices?
inline constexpr auto NUM_PARENTATTACHMENT_BITS = 6;

// Maximum number of vphysics objects per entity
inline constexpr auto VPHYSICS_MAX_OBJECT_LIST_COUNT = 1024;

inline constexpr auto DEFAULT_LOOK_AT_USE_ANGLE = 0.8f;

// CBaseEntity inlines
bool CBaseEntity::IsPlayerSimulated( void ) const
{
	return m_bIsPlayerSimulated;
}

inline HSCRIPT ToHScript( CBaseEntity *pEnt )
{
	return ( pEnt ) ? pEnt->GetScriptInstance() : nullptr;
}

template <> ScriptClassDesc_t *GetScriptDesc<CBaseEntity>( CBaseEntity * );
CBaseEntity *ToEnt( HSCRIPT hScript )
{

	return ( hScript ) ? (CBaseEntity *)g_pScriptVM->GetInstanceValue( hScript, GetScriptDescForClass(CBaseEntity) ) : nullptr;
}

// Shared EntityMessage between game and client .dlls
inline constexpr auto BASEENTITY_MSG_REMOVE_DECALS = 1;

inline bool IsPushableMoveType( int nMoveType )
{
	if ( nMoveType == MOVETYPE_PUSH || nMoveType == MOVETYPE_NONE || 
		nMoveType == MOVETYPE_VPHYSICS || nMoveType == MOVETYPE_NOCLIP )
		return false;
	return true;
}

extern float k_flMaxEntityPosCoord;
extern float k_flMaxEntityEulerAngle;
extern float k_flMaxEntitySpeed;
extern float k_flMaxEntitySpinRate;

inline bool IsEntityCoordinateReasonable( const vec_t c )
{
	float r = k_flMaxEntityPosCoord;
	return c > -r && c < r;
}

inline bool IsEntityPositionReasonable( const Vector &v )
{
	float r = k_flMaxEntityPosCoord;
	return
		v.x > -r && v.x < r &&
		v.y > -r && v.y < r &&
		v.z > -r && v.z < r;
}

// Returns:
//   -1 - velocity is really, REALLY bad and probably should be rejected.
//   0  - velocity was suspicious and clamped.
//   1  - velocity was OK and not modified
extern int CheckEntityVelocity( Vector &v );

inline bool IsEntityQAngleReasonable( const QAngle &q )
{
	float r = k_flMaxEntityEulerAngle;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Angular velocity in exponential map form
inline bool IsEntityAngularVelocityReasonable( const Vector &q )
{
	float r = k_flMaxEntitySpinRate;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Angular velocity of each Euler angle.
inline bool IsEntityQAngleVelReasonable( const QAngle &q )
{
	float r = k_flMaxEntitySpinRate;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Should we emit physics spew into the log or not?
extern bool CheckEmitReasonablePhysicsSpew();

Vector	CBaseEntity::Forward() const __restrict__  ///< get my forward (+x) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][0], mat[1][0], mat[2][0] );
}

Vector	CBaseEntity::Left() const __restrict__     ///< get my left    (+y) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][1], mat[1][1], mat[2][1] );
}

Vector	CBaseEntity::Up() const  __restrict__      ///< get my up      (+z) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][2], mat[1][2], mat[2][2] );
}

class CBaseEntity;

using namespace ::ks::reflect;

inline constexpr auto MAX_NUM_PANELS = 16;

inline constexpr auto NUM_EDGE_TYPES = 4;
inline constexpr auto NUM_EDGE_STYLES = 3;

extern IVDebugOverlay *debugoverlay;

enum WinSide_t
{
	WIN_SIDE_BOTTOM,
	WIN_SIDE_RIGHT,
	WIN_SIDE_TOP,
	WIN_SIDE_LEFT,
};

enum WinEdge_t
{
	EDGE_NOT = -1,		// No edge
	EDGE_NONE,				/* No edge on both sides	/##\  */
	EDGE_FULL,				// Edge on both sides		|##| 
	EDGE_LEFT,				/* Edge is on left only		|##\  */
	EDGE_RIGHT,				// Edge is on right only	/##|
};

//-----------------------------------------------------------------------------
// All the information associated with a particular handle
//-----------------------------------------------------------------------------
struct Panel_t
{
	char			m_nWidth;
	char			m_nHeight;
	char			m_nSide;
	char			m_nEdgeType;
	char			m_nStyle;
};

struct EdgeTexture_t
{
	int					m_nRenderIndex;
	int					m_nStyle;
	CMaterialReference	m_pMaterialEdge;
	CTextureReference	m_pMaterialEdgeTexture;
};

class CBreakableSurface : public CBaseEntity, public IBrushRenderer
{
	[[= ks::reflect::NetTable{ .name = "DT_BreakableSurface" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBreakableSurface, CBaseEntity );
	DECLARE_CLIENTCLASS();

	[[= ks::reflect::Net{} ]] int				m_nNumWide;
	[[= ks::reflect::Net{} ]] int				m_nNumHigh;
	[[= ks::reflect::Net{} ]] float			m_flPanelWidth;
	[[= ks::reflect::Net{} ]] float			m_flPanelHeight;
	[[= ks::reflect::Net{} ]] Vector			m_vNormal;
	[[= ks::reflect::Net{} ]] Vector			m_vCorner;
	[[= ks::reflect::Net{} ]] bool			m_bIsBroken;
	[[= ks::reflect::Net{} ]] int				m_nSurfaceType;

	// This is the texture we're going to use to multiply by the cracked base texture
	ITexture*	m_pCurrentDetailTexture;

	// Stores linked list of edges to render
	std::vector< Panel_t >		m_RenderList;

	CBreakableSurface();
	~CBreakableSurface();

public:
	void		InitMaterial(WinEdge_t nEdgeType, int nEdgeStyle, char const* pMaterialName);
	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		OnPreDataChanged( DataUpdateType_t updateType );

	RenderableTranslucencyType_t ComputeTranslucencyType( void );
	bool		HavePanel(int nWidth, int nHeight);
	bool		RenderBrushModelSurface( IClientEntity* pBaseEntity, IBrushSurface* pBrushSurface ); 
	int			DrawModel( int flags, const RenderableInstance_t &instance );
	void		DrawSolidBlocks( IBrushSurface* pBrushSurface );

	virtual void	OnRestore();

	virtual bool	ShouldReceiveProjectedTextures( int flags );

private:
	// One bit per pane
	CNetworkArray( bool, m_RawPanelBitVec, MAX_NUM_PANELS * MAX_NUM_PANELS, [[= ks::reflect::Net{} ]] );
	bool m_PrevRawPanelBitVec[ MAX_NUM_PANELS * MAX_NUM_PANELS ];
	
	// 2 bits of flags and 2 bits of edge type
	byte		m_nPanelBits[MAX_NUM_PANELS][MAX_NUM_PANELS];	//UNDONE: allocate this dynamically?
	CMaterialReference	m_pMaterialBox;	
	EdgeTexture_t	m_pSolid;
	EdgeTexture_t	m_pEdge[NUM_EDGE_TYPES][NUM_EDGE_STYLES];

	inline bool InLegalRange(int nWidth, int nHeight);
	inline bool	IsPanelSolid(int nWidth, int nHeight);
	inline bool	IsPanelStale(int nWidth, int nHeight);
	inline void	SetPanelSolid(int nWidth, int nHeight, bool value);
	inline void	SetPanelStale(int nWidth, int nHeight, bool value);

	void		DrawOneEdge( IBrushSurface* pBrushSurface, IMesh* pMesh, 
					CMeshBuilder *pMeshBuilder, const Vector &vStartPos,  
					const Vector &vWStep, const Vector &vHstep, WinSide_t nEdge);
	void		DrawOneHighlight( IBrushSurface* pBrushSurface, IMesh* pMesh, 
					CMeshBuilder *pMeshBuilder, const Vector &vStartPos,  
					const Vector &vWStep, const Vector &vHstep, WinSide_t nEdge);
	void		DrawOneBlock(IBrushSurface* pBrushSurface, IMesh* pMesh, 
					CMeshBuilder *pMeshBuilder, const Vector &vPosition, 
					const Vector &vWidth, const Vector &vHeight);

	void		DrawRenderList( IBrushSurface* pBrushSurface);
	void		DrawRenderListHighlights( IBrushSurface* pBrushSurface );
	int			FindRenderPanel(int nWidth, int nHeight, WinSide_t nSide);
	void		AddToRenderList(int nWidth, int nHeight, WinSide_t nSide, WinEdge_t nEdgeType, int forceStyle);
	int			FindFirstRenderTexture(WinEdge_t nEdgeType, int nStyle);

	inline void SetStyleType( int w, int h, int type )
	{
		contract_assert( type < NUM_EDGE_STYLES );
		contract_assert( type >= 0 );
		// Clear old value
		m_nPanelBits[ w ][ h ] &= ( ~0x03 << 2 );
		// Insert new value
		m_nPanelBits[ w ][ h ] |= ( type << 2 );
	}

	inline int GetStyleType( int w, int h )
	{
		int value = m_nPanelBits[ w ][ h ];
		value = ( value >> 2 ) & 0x03;
		contract_assert( value < NUM_EDGE_STYLES );
		return value;
	}

	// Gets at the cracked version of the material
	void FindCrackedMaterial();

	CMaterialReference	m_pCrackedMaterial;
	CTextureReference	m_pMaterialBoxTexture;

	void		UpdateEdgeType(int nWidth, int nHeight, int forceStyle = -1 );
};

class CBreakableSurfaceProxy : public CEntityMaterialProxy
{
	public:
	CBreakableSurfaceProxy();
	virtual ~CBreakableSurfaceProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( CBaseEntity *pC_BaseEntity );
	virtual IMaterial *GetMaterial();

	private:
	// get at the material whose texture we're going to steal
	void AcquireSourceMaterial( CBaseEntity* pEnt );

	IMaterialVar* m_BaseTextureVar;
};

class CBaseEntity;

using namespace ::ks::reflect;

class CFuncBrush : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncBrush" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncBrush, CBaseEntity );
	DECLARE_CLIENTCLASS();

	void OnDataChanged( DataUpdateType_t type );

private:
};

class CFunc_Dust;

class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
struct Particle;

using namespace ::ks::reflect;


// ------------------------------------------------------------------------------------ //
// CDustEffect particle renderer.
// ------------------------------------------------------------------------------------ //


class CFuncDustParticle : public Particle
{
public:
	Vector		m_vVelocity;
	float		m_flLifetime;
	float		m_flDieTime;
	float		m_flSize;
	color32		m_Color;
};

class CDustEffect : public CParticleEffect
{
public:
	explicit CDustEffect( const char *pDebugName ) : CParticleEffect( pDebugName ) {}

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	CFunc_Dust		*m_pDust;

private:
	CDustEffect( const CDustEffect & ); // not defined, not accessible
};

// ------------------------------------------------------------------------------------ //
// CFunc_Dust class.
// ------------------------------------------------------------------------------------ //

class CFunc_Dust : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Func_Dust", .base = false } ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Collision", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFunc_Dust, CBaseEntity );
	DECLARE_CLIENTCLASS();

						CFunc_Dust();
	virtual				~CFunc_Dust();
	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		ClientThink();
	virtual bool		ShouldDraw();

private:

	void				AttemptSpawnNewParticle();

// Vars from server.
public:

	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32			m_Color;
	[[= ks::reflect::Net{} ]] int				m_SpawnRate;
	
	[[= ks::reflect::Net{} ]] float			m_flSizeMin;
	[[= ks::reflect::Net{} ]] float			m_flSizeMax;

	[[= ks::reflect::Net{} ]] int				m_SpeedMax;

	[[= ks::reflect::Net{} ]] int				m_LifetimeMin;
	[[= ks::reflect::Net{} ]] int				m_LifetimeMax;

	[[= ks::reflect::Net{} ]] int				m_DistMax;

	[[= ks::reflect::Net{} ]] float			m_FallSpeed;	// extra 'gravity'
	[[= ks::reflect::Net{} ]] bool			m_bAffectedByWind;

public:

	[[= ks::reflect::Net{} ]] int				m_DustFlags;	// Combination of DUSTFLAGS_

public:
	CDustEffect		m_Effect;
	PMaterialHandle		m_hMaterial;
	TimedEvent			m_Spawner;

private:
	CFunc_Dust( const CFunc_Dust & ); // not defined, not accessible
};

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

class CInfoLadderDismount : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_InfoLadderDismount" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CInfoLadderDismount, CBaseEntity );
	DECLARE_NETWORKCLASS();

	virtual void DrawDebugGeometryOverlays();
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

	virtual void Spawn();

	virtual void DrawDebugGeometryOverlays(void);

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

	virtual void Activate();

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

};

const char *FuncLadder_GetSurfaceprops(CBaseEntity *pLadderEntity);

//-----------------------------------------------------------------------------
// Do we have reflective glass in view? If so, what's the reflection plane?
//-----------------------------------------------------------------------------
bool IsReflectiveGlassInView( const CViewSetup& view, cplane_t &plane );

class CBaseEntity;

// Purpose: These are a couple of base proxy classes to help us with
// getting/setting source/result material vars


//-----------------------------------------------------------------------------
// Helper class to deal with floating point inputs
//-----------------------------------------------------------------------------
class CFloatInput
{
public:
	bool  Init( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName, float flDefault = 0.0f );
	float GetFloat() const;

private:
	float m_flValue;
	IMaterialVar *m_pFloatVar;
	int	m_FloatVecComp;
};

//-----------------------------------------------------------------------------
// Result proxy; a result (with vector friendliness)
//-----------------------------------------------------------------------------
class CResultProxy : public IMaterialProxy
{
public:
	CResultProxy();
	virtual ~CResultProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

protected:
	CBaseEntity *BindArgToEntity( void *pArg );
	void SetFloatResult( float result );
	void SetVecResult( float x, float y, float z, float w );

	IMaterialVar* m_pResult;
	int m_ResultVecComp;
};

//-----------------------------------------------------------------------------
// Base functional proxy; two sources (one is optional) and a result
//-----------------------------------------------------------------------------
class CFunctionProxy : public CResultProxy
{
public:
	CFunctionProxy();
	virtual ~CFunctionProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );

protected:
	void ComputeResultType( MaterialVarType_t& resultType, int& vecSize );

	IMaterialVar* m_pSrc1;
	IMaterialVar* m_pSrc2;
};

class CBaseEntity;

using namespace ::ks::reflect;

class CPhysPropClientside : public CBreakableProp, public IBreakableWithPropData, public IMultiplayerPhysics
{
	
public:
	DECLARE_CLASS( CPhysPropClientside, CBreakableProp );
	
	CPhysPropClientside();
	virtual ~CPhysPropClientside();

			bool			Initialize();
			void			Spawn();
			int				ParsePropData( void );
	virtual bool			IsDormant( void ) { return false; } // we could add a PVS check here
	virtual void			ClientThink( void );
	virtual CollideType_t	GetCollideType( void ) { return ENTITY_SHOULD_RESPOND; }
	virtual void			StartTouch( CBaseEntity *pOther );
	virtual	void			HitSurface( CBaseEntity *pOther );
	virtual	void			ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	virtual	bool			IsClientCreated( void ) const { return true; }
	virtual int				GetMultiplayerPhysicsMode() { return m_iPhysicsMode; }
	virtual float			GetMass();
	virtual bool			IsAsleep();
	
	virtual bool			KeyValue( const char *szKeyName, const char *szValue );

	virtual	void			OnTakeDamage( int iDamage ); // very simple version
	virtual void			Break();
	virtual	void			Clone( Vector &velocity );
	virtual void			StartFadeOut( float fTime );
	virtual void			SetHealth(int iHealth) { m_iHealth = iHealth; }
	virtual int				GetHealth() const { return m_iHealth; }
			int				GetNumBreakableChunks( void ) { return m_iNumBreakableChunks; }
	
// IBreakableWithPropData interface:
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
	void			SetExplosiveDamage( float flDamage ) { m_explodeDamage = flDamage; }
	float			GetExplosiveRadius( void ) { return m_explodeRadius; }
	float			GetExplosiveDamage( void ) { return m_explodeDamage; }
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
	void			SetPropDataBlocksLOS( bool bBlocksLOS ) { m_bBlockLOSSetByPropData = true; SetBlocksLOS( bBlocksLOS ); }
	void			SetPropDataIsAIWalkable( bool b ) { m_bIsWalkableSetByPropData = true; SetAIWalkable( b ); }
	void			SetBasePropData( string_t iszBase ) { m_iszBasePropData = iszBase; }
	string_t		GetBasePropData( void ) { return m_iszBasePropData; }
	void			SetInteraction( propdata_interactions_t Interaction ) { m_iInteractions |= (1 << Interaction); }
	bool			HasInteraction( propdata_interactions_t Interaction ) { return ( m_iInteractions & (1 << Interaction) ) != 0; }
	void			SetPhysicsMode(int iMode);
	int				GetPhysicsMode() { return m_iPhysicsMode; }
	void			SetMultiplayerBreakMode( mp_break_t mode ) {}
	mp_break_t		GetMultiplayerBreakMode( void ) const { return MULTIPLAYER_BREAK_DEFAULT; }

// static management fucntions:
	static void RecreateAll(); // recreate all clientside props in map
	static void DestroyAll();  // clear all clientside created phys props
	static CPhysPropClientside *CreateNew(bool bForce = false);

protected:
	
	static void ParseAllEntities(const char *pMapData);
	static const char *ParseEntity( const char *pEntData );
		
public:
	
	int		m_iPhysicsMode;		// should always be PHYSICS_MULTIPLAYER_CLIENTSIDE
	float	m_flTouchDelta;		// Amount of time that must pass before another touch function can be called
	float 	m_fDeathTime;		// Point at which this object self destructs.  
								// The default of -1 indicates the object shouldn't destruct.

// properties from serverclass CPhysicsProp
	float		m_impactEnergyScale;
	int			m_spawnflags;
	float		m_inertiaScale;

protected:
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
	// Count of how many pieces we'll break into, custom or generic
	int				m_iNumBreakableChunks;

	float			m_explodeDamage;
	float			m_explodeRadius;
	bool			m_bBlockLOSSetByPropData;
	bool			m_bIsWalkableSetByPropData;
};
