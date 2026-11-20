// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "activitylist.h"
#include "studio.h"
#include "animation.h"
#include "client_class.h"
#include "reflect_annotations.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "bittools.h"
#include "datamap.h"
#include "worldsize.h"
#include "shareddefs.h"
#include "engine/IEngineSound.h"
#include "filesystem.h"
#include "declare_class.h"
#include "engine/IEngineTrace.h"
#include "iclientalphaproperty.h"
#include "icliententity.h"
#include "irecipientfilter.h"
#include "mouthinfo.h"
#include "networkvar.h"
#include "soundinfo.h"
#include "toolframework/itoolentity.h"
#include "utils/common/scriplib.h"
#include "vcollide_parse.h"
#include "vscript/ivscript.h"
#include "baseentity.h"
#include "cdll_client_int.h"
#include "predictable_entity.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "ihandleentity.h"
#include "ispatialpartition.h"
#include "dt_recv.h"
#include "view_shared.h"
#include "engine/ivmodelinfo.h"
#include "materialsystem/MaterialSystemUtil.h"
module ks.client:baseentity;

import std;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import ks.game.ai_activity;
import ks.tier1.convar;
import ks.game.shareddefs;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.core.basetypes;
import ks.core.vprof;
import ks.tier1.lerp_functions;
import ks.tier1.rangecheckedvar;
import ks.tier1.createinterface;
import ks.core.random;
import ks.tier1.interfaces;
import ks.game.iplayeranimstate;
import ks.math.vector2d;
import ks.core.numeric;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.transform;
import ks.tier1.bitbuf;
import ks.tier1.exprevaluator;
import ks.tier1.interpolatedvar;
import ks.tier1.reflect_wirevar;
import ks.vstdlib.callqueue;
import :particlemgr;
import ks.game.entitycore;
import ks.game.mapentities_shared;
import ks.math.polyhedron;
import ks.math.vertexcolor;
import ks.math.vmatrix;
import ks.tier1.strtools;

typedef struct activityentry_s activityentry_t;

class CActivityRemap
{
public:

	CActivityRemap()
	{
		pExtraBlock = nullptr;
	}

	void SetExtraKeyValueBlock ( KeyValues *pKVBlock )
	{
		pExtraBlock = pKVBlock;
	}

	KeyValues *GetExtraKeyValueBlock ( void ) { return pExtraBlock; }

	Activity 		activity;
	Activity		mappedActivity;

private:

	KeyValues		*pExtraBlock;
};

class CActivityRemapCache
{
public:

	CActivityRemapCache()
	{
	}

	CActivityRemapCache( const CActivityRemapCache& src );

	CActivityRemapCache& operator = ( const CActivityRemapCache& src )
	{
		if ( this == &src )
			return *this;

		int c = std::ssize( src.m_cachedActivityRemaps );
		for ( int i = 0; i < c; i++ )
		{
			m_cachedActivityRemaps.push_back( src.m_cachedActivityRemaps[ i ] );
		}

		return *this;
	}

	std::vector< CActivityRemap > m_cachedActivityRemaps;
};

void UTIL_LoadActivityRemapFile( const char *filename, const char *section, std::vector< CActivityRemap > &entries );

//=========================================================
//=========================================================
extern void ActivityList_Init( void );
extern void ActivityList_Free( void );
extern bool ActivityList_RegisterSharedActivity( const char *pszActivityName, int iActivityIndex );
extern Activity ActivityList_RegisterPrivateActivity( const char *pszActivityName );
extern int ActivityList_IndexForName( const char *pszActivityName );
extern const char *ActivityList_NameForIndex( int iActivityIndex );
extern int ActivityList_HighestIndex();

// This macro guarantees that the names of each activity and the constant used to
// reference it in the code are identical.
#define REGISTER_PRIVATE_ACTIVITY( _n ) _n = ActivityList_RegisterPrivateActivity( #_n );

// Implemented in shared code
extern void ActivityList_RegisterSharedActivities( void );

#undef REGISTER_PRIVATE_ACTIVITY

class CAmmoDef;

class CBaseCombatCharacter;

// Purpose: Holds defintion for game ammo types


struct Ammo_t 
{
	char 				*pName;
	int					nDamageType;
	int					eTracerType;
	int					nMinSplashSize;
	int					nMaxSplashSize;

	int					nFlags;

	// Values for player/NPC damage and carrying capability
	// If the integers are set, they override the CVars
	int					pPlrDmg;					// CVar for player damage amount
	int					pNPCDmg;					// CVar for NPC damage amount
	int					pMaxCarry;					// CVar for maximum number can carry
	int					pPhysicsForceImpulse;		// CVar for the physics impulse
	const ConVar*		pPlrDmgCVar;				// CVar for player damage amount
	const ConVar*		pNPCDmgCVar;				// CVar for NPC damage amount
	const ConVar*		pMaxCarryCVar;				// CVar for maximum number can carry
	const ConVar*		pPhysicsForceImpulseCVar;	// CVar for maximum number can carry
};

// Used to tell AmmoDef to use the cvars, not the integers
inline constexpr auto USE_CVAR = -1;
// Ammo is infinite
inline constexpr auto INFINITE_AMMO = -2;

enum AmmoTracer_t
{
	TRACER_NONE,
	TRACER_LINE,
	TRACER_RAIL,
	TRACER_BEAM,
	TRACER_LINE_AND_WHIZ,
};

enum AmmoFlags_t
{
	AMMO_FORCE_DROP_IF_CARRIED = 0x1,
	AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER = 0x2,
};

//=============================================================================
//	>> CAmmoDef
//=============================================================================
class CAmmoDef
{

public:
	int					m_nAmmoIndex;

	Ammo_t				m_AmmoType[MAX_AMMO_TYPES];

	Ammo_t				*GetAmmoOfIndex(int nAmmoIndex);
	int					Index(const char *psz);
	int					PlrDamage(int nAmmoIndex);
	int					NPCDamage(int nAmmoIndex);
	int					MaxCarry(int nAmmoIndex, const CBaseCombatCharacter *owner);
	bool				CanCarryInfiniteAmmo(int nAmmoIndex);
	int					DamageType(int nAmmoIndex);
	int					TracerType(int nAmmoIndex);
	float				DamageForce(int nAmmoIndex);
	int					MinSplashSize(int nAmmoIndex);
	int					MaxSplashSize(int nAmmoIndex);
	int					Flags(int nAmmoIndex);

	void				AddAmmoType(char const* name, int damageType, int tracerType, int plr_dmg, int npc_dmg, int carry, int impulse, int nFlags, int minSplashSize = 4, int maxSplashSize = 8 );
	void				AddAmmoType(char const* name, int damageType, int tracerType, char const* plr_cvar, char const* npc_var, char const* carry_cvar, char const* impulse_cvar, int nFlags, int minSplashSize = 4, int maxSplashSize = 8 );
	int					NumAmmoTypes() { return m_nAmmoIndex; }

	CAmmoDef(void);
	virtual ~CAmmoDef( void );

private:
	bool				AddAmmoType(char const* name, int damageType, int tracerType, int nFlags, int minSplashSize, int maxSplashSize );
};

// Get the global ammodef object. This is usually implemented in each mod's game rules file somewhere,
// so the mod can setup custom ammo types.
CAmmoDef* GetAmmoDef();

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

class CBaseAnimatingOverlay;

// The overloads below must not hide the shared ones.
using ::LoopingLerp;
using ::LoopingLerp_Hermite;
using ::Lerp;
using ::Lerp_Hermite;
using ::Lerp_Clamp;


void RecvProxy_SequenceChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_WeightChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_WeightDeltaRateChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_CycleChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_PlaybackRateChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_OrderChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CAnimationLayer
{
	[[= ks::reflect::NetTable{ .name = "DT_Animationlayer", .base = false } ]]
	static constexpr bool ks_class = true;

public:

	// This allows the datatables to access private members.
	ALLOW_DATATABLES_PRIVATE_ACCESS();

	CAnimationLayer();
	void Reset();

	void SetOwner( CBaseAnimatingOverlay *pOverlay );
	CBaseAnimatingOverlay *GetOwner() const;

	void SetOrder( int order );
	bool IsActive( void );
	float GetFadeout( float flCurTime );

	void SetSequence( int nSequence );
	void SetCycle( float flCycle );
	void SetPrevCycle( float flCycle );
	void SetPlaybackRate( float flPlaybackRate );
	void SetWeight( float flWeight );
	void SetWeightDeltaRate( float flDelta );

	int   GetOrder() const;
	int   GetSequence( ) const;
	float GetCycle( ) const;
	float GetPrevCycle( ) const;
	float GetPlaybackRate( ) const;
	float GetWeight( ) const;
	float GetWeightDeltaRate( ) const;

	// If the weights, cycle or sequence #s changed due to interpolation then 
	//  we'll need to recompute the bbox
	int GetInvalidatePhysicsBits() const;
	void SetInvalidatePhysicsBits( int iBit ) { m_nInvalidatePhysicsBits = iBit; }

public:
	float	m_flLayerAnimtime;
	float	m_flLayerFadeOuttime;

	// dispatch flags
	CStudioHdr	*m_pDispatchedStudioHdr;
	int		m_nDispatchedSrc;
	int		m_nDispatchedDst;

private:
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_OrderChanged, ks::reflect::WireSide::Recv>{} ]] int		m_nOrder;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_SequenceChanged, ks::reflect::WireSide::Recv>{} ]] CRangeCheckedVar<int, -1, 65535, 0>		m_nSequence;
	[[= ks::reflect::Net{} ]] CRangeCheckedVar<float, -2, 2, 0>		m_flPrevCycle;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_WeightChanged, ks::reflect::WireSide::Recv>{} ]] CRangeCheckedVar<float, -5, 5, 0>		m_flWeight;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_WeightDeltaRateChanged, ks::reflect::WireSide::Recv>{} ]] CRangeCheckedVar<float, -5, 5, 0>		m_flWeightDeltaRate;

	// used for automatic crossfades between sequence changes
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_PlaybackRateChanged, ks::reflect::WireSide::Recv>{} ]] CRangeCheckedVar<float, -50, 50, 1>		m_flPlaybackRate;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_CycleChanged, ks::reflect::WireSide::Recv>{} ]] CRangeCheckedVar<float, -2, 2, 0>		m_flCycle;

	CBaseAnimatingOverlay	*m_pOwner;
	int					m_nInvalidatePhysicsBits;

	friend class CBaseAnimatingOverlay;
	friend CAnimationLayer LoopingLerp( float flPercent, CAnimationLayer& from, CAnimationLayer& to );
	friend CAnimationLayer Lerp( float flPercent, const CAnimationLayer& from, const CAnimationLayer& to );
	friend CAnimationLayer LoopingLerp_Hermite( const CAnimationLayer& current, float flPercent, CAnimationLayer& prev, CAnimationLayer& from, CAnimationLayer& to );
	friend CAnimationLayer Lerp_Hermite( const CAnimationLayer& current, float flPercent, const CAnimationLayer& prev, const CAnimationLayer& from, const CAnimationLayer& to );
	friend void Lerp_Clamp( CAnimationLayer &val );
	friend int CheckForSequenceBoxChanges( const CAnimationLayer& newLayer, const CAnimationLayer& oldLayer );
};

CAnimationLayer::CAnimationLayer()
{
	m_pOwner = nullptr;
	m_nInvalidatePhysicsBits = 0;
	m_pDispatchedStudioHdr = nullptr;
	m_nDispatchedSrc = ACT_INVALID;
	m_nDispatchedDst = ACT_INVALID;

	Reset();
}

[[gnu::always_inline]] inline void CAnimationLayer::SetPrevCycle( float flPrevCycle )
{
	m_flPrevCycle = flPrevCycle;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetPlaybackRate( float flPlaybackRate )
{
	m_flPlaybackRate = flPlaybackRate;
}

[[gnu::always_inline]] inline void CAnimationLayer::SetWeightDeltaRate( float flDelta )
{
	m_flWeightDeltaRate = flDelta;
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

[[gnu::always_inline]] inline int CAnimationLayer::GetOrder() const
{
	return m_nOrder;
}

float CAnimationLayer::GetFadeout( float flCurTime )
{
	float s;

    if (m_flLayerFadeOuttime <= 0.0f)
	{
		s = 0;
	}
	else
	{
		// blend in over 0.2 seconds
		s = 1.0 - (flCurTime - m_flLayerAnimtime) / m_flLayerFadeOuttime;
		if (s > 0 && s <= 1.0)
		{
			// do a nice spline curve
			s = 3 * s * s - 2 * s * s * s;
		}
		else if ( s > 1.0f )
		{
			// Shouldn't happen, but maybe curtime is behind animtime?
			s = 1.0f;
		}
	}
	return s;
}

[[gnu::always_inline]] inline int CAnimationLayer::GetInvalidatePhysicsBits() const
{
	return m_nInvalidatePhysicsBits;
}

inline CAnimationLayer LoopingLerp( float flPercent, CAnimationLayer& from, CAnimationLayer& to )
{
	contract_assert( from.GetOwner() == to.GetOwner() );

	CAnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = LoopingLerp( flPercent, (float)from.m_flCycle, (float)to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;
	output.SetOwner( to.GetOwner() );
	return output;
}

inline CAnimationLayer Lerp( float flPercent, const CAnimationLayer& from, const CAnimationLayer& to )
{
	contract_assert( from.GetOwner() == to.GetOwner() );

	CAnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = Lerp( flPercent, from.m_flCycle, to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;
	output.SetOwner( to.GetOwner() );
	return output;
}

inline int CheckForSequenceBoxChanges( const CAnimationLayer& newLayer, const CAnimationLayer& oldLayer )
{
	int nChangeFlags = 0;

	bool bOldIsZero = ( oldLayer.GetWeight() == 0.0f );
	bool bNewIsZero = ( newLayer.GetWeight() == 0.0f );

	if ( ( newLayer.GetSequence() != oldLayer.GetSequence() ) ||
		 ( bNewIsZero != bOldIsZero ) ) 
	{
		nChangeFlags |= SEQUENCE_CHANGED | BOUNDS_CHANGED;
	}

	if ( newLayer.GetCycle() != oldLayer.GetCycle() )
	{
		nChangeFlags |= ANIMATION_CHANGED;
	}

	if ( newLayer.GetOrder() != oldLayer.GetOrder() )
	{
		nChangeFlags |= BOUNDS_CHANGED;
	}

	return nChangeFlags;
}

inline CAnimationLayer LoopingLerp_Hermite( const CAnimationLayer& current, float flPercent, CAnimationLayer& prev, CAnimationLayer& from, CAnimationLayer& to )
{
	contract_assert( prev.GetOwner() == from.GetOwner() );
	contract_assert( from.GetOwner() == to.GetOwner() );

	CAnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = LoopingLerp_Hermite( (float)current.m_flCycle, flPercent, (float)prev.m_flCycle, (float)from.m_flCycle, (float)to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;

	output.SetOwner( to.GetOwner() );
	output.m_nInvalidatePhysicsBits = CheckForSequenceBoxChanges( output, current );
	return output;
}

// YWB:  Specialization for interpolating euler angles via quaternions...
inline CAnimationLayer Lerp_Hermite( const CAnimationLayer& current, float flPercent, const CAnimationLayer& prev, const CAnimationLayer& from, const CAnimationLayer& to )
{
	contract_assert( prev.GetOwner() == from.GetOwner() );
	contract_assert( from.GetOwner() == to.GetOwner() );

	CAnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = Lerp_Hermite( (float)current.m_flCycle, flPercent, (float)prev.m_flCycle, (float)from.m_flCycle, (float)to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;
	output.SetOwner( to.GetOwner() );
 	output.m_nInvalidatePhysicsBits = CheckForSequenceBoxChanges( output, current );
	return output;
}

inline void Lerp_Clamp( CAnimationLayer &val )
{
	Lerp_Clamp( val.m_nSequence );
	Lerp_Clamp( val.m_flCycle );
	Lerp_Clamp( val.m_flPrevCycle );
	Lerp_Clamp( val.m_flWeight );
	Lerp_Clamp( val.m_nOrder );
	Lerp_Clamp( val.m_flLayerAnimtime );
	Lerp_Clamp( val.m_flLayerFadeOuttime );
}

inline float CalcDistance( float x, float y )
{
	return x - y;
}

inline float CalcDistance( const Vector &a, const Vector &b )
{
	return a.DistTo( b );
}

template<class T>
class CDefaultCalcDistance
{
public:
	static inline float CalcDistance( const T &a, const T &b )
	{
		return ::CalcDistance( a, b );
	}
};

class CCalcDistance2D
{
public:
	static inline float CalcDistance( const Vector &a, const Vector &b )
	{
		return (a-b).Length2D();
	}
};

template< class T, class Functor=CDefaultCalcDistance<T> >
class CApparentVelocity
{
public:
	CApparentVelocity()
	{
		m_LastTime = -1;
	}

	float AddSample( float time, T value )
	{
		float flRet = 0;
		if ( m_LastTime != -1 )
		{
			flRet = Functor::CalcDistance(value, m_LastValue) / (time - m_LastTime);
		}

		m_LastTime = time;
		m_LastValue = value;

		return flRet;
	}

private:
	T m_LastValue;
	float m_LastTime;
};

class CAnimationLayer;

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

class CBaseEntity;
struct inputdata_t;


// Entity input handler. typedescription_t keeps it in an inputfunc_storage_t, which datamap.h
// leaves untyped so that it names no game class.
typedef void (CBaseEntity::*inputfunc_t)(inputdata_t &data);

inline inputfunc_storage_t ToInputStorage( inputfunc_t pFunction )
{
	static_assert( sizeof( inputfunc_t ) == sizeof( inputfunc_storage_t ) );
	inputfunc_storage_t storage = {};
	std::memcpy( &storage, &pFunction, sizeof( pFunction ) );
	return storage;
}

inline inputfunc_t FromInputStorage( const inputfunc_storage_t &storage )
{
	inputfunc_t pFunction;
	std::memcpy( &pFunction, &storage, sizeof( pFunction ) );
	return pFunction;
}
struct FireBulletsInfo_t
{
	FireBulletsInfo_t()
	{
		m_iShots = 1;
		m_vecSpread.Init( 0, 0, 0 );
		m_flDistance = 8192;
		m_iTracerFreq = 4;
		m_flDamage = 0.0f;
		m_flPlayerDamage = 0.0f;
		m_pAttacker = nullptr;
		m_nFlags = 0;
		m_pAdditionalIgnoreEnt = nullptr;
		m_flDamageForceScale = 1.0f;

		m_bPrimaryAttack = true;
	}

	FireBulletsInfo_t( int nShots, const Vector &vecSrc, const Vector &vecDir, const Vector &vecSpread, float flDistance, int nAmmoType, bool bPrimaryAttack = true )
	{
		m_iShots = nShots;
		m_vecSrc = vecSrc;
		m_vecDirShooting = vecDir;
		m_vecSpread = vecSpread;
		m_flDistance = flDistance;
		m_iAmmoType = nAmmoType;
		m_iTracerFreq = 4;
		m_flDamage = 0;
		m_flPlayerDamage = 0;
		m_pAttacker = nullptr;
		m_nFlags = 0;
		m_pAdditionalIgnoreEnt = nullptr;
		m_flDamageForceScale = 1.0f;
		m_bPrimaryAttack = bPrimaryAttack;
	}

	int m_iShots;
	Vector m_vecSrc;
	Vector m_vecDirShooting;
	Vector m_vecSpread;
	float m_flDistance;
	int m_iAmmoType;
	int m_iTracerFreq;
	float m_flDamage;
	float m_flPlayerDamage;	// Damage to be used instead of m_flDamage if we hit a player
	int m_nFlags;			// See FireBulletsFlags_t
	float m_flDamageForceScale;
	CBaseEntity *m_pAttacker;
	CBaseEntity *m_pAdditionalIgnoreEnt;
	bool m_bPrimaryAttack;
};

struct EmitSound_t
{
	EmitSound_t() :
		m_nChannel( 0 ),
		m_pSoundName( 0 ),
		m_flVolume( VOL_NORM ),
		m_SoundLevel( SNDLVL_NONE ),
		m_nFlags( 0 ),
		m_nPitch( PITCH_NORM ),
		m_pOrigin( 0 ),
		m_flSoundTime( 0.0f ),
		m_pflSoundDuration( 0 ),
		m_bEmitCloseCaption( true ),
		m_bWarnOnMissingCloseCaption( false ),
		m_bWarnOnDirectWaveReference( false ),
		m_nSpeakerEntity( -1 ),
		m_UtlVecSoundOrigin(),
		m_hSoundScriptHash( SOUNDEMITTER_INVALID_HASH ),
		m_nSoundEntryVersion( 1 )
	{
	}

	EmitSound_t( const CSoundParameters &src );

	int							m_nChannel;
	char const					*m_pSoundName;
	float						m_flVolume;
	soundlevel_t				m_SoundLevel;
	int							m_nFlags;
	int							m_nPitch;
	const Vector				*m_pOrigin;
	float						m_flSoundTime; ///< NOT DURATION, but rather, some absolute time in the future until which this sound should be delayed
	float						*m_pflSoundDuration;
	bool						m_bEmitCloseCaption;
	bool						m_bWarnOnMissingCloseCaption;
	bool						m_bWarnOnDirectWaveReference;
	int							m_nSpeakerEntity;
	mutable std::vector< Vector >	m_UtlVecSoundOrigin;  ///< Actual sound origin(s) (can be multiple if sound routed through speaker entity(ies) )
	mutable HSOUNDSCRIPTHASH	m_hSoundScriptHash;
	int							m_nSoundEntryVersion;
};

bool IsHeadTrackingEnabled();

// Purpose: Exposes client-server neutral interfaces implemented in both places

class IMapData;

extern IFileSystem				*filesystem;
extern CUniformRandomStream		*random_valve;
extern CGaussianRandomStream *randomgaussian;
extern IEngineSound				*enginesound;
extern IMapData					*g_pMapData;			// TODO: current implementations of the 
														// interface are in TF2, should probably move
														// to TF2/HL2 neutral territory

class CBaseAnimatingOverlay;

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

class CBaseEntity;


// Purpose: insulates client DLL from dependencies on vphysics

// HACKHACK: Make this part of IClientSystem somehow???
extern bool PhysicsDLLInit( CreateInterfaceFn physicsFactory );

extern void PhysicsReset();
extern void PhysicsSimulate();
extern float PhysGetSyncCreateTime();
extern void VPhysicsShadowDataChanged( bool bCreate, CBaseEntity *pEntity );

void PhysAddShadow( CBaseEntity *pEntity );
void PhysRemoveShadow( CBaseEntity *pEntity );
void PhysSetPredictionCommandNum( int iCommandNum );

class CBaseEntity;

class CClientAlphaProperty;
class CNewParticleEffect;
class CPrediction;
class CTakeDamageInfo;
class CUserCmd;
class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBasePlayer;
class CRecipientFilter;
class CTeam;
struct groundlink_t;
struct touchlink_t;

using namespace ::ks::reflect;

// Purpose: A base class for the client-side representation of entities.
//			This class encompasses both entities that are created on the server
//			and networked to the client AND entities that are created on the
//			client.

void RecvProxy_AnimTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CDamageModifier;

typedef unsigned int			AimEntsListHandle_t;

inline constexpr auto INVALID_AIMENTS_LIST_HANDLE = (AimEntsListHandle_t)~0;

extern void RecvProxy_LocalVelocity( const CRecvProxyData *pData, void *pStruct, void *pOut );

enum CollideType_t
{
	ENTITY_SHOULD_NOT_COLLIDE = 0,
	ENTITY_SHOULD_COLLIDE,
	ENTITY_SHOULD_RESPOND
};

class VarMapEntry_t
{
	
public:
	unsigned short		type;
	unsigned short		m_bNeedsToInterpolate;	// Set to false when this var doesn't
												// need Interpolate() called on it anymore.
	void				*data;
	IInterpolatedVar	*watcher;
};

struct VarMapping_t
{
	VarMapping_t()
	{
		m_nInterpolatedEntries = 0;
	}

	std::vector< VarMapEntry_t >	m_Entries;
	int							m_nInterpolatedEntries;
	float						m_lastInterpolationTime;
};

// How many data slots to use when in multiplayer.
inline constexpr auto MULTIPLAYER_BACKUP = 150;

struct serialentity_t;

typedef CHandle<CBaseEntity> EHANDLE; // The client's version of EHANDLE.

typedef void (CBaseEntity::*BASEPTR)(void);
typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther );

// For entity creation on the client
typedef CBaseEntity* (*DISPATCHFUNCTION)( void );

//-----------------------------------------------------------------------------
// Purpose: think contexts
//-----------------------------------------------------------------------------
struct thinkfunc_t
{
	BASEPTR		m_pfnThink;
	string_t	m_iszContext;
	int			m_nNextThinkTick;
	int			m_nLastThinkTick;
};

#define CREATE_PREDICTED_ENTITY( className )	\
	CBaseEntity::CreatePredictedEntityByName( className, __FILE__, __LINE__ );

// Entity flags that only exist on the client.
inline constexpr auto ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS = 0x0001; // Tells us if we're getting the real ent render bounds or the shadow render bounds.
inline constexpr auto ENTCLIENTFLAG_DONTUSEIK = 0x0002; // Don't use IK on this entity even if its model has IK.
inline constexpr auto ENTCLIENTFLAG_ALWAYS_INTERPOLATE = 0x0004; // Used by view models.

enum entity_list_ids_t
{
	ENTITY_LIST_INTERPOLATE = 0,
	ENTITY_LIST_TELEPORT,
	ENTITY_LIST_PRERENDER,
	ENTITY_LIST_SIMULATE,
	ENTITY_LIST_DELETE,
	
	NUM_ENTITY_LISTS
};

template< typename Type >
class CDiscontinuousInterpolatedVar : public CInterpolatedVar<Type>
{
public:
	explicit CDiscontinuousInterpolatedVar( const char *pDebugName = nullptr )
		: CInterpolatedVar<Type>(pDebugName)
	{

	}

	// Returns 1 if the value will always be the same if currentTime is always increasing.
	virtual int Interpolate( float currentTime )
	{
		int iRetVal = CInterpolatedVar<Type>::Interpolate(currentTime);
		if( m_Discontinuities.empty() )
		{
			return iRetVal;
		}

		ClearOldDiscontinuities();

		float fInterpolatedTime = currentTime - this->m_InterpolationAmount;

		for( int i = std::ssize( m_Discontinuities ); --i >= 0; )
		{
			if( m_Discontinuities[i].fBeforeTime < fInterpolatedTime )
			{
				break;
			}

			TransformValue( m_Discontinuities[i].matTransform, *(this->m_pValue) );
			iRetVal = 0;
		}

		return iRetVal;
	}

	virtual void Reset( float flCurrentTime )
	{
		CInterpolatedVar<Type>::Reset(flCurrentTime);
		ClearOldDiscontinuities();
	}

	//transforms all history prior to fDiscontinuityTime by matTransform (with the assumption that newer entries are in the new space)
	//base interpolation is applied in the new space, with the inverse of matTransform applied to values interpolated to a time prior to fDiscontinuityTime
	void InsertDiscontinuity( const matrix3x4_t &matTransform, float fDiscontinuityTime )
	{
		ClearOldDiscontinuities();

		TransformBefore( matTransform, fDiscontinuityTime );

		int iInsertAfter;
		for( iInsertAfter = m_Discontinuities.Count(); --iInsertAfter >= 0; )
		{
			if( m_Discontinuities[iInsertAfter].fBeforeTime <= fDiscontinuityTime )
				break;
		}
		if( iInsertAfter < 0 )
		{
			iInsertAfter = m_Discontinuities.AddToTail();
		}
		else
		{
			iInsertAfter = m_Discontinuities.InsertAfter( iInsertAfter );
		}

		MatrixInvert( matTransform, m_Discontinuities[iInsertAfter].matTransform );
		m_Discontinuities[iInsertAfter].fBeforeTime = fDiscontinuityTime;
	}

	bool RemoveDiscontinuity( float fDiscontinuityTime, const matrix3x4_t *pFailureTransform = nullptr )
	{
		//assume the general case for this is rolling back in time for prediction
		for( int i = m_Discontinuities.Count(); --i >= 0; )
		{
			if( m_Discontinuities[i].fBeforeTime == fDiscontinuityTime )
			{
				TransformBefore( m_Discontinuities[i].matTransform, fDiscontinuityTime );
				m_Discontinuities.Remove( i );
				return true;
			}
		}

		if( pFailureTransform )
		{
			TransformBefore( *pFailureTransform, fDiscontinuityTime );
		}
		return false;
	}

	float GetInterpolationAmount( void ) { return this->m_InterpolationAmount; }
	float GetInterpolatedTime( float fCurTime ) //What's the timestamp on the value we'll use.
	{
		float fTargetTime = fCurTime - this->m_InterpolationAmount;
		float fOldestEntryTime = this->GetOldestEntry();
		fOldestEntryTime = std::min( fOldestEntryTime, fCurTime ); //pull entries in the future to now
		return std::max( fTargetTime, fOldestEntryTime );
	}

	bool HasDiscontinuityForTime( float fCurTime )
	{
		if( m_Discontinuities.Count() == 0 )
			return false;

		float fTargetTime = GetInterpolatedTime( fCurTime );

		return ( fTargetTime < m_Discontinuities[m_Discontinuities.Count()-1].fBeforeTime );
	}

	bool GetDiscontinuityTransform( float fCurTime, matrix3x4_t &matOut )
	{		
		if( m_Discontinuities.empty() )
			return false;

		float fTargetTime = GetInterpolatedTime( fCurTime );

		if( fTargetTime >= m_Discontinuities[std::ssize( m_Discontinuities )-1].fBeforeTime )
			return false;

		//common case is exactly 0 or 1 transforms. 0's handled above. Do a copy of the first transform now and skip it in the iterator below
		matOut = m_Discontinuities[std::ssize( m_Discontinuities ) - 1].matTransform;

		matrix3x4_t matTemp;
		matrix3x4_t *pSwapMatrices[2] = { &matOut, &matTemp };
		int iSwapRead = 0; //which of the swap indices has the newest value
		for( int i = std::ssize( m_Discontinuities ) - 1; --i >= 0; )
		{
			if( m_Discontinuities[i].fBeforeTime < fTargetTime )
			{
				break;
			}

			ConcatTransforms( m_Discontinuities[i].matTransform, *pSwapMatrices[iSwapRead], *pSwapMatrices[1 - iSwapRead] );
			iSwapRead = 1 - iSwapRead;
		}

		if( iSwapRead == 1 ) //matTemp has the most recent value one final copy necessary to output matrix
			matOut = matTemp;

		return true;			
	}

protected:
	struct Discontinuity_t
	{
		matrix3x4_t matTransform; //from current space to previous space
		float fBeforeTime;
	};

	void ClearOldDiscontinuities( void )
	{
		if( m_Discontinuities.empty() )
			return;

		float fOldestEntry = this->GetOldestEntry();
		while( fOldestEntry >= m_Discontinuities[0].fBeforeTime )
		{
			m_Discontinuities.erase( m_Discontinuities.begin() + 0 );
			if( m_Discontinuities.empty() )
				break;
		}
	}

	void TransformBefore( const matrix3x4_t &matTransform, float fDiscontinuityTime )
	{
		int iHead = this->GetHead();
		if( !this->IsValidIndex( iHead ) )
			return;

		float fTime;
		Type *pCurrent;
		int iCurrent;

		iCurrent = iHead;

		while( (pCurrent = this->GetHistoryValue( iCurrent, fTime )) != nullptr )
		{

			if( fTime < fDiscontinuityTime )
				TransformValue( matTransform, *pCurrent );

			iCurrent = this->GetNext( iCurrent );
			if( iCurrent == iHead )
				break;
		}
	}

	static void TransformValue( const matrix3x4_t &matTransform, Type &Value );

	std::vector< Discontinuity_t > m_Discontinuities;	
};

template<>
inline void CDiscontinuousInterpolatedVar<Vector>::TransformValue( const matrix3x4_t &matTransform, Vector &Value )
{
	Vector vTemp = Value;
	VectorTransform( vTemp, matTransform, Value );
}

template<>
inline void CDiscontinuousInterpolatedVar<QAngle>::TransformValue( const matrix3x4_t &matTransform, QAngle &Value )
{
	Value = TransformAnglesToWorldSpace( Value, matTransform );
}

// Named by CBaseEntity's prediction annotation on m_vecNetworkOrigin, so it has to be a
// constant expression here rather than a const float in the .cpp.
constexpr float coordTolerance = 2.0f / (float)( 1 << COORD_FRACTIONAL_BITS );

// DT_BaseEntity names DT_AnimTimeMustBeFirst as a sub-table prop; the global is defined by
// IMPLEMENT_REFLECT_TABLE_IN in baseentity.cpp.
namespace DT_AnimTimeMustBeFirst { extern RecvTable g_RecvTable; }

// Defined in baseentity.cpp and named by CBaseEntity's network annotations, which have to
// be constant expressions here rather than only beside the table.
void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut );
// The two props named by a bare wire string: the proxy writes into the entity through
// SetMoveType/SetMoveCollide and no member is named, so there is nothing to annotate but the
// class. They were .cpp statics; a Bare<> annotation needs external linkage.
void RecvProxy_MoveType( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_MoveCollide( const CRecvProxyData *pData, void *pStruct, void *pOut );

//-----------------------------------------------------------------------------
// Purpose: Base client side entity object
//-----------------------------------------------------------------------------
class CBaseEntity : public IClientEntity, public IClientModelRenderable, public CEntityCore
{
	[[= ks::reflect::NetTable{ .name = "DT_AnimTimeMustBeFirst", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_BaseEntity", .base = false } ]]
	[[= ks::reflect::SubTable<"AnimTimeMustBeFirst", &DT_AnimTimeMustBeFirst::g_RecvTable,
	nullptr, true>{} ]]
	[[= ks::reflect::Bare<"movetype",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }, RecvProxy_MoveType>{} ]]
	[[= ks::reflect::Bare<"movecollide",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }, RecvProxy_MoveCollide>{} ]]
	[[= ks::reflect::PredFrom<"m_MoveType", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_MoveCollide", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_fFlags", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_vecViewOffset", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.25f } >{} ]]
	[[= ks::reflect::PredFrom<"m_vecVelocity", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.5f } >{} ]]
	[[= ks::reflect::PredFrom<"m_flFriction", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	static constexpr bool ks_class = true;

// Construction
	DECLARE_CLASS_NOBASE( CBaseEntity );

	friend class CPrediction;
	friend void cc_cl_interp_all_changed( ConVar *pConVar, const char *pOldString, float flOldValue );

public:
	DECLARE_DATADESC();
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	// script description
	DECLARE_ENT_SCRIPTDESC();

									CBaseEntity();

protected:
	// Use UTIL_Remove to delete!
	virtual							~CBaseEntity();

public:
	static CBaseEntity				*CreatePredictedEntityByName( const char *classname, const char *module, int line, bool persist = false );
	static void						UpdateVisibilityAllEntities();

	// FireBullets uses shared code for prediction.
	virtual void					FireBullets( const FireBulletsInfo_t &info );
	virtual bool					ShouldDrawUnderwaterBulletBubbles();
	virtual bool					ShouldDrawWaterImpacts( void ) { return true; }
	virtual bool					HandleShotImpactingWater( const FireBulletsInfo_t &info, 
		const Vector &vecEnd, ITraceFilter *pTraceFilter, Vector *pVecTracerDest );
	virtual ITraceFilter*			GetBeamTraceFilter( void );
	virtual void					DispatchTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void					TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void					DoImpactEffect( trace_t &tr, int nDamageType );
	virtual void					MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	virtual int						GetTracerAttachment( void );
	void							ComputeTracerStartPosition( const Vector &vecShotSrc, Vector *pVecTracerStart );
	void							TraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType );
	virtual int						BloodColor();
	virtual const char*				GetTracerType();

	// called when entity is damaged by predicted attacks
	virtual void					TakeDamage( const CTakeDamageInfo &info ) { }

	virtual void					Spawn( void );
	virtual void					SpawnClientEntity( void );
	virtual void					Precache( void );
	virtual void					Activate();

	void							ParseMapData( CEntityMapData *mapData );
	virtual void					OnParseMapDataFinished();
	virtual bool					KeyValue( const char *szKeyName, const char *szValue );
	virtual bool					KeyValue( const char *szKeyName, float flValue );
	virtual bool					KeyValue( const char *szKeyName, int nValue );
	virtual bool					KeyValue( const char *szKeyName, const Vector &vecValue );
	virtual bool					GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );

		// Entities block Line-Of-Sight for NPCs by default.
	// Set this to false if you want to change this behavior.
	void							SetBlocksLOS( bool bBlocksLOS );
	bool							BlocksLOS( void );
	void							SetAIWalkable( bool bBlocksLOS );
	bool							IsAIWalkable( void );

	virtual void					InitSharedVars( void ) {};

	void							Interp_SetupMappings( VarMapping_t *map );
	
	// Returns 1 if there are no more changes (ie: we could call RemoveFromInterpolationList).
	int								Interp_Interpolate( VarMapping_t *map, float currentTime );
	
	void							Interp_RestoreToLastNetworked( VarMapping_t *map, int flags );
	void							Interp_UpdateInterpolationAmounts( VarMapping_t *map );
	void							Interp_HierarchyUpdateInterpolationAmounts();

	// Called by the CLIENTCLASS macros.
	virtual bool					Init( int entnum, int iSerialNum );

	// Called in the destructor to shutdown everything.
	void							Term();

	// memory handling, uses calloc so members are zero'd out on instantiation
    void							*operator new( std::size_t stAllocateBlock );
	void							*operator new[]( std::size_t stAllocateBlock );
    void							*operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );
	void							*operator new[]( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );
	void							operator delete( void *pMem );
	void							operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine ) { operator delete( pMem ); }

	// This just picks one of the routes to IClientUnknown.
	IClientUnknown*					GetIClientUnknown()	{ return this; }
	virtual CBaseAnimating*		GetBaseAnimating() { return nullptr; }
	virtual CBaseAnimatingOverlay *GetBaseAnimatingOverlay() { return nullptr; }
	virtual void					SetClassname( const char *className );

	virtual Class_T					Classify( void ) { return CLASS_NONE; }

	string_t						m_iClassname;

	HSCRIPT GetScriptInstance();

	HSCRIPT			m_hScriptInstance = nullptr;
	string_t		m_iszScriptId;

	[[= ks::reflect::Net{} ]] float m_flLastMadeNoiseTime;

// IClientUnknown overrides.
public:

	virtual void SetRefEHandle( const CBaseHandle &handle );
	virtual const CBaseHandle& GetRefEHandle() const;

	void					SetToolHandle( HTOOLHANDLE handle );
	HTOOLHANDLE				GetToolHandle() const;

	void					EnableInToolView( bool bEnable );
	bool					IsEnabledInToolView() const;

	void					SetToolRecording( bool recording );
	bool					IsToolRecording() const;
	bool					HasRecordedThisFrame() const;
	virtual void			RecordToolMessage();
	virtual void			OnToolStartRecording( void ) {};

	// used to exclude entities from being recorded in the SFM tools
	void					DontRecordInTools();
	bool					ShouldRecordInTools() const;

protected:
	virtual void					Release();

// IClientEntity
public:
	virtual ICollideable*			GetCollideable()		{ return CollisionProp(); }
	virtual IClientNetworkable*		GetClientNetworkable()	{ return this; }
	virtual IClientRenderable*		GetClientRenderable()	{ return this; }
	virtual IClientEntity*			GetIClientEntity()		{ return this; }
	virtual CBaseEntity*			GetBaseEntity()			{ return this; }
	virtual IClientThinkable*		GetClientThinkable()	{ return this; }
	virtual IClientModelRenderable*	GetClientModelRenderable();
	virtual IClientAlphaProperty*	GetClientAlphaProperty();

	// Inherited from IClientModelRenderable
public:
	virtual bool GetRenderData( void *pData, ModelDataCategory_t nCategory );

// Methods of IClientRenderable
public:

	virtual const Vector&			GetRenderOrigin( void );
	virtual const QAngle&			GetRenderAngles( void );
	virtual Vector					GetObserverCamOrigin( void ) { return GetRenderOrigin(); }	// Return the origin for player observers tracking this target
	virtual const matrix3x4_t &		RenderableToWorldTransform();
	virtual bool					IsTransparent( void );
	virtual int 					GetRenderFlags( void );
	virtual const model_t			*GetModel( void ) const;
	virtual int						DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool					LODTest() { return true; }   // NOTE: UNUSED
	virtual void					GetRenderBounds( Vector& mins, Vector& maxs );
	virtual IPVSNotify*				GetPVSNotifyInterface();
	virtual void					GetRenderBoundsWorldspace( Vector& absMins, Vector& absMaxs );

	virtual void					GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );

	// Determine the color modulation amount
	virtual void					GetColorModulation( float* color );

	virtual void OnThreadedDrawSetup() {}
public:
	virtual bool					TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual bool					TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	// To mimic server call convention
	CBaseEntity					*GetOwnerEntity( void ) const;
	void							SetOwnerEntity( CBaseEntity *pOwner );

	CBaseEntity					*GetEffectEntity( void ) const;
	void							SetEffectEntity( CBaseEntity *pEffectEnt );

	bool							IsAbleToHaveFireEffect( void ) const;

	// This function returns a value that scales all damage done by this entity.
	// Use CDamageModifier to hook in damage modifiers on a guy.
	virtual float					GetAttackDamageScale( void );

// IClientNetworkable implementation.
public:
	virtual void					NotifyShouldTransmit( ShouldTransmitState_t state );

	// save out interpolated values
	virtual void					PreDataUpdate( DataUpdateType_t updateType );
	virtual void					PostDataUpdate( DataUpdateType_t updateType );
	virtual void					OnDataUnchangedInPVS();

	virtual void					ValidateModelIndex( void );

	// pvs info. NOTE: Do not override these!!
	virtual void					SetDormant( bool bDormant );
	virtual bool					IsDormant( void ) const;

	virtual void					OnSetDormant( bool bDormant ) {}

	// Tells the entity that it's about to be destroyed due to the client receiving
	// an uncompressed update that's caused it to destroy all entities & recreate them.
	virtual void					SetDestroyedOnRecreateEntities( void );

	virtual int				GetEFlags() const;
	virtual void			SetEFlags( int iEFlags );
	void					AddEFlags( int nEFlagMask );
	void					RemoveEFlags( int nEFlagMask );

	// checks to see if the entity is marked for deletion

	virtual int						entindex( void ) const;
	
	// This works for client-only entities and returns the GetEntryIndex() of the entity's handle,
	// so the sound system can get an IClientEntity from it.
	int GetSoundSourceIndex() const;	

	// Server to client message received
	virtual void					ReceiveMessage( int classID, bf_read &msg );

	virtual void*					GetDataTableBasePtr();

// IClientThinkable.
public:
	// Called whenever you registered for a think message (with SetNextClientThink).
	virtual void					ClientThink();

	virtual ClientThinkHandle_t		GetThinkHandle();
	virtual void					SetThinkHandle( ClientThinkHandle_t hThink );

public:

	void AddVar( void *data, IInterpolatedVar *watcher, int type, bool bSetup=false );
	void RemoveVar( void *data, bool bAssert=true );
	VarMapping_t* GetVarMapping();

	VarMapping_t	m_VarMap;

public:
	// An inline version the game code can use
	CCollisionProperty		*CollisionProp();
	const CCollisionProperty*CollisionProp() const;
	CParticleProperty		*ParticleProp();
	const CParticleProperty *ParticleProp() const;
	CClientAlphaProperty	*AlphaProp();
	const CClientAlphaProperty *AlphaProp() const;

	// Simply here for game shared 
	bool					IsFloating();

	virtual bool					ShouldSavePhysics();

// save/restore stuff
	virtual void					OnSave();
	virtual void					OnRestore();
	// capabilities for save/restore
	virtual int						ObjectCaps( void );
	// only overload these if you have special data to serialize

	bool IsRenderingInFastReflections() const;

private:
	bool ComputeIsRenderingInFastReflections() const;

	// Client code should call this under any circumstances where fast reflection rendering type may change
	void OnFastReflectionRenderingChanged();
	void OnDisableShadowDepthRenderingChanged();
	void OnDisableCSMRenderingChanged();
	void OnShadowDepthRenderingCacheableStateChanged();
	
public:

	// Called after spawn, and in the case of self-managing objects, after load
	virtual bool	CreateVPhysics();

	// Convenience routines to init the vphysics simulation for this object.
	// This creates a static object.  Something that behaves like world geometry - solid, but never moves
	IPhysicsObject *VPhysicsInitStatic( void );

	// This creates a normal vphysics simulated object
	IPhysicsObject *VPhysicsInitNormal( SolidType_t solidType, int nSolidFlags, bool createAsleep, solid_t *pSolid = nullptr );

	// This creates a vphysics object with a shadow controller that follows the AI
	// Move the object to where it should be and call UpdatePhysicsShadowToCurrentPosition()
	IPhysicsObject *VPhysicsInitShadow( bool allowPhysicsMovement, bool allowPhysicsRotation, solid_t *pSolid = nullptr );

private:
	// called by all vphysics inits
	bool			VPhysicsInitSetup();
public:

	void			VPhysicsSetObject( IPhysicsObject *pPhysics );
	void			VPhysicsSwapObject( IPhysicsObject *pSwap );
	// destroy and remove the physics object for this entity
	virtual void	VPhysicsDestroyObject( void );

	// Purpose: My physics object has been updated, react or extract data
	virtual void					VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual void					VPhysicsShadowUpdate( IPhysicsObject *pPhysics ) {}
	virtual int						VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );
	virtual bool					VPhysicsIsFlesh( void );
	virtual void					VPhysicsCompensateForPredictionErrors( const byte *predictedFrame ); //compare your predictive data vs the server data and do something about any discrepancies.

// IClientEntity implementation.
public:
	virtual bool					SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );
	virtual void					SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	virtual bool					UsesFlexDelayedWeights() { return false; }
	virtual void					DoAnimationEvents( void );

	virtual const Vector&			GetAbsOrigin( void ) const;
	virtual const QAngle&			GetAbsAngles( void ) const;  // see also GetVectors()
	inline Vector					Forward() const __restrict__; ///< get my forward (+x) vector
	inline Vector					Left() const __restrict__;    ///< get my left    (+y) vector
	inline Vector					Up() const __restrict__;      ///< get my up      (+z) vector

	const Vector&					GetNetworkOrigin() const;
	const QAngle&					GetNetworkAngles() const;

	void SetNetworkOrigin( const Vector& org );
	void SetNetworkAngles( const QAngle& ang );

	void							SetLocalOrigin( const Vector& origin );
	vec_t							GetLocalOriginDim( int iDim ) const;		// You can use the X_INDEX, Y_INDEX, and Z_INDEX defines here.
	void							SetLocalOriginDim( int iDim, vec_t flValue );

	void							SetLocalAngles( const QAngle& angles );
	vec_t							GetLocalAnglesDim( int iDim ) const;		// You can use the X_INDEX, Y_INDEX, and Z_INDEX defines here.
	void							SetLocalAnglesDim( int iDim, vec_t flValue );

	virtual const Vector&			GetPrevLocalOrigin() const;
	virtual const QAngle&			GetPrevLocalAngles() const;

	// change position, velocity, orientation instantly
	// passing NULL means no change
	virtual void					Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity );

	void							SetLocalTransform( const matrix3x4_t &localTransform );

	void							SetModelName( string_t name );
	string_t						GetModelName( void ) const;

	int								GetModelIndex( void ) const;
	virtual void					SetModelIndex( int index );

	// These methods return a *world-aligned* box relative to the absorigin of the entity.
	// This is used for collision purposes and is *not* guaranteed
	// to surround the entire entity's visual representation
	// NOTE: It is illegal to ask for the world-aligned bounds for
	// SOLID_BSP objects
	virtual const Vector&			WorldAlignMins( ) const;
	virtual const Vector&			WorldAlignMaxs( ) const;

	// This defines collision bounds *in whatever space is currently defined by the solid type*
	//	SOLID_BBOX:		World Align
	//	SOLID_OBB:		Entity space
	//	SOLID_BSP:		Entity space
	//	SOLID_VPHYSICS	Not used
	void							SetCollisionBounds( const Vector& mins, const Vector &maxs );

	// NOTE: These use the collision OBB to compute a reasonable center point for the entity
	virtual const Vector&			WorldSpaceCenter( ) const;

	// FIXME: Do we want this?
	const Vector&					WorldAlignSize( ) const;
	bool							IsPointSized() const;

	// Returns a radius of a sphere 
	// *centered at the world space center* bounding the collision representation 
	// of the entity. NOTE: The world space center *may* move when the entity rotates.
	float							BoundingRadius() const;

	// Used when the collision prop is told to ask game code for the world-space surrounding box
	virtual void					ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Returns the entity-to-world transform
	matrix3x4_t						&EntityToWorldTransform();
	const matrix3x4_t				&EntityToWorldTransform() const;

	// Some helper methods that transform a point from entity space to world space + back
	void							EntityToWorldSpace( const Vector &in, Vector *pOut ) const;
	void							WorldToEntitySpace( const Vector &in, Vector *pOut ) const;

	// This function gets your parent's transform. If you're parented to an attachment,
	// this calculates the attachment's transform and gives you that.
	//
	// You must pass in tempMatrix for scratch space - it may need to fill that in and return it instead of 
	// pointing you right at a variable in your parent.
	matrix3x4_t&					GetParentToWorldTransform( matrix3x4_t &tempMatrix );

	virtual void						GetVectors(Vector* forward, Vector* right, Vector* up) const;

	// Sets abs angles, but also sets local angles to be appropriate
	void							SetAbsOrigin( const Vector& origin );
 	void							SetAbsAngles( const QAngle& angles );

	void							SetDistanceFade( float flMinDist, float flMaxDist );
	void							SetGlobalFadeScale( float flFadeScale );
	float							GetMinFadeDist( ) const;
	float							GetMaxFadeDist( ) const;
	float							GetGlobalFadeScale( ) const;

	virtual SolidType_t				GetSolid( void ) const;
	virtual int			 			GetSolidFlags( void ) const;
	bool							IsSolidFlagSet( int flagMask ) const;
	void							SetSolidFlags( int nFlags );
	void							AddSolidFlags( int nFlags );
	void							RemoveSolidFlags( int nFlags );
	bool							IsSolid() const;

	virtual class CMouthInfo		*GetMouth( void );

	// Retrieve sound spatialization info for the specified sound on this entity
	// Return false to indicate sound is not audible
	virtual bool					GetSoundSpatialization( SpatializationInfo_t& info );

	// Attachments
	virtual int						LookupAttachment( const char *pAttachmentName ) { return -1; }
	virtual bool					GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool					GetAttachment( int number, Vector &origin );
	virtual	bool					GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool					ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter );
	virtual bool					GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );
	virtual void					InvalidateAttachments() {}

	// Team handling
	virtual CTeam					*GetTeam( void );
	virtual int						GetTeamNumber( void ) const;
	virtual int						GetPendingTeamNumber( void ) const;
	virtual void					ChangeTeam( int iTeamNum );			// Assign this entity to a team.
	virtual int						GetRenderTeamNumber( void );
	virtual bool					InSameTeam( CBaseEntity *pEntity );	// Returns true if the specified entity is on the same team as this one
	virtual bool					InLocalTeam( void );

	// ID Target handling
	virtual bool					IsValidIDTarget( void ) { return false; }
	virtual const char      		*GetIDString( void ) { return ""; };

	// See CSoundEmitterSystem
	// Will return the sound guid. If negative, the guid is unknown (call may be successful or not). 0 if the sound was not emitted. Positive if the guid is valid.
	int		EmitSound( const char *soundname, float soundtime = 0.0f, float *duration = nullptr );  // Override for doing the general case of CPASAttenuationFilter( this ), and EmitSound( filter, entindex(), etc. );
	int		EmitSound( const char *soundname, HSOUNDSCRIPTHASH& handle, float soundtime = 0.0f, float *duration = nullptr );  // Override for doing the general case of CPASAttenuationFilter( this ), and EmitSound( filter, entindex(), etc. );
	void	StopSound( const char *soundname );
	void	StopSound( const char *soundname, HSOUNDSCRIPTHASH& handle );
	void	GenderExpandString( char const *in, char *out, int maxlen );

	void UpdateLastMadeNoiseTime( const char* pszSoundName = nullptr );

	static float GetSoundDuration( const char *soundname, char const *actormodel );

	static bool	GetParametersForSound( const char *soundname, CSoundParameters &params, const char *actormodel );
	static bool	GetParametersForSound( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters &params, const char *actormodel );

	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, const Vector *pOrigin = nullptr, float soundtime = 0.0f, float *duration = nullptr );
	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, HSOUNDSCRIPTHASH& handle, const Vector *pOrigin = nullptr, float soundtime = 0.0f, float *duration = nullptr );
	static void StopSound( int iEntIndex, const char *soundname );
	static soundlevel_t LookupSoundLevel( const char *soundname );
	static soundlevel_t LookupSoundLevel( const char *soundname, HSOUNDSCRIPTHASH& handle );

	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params );
	static int EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params, HSOUNDSCRIPTHASH& handle );

	static void StopSound( int iEntIndex, int iChannel, const char *pSample, bool bIsStoppingSpeakerSound = false );

	static void EmitAmbientSound( int entindex, const Vector& origin, const char *soundname, int flags = 0, float soundtime = 0.0f, float *duration = nullptr );

	// These files need to be listed in scripts/game_sounds_manifest.txt
	static HSOUNDSCRIPTHASH PrecacheScriptSound( const char *soundname );
	static void PrefetchScriptSound( const char *soundname );

	// For each client who appears to be a valid recipient, checks the client has disabled CC and if so, removes them from 
	//  the recipient list.
	static void RemoveRecipientsIfNotCloseCaptioning( CRecipientFilter& filter );
	static void EmitCloseCaption( IRecipientFilter& filter, int entindex, char const *token, std::vector< Vector >& soundorigins, float duration, bool warnifmissing = false );

	// Moves all aiments into their correct position for the frame
	static void	MarkAimEntsDirty();
	static void CalcAimEntPositions();

	static bool IsPrecacheAllowed();
	static void SetAllowPrecache( bool allow );

	static bool m_bAllowPrecache;

	static bool IsSimulatingOnAlternateTicks();

public:
	static bool	sm_bAccurateTriggerBboxChecks;	// SOLID_BBOX entities do a fully accurate trigger vs bbox check when this is set

// CBaseEntity local functions
public:

	virtual void UpdatePartitionListEntry();

	// This can be used to setup the entity as a client-only entity. 
	// Override this to perform per-entity clientside setup
	virtual bool InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels );

	// This function gets called on all client entities once per simulation phase. If the entity
	// is in the simulate list.  It dispatches events like OnDataChanged()
	// return false if this entity no longer needs to simulate, true otherwise
	virtual bool					Simulate() { return false; }

	// This event is triggered during the simulation phase if an entity's data has changed. It is 
	// better to hook this instead of PostDataUpdate() because in PostDataUpdate(), server entity origins
	// are incorrect and attachment points can't be used.
	virtual void					OnDataChanged( DataUpdateType_t type );

	// This is called once per frame before any data is read in from the server.
	virtual void					OnPreDataChanged( DataUpdateType_t type );

	bool IsStandable() const;
	bool IsBSPModel() const;

	// If this is a vehicle, returns the vehicle interface

	// Returns the aiment render origin + angles
	virtual void					GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );
	
	// get network origin from previous update
	virtual const Vector&			GetOldOrigin();

	// Methods relating to traversing hierarchy
	CBaseEntity *GetMoveParent( void ) const;
	CBaseEntity *GetRootMoveParent();
	CBaseEntity *FirstMoveChild( void ) const;
	CBaseEntity *NextMovePeer( void ) const;

	inline ClientEntityHandle_t		GetClientHandle() const	{ return ClientEntityHandle_t( m_RefEHandle ); }
	inline bool						IsServerEntity( void ) const;

	void							RenderForceOpaquePass( bool bEnable );
	bool							IsRenderForceOpaquePass() const;
	void							RenderWithViewModels( bool bEnable );
	bool							IsRenderingWithViewModels() const;
	void							DisableCachedRenderBounds( bool bDisabled );
	bool							IsCachedRenderBoundsDisabled() const;

	// NOTE: The goal of this function is different from IsTranslucent().
	// Here, we need to determine whether a renderable is inherently translucent
	// when run-time alpha modulation or any other game code is not taken into account
	virtual RenderableTranslucencyType_t ComputeTranslucencyType( );
	virtual uint8	OverrideAlphaModulation( uint8 nAlpha ) { return nAlpha; }
	virtual uint8	OverrideShadowAlphaModulation( uint8 nAlpha ) { return nAlpha; }
	
	// Client code should call this under any circumstances where translucency type may change
	void							OnTranslucencyTypeChanged();

	// Client code should call this under any circumstances where splitscreen rendering may change
	void							OnSplitscreenRenderingChanged();

	virtual void					GetToolRecordingState( KeyValues *msg );
	virtual void					CleanupToolRecordingState( KeyValues *msg );

	// The value returned by here determines whether or not (and how) the entity
	// is put into the spatial partition.
	virtual CollideType_t			GetCollideType( void );

	virtual bool					ShouldDraw();
	inline	bool					IsVisible() const;
	inline bool						IsVisibleToAnyPlayer() const;
			void					UpdateVisibility();
	
	// Totally sucky, but I can't think of a better way of doing this
	// without changing zillions of ShouldDraw calls.
	virtual bool					ShouldSuppressForSplitScreenPlayer( int nSlot ) { return false; }

	// Returns true if the entity changes its position every frame on the server but it doesn't
	// set animtime. In that case, the client returns true here so it copies the server time to
	// animtime in OnDataChanged and the position history is correct for interpolation.
	virtual bool					IsSelfAnimating();

	// Set appropriate flags and store off data when these fields are about to change
	virtual	void					OnLatchInterpolatedVariables( int flags );
	// For predictable entities, stores last networked value
	void							OnStoreLastNetworkedValue();

	// Initialize things given a new model.
	virtual CStudioHdr				*OnNewModel();
	virtual void					OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void					OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	bool							IsSimulatedEveryTick() const;
	bool							IsAnimatedEveryTick() const;
	void							SetSimulatedEveryTick( bool sim );
	void							SetAnimatedEveryTick( bool anim );

	void							Interp_Reset( VarMapping_t *map );
	virtual void					ResetLatched();
	
	float							GetInterpolationAmount( int flags );
	float							GetLastChangeTime( int flags );

	//Get the time we would pass as an input to our interpolators
	float							GetEffectiveInterpolationCurTime( float currentTime );

	// Interpolate the position for rendering
	virtual bool					Interpolate( float currentTime );

	// Did the object move so far that it shouldn't interpolate?
	bool							Teleported( void );
	// Is this a submodel of the world ( *1 etc. in name ) ( brush models only )
	virtual bool					IsSubModel( void );
	// Deal with EF_* flags
	virtual bool					CreateLightEffects( void );

	void							AddToAimEntsList();
	void							RemoveFromAimEntsList();

	// Reset internal fields
	virtual void					Clear( void );
	// Helper to draw raw brush models
	virtual int						DrawBrushModel( bool bTranslucent, int nFlags, bool bTwoPass );

	// returns the material animation start time
	virtual float					GetTextureAnimationStartTime();
	// Indicates that a texture animation has wrapped
	virtual void					TextureAnimationWrapped();

	// Set the next think time. Pass in CLIENT_THINK_ALWAYS to have Think() called each frame.
	virtual void					SetNextClientThink( float nextThinkTime );

	// anything that has health can override this...
	virtual void					SetHealth(int iHealth) {}
	virtual int						GetHealth() const { return 0; }
	virtual int						GetMaxHealth() const { return 1; }

	// Returns the health fraction
	float							HealthFraction() const;

	// Should this object cast shadows?
	virtual ShadowType_t			ShadowCastType();

	// Should this object receive shadows?
	virtual bool					ShouldReceiveProjectedTextures( int flags );

	// Shadow-related methods
	virtual bool IsShadowDirty( );
	virtual void MarkShadowDirty( bool bDirty );
	virtual IClientRenderable *GetShadowParent();
	virtual IClientRenderable *FirstShadowChild();
	virtual IClientRenderable *NextShadowPeer();

	// Sets up a render handle so the leaf system will draw this entity.
	void							AddToLeafSystem();
	void							AddToLeafSystem( bool bRenderWithViewModels );
	// remove entity form leaf system again
	void							RemoveFromLeafSystem();	

	// A method to apply a decal to an entity
	virtual void					AddDecal( const Vector& rayStart, const Vector& rayEnd,
										const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal = ADDDECAL_TO_ALL_LODS, const Vector *saxis = nullptr, int nAdditionalDecalFlags = 0 );
	// A method to remove all decals from an entity
	void							RemoveAllDecals( void );

	// Is this a brush model?
	bool							IsBrushModel() const;

	// A random value 0-1 used by proxies to make sure they're not all in sync
	float							ProxyRandomValue() const { return m_flProxyRandomValue; }
	void							SetProxyRandomValue( float flNewValue ) { m_flProxyRandomValue = flNewValue; }

	// The spawn time of this entity
	float							SpawnTime() const { return m_flSpawnTime; }

	virtual bool					IsClientCreated( void ) const;

	virtual void					UpdateOnRemove( void );

	virtual void					SUB_Remove( void );

	// Prediction stuff
	/////////////////
	void							CheckInitPredictable( const char *context );
	void							CheckShutdownPredictable( const char *context );
	virtual CBasePlayer			*GetPredictionOwner( void );

	void							AllocateIntermediateData( void );
	void							DestroyIntermediateData( void );
	void							ShiftIntermediateDataForward( int slots_to_remove, int previous_last_slot );
	void							ShiftFirstPredictedIntermediateDataForward( int slots_to_remove );
	void							ShiftIntermediateData_TickAdjust( int delta, int last_slot );

	void							*GetPredictedFrame( int framenumber );
	void							*GetFirstPredictedFrame( int framenumber ); //similar to GetPredictedFrame() but only stores the results from the first prediction of each command
	void							GetUnacknowledgedPredictedFrameRange( int &iStart, int &iEnd ); //Get the range of predicted frames we may restore from at any moment.
	void							*GetOriginalNetworkDataObject( void );
	bool							IsIntermediateDataAllocated( void ) const;

	virtual void					InitPredictable( CBasePlayer *pOwner );
	void							ShutdownPredictable( void );
	int								GetSplitUserPlayerPredictionSlot();

	virtual void					SetPredictable( bool state );
	bool							GetPredictable( void ) const;
	void							PreEntityPacketReceived( int commands_acknowledged );
	void							PostEntityPacketReceived( void );
	bool							PostNetworkDataReceived( int commands_acknowledged );
	virtual void					HandlePredictionError( bool bErrorInThisEntity ); //we just processed a network update with errors, bErrorInThisEntity is false if the prediction errors were entirely in other entities and not this one
	virtual bool					PredictionErrorShouldResetLatchedForAllPredictables( void ) { return true; } //legacy behavior is that any prediction error causes all predictables to reset latched
	virtual bool					PredictionIsPhysicallySimulated( void ) { return false; } //by default, all prediction is driven by player commands, return true if your prediction is based on server simulation ticks
	bool							GetPredictionEligible( void ) const;
	void							SetPredictionEligible( bool canpredict );

	enum
	{
		SLOT_ORIGINALDATA = -1,
	};

	void							SaveData( const char *context, int slot, int type );
	void							RestoreData( const char *context, int slot, int type );

	// Called after restoring data into prediction slots. This function is used in place of proxies
	// on the variables, so if some variable like m_nModelIndex needs to update other state (like 
	// the model pointer), it is done here.
	void OnPostRestoreData();

	virtual char const *			DamageDecal( int bitsDamageType, int gameMaterial );
	virtual void					DecalTrace( trace_t *pTrace, char const *decalName );
	virtual void					ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	virtual bool					ShouldPredict( void ) { return false; };
	
	// interface function pointers
	union{
		// Doing this lame union to force the compiler to properly align the
		// member function pointer to at least 16-bytes for warning control.
		void							(CBaseEntity::*m_pfnThink)(void);
		char						__dont_use_me_pad[16];
	};
	virtual void					Think( void );

	void							PhysicsDispatchThink( BASEPTR thinkFunc );
	
	// Toggle the visualization of the entity's abs/bbox
	enum
	{
		VISUALIZE_COLLISION_BOUNDS		= 0x1,
		VISUALIZE_SURROUNDING_BOUNDS	= 0x2,
		VISUALIZE_RENDER_BOUNDS			= 0x4,
	};

	void							ClearBBoxVisualization( void );
	void							ToggleBBoxVisualization( int fVisFlags );
	void							DrawBBoxVisualizations( void );

	virtual bool					PreRender( int nSplitScreenPlayerSlot );

	bool							IsViewEntity() const;

// Methods implemented on both client and server
public:
	void							SetSize( const Vector &vecMin, const Vector &vecMax ); // UTIL_SetSize( pev, mins, maxs );
	virtual char const				*GetClassname( void );
	char const						*GetDebugName( void );
	virtual const char				*GetPlayerName() const { return nullptr; }
	static int						PrecacheModel( const char *name ); 
	static bool						PrecacheSound( const char *name );
	static void						PrefetchSound( const char *name );
	void							Remove( ); // UTIL_Remove( this );

public:

	// Returns the attachment point index on our parent that our transform is relative to.
	// 0 if we're relative to the parent's absorigin and absangles.
	unsigned char			GetParentAttachment() const;

	// Externalized data objects ( see sharreddefs.h for DataObjectType_t )
	bool					HasDataObjectType( int type ) const;
	void					AddDataObjectType( int type );
	void					RemoveDataObjectType( int type );

	void					*GetDataObject( int type );
	void					*CreateDataObject( int type );
	void					DestroyDataObject( int type );
	void					DestroyAllDataObjects( void );

	// Determine approximate velocity based on updates from server
	virtual void			EstimateAbsVelocity( Vector& vel );

	// The player drives simulation of this entity
	void					SetPlayerSimulated( CBasePlayer *pOwner );
	bool					IsPlayerSimulated( void ) const;
	CBasePlayer				*GetSimulatingPlayer( void );
	void					UnsetPlayerSimulated( void );

	// Sorry folks, here lies TF2-specific stuff that really has no other place to go
	virtual bool			CanBePoweredUp( void ) { return false; }
	virtual bool			AttemptToPowerup( int iPowerup, float flTime, float flAmount = 0, CBaseEntity *pAttacker = nullptr, CDamageModifier *pDamageModifier = nullptr ) { return false; }

	void					SetCheckUntouch( bool check );
	bool					GetCheckUntouch() const;

	virtual bool			IsCurrentlyTouching( void ) const;

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );

	void (CBaseEntity ::*m_pfnTouch)( CBaseEntity *pOther );

	void					PhysicsStep( void );

protected:
	static bool				sm_bDisableTouchFuncs;	// Disables PhysicsTouch and PhysicsStartTouch function calls

	[[= ks::reflect::Net{} ]] bool					m_bIsAutoaimTarget;

public:
	touchlink_t				*PhysicsMarkEntityAsTouched( CBaseEntity *other );
	void					PhysicsTouch( CBaseEntity *pentOther );
	void					PhysicsStartTouch( CBaseEntity *pentOther );

	// HACKHACK:Get the trace_t from the last physics touch call (replaces the even-hackier global trace vars)
	static const trace_t	&GetTouchTrace( void );

	// FIXME: Should be private, but I can't make em private just yet
	void					PhysicsImpact( CBaseEntity *other, trace_t &trace );
 	void					PhysicsMarkEntitiesAsTouching( CBaseEntity *other, trace_t &trace );
	void					PhysicsMarkEntitiesAsTouchingEventDriven( CBaseEntity *other, trace_t &trace );
	void					PhysicsTouchTriggers( const Vector *pPrevAbsOrigin = nullptr );

	// Physics helper
	static void				PhysicsRemoveTouchedList( CBaseEntity *ent );
	static void				PhysicsNotifyOtherOfUntouch( CBaseEntity *ent, CBaseEntity *other );
	static void				PhysicsRemoveToucher( CBaseEntity *other, touchlink_t *link );

	groundlink_t			*AddEntityToGroundList( CBaseEntity *other );
	void					PhysicsStartGroundContact( CBaseEntity *pentOther );

	static void				PhysicsNotifyOtherOfGroundRemoval( CBaseEntity *ent, CBaseEntity *other );
	static void				PhysicsRemoveGround( CBaseEntity *other, groundlink_t *link );
	static void				PhysicsRemoveGroundList( CBaseEntity *ent );

	void					StartGroundContact( CBaseEntity *ground );
	void					EndGroundContact( CBaseEntity *ground );

	void					SetGroundChangeTime( float flTime );
	float					GetGroundChangeTime( void );

	// Remove this as ground entity for all object resting on this object
	void					WakeRestingObjects();
	bool					HasNPCsOnIt();

	bool					PhysicsCheckWater( void );
	void					PhysicsCheckVelocity( void );
	void					PhysicsAddHalfGravity( float timestep );
	void					PhysicsAddGravityMove( Vector &move );

	virtual unsigned int	PhysicsSolidMaskForEntity( void ) const;

	void					SetGroundEntity( CBaseEntity *ground );
	CBaseEntity			*GetGroundEntity( void );
	CBaseEntity			*GetGroundEntity( void ) const { return const_cast<CBaseEntity *>(this)->GetGroundEntity(); }

	void					PhysicsPushEntity( const Vector& push, trace_t *pTrace );
	void					PhysicsCheckWaterTransition( void );

	// Performs the collision resolution for fliers.
	void					PerformFlyCollisionResolution( trace_t &trace, Vector &move );
	void					ResolveFlyCollisionBounce( trace_t &trace, Vector &vecVelocity, float flMinTotalElasticity = 0.0f );
	void					ResolveFlyCollisionSlide( trace_t &trace, Vector &vecVelocity );
	void					ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );

	void					PhysicsCheckForEntityUntouch( void );

	// Creates the shadow (if it doesn't already exist) based on shadow cast type
	void					CreateShadow();

	// Destroys the shadow; causes its type to be recomputed if the entity doesn't go away immediately.
	void					DestroyShadow();

protected:
		// think function handling
	enum thinkmethods_t
	{
		THINK_FIRE_ALL_FUNCTIONS,
		THINK_FIRE_BASE_ONLY,
		THINK_FIRE_ALL_BUT_BASE,
	};
public:

	// Unlinks from hierarchy
	// Set the movement parent. Your local origin and angles will become relative to this parent.
	// If iAttachment is a valid attachment on the parent, then your local origin and angles 
	// are relative to the attachment on this entity.
	void SetParent( CBaseEntity *pParentEntity, int iParentAttachment=0 );

	bool					PhysicsRunThink( thinkmethods_t thinkMethod = THINK_FIRE_ALL_FUNCTIONS );
	bool					PhysicsRunSpecificThink( int nContextIndex, BASEPTR thinkFunc );

	virtual void					PhysicsSimulate( void );
	virtual bool					IsAlive( void )const;

	bool							IsInWorld( void ) { return true; }

	bool							IsWorld() const { return entindex() == 0; }
	/////////////////

	virtual bool 					ShouldRegenerateOriginFromCellBits() const;

	virtual bool					IsPlayer( void ) const { return false; };
	virtual bool					IsWeaponWorldModel( void ) const { return false; };
	virtual bool					IsBaseCombatCharacter( void ) { return false; };
	//virtual bool					IsGrenadeProjectile( void ) { return false; };
	virtual CBaseCombatCharacter	*MyCombatCharacterPointer( void ) { return nullptr; }
	virtual bool					IsNPC( void ) { return false; }

	virtual bool					IsSprite( void ) const { return false; }
	virtual bool					IsProp( void ) const { return false; }

	// TF2 specific
	virtual bool					IsBaseObject( void ) const { return false; }
	virtual bool					IsBaseCombatWeapon( void ) const { return false; }
	virtual class CBaseCombatWeapon		*MyCombatWeaponPointer() { return nullptr; }

	// Entities like the player, weapon models, and view models have special logic per-view port related to visibility and the model to be used, etc.
	virtual bool					ShouldDrawForSplitScreenUser( int nSlot );
	void							SetBlurState( bool bShouldBlur );
	virtual bool					IsBlurred( void );

	virtual bool					IsBaseTrain( void ) const { return false; }

	// Returns the eye point + angles (used for viewing + shooting)
	virtual Vector			EyePosition( void );
	virtual const QAngle&	EyeAngles( void );		// Direction of eyes
	virtual const QAngle&	LocalEyeAngles( void );	// Direction of eyes in local space (pl.v_angle)
	
	// position of ears
	virtual Vector		EarPosition( void );

	Vector	EyePosition( void ) const;			// position of eyes
	const QAngle &EyeAngles( void ) const;		// Direction of eyes in world space
	const QAngle &LocalEyeAngles( void ) const;	// Direction of eyes
	Vector	EarPosition( void ) const;			// position of ears

	// Called by physics to see if we should avoid a collision test....
	virtual bool		ShouldCollide( int collisionGroup, int contentsMask ) const;

	// Sets physics parameters
	void				SetFriction( float flFriction );


	// Sets the model from a model index 
	void				SetModelByIndex( int nModelIndex );

	// Set model... (NOTE: Should only be used by client-only entities
	// Returns false if the model name is bogus
	bool				SetModel( const char *pModelName );

	virtual void		SetModelPointer( const model_t *pModel );

	// Access movetype and solid.
	void				SetMoveType( MoveType_t val, MoveCollide_t moveCollide = MOVECOLLIDE_DEFAULT );	// Set to one of the MOVETYPE_ defines.
	void				SetSolid( SolidType_t val );	// Set to one of the SOLID_ defines.

	// NOTE: Setting the abs velocity in either space will cause a recomputation
	// in the other space, so setting the abs velocity will also set the local vel
	void				SetLocalVelocity( const Vector &vecVelocity );
	void				SetAbsVelocity( const Vector &vecVelocity );
	const Vector&		GetAbsVelocity( ) const;

	void				ApplyLocalVelocityImpulse( const Vector &vecImpulse );
	void				ApplyAbsVelocityImpulse( const Vector &vecImpulse );
	void				ApplyLocalAngularVelocityImpulse( const AngularImpulse &angImpulse );

	// NOTE: Setting the abs velocity in either space will cause a recomputation
	// in the other space, so setting the abs velocity will also set the local vel
	void				SetLocalAngularVelocity( const QAngle &vecAngVelocity );

//	void				SetAbsAngularVelocity( const QAngle &vecAngAbsVelocity );
//	const QAngle&		GetAbsAngularVelocity( ) const;


	virtual const Vector &GetViewOffset() const;
	virtual void		  SetViewOffset( const Vector& v );

	virtual void		GetGroundVelocityToApply( Vector &vecGroundVel ) { vecGroundVel = vec3_origin; }

	// Invalidates the abs state of all children
	void InvalidatePhysicsRecursive( int nChangeFlags );

	ClientRenderHandle_t	GetRenderHandle() const;

	void				SetRemovalFlag( bool bRemove );

	bool				HasSpawnFlags( int nFlags ) const;

	// Effects...
	bool				IsEffectActive( int nEffectMask ) const;
	void				AddEffects( int nEffects );
	void				RemoveEffects( int nEffects );
	int					GetEffects( void ) const;
	void				ClearEffects( void );
	void				SetEffects( int nEffects );

	// Computes the abs position of a point specified in local space
	void				ComputeAbsPosition( const Vector &vecLocalPosition, Vector *pAbsPosition );

	// Computes the abs position of a direction specified in local space
	void				ComputeAbsDirection( const Vector &vecLocalDirection, Vector *pAbsDirection );

	// These methods encapsulate MOVETYPE_FOLLOW, which became obsolete
	void				FollowEntity( CBaseEntity *pBaseEntity, bool bBoneMerge = true );
	void				StopFollowingEntity( );	// will also change to MOVETYPE_NONE
	virtual bool		IsFollowingEntity();
	CBaseEntity			*GetFollowedEntity();

	// For shadows rendering the correct body + sequence...
	virtual int GetBody() { return 0; }
	virtual int GetSkin() { return 0; }

	const Vector &ScriptGetForward( void ) { static Vector vecForward; GetVectors( &vecForward, nullptr, nullptr ); return vecForward; }
	const Vector &ScriptGetLeft( void ) { static Vector vecLeft; GetVectors( nullptr, &vecLeft, nullptr ); return vecLeft; }
	const Vector &ScriptGetUp( void ) { static Vector vecUp; GetVectors( nullptr, nullptr, &vecUp ); return vecUp; }

	// Stubs on client
	void	NetworkStateManualMode( bool activate )		{ }
	void	NetworkStateChanged()						{ }
	void	NetworkStateChanged( void *pVar )			{ }
	void	NetworkStateSetUpdateInterval( float N )	{ }
	void	NetworkStateForceUpdate()					{ }

	// Think functions with contexts
	int		RegisterThinkContext( const char *szContext );
	BASEPTR	ThinkSet( BASEPTR func, float flNextThinkTime = 0, const char *szContext = nullptr );
	void	SetNextThink( float nextThinkTime, const char *szContext = nullptr );
	float	GetNextThink( const char *szContext = nullptr );
	float	GetLastThink( const char *szContext = nullptr );
	int		GetNextThinkTick( const char *szContext = nullptr );
	int		GetLastThinkTick( const char *szContext = nullptr );
	
	// These set entity flags (EFL_*) to help optimize queries
	void	CheckHasThinkFunction( bool isThinkingHint = false );
	void	CheckHasGamePhysicsSimulation();
	bool	WillThink();
	bool	WillSimulateGamePhysics();
	int		GetFirstThinkTick();	// get first tick thinking on any context

	float	GetAnimTime() const;
	void	SetAnimTime( float at );

	float	GetSimulationTime() const;
	void	SetSimulationTime( float st );

	int		GetCreationTick() const;

	// Gets the model instance + shadow handle
	virtual ModelInstanceHandle_t GetModelInstance() { return m_ModelInstance; }
	void SetModelInstance( ModelInstanceHandle_t hInstance) { m_ModelInstance = hInstance; }
	bool SnatchModelInstance( CBaseEntity * pToEntity );
	virtual ClientShadowHandle_t GetShadowHandle() const	{ return m_ShadowHandle; }
	virtual ClientRenderHandle_t&	RenderHandle();

	void CreateModelInstance();

	// Sets the origin + angles to match the last position received
	void MoveToLastReceivedPosition( bool force = false );

	bool IsAutoaimTarget( void ) const;

protected:
	// Only meant to be called from subclasses
	void DestroyModelInstance();

	// Interpolate entity
	static void ProcessTeleportList();
	static void ProcessInterpolatedList();
	static void CheckInterpolatedVarParanoidMeasurement();

	// overrideable rules if an entity should interpolate
	virtual bool ShouldInterpolate();

	// Call this in OnDataChanged if you don't chain it down!
	void MarkMessageReceived();

	// Gets the last message time
	float	GetLastMessageTime() const		{ return m_flLastMessageTime; }

	// For non-players
	int	PhysicsClipVelocity (const Vector& in, const Vector& normal, Vector& out, float overbounce );

protected:
	// Two part guts of Interpolate(). Shared with CBaseAnimating.
	enum
	{
		INTERPOLATE_STOP=0,
		INTERPOLATE_CONTINUE
	};

	// Returns INTERPOLATE_STOP or INTERPOLATE_CONTINUE.
	// bNoMoreChanges is set to 1 if you can call RemoveFromInterpolationList on the entity.
	int BaseInterpolatePart1( float &currentTime, Vector &oldOrigin, QAngle &oldAngles, int &bNoMoreChanges );
	void BaseInterpolatePart2( Vector &oldOrigin, QAngle &oldAngles, int nChangeFlags );

public:
	// Accessors for above
	static int						GetPredictionRandomSeed( void );
	static void						SetPredictionRandomSeed( const CUserCmd *cmd );
	static CBasePlayer				*GetPredictionPlayer( void );
	static void						SetPredictionPlayer( CBasePlayer *player );
	static void						CheckCLInterpChanged();

	// Collision group accessors
	int GetCollisionGroup() const;
	void SetCollisionGroup( int collisionGroup );
	void							CollisionRulesChanged();

	static CBaseEntity				*Instance( int iEnt );
	// Doesn't do much, but helps with trace results
	static CBaseEntity				*Instance( IClientEntity *ent );
	static CBaseEntity				*Instance( CBaseHandle hEnt );
	// For debugging shared code
	static bool						IsServer( void );
	static bool						IsClient( void );
	static char const				*GetDLLType( void );
	static void	SetAbsQueriesValid( bool bValid );
	static bool IsAbsQueriesValid( void );

	// Enable/disable abs recomputations on a stack.
	static void PushEnableAbsRecomputations( bool bEnable );
	static void PopEnableAbsRecomputations();

	// This requires the abs recomputation stack to be empty and just sets the global state. 
	// It should only be used at the scope of the frame loop.
	static void EnableAbsRecomputations( bool bEnable );		
	
	static bool IsAbsRecomputationsEnabled( void );

	static void PreRenderEntities( int nSplitScreenPlayerSlot );
	static void PurgeRemovedEntities();
	static void SimulateEntities();

	//A queue that processes after we simulate all client entities. For operations that modify the simulate list (portal ghostrenderables)
	static CCallQueue *				GetSimulateCallQueue( void );

	// Bloat the culling bbox past the parent ent's bbox in local space if EF_BONEMERGE_FASTCULL is set.
	virtual void BoneMergeFastCullBloat( Vector &localMins, Vector &localMaxs, const Vector &thisEntityMins, const Vector &thisEntityMaxs ) const;

	// Accessors for color.
	const color24 GetRenderColor() const;
	byte GetRenderColorR() const;
	byte GetRenderColorG() const;
	byte GetRenderColorB() const;
	byte GetRenderAlpha() const;
	void SetRenderColor( byte r, byte g, byte b );
	void SetRenderColorR( byte r );
	void SetRenderColorG( byte g );
	void SetRenderColorB( byte b );
	void SetRenderAlpha( byte a );

	void SetRenderMode( RenderMode_t nRenderMode, bool bForceUpdate = false );
	RenderMode_t GetRenderMode() const;

	void SetRenderFX( RenderFx_t nRenderFX, float flStartTime = std::numeric_limits<float>::max(), float flDuration = 0.0f );
	RenderFx_t GetRenderFX() const;

	// Returns true if there was a change.
	bool SetCellBits( int cellbits = CELL_BASEENTITY_ORIGIN_CELL_BITS );

	static void RecvProxy_CellBits( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_CellX( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_CellY( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_CellZ( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_CellOrigin( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_CellOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxy_CellOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void RecvProxyOldSpottedByMask( const CRecvProxyData *pData, void *pStruct, void *pOut );

	const char	*GetEntityName();

public:	

	// Determine what entity this corresponds to
	int								index;	

	// Entity flags that are only for the client (ENTCLIENTFLAG_ defines).
	unsigned short					m_EntClientFlags;

private:
	
	// Model for rendering
	const model_t					*model;
	CNetworkColor32( m_clrRender, [[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_ClrRender, ks::reflect::WireSide::Recv>{} ]] );

public:
protected: // Cell data is available to derived classes for RecvProxy issues
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<CBaseEntity::RecvProxy_CellBits, ks::reflect::WireSide::Recv>{} ]] int								m_cellbits;
	int								m_cellwidth;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<CBaseEntity::RecvProxy_CellX, ks::reflect::WireSide::Recv>{} ]] int								m_cellX;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<CBaseEntity::RecvProxy_CellY, ks::reflect::WireSide::Recv>{} ]] int								m_cellY;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<CBaseEntity::RecvProxy_CellZ, ks::reflect::WireSide::Recv>{} ]] int								m_cellZ;
	Vector							m_vecCellOrigin; // cached cell offset position
// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
// FTYPEDESC_INSENDTABLE STUFF
private:
	// A random value used by material proxies for each model instance.
	float							m_flProxyRandomValue;

	// For client/server entities, true if the entity goes outside the PVS.
	// Unused for client only entities.
	bool							m_bDormant;

	bool							m_bCanUseBrushModelFastPath;

// FTYPEDESC_INSENDTABLE STUFF (end)
private:
	// Effects to apply
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_EffectFlags, ks::reflect::WireSide::Recv>{} ]] int								m_fEffects;
public:
	// Team Handling
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] int								m_iTeamNum;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] int								m_iPendingTeamNum;
	int								m_nNextThinkTick;
	int								m_iHealth;
private:
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK } ]] [[= ks::reflect::Net{ .wire = "m_angRotation" } ]] QAngle							m_angNetworkAngles;

	// Last values to come over the wire. Used for interpolation.
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = coordTolerance } ]] [[= ks::reflect::Net{ .wire = "m_vecOrigin" } ]] [[= ks::reflect::Proxy<CBaseEntity::RecvProxy_CellOrigin, ks::reflect::WireSide::Recv>{} ]] Vector							m_vecNetworkOrigin;

	// The moveparent received from networking data
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int, .wire = "moveparent" } ]] [[= ks::reflect::Proxy<RecvProxy_IntToMoveParent, ks::reflect::WireSide::Recv>{} ]] CHandle<CBaseEntity>			m_hNetworkMoveParent;
	// The owner!
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] EHANDLE							m_hOwnerEntity;

	[[= ks::reflect::Net{} ]] char							m_iName[MAX_PATH];

public:
	// Object model index
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } ]] [[= ks::reflect::Net{} ]] short							m_nModelIndex;
private:
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] unsigned char					m_nRenderFX;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Net{} ]] unsigned char 					m_nRenderMode;

public:
	char							m_lifeState;

// END PREDICTION DATA COMPACTION
public:

public:
	// Time animation sequence or frame was last changed
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int, .table = "DT_AnimTimeMustBeFirst" } ]] [[= ks::reflect::Proxy<RecvProxy_AnimTime, ks::reflect::WireSide::Recv>{} ]] float							m_flAnimTime;
	float							m_flOldAnimTime;

	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Proxy<RecvProxy_SimulationTime, ks::reflect::WireSide::Recv>{} ]] float							m_flSimulationTime;
	float							m_flOldSimulationTime;
	
private:
	unsigned char 					m_nOldRenderMode;

public:
	// Used to store the state we were added to the BSP as, so it can
	// reinsert the entity if the state changes.
	ClientRenderHandle_t			m_hRender;	// link into spatial partition
	std::bitset< MAX_SPLITSCREEN_PLAYERS > m_VisibilityBits;

	// Interpolation says don't draw yet
	bool							m_bReadyToDraw;
	bool							m_bClientSideRagdoll;

	// Should we be interpolating?
	static bool						IsInterpolationEnabled();

	// 

	int								m_nLastThinkTick;

	char							m_takedamage;

	// was pev->speed
	float							m_flSpeed;

	// Certain entities (projectiles) can be created on the client

	// used so we know when things are no longer touching
	int								touchStamp;	

	// Called after predicted entity has been acknowledged so that no longer needed entity can
	//  be deleted
	// Return true to force deletion right now, regardless of isbeingremoved
	virtual bool					OnPredictedEntityRemove( bool isbeingremoved, CBaseEntity *predicted );

	bool							IsDormantPredictable( void ) const;
	bool							BecameDormantThisPacket( void ) const;
	void							SetDormantPredictable( bool dormant );

	float							GetElasticity( void ) const;

	int								GetTextureFrameIndex( void );
	void							SetTextureFrameIndex( int iIndex );

	virtual bool					GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const;
	virtual bool					GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;
	virtual CBaseEntity 			*GetShadowUseOtherEntity( void ) const;
	virtual void					SetShadowUseOtherEntity( CBaseEntity *pEntity );

	CDiscontinuousInterpolatedVar< QAngle >& GetRotationInterpolator();
	CDiscontinuousInterpolatedVar< Vector >& GetOriginInterpolator();
	virtual bool					AddRagdollToFadeQueue( void ) { return true; }

	// Dirty bits
	void							MarkRenderHandleDirty();

	// used by SourceTV since move-parents may be missing when child spawns.
	void							HierarchyUpdateMoveParent();

	void							SetCPULevels( int nMinCPULevel, int nMaxCPULevel );
	void							SetGPULevels( int nMinGPULevel, int nMaxGPULevel );
	int								GetMinCPULevel( ) const;
	int								GetMaxCPULevel( ) const;
	int								GetMinGPULevel( ) const;
	int								GetMaxGPULevel( ) const;

protected:
	// FIXME: Should I move the functions handling these out of C_ClientEntity
	// and into CBaseEntity? Then we could make these private.
	// Client handle

private:
	// Set by tools if this entity should route "info" to various tools listening to HTOOLENTITIES
	bool							m_bEnabledInToolView;
	bool							m_bToolRecording;
	HTOOLHANDLE						m_ToolHandle;
	int								m_nLastRecordedFrame;
	bool							m_bRecordInTools; // should this entity be recorded in the tools (we exclude some things like models for menus)

protected:
	// pointer to the entity's physics object (vphysics.dll)

	bool							m_bPredictionEligible;

	// Think contexts
	int								GetIndexForThinkContext( const char *pszContext );
	std::vector< thinkfunc_t >		m_aThinkFunctions;
	int								m_iCurrentThinkContext;

	int								m_spawnflags;

	// Allow studio models to tell us what their m_nBody value is
	virtual int						GetStudioBody( void ) { return 0; }
	// call this in postdataupdate to detect hierarchy changes
	bool							IsParentChanging();

private:
	friend void OnRenderStart();

	// This can be used to setup the entity as a client-only entity. It gets an entity handle,
	// a render handle, and is put into the spatial partition.
	bool InitializeAsClientEntityByIndex( int iIndex, bool bRenderWithViewModels );

	// Figure out the smoothly interpolated origin for all server entities. Happens right before
	// letting all entities simulate.
	static void InterpolateServerEntities();
	
	// Check which entities want to be drawn and add them to the leaf system.
	static void	AddVisibleEntities();

	// Computes the base velocity
	void UpdateBaseVelocity( void );

	// Physics-related private methods
	void PhysicsPusher( void );
	void PhysicsNone( void );
	void PhysicsNoclip( void );
	void PhysicsParent( void );
	void PhysicsStepRunTimestep( float timestep );
	void PhysicsToss( void );
	void PhysicsCustom( void );

	// Simulation in local space of rigid children
	void PhysicsRigidChild( void );

	// Computes absolute position based on hierarchy
	void CalcAbsolutePosition( );
	void CalcAbsoluteVelocity();

	// Computes new angles based on the angular velocity
	void SimulateAngles( float flFrameTime );

	// Implement this if you use MOVETYPE_CUSTOM
	virtual void PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity );

	// methods related to decal adding
	void AddStudioDecal( const Ray_t& ray, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal = ADDDECAL_TO_ALL_LODS, int nAdditionalDecalFlags = 0 );
	void AddBrushModelDecal( const Ray_t& ray, const Vector& decalCenter, int decalIndex, bool doTrace, trace_t& tr, const Vector *saxis = nullptr, int nAdditionalDecalFlags = 0 );
public:
	void AddStudioMaterialDecal( const Ray_t& ray, IMaterial *pDecalMaterial, float flInputRadius, Vector vec_up );

	const float	GetUseLookAtAngle( void ) { return m_flUseLookAtAngle; }
private:

	void ComputePackedOffsets( void );
	int GetIntermediateDataSize( void );

	void UnlinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
	void LinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
	void HierarchySetParent( CBaseEntity *pNewParent );
	void UnlinkFromHierarchy();

	// Computes the water level + type
	void UpdateWaterState();

	// Checks a sweep without actually performing the move
	void PhysicsCheckSweep( const Vector& vecAbsStart, const Vector &vecAbsDelta, trace_t *pTrace );

	// FIXME: REMOVE!!!
	void MoveToAimEnt( );
	
	// Sets/Gets the next think based on context index
	void SetNextThink( int nContextIndex, float thinkTime );
	void SetLastThink( int nContextIndex, float thinkTime );
	float GetNextThink( int nContextIndex ) const;
	int	GetNextThinkTick( int nContextIndex ) const;

	void CleanUpAlphaProperty();

	[[= ks::reflect::Net{} ]] float							m_flUseLookAtAngle;

	// It's still in the list for "fixup purposes" and simulation, but don't try to render it any more...
	bool							m_bDormantPredictable;

	// So we can clean it up
	int								m_nIncomingPacketEntityBecameDormant;

	// The spawn time of the entity
	float							m_flSpawnTime;

	// Timestamp of message arrival
	float							m_flLastMessageTime;

	// Model instance data..
	ModelInstanceHandle_t			m_ModelInstance;

	// Shadow data
	ClientShadowHandle_t			m_ShadowHandle;
	std::bitset< MAX_SPLITSCREEN_PLAYERS > m_ShadowBits; // Per-splitscreen user shadow visibility bits

	ClientThinkHandle_t				m_hThink;

	[[= ks::reflect::Net{} ]] unsigned char					m_iParentAttachment; // 0 if we're relative to the parent's absorigin and absangles.
	unsigned char					m_iOldParentAttachment;

	// Prediction system
	bool							m_bPredictable;
	bool							m_bRenderWithViewModels;
	bool							m_bDisableCachedRenderBounds;
protected:
	// NOTE: This is a hack for portal2.  There is a piece of networking code in OnDataChangedInPVS() that slams m_flSimulationTime
	// this causes interpolation bugs on remote players in p2
	bool							m_bDisableSimulationFix;

	// Fades
	[[= ks::reflect::Net{} ]] float							m_fadeMinDist;
	[[= ks::reflect::Net{} ]] float							m_fadeMaxDist;
	[[= ks::reflect::Net{} ]] float							m_flFadeScale;

public:

	void							OnSimulationTimeChanging( float flPreviousSimulationTime, float flNextSimulationTime );

private:
	int								m_nSplitUserPlayerPredictionSlot;

	// Hierarchy
	CHandle<CBaseEntity>			m_pMoveParent;
	CHandle<CBaseEntity>			m_pMoveChild;
	CHandle<CBaseEntity>			m_pMovePeer;
	CHandle<CBaseEntity>			m_pMovePrevPeer;
	CHandle<CBaseEntity>			m_hOldMoveParent;

	string_t						m_ModelName;

	CNetworkVarEmbedded( CCollisionProperty, m_Collision, [[= ks::reflect::Pred{} ]] [[= ks::reflect::Net{} ]] );
	CNetworkVarEmbedded( CParticleProperty, m_Particles );
	CClientAlphaProperty			*m_pClientAlphaProperty;

	// Physics state
	[[= ks::reflect::Net{} ]] float							m_flElasticity;

	[[= ks::reflect::Net{} ]] float							m_flShadowCastDistance;
	EHANDLE							m_ShadowDirUseOtherEntity;

	Vector							m_vecOldOrigin;
	QAngle							m_vecOldAngRotation;

	CDiscontinuousInterpolatedVar< Vector >		m_iv_vecOrigin;
	CDiscontinuousInterpolatedVar< QAngle >		m_iv_angRotation;

	// Specifies the entity-to-world transform

	// used to cull collision tests
	[[= ks::reflect::Net{} ]] int								m_CollisionGroup;

	// For storing prediction results and pristine network state
	byte							*m_pIntermediateData[ MULTIPLAYER_BACKUP ] = {};
	byte							*m_pIntermediateData_FirstPredicted[ MULTIPLAYER_BACKUP + 1 ] = {};
	byte							*m_pOriginalData = nullptr;
	int								m_nIntermediateDataCount = -1;
	static int						s_nIncomingPacketCommandsAcknowledged; //only set to a valid value during entity network update processing
	int								m_nIntermediateData_FirstPredictedShiftMarker = -1; //can't use predicted commands to optimize first predicted version of ShiftIntermediateDataForward(). Use this instead for its longer lifetime
	bool							m_bEverHadPredictionErrorsForThisCommand = false;

	bool							m_bIsPlayerSimulated = false;

	CNetworkVar( bool, m_bSimulatedEveryTick, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Proxy<RecvProxy_InterpolationAmountChanged, ks::reflect::WireSide::Recv>{} ]] );
	CNetworkVar( bool, m_bAnimatedEveryTick, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Proxy<RecvProxy_InterpolationAmountChanged, ks::reflect::WireSide::Recv>{} ]] );
	CNetworkVar( bool, m_bAlternateSorting, [[= ks::reflect::Net{} ]] );
	//CNetworkVar( bool, m_bSpotted );

	[[= ks::reflect::Net{} ]] bool m_bSpotted;
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Proxy<CBaseEntity::RecvProxyOldSpottedByMask, ks::reflect::WireSide::Recv>{} ]] bool m_bSpottedBy[MAX_PLAYERS + 1]; // OBSELETE USED BY OLD DEMOS

	CNetworkArray( uint32, m_bSpottedByMask, kNumSpottedByMask, [[= ks::reflect::Net{} ]] );

	[[= ks::reflect::Net{} ]] unsigned char m_nMinCPULevel;
	[[= ks::reflect::Net{} ]] unsigned char m_nMaxCPULevel;
	[[= ks::reflect::Net{} ]] unsigned char m_nMinGPULevel;
	[[= ks::reflect::Net{} ]] unsigned char m_nMaxGPULevel;

	//Adrian
	[[= ks::reflect::Net{} ]] unsigned char					m_iTextureFrameIndex;

	// Bbox visualization
	unsigned char					m_fBBoxVisFlags;

	bool							m_bIsValidIKAttachment;

	// The list that holds OnDataChanged events uses this to make sure we don't get multiple
	// OnDataChanged calls in the same frame if the client receives multiple packets.
	int								m_DataChangeEventRef;

	// Player who is driving my simulation
	CHandle< CBasePlayer >			m_hPlayerSimulationOwner;

	[[= ks::reflect::Net{} ]] EHANDLE							m_hEffectEntity;
	
	// This is a random seed used by the networking code to allow client - side prediction code
	//  randon number generators to spit out the same random numbers on both sides for a particular
	//  usercmd input.
	static int						m_nPredictionRandomSeed;
	static CBasePlayer				*m_pPredictionPlayer;
	static bool						s_bAbsQueriesValid;
	static bool						s_bAbsRecomputationEnabled;

	static bool						s_bInterpolate;

	AimEntsListHandle_t				m_AimEntsListHandle;
	int								m_nCreationTick;

public:
	float							m_fRenderingClipPlane[4]; //world space clip plane when drawing
	bool							m_bEnableRenderingClipPlane; //true to use the custom clip plane when drawing
	float *							GetRenderClipPlane( void ); // Rendering clip plane, should be 4 floats, return value of NULL indicates a disabled render clip plane
	bool							IsSpotted( void ) { return m_bSpotted && !IsDormant(); }
	bool							IsSpottedBy( int nPlayerIndex );
	bool							IsSpottedByFriends( int nPlayerIndex );

protected:
	void SetIsSpottedBy( int nPlayerEntIndex );

	void AddToEntityList( entity_list_ids_t listId );
	void RemoveFromEntityList( entity_list_ids_t listId );
	unsigned short m_ListEntry[NUM_ENTITY_LISTS];	// Entry into each g_EntityList (or InvalidIndex() if not in the list).
	
	std::recursive_mutex m_CalcAbsolutePositionMutex;
	std::recursive_mutex m_CalcAbsoluteVelocityMutex;

private:
	bool							m_bIsBlurred;
};

EXTERN_RECV_TABLE(DT_BaseEntity);

inline bool FClassnameIs( CBaseEntity *pEntity, const char *szClassname )
{ 
	contract_assert( pEntity );
	return pEntity && !std::strcmp( pEntity->GetClassname(), szClassname ); 
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this entity was created on the client.
//-----------------------------------------------------------------------------
bool CBaseEntity::IsServerEntity( void ) const
{
	return index != -1;
}

//-----------------------------------------------------------------------------
// Client version of UTIL_Remove
//-----------------------------------------------------------------------------
inline void UTIL_Remove( CBaseEntity *pEntity )
{
	pEntity->Remove();
}

bool CBaseEntity::IsVisible() const
{
	if ( INVALID_CLIENT_RENDER_HANDLE == m_hRender )
		return false;
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return m_VisibilityBits[GET_ACTIVE_SPLITSCREEN_SLOT()];
}

bool CBaseEntity::IsVisibleToAnyPlayer() const
{
	return m_VisibilityBits.any();
}

class CAbsQueryScopeGuard
{
public:
	explicit CAbsQueryScopeGuard( bool state );
	~CAbsQueryScopeGuard()
	{
		CBaseEntity::SetAbsQueriesValid( m_bSavedState );
	}
private:
	bool	m_bSavedState;
};

CBaseEntity *CreateEntityByName( const char *className );

struct CEntIndexLessFunc
{
	bool operator()( CBaseEntity *lhs, CBaseEntity *rhs ) const
	{
		int e1 = lhs->entindex();
		int e2 = rhs->entindex();

		// if an entity has an invalid entity index, then put it at the end of the list
		e1 = ( e1 == -1 ) ? MAX_EDICTS : e1;
		e2 = ( e2 == -1 ) ? MAX_EDICTS : e2;

		return e1 < e2;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Maintains a list of predicted or client created entities
//-----------------------------------------------------------------------------
class CPredictableList
{
public:
	CBaseEntity	*GetPredictable( int slot );
	int				GetPredictableCount( void ) const;

protected:
	void			AddToPredictableList( CBaseEntity *add );
	void			RemoveFromPredictablesList( CBaseEntity *remove );

private:
	std::flat_set< CBaseEntity *, CEntIndexLessFunc >	m_Predictables;	// one entity per entindex

	friend class CBaseEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slot - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline CBaseEntity *CPredictableList::GetPredictable( int slot )
{
	return m_Predictables.begin()[ slot ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline int CPredictableList::GetPredictableCount( void ) const
{
	return std::ssize( m_Predictables );
}

[[gnu::always_inline]] inline int CBaseEntity::GetSplitUserPlayerPredictionSlot()
{
	contract_assert( m_bPredictable );
	return m_nSplitUserPlayerPredictionSlot;
}

extern CPredictableList *GetPredictables( int nSlot );

// To temporarily muck with gpGlobals->curtime
class CCurTimeScopeGuard
{
public:
	CCurTimeScopeGuard( float flNewCurTime, bool bOptionalCondition = true );
	~CCurTimeScopeGuard();
private:

	float	m_flSavedTime;
	bool	m_bActive;
};

#undef CREATE_PREDICTED_ENTITY

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: Color correction entity with simple radial falloff

//------------------------------------------------------------------------------
// Purpose : Color correction entity with radial falloff
//------------------------------------------------------------------------------
class CColorCorrection : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ColorCorrection" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CColorCorrection, CBaseEntity );

	DECLARE_CLIENTCLASS();

	CColorCorrection();
	virtual ~CColorCorrection();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

	virtual void Update(CBasePlayer *pPlayer, float ccScale);
	
	bool IsMaster() const { return m_bMaster; }
	bool IsClientSide() const;
	bool IsExclusive() const { return m_bExclusive; }

	void EnableOnClient( bool bEnable, bool bSkipFade = false );

	Vector GetOrigin();
	float  GetMinFalloff();
	float  GetMaxFalloff();

	void   SetWeight( float fWeight );

protected:
	void StartFade( int nSplitScreenSlot, float flDuration );
	float GetFadeRatio( int nSplitScreenSlot ) const;
	bool IsFadeTimeElapsed( int nSplitScreenSlot ) const;

	[[= ks::reflect::Net{} ]] Vector	m_vecOrigin;

	[[= ks::reflect::Net{} ]] float	m_minFalloff;
	[[= ks::reflect::Net{} ]] float	m_maxFalloff;
	[[= ks::reflect::Net{} ]] float	m_flFadeInDuration;
	[[= ks::reflect::Net{} ]] float	m_flFadeOutDuration;
	[[= ks::reflect::Net{} ]] float	m_flMaxWeight;
	[[= ks::reflect::Net{} ]] float	m_flCurWeight;		// networked from server
	[[= ks::reflect::Net{} ]] char	m_netLookupFilename[MAX_PATH];

	[[= ks::reflect::Net{} ]] bool	m_bEnabled;			// networked from server
	[[= ks::reflect::Net{} ]] bool	m_bMaster;
	[[= ks::reflect::Net{} ]] bool	m_bClientSide;
	[[= ks::reflect::Net{} ]] bool	m_bExclusive;

	bool	m_bEnabledOnClient[MAX_SPLITSCREEN_PLAYERS];
	float	m_flCurWeightOnClient[MAX_SPLITSCREEN_PLAYERS];
	bool	m_bFadingIn[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeStartWeight[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeStartTime[MAX_SPLITSCREEN_PLAYERS];
	float	m_flFadeDuration[MAX_SPLITSCREEN_PLAYERS];

	ClientCCHandle_t m_CCHandle;
};

const float PIXELVIS_DEFAULT_PROXY_SIZE = 2.0f;
const float PIXELVIS_DEFAULT_FADE_TIME = 0.0625f;

typedef int pixelvis_handle_t;
struct pixelvis_queryparams_t
{
	pixelvis_queryparams_t()
	{
		bSetup = false;
	}

	void Init( const Vector &origin, float proxySizeIn = PIXELVIS_DEFAULT_PROXY_SIZE, float proxyAspectIn = 1.0f, float fadeTimeIn = PIXELVIS_DEFAULT_FADE_TIME )
	{
		position = origin;
		proxySize = proxySizeIn;
		proxyAspect = proxyAspectIn;
		fadeTime = fadeTimeIn;
		bSetup = true;
		bSizeInScreenspace = false;
	}

	Vector		position;
	float		proxySize;
	float		proxyAspect;
	float		fadeTime;
	bool		bSetup;
	bool		bSizeInScreenspace;
};

float PixelVisibility_FractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle );
float StandardGlowBlend( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle, int rendermode, int renderfx, int alpha, float *pscale );

void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID ); //mainly needed by portal mod to avoid a pop in visibility when teleporting the player

void PixelVisibility_EndCurrentView();
void PixelVisibility_EndScene();
float GlowSightDistance( const Vector &glowOrigin, bool bShouldTrace );

// returns true if the video hardware is doing the tests, false is traceline is doing so.
bool PixelVisibility_IsAvailable();

class COcclusionQuerySet //A set of occlusion query handles for the same object in different rendering views. Self-registers and shifts appropriately with portals. 
{
public:
	COcclusionQuerySet( void );
	~COcclusionQuerySet( void );
	
	//wrap your draws with these 2
	void BeginQueryDrawing( int iViewID, int iSplitScreenSlot );
	void EndQueryDrawing( int iViewID, int iSplitScreenSlot );

	//and here's your data, expect a frame of delay for performance reasons
	int QueryNumPixelsRendered( int iViewID, int iSplitScreenSlot );	
	float QueryPercentageOfScreenRendered( int iViewID, int iSplitScreenSlot ); // (Pixels rendered) / (total screen pixels)
	int QueryNumPixelsRenderedForAllViewsLastFrame( int iSplitScreenSlot );

	int GetLastFrameDrawn( int iViewID, int iSplitScreenSlot );

	//these implementations call the above with current view id and active splitscreen slot
	void BeginQueryDrawing( void );
	void EndQueryDrawing( void );
	int QueryNumPixelsRendered( void );
	float QueryPercentageOfScreenRendered( void );
	int QueryNumPixelsRenderedForAllViewsLastFrame( void );
	int GetLastFrameDrawn( void );

private:
	void *m_pManagedData; //no need to worry what's under the hood here
};

class CBaseEntity;

using namespace ::ks::reflect;


class CPlayerEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_NOBASE( CPlayerEnumerator );
public:
	//Forced constructor
	CPlayerEnumerator( float radius, Vector vecOrigin )
	{
		m_flRadiusSquared = radius * radius;
		m_vecOrigin = vecOrigin;
		m_Objects.clear();
	}

	int	GetObjectCount() { return std::ssize( m_Objects ); }

	CBaseEntity *GetObject( int index )
	{
		if ( index < 0 || index >= GetObjectCount() )
			return nullptr;

		return m_Objects[ index ];
	}

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
		if ( pEnt == nullptr )
			return ITERATION_CONTINUE;

		if ( !pEnt->IsPlayer() )
			return ITERATION_CONTINUE;

		Vector	deltaPos = pEnt->GetAbsOrigin() - m_vecOrigin;
		if ( deltaPos.LengthSqr() > m_flRadiusSquared )
			return ITERATION_CONTINUE;

		CHandle< CBaseEntity > h;
		h = pEnt;
		m_Objects.push_back( h );

		return ITERATION_CONTINUE;
	}

public:
	//Data members
	float	m_flRadiusSquared;
	Vector m_vecOrigin;

	std::vector< CHandle< CBaseEntity > > m_Objects;
};

class CFogController;

inline constexpr auto NUM_AUDIO_LOCAL_SOUNDS = 8;

// These structs are contained in each player's local data and shared by the client & server

struct fogparams_t
{
	DECLARE_CLASS_NOBASE( fogparams_t );
	DECLARE_EMBEDDED_NETWORKVAR();

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

	CNetworkArray( Vector, localSound, NUM_AUDIO_LOCAL_SOUNDS )
	CNetworkVar( int, soundscapeIndex );	// index of the current soundscape from soundscape.txt
	CNetworkVar( int, localBits );			// if bits 0,1,2,3 are set then position 0,1,2,3 are valid/used
	CNetworkVar( int, entIndex );		// the entity setting the soundscape
};

struct audioparams_t;

using namespace ::ks::reflect;

// Purpose: Defines the player specific data that is sent only to the player
//			to whom it belongs.


//-----------------------------------------------------------------------------
// Purpose: Player specific data ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CPlayerLocalData : public CPlayerLocalMoveCore
{
	[[= ks::reflect::NetTable{ .name = "DT_Local", .base = false } ]]
	[[= ks::reflect::From<"m_skybox3d.scale", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.origin", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.area", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.enable", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.blend", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.dirPrimary", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.colorPrimary", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.colorSecondary", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.start", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.end", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.maxdensity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_skybox3d.fog.HDRColorScale", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[0]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[1]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[2]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[3]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[4]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[5]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[6]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localSound[7]", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.soundscapeIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.localBits", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_audio.entIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nDuckTimeMsecs", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_nDuckTimeMsecs", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_nDuckJumpTimeMsecs", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_nDuckJumpTimeMsecs", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_nJumpTimeMsecs", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_nJumpTimeMsecs", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_flFallVelocity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_flFallVelocity", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.5f } >{} ]]
	[[= ks::reflect::From<"m_flStepSize", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_flStepSize", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_viewPunchAngle", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::PredFrom<"m_viewPunchAngle", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } >{} ]]
	[[= ks::reflect::From<"m_aimPunchAngle", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::PredFrom<"m_aimPunchAngle", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } >{} ]]
	[[= ks::reflect::From<"m_aimPunchAngleVel", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::PredFrom<"m_aimPunchAngleVel", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } >{} ]]
	[[= ks::reflect::From<"m_bDucked", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::PredFrom<"m_bDucked", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_bDucking", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::PredFrom<"m_bDucking", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_flLastDuckTime", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::PredFrom<"m_flLastDuckTime", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_bInDuckJump", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::PredFrom<"m_bInDuckJump", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::From<"m_bAllowAutoMovement", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }>{} ]]
	[[= ks::reflect::PredFrom<"m_bAllowAutoMovement", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( CPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	CPlayerLocalData() :
		m_iv_viewPunchAngle( "CPlayerLocalData::m_iv_viewPunchAngle" ),
		m_iv_aimPunchAngle( "CPlayerLocalData::m_iv_aimPunchAngle" ),
		m_iv_aimPunchAngleVel( "CPlayerLocalData::m_iv_aimPunchAngleVel" )
	{
		m_pfnStateChanged = []( CPlayerLocalMoveCore *pCore, void *pVar ) {
			CPlayerLocalData *pThis = static_cast<CPlayerLocalData *>( pCore );
			if ( pVar ) pThis->NetworkStateChanged( pVar );
			else pThis->NetworkStateChanged();
		};
		std::memset( m_chAreaBits, 0, sizeof( m_chAreaBits ) );
		memset( m_chAreaPortalBits, 0, sizeof( m_chAreaPortalBits ) );

		m_nStepside = 0;
		m_nOldButtons = 0;
		m_flFOVRate = 0.0f;

		m_iHideHUD = 0;
		m_nDuckTimeMsecs = 0;
		m_nDuckJumpTimeMsecs = 0;
		m_nJumpTimeMsecs = 0;

		m_flFallVelocity = 0.0f;
		m_flOldFallVelocity = 0.0f;
		m_flStepSize = 0.0f;

		/* REI: How to safely init these in constructor?
		m_viewPunchAngle.Zero();
		m_aimPunchAngle.Zero();
		m_aimPunchAngleVel.Zero();
		*/

		m_bDucked = false;
		m_bDucking = false;
		m_flLastDuckTime = -1.0f;
		m_bInDuckJump = false;
		m_bDrawViewmodel = false;
		m_bWearingSuit = false;
		m_bPoisoned = false;
		m_bAllowAutoMovement = true;

		m_bInLanding = false;
		m_flLandingTime = -1.0f;

		m_vecClientBaseVelocity.Zero();

		m_iv_viewPunchAngle.Setup( &m_viewPunchAngle, LATCH_SIMULATION_VAR );
		m_iv_aimPunchAngle.Setup( &m_aimPunchAngle, LATCH_SIMULATION_VAR );
		m_iv_aimPunchAngleVel.Setup( &m_aimPunchAngleVel, LATCH_SIMULATION_VAR );

		m_bAutoAimTarget = false;

		m_bSlowMovement = false;
		m_fTBeamEndTime = 0.0f;
	}

	[[= ks::reflect::Net{} ]] unsigned char			m_chAreaBits[MAX_AREA_STATE_BYTES];				// Area visibility flags.
	[[= ks::reflect::Net{} ]] unsigned char			m_chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES];// Area portal visibility flags.

// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
	[[= ks::reflect::Net{} ]] float					m_flFOVRate;		// rate at which the FOV changes

	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] int						m_iHideHUD;			// bitfields containing sections of the HUD to hide

	float					m_flOldFallVelocity;



	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] bool					m_bDrawViewmodel;
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] bool					m_bWearingSuit;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] bool					m_bPoisoned;
// END PREDICTION DATA COMPACTION

	bool					m_bInLanding;
	float					m_flLandingTime;

	// Base velocity that was passed in to server physics so 
	//  client can predict conveyors correctly.  Server zeroes it, so we need to store here, too.
	Vector					m_vecClientBaseVelocity;  
	CInterpolatedVar< QAngle >	m_iv_viewPunchAngle;
	CInterpolatedVar< QAngle >	m_iv_aimPunchAngle;
	CInterpolatedVar< QAngle >	m_iv_aimPunchAngleVel;

	// Autoaim
	bool					m_bAutoAimTarget;

	// 3d skybox
	sky3dparams_t			m_skybox3d;
	// audio environment
	audioparams_t			m_audio;

	float					m_fTBeamEndTime;

};

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Directional lighting with cascaded shadow mapping entity.

//------------------------------------------------------------------------------
// Purpose: Directional lighting with cascaded shadow mapping entity.
//------------------------------------------------------------------------------
class CCascadeLight : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_CascadeLight" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCascadeLight, CBaseEntity );
		
	DECLARE_CLIENTCLASS();

	CCascadeLight();

	virtual ~CCascadeLight();

	static CCascadeLight *Get() { return m_pCascadeLight; }

	virtual void Spawn();
	virtual void Release();
	virtual bool ShouldDraw();
	virtual void ClientThink();

	inline const Vector &GetShadowDirection() const { return m_shadowDirection; }
	inline const Vector &GetEnvLightShadowDirection() const { return m_envLightShadowDirection; }
	inline bool IsEnabled() const { return m_bEnabled; }
	inline color32 GetColor() const { return m_LightColor; }
	inline int GetColorScale() const { return m_LightColorScale; }
	inline bool UseLightEnvAngles() const { return m_bUseLightEnvAngles; }
	float GetMaxShadowDist() const { return m_flMaxShadowDist; }

private:
	static CCascadeLight *m_pCascadeLight;

	[[= ks::reflect::Net{} ]] Vector m_shadowDirection;
	[[= ks::reflect::Net{} ]] Vector m_envLightShadowDirection;
	[[= ks::reflect::Net{} ]] bool m_bEnabled;
	[[= ks::reflect::Net{} ]] bool m_bUseLightEnvAngles;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32	m_LightColor;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToInt32, ks::reflect::WireSide::Recv>{} ]] int	m_LightColorScale;
	[[= ks::reflect::Net{} ]] float m_flMaxShadowDist;
};

class CDebugPrimRenderer2D
{
public:
	CDebugPrimRenderer2D();

	void Clear();

	// Normalized [0,1] coords, where (0,0) is upper left
	void AddNormalizedLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b );
	
	// Screenspace (pixel) coords
	void AddScreenspaceLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b );
	void AddScreenspaceRect2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b );
	void AddScreenspaceLineList2D( uint nCount, const Vector2D *pVerts, const VertexColor_t &color );
	void AddScreenspaceWireframeFrustum2D( const VMatrix &xform, const VertexColor_t &color, bool bShowAxes );
		
	void Render2D( );

	void RenderScreenspaceDepthTexture( float sx, float sy, float ex, float ey, float su, float sv, float eu, float ev, CTextureReference &depthTex, float zLo, float zHi );

private:
	class CDebugLine
	{
	public:
		inline CDebugLine() { }
		inline CDebugLine( const Vector2D &start, const Vector2D &end, uint r = 255, uint g = 255, uint b = 255, uint a = 255 ) { Init( start, end, r, g, b, a ); }

		inline void Init( const Vector2D &start, const Vector2D &end, uint r = 255, uint g = 255, uint b = 255, uint a = 255 ) { m_EndPoints[0] = start; m_EndPoints[1] = end; m_nColor[0] = r; m_nColor[1] = g; m_nColor[2] = b; m_nColor[3] = a; }

		Vector2D m_EndPoints[2];		// normalized [0,1] coordinates, (0,0) is top-left
		uint8 m_nColor[4];
	};
	std::vector< CDebugLine > m_debugLines;

	void RenderDebugLines2D( uint nNumLines, const CDebugLine *pLines );
};

class CCascadeLightManager : public CAutoGameSystemPerFrame
{
public:
	CCascadeLightManager();
	virtual ~CCascadeLightManager();

	bool InitRenderTargets();
	void ShutdownRenderTargets();

	void LevelInitPreEntity();
	void LevelInitPostEntity();
	void LevelShutdownPreEntity();
	void LevelShutdownPostEntity();

	void Shutdown();
	
	bool IsEnabled() const;
	bool IsEnabledAndActive() const;

	virtual void PreRender ();
	
	void ComputeShadowDepthTextures( const CViewSetup &viewSetup, bool bSetup = false );
	void UnlockAllShadowDepthTextures();

	void BeginViewModelRendering();
	void EndViewModelRendering();

	void BeginReflectionView();
	void EndReflectionView();

	void Draw3DDebugInfo();
	void Draw2DDebugInfo();

	void DumpStatus();

	CSMQualityMode_t GetCSMQualityMode();

public:
	static void RotXPlusDown( const CCommand &args );
	static void RotXPlusUp( const CCommand &args );
	static void RotXNegDown( const CCommand &args );
	static void RotXNegUp( const CCommand &args );
	static void RotYPlusDown( const CCommand &args );
	static void RotYPlusUp( const CCommand &args );
	static void RotYNegDown( const CCommand &args );
	static void RotYNegUp( const CCommand &args );

private:
	class CFullCSMState
	{
	public:
		CFullCSMState()
		{
			Clear();
		}

		inline void Clear()
		{
			V_memset( &m_sceneFrustum, 0, sizeof( m_sceneFrustum ) );
			m_flSceneAspectRatio = 0.0f;
			m_sceneWorldToView.Identity();
			m_sceneViewToProj.Identity();
			m_sceneWorldToProj.Identity();
			m_CSMParallelSplit.Clear();
			m_shadowDir.Init( 0, 0, -1.0f );
			m_flMaxShadowDist = 0.0f;
			m_flMaxVisibleDist = 0.0f;
			m_nMaxCascadeSize = 0;
			m_bValid = false;
		}

		void Update( const CViewSetup &viewSetup, const Vector &shadowDir, const color32 lightColor, const int lightColorScale, float flMaxShadowDist, float flMaxVisibleDist, uint nMaxCascadeSize, uint nAtlasFirstCascadeIndex, int nCSMQualityLevel, bool bSetAllCascadesToFirst );

		bool IsValid() const { return m_CSMParallelSplit.IsValid() && m_CSMParallelSplit.GetLightState().m_nShadowCascadeSize; }
		void Reset() { m_CSMParallelSplit.Reset(); }

		CFrustum m_sceneFrustum;
		float m_flSceneAspectRatio;
		VMatrix	m_sceneWorldToView, m_sceneViewToProj, m_sceneWorldToProj;
		Vector m_shadowDir;
		float m_flMaxShadowDist;
		float m_flMaxVisibleDist;
		uint m_nMaxCascadeSize;
		CCSMParallelSplit m_CSMParallelSplit;

		bool m_bValid;
	};

	void RenderViews( CFullCSMState &state,  bool bIncludeViewModels );

	int m_nDepthTextureResolution;
	CSMQualityMode_t m_nCurRenderTargetQualityMode;

	CFullCSMState m_curState;
	CFullCSMState m_curViewModelState;

	CFullCSMState m_capturedState;
	
	CDebugPrimRenderer2D m_debugPrimRenderer;

	bool m_bRenderTargetsAllocated;
	CTextureReference m_ShadowDepthTexture;
	CTextureReference m_DummyColorTexture;

	float m_flRotX[2], m_flRotY[2];

	bool m_bCSMIsActive;
	bool m_bStateIsValid;

	void DeinitRenderTargets();
	void DrawTextDebugInfo();
	Vector GetShadowDirection();
	inline CFullCSMState &GetActiveState() { return m_capturedState.m_CSMParallelSplit.IsValid() ? m_capturedState : m_curState; }
};

extern CCascadeLightManager g_CascadeLightManager;

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Implementation of the class that controls detail prop fade distances
//-----------------------------------------------------------------------------
class CEnvDetailController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_DetailController", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvDetailController, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CEnvDetailController();
	virtual ~CEnvDetailController();

using BaseClass::KeyValue;

	CNetworkVar( float, m_flFadeStartDist, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flFadeEndDist, [[= ks::reflect::Net{ .bits = 32 } ]] );

	// ALWAYS transmit to all clients.
	virtual int UpdateTransmitState( void );
};

CEnvDetailController * GetDetailController();

class CBaseEntity;

using namespace ::ks::reflect;

//=============================================================================
//
// Class Fog Controller:
// Compares a set of integer inputs to the one main input
// Outputs true if they are all equivalant, false otherwise
//
class CFogController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FogController", .base = false } ]]
	[[= ks::reflect::From<"m_fog.enable", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.blend", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.dirPrimary", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.colorPrimary", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_fog.colorSecondary", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_fog.start", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.end", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.farz", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.maxdensity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.colorPrimaryLerpTo", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_fog.colorSecondaryLerpTo", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_fog.startLerpTo", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.endLerpTo", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.maxdensityLerpTo", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.lerptime", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.duration", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.HDRColorScale", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fog.ZoomFogScale", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_NETWORKCLASS();
	DECLARE_CLASS( CFogController, CBaseEntity );

	CFogController();

public:

	fogparams_t				m_fog;
};

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvProjectedTexture : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvProjectedTexture" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvProjectedTexture, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	void SetMaterial( IMaterial *pMaterial );
	void SetLightColor( byte r, byte g, byte b, byte a );
	void SetSize( float flSize );
	void SetRotation( float flRotation );

	virtual void OnDataChanged( DataUpdateType_t updateType );
	void	ShutDownLightHandle( void );

	virtual bool Simulate();

	bool	ShouldUpdate( void );
	void	UpdateLight( void );

	CEnvProjectedTexture();
	~CEnvProjectedTexture();

	static void SetVisibleBBoxMinHeight( float flVisibleBBoxMinHeight ) { m_flVisibleBBoxMinHeight = flVisibleBBoxMinHeight; }
	static float GetVisibleBBoxMinHeight( void ) { return m_flVisibleBBoxMinHeight; }
	static CEnvProjectedTexture *Create( );

private:

	inline bool IsBBoxVisible( void );
	bool IsBBoxVisible( Vector vecExtentsMin,
						Vector vecExtentsMax );

	ClientShadowHandle_t m_LightHandle;
	bool m_bForceUpdate;

	[[= ks::reflect::Net{} ]] EHANDLE	m_hTargetEntity;

	[[= ks::reflect::Net{} ]] bool		m_bState;
	[[= ks::reflect::Net{} ]] bool		m_bAlwaysUpdate;
	[[= ks::reflect::Net{} ]] float		m_flLightFOV;
	[[= ks::reflect::Net{} ]] bool		m_bEnableShadows;
	[[= ks::reflect::Net{} ]] bool		m_bSimpleProjection;
	[[= ks::reflect::Net{} ]] bool		m_bLightOnlyTarget;
	[[= ks::reflect::Net{} ]] bool		m_bLightWorld;
	[[= ks::reflect::Net{} ]] bool		m_bCameraSpace;
	[[= ks::reflect::Net{} ]] float		m_flBrightnessScale;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32		m_LightColor;
	Vector		m_CurrentLinearFloatLightColor;
	float		m_flCurrentLinearFloatLightAlpha;
	[[= ks::reflect::Net{} ]] float		m_flColorTransitionTime;
	[[= ks::reflect::Net{} ]] float		m_flAmbient;
	[[= ks::reflect::Net{} ]] float		m_flNearZ;
	[[= ks::reflect::Net{} ]] float		m_flFarZ;
	[[= ks::reflect::Net{} ]] char		m_SpotlightTextureName[ MAX_PATH ];
	CTextureReference m_SpotlightTexture;
	CMaterialReference m_ProjectedMaterial;
	[[= ks::reflect::Net{} ]] int			m_nSpotlightTextureFrame;
	[[= ks::reflect::Net{} ]] int			m_nShadowQuality;
	[[= ks::reflect::Net{} ]] int			m_iStyle;
	bool		m_bIsCurrentlyProjected;

	// simple projection
	IMaterial	*m_pMaterial;
	[[= ks::reflect::Net{} ]] float		m_flProjectionSize;
	[[= ks::reflect::Net{} ]] float		m_flRotation;

	Vector	m_vecExtentsMin;
	Vector	m_vecExtentsMax;

	static float m_flVisibleBBoxMinHeight;
};

bool CEnvProjectedTexture::IsBBoxVisible( void )
{
	return IsBBoxVisible( GetAbsOrigin() + m_vecExtentsMin, GetAbsOrigin() + m_vecExtentsMax );
}

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CSoundPatch;

//-----------------------------------------------------------------------------
// Implementation of the class that computes windspeed
//-----------------------------------------------------------------------------
class CEnvWindShared
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvWindShared", .base = false } ]]
	[[= ks::reflect::From<"m_iMinWind", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iMaxWind", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iMinGust", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iMaxGust", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_flMinGustDelay", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flMaxGustDelay", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_iGustDirChange", ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iWindSeed", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iInitialWindDir", ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_flInitialWindSpeed", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flStartTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flGustDuration", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CEnvWindShared );
	DECLARE_EMBEDDED_NETWORKVAR();
	
	CEnvWindShared();
	~CEnvWindShared();

	CEnvWindShared( const CEnvWindShared & ) = delete;
	CEnvWindShared& operator= ( const CEnvWindShared& ) = delete;

	void Init( int iEntIndex, int iRandomSeed, float flTime, int iWindDir, float flInitialWindSpeed );

	void SetLocation( const Vector &location );

	// Method to update the wind speed
	// Time passed in here is global time, not delta time
	// The function returns the time at which it must be called again
	float WindThink( float flTime );

	// FIXME: These really should be private
	CNetworkVar( float, m_flStartTime );

	CNetworkVar( int, m_iWindSeed );		// random number seed...

	CNetworkVar( int, m_iMinWind );			// the slowest the wind can normally blow
	CNetworkVar( int, m_iMaxWind );			// the fastest the wind can normally blow
	CNetworkVar( int, m_windRadius );		// the radius this entity affects with its windiness, so a map can have multiple
	CNetworkVar( int, m_iMinGust );			// the slowest that a gust can be
	CNetworkVar( int, m_iMaxGust );			// the fastest that a gust can be

	CNetworkVar( float, m_flMinGustDelay );	// min time between gusts
	CNetworkVar( float, m_flMaxGustDelay );	// max time between gusts

	CNetworkVar( float, m_flGustDuration );	// max time between gusts

	CNetworkVar( int, m_iGustDirChange );	// max number of degrees wind dir changes on gusts.
	CNetworkVector( m_location );			// The location of this wind controller

	int m_iszGustSound;		// name of the wind sound to play for gusts.
	int m_iWindDir;			// wind direction (yaw)
	float m_flWindSpeed;	// the wind speed

	Vector m_currentWindVector;	// For all the talk of proper prediction, we ended up just storing and returning through a static vector.  Now we can have multiple env_wind, so we need this in here.
	Vector m_CurrentSwayVector;
	Vector m_PrevSwayVector;

	CNetworkVar( int, m_iInitialWindDir );
	CNetworkVar( float, m_flInitialWindSpeed );

private:
	void ComputeWindVariation( float flTime );

	// Updates the wind sound
	void UpdateWindSound( float flTotalWindSpeed );

	void UpdateTreeSway( float flTime );

	float	m_flVariationTime;
	float	m_flSwayTime;
	float	m_flSimTime;		// What's the time I last simulated up to?
	float	m_flSwitchTime;		// when do I actually switch from gust to not gust
	float	m_flAveWindSpeed;	// the average wind speed
	bool	m_bGusting;			// is the wind gusting right now?

	float m_flWindAngleVariation;
	float m_flWindSpeedVariation;

	int m_iEntIndex;

	// Used to generate random numbers
	CUniformRandomStream m_Stream;

	// NOTE: In order to make this algorithm independent of calling frequency
	// I have to decouple the stream used to generate average wind speed
	// and the stream used to generate wind variation since they are
	// simulated using different timesteps
	CUniformRandomStream m_WindVariationStream;

	// Used to generate the wind sound...
	CSoundPatch *m_pWindSound;
};

//-----------------------------------------------------------------------------
// Method to sample the wind speed at a particular location
//-----------------------------------------------------------------------------
Vector GetWindspeedAtLocation( const Vector &location );

//-----------------------------------------------------------------------------
// Method to sample the windspeed at a particular time
//-----------------------------------------------------------------------------
void GetWindspeedAtTime( float flTime, Vector &vecVelocity );

//-----------------------------------------------------------------------------
// Method to reset wind speed..
//-----------------------------------------------------------------------------
void ResetWindspeed();

class IGameSystem;
struct audioparams_t;



extern IGameSystem *ClientSoundscapeSystem();

// call when audio params may have changed
extern void Soundscape_Update( audioparams_t &audio );

// Called on round restart, otherwise the soundscape system thinks all its
// sounds are still playing when they're not.
void Soundscape_OnStopAllSounds();

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Spatial entity with simple radial falloff

inline constexpr auto SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE = 128.0f;

//------------------------------------------------------------------------------
// Purpose : Spatial entity with radial falloff
//------------------------------------------------------------------------------
class CSpatialEntity : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SpatialEntity" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSpatialEntity, CBaseEntity );
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged(DataUpdateType_t updateType);
	virtual void UpdateOnRemove( void );
	virtual bool ShouldDraw();

	virtual void ClientThink();
	virtual void InitSpatialEntity();

	virtual void ResetAccumulation( void ) {};
	virtual void Accumulate( void ) {};
	virtual void ApplyAccumulation( void ) {};

	float GetMinFalloff( void ) { return m_minFalloff; }
	float GetMaxFalloff( void ) { return m_maxFalloff; }

	void SetFalloff( float flMinFalloff, float flMaxFalloff ) { m_minFalloff = flMinFalloff; m_maxFalloff = flMaxFalloff; }
	void SetEnabled( bool bEnabled ) { m_bEnabled = bEnabled; }
	void SetCurWeight( float flWeight ) { m_flCurWeight = flWeight; }

protected:
	virtual void AddToPersonalSpatialEntityMgr( void ) {};
	virtual void RemoveFromPersonalSpatialEntityMgr( void ) {};

private:
	[[= ks::reflect::Net{} ]] Vector	m_vecOrigin;

	[[= ks::reflect::Net{} ]] float	m_minFalloff;
	[[= ks::reflect::Net{} ]] float	m_maxFalloff;
	[[= ks::reflect::Net{} ]] float	m_flCurWeight;
	char	m_netLookupFilename[MAX_PATH];

	[[= ks::reflect::Net{} ]] bool	m_bEnabled;

protected:
	float	m_flWeight;		// Interpolator for how much it's value adds to the total
	float	m_flInfluence;	// Distance within the inner radius
};

template <class T>
class CSpatialEntityTemplate : public CSpatialEntity
{
	DECLARE_CLASS( CSpatialEntityTemplate, CSpatialEntity );
public:
	virtual void ResetAccumulation( void )
	{
		m_AccumulatedValue = 0;
		m_ForcedValue = 0;
		m_ForcedMinFalloff = GetMinFalloff();
		m_ForcedInfluence = 0.0f;
	}

	virtual void Accumulate( void )
	{
		if ( m_flInfluence > m_ForcedInfluence )
		{
			// This value is in control unless a stronger influence is accumulated
			m_ForcedValue = m_Value;
			m_ForcedMinFalloff = GetMinFalloff();
			m_ForcedInfluence = m_flInfluence;
		}

		// Contribute to the total
		m_AccumulatedValue += m_Value * m_flWeight;
	}

	T BlendedValue( void )
	{
		if ( m_ForcedInfluence <= 0.0f )
		{
			// No forced value
			return m_AccumulatedValue;
		}

		// Check if we're fully inside the forced area
		if ( m_ForcedInfluence >= SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE )
		{
			// Fully inside the forced area
			return m_ForcedValue;
		}

		// Blend between the accumulated value and the forced value
		float flInterp = ( SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE - m_ForcedInfluence ) / SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE;

		return ( m_AccumulatedValue * flInterp ) + ( m_ForcedValue * ( 1.0f - flInterp ) );
	}

protected:

	T				m_Value;

private:
	static T		m_AccumulatedValue;
	static T		m_ForcedValue;
	static float	m_ForcedMinFalloff;
	static float	m_ForcedInfluence;
};

template <class T>
T CSpatialEntityTemplate<T>::m_AccumulatedValue;

template <class T>
T CSpatialEntityTemplate<T>::m_ForcedValue;

template <class T>
float CSpatialEntityTemplate<T>::m_ForcedMinFalloff;

template <class T>
float CSpatialEntityTemplate<T>::m_ForcedInfluence;

template <>
inline void CSpatialEntityTemplate<Vector>::ResetAccumulation( void )
{
	m_AccumulatedValue.Init();
	m_ForcedValue.Init();
	m_ForcedMinFalloff = GetMinFalloff();
	m_ForcedInfluence = 0.0f;
}
