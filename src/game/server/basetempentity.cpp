// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "irecipientfilter.h"
#include "networkvar.h"
#include "server_class.h"
#include "reflect_sendtable.h"
#include "dt_common.h"
#include "edict.h"
#include "eiface.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "predictable_entity.h"
#include "util.h"
#include "declare_class.h"
#include "model_types.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "dt_recv.h"
#include "reflect_annotations.h"
#include "worldsize.h"
#include "shareddefs.h"
#include "bone_setup.h"
module ks.server;

import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import :vscript;
import :fish;
import :sharedvar;
import :player;
import :baseentity;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import std;
import ks.core.basetypes;
import ks.tier1.interfaces;
import ks.tier1.small_vector;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.tier1.exprevaluator;
import :cstrike15.cs_playeranimstate;
import :entity_dispatch;
import :entity_kind;
import :econ.econ_item_schema_minimal;
import ks.game.beam_flags;
import ks.tier1.strtools;
import ks.core.types;
import ks.core.numeric;
import ks.core.random;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vmatrix;

class CBaseTempEntity;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_SERVERCLASS( CBaseTempEntity, DT_BaseTempEntity )

// Global list of temp entity event classes
CBaseTempEntity *CBaseTempEntity::s_pTempEntities = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Returns head of list
// Output : CBaseTempEntity * -- head of list
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetList( void )
{
	return s_pTempEntities;
}

//-----------------------------------------------------------------------------
// Purpose: Creates temp entity, sets name, adds to global list
// Input  : *name - 
//-----------------------------------------------------------------------------
CBaseTempEntity::CBaseTempEntity( const char *name )
{
	m_pszName = name;
	contract_assert( m_pszName );

	// Add to list
	m_pNext			= s_pTempEntities;
	s_pTempEntities = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTempEntity::~CBaseTempEntity( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of this temp entity
// Output : const char *
//-----------------------------------------------------------------------------
const char *CBaseTempEntity::GetName( void )
{
	return m_pszName ? m_pszName : "Unnamed";
}

//-----------------------------------------------------------------------------
// Purpose: Get next temp ent in chain
// Output : CBaseTempEntity *
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetNext( void )
{
	return m_pNext;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTempEntity::Precache( void )
{
	// Nothing...
}

//-----------------------------------------------------------------------------
// Purpose: Default test implementation. Should only be called by derived classes
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CBaseTempEntity::Test( const Vector& current_origin, const QAngle& current_angles )
{
	Vector origin, forward;

	Msg( "%s\n", m_pszName );
	AngleVectors( current_angles, &forward );

	VectorMA( current_origin, 20, forward, origin );

	CBroadcastRecipientFilter filter;

	Create( filter, 0.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Called at startup to allow temp entities to precache any models/sounds that they need
//-----------------------------------------------------------------------------
void CBaseTempEntity::PrecacheTempEnts( void )
{
	CBaseTempEntity *te = GetList();
	while ( te )
	{
		te->Precache();
		te = te->GetNext();
	}
}

void CBaseTempEntity::Create( IRecipientFilter& filter, float delay )
{
	// temp entities can't be part of the signon message, use real entities instead
	// temp entities *can* be reliable, necessarily, due to particles.
	contract_assert( !filter.IsInitMessage() );
	contract_assert( delay >= -1 && delay <= 1 ); // 1 second max delay

	engine->PlaybackTempEntity( filter, delay, 
		(void *)this, GetServerClass()->m_pTable, GetServerClass()->m_ClassID );
}

class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;

void SendProxy_AnimTime( const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_SequenceChanged( const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: Save Data for Base Weapon object
//-----------------------------------------------------------------------------// 

int CBaseViewModel::UpdateTransmitState_Impl()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

int CBaseViewModel::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
{
	// Check if recipient owns this weapon viewmodel
	CBasePlayer *pOwner = ToBasePlayer( m_hOwner );

	if ( pOwner && 
		 ( pOwner->edict() == pInfo->m_pClientEnt ||
			// If we're using the other guys network connection in split screen, then also force transmit
		 pOwner->IsSplitScreenUserOnEdict( pInfo->m_pClientEnt ) ) )
	{
		return FL_EDICT_ALWAYS;
	}

	// check if recipient (or one of his splitscreen parasites) is spectating the owner of this viewmodel
	CBasePlayer *pPlayer = ToBasePlayer( CBaseEntity::Instance( pInfo->m_pClientEnt ) );
	if ( pPlayer)
	{
		// Bug 28591:  In splitscreen, when the second slot is the one in spectator mode, it wouldn't
		//  get the viewmodel for the spectatee.
		//
		// The new logic is to loop through the splitscreen parasites (as well as the host player) 
		//  and see if any of them are observing the viewmodel owner, and if so, FL_EDICT_ALWAYS the vm for them, too.

		// This container was the source of most of the allocation cost in CS:GO so using a std::inplace_vector to avoid
		// allocations is important.
		ks::small_vector< CBasePlayer *, MAX_SPLITSCREEN_CLIENTS > checkList;
		checkList.push_back( pPlayer );
		std::vector< CHandle< CBasePlayer > > &vecParasites = pPlayer->GetSplitScreenPlayers();
		for ( int i = 0; i < std::ssize( vecParasites ); ++i )
		{
			checkList.push_back( vecParasites[ i ] );
		}

		for ( int i = 0; i < std::ssize( checkList ); ++i )
		{
			CBasePlayer *pPlayer = checkList[ i ];
			if ( !pPlayer )
				continue;

			if ( pPlayer->IsHLTV() || pPlayer->IsReplay() )
			{
				// if this is the HLTV or Replay client, transmit all viewmodels in our PVS
				return FL_EDICT_PVSCHECK;
			}
			if ( (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)  && (pPlayer->GetObserverTarget() == pOwner) )
			{
				return FL_EDICT_ALWAYS;
			}
		}
	}

	// Don't send to anyone else except the local player or his spectator
	return FL_EDICT_DONTSEND;
}

void CBaseViewModel::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;

extern ConVar in_forceuser;

static constexpr auto VIEWMODEL_ANIMATION_PARITY_BITS = 3;
inline constexpr const char *SCREEN_OVERLAY_MATERIAL = "vgui/screens/vgui_overlay";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::CBaseViewModel()
{

	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );

	// View model of this weapon
	m_sVMName			= NULL_STRING;		
	// Prefix of the animations that should be used by the player carrying this weapon
	m_sAnimationPrefix	= NULL_STRING;

	m_nViewModelIndex	= 0;

	m_nAnimationParity	= 0;

	m_bShouldIgnoreOffsetAndAccuracy = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::~CBaseViewModel()
{
}

void CBaseViewModel::UpdateOnRemove_Impl( void )
{
	BaseClass::UpdateOnRemove_Impl();

	DestroyControlPanels();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Precache_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Spawn_Impl( void )
{
	Precache( );
	SetSize( Vector( -8, -4, -2), Vector(8, 4, 2) );
	SetSolid( SOLID_NONE );
}

// VGUI in-world control panels removed (VGUI teardown); VGUI_CONTROL_PANELS intentionally left undefined.

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetControlPanelsActive( bool bState )
{
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CBaseViewModel::SpawnControlPanels()
{
}

void CBaseViewModel::DestroyControlPanels()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetOwner( CBaseEntity *pEntity )
{
	m_hOwner = pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetIndex( int nIndex )
{
	m_nViewModelIndex = nIndex;
	contract_assert( m_nViewModelIndex < (1 << VIEWMODEL_INDEX_BITS) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseViewModel::ViewModelIndex( ) const
{
	return m_nViewModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Pass our visibility on to our child screens
//-----------------------------------------------------------------------------
void CBaseViewModel::AddEffects( int nEffects )
{
	if ( nEffects & EF_NODRAW )
	{
		SetControlPanelsActive( false );
	}

	BaseClass::AddEffects( nEffects );
}

//-----------------------------------------------------------------------------
// Purpose: Pass our visibility on to our child screens
//-----------------------------------------------------------------------------
void CBaseViewModel::RemoveEffects( int nEffects )
{
	if ( nEffects & EF_NODRAW )
	{
		SetControlPanelsActive( true );
	}

	BaseClass::RemoveEffects( nEffects );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *modelname - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetWeaponModel( const char *modelname, CBaseCombatWeapon *weapon )
{
	m_hWeapon = weapon;

	string_t str;
	if ( modelname != nullptr )
	{
		str = MAKE_STRING( modelname );
	}
	else
	{
		str = NULL_STRING;
	}

	if ( str != m_sVMName )
	{
		// Msg( "SetWeaponModel %s at %f\n", modelname, gpGlobals->curtime );
		m_sVMName = str;
		SetModel( STRING( m_sVMName ) );

		// Create any vgui control panels associated with the weapon
		SpawnControlPanels();

		bool showControlPanels = weapon && weapon->ShouldShowControlPanels();
		SetControlPanelsActive( showControlPanels );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseViewModel::GetOwningWeapon( void )
{
	return m_hWeapon.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sequence - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SendViewModelMatchingSequence( int sequence )
{
	// since all we do is send a sequence number down to the client, 
	// set this here so other weapons code knows which sequence is playing.
	SetSequence( sequence );

	m_nAnimationParity = ( m_nAnimationParity + 1 ) & ( (1<<VIEWMODEL_ANIMATION_PARITY_BITS) - 1 );

	CBaseCombatWeapon *weapon = m_hWeapon.Get();
	bool showControlPanels = weapon && weapon->ShouldShowControlPanels();
	SetControlPanelsActive( showControlPanels );

	// Restart animation at frame 0
	SetCycle( 0 );
	ResetSequenceInfo();
}

void CBaseViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{

	// UNDONE: Calc this on the server?  Disabled for now as it seems unnecessary to have this info on the server

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles )
{
	Vector vOriginalOrigin = origin;
	QAngle vOriginalAngles = angles;

	// Calculate our drift
	Vector	forward;
	AngleVectors( angles, &forward, nullptr, nullptr );

	if ( gpGlobals->frametime != 0.0f )
	{
		Vector vDifference;
		VectorSubtract( forward, m_vecLastFacing, vDifference );

		float flSpeed = 5.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.  Solves the problem with fast cl_yawspeed, m_yaw or joysticks
		//  rotating quickly.  The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();
		if ( flDiff > 1.5f )
		{
			float flScale = flDiff / 1.5f;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		VectorMA( m_vecLastFacing, flSpeed * gpGlobals->frametime, vDifference, m_vecLastFacing );
		// Make sure it doesn't grow out of control!!!
		VectorNormalize( m_vecLastFacing );
		VectorMA( origin, 5.0f, vDifference * -1.0f, origin );

		contract_assert( m_vecLastFacing.IsValid() );
	}

	Vector right, up;
	AngleVectors( original_angles, &forward, &right, &up );

	float pitch = original_angles[ PITCH ];
	if ( pitch > 180.0f )
		pitch -= 360.0f;
	else if ( pitch < -180.0f )
		pitch += 360.0f;

	//FIXME: These are the old settings that caused too many exposed polys on some models
	VectorMA( origin, -pitch * 0.035f,	forward,	origin );
	VectorMA( origin, -pitch * 0.03f,		right,	origin );
	VectorMA( origin, -pitch * 0.02f,		up,		origin);
}

//-----------------------------------------------------------------------------
// Stub to keep networking consistent for DEM files
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Resets anim cycle when the server changes the weapon on us
//-----------------------------------------------------------------------------

IMPLEMENT_NETWORKCLASS_ALIASED( BaseViewModel, DT_BaseViewModel )
LINK_ENTITY_TO_CLASS_ALIASED( viewmodel, BaseViewModel );

IMPLEMENT_REFLECT_TABLE( CBaseViewModel, DT_BaseViewModel );

class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

static constexpr auto BEAM_DEFAULT_HALO_SCALE = 10;

//-----------------------------------------------------------------------------
// info targets are like point entities except you can force them to spawn on the client
//-----------------------------------------------------------------------------
class CInfoTarget : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoTarget, CPointEntity );

	void	Spawn_Impl( void );
	 int UpdateTransmitState_Impl();
};

//-----------------------------------------------------------------------------
// Force transmission
//-----------------------------------------------------------------------------
void CInfoTarget::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	if ( HasSpawnFlags(0x01) )
	{
		SetEFlags( EFL_FORCE_CHECK_TRANSMIT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Always transmitted to clients
//-----------------------------------------------------------------------------
int CInfoTarget::UpdateTransmitState_Impl()
{
	// Spawn flags 2 means we always transmit
	if ( HasSpawnFlags(0x02) )
		return SetTransmitState( FL_EDICT_ALWAYS );
	return BaseClass::UpdateTransmitState_Impl();
}

LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );

//-----------------------------------------------------------------------------
// Purpose: Returns true if the given entity is a fixed target for lightning.
//-----------------------------------------------------------------------------
bool IsStaticPointEntity( CBaseEntity *pEnt )
{
	if ( pEnt->GetMoveParent() )
		return false;

	if ( !pEnt->GetModelIndex() )
		return 1;

	if ( FClassnameIs( pEnt, "info_target" ) || FClassnameIs( pEnt, "info_landmark" ) || 
		FClassnameIs( pEnt, "path_corner" ) )
		return true;

	return false;
}

// This table encodes the CBeam data.
IMPLEMENT_NETWORKCLASS_ALIASED( Beam, DT_Beam )
LINK_ENTITY_TO_CLASS_ALIASED( beam, Beam );

IMPLEMENT_REFLECT_TABLE( CBeam, DT_Beam );

IMPLEMENT_REFLECT_DATAMAP( CBeam )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBeam::CBeam( void )
{

	m_flHDRColorScale = 1.0f; // default value.

	m_nDissolveType = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szModelName - 
//-----------------------------------------------------------------------------
void CBeam::SetModel_Impl( const char *szModelName )
{
	int modelIndex = modelinfo->GetModelIndex( szModelName );
	const model_t *model = modelinfo->GetModel( modelIndex );
	if ( model && modelinfo->GetModelType( model ) != mod_sprite )
	{
		Msg( "Setting CBeam to non-sprite model %s\n", szModelName );
	}
	UTIL_SetModel( this, szModelName );
}

void CBeam::Spawn_Impl( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetRenderMode( kRenderTransTexture );

	// Opt out of all shadow routines
	// Beams are highly visible; always render these into reflection buffer even when using fast reflection
	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW | EF_MARKED_FOR_FAST_REFLECTION );

	Precache( );
}

void CBeam::Precache_Impl( void )
{
	if ( GetOwnerEntity() )
	{
		SetStartEntity( GetOwnerEntity() );
	}
	
	if ( m_hEndEntity.Get() )
	{
		SetEndEntity( m_hEndEntity );
	}
}

void CBeam::SetType( int type )		
{ 
	contract_assert( type < NUM_BEAM_TYPES );
	m_nBeamType = type;
}

void CBeam::SetBeamFlags( int flags )	
{ 
	contract_assert( flags < (1 << NUM_BEAM_FLAGS) );
	m_nBeamFlags = flags;
}

void CBeam::SetBeamFlag( int flag )		
{ 
	m_nBeamFlags |= flag;
}

int CBeam::GetType( void ) const 
{ 
	return m_nBeamType;
}

int CBeam::GetBeamFlags( void ) const
{ 
	return m_nBeamFlags;
}

void CBeam::SetStartEntity( CBaseEntity *pEntity )
{ 
	contract_assert( m_nNumBeamEnts >= 2 );
	m_hAttachEntity.Set( 0, pEntity );
	SetOwnerEntity( pEntity );
	RelinkBeam();
	if ( pEntity )
	{
		pEntity->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	}
}

void CBeam::SetEndEntity( CBaseEntity *pEntity ) 
{ 
	contract_assert( m_nNumBeamEnts >= 2 );
	m_hAttachEntity.Set( m_nNumBeamEnts-1, pEntity );
	m_hEndEntity = pEntity;
	RelinkBeam();
	if ( pEntity )
	{
		pEntity->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	}
}

//-----------------------------------------------------------------------------
// This will change things so the abs position matches the requested spot
//-----------------------------------------------------------------------------
void CBeam::SetAbsStartPos( const Vector &pos )
{
	if (!GetMoveParent())
	{
		SetStartPos( pos );
		return;
	}

	Vector vecLocalPos;
	matrix3x4_t worldToBeam;
	MatrixInvert( EntityToWorldTransform(), worldToBeam );
	VectorTransform( pos, worldToBeam, vecLocalPos );
	SetStartPos( vecLocalPos );
}

void CBeam::SetAbsEndPos( const Vector &pos )
{
	if (!GetMoveParent())
	{
		SetEndPos( pos );
		return;
	}

	Vector vecLocalPos;
	matrix3x4_t worldToBeam;
	MatrixInvert( EntityToWorldTransform(), worldToBeam );
	VectorTransform( pos, worldToBeam, vecLocalPos );
	SetEndPos( vecLocalPos );
}

// These don't take attachments into account
const Vector &CBeam::GetAbsStartPos( void ) const
{
	if ( GetType() == BEAM_ENTS && GetStartEntity() )
	{
		edict_t *pent =  INDEXENT( GetStartEntity() );
		CBaseEntity *ent = CBaseEntity::Instance( pent );
		if ( !ent )
		{
			return GetAbsOrigin();
		}
		return ent->GetAbsOrigin();
	}
	return GetAbsOrigin();
}

const Vector &CBeam::GetAbsEndPos( void ) const
{
	if ( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE && GetEndEntity() ) 
	{
		edict_t *pent = INDEXENT( GetEndEntity() );
		CBaseEntity *ent = CBaseEntity::Instance( pent );
		if ( ent )
			return ent->GetAbsOrigin();
	}

	if (!const_cast<CBeam*>(this)->GetMoveParent())
		return m_vecEndPos.Get();

	// FIXME: Cache this off?
	static Vector vecAbsPos;
	VectorTransform( m_vecEndPos, EntityToWorldTransform(), vecAbsPos );
	return vecAbsPos;
}

CBeam *CBeam::BeamCreate( const char *pSpriteName, float width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam  = CREATE_ENTITY( CBeam, "beam" );
	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSprite
//-----------------------------------------------------------------------------
CBeam *CBeam::BeamCreatePredictable( const char *module, int line, bool persist, const char *pSpriteName, float width, CBasePlayer *pOwner )
{
	CBeam *pBeam = ( CBeam * )CBaseEntity::CreatePredictedEntityByName( "beam", module, line, persist );
	if ( pBeam )
	{
		pBeam->BeamInit( pSpriteName, width );
		pBeam->SetOwnerEntity( pOwner );
		pBeam->SetPlayerSimulated( pOwner );
	}

	return pBeam;
}

void CBeam::BeamInit( const char *pSpriteName, float width )
{
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	SetModelName( MAKE_STRING( pSpriteName ) );
	SetRenderMode( kRenderTransTexture );
	SetTexture( PrecacheModel( pSpriteName ) );
	SetWidth( width );
	SetEndWidth( width );
	SetFadeLength( 0 );			// No fade

	Spawn();

	for (int i=0;i<MAX_BEAM_ENTS;i++)
	{
		m_hAttachEntity.Set( i, INVALID_EHANDLE );
		m_nAttachIndex.Set( i, 0 );
	}
	m_nHaloIndex	= 0;
	m_fHaloScale	= BEAM_DEFAULT_HALO_SCALE;
	m_nBeamType		= 0;
	m_nBeamFlags    = 0;
}

void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	m_nNumBeamEnts = 2;
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	m_nNumBeamEnts = 2;
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::PointEntInit( const Vector &start, CBaseEntity *pEndEntity )
{
	SetType( BEAM_ENTPOINT );
	m_nNumBeamEnts = 2;
	SetStartPos( start );
	SetEndEntity( pEndEntity );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( CBaseEntity *pStartEntity, CBaseEntity *pEndEntity )
{
	SetType( BEAM_ENTS );
	m_nNumBeamEnts = 2;
	SetStartEntity( pStartEntity );
	SetEndEntity( pEndEntity );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::LaserInit( CBaseEntity *pStartEntity, CBaseEntity *pEndEntity )
{
	SetType( BEAM_LASER );
	m_nNumBeamEnts = 2;
	SetStartEntity( pStartEntity );
	SetEndEntity( pEndEntity );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::SplineInit( int nNumEnts, CBaseEntity** pEntList, int *attachment )
{
	if (nNumEnts < 2)
	{
		Msg("ERROR: Min of 2 ents required for spline beam.\n");
	}
	else if (nNumEnts > MAX_BEAM_ENTS)
	{
		Msg("ERROR: Max of %i ents allowed for spline beam.\n",MAX_BEAM_ENTS);
	}
	SetType( BEAM_SPLINE );

	for (int i=0;i<nNumEnts;i++)
	{
		m_hAttachEntity.Set( i, pEntList[i] ); 
		m_nAttachIndex.Set( i, attachment[i] );
	}
	m_nNumBeamEnts = nNumEnts;
	RelinkBeam();
}

void CBeam::RelinkBeam( void )
{
	// FIXME: Why doesn't this just define the absbox too?
	// It seems that we don't need to recompute the absbox
	// in CBaseEntity::SetObjectCollisionBox, in fact the absbox
	// computed there seems way too big
	Vector startPos = GetAbsStartPos(), endPos = GetAbsEndPos();

	// UNDONE: Should we do this to make the boxes smaller?
	//SetAbsOrigin( startPos );

	Vector vecBeamMin, vecBeamMax;
	VectorMin( startPos, endPos, vecBeamMin );
	VectorMax( startPos, endPos, vecBeamMax );

	SetCollisionBounds( vecBeamMin - GetAbsOrigin(), vecBeamMax - GetAbsOrigin() );
}

CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	CBaseEntity *pEntity = nullptr;
	CBaseEntity *pNewEntity = nullptr;
	while ((pNewEntity = gEntList.FindEntityByName( pNewEntity, szName )) != nullptr)
	{
		total++;
		if (random_valve->RandomInt(0,total-1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
}

void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if ( HasSpawnFlags(SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND) )
	{
		if ( HasSpawnFlags( SF_BEAM_SPARKSTART ) )
		{
			g_pEffects->Sparks( start );
		}
		if ( HasSpawnFlags( SF_BEAM_SPARKEND ) )
		{
			g_pEffects->Sparks( end );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Damages anything in the beam.
// Input  : ptr - 
//-----------------------------------------------------------------------------
void CBeam::BeamDamage( trace_t *ptr )
{
	RelinkBeam();
	if ( ptr->fraction != 1.0 && ptr->Ent<CBaseEntity>() != nullptr )
	{
		CBaseEntity *pHit = ptr->Ent<CBaseEntity>();
		if ( pHit )
		{
			ClearMultiDamage();
			Vector dir = ptr->endpos - GetAbsOrigin();
			VectorNormalize( dir );
			int nDamageType = DMG_ENERGYBEAM;

			if (m_nDissolveType == 0)
			{
				nDamageType = DMG_DISSOLVE;
			}
			else if ( m_nDissolveType > 0 )
			{
				nDamageType = DMG_DISSOLVE | DMG_SHOCK; 
			}

			CTakeDamageInfo info( this, this, m_flDamage * (gpGlobals->curtime - m_flFireTime), nDamageType );
			CalculateMeleeDamageForce( &info, dir, ptr->endpos );
			pHit->DispatchTraceAttack( info, dir, ptr );
			ApplyMultiDamage();
			if ( HasSpawnFlags( SF_BEAM_DECALS ) )
			{
				if ( pHit->IsBSPModel() )
				{
					UTIL_DecalTrace( ptr, GetDecalName() );
				}
			}
		}
	}
	m_flFireTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeam::TurnOn( void )
{
	AddEffects( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeam::TurnOff( void )
{
	RemoveEffects( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for the beam width. Sets the end width based on the
//			beam width.
// Input  : Beam width in tenths of world units.
//-----------------------------------------------------------------------------
void CBeam::InputWidth( inputdata_t &inputdata )
{
	SetWidth( inputdata.value.Float() );
	SetEndWidth( inputdata.value.Float() );
}

void CBeam::InputColorRedValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( nNewColor, m_clrRender->g, m_clrRender->b );
}

void CBeam::InputColorGreenValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( m_clrRender->r, nNewColor, m_clrRender->b );
}

void CBeam::InputColorBlueValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( m_clrRender->r, m_clrRender->g, nNewColor );
}

void CBeam::InputNoise( inputdata_t &inputdata )
{
	SetNoise( inputdata.value.Float() );
}

int CBeam::UpdateTransmitState_Impl( void )
{
	// we must call ShouldTransmit() if we have a move parent
	if ( GetMoveParent() )
		return SetTransmitState( FL_EDICT_FULLCHECK );

	return BaseClass::UpdateTransmitState_Impl( );
}

void CBeam::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );
	
	// Force our attached entities to go too...
	for ( int i=0; i < MAX_BEAM_ENTS; ++i )
	{
		if ( m_hAttachEntity[i].Get() )
		{
			m_hAttachEntity[i]->SetTransmit( pInfo, bAlways );
		}
	}
}

int CBeam::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return FL_EDICT_DONTSEND;

	// Transmit us with the same rules as our move parent
	if ( GetMoveParent() )
	{
		return GetMoveParent()->ShouldTransmit( pInfo );
	}

	return BaseClass::ShouldTransmit_Impl( pInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays.
// Output : Returns the new text offset from the top.
//-----------------------------------------------------------------------------
int CBeam::DrawDebugTextOverlays_Impl(void)
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();
	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// Print state
		char tempstr[512];
		V_snprintf(tempstr, sizeof(tempstr), "start: (%.2f,%.2f,%.2f)", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr, sizeof(tempstr), "end  : (%.2f,%.2f,%.2f)", m_vecEndPos.GetX(), m_vecEndPos.GetY(), m_vecEndPos.GetZ());
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	
	return text_offset;
}

//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
int CBeam::ObjectCaps_Impl( void )
{ 
	int flags = 0;
	if ( HasSpawnFlags( SF_BEAM_TEMPORARY ) )
		flags = FCAP_DONT_SAVE;
	return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) | flags; 
}

void	CBeam::SetFireTime( float flFireTime )		
{ 
	m_flFireTime = flFireTime; 
}

//-----------------------------------------------------------------------------
// NOTE: Start + End Pos are specified in *relative* coordinates 
//-----------------------------------------------------------------------------
void CBeam::SetStartPos( const Vector &pos ) 
{ 
	SetLocalOrigin( pos );
}

void CBeam::SetEndPos( const Vector &pos ) 
{ 
	m_vecEndPos = pos; 
}

 // center point of beam
const Vector &CBeam::WorldSpaceCenter_Impl( void ) const 
{
	Vector &vecResult = AllocTempVector();
	VectorAdd( GetAbsStartPos(), GetAbsEndPos(), vecResult );
	vecResult *= 0.5f;
	return vecResult;
}

void CBeam::SetStartAttachment( int attachment )	
{
	contract_assert( (attachment & ~ATTACHMENT_INDEX_MASK) == 0 );
	m_nAttachIndex.Set( 0, attachment );
}

void CBeam::SetEndAttachment( int attachment )		
{ 
	contract_assert( (attachment & ~ATTACHMENT_INDEX_MASK) == 0 );
	m_nAttachIndex.Set( m_nNumBeamEnts-1, attachment );
}

void CBeam::SetTexture( int spriteIndex )		
{ 
	SetModelIndex( spriteIndex ); 
}

void CBeam::SetHaloTexture( int spriteIndex )	
{ 
	m_nHaloIndex = spriteIndex; 
}

void CBeam::SetHaloScale( float haloScale )		
{ 
	m_fHaloScale = haloScale; 
}

void CBeam::SetWidth( float width )				
{
	contract_assert( width <= MAX_BEAM_WIDTH );
	m_fWidth = std::min( MAX_BEAM_WIDTH, width );
}

void CBeam::SetEndWidth( float endWidth )		
{ 
	contract_assert( endWidth <= MAX_BEAM_WIDTH );
	m_fEndWidth	= std::min( MAX_BEAM_WIDTH, endWidth );
}

void CBeam::SetFadeLength( float fadeLength )	
{ 
	m_fFadeLength = fadeLength; 
}

void CBeam::SetNoise( float amplitude )			
{ 
	m_fAmplitude = amplitude; 
}

void CBeam::SetColor( int r, int g, int b )		
{ 
	SetRenderColor( r, g, b );
}

void CBeam::SetBrightness( int brightness )		
{ 
	SetRenderAlpha( brightness ); 
}

void CBeam::SetFrame( float frame )				
{ 
	m_fStartFrame = frame; 
}

void CBeam::SetScrollRate( int speed )			
{ 
	m_fSpeed = speed; 
}

CBaseEntity* CBeam::GetStartEntityPtr( void ) const 
{ 
	return m_hAttachEntity[0].Get(); 
}

int CBeam::GetStartEntity( void ) const 
{ 
	CBaseEntity *pEntity = m_hAttachEntity[0].Get();
	return pEntity ? pEntity->entindex() : 0; 
}

CBaseEntity* CBeam::GetEndEntityPtr( void ) const 
{ 
	return m_hAttachEntity[1].Get(); 
}

int CBeam::GetEndEntity( void ) const	
{ 
	CBaseEntity *pEntity = m_hAttachEntity[m_nNumBeamEnts-1].Get();
	return pEntity ? pEntity->entindex() : 0; 
}

int CBeam::GetStartAttachment() const
{
	return m_nAttachIndex[0] & ATTACHMENT_INDEX_MASK;
}

int CBeam::GetEndAttachment() const
{
	return m_nAttachIndex[m_nNumBeamEnts-1] & ATTACHMENT_INDEX_MASK;
}

int CBeam::GetTexture( void )		
{ 
	return GetModelIndex(); 
}

int CBeam::GetHaloTexture( void ) const
{
	return m_nHaloIndex;
}

float CBeam::GetWidth( void ) const		
{
	return m_fWidth; 
}

float CBeam::GetEndWidth( void ) const	
{ 
	return m_fEndWidth; 
}

float CBeam::GetFadeLength( void ) const	
{ 
	return m_fFadeLength; 
}

float CBeam::GetNoise( void ) const		
{ 
	return m_fAmplitude; 
}

float CBeam::GetHaloScale( void ) const
{
	return m_fHaloScale;
}

int CBeam::GetBrightness( void ) const	
{ 
	return GetRenderAlpha();
}

float CBeam::GetFrame( void ) const		
{ 
	return m_fStartFrame; 
}

float CBeam::GetScrollRate( void ) const	
{
	return m_fSpeed; 
}

float CBeam::GetHDRColorScale( void ) const
{
	return m_flHDRColorScale;
}

void CBeam::LiveForTime( float time ) 
{ 
	SetThink(&CBeam::SUB_Remove); 
	SetNextThink( gpGlobals->curtime + time ); 
}

void	CBeam::BeamDamageInstant( trace_t *ptr, float damage ) 
{ 
	m_flDamage = damage; 
	m_flFireTime = gpGlobals->curtime - 1;
	BeamDamage(ptr); 
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: A special kind of beam effect that traces from its start position to
//			its end position and stops if it hits anything.

// Spawnflags
enum BeamSpotlightSpawnFlags_t
{
	SF_BEAM_SPOTLIGHT_START_LIGHT_ON    = 1,
	SF_BEAM_SPOTLIGHT_NO_DYNAMIC_LIGHT  = 2,
	SF_BEAM_SPOTLIGHT_START_ROTATE_ON   = 4,
	SF_BEAM_SPOTLIGHT_REVERSE_DIRECTION = 8,
	SF_BEAM_SPOTLIGHT_X_AXIS            = 16,
	SF_BEAM_SPOTLIGHT_Y_AXIS            = 32,
};

class CBeamSpotlight : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BeamSpotlight" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBeamSpotlight, CBaseEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CBeamSpotlight();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	[[= ks::reflect::Input{ .name = "LightOn",  .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Start",    .type = FIELD_VOID } ]] void InputStart( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop",     .type = FIELD_VOID } ]] void InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Reverse",  .type = FIELD_VOID } ]] void InputReverse( inputdata_t &inputdata );

protected:
	public: using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	protected:

private:
	public: int  UpdateTransmitState_Impl();
	private:
	void RecalcRotation( void );

	CNetworkVar( int, m_nHaloIndex , [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]]);

	CNetworkVar( bool, m_bSpotlightOn , [[= ks::reflect::Net{} ]]);
	CNetworkVar( bool, m_bHasDynamicLight , [[= ks::reflect::Net{} ]]);

	CNetworkVar( float, m_flSpotlightMaxLength, [[= ks::reflect::Key{ .name = "SpotlightLength" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);
	CNetworkVar( float,	m_flSpotlightGoalWidth, [[= ks::reflect::Key{ .name = "SpotlightWidth" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);
	CNetworkVar( float,	m_flHDRColorScale, [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);
	CNetworkVar( int, m_nMinDXLevel );
	CNetworkVar( int, m_nRotationAxis , [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]]);
	CNetworkVar( float, m_flRotationSpeed , [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);

	[[= ks::reflect::Key{ .name = "maxspeed" } ]] float m_flmaxSpeed;
	bool m_isRotating;
	bool m_isReversed;

public:
	// Split from one declaration: each output needs its own external name.
	[[= ks::reflect::Key{ .name = "OnLightOn" } ]]  COutputEvent m_OnOn;
	[[= ks::reflect::Key{ .name = "OnLightOff" } ]] COutputEvent m_OnOff;
};

LINK_ENTITY_TO_CLASS( beam_spotlight, CBeamSpotlight );

IMPLEMENT_REFLECT_DATAMAP( CBeamSpotlight )

IMPLEMENT_REFLECT_SERVERCLASS( CBeamSpotlight, DT_BeamSpotlight )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBeamSpotlight::CBeamSpotlight()
: m_bSpotlightOn( false )
, m_bHasDynamicLight( true )
, m_flSpotlightMaxLength( 500.0f )
, m_flSpotlightGoalWidth( 50.0f )
, m_flHDRColorScale( 0.7f )
, m_isRotating( false )
, m_isReversed( false )
, m_flmaxSpeed( 100.0f )
, m_flRotationSpeed(0.0f)
, m_nRotationAxis(0)
{
}

bool CBeamSpotlight::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( !V_stricmp( szKeyName, "SpotlightWidth" ) )
	{
		m_flSpotlightGoalWidth = V_atof(szValue);
		if ( m_flSpotlightGoalWidth > MAX_BEAM_WIDTH )
		{
			Warning( "Map Bug:  %s has SpotLightWidth %f > %f, clamping value\n",
					 STRING( GetEntityName()), m_flSpotlightGoalWidth.m_Value, (float)MAX_BEAM_WIDTH );
			m_flSpotlightGoalWidth = Min( MAX_BEAM_WIDTH, m_flSpotlightGoalWidth.Get() );
		}
		return true;
	}
	
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSpotlight::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	// Sprites.
	m_nHaloIndex = PrecacheModel("sprites/light_glow03.vmt");
	PrecacheModel( "sprites/glow_test02.vmt" );
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::RecalcRotation( void )
{
	if ( !m_isRotating || m_flmaxSpeed == 0.0f ) 
	{
		m_flRotationSpeed = 0.0f;
		return;
	}

	//
	// Build the axis of rotation based on spawnflags.
	//
	// Pitch Yaw Roll -> Y Z X
	m_nRotationAxis = 1;
	if ( HasSpawnFlags(SF_BEAM_SPOTLIGHT_Y_AXIS) )
	{
		m_nRotationAxis = 0;
	}
	else if ( HasSpawnFlags(SF_BEAM_SPOTLIGHT_X_AXIS) )
	{
		m_nRotationAxis = 2;
	}

	m_flRotationSpeed = m_flmaxSpeed;
	if ( m_isReversed ) 
	{
		m_flRotationSpeed *= -1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSpotlight::Spawn_Impl( void )
{
	Precache();

	UTIL_SetSize( this,vec3_origin,vec3_origin );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	m_bHasDynamicLight = !HasSpawnFlags( SF_BEAM_SPOTLIGHT_NO_DYNAMIC_LIGHT);
	m_bSpotlightOn = HasSpawnFlags( SF_BEAM_SPOTLIGHT_START_LIGHT_ON );
	m_isRotating = HasSpawnFlags( SF_BEAM_SPOTLIGHT_START_ROTATE_ON );
	m_isReversed = HasSpawnFlags( SF_BEAM_SPOTLIGHT_REVERSE_DIRECTION );

	RecalcRotation();
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputTurnOn( inputdata_t &inputdata )
{
	if ( !m_bSpotlightOn )
	{
		m_bSpotlightOn = true;
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputTurnOff( inputdata_t &inputdata )
{
	if ( m_bSpotlightOn )
	{
		m_bSpotlightOn = false;
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputStart( inputdata_t &inputdata )
{
	if ( !m_isRotating )
	{
		m_isRotating = true;
		RecalcRotation();
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputStop( inputdata_t &inputdata )
{
	if ( m_isRotating )
	{
		m_isRotating = false;
		RecalcRotation();
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputReverse( inputdata_t &inputdata )
{
	m_isReversed = !m_isReversed;
	RecalcRotation();
}

//-------------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model so spotlight gets proper position
//-------------------------------------------------------------------------------------
int CBeamSpotlight::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_PVSCHECK );
}

// Purpose: Arbitrary length bit string
//				** NOTE: This class does NOT override the bitwise operators
//						 as doing so would require overriding the operators
//						 to allocate memory for the returned bitstring.  This method
//						 would be prone to memory leaks as the calling party
//						 would have to remember to delete the memory.  Funtions
//						 are used instead to require the calling party to allocate
//						 and destroy their own memory

class CBaseEntity;
class CBaseFilter;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Spawn, think, and use functions for common brush entities.

static constexpr auto SF_BRUSH_ACCDCC = 16; // brush should accelerate and decelerate when toggled
static constexpr auto SF_BRUSH_HURT = 32; // rotating brush that inflicts pain based on rotation speed
static constexpr auto SF_ROTATING_NOT_SOLID = 64; // some special rotating objects are not solid.

// =================== FUNC_WALL ==============================================
class CFuncWall : public CBaseEntity
{
public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncWall, CBaseEntity );
	void	Spawn_Impl( void );
	bool	CreateVPhysics_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_nState;
};

LINK_ENTITY_TO_CLASS( func_wall, CFuncWall );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncWall )

void CFuncWall::Spawn_Impl( void )
{
	SetLocalAngles( vec3_angle );
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetModel( STRING( GetModelName() ) );
	
	// If it can't move/go away, it's really part of the world
	AddFlag( FL_WORLDBRUSH );

	// set manual mode
	CreateVPhysics();
}

bool CFuncWall::CreateVPhysics_Impl( void )
{
	SetSolid( SOLID_BSP );
	IPhysicsObject *pPhys = VPhysicsInitStatic();
	if ( pPhys )
	{
		int contents = modelinfo->GetModelContents( GetModelIndex() );
		if ( ! (contents & (MASK_SOLID|MASK_PLAYERSOLID|MASK_NPCSOLID)) )
		{
			// leave the physics shadow there in case it has crap constrained to it
			// but disable collisions with it
			pPhys->EnableCollisions( false );
		}
	}

	return true;
}

void CFuncWall::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, m_nState ) )
	{
		m_nState = 1 - m_nState;
	}
}

#define SF_WALL_START_OFF		0x0001

class CFuncWallToggle : public CFuncWall
{
public:
	DECLARE_CLASS( CFuncWallToggle, CFuncWall );

	DECLARE_DATADESC();

	void	Spawn_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle( inputdata_t &inputdata );

	void	TurnOff( void );
	void	TurnOn( void );
	bool	IsOn( void );
};

IMPLEMENT_REFLECT_DATAMAP( CFuncWallToggle )

LINK_ENTITY_TO_CLASS( func_wall_toggle, CFuncWallToggle );

void CFuncWallToggle::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();
	if ( HasSpawnFlags( SF_WALL_START_OFF ) )
		TurnOff();
	
	SetMoveType( MOVETYPE_PUSH );
}

void CFuncWallToggle::TurnOff( void )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableCollisions( false );
	}
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW );
}

void CFuncWallToggle::TurnOn( void )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableCollisions( true );
	}
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	RemoveEffects( EF_NODRAW );
}

bool CFuncWallToggle::IsOn( void )
{
	if ( IsSolidFlagSet( FSOLID_NOT_SOLID ) )
		return false;
	return true;
}

void CFuncWallToggle::InputToggle( inputdata_t &inputdata )
{
	int status = IsOn();

	if ( ShouldToggle( USE_TOGGLE, status ) )
	{
		if ( status )
			TurnOff();
		else
			TurnOn();
	}
}

//Adrian - Is this function needed at all?
void CFuncWallToggle::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int status = IsOn();

	if ( ShouldToggle( useType, status ) )
	{
		if ( status )
			TurnOff();
		else
			TurnOn();
	}
}

//============================== FUNC_VEHICLECLIP =====================================
class CFuncVehicleClip : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncVehicleClip, CBaseEntity );
	DECLARE_DATADESC();

	void Spawn_Impl();
	bool CreateVPhysics_Impl( void );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &data );

private:
};

IMPLEMENT_REFLECT_DATAMAP( CFuncVehicleClip )

LINK_ENTITY_TO_CLASS( func_vehicleclip, CFuncVehicleClip );

void CFuncVehicleClip::Spawn_Impl()
{

	SetLocalAngles( vec3_angle );
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetModel( STRING( GetModelName() ) );
	
	// It's part of the world
	AddFlag( FL_WORLDBRUSH );

	CreateVPhysics();

	AddEffects( EF_NODRAW );		// make entity invisible

	SetCollisionGroup( COLLISION_GROUP_VEHICLE_CLIP );
}

bool CFuncVehicleClip::CreateVPhysics_Impl( void )
{
	SetSolid( SOLID_BSP );
	VPhysicsInitStatic();

	return true;
}

void CFuncVehicleClip::InputEnable( inputdata_t &data )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableCollisions( true );
	}
	RemoveSolidFlags( FSOLID_NOT_SOLID );
}

void CFuncVehicleClip::InputDisable( inputdata_t &data )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableCollisions( false );
	}
	AddSolidFlags( FSOLID_NOT_SOLID );
}

//============================= FUNC_CONVEYOR =======================================

static constexpr auto SF_CONVEYOR_VISUAL = 0x0001;
static constexpr auto SF_CONVEYOR_NOTSOLID = 0x0002;

class CFuncConveyor : public CFuncWall
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncConveyor" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncConveyor, CFuncWall );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CFuncConveyor();

	void	Spawn_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UpdateSpeed( float flNewSpeed );

	void	GetGroundVelocityToApply_Impl( Vector &vecGroundVel );

	// Input handlers.
	[[= ks::reflect::Input{ .name = "ToggleDirection", .type = FIELD_VOID } ]] void	InputToggleDirection( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_VOID } ]] void	InputSetSpeed( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector m_vecMoveDir;
	CNetworkVar( float, m_flConveyorSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};

LINK_ENTITY_TO_CLASS( func_conveyor, CFuncConveyor );

IMPLEMENT_REFLECT_DATAMAP( CFuncConveyor )

IMPLEMENT_REFLECT_SERVERCLASS( CFuncConveyor, DT_FuncConveyor )

CFuncConveyor::CFuncConveyor()
{
	m_flConveyorSpeed = 0.0;
}

void CFuncConveyor::Spawn_Impl( void )
{
	// Convert movedir from angles to a vector
	QAngle angMoveDir = QAngle( m_vecMoveDir.x, m_vecMoveDir.y, m_vecMoveDir.z );
	AngleVectors( angMoveDir, &m_vecMoveDir );

	BaseClass::Spawn_Impl();

	if ( !HasSpawnFlags(SF_CONVEYOR_VISUAL) )
		AddFlag( FL_CONVEYOR );

	// HACKHACK - This is to allow for some special effects
	if ( HasSpawnFlags( SF_CONVEYOR_NOTSOLID ) )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	if ( m_flSpeed == 0 )
		m_flSpeed = 100;

	UpdateSpeed( m_flSpeed );
}

void CFuncConveyor::UpdateSpeed( float flNewSpeed )
{
	m_flConveyorSpeed = flNewSpeed;
}

void CFuncConveyor::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
    m_flSpeed = -m_flSpeed;
	UpdateSpeed( m_flSpeed );
}

void CFuncConveyor::InputToggleDirection( inputdata_t &inputdata )
{
	Use( inputdata.pActivator, inputdata.pCaller, USE_TOGGLE, 0 );
}

void CFuncConveyor::InputSetSpeed( inputdata_t &inputdata )
{
    m_flSpeed = inputdata.value.Float();
	UpdateSpeed( m_flSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the velocity imparted to players standing on us.
//-----------------------------------------------------------------------------
void CFuncConveyor::GetGroundVelocityToApply_Impl( Vector &vecGroundVel )
{
	vecGroundVel = m_vecMoveDir * m_flSpeed;
}

// =================== FUNC_ILLUSIONARY ==============================================
// A simple entity that looks solid but lets you walk through it.
class CFuncIllusionary : public CBaseEntity 
{
	DECLARE_CLASS( CFuncIllusionary, CBaseEntity );
public:
	void Spawn_Impl( void );
};

LINK_ENTITY_TO_CLASS( func_illusionary, CFuncIllusionary );

void CFuncIllusionary::Spawn_Impl( void )
{
	SetLocalAngles( vec3_angle );
	SetMoveType( MOVETYPE_NONE );  
	SetSolid( SOLID_NONE );
	SetModel( STRING( GetModelName() ) );
}

//-----------------------------------------------------------------------------
// Purpose: A rotating brush entity.
//
//			You need to have an origin brush as part of this entity.  The  
//			center of that brush will be the point around which it is rotated.
//
//			It will rotate around the Z axis by default. Spawnflags can be set
//			to make it rotate around the X or Y axes.
//
//			The direction of rotation is also controlled by a spawnflag.
//-----------------------------------------------------------------------------
void SendProxy_FuncRotatingAngle( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void SendProxy_FuncRotatingOrigin( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void SendProxy_FuncRotatingSimulationTime( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class CFuncRotating : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncRotating" } ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::Vector }, SendProxy_FuncRotatingOrigin>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .flags = SPROP_CHANGES_OFTEN, .index = 0 }, SendProxy_FuncRotatingAngle>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .flags = SPROP_CHANGES_OFTEN, .index = 1 }, SendProxy_FuncRotatingAngle>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .flags = SPROP_CHANGES_OFTEN, .index = 2 }, SendProxy_FuncRotatingAngle>{} ]]
	[[= ks::reflect::From<"m_flSimulationTime", ks::reflect::Net{ .bits = SIMULATION_TIME_WINDOW_BITS, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN|SPROP_ENCODED_AGAINST_TICKCOUNT, .enc = ks::reflect::WireEnc::Int }, SendProxy_FuncRotatingSimulationTime>{} ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_angRotation" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_vecOrigin" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseEntity", .prop = "m_flSimulationTime" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CFuncRotating, CBaseEntity );
public:
	// basic functions
	void Spawn_Impl( void  );
	void Precache_Impl( void  );
	bool CreateVPhysics_Impl( void );
	void SpinUpMove( void );
	void SpinDownMove( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void HurtTouch ( CBaseEntity *pOther );
	void RotatingUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RotateMove( void );
	void ReverseMove( void );
	void RampPitchVol( void );
	void Blocked_Impl( CBaseEntity *pOther );
	void SetTargetSpeed( float flSpeed );
	void UpdateSpeed( float flNewSpeed );
	
	int	 DrawDebugTextOverlays_Impl(void);

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

protected:
	bool SpinDown( float flTargetSpeed );
	float GetMoveSpeed( float flSpeed );

	float GetNextMoveInterval() const;

	// If our euler angle is starting to get too big, normalize it
	void NormalizeAngleIfNeeded();

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GetSpeed", .type = FIELD_VOID } ]] void InputGetSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Start", .type = FIELD_VOID } ]] void InputStart( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop", .type = FIELD_VOID } ]] void InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartForward", .type = FIELD_VOID } ]] void InputStartForward( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartBackward", .type = FIELD_VOID } ]] void InputStartBackward( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Reverse", .type = FIELD_VOID } ]] void InputReverse( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopAtStartPos", .type = FIELD_VOID } ]] void InputStopAtStartPos( inputdata_t &inputdata );

	QAngle	m_vecMoveAng;

	float m_flFanFriction;
	float m_flAttenuation;
	float m_flVolume;
	float m_flTargetSpeed;			// Target value for m_flSpeed, used for spinning up and down.
	[[= ks::reflect::Key{ .name = "maxspeed" } ]] float m_flMaxSpeed;				// Maximum value for m_flSpeed, used for ramping sound effects.
	[[= ks::reflect::Key{ .name = "dmg" } ]] float m_flBlockDamage;			// Damage inflicted when blocked.
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "message" } ]] string_t m_NoiseRunning;
	bool m_bReversed;

	QAngle	m_angStart;
	bool m_bStopAtStartPos;

	[[= ks::reflect::Key{ .name = "solidbsp" } ]] bool m_bSolidBsp;				// Brush is SOLID_BSP

	//outputs
	[[= ks::reflect::Key{ .name = "OnGetSpeed" } ]] COutputFloat m_OnGetSpeed;	// Used for polling the speed value.

public:
	Vector m_vecClientOrigin;
	QAngle m_vecClientAngles;
};

LINK_ENTITY_TO_CLASS( func_rotating, CFuncRotating );

IMPLEMENT_REFLECT_DATAMAP( CFuncRotating )

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
void SendProxy_FuncRotatingOrigin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CFuncRotating *entity = (CFuncRotating*)pStruct;
	contract_assert( entity );
	if ( entity->HasSpawnFlags(SF_BRUSH_ROTATE_CLIENTSIDE) )
	{
		const Vector *v = &entity->m_vecClientOrigin;
		pOut->m_Vector[ 0 ] = v->x;
		pOut->m_Vector[ 1 ] = v->y;
		pOut->m_Vector[ 2 ] = v->z;
		return;
	}

	SendProxy_Origin( pProp, pStruct, pData, pOut, iElement, objectID );
}

void SendProxy_FuncRotatingAngle( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID)
{
	vec_t const *qa = (vec_t *)pData;

	pOut->m_Float = anglemod( *qa );

	contract_assert( IsFinite( pOut->m_Float ) );
}

extern void SendProxy_SimulationTime( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void SendProxy_FuncRotatingSimulationTime( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID )
{

	SendProxy_SimulationTime( pProp, pStruct, pVarData, pOut, iElement, objectID );
}

IMPLEMENT_REFLECT_SERVERCLASS( CFuncRotating, DT_FuncRotating )

//-----------------------------------------------------------------------------
// Purpose: Handles keyvalues from the BSP. Called before spawning.
//-----------------------------------------------------------------------------
bool CFuncRotating::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "fanfriction"))
	{
		m_flFanFriction = std::atof(szValue)/100;
	}
	else if (FStrEq(szKeyName, "Volume"))
	{
		m_flVolume = std::atof(szValue) / 10.0;
		m_flVolume = clamp(m_flVolume, 0.0, 1.0f);
	}
	else
	{ 
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been set.
//-----------------------------------------------------------------------------
void CFuncRotating::Spawn_Impl( )
{

	//
	// Maintain compatibility with previous maps.
	//
	if (m_flVolume == 0.0)
	{
		m_flVolume = 1.0;
	}

	//
	// If the designer didn't set a sound attenuation, default to one.
	//
	if ( HasSpawnFlags(SF_BRUSH_ROTATE_SMALLRADIUS) )
	{
		m_flAttenuation = ATTN_IDLE;
	}
	else if ( HasSpawnFlags(SF_BRUSH_ROTATE_MEDIUMRADIUS) )
	{
		m_flAttenuation = ATTN_STATIC;
	}
	else if ( HasSpawnFlags(SF_BRUSH_ROTATE_LARGERADIUS) )
	{
		m_flAttenuation = ATTN_NORM;
	}
	else
	{
		m_flAttenuation = ATTN_NORM;
	}

	//
	// Prevent divide by zero if level designer forgets friction!
	//
	if ( m_flFanFriction == 0 )
	{
		m_flFanFriction = 1;
	}
	
	//
	// Build the axis of rotation based on spawnflags.
	//
	if ( HasSpawnFlags(SF_BRUSH_ROTATE_Z_AXIS) )
	{
		m_vecMoveAng = QAngle(0,0,1);
	}
	else if ( HasSpawnFlags(SF_BRUSH_ROTATE_X_AXIS) )
	{
		m_vecMoveAng = QAngle(1,0,0);
	}
	else
	{
		m_vecMoveAng = QAngle(0,1,0);	// y-axis
	}

	//
	// Check for reverse rotation.
	//
	if ( HasSpawnFlags(SF_BRUSH_ROTATE_BACKWARDS) )
	{
		m_vecMoveAng = m_vecMoveAng * -1;
	}

	SetSolid( SOLID_VPHYSICS );

	//
	// Some rotating objects like fake volumetric lights will not be solid.
	//
	if ( HasSpawnFlags(SF_ROTATING_NOT_SOLID) )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
		SetMoveType( MOVETYPE_PUSH );
	}
	else
	{
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		SetMoveType( MOVETYPE_PUSH );
	}

	SetModel( STRING( GetModelName() ) );

	SetUse( &CFuncRotating::RotatingUse );

	//
	// Did level designer forget to assign a maximum speed? Prevent a divide by
	// zero in RampPitchVol as well as allowing the rotator to work.
	//
	m_flMaxSpeed = fabs( m_flMaxSpeed );
	if (m_flMaxSpeed == 0)
	{
		m_flMaxSpeed = 100;
	}

	//
	// If the brush should be initially rotating, use it in a little while.
	//
	if ( HasSpawnFlags(SF_BRUSH_ROTATE_START_ON) )
	{		
		SetThink( &CFuncRotating::SUB_CallUseToggle );
		SetNextThink( gpGlobals->curtime + .2 );	// leave a magic delay for client to start up
	}
	else
	{
		SetThink( &CFuncRotating::NormalizeAngleIfNeeded );
		SetNextThink( gpGlobals->curtime + .2 );
	}

	//
	// Can this brush inflict pain?
	//
	if ( HasSpawnFlags(SF_BRUSH_HURT) )
	{
		SetTouch( &CFuncRotating::HurtTouch );
	}

	//
	// Set speed to 0 in case there's an old "speed" key lying around.
	//
	m_flSpeed = 0;
	
	Precache( );
	CreateVPhysics();

	m_angStart = GetLocalAngles();
	
	// Slam the object back to solid - if we really want it to be solid.
	if ( m_bSolidBsp )
	{
		SetSolid( SOLID_BSP );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFuncRotating::CreateVPhysics_Impl( void )
{
	if ( !IsSolidFlagSet( FSOLID_NOT_SOLID ))
	{
		VPhysicsInitShadow( false, false );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRotating::Precache_Impl( void )
{
	//
	// Set up rotation sound.
	//
	char *szSoundFile = ( char * )STRING( m_NoiseRunning );
	if ( !m_NoiseRunning || std::strlen( szSoundFile ) == 0 )
	{
		// No sound set up, use the null sound.
		m_NoiseRunning = AllocPooledString("DoorSound.Null");
	}
	PrecacheScriptSound( STRING( m_NoiseRunning ) );
	
	if (GetLocalAngularVelocity() != vec3_angle )
	{
		//
		// If fan was spinning, and we went through transition or save/restore,
		// make sure we restart the sound.  1.5 sec delay is a magic number.
		//
		SetMoveDone( &CFuncRotating::SpinUpMove );
		SetMoveDoneTime( 1.5 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Will hurt others based on how fast the brush is spinning.
// Input  : pOther - 
//-----------------------------------------------------------------------------
void CFuncRotating::HurtTouch ( CBaseEntity *pOther )
{
	// we can't hurt this thing, so we're not concerned with it
	if ( !pOther->m_takedamage )
		return;

	// calculate damage based on rotation speed
	m_flBlockDamage = GetLocalAngularVelocity().Length() / 10;

	pOther->TakeDamage( CTakeDamageInfo( this, this, m_flBlockDamage, DMG_CRUSH ) );

	Vector vecNewVelocity = pOther->GetAbsOrigin() - WorldSpaceCenter();
	VectorNormalize(vecNewVelocity);
	vecNewVelocity *= m_flBlockDamage;
	pOther->SetAbsVelocity( vecNewVelocity );
}

static constexpr auto FANPITCHMIN = 30;
static constexpr auto FANPITCHMAX = 100;

//-----------------------------------------------------------------------------
// Purpose: Ramp pitch and volume up to maximum values, based on the difference
//			between how fast we're going vs how fast we can go.
//-----------------------------------------------------------------------------
void CFuncRotating::RampPitchVol( void )
{
	//
	// Calc volume and pitch as % of maximum vol and pitch.
	//
	float fpct = fabs(m_flSpeed) / m_flMaxSpeed;
	float fvol = clamp(m_flVolume * fpct, 0, 1);			  // slowdown volume ramps down to 0

	float fpitch = FANPITCHMIN + (FANPITCHMAX - FANPITCHMIN) * fpct;	
	
	int pitch = clamp(fpitch, 0, 255);
	if (pitch == PITCH_NORM)
	{
		pitch = PITCH_NORM - 1;
	}

	//
	// Update the fan's volume and pitch.
	//
	CBroadcastRecipientFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_STATIC;
	ep.m_pSoundName = STRING(m_NoiseRunning);
	ep.m_flVolume = fvol;
	ep.m_SoundLevel = ATTN_TO_SNDLVL( m_flAttenuation );
	ep.m_nFlags = SND_CHANGE_PITCH | SND_CHANGE_VOL;
	ep.m_nPitch = pitch;

	EmitSound( filter, entindex(), ep );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CFuncRotating::GetNextMoveInterval() const
{
	if ( m_bStopAtStartPos )
	{
		return TICK_INTERVAL;
	}
	return 0.1f;
}

//-----------------------------------------------------------------------------
// Purpose: stop our rotation value from becoming too large
//-----------------------------------------------------------------------------
void CFuncRotating::NormalizeAngleIfNeeded()
{
	// entity max angle is 1000 rotations, so we'll renormalize after ~200.
	// note that we expect this to be a multiple of 360
	static const float kMaxAngle = 200 * 360.0f;
	
	// check our rotation for normalization every 15-30 seconds.
	static const float kTimeToRenormalize = 15.0f;

	// check angle
	QAngle angle = GetLocalAngles();
	bool ok = true;
	if ( angle.x > kMaxAngle )  { ok = false; angle.x -= kMaxAngle; }
	if ( angle.x < -kMaxAngle ) { ok = false; angle.x += kMaxAngle; }
	if ( angle.y > kMaxAngle )  { ok = false; angle.y -= kMaxAngle; }
	if ( angle.y < -kMaxAngle ) { ok = false; angle.y += kMaxAngle; }
	if ( angle.z > kMaxAngle )  { ok = false; angle.z -= kMaxAngle; }
	if ( angle.z < -kMaxAngle ) { ok = false; angle.z += kMaxAngle; }

	// if we changed it, update with new values
	if ( !ok )
	{
		// Disable interpolation on this entity for 1 frame (so it doesn't 'spin backwards 100 times')
		AddEffects( EF_NOINTERP );
		SetLocalAngles( angle );
	}

	// think at semi-random intervals so func rotatings don't all stack up on the same tick
	SetThink( &CFuncRotating::NormalizeAngleIfNeeded );
	SetNextThink( gpGlobals->curtime + RandomFloat( kTimeToRenormalize, kTimeToRenormalize * 2.0f ) );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the current speed to the given value and manages the sound effects.
// Input  : flNewSpeed - New speed in degrees per second.
//-----------------------------------------------------------------------------
void CFuncRotating::UpdateSpeed( float flNewSpeed )
{
	float flOldSpeed = m_flSpeed;
	m_flSpeed = clamp( flNewSpeed, -m_flMaxSpeed, m_flMaxSpeed );

	if ( m_bStopAtStartPos )
	{
		int checkAxis = 2;
		// See if we got close to the starting orientation
		if ( m_vecMoveAng[0] != 0 )
		{
			checkAxis = 0;
		}
		else if ( m_vecMoveAng[1] != 0 )
		{
			checkAxis = 1;
		}

		float angDelta = anglemod( GetLocalAngles()[ checkAxis ] - m_angStart[ checkAxis ] );
		if ( angDelta > 180.0f )
		{
			angDelta -= 360.0f;
		}

		if ( flNewSpeed < 100 )
		{
			if ( flNewSpeed <= 25 && fabs( angDelta ) < 1.0f )
			{
				m_flTargetSpeed = 0;
				m_bStopAtStartPos = false;
				m_flSpeed = 0.0f;

				SetLocalAngles( m_angStart );
			}
			else if ( fabs( angDelta ) > 90.0f )
			{
				// Keep rotating at same speed for now
				m_flSpeed = flOldSpeed;
			}
			else
			{
				float minSpeed =  fabs( angDelta );
				if ( minSpeed < 20 )
					minSpeed = 20;
	
				m_flSpeed = flOldSpeed > 0.0f ? minSpeed : -minSpeed;
			}
		}
	}

	if ( ( flOldSpeed == 0 ) && ( m_flSpeed != 0 ) )
	{
		// Starting to move - emit the sound.
		CBroadcastRecipientFilter filter;
	
		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = STRING(m_NoiseRunning);
		ep.m_flVolume = 0.01;
		ep.m_SoundLevel = ATTN_TO_SNDLVL( m_flAttenuation );
		ep.m_nPitch = FANPITCHMIN;

		EmitSound( filter, entindex(), ep );
		RampPitchVol();
	}
	else if ( ( flOldSpeed != 0 ) && ( m_flSpeed == 0 ) )
	{
		// Stopping - stop the sound.
		StopSound( entindex(), CHAN_STATIC, STRING(m_NoiseRunning) );
		
	}
	else
	{
		// Changing speed - adjust the pitch and volume.
		RampPitchVol();
	}

	SetLocalAngularVelocity( m_vecMoveAng * m_flSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: Think function. Accelerates a func_rotating to a higher angular velocity.
//-----------------------------------------------------------------------------
void CFuncRotating::SpinUpMove( void )
{
	//
	// Calculate our new speed.
	//
	bool bSpinUpDone = false;
	float flNewSpeed = fabs( m_flSpeed ) + 0.2 * m_flMaxSpeed * m_flFanFriction;
	if ( fabs( flNewSpeed ) >=  fabs( m_flTargetSpeed ) )
	{
		// Reached our target speed.
		flNewSpeed = m_flTargetSpeed;
		bSpinUpDone = !m_bStopAtStartPos;
	}
	else if ( m_flTargetSpeed < 0 )
	{
		// Spinning up in reverse - negate the speed.
		flNewSpeed *= -1;
	}

	//
	// Apply the new speed, adjust sound pitch and volume.
	//
	UpdateSpeed( flNewSpeed );

	//
	// If we've met or exceeded target speed, stop spinning up.
	//
	if ( bSpinUpDone )
	{
		SetMoveDone( &CFuncRotating::RotateMove );
		RotateMove();
	} 

	SetMoveDoneTime( GetNextMoveInterval() );
}

//-----------------------------------------------------------------------------
// Purpose: Decelerates the rotator from a higher speed to a lower one.
// Input  : flTargetSpeed - Speed to spin down to.
// Output : Returns true if we reached the target speed, false otherwise.
//-----------------------------------------------------------------------------
bool CFuncRotating::SpinDown( float flTargetSpeed )
{
	//
	// Bleed off a little speed due to friction.
	//
	bool bSpinDownDone = false;
	float flNewSpeed = fabs( m_flSpeed ) - 0.1 * m_flMaxSpeed * m_flFanFriction;
	if ( flNewSpeed < 0 )
	{
		flNewSpeed = 0;
	}

	if ( fabs( flNewSpeed ) <= fabs( flTargetSpeed ) )
	{
		// Reached our target speed.
		flNewSpeed = flTargetSpeed;
		bSpinDownDone = !m_bStopAtStartPos;
	}
	else if ( m_flSpeed < 0 )
	{
		// Spinning down in reverse - negate the speed.
		flNewSpeed *= -1;
	}

	//
	// Apply the new speed, adjust sound pitch and volume.
	//
	UpdateSpeed( flNewSpeed );

	//
	// If we've met or exceeded target speed, stop spinning down.
	//
	return bSpinDownDone;
}

//-----------------------------------------------------------------------------
// Purpose: Think function. Decelerates a func_rotating to a lower angular velocity.
//-----------------------------------------------------------------------------
void CFuncRotating::SpinDownMove( void )
{
	//
	// If we've met or exceeded target speed, stop spinning down.
	//
	if ( SpinDown( m_flTargetSpeed ) )
	{
		SetMoveDone( &CFuncRotating::RotateMove );
		RotateMove();
	}
	else
	{
		SetMoveDoneTime( GetNextMoveInterval() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function for reversing directions. Spins down to zero, then
//			starts spinning up to the target speed.
//-----------------------------------------------------------------------------
void CFuncRotating::ReverseMove( void )
{
	if ( SpinDown( 0 ) )
	{
		// We've reached zero - spin back up to the target speed.
		SetTargetSpeed( m_flTargetSpeed );
	}
	else
	{
		SetMoveDoneTime( GetNextMoveInterval() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function. Called while rotating at a constant angular velocity.
//-----------------------------------------------------------------------------
void CFuncRotating::RotateMove( void )
{
	SetMoveDoneTime( 10 );

	if ( m_bStopAtStartPos )
	{
		SetMoveDoneTime( GetNextMoveInterval() );
		int checkAxis = 2;

		// See if we got close to the starting orientation
		if ( m_vecMoveAng[0] != 0 )
		{
			checkAxis = 0;
		}
		else if ( m_vecMoveAng[1] != 0 )
		{
			checkAxis = 1;
		}

		float angDelta = anglemod( GetLocalAngles()[ checkAxis ] - m_angStart[ checkAxis ] );
		if ( angDelta > 180.0f )
			angDelta -= 360.0f;

		QAngle avel = GetLocalAngularVelocity();
		// Delta per tick
		QAngle avelpertick = avel * TICK_INTERVAL;

		if ( fabs( angDelta ) < fabs( avelpertick[ checkAxis ] ) )
		{
			SetTargetSpeed( 0 );
			SetLocalAngles( m_angStart );
			m_bStopAtStartPos = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used for debug output. Returns the given speed considering our current
//			direction of rotation, so that positive values are forward and negative
//			values are backward.
// Input  : flSpeed - Angular speed in degrees per second.
//-----------------------------------------------------------------------------
float CFuncRotating::GetMoveSpeed( float flSpeed )
{
	if ( m_vecMoveAng[0] != 0 )
	{
		return flSpeed * m_vecMoveAng[0];
	}

	if ( m_vecMoveAng[1] != 0 )
	{
		return flSpeed * m_vecMoveAng[1];
	}

	return flSpeed * m_vecMoveAng[2];
}

//-----------------------------------------------------------------------------
// Purpose: Sets a new angular velocity to achieve.
// Input  : flSpeed - Target angular velocity in degrees per second.
//-----------------------------------------------------------------------------
void CFuncRotating::SetTargetSpeed( float flSpeed )
{
	//
	// Make sure the sign is correct - positive for forward rotation,
	// negative for reverse rotation.
	//
	flSpeed = fabs( flSpeed );
	if ( m_bReversed )
	{
		flSpeed *= -1;
	}

	m_flTargetSpeed = flSpeed;

	//
	// If we don't accelerate, change to the new speed instantly.
	//
	if ( !HasSpawnFlags(SF_BRUSH_ACCDCC ) )
	{
		UpdateSpeed( m_flTargetSpeed );
		SetMoveDone( &CFuncRotating::RotateMove );
	}
	//
	// Otherwise deal with acceleration/deceleration:
	//
	else
	{
		//
		// Check for reversing directions.
		//
		if ((( m_flSpeed > 0 ) && ( m_flTargetSpeed < 0 )) || 
			(( m_flSpeed < 0 ) && ( m_flTargetSpeed > 0 )))
		{
			SetMoveDone( &CFuncRotating::ReverseMove );
		}
		//
		// If we are below the new target speed, spin up to the target speed.
		//
		else if ( fabs( m_flSpeed ) < fabs( m_flTargetSpeed ) )
		{
			SetMoveDone( &CFuncRotating::SpinUpMove );
		}
		//
		// If we are above the new target speed, spin down to the target speed.
		//
		else if ( fabs( m_flSpeed ) > fabs( m_flTargetSpeed ) )
		{
			SetMoveDone( &CFuncRotating::SpinDownMove );
		}
		//
		// We are already at the new target speed. Just keep rotating.
		//
		else
		{
			SetMoveDone( &CFuncRotating::RotateMove );
		}
	}

	SetMoveDoneTime( GetNextMoveInterval() );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a rotating brush is used by the player.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CFuncRotating::RotatingUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//
	// If the rotator is spinning, stop it.
	//
	if ( m_flSpeed != 0 )
	{
		SetTargetSpeed( 0 );
	}
	//
	// Rotator is not moving, so start it.
	//
	else
	{
		SetTargetSpeed( m_flMaxSpeed );
	}

	// start renormalization thinker
	SetThink( &CFuncRotating::NormalizeAngleIfNeeded );
	SetNextThink( gpGlobals->curtime + 0.2f );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that reverses the direction of rotation.
//-----------------------------------------------------------------------------
void CFuncRotating::InputReverse( inputdata_t &inputdata )
{
	m_bStopAtStartPos = false;
	m_bReversed = !m_bReversed;
	SetTargetSpeed( m_flSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the speed of the rotator.
// Input  : Float target angular velocity as a ratio of maximum speed [0, 1].
//-----------------------------------------------------------------------------
void CFuncRotating::InputSetSpeed( inputdata_t &inputdata )
{
	m_bStopAtStartPos = false;
	float flSpeed = inputdata.value.Float();
	m_bReversed = flSpeed < 0 ? true : false;
	flSpeed = fabs(flSpeed);
	SetTargetSpeed( clamp( flSpeed, 0, 1 ) * m_flMaxSpeed );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFuncRotating::InputGetSpeed( inputdata_t &inputdata )
{
	float flOutValue = m_flSpeed;
	if ( flOutValue < 0 )
		flOutValue *= -1;

	m_OnGetSpeed.Set( flOutValue, inputdata.pActivator, inputdata.pCaller );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to start the rotator spinning.
//-----------------------------------------------------------------------------
void CFuncRotating::InputStart( inputdata_t &inputdata )
{
	m_bStopAtStartPos = false;
	SetTargetSpeed( m_flMaxSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to start the rotator spinning.
//-----------------------------------------------------------------------------
void CFuncRotating::InputStartForward( inputdata_t &inputdata )
{
	m_bReversed = false;
	SetTargetSpeed( m_flMaxSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to start the rotator spinning.
//-----------------------------------------------------------------------------
void CFuncRotating::InputStartBackward( inputdata_t &inputdata )
{
	m_bStopAtStartPos = false;
	m_bReversed = true;
	SetTargetSpeed( m_flMaxSpeed );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to stop the rotator from spinning.
//-----------------------------------------------------------------------------
void CFuncRotating::InputStop( inputdata_t &inputdata )
{
	m_bStopAtStartPos = false;
	SetTargetSpeed( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncRotating::InputStopAtStartPos( inputdata_t &inputdata )
{
	m_bStopAtStartPos = true;
	SetTargetSpeed( 0 );
	SetMoveDoneTime( GetNextMoveInterval() );
}

//-----------------------------------------------------------------------------
// Purpose: Starts the rotator if it is still, stops it if it is spinning.
//-----------------------------------------------------------------------------
void CFuncRotating::InputToggle( inputdata_t &inputdata )
{	
	if (m_flSpeed > 0)
	{
		SetTargetSpeed( 0 );
	}
	else
	{
		SetTargetSpeed( m_flMaxSpeed );
	}
}

//-----------------------------------------------------------------------------
// Purpose: An entity has blocked the brush.
// Input  : pOther - 
//-----------------------------------------------------------------------------
void CFuncRotating::Blocked_Impl( CBaseEntity *pOther )
{
	pOther->TakeDamage( CTakeDamageInfo( this, this, m_flBlockDamage, DMG_CRUSH ) );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFuncRotating::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		V_snprintf( tempstr, sizeof( tempstr ),"Speed cur (target): %3.2f (%3.2f)", GetMoveSpeed( m_flSpeed ), GetMoveSpeed( m_flTargetSpeed ) );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;

}

class CFuncVPhysicsClip : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncVPhysicsClip, CBaseEntity );

public:
	void Spawn_Impl();
	void Activate_Impl();
	bool CreateVPhysics_Impl( void );

	bool EntityPassesFilter( CBaseEntity *pOther );
	bool ForceVPhysicsCollide_Impl( CBaseEntity *pEntity );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t						m_iFilterName;
	CHandle<CBaseFilter>			m_hFilter;
	bool							m_bDisabled;
};

// Global Savedata for base trigger
IMPLEMENT_REFLECT_DATAMAP( CFuncVPhysicsClip )

LINK_ENTITY_TO_CLASS( func_clip_vphysics, CFuncVPhysicsClip );

void CFuncVPhysicsClip::Spawn_Impl( void )
{
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	CreateVPhysics();
	VPhysicsGetObject()->EnableCollisions( !m_bDisabled );
}

bool CFuncVPhysicsClip::CreateVPhysics_Impl( void )
{
	VPhysicsInitStatic();
	return true;
}

void CFuncVPhysicsClip::Activate_Impl( void ) 
{ 
	// Get a handle to my filter entity if there is one
	if (m_iFilterName != NULL_STRING)
	{
		m_hFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( nullptr, m_iFilterName ));
	}
	BaseClass::Activate_Impl();
}

bool CFuncVPhysicsClip::EntityPassesFilter( CBaseEntity *pOther )
{
	CBaseFilter* pFilter = (CBaseFilter*)(m_hFilter.Get());

	if ( pFilter )
		return pFilter->PassesFilter( this, pOther );

	if ( !pOther->VPhysicsGetObject() )
		return false;

	if ( pOther->GetMoveType() == MOVETYPE_VPHYSICS && pOther->VPhysicsGetObject()->IsMoveable() )
		return true;
	
	return false;
}

bool CFuncVPhysicsClip::ForceVPhysicsCollide_Impl( CBaseEntity *pEntity )
{
	return EntityPassesFilter(pEntity);
}

void CFuncVPhysicsClip::InputEnable( inputdata_t &inputdata )
{
	VPhysicsGetObject()->EnableCollisions(true);
	m_bDisabled = false;
}

void CFuncVPhysicsClip::InputDisable( inputdata_t &inputdata )
{
	VPhysicsGetObject()->EnableCollisions(false);
	m_bDisabled = true;
}

#undef SF_WALL_START_OFF

class CBaseAnimating;
class CBoneMergeCache;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// CBoneMergeCache
//-----------------------------------------------------------------------------

CBoneMergeCache::CBoneMergeCache()
{
	Init( nullptr );
}

void CBoneMergeCache::Init( CBaseAnimating *pOwner )
{
	m_pOwner = pOwner;
	m_pFollow = nullptr;
	m_pFollowHdr = nullptr;
	m_pFollowRenderHdr = nullptr;
	m_pOwnerHdr = nullptr;
	m_pFollowRenderHdr = nullptr;
	m_nFollowBoneSetupMask = 0;
	m_bForceCacheClear = false;
	m_MergedBones.clear(); m_MergedBones.shrink_to_fit();
	V_memset( m_iRawIndexMapping, 0xFF, sizeof( m_iRawIndexMapping ) );	// initialize to -1s
}

void CBoneMergeCache::UpdateCache()
{
	if ( !m_pOwner )
		return;

	CStudioHdr *pOwnerHdr = m_pOwner->GetModelPtr();
	if ( !pOwnerHdr )
		return;
	const studiohdr_t *pOwnerRenderHdr = pOwnerHdr->GetRenderHdr();

	CBaseAnimating *pFollow = m_pOwner->FindFollowedEntity();
	CStudioHdr *pFollowHdr = (pFollow ? pFollow->GetModelPtr() : nullptr);
	const studiohdr_t *pFollowRenderHdr = (pFollowHdr ? pFollowHdr->GetRenderHdr() : nullptr );

	// if the follow parent has changed, or any of the underlying models has changed, reset the MergedBones list
	if ( pFollow != m_pFollow || pFollowRenderHdr != m_pFollowRenderHdr || pOwnerRenderHdr != m_pOwnerRenderHdr || m_bForceCacheClear )
	{
		m_MergedBones.clear(); m_MergedBones.shrink_to_fit();

		m_bForceCacheClear = false;
	
		// Update the cache.
		if ( pFollow && pFollowHdr && pOwnerHdr )
		{
			m_pFollow = pFollow;
			m_pFollowHdr = pFollowHdr;
			m_pFollowRenderHdr = pFollowRenderHdr;
			m_pOwnerHdr = pOwnerHdr;
			m_pOwnerRenderHdr = pOwnerRenderHdr;

			m_BoneMergeBits.assign( pOwnerHdr->numbones(), false );

			const mstudiobone_t *pOwnerBones = m_pOwnerHdr->pBone( 0 );
			
			m_nFollowBoneSetupMask = BONE_USED_BY_BONE_MERGE;
			for ( int i = 0; i < m_pOwnerHdr->numbones(); i++ )
			{
				int parentBoneIndex = Studio_BoneIndexByName( m_pFollowHdr, pOwnerBones[i].pszName() );
				if ( parentBoneIndex < 0 )
					continue;

				m_iRawIndexMapping[i] = parentBoneIndex;

				// Add a merged bone here.
				CMergedBone mergedBone;
				mergedBone.m_iMyBone = i;
				mergedBone.m_iParentBone = parentBoneIndex;
				m_MergedBones.push_back( mergedBone );
				m_BoneMergeBits[i] = true;

				// flag bones used in merge so that they'll always be setup
				if ( ( m_pFollowHdr->boneFlags( parentBoneIndex ) & BONE_USED_BY_BONE_MERGE ) == 0 )
				{
					// go ahead and mark the bone and its parents
					int n = parentBoneIndex;
					while (n != -1)
					{
						m_pFollowHdr->setBoneFlags( n, BONE_USED_BY_BONE_MERGE );
						n = m_pFollowHdr->boneParent( n );
					}
				}

				// FIXME: only do this if it's for a "reverse" merge
				if ( ( m_pOwnerHdr->boneFlags( i ) & BONE_USED_BY_BONE_MERGE ) == 0 )
				{
					// go ahead and mark the bone and its parents
					int n = i;
					while (n != -1)
					{
						m_pOwnerHdr->setBoneFlags( n, BONE_USED_BY_BONE_MERGE );
						n = m_pOwnerHdr->boneParent( n );
					}
				}
			}

			// No merged bones found? Slam the mask to 0
			if ( m_MergedBones.empty() )
			{
				m_nFollowBoneSetupMask = 0;
			}

			// find and record pose params that match by name
			for ( int i = 0; i < MAXSTUDIOPOSEPARAM; i++ )
			{
				// init mapping as invalid
				m_nOwnerToFollowPoseParamMapping[i] = -1;
				
				if ( i < m_pFollowHdr->GetNumPoseParameters() )
				{
					// get the follower's pose param name for this index
					const char * szFollowerPoseParamName = m_pFollowHdr->pPoseParameter( i ).pszName();

					// find it on the owner
					for ( int n = 0; n < MAXSTUDIOPOSEPARAM && n < m_pOwnerHdr->GetNumPoseParameters(); n++ )
					{
						const char * szOwnerPoseParamName = m_pOwnerHdr->pPoseParameter( n ).pszName();
						if ( !V_strcmp( szFollowerPoseParamName, szOwnerPoseParamName ) )
						{
							//match
							m_nOwnerToFollowPoseParamMapping[i] = n;
							break;
						}
					}
				}
			}

		}
		else
		{
			Init( m_pOwner );
		}
	}
}

void CBoneMergeCache::MergeMatchingPoseParams( void )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || std::ssize( m_MergedBones ) == 0 )
		return;

	// set follower pose params using mapped indices from owner
	for ( int i = 0; i < MAXSTUDIOPOSEPARAM; i++ )
	{
		if ( m_nOwnerToFollowPoseParamMapping[i] != -1 )
		{
			m_pOwner->SetPoseParameter( m_nOwnerToFollowPoseParamMapping[i], m_pFollow->GetPoseParameter( i ) );
		}
	}
}

void CBoneMergeCache::BuildMatricesWithBoneMerge( 
	const CStudioHdr *pStudioHdr,
	const QAngle& angles, 
	const Vector& origin, 
	const Vector pos[MAXSTUDIOBONES],
	const Quaternion q[MAXSTUDIOBONES],
	matrix3x4_t bonetoworld[MAXSTUDIOBONES],
	CBaseAnimating *pParent,
	CBoneCache *pParentCache,
	int boneMask
	)
{

	UpdateCache();

	bool bMergedBone[ MAXSTUDIOBONES ];
	std::memset( &bMergedBone, 0, sizeof(bool) * MAXSTUDIOBONES );

	for ( int i=0; i < std::ssize( m_MergedBones ); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;

		if ( iParentBone >= 0 )
		{
			// Only update bones reference by the bone mask.
			if ( !( pStudioHdr->boneFlags( iOwnerBone ) & boneMask ) )
				continue;

			matrix3x4_t *pMat = pParentCache->GetCachedBone( iParentBone );
			if ( pMat )
			{
				MatrixCopy( *pMat, bonetoworld[ iOwnerBone ] );
				bMergedBone[iOwnerBone] = true;
			}
		}

	}

	const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	
	matrix3x4_t rotationmatrix; // model to world transformation
	AngleMatrix( angles, origin, rotationmatrix);

	for ( int i=0; i < pStudioHdr->numbones(); i++ )
	{
		if ( !bMergedBone[i] )
		{
			// If we get down here, then the bone wasn't merged.
			matrix3x4_t bonematrix;
			QuaternionMatrix( q[i], pos[i], bonematrix );
	
			if (pbones[i].parent == -1) 
			{
				ConcatTransforms (rotationmatrix, bonematrix, bonetoworld[i]);
			} 
			else 
			{
				ConcatTransforms (bonetoworld[pbones[i].parent], bonematrix, bonetoworld[i]);
			}
		}
	}

	//CStudioHdr *fhdr = pParent->GetModelPtr();
	//const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	//
	//matrix3x4_t rotationmatrix; // model to world transformation
	//AngleMatrix( angles, origin, rotationmatrix);
	//
	//for ( int i=0; i < pStudioHdr->numbones(); i++ )
	//{
	//	// Now find the bone in the parent entity.
	//	bool merged = false;
	//	int parentBoneIndex = Studio_BoneIndexByName( fhdr, pbones[i].pszName() );
	//	if ( parentBoneIndex >= 0 )
	//	{
	//		matrix3x4_t *pMat = pParentCache->GetCachedBone( parentBoneIndex );
	//		if ( pMat )
	//		{
	//			MatrixCopy( *pMat, bonetoworld[ i ] );
	//			merged = true;
	//		}
	//	}
	//
	//	if ( !merged )
	//	{
	//		// If we get down here, then the bone wasn't merged.
	//		matrix3x4_t bonematrix;
	//		QuaternionMatrix( q[i], pos[i], bonematrix );
	//
	//		if (pbones[i].parent == -1) 
	//		{
	//			ConcatTransforms (rotationmatrix, bonematrix, bonetoworld[i]);
	//		} 
	//		else 
	//		{
	//			ConcatTransforms (bonetoworld[pbones[i].parent], bonematrix, bonetoworld[i]);
	//		}
	//	}
	//}

}

void CBoneMergeCache::CopyFromFollow( const BoneVector followPos[], const BoneQuaternion followQ[], int boneMask, BoneVector myPos[], BoneQuaternion myQ[] )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || std::ssize( m_MergedBones ) == 0 )
		return;

	// Now copy the bone matrices.
	for ( int i=0; i < std::ssize( m_MergedBones ); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		myPos[ iOwnerBone ] = followPos[ iParentBone ];
		myQ[ iOwnerBone ] = followQ[ iParentBone ];
	}
}

void CBoneMergeCache::CopyToFollow( const BoneVector myPos[], const BoneQuaternion myQ[], int boneMask, BoneVector followPos[], BoneQuaternion followQ[] )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || std::ssize( m_MergedBones ) == 0 )
		return;

	// Now copy the bone matrices.
	for ( int i=0; i < std::ssize( m_MergedBones ); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		followPos[ iParentBone ] = myPos[ iOwnerBone ];
		followQ[ iParentBone ] = myQ[ iOwnerBone ];
	}
	m_nCopiedFramecount = gpGlobals->framecount;
}

bool CBoneMergeCache::IsCopied( )
{
	return (m_nCopiedFramecount == gpGlobals->framecount);
}

bool CBoneMergeCache::GetAimEntOrigin( Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || std::ssize( m_MergedBones ) == 0 )
		return false;

	// We want the abs origin such that if we put the entity there, the first merged bone
	// will be aligned. This way the entity will be culled in the correct position.
	//
	// ie: mEntity * mBoneLocal = mFollowBone
	// so: mEntity = mFollowBone * Inverse( mBoneLocal )
	//
	// Note: the code below doesn't take animation into account. If the attached entity animates
	// all over the place, then this won't get the right results.
	
	// FIXME: we're merged onto a dead player that's likely ragdolled all over the place.
	// The parent's cached position and angles are dirty, but the absorigin and absangles are good enough.
	// Returning false here means the uncached parent origin/angles will be used.
	if ( m_pFollow->IsPlayer() && !m_pFollow->IsAlive() )
		return false;

	// Get mFollowBone.
	m_pFollow->SetupBones( nullptr, m_nFollowBoneSetupMask );

	matrix3x4_t mFollowBone;
	m_pFollow->GetBoneTransform( m_MergedBones[0].m_iParentBone, mFollowBone );

	// Get Inverse( mBoneLocal )
	matrix3x4_t mBoneLocal, mBoneLocalInv;
	SetupSingleBoneMatrix( m_pOwnerHdr, m_pOwner->GetSequence(), 0, m_MergedBones[0].m_iMyBone, mBoneLocal );
	MatrixInvert( mBoneLocal, mBoneLocalInv );

	// Now calculate mEntity = mFollowBone * Inverse( mBoneLocal )
	matrix3x4_t mEntity;
	ConcatTransforms( mFollowBone, mBoneLocalInv, mEntity );
	MatrixAngles( mEntity, *pAbsAngles, *pAbsOrigin );

	return true;
}

bool CBoneMergeCache::GetRootBone( matrix3x4_t &rootBone )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || std::ssize( m_MergedBones ) == 0 )
		return false;

	// Get mFollowBone.
	m_pFollow->SetupBones( nullptr, m_nFollowBoneSetupMask );
	m_pFollow->GetBoneTransform( m_MergedBones[0].m_iParentBone, rootBone );

	return true;
}

int CBoneMergeCache::IsBoneMerged( int iBone ) const
{
	if ( m_pOwnerHdr )
		return m_BoneMergeBits[iBone];
	else
		return 0;
}
