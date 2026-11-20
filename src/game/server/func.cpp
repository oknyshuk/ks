// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2010 Valve Corporation

module;
#include <stdlib.h>
#include <math.h>
#include "declare_class.h"
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "server_class.h"
#include "reflect_sendtable.h"
#include "igameevents.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "shattersurfacetypes.h"
#include "irecipientfilter.h"
#include "worldsize.h"
#include "predictable_entity.h"
#include "shareddefs.h"
#include "reflect_annotations.h"
#include "string_t.h"
module ks.server;

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.math.vplane;
import ks.tier1.vector_ops;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.threadtools;
import ks.math.mathlib;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.core.numeric;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.core.platform;
import ks.core.wchartypes;
import ks.game.func_dust_shared;
import ks.core.vprof;

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: area portal entity: toggles visibility areas on/off
// NOTE: These are not really brush entities.  They are brush entities from a
// designer/worldcraft perspective, but by the time they reach the game, the
// brush model is gone and this is, in effect, a point entity.

enum areaportal_state
{
	AREAPORTAL_CLOSED = 0,
	AREAPORTAL_OPEN = 1,
};

class CAreaPortal : public CFuncAreaPortalBase
{
	[[= ks::reflect::KeyFrom<"m_portalNumber", ks::reflect::Key{ .name = "portalnumber" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CAreaPortal, CFuncAreaPortalBase );

					CAreaPortal();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );
	int		UpdateTransmitState_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] [[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputOpen( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] [[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputClose( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	virtual bool	UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient );

	DECLARE_DATADESC();

private:
	bool	UpdateState( void );

	int		m_state;
};

LINK_ENTITY_TO_CLASS( func_areaportal, CAreaPortal );

IMPLEMENT_REFLECT_DATAMAP( CAreaPortal )

CAreaPortal::CAreaPortal()
{
	m_state = AREAPORTAL_OPEN;
}

void CAreaPortal::Spawn_Impl( void )
{
	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW );
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: notify the engine of the state at startup/restore
//-----------------------------------------------------------------------------
void CAreaPortal::Precache_Impl( void )
{
	UpdateState();
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CAreaPortal::InputClose( inputdata_t &inputdata )
{
	m_state = AREAPORTAL_CLOSED;
	UpdateState();
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CAreaPortal::InputOpen( inputdata_t &inputdata )
{
	m_state = AREAPORTAL_OPEN;
	UpdateState();
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CAreaPortal::InputToggle( inputdata_t &inputdata )
{
	m_state = ((m_state == AREAPORTAL_OPEN) ? AREAPORTAL_CLOSED : AREAPORTAL_OPEN);
	UpdateState();
}

bool CAreaPortal::UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient )
{
	if ( m_state )
	{
		// We're not closed, so give the base class a chance to close it.
		return BaseClass::UpdateVisibility( vecOrigins, fovDistanceAdjustFactor, bIsOpenOnClient );
	}
	else
	{
		bIsOpenOnClient = false;
		return false;
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CAreaPortal::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_ON )
	{
		m_state = AREAPORTAL_OPEN;
	}
	else if ( useType == USE_OFF )
	{
		m_state = AREAPORTAL_CLOSED;
	}
	else
	{
		return;
	}

	UpdateState();
}

bool CAreaPortal::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if( FStrEq( szKeyName, "StartOpen" ) )
	{
		m_state = (std::atoi(szValue) != 0) ? AREAPORTAL_OPEN : AREAPORTAL_CLOSED;

		return true;
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}
}

bool CAreaPortal::UpdateState()
{
	engine->SetAreaPortalState( m_portalNumber, m_state );
	return !!m_state;
}

int CAreaPortal::UpdateTransmitState_Impl()
{
	// Our brushes are kept around so don't transmit anything since we don't want to draw them.
	return SetTransmitState( FL_EDICT_DONTSEND );
}

using namespace ::ks::reflect;

// A sphere around the player used for backface culling of areaportals.
static constexpr auto VIEWER_PADDING = 80;

std::vector<CFuncAreaPortalBase*> g_AreaPortals;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncAreaPortalBase )

CFuncAreaPortalBase::CFuncAreaPortalBase()
{
	m_portalNumber = -1;
	g_AreaPortals.push_back( this );
	m_iPortalVersion = 0;
}

CFuncAreaPortalBase::~CFuncAreaPortalBase()
{
	ks::erase_first( g_AreaPortals, this );
}

bool CFuncAreaPortalBase::UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient )
{
	// NOTE: We leave bIsOpenOnClient alone on purpose here. See the header for a description of why.
	
	if( m_portalNumber == -1 )
		return false;

	// See if the viewer is on the backside.
	for ( int i = 0; i < std::ssize( vecOrigins ); ++i )
	{
		const Vector &vOrigin = vecOrigins[ i ];
		VPlane plane;
		if( !engine->GetAreaPortalPlane( vOrigin, m_portalNumber, &plane ) )
			return true; // leave it open if there's an error here for some reason

		// If either players' origin is on the front, the areaportal is considered opened...
		if( plane.DistTo( vOrigin ) + VIEWER_PADDING > 0 )
			return true;
	}

	return false;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// The server will still send entities through a window even after it opaque 
// to allow for net lag.
static constexpr auto FADE_DIST_BUFFER = 10;

LINK_ENTITY_TO_CLASS( func_areaportalwindow, CFuncAreaPortalWindow );

IMPLEMENT_REFLECT_SERVERCLASS( CFuncAreaPortalWindow, DT_FuncAreaPortalWindow )

IMPLEMENT_REFLECT_DATAMAP( CFuncAreaPortalWindow )

CFuncAreaPortalWindow::CFuncAreaPortalWindow()
{
	m_iBackgroundModelIndex = -1;
}

CFuncAreaPortalWindow::~CFuncAreaPortalWindow()
{
}

void CFuncAreaPortalWindow::Spawn_Impl()
{
	Precache();

	engine->SetAreaPortalState( m_portalNumber, 1 );
}

void CFuncAreaPortalWindow::Activate_Impl()
{
	BaseClass::Activate_Impl();
	
	// Find our background model.
	CBaseEntity *pBackground = gEntList.FindEntityByName( nullptr, m_iBackgroundBModelName );
	if( pBackground )
	{
		m_iBackgroundModelIndex  = modelinfo->GetModelIndex( STRING( pBackground->GetModelName() ) );
		pBackground->AddEffects( EF_NODRAW ); // we will draw for it.
	}

	// Find our target and steal its bmodel.
	CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, m_target );
	if( pTarget )
	{
		SetModel( STRING(pTarget->GetModelName()) );
		SetAbsOrigin( pTarget->GetAbsOrigin() );
		pTarget->AddEffects( EF_NODRAW ); // we will draw for it.
	}
}

bool CFuncAreaPortalWindow::IsWindowOpen( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor )
{
	float flClosestDistance = std::numeric_limits<float>::max();
	for ( int i = 0; i < std::ssize( vecOrigins ); ++i )
	{
		float flDist = CollisionProp()->CalcDistanceFromPoint( vecOrigins[ i ] );
		if ( flDist < flClosestDistance )
		{
			flClosestDistance = flDist;
		}
	}

	if ( flClosestDistance == std::numeric_limits<float>::max() )
		return false;

	flClosestDistance *= fovDistanceAdjustFactor;
	return ( flClosestDistance <= (m_flFadeDist + FADE_DIST_BUFFER) );
}

bool CFuncAreaPortalWindow::UpdateVisibility( const std::vector< Vector > &vecOrigins, float fovDistanceAdjustFactor, bool &bIsOpenOnClient )
{
	if ( IsWindowOpen( vecOrigins, fovDistanceAdjustFactor ) )
	{
		return BaseClass::UpdateVisibility( vecOrigins, fovDistanceAdjustFactor, bIsOpenOnClient );
	}
	else
	{
		bIsOpenOnClient = false;
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Changes the fade start distance 
// Input: float distance in inches
//-----------------------------------------------------------------------------
void CFuncAreaPortalWindow::InputSetFadeStartDistance( inputdata_t &inputdata )
{
	m_flFadeStartDist = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Changes the fade end distance
// Input: float distance in inches
//-----------------------------------------------------------------------------
void CFuncAreaPortalWindow::InputSetFadeEndDistance( inputdata_t &inputdata )
{
	m_flFadeDist = inputdata.value.Float();
}

class CBaseEntity;
class CBasePlayer;
class CBreakable;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements breakables and pushables. func_breakable is a bmodel
//			that breaks into pieces after taking damage.

ConVar func_break_max_pieces( side_registry(), "func_break_max_pieces", "15", FCVAR_ARCHIVE | FCVAR_REPLICATED );
ConVar func_break_reduction_factor( side_registry(), "func_break_reduction_factor", ".5" );
extern ConVar breakable_disable_gib_limit;

extern Vector		g_vecAttackDir;

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname in the FGD so we can control which entities can
// be spawned, and still remain fairly flexible

const char *CBreakable::pSpawnObjects[] =
{
	nullptr,						// 0
	"item_battery",				// 1
	"item_healthkit",			// 2
	"item_ammo_pistol",			// 3
	"item_ammo_pistol_large",	// 4
	"item_ammo_smg1",			// 5
	"item_ammo_smg1_large",		// 6
	"item_ammo_ar2",			// 7
	"item_ammo_ar2_large",		// 8
	"item_box_buckshot",		// 9
	"item_flare_round",			// 10
	"item_box_flare_rounds",	// 11
	"item_rpg_round",			// 12
	"unused (item_smg1_grenade) 13",// 13
	"item_box_sniper_rounds",	// 14
	"unused (???"") 15",		// 15 - split into two strings to avoid trigraph warning 
	"weapon_stunstick",			// 16
	"unused (weapon_ar1) 17",	// 17
	"weapon_ar2",				// 18
	"unused (???"") 19",		// 19 - split into two strings to avoid trigraph warning 
	"weapon_rpg",				// 20
	"weapon_smg1",				// 21
	"unused (weapon_smg2) 22",	// 22
	"unused (weapon_slam) 23",	// 23
	"weapon_shotgun",			// 24
	"unused (weapon_molotov) 25",// 25
	"item_dynamic_resupply",	// 26
};

const char *pFGDPropData[] =
{
	nullptr,
	"Wooden.Tiny",
	"Wooden.Small",
	"Wooden.Medium",
	"Wooden.Large",
	"Wooden.Huge",
	"Metal.Small",
	"Metal.Medium",
	"Metal.Large",
	"Cardboard.Small",
	"Cardboard.Medium",
	"Cardboard.Large",
	"Stone.Small",
	"Stone.Medium",
	"Stone.Large",
	"Stone.Huge",
	"Glass.Small",
	"Plastic.Small",
	"Plastic.Medium",
	"Plastic.Large",
	"Pottery.Small",
	"Pottery.Medium",
	"Pottery.Large",
	"Pottery.Huge",
	"Glass.Window",
};

LINK_ENTITY_TO_CLASS( func_breakable, CBreakable );
IMPLEMENT_REFLECT_DATAMAP( CBreakable )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBreakable::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(szKeyName, "material"))
	{
		int i = std::atoi( szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood

		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;
	}
	else if (FStrEq(szKeyName, "deadmodel"))
	{
	}
	else if (FStrEq(szKeyName, "shards"))
	{
//			m_iShards = atof(szValue);
	}
	else if (FStrEq(szKeyName, "gibmodel") )
	{
		m_iszGibModel = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "spawnobject") )
	{
		int object = std::atoi( szValue );
		if ( object > 0 && object < std::size( pSpawnObjects ) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
	}
	else if (FStrEq(szKeyName, "propdata") )
	{
		int pdata = std::atoi( szValue );
		if ( pdata > 0 && pdata < std::size( pFGDPropData ) )
		{
			m_iszPropData = MAKE_STRING( pFGDPropData[pdata] );
		}
		else if ( pdata )
		{
			// If you've hit this warning, it's probably because someone's added a new
			// propdata field to func_breakables in the .fgd, and not added it to the
			// pFGDPropData list.
			Warning("func_breakable with invalid propdata %d.\n", pdata );
		}
	}
	else if (FStrEq(szKeyName, "lip") )
	{
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBreakable::Spawn_Impl( void )
{
	// Initialize damage modifiers. Must be done before baseclass spawn.
	m_flDmgModBullet = func_breakdmg_bullet.GetFloat();
	m_flDmgModClub = func_breakdmg_club.GetFloat();
	m_flDmgModExplosive = func_breakdmg_explosive.GetFloat();
	m_flDmgModFire = 1.0f;

	ParsePropData();

    Precache( );    

	if ( !m_iHealth || FBitSet( m_spawnflags, SF_BREAK_TRIGGER_ONLY ) )
	{
		// This allows people to shoot at the glass (since it's penetrable)
		if ( m_Material == matGlass )
		{
			m_iHealth = 1;
		}

		m_takedamage = DAMAGE_NO;
	}
	else
	{
		m_takedamage = DAMAGE_YES;
	}

	m_iMaxHealth = ( m_iHealth > 0 ) ? m_iHealth : 1;
  
	SetSolid( SOLID_BSP );
    SetMoveType( MOVETYPE_PUSH );
	
	SetModel( STRING( GetModelName() ) );//set size and link into world.

	SetTouch( &CBreakable::BreakTouch );
	if ( FBitSet( m_spawnflags, SF_BREAK_TRIGGER_ONLY ) )		// Only break on trigger
	{
		SetTouch( nullptr );
	}

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if ( !IsBreakable() && m_nRenderMode != kRenderNormal )
		AddFlag( FL_WORLDBRUSH );

	if ( m_impactEnergyScale == 0 )
	{
		m_impactEnergyScale = 1.0;
	}

	CreateVPhysics();
}

//-----------------------------------------------------------------------------
// Purpose: Parse this prop's data, if it has a keyvalues section.
//			Returns true only if this prop is using a model that has a prop_data section that's invalid.
//-----------------------------------------------------------------------------
void CBreakable::ParsePropData( void )
{
	if ( m_iszPropData == NULL_STRING )
		return;

	if ( StringHasPrefixCaseSensitive( STRING(m_iszPropData), "None" ) )
		return;

	g_PropDataSystem.ParsePropFromBase( this, this, STRING(m_iszPropData) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBreakable::CreateVPhysics_Impl( void )
{
	VPhysicsInitStatic();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CBreakable::MaterialSound( Materials precacheMaterial )
{
    switch ( precacheMaterial ) 
	{
	case matWood:
		return "Breakable.MatWood";
	case matFlesh:
		return "Breakable.MatFlesh";
	case matComputer:
		return "Breakable.Computer";
	case matUnbreakableGlass:
	case matGlass:
		return "Breakable.MatGlass";
	case matMetalPanel:
	case matMetal:
		return "Breakable.MatMetal";
	case matCinderBlock:
	case matRocks:
		return "Breakable.MatConcrete";
	case matCeilingTile:
	case matNone:
	default:
		break;
	}

	return nullptr;
}

void CBreakable::MaterialSoundRandom( int entindex, Materials soundMaterial, float volume )
{
	const char	*soundname;
	soundname = MaterialSound( soundMaterial );
	if ( !soundname )
		return;

	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, nullptr ) )
		return;

	CPASAttenuationFilter filter( CBaseEntity::Instance( entindex ), params.soundlevel );

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = volume;
	ep.m_SoundLevel = params.soundlevel;

	EmitSound( filter, entindex, ep );
}

void CBreakable::Precache_Impl( void )
{
	const char *pGibName = "WoodChunks";

    switch (m_Material) 
	{
	case matWood:
		pGibName = "WoodChunks";
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "GlassChunks";
		break;

	case matMetalPanel:
		pGibName = "MetalPanelChunks";
		break;

	case matMetal:
		pGibName = "MetalChunks";
		break;

	case matRocks:
		pGibName = "ConcreteChunks";
		break;

	case matCinderBlock:
		pGibName = "ConcreteChunks";
		break;

	default:
		Warning("%s (%s) at (%.3f %.3f %.3f) using obsolete or unknown material type.\n", GetClassname(), GetDebugName(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		pGibName = "WoodChunks";
		break;
	}

	if ( m_iszGibModel != NULL_STRING )
	{
		pGibName = STRING(m_iszGibModel);
	}

	m_iszModelName = MAKE_STRING( pGibName );

	// Precache the spawn item's data
	if ( !CommandLine()->CheckParm("-makereslists"))
	{
		if ( m_iszSpawnObject != NULL_STRING )
		{
			UTIL_PrecacheOther( STRING( m_iszSpawnObject ) );
		}
	}
	else
	{
		// Actually, precache all possible objects...
		for ( int i = 0; i < std::size( pSpawnObjects ) ; ++i )
		{
			if ( !pSpawnObjects[ i ] )
				continue;

			if ( !V_strnicmp( pSpawnObjects[ i ], "unused", V_strlen( "unused" ) ) )
				continue;

			UTIL_PrecacheOther( pSpawnObjects[ i ] );
		}	
	}

	PrecacheScriptSound( "Breakable.MatGlass" );
	PrecacheScriptSound( "Breakable.MatWood" );
	PrecacheScriptSound( "Breakable.MatMetal" );
	PrecacheScriptSound( "Breakable.MatFlesh" );
	PrecacheScriptSound( "Breakable.MatConcrete" );
	PrecacheScriptSound( "Breakable.Computer" );
	PrecacheScriptSound( "Breakable.Crate" );
	PrecacheScriptSound( "Breakable.Glass" );
	PrecacheScriptSound( "Breakable.Metal" );
	PrecacheScriptSound( "Breakable.Flesh" );
	PrecacheScriptSound( "Breakable.Concrete" );
	PrecacheScriptSound( "Breakable.Ceiling" );
}

// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.

void CBreakable::DamageSound( void )
{
	int pitch;
	float fvol;
	const char *soundname = nullptr;
	int material = m_Material;

	if (random_valve->RandomInt(0,2))
	{
		pitch = PITCH_NORM;
	}
	else
	{
		pitch = 95 + random_valve->RandomInt(0,34);
	}

	fvol = random_valve->RandomFloat(0.75, 1.0);

	if (material == matComputer && random_valve->RandomInt(0,1))
	{
		material = matMetal;
	}

	switch (material)
	{
	case matGlass:
	case matUnbreakableGlass:
		soundname = "Breakable.MatGlass";
		break;

	case matWood:
		soundname = "Breakable.MatWood";
		break;

	case matMetalPanel:
	case matMetal:
		soundname = "Breakable.MatMetal";
		break;

	case matRocks:
	case matCinderBlock:
		soundname = "Breakable.MatConcrete";
		break;

	case matComputer:
		soundname = "Breakable.Computer";
		break;

	default:
		break;
	}

	if ( soundname )
	{
		CSoundParameters params;
		if ( GetParametersForSound( soundname, params, nullptr ) )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = params.channel;
			ep.m_pSoundName = params.soundname;
			ep.m_flVolume = fvol;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = pitch;

			EmitSound( filter, entindex(), ep );
		}
	}
}

void CBreakable::BreakTouch( CBaseEntity *pOther )
{
	float flDamage;
	
	// only players can break these right now
	if ( !pOther->IsPlayer() || !IsBreakable() )
	{
        return;
	}

	// can I be broken when run into?
	if ( HasSpawnFlags( SF_BREAK_TOUCH ) )
	{
		flDamage = pOther->GetSmoothedVelocity().Length() * 0.01;

		if (flDamage >= m_iHealth)
		{
			m_takedamage = DAMAGE_YES;

			SetTouch( nullptr );
			OnTakeDamage( CTakeDamageInfo( pOther, pOther, flDamage, DMG_CRUSH ) );

			// do a little damage to player if we broke glass or computer
			CTakeDamageInfo info( pOther, pOther, flDamage/4, DMG_SLASH );
			CalculateMeleeDamageForce( &info, (pOther->GetAbsOrigin() - GetAbsOrigin()), GetAbsOrigin() );
			pOther->TakeDamage( info );
		}
	}

	// can I be broken when stood upon?
	if ( HasSpawnFlags( SF_BREAK_PRESSURE ) && pOther->GetGroundEntity() == this )
	{
		// play creaking sound here.
		DamageSound();

		m_hBreaker = pOther;

		SetThink ( &CBreakable::Die );
		SetTouch( nullptr );
		
		// Add optional delay 
		SetNextThink( gpGlobals->curtime + m_flPressureDelay );

	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for adding to the breakable's health.
// Input  : Integer health points to add.
//-----------------------------------------------------------------------------
void CBreakable::InputAddHealth( inputdata_t &inputdata )
{
	UpdateHealth( m_iHealth + inputdata.value.Int(), inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for breaking the breakable immediately.
//-----------------------------------------------------------------------------
void CBreakable::InputBreak( inputdata_t &inputdata )
{
	Break( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for removing health from the breakable.
// Input  : Integer health points to remove.
//-----------------------------------------------------------------------------
void CBreakable::InputRemoveHealth( inputdata_t &inputdata )
{
	UpdateHealth( m_iHealth - inputdata.value.Int(), inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the breakable's health.
//-----------------------------------------------------------------------------
void CBreakable::InputSetHealth( inputdata_t &inputdata )
{
	UpdateHealth( inputdata.value.Int(), inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the breakable's mass.
//-----------------------------------------------------------------------------
void CBreakable::InputSetMass( inputdata_t &inputdata )
{
	IPhysicsObject * vPhys = VPhysicsGetObject();
	if ( vPhys )
	{
		float toMass = inputdata.value.Float();
		contract_assert( toMass > 0 );
		vPhys->SetMass( toMass );
	}
	else
	{
		Warning( "Tried to call SetMass() on %s but it has no physics.\n", GetEntityName().ToCStr() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Choke point for changes to breakable health. Ensures outputs are fired.
// Input  : iNewHealth - 
//			pActivator - 
// Output : Returns true if the breakable survived, false if it died (broke).
//-----------------------------------------------------------------------------
bool CBreakable::UpdateHealth( int iNewHealth, CBaseEntity *pActivator )
{
	if ( iNewHealth != m_iHealth )
	{
		m_iHealth = iNewHealth;

		if ( m_iMaxHealth == 0 )
		{
			contract_assert( false );
			m_iMaxHealth = 1;
		}

		// Output the new health as a percentage of max health [0..1]
		float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0, 1 );
		m_OnHealthChanged.Set( flRatio, pActivator, this );

		if ( m_iHealth <= 0 )
		{
			Break( pActivator );
			return false;
		}
		else
		{
			if ( FBitSet( m_spawnflags, SF_BREAK_TRIGGER_ONLY ) )
			{
				m_takedamage = DAMAGE_NO;
			}
			else
			{
				m_takedamage = DAMAGE_YES;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Breaks the breakable if it can be broken.
// Input  : pBreaker - The entity that caused us to break, either via an input,
//				by shooting us, or by touching us.
//-----------------------------------------------------------------------------
void CBreakable::Break( CBaseEntity *pBreaker )
{
	if ( IsBreakable() )
	{
		m_hBreaker = pBreaker;
		Die();
	}
}

void CBreakable::TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	// random spark if this is a 'computer' object
	if (random_valve->RandomInt(0,1) )
	{
		switch( m_Material )
		{
			case matComputer:
			{
				g_pEffects->Sparks( ptr->endpos );

				EmitSound( "Breakable.Computer" );
			}
			break;
			
			case matUnbreakableGlass:
				g_pEffects->Ricochet( ptr->endpos, (vecDir*-1.0f) );
			break;
		}
	}

	BaseClass::TraceAttack_Impl( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: Allows us to take damage from physics objects
//-----------------------------------------------------------------------------
void CBreakable::VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision_Impl( index, pEvent );

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	if ( damageForce == vec3_origin )
	{
		// This can happen if this entity is a func_breakable, and can't move.
		// Use the velocity of the entity that hit us instead.
		damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
	}

	// If we're supposed to explode on collision, do so
	if ( HasSpawnFlags( SF_BREAK_PHYSICS_BREAK_IMMEDIATELY ) )
	{
		// We're toast
		m_bTookPhysicsDamage = true;
		CBaseEntity *pHitEntity = pEvent->pEntities[!index];

		// HACKHACK: Reset mass to get correct collision response for the object breaking this glass
		if ( m_Material == matGlass )
		{
			pEvent->pObjects[index]->SetMass( 2.0f );
		}
		CTakeDamageInfo dmgInfo( pHitEntity, pHitEntity, damageForce, damagePos, (m_iHealth + 1), DMG_CRUSH );
		PhysCallbackDamage( this, dmgInfo, *pEvent, index );
	}
	else if ( !HasSpawnFlags( SF_BREAK_DONT_TAKE_PHYSICS_DAMAGE ) )
	{
		int otherIndex = !index;
		CBaseEntity *pOther = pEvent->pEntities[otherIndex];

		// We're to take normal damage from this
		int damageType;
		IBreakableWithPropData *pBreakableInterface = static_cast<IBreakableWithPropData*>(this);
		float damage = CalculateDefaultPhysicsDamage( index, pEvent, m_impactEnergyScale, true, damageType, pBreakableInterface->GetPhysicsDamageTable() );
		if ( damage > 0 )
		{
			// HACKHACK: Reset mass to get correct collision response for the object breaking this glass
			if ( m_Material == matGlass )
			{
				pEvent->pObjects[index]->SetMass( 2.0f );
			}
			CTakeDamageInfo dmgInfo( pOther, pOther, damageForce, damagePos, damage, damageType );
			PhysCallbackDamage( this, dmgInfo, *pEvent, index );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allows us to make damage exceptions that are breakable-specific.
//-----------------------------------------------------------------------------
int CBreakable::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	Vector	vecTemp;

	CTakeDamageInfo subInfo = info;

	// If attacker can't do at least the min required damage to us, don't take any damage from them
	if ( m_takedamage == DAMAGE_NO || info.GetDamage() < m_iMinHealthDmg )
		return 0;

	// Check our damage filter
	if ( !PassesDamageFilter(subInfo) )
	{
		m_bTookPhysicsDamage = false;
		return 1;
	}

	vecTemp = subInfo.GetInflictor()->GetAbsOrigin() - WorldSpaceCenter();

	if (!IsBreakable())
		return 0;

	float flPropDamage = GetBreakableDamage( subInfo, this );
	subInfo.SetDamage( flPropDamage );
	
	int iPrevHealth = m_iHealth;
	BaseClass::OnTakeDamage_Impl( subInfo );

	// HACK: slam health back to what it was so UpdateHealth can do its thing
	int iNewHealth = m_iHealth;
	m_iHealth = iPrevHealth;
	if ( !UpdateHealth( iNewHealth, info.GetAttacker() ) )
		return 1;

	// Make a shard noise each time func breakable is hit, if it's capable of taking damage and it took damage
	if ( m_takedamage == DAMAGE_YES && m_iHealth < iPrevHealth )
	{
		// Don't play shard noise if being burned.
		// Don't play shard noise if cbreakable actually died.
		if ( ( subInfo.GetDamageType() & DMG_BURN ) == false )
		{
			DamageSound();
		}
	}

	return 1;
}

//------------------------------------------------------------------------------
// Purpose : Reset the OnGround flags for any entities that may have been
//			 resting on me
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakable::ResetOnGroundFlags(void)
{
	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 9 inch high sheet
	Vector mins, maxs;
	CollisionProp()->WorldSpaceAABB( &mins, &maxs );
	mins.z -= 1;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, 256, mins, maxs, FL_ONGROUND );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			pList[i]->SetGroundEntity( (CBaseEntity *)nullptr );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: Breaks the breakable. m_hBreaker is the entity that caused us to break.
//-----------------------------------------------------------------------------
void CBreakable::Die( void )
{
	Vector vecVelocity;// shard velocity
	char cFlag = 0;
	int pitch;
	float fvol;
	
	pitch = 95 + random_valve->RandomInt(0,29);

	if (pitch > 97 && pitch < 103)
	{
		pitch = 100;
	}

	// The more negative m_iHealth, the louder
	// the sound should be.

	fvol = random_valve->RandomFloat(0.85, 1.0) + (abs(m_iHealth.Get()) / 100.0);
	if (fvol > 1.0)
	{
		fvol = 1.0;
	}

	const char *soundname = nullptr;

	switch (m_Material)
	{
	default:
		break;

	case matGlass:
		soundname = "Breakable.Glass";
		cFlag = BREAK_GLASS;
		break;

	case matWood:
		soundname = "Breakable.Crate";
		cFlag = BREAK_WOOD;
		break;

	case matComputer:
		soundname = "Breakable.Computer";
		cFlag = BREAK_METAL;
		break;

	case matMetal:
	case matMetalPanel:
		soundname = "Breakable.Metal";
		cFlag = BREAK_METAL;
		break;

	case matFlesh:
		soundname = "Breakable.Flesh";
		cFlag = BREAK_FLESH;
		break;

	case matRocks:
	case matCinderBlock:
		soundname = "Breakable.Concrete";
		cFlag = BREAK_CONCRETE;
		break;

	case matCeilingTile:
		soundname = "Breakable.Ceiling";
		break;
	}
    
	if ( soundname )
	{
		if ( m_hBreaker && m_hBreaker->IsPlayer() )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "break_breakable" );
			if ( event )
			{
				event->SetInt( "userid", ToBasePlayer( m_hBreaker )->GetUserID() );
				event->SetInt( "entindex", entindex() );
				event->SetInt( "material", cFlag );
				gameeventmanager->FireEvent( event );
			}
		}

		CSoundParameters params;
		if ( GetParametersForSound( soundname, params, nullptr ) )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = params.channel;
			ep.m_pSoundName = params.soundname;
			ep.m_flVolume = fvol;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = pitch;

			EmitSound( filter, entindex(), ep );	
		}
	}
		
	switch( m_Explosion )
	{
	case expDirected:
		vecVelocity = g_vecAttackDir * -200;
		break;

	case expUsePrecise:
		{
			AngleVectors( m_GibDir, &vecVelocity, nullptr, nullptr );
			vecVelocity *= 200;
		}
		break;

	case expRandom:
		vecVelocity.x = 0;
		vecVelocity.y = 0;
		vecVelocity.z = 0;
		break;

	default:
		DevMsg("**ERROR - Unspecified gib dir method in func_breakable!\n");
		break;
	}

	Vector vecSpot = WorldSpaceCenter();
	CPVSFilter filter2( vecSpot );

	int iModelIndex = 0;
	CCollisionProperty *pCollisionProp = CollisionProp();

	Vector vSize = pCollisionProp->OBBSize();
	int iCount = ( vSize[0] * vSize[1] + vSize[1] * vSize[2] + vSize[2] * vSize[0] ) / ( 3 * 12 * 12 );

	if ( iCount > func_break_max_pieces.GetInt() )
	{
		iCount = func_break_max_pieces.GetInt();
	}

	if ( !breakable_disable_gib_limit.GetBool() && iCount )
	{
		if ( m_PerformanceMode == PM_NO_GIBS )
		{
			iCount = 0;
		}
		else if ( m_PerformanceMode == PM_REDUCED_GIBS )
		{
			int iNewCount = iCount * func_break_reduction_factor.GetFloat();
			iCount = std::max( iNewCount, 1 );
		}
	}

	if ( m_iszModelName != NULL_STRING )
	{
		for ( int i = 0; i < iCount; i++ )
		{
			iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel(  STRING( m_iszModelName ) ) );

			// All objects except the first one in this run are marked as slaves...
			int slaveFlag = 0;
			if ( i != 0 )
			{
				slaveFlag = BREAK_SLAVE;
			}

			te->BreakModel( filter2, 0.0, 
				vecSpot, pCollisionProp->GetCollisionAngles(), vSize, 
				vecVelocity, iModelIndex, 100, 1, 2.5, cFlag | slaveFlag );
		}
	}

	ResetOnGroundFlags();

	// Don't fire something that could fire myself
	SetName( NULL_STRING );

	AddSolidFlags( FSOLID_NOT_SOLID );
	
	// Fire targets on break
	m_OnBreak.FireOutput( m_hBreaker, this );

	VPhysicsDestroyObject();
	SetThink( &CBreakable::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
	if ( m_iszSpawnObject != NULL_STRING )
	{
		CBaseEntity::Create( STRING(m_iszSpawnObject), vecSpot, pCollisionProp->GetCollisionAngles(), this );
	}

	if ( Explodable() )
	{
		ExplosionCreate( vecSpot, pCollisionProp->GetCollisionAngles(), this, GetExplosiveDamage(), GetExplosiveRadius(), true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this object can be broken.
//-----------------------------------------------------------------------------
bool CBreakable::IsBreakable( void ) 
{ 
	return m_Material != matUnbreakableGlass;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *CBreakable::DamageDecal_Impl( int bitsDamageType, int gameMaterial )
{
	if ( m_Material == matGlass  )
		return "GlassBreak";

	if ( m_Material == matUnbreakableGlass )
		return "BulletProof";

	return BaseClass::DamageDecal_Impl( bitsDamageType, gameMaterial );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CBreakable::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		if ( GetMaxHealth() )
		{
			char tempstr[512];
			V_snprintf(tempstr,sizeof(tempstr),"Health: %i",GetHealth());
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		if ( m_iszBasePropData != NULL_STRING )
		{
			char tempstr[512];
			V_snprintf(tempstr, sizeof(tempstr),"Base PropData: %s", STRING(m_iszBasePropData) );
			EntityText( text_offset, tempstr, 0);
			text_offset++;
		}
	}

	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Keep track of physgun influence
//-----------------------------------------------------------------------------

void CBreakable::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;
}

void CBreakable::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;
}

CBasePlayer *CBreakable::HasPhysicsAttacker_Impl( float dt )
{
	if (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime)
	{
		return m_hPhysicsAttacker;
	}
	return nullptr;
}

//=============================================================================================================================
// PUSHABLE
//=============================================================================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPushable : public CBreakable
{
public:
	DECLARE_CLASS( CPushable, CBreakable );

	void	Spawn_Impl ( void );
	bool	CreateVPhysics_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() | FCAP_ONOFF_USE; }

	// breakables use an overridden takedamage
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	unsigned int PhysicsSolidMaskForEntity_Impl( void ) const { return MASK_PLAYERSOLID; }
};

LINK_ENTITY_TO_CLASS( func_pushable, CPushable );

void CPushable::Spawn_Impl( void )
{
	if ( HasSpawnFlags( SF_PUSH_BREAKABLE ) )
	{
		BaseClass::Spawn_Impl();
	}
	else
	{
		Precache();

		SetSolid( SOLID_VPHYSICS );

		SetMoveType( MOVETYPE_PUSH );
		SetModel( STRING( GetModelName() ) );

		CreateVPhysics();
	}
}

bool CPushable::CreateVPhysics_Impl( void )
{
	VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj )
	{
		pPhysObj->SetMass( 30 );
//		Vector vecInertia = Vector(800, 800, 800);
//		pPhysObj->SetInertia( vecInertia );
	}

	return true;
}

// Pull the func_pushable
void CPushable::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BaseClass::Use_Impl( pActivator, pCaller, useType, value );
}

int CPushable::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	if ( m_spawnflags & SF_PUSH_BREAKABLE )
		return BaseClass::OnTakeDamage_Impl( info );

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Allows us to take damage from physics objects
//-----------------------------------------------------------------------------
void CPushable::VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];
	if ( pOther->IsPlayer() )
	{
		// Pushables don't take damage from impacts with the player
		// We call all the way back to the baseclass to get the physics effects.
		CBaseEntity::VPhysicsCollision_Impl( index, pEvent );
		return;
	}

	BaseClass::VPhysicsCollision_Impl( index, pEvent );
}

class CBaseEntity;
class CBasePlayer;
class CEffectData;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: A planar textured surface that breaks into increasingly smaller fragments
//			as it takes damage. Undamaged pieces remain attached to the world
//			until they are damaged. Used for window panes.

// Spawn flags
static constexpr auto SF_BREAKABLESURF_CRACK_DECALS = 0x00000001;
static constexpr auto SF_BREAKABLESURF_DAMAGE_FROM_HELD_OBJECTS = 0x00000002;

//#############################################################################
//  > CWindowPane
//#############################################################################
static constexpr auto WINDOW_PANEL_SIZE = 12;
static constexpr auto WINDOW_SMALL_SHARD_SIZE = 4;
static constexpr auto WINDOW_LARGE_SHARD_SIZE = 7;
static constexpr auto WINDOW_MAX_SUPPORT = 6.75;
static constexpr auto WINDOW_BREAK_SUPPORT = 0.20;

static constexpr auto WINDOW_PANE_BROKEN = -1;
static constexpr auto WINDOW_PANE_HEALTHY = 1;

// Also defined in WC
static constexpr auto QUAD_ERR_NONE = 0;
static constexpr auto QUAD_ERR_MULT_FACES = 1;
static constexpr auto QUAD_ERR_NOT_QUAD = 2;

//
// func_breakable - bmodel that breaks into pieces after taking damage
//
LINK_ENTITY_TO_CLASS( window_pane, CWindowPane );

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWindowPane::Spawn_Impl( void )
{
    Precache( );    
  
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	m_takedamage = DAMAGE_YES;
 	
	SetCollisionGroup( COLLISION_GROUP_BREAKABLE_GLASS ); 

	SetModel( "models/brokenglass_piece.mdl" );//set size and link into world.
}

void CWindowPane::Precache_Impl( void )
{
	PrecacheModel( "models/brokenglass_piece.mdl" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pOther - 
//-----------------------------------------------------------------------------
void CWindowPane::PaneTouch( CBaseEntity *pOther )
{
	if (pOther &&
		pOther->GetCollisionGroup() != COLLISION_GROUP_BREAKABLE_GLASS)
	{
		Die();
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWindowPane::Die( void )
{
	Vector flForce = -1 * GetAbsVelocity();

	CPASFilter filter( GetAbsOrigin() );
	te->ShatterSurface( filter, 0.0,
					 &GetAbsOrigin(), &GetAbsAngles(), 
					 &GetAbsVelocity(), &GetAbsOrigin(),
					 WINDOW_PANEL_SIZE, WINDOW_PANEL_SIZE,WINDOW_SMALL_SHARD_SIZE,SHATTERSURFACE_GLASS,
					 255,255,255,255,255,255);

	UTIL_Remove(this);
}

///------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CWindowPane* CWindowPane::CreateWindowPane( const Vector &vecOrigin, const QAngle &vecAngles )
{
	CWindowPane *pGlass = (CWindowPane*)CreateEntityByName( "window_pane" );
	if ( !pGlass )
	{
		Msg( "NULL Ent in CreateWindowPane!\n" );
		return nullptr;
	}

	if ( pGlass->edict() )
	{
		pGlass->SetLocalOrigin( vecOrigin );
		pGlass->SetLocalAngles( vecAngles );
		pGlass->Spawn();
		pGlass->SetTouch(&CWindowPane::PaneTouch);
		pGlass->SetLocalAngularVelocity( RandomAngle(-50,50) );
		pGlass->m_nBody = random_valve->RandomInt(0,2);
	}
	return pGlass;
}

//####################################################################################
//	> CBreakableSurface
//####################################################################################
LINK_ENTITY_TO_CLASS( func_breakable_surf, CBreakableSurface );

IMPLEMENT_REFLECT_DATAMAP( CBreakableSurface )

IMPLEMENT_REFLECT_SERVERCLASS( CBreakableSurface, DT_BreakableSurface )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBreakableSurface::Precache_Impl(void)
{
	UTIL_PrecacheOther( "window_pane" );

	// Load the edge types and styles for the specific surface type
	if (m_nSurfaceType == SHATTERSURFACE_TILE)
	{
		PrecacheMaterial( "models/brokentile/tilebroken_03a" );
		PrecacheMaterial( "models/brokentile/tilebroken_03b" );
		PrecacheMaterial( "models/brokentile/tilebroken_03c" );
		PrecacheMaterial( "models/brokentile/tilebroken_03d" );

		PrecacheMaterial( "models/brokentile/tilebroken_02a" );
		PrecacheMaterial( "models/brokentile/tilebroken_02b" );
		PrecacheMaterial( "models/brokentile/tilebroken_02c" );
		PrecacheMaterial( "models/brokentile/tilebroken_02d" );

		PrecacheMaterial( "models/brokentile/tilebroken_01a" );
		PrecacheMaterial( "models/brokentile/tilebroken_01b" );
		PrecacheMaterial( "models/brokentile/tilebroken_01c" );
		PrecacheMaterial( "models/brokentile/tilebroken_01d" );
	}
	else
	{
		PrecacheMaterial( "models/brokenglass/glassbroken_solid" );
		PrecacheMaterial( "models/brokenglass/glassbroken_01a" );
		PrecacheMaterial( "models/brokenglass/glassbroken_01b" );
		PrecacheMaterial( "models/brokenglass/glassbroken_01c" );
		PrecacheMaterial( "models/brokenglass/glassbroken_01d" );
		PrecacheMaterial( "models/brokenglass/glassbroken_02a" );
		PrecacheMaterial( "models/brokenglass/glassbroken_02b" );
		PrecacheMaterial( "models/brokenglass/glassbroken_02c" );
		PrecacheMaterial( "models/brokenglass/glassbroken_02d" );
		PrecacheMaterial( "models/brokenglass/glassbroken_03a" );
		PrecacheMaterial( "models/brokenglass/glassbroken_03b" );
		PrecacheMaterial( "models/brokenglass/glassbroken_03c" );
		PrecacheMaterial( "models/brokenglass/glassbroken_03d" );
	}

	PrecacheEffect( "GlassImpact" );
	PrecacheEffect( "Impact" );

	BaseClass::Precache_Impl();
}

//------------------------------------------------------------------------------
// Purpose : Window has been touched.  Break out pieces based on touching
//			 entity's bounding box
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::SurfaceTouch( CBaseEntity *pOther )
{
	// If tile only break if object is moving fast
	if (m_nSurfaceType == SHATTERSURFACE_TILE)
	{
		Vector vVel;
		pOther->GetVelocity( &vVel, nullptr );
		if (vVel.Length() < 500)
		{
			return;
		}
	}

	// Find nearest point on plane for max
	Vector vecAbsMins, vecAbsMaxs;
	pOther->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );
	Vector vToPlane		= (vecAbsMaxs - m_vCorner);
	float  vDistToPlane = DotProduct(m_vNormal,vToPlane);
	Vector vTouchPos	= vecAbsMaxs + vDistToPlane*m_vNormal;

	float flMinsWidth,flMinsHeight;
	PanePos(vTouchPos, &flMinsWidth, &flMinsHeight);

	// Find nearest point on plane for mins
	vToPlane		= (vecAbsMins - m_vCorner);
	vDistToPlane = DotProduct(m_vNormal,vToPlane);
	vTouchPos	= vecAbsMins + vDistToPlane*m_vNormal;

	float flMaxsWidth,flMaxsHeight;
	PanePos(vTouchPos, &flMaxsWidth, &flMaxsHeight);

	int nMinWidth = Floor2Int(std::max<std::common_type_t<int, float>>(0,		std::min(flMinsWidth,flMaxsWidth)));
	int nMaxWidth = Ceil2Int( fpmin(m_nNumWide, std::max(flMinsWidth,flMaxsWidth)));

	int nMinHeight = Floor2Int(std::max<std::common_type_t<int, float>>(0,		std::min(flMinsHeight,flMaxsHeight)));
	int nMaxHeight = Ceil2Int( fpmin(m_nNumHigh, std::max(flMinsHeight,flMaxsHeight)));

	Vector vHitVel;
	pOther->GetVelocity( &vHitVel, nullptr );

	// Move faster then penetrating object so can see shards
	vHitVel *= 5;

	// If I'm not broken yet, break me
	if ( !m_bIsBroken )
	{
		Die( pOther, vHitVel );
	}

	for (int height=nMinHeight;height<nMaxHeight;height++)
	{
		// Randomly break the one before so it doesn't look square
		if (random_valve->RandomInt(0,1))
		{
			ShatterPane(nMinWidth-1, height,vHitVel,pOther->GetLocalOrigin());
		}
		for (int width=nMinWidth;width<nMaxWidth;width++)
		{
			ShatterPane(width, height,vHitVel,pOther->GetLocalOrigin());
		}
		// Randomly break the one after so it doesn't look square
		if (random_valve->RandomInt(0,1))
		{
			ShatterPane(nMaxWidth+1, height,vHitVel,pOther->GetLocalOrigin());
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Only take damage in trace attack
// Input   :
// Output  :
//------------------------------------------------------------------------------
int CBreakableSurface::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	if ( !m_bIsBroken && info.GetDamageType() == DMG_CRUSH )
	{
		// physics will kill me now
		Die( info.GetAttacker(), info.GetDamageForce() );
		return 0;
	}

	if ( m_nSurfaceType == SHATTERSURFACE_GLASS && info.GetDamageType() & DMG_BLAST )
	{
		Vector vecDir = info.GetInflictor()->GetAbsOrigin() - WorldSpaceCenter();
		VectorNormalize( vecDir );
		Die( info.GetAttacker(), vecDir );
		return 0;
	}

	// Accept slash damage, too. Manhacks and such.
	if ( m_nSurfaceType == SHATTERSURFACE_GLASS && (info.GetDamageType() & DMG_SLASH) )
	{
		Die( info.GetAttacker(), info.GetDamageForce() );
		return 0;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Purpose: Accepts damage and breaks if health drops below zero.
//------------------------------------------------------------------------------
void CBreakableSurface::TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	// [dwenger] Window break stat tracking
	// Make sure this pane has not already been shattered
	bool bWasBroken = m_bIsBroken;

	// Decrease health
	m_iHealth -= info.GetDamage();
	m_OnHealthChanged.Set( m_iHealth, info.GetAttacker(), this );

	// If I'm not broken yet, break me
	if (!m_bIsBroken )
	{
		Vector vSurfDir = ptr->endpos - ptr->startpos;
		Die( info.GetAttacker(), vSurfDir );
	}

	if (info.GetDamageType() & (DMG_BULLET | DMG_CLUB))
	{
		// Figure out which panel has taken the damage and break it
		float flWidth,flHeight;
		PanePos(ptr->endpos,&flWidth,&flHeight);
		int nWidth  = flWidth;
		int nHeight = flHeight;
		
		if ( ShatterPane(nWidth, nHeight,vecDir*500,ptr->endpos) )
		{
			// [dwenger] Window break stat tracking
			CBasePlayer* pAttacker = ToBasePlayer(info.GetAttacker());
			if ( ( pAttacker ) && ( !bWasBroken ) )
			{
				gamestats->Event_WindowShattered( pAttacker );
			}

			// Do an impact hit
			CEffectData	data;

			data.m_vNormal = ptr->plane.normal;
			data.m_vOrigin = ptr->endpos;

			CPASFilter filter( data.m_vOrigin );

			// client cannot trace against triggers
			filter.SetIgnorePredictionCull( true );

			DispatchEffect( filter, 0.0, "GlassImpact", data );
		}

		if (m_nSurfaceType == SHATTERSURFACE_GLASS)
		{
			// Break nearby panes if damages was near pane edge
			float flWRem = flWidth  - nWidth;
			float flHRem = flHeight - nHeight;

			if (flWRem > 0.8 && nWidth != m_nNumWide-1)
			{
				ShatterPane(nWidth+1, nHeight,vecDir*500,ptr->endpos);
			}
			else if (flWRem < 0.2 && nWidth != 0)
			{
				ShatterPane(nWidth-1, nHeight,vecDir*500,ptr->endpos);
			}
			if (flHRem > 0.8 && nHeight != m_nNumHigh-1)
			{
				ShatterPane(nWidth, nHeight+1,vecDir*500,ptr->endpos);
			}
			else if (flHRem < 0.2 && nHeight != 0)
			{
				ShatterPane(nWidth, nHeight-1,vecDir*500,ptr->endpos);
			}

			// Occasionally break the pane above me
			if (random_valve->RandomInt(0,1)==0)
			{
				ShatterPane(nWidth, nHeight+1,vecDir*1000,ptr->endpos);
				// Occasionally break the pane above that
				if (random_valve->RandomInt(0,1)==0)
				{
					ShatterPane(nWidth, nHeight+2,vecDir*1000,ptr->endpos);
				}		
			}
		}
	}
	else if (info.GetDamageType() & (DMG_SONIC | DMG_BLAST))
	{
		// ----------------------------------------
		// If it's tile blow out nearby tiles
		// ----------------------------------------
		if (m_nSurfaceType == SHATTERSURFACE_TILE)
		{
			// Figure out which panel has taken the damage and break it
			float flWidth,flHeight;
			if (info.GetAttacker())
			{
				PanePos(info.GetAttacker()->GetAbsOrigin(),&flWidth,&flHeight);
			}
			else
			{
				PanePos(ptr->endpos,&flWidth,&flHeight);
			}
			int nWidth  = flWidth;
			int nHeight = flHeight;

			// Blow out a roughly circular patch of tile with some randomness
			for (int width =nWidth-4;width<nWidth+4;width++)
			{
				for (int height =nHeight-4;height<nHeight+4;height++)
				{
					if ((abs(nWidth-width)+abs(nHeight-height))<random_valve->RandomInt(2,5))
					{
						ShatterPane(width, height,vecDir*500,ptr->endpos);
					}
				}
			}
		}
		// ----------------------------------------
		// If it's glass blow out the whole window
		// ----------------------------------------
		else
		{
			// [pfreese] Window break stat tracking
			CBasePlayer* pAttacker = ToBasePlayer(info.GetAttacker());
			if ( ( pAttacker ) && ( !bWasBroken ) )
			{
				gamestats->Event_WindowShattered( pAttacker );
			}

			float flDot = DotProduct(m_vNormal,vecDir);

			float damageMultiplier = info.GetDamage();

			Vector vBlastDir;
			if (flDot > 0)
			{
				vBlastDir = damageMultiplier * 3000 * m_vNormal;
			}
			else
			{
				vBlastDir = damageMultiplier * -3000 * m_vNormal;
			}

			// Has the window already been destroyed?
			if (m_nNumBrokenPanes >= m_nNumWide*m_nNumHigh)
			{
				return;
			}
			// ---------------------------------------------------------------
			// If less than 10% of my panels have been broken, blow me 
			// up in one large glass shatter
			// ---------------------------------------------------------------
			else if ( m_nNumBrokenPanes < 0.1*(m_nNumWide*m_nNumHigh))
			{
				QAngle vAngles;
				VectorAngles(-1*m_vNormal,vAngles);

				CreateShards(m_vCorner, vAngles,vBlastDir, ptr->endpos, 
							m_nNumWide*m_flPanelWidth, m_nNumHigh*m_flPanelHeight, WINDOW_LARGE_SHARD_SIZE);
			}
			// ---------------------------------------------------------------
			// Otherwise break in the longest vertical strips possible
			// (to cut down on the network bandwidth)
			// ---------------------------------------------------------------
			else
			{
				QAngle vAngles;
				VectorAngles(-1*m_vNormal,vAngles);
				Vector vWidthDir,vHeightDir;
				AngleVectors(vAngles,nullptr,&vWidthDir,&vHeightDir);

				for (int width=0;width<m_nNumWide;width++)
				{
					int height;
					int nHCount = 0;
					for ( height=0;height<m_nNumHigh;height++)
					{
						// Keep count of how many panes
						if (!IsBroken(width,height))
						{
							nHCount++;
						}
						// Shatter the strip and start counting again
						else if (nHCount > 0)
						{
							Vector vBreakPos = m_vCorner + 
													(width*vWidthDir*m_flPanelWidth) + 
													((height-nHCount)*vHeightDir*m_flPanelHeight);

							CreateShards(vBreakPos, vAngles,
								 vBlastDir,	  ptr->endpos,
								 m_flPanelWidth, nHCount*m_flPanelHeight,
								 WINDOW_LARGE_SHARD_SIZE);

							nHCount = 0;
						}
					}
					if (nHCount)
					{
						Vector vBreakPos = m_vCorner + 
												(width*vWidthDir*m_flPanelWidth) + 
												((height-nHCount)*vHeightDir*m_flPanelHeight);

						CreateShards(vBreakPos, vAngles,
								 vBlastDir,	  ptr->endpos,
								 m_flPanelWidth,nHCount*m_flPanelHeight,
								 WINDOW_LARGE_SHARD_SIZE);
					}
				}
			}

			BreakAllPanes();
		}
	}
}

//------------------------------------------------------------------------------
// Purpose: Break into panels
// Input  : pBreaker - 
//			vDir - 
//-----------------------------------------------------------------------------
void CBreakableSurface::Die( CBaseEntity *pBreaker, const Vector &vAttackDir )
{
	if ( m_bIsBroken )
		return;

	// Play a break sound
	PhysBreakSound( this, VPhysicsGetObject(), GetAbsOrigin() );

	m_bIsBroken = true;
	m_iHealth = 0.0f;

	if (pBreaker)
	{
		m_OnBreak.FireOutput( pBreaker, this );
	}
	else
	{
		m_OnBreak.FireOutput( this, this );
	}

	float flDir = -1;

	if ( vAttackDir.LengthSqr() > 0.001 )
	{
		float flDot = DotProduct( m_vNormal, vAttackDir );
		if (flDot < 0)
		{
			m_vLLVertex += m_vNormal;
			m_vLRVertex += m_vNormal;
			m_vULVertex += m_vNormal;
			m_vURVertex += m_vNormal;
			m_vNormal	*= -1;
			flDir		=   1;
		}
	}

	// -------------------------------------------------------
	// The surface has two sides, when we are killed pick 
	// the side that the damage came from 
	// -------------------------------------------------------
	Vector vWidth		= m_vLLVertex - m_vLRVertex;
	Vector vHeight		= m_vLLVertex - m_vULVertex;
	CrossProduct( vWidth, vHeight, m_vNormal.GetForModify() );
	VectorNormalize(m_vNormal.GetForModify());

	// ---------------------------------------------------
	//  Make sure width and height are oriented correctly
	// ---------------------------------------------------
	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthDir,vHeightDir;
	AngleVectors(vAngles,nullptr,&vWidthDir,&vHeightDir);

	float flWDist = DotProduct(vWidthDir,vWidth);
	if (fabs(flWDist)<0.5)
	{
		Vector vSaveHeight	= vHeight;
		vHeight				= vWidth * flDir;
		vWidth				= vSaveHeight * flDir;
	}

	// -------------------------------------------------
	// Find which corner to use
	// -------------------------------------------------
	bool bLeft  = (DotProduct(vWidthDir,vWidth)   < 0);
	bool bLower = (DotProduct(vHeightDir,vHeight) < 0);
	if (bLeft)
	{
		m_vCorner = bLower ? m_vLLVertex : m_vULVertex;
	}
	else 
	{
		m_vCorner = bLower ? m_vLRVertex : m_vURVertex;
	}

	// -------------------------------------------------
	//  Calculate the number of panels
	// -------------------------------------------------
	float flWidth		= vWidth.Length();
	float flHeight		= vHeight.Length();
	m_nNumWide			= flWidth  / WINDOW_PANEL_SIZE;
	m_nNumHigh			= flHeight / WINDOW_PANEL_SIZE;

	// If to many panels make panel size bigger
	if (m_nNumWide > MAX_NUM_PANELS) m_nNumWide = MAX_NUM_PANELS;
	if (m_nNumHigh > MAX_NUM_PANELS) m_nNumHigh = MAX_NUM_PANELS;

	m_flPanelWidth	= flWidth  / m_nNumWide;
	m_flPanelHeight	= flHeight / m_nNumHigh;
	
	// Initialize panels
	for (int w=0;w<MAX_NUM_PANELS;w++)
	{ 
		for (int h=0;h<MAX_NUM_PANELS;h++)
		{
			SetSupport( w, h, WINDOW_PANE_HEALTHY );
		}
	}

	// Reset onground flags for any entity that may 
	// have been standing on me
	ResetOnGroundFlags();

	VPhysicsDestroyObject();
	AddSolidFlags( FSOLID_TRIGGER );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetTouch(&CBreakableSurface::SurfaceTouch);
}

//------------------------------------------------------------------------------
// Purpose: Set an instaneous force on the rope.
// Input  : Force vector.
//------------------------------------------------------------------------------
void CBreakableSurface::InputShatter( inputdata_t &inputdata )
{
	Vector vecShatterInfo;
	inputdata.value.Vector3D(vecShatterInfo);

	if (!m_bIsBroken)
	{
		Die( nullptr, vec3_origin );
	}

	// Figure out which panel has taken the damage and break it
	float flCenterX = vecShatterInfo.x * m_nNumWide;
	float flCenterY = vecShatterInfo.y * m_nNumHigh;

	// Bah: m_flPanelWidth is the width of a single panel
	int nMinX = (int)(flCenterX - vecShatterInfo.z / m_flPanelWidth);
	int nMaxX = (int)(flCenterX + vecShatterInfo.z / m_flPanelWidth) + 1;
	if (nMinX < 0)
		nMinX = 0;
	if (nMaxX > m_nNumWide)
		nMaxX = m_nNumWide;

	int nMinY = (int)(flCenterY - vecShatterInfo.z / m_flPanelHeight);
	int nMaxY = (int)(flCenterY + vecShatterInfo.z / m_flPanelHeight) + 1;

	if (nMinY < 0)
		nMinY = 0;
	if (nMaxY > m_nNumHigh)
		nMaxY = m_nNumHigh;

	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthDir,vHeightDir;
	AngleVectors(vAngles,nullptr,&vWidthDir,&vHeightDir);

	// Blow out a roughly circular of tile with some randomness
	Vector2D vecActualCenter( flCenterX * m_flPanelWidth, flCenterY * m_flPanelHeight ); 
	for (int width = nMinX; width < nMaxX; width++)
	{
		for (int height = nMinY; height < nMaxY; height++)
		{
			Vector2D pt( (width + 0.5f) * m_flPanelWidth, (height + 0.5f) * m_flPanelWidth );
			if ( pt.DistToSqr(vecActualCenter) <= vecShatterInfo.z * vecShatterInfo.z )
			{
				Vector vBreakPos	= m_vCorner + 
									(width*vWidthDir*m_flPanelWidth) + 
									(height*vHeightDir*m_flPanelHeight);

				ShatterPane( width, height, m_vNormal * 500, vBreakPos );
			}
		}
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::Event_Killed( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType )
{
	return;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CBreakableSurface::IsBroken(int nWidth, int nHeight)
{
	if (nWidth  < 0  || nWidth  >= m_nNumWide) return true;
	if (nHeight < 0 || nHeight >= m_nNumHigh)  return true;

	return (m_flSupport[nWidth][nHeight]==WINDOW_PANE_BROKEN);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : w - 
//			h - 
//			support - 
//-----------------------------------------------------------------------------
void CBreakableSurface::SetSupport( int w, int h, float support )
{
	m_flSupport[ w ][ h ] = support;

	int offset = w + h * m_nNumWide;

	bool prevval = m_RawPanelBitVec.Get( offset );
	bool curval = prevval;

	if ( support < 0.0f )
	{
		curval = false;
	}
	else
	{
		curval = true;
	}
	if ( curval != prevval )
	{
		m_RawPanelBitVec.Set( offset, curval );
		m_RawPanelBitVec.GetForModify( offset );
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
float CBreakableSurface::GetSupport(int nWidth, int nHeight)
{
	return std::max<std::common_type_t<int, float>>(0,m_flSupport[nWidth][nHeight]);
}

//------------------------------------------------------------------------------
// Purpose : Return the structural support for this pane.  Assumes window
//			 is upright.  Still works for windows parallel to the ground
//			 but simulation isn't quite as good
// Input   :
// Output  :
//------------------------------------------------------------------------------
float CBreakableSurface::RecalcSupport(int nWidth, int nHeight)
{
	// Always has some support. Zero signifies that it has been broken
	float flSupport = 0.01;

	// ------------
	// Top support
	// ------------
	if (nHeight == m_nNumHigh-1)
	{
		flSupport += 1.0;
	}
	else
	{
		flSupport += GetSupport(nWidth,nHeight+1);	 
	}

	// ------------
	// Bottom Support
	// ------------
	if (nHeight == 0)
	{
		flSupport += 1.25;
	}
	else
	{
 		flSupport += 1.25 * GetSupport(nWidth,nHeight-1);
	}

	// ------------
	// Left Support
	// ------------
	if (nWidth == 0)
	{
		flSupport += 1.0;
	}
	else
	{
		flSupport += GetSupport(nWidth-1,nHeight);
	}

	// --------------
	// Right Support
	// --------------
	if (nWidth == m_nNumWide-1)
	{
		flSupport += 1.0;
	}
	else
	{
		flSupport += GetSupport(nWidth+1,nHeight);
	}

	// --------------------
	// Bottom Left Support
	// --------------------
	if (nHeight == 0 || nWidth == 0)
	{
		flSupport += 1.0;
	}
	else
	{
		flSupport += GetSupport(nWidth-1,nHeight-1);
	}

	// ---------------------
	// Bottom Right Support
	// ---------------------
	if (nHeight == 0 || nWidth == m_nNumWide-1)
	{
		flSupport += 1.0;
	}
	else
	{
		flSupport += GetSupport(nWidth+1,nHeight-1);
	}

	// -----------------
	// Top Right Support
	// -----------------
	if (nHeight == m_nNumHigh-1 || nWidth == m_nNumWide-1)
	{
		flSupport += 0.25;
	}
	else
	{
		flSupport += 0.25 * GetSupport(nWidth+1,nHeight+1);
	}

	// -----------------
	// Top Left Support
	// -----------------
	if (nHeight == m_nNumHigh-1 || nWidth == 0)
	{
		flSupport += 0.25;
	}
	else
	{
		flSupport += 0.25 * GetSupport(nWidth-1,nHeight+1);
	}
	
	return flSupport;
}

//------------------------------------------------------------------------------
// Purpose : Itterate through the panels and make sure none have become
//			 unstable
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::BreakThink(void)
{
	// Don't calculate support if I'm tile
	if (m_nSurfaceType == SHATTERSURFACE_TILE)
	{
		return;
	}

	// -----------------------
	// Recalculate all support
	// -----------------------
	int w;
	float flSupport[MAX_NUM_PANELS][MAX_NUM_PANELS];
	for (w=0;w<m_nNumWide;w++)
	{
		for (int h=0;h<m_nNumHigh;h++)
		{
			if (!IsBroken(w,h))
			{
				flSupport[w][h] = RecalcSupport(w,h);
			}
		}
	}

	// ----------------------------------------------------
	//  Set support and break inadequately supported panes
	// ----------------------------------------------------
	float flBreakValue = WINDOW_BREAK_SUPPORT*(m_nFragility/100.0);
	for (w=0;w<m_nNumWide;w++)
	{
		for (int h=0;h<m_nNumHigh;h++)
		{
			if (!IsBroken(w,h))
			{
				SetSupport( w, h, flSupport[w][h]/WINDOW_MAX_SUPPORT );
				if (m_flSupport[w][h] < flBreakValue)
				{
					// Occasionaly drop a pane
					if (random_valve->RandomInt(0,1))
					{
						DropPane(w,h);
					}
					// Otherwise just shatter the glass
					else
					{
						ShatterPane(w,h,vec3_origin,vec3_origin);
					}
					SetNextThink( gpGlobals->curtime );
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Purpose : Given a 3D position on the window in space return the height and
//			 width of the position from the window's corner
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::PanePos(const Vector &vPos, float *flWidth, float *flHeight)
{
	Vector vAttackVec = vPos - m_vCorner;
	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthDir,vHeightDir;
	AngleVectors(vAngles,nullptr,&vWidthDir,&vHeightDir);
	float flWDist = DotProduct(vWidthDir,vAttackVec);
	float flHDist = DotProduct(vHeightDir,vAttackVec);

	// Figure out which quadrent I'm in
	*flWidth  = flWDist/m_flPanelWidth;
	*flHeight = flHDist/m_flPanelHeight;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::BreakAllPanes(void)
{
	// Now tell the client all the panes have been broken
	for (int width=0;width<m_nNumWide;width++)
	{
		for (int height=0;height<m_nNumHigh;height++)
		{
			//SetSupport( width, height, WINDOW_PANE_BROKEN );

			BreakPane(width,height);
		}
	}

	m_nNumBrokenPanes = m_nNumWide*m_nNumHigh;
}

//------------------------------------------------------------------------------
// Purpose : Drop a window pane entity
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::BreakPane(int nWidth, int nHeight)
{
	// Check parameter range
	if (nWidth < 0  || nWidth  >= m_nNumWide) return;
	if (nHeight < 0 || nHeight >= m_nNumHigh) return;

	// Count how many panes have been broken or dropped
	m_nNumBrokenPanes++;
	SetSupport( nWidth, nHeight, WINDOW_PANE_BROKEN );

	SetThink(&CBreakableSurface::BreakThink);
	SetNextThink( gpGlobals->curtime );
}

//------------------------------------------------------------------------------
// Purpose : Drop a window pane entity
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::DropPane(int nWidth, int nHeight)
{
	// Check parameter range
	if (nWidth < 0  || nWidth  >= m_nNumWide) return;
	if (nHeight < 0 || nHeight >= m_nNumHigh) return;

	if (!IsBroken(nWidth,nHeight))
	{
		BreakPane(nWidth,nHeight);

		QAngle vAngles;
		VectorAngles(-1*m_vNormal,vAngles);
		
		Vector vWidthDir,vHeightDir;
		AngleVectors(vAngles,nullptr,&vWidthDir,&vHeightDir);
		Vector vBreakPos	= m_vCorner + 
								(nWidth*vWidthDir*m_flPanelWidth) + 
								(nHeight*vHeightDir*m_flPanelHeight);

		CreateShards(vBreakPos, vAngles, vec3_origin, vec3_origin,
						WINDOW_PANEL_SIZE,	WINDOW_PANEL_SIZE,
						WINDOW_SMALL_SHARD_SIZE);

		DamageSound();

		CWindowPane *pPane = CWindowPane::CreateWindowPane(vBreakPos, vAngles);
		if (pPane)
		{
			pPane->SetLocalAngularVelocity( RandomAngle(-120,120) );
		}
	}
}

void CBreakableSurface::CreateShards(const Vector &vBreakPos, const QAngle &vAngles,
									 const Vector &vForce,	  const Vector &vForcePos,
									 float flWidth,			  float flHeight,
									 int   nShardSize)
{
	Vector	vAdjustedBreakPos = vBreakPos;
	Vector	vAdjustedForce	 = vForce;
	int		front_r,front_g,front_b;
	int		back_r,back_g,back_b;

	// UNDONE: For now hardcode these colors.  Later when used by more textures
	//         we'll automate this process or expose the colors in WC
	if (m_nSurfaceType == SHATTERSURFACE_TILE)
	{
		// If tile shoot shards back from the shattered surface and offset slightly 
		// from the surface.
		vAdjustedBreakPos  -=  8*m_vNormal; 
		vAdjustedForce		= -0.75*vForce;
		front_r				= 89;
		front_g				= 120;
		front_b				= 83;
		back_r				= 99;
		back_g				= 76;
		back_b				= 21;
	}
	else
	{
		front_r				= 255;
		front_g				= 255;
		front_b				= 255;
		back_r				= 255;
		back_g				= 255;
		back_b				= 255;
	}
	
	CPASFilter filter( vAdjustedBreakPos );
	te->ShatterSurface(filter, 0.0,
		&vAdjustedBreakPos, &vAngles, 
		&vAdjustedForce, &vForcePos, 
		flWidth, flHeight,WINDOW_SMALL_SHARD_SIZE,m_nSurfaceType,
		front_r,front_g,front_b,back_r,back_g,back_b);//4);
}

//------------------------------------------------------------------------------
// Purpose : Break a panel
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CBreakableSurface::ShatterPane(int nWidth, int nHeight, const Vector &vForce, const Vector &vForcePos)
{
	// Check parameter range
	if (nWidth < 0  || nWidth  >= m_nNumWide) return false;
	if (nHeight < 0 || nHeight >= m_nNumHigh) return false;

	if ( IsBroken(nWidth,nHeight) )
		return false;

	BreakPane(nWidth,nHeight);

	QAngle vAngles;
	VectorAngles(-1*m_vNormal,vAngles);
	Vector vWidthDir,vHeightDir;
	AngleVectors(vAngles,nullptr,&vWidthDir,&vHeightDir);
	Vector vBreakPos	= m_vCorner + 
						(nWidth*vWidthDir*m_flPanelWidth) + 
						(nHeight*vHeightDir*m_flPanelHeight);

	CreateShards(vBreakPos, vAngles,vForce,	vForcePos, m_flPanelWidth, m_flPanelHeight, WINDOW_SMALL_SHARD_SIZE);

	DamageSound();
	return true;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakableSurface::Spawn_Impl(void)
{
	BaseClass::Spawn_Impl();
	SetCollisionGroup( COLLISION_GROUP_BREAKABLE_GLASS ); 
	m_bIsBroken = false;

	if (m_nQuadError == QUAD_ERR_MULT_FACES)
	{
		Vector center = WorldSpaceCenter();
		Warning( "Rejecting func_breakablesurf at (%2.2f, %2.2f, %2.2f).  Has multiple faces that aren't NODRAW.\n",
			center.x, center.y, center.z );
		UTIL_Remove(this);
	}
	else if (m_nQuadError == QUAD_ERR_NOT_QUAD)
	{
		Vector center = WorldSpaceCenter();
		Warning( "Rejecting func_breakablesurf at (%2.2f, %2.2f, %2.2f).  Drawn face isn't a quad.\n",
			center.x, center.y, center.z );
		UTIL_Remove(this);
	}

	int materialCount = modelinfo->GetModelMaterialCount( const_cast<model_t*>(GetModel()) );
	if( materialCount != 1 )
	{
		Vector center = WorldSpaceCenter();
		Warning( "Encountered func_breakablesurf at (%2.2f, %2.2f, %2.2f) that has a material applied to more than one surface!\n",
			center.x, center.y, center.z );
		UTIL_Remove(this);
	}

	// Get at the first material; even if there are more than one.
	IMaterial* pMaterial;
	modelinfo->GetModelMaterials( const_cast<model_t*>(GetModel()), 1, &pMaterial );

	// The material should point to a cracked version of itself
	bool foundVar;
	IMaterialVar* pCrackName = pMaterial->FindVar( "$crackmaterial", &foundVar, false );
	if ( foundVar && IsPrecacheAllowed() )
	{
		PrecacheMaterial( pCrackName->GetStringValue() );
	}

	// Init the Panel bit vector to all true. ( no panes are broken )
	int bitVecLength = MAX_NUM_PANELS * MAX_NUM_PANELS;
	
	for( int i=0;i<bitVecLength;i++ )
	{
		m_RawPanelBitVec.Set( i, true );
	}
}

void CBreakableSurface::VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent )
{
	if ( !m_bIsBroken )
	{
		int damageType = 0;
		string_t iszDamageTable = ( ( m_nSurfaceType == SHATTERSURFACE_GLASS ) ? ( "glass" ) : ( NULL_STRING ) );
		bool bDamageFromHeldObjects = ( ( m_spawnflags & SF_BREAKABLESURF_DAMAGE_FROM_HELD_OBJECTS ) != 0 );
		float damage = CalculateDefaultPhysicsDamage( index, pEvent, 1.0, false, damageType, iszDamageTable, bDamageFromHeldObjects );

		if ( damage > 10 )
		{
			// HACKHACK: Reset mass to get correct collision response for the object breaking this
			pEvent->pObjects[index]->SetMass( 2.0f );

			Vector normal, damagePos;
			pEvent->pInternalData->GetSurfaceNormal( normal );
			if ( index == 0 )
			{
				normal *= -1.0f;
			}
			pEvent->pInternalData->GetContactPoint( damagePos );
			int otherIndex = !index;
			CBaseEntity *pInflictor = pEvent->pEntities[otherIndex];
			CTakeDamageInfo info( pInflictor, pInflictor, normal, damagePos, damage, damageType );
			PhysCallbackDamage( this, info, *pEvent, index );
		}
		else if ( damage > 0 )
		{
			if ( m_spawnflags & SF_BREAKABLESURF_CRACK_DECALS )
			{

				Vector normal, damagePos;
				pEvent->pInternalData->GetSurfaceNormal( normal );
				if ( index == 0 )
				{
					normal *= -1.0f;
				}
				pEvent->pInternalData->GetContactPoint( damagePos );

				trace_t tr;
				UTIL_TraceLine ( damagePos - normal, damagePos + normal, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

				// Only place decals and draw effects if we hit something valid
				if ( tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>() == this )
				{
					// Build the impact data
					CEffectData data;
					data.m_vOrigin = tr.endpos;
					data.m_vStart = tr.startpos;
					data.m_nSurfaceProp = tr.surface.surfaceProps;
					data.m_nDamageType = DMG_CLUB;
					data.m_nHitBox = tr.hitbox;
					data.m_nEntIndex = entindex();

					// Send it on its way
					DispatchEffect( "Impact", data );
				}
			}
		}
	}
	BaseClass::VPhysicsCollision_Impl( index, pEvent );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Volumetric dust motes.

static constexpr auto DUST_LIFETIME_NETWORK_BITS = 4;

class CFunc_Dust : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Func_Dust", .base = false } ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } >{} ]]
	[[= ks::reflect::FromTable<"m_Collision", &REFERENCE_SEND_TABLE( DT_CollisionProperty )>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFunc_Dust, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CFunc_Dust();
	virtual 		~CFunc_Dust();

// CBaseEntity overrides.
public:

	void	Spawn_Impl();
	void	Activate_Impl();
	void	Precache_Impl();
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

// Input handles.
public:
	
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );

// FGD properties.
public:

	CNetworkVar( color32, m_Color, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]]
	                              [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "Color" } ]] );
	CNetworkVar( int, m_SpawnRate, [[= ks::reflect::Net{ .bits = 12, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "SpawnRate" } ]] );
	
	CNetworkVar( float, m_flSizeMin, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "SizeMin" } ]] );
	CNetworkVar( float, m_flSizeMax, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "SizeMax" } ]] );

	CNetworkVar( int, m_SpeedMax, [[= ks::reflect::Net{ .bits = 12, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "SpeedMax" } ]] );

	CNetworkVar( int, m_LifetimeMin, [[= ks::reflect::Net{ .bits = DUST_LIFETIME_NETWORK_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "LifetimeMin" } ]] );
	CNetworkVar( int, m_LifetimeMax, [[= ks::reflect::Net{ .bits = DUST_LIFETIME_NETWORK_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "LifetimeMax" } ]] );

	CNetworkVar( int, m_DistMax, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "DistMax" } ]] );

	CNetworkVar( float, m_FallSpeed, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "FallSpeed" } ]] );

	CNetworkVar( bool, m_bAffectedByWind, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "AffectedByWind" } ]] );

public:

	CNetworkVar( int, m_DustFlags, [[= ks::reflect::Net{ .bits = DUST_NUMFLAGS, .flags = SPROP_UNSIGNED } ]] );	// Combination of DUSTFLAGS_

private:	
	int			m_iAlpha;

};

class CFunc_DustMotes : public CFunc_Dust
{
	DECLARE_CLASS( CFunc_DustMotes, CFunc_Dust );
public:
					CFunc_DustMotes();
};

class CFunc_DustCloud : public CFunc_Dust
{
	DECLARE_CLASS( CFunc_DustCloud, CFunc_Dust );
public:
};

// Changing this will break demos. Peeling it out to clamp post creation to fix some shipped maps that specify out of range lifetimes. 
IMPLEMENT_REFLECT_SERVERCLASS( CFunc_Dust, DT_Func_Dust )

IMPLEMENT_REFLECT_DATAMAP( CFunc_Dust )

LINK_ENTITY_TO_CLASS( func_dustmotes, CFunc_DustMotes );
LINK_ENTITY_TO_CLASS( func_dustcloud, CFunc_DustCloud );

// ------------------------------------------------------------------------------------- //
// CFunc_DustMotes implementation.
// ------------------------------------------------------------------------------------- //

CFunc_DustMotes::CFunc_DustMotes()
{
	m_DustFlags |= DUSTFLAGS_SCALEMOTES;
}

// ------------------------------------------------------------------------------------- //
// CFunc_Dust implementation.
// ------------------------------------------------------------------------------------- //

CFunc_Dust::CFunc_Dust()
{
	m_DustFlags = DUSTFLAGS_ON;
	m_FallSpeed = 0.0f;
}

CFunc_Dust::~CFunc_Dust()
{
}

void CFunc_Dust::Spawn_Impl()
{
	Precache();

	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );
	//AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_VOLUME_CONTENTS );

	// Clamp to values in a networkable range... can't up the networked bits without breaking demos. 
	const int unMaxLifetimeVal = (1 << (DUST_LIFETIME_NETWORK_BITS) ) - 1;
	m_LifetimeMin = Clamp( m_LifetimeMin.Get(), 0, unMaxLifetimeVal );
	m_LifetimeMax = Clamp( m_LifetimeMax.Get(), 0, unMaxLifetimeVal );

	//Since keyvalues can arrive in any order, and UTIL_StringToColor32 stomps alpha,
	//install the alpha value here.
	color32 clr = { m_Color.m_Value.r, m_Color.m_Value.g, m_Color.m_Value.b, (byte)m_iAlpha };
	m_Color.Set( clr );

	BaseClass::Spawn_Impl();
}

void CFunc_Dust::Precache_Impl()
{
	PrecacheMaterial( "particle/sparkles" );
}

void CFunc_Dust::Activate_Impl()
{
	BaseClass::Activate_Impl();
}

bool CFunc_Dust::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if( stricmp( szKeyName, "StartDisabled" ) == 0 )
	{
		if( szValue[0] == '1' )
			m_DustFlags &= ~DUSTFLAGS_ON;
		else
			m_DustFlags |= DUSTFLAGS_ON;
	
		return true;
	}
	else if( stricmp( szKeyName, "Alpha" ) == 0 )
	{
		m_iAlpha = std::atoi( szValue );
		return true;
	}
	else if( stricmp( szKeyName, "Frozen" ) == 0 )
	{
		if( szValue[0] == '1' )
			m_DustFlags |= DUSTFLAGS_FROZEN;
		else
			m_DustFlags &= ~DUSTFLAGS_FROZEN;
	
		return true;
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}
}

void CFunc_Dust::InputTurnOn( inputdata_t &inputdata )
{
	if( !(m_DustFlags & DUSTFLAGS_ON) )
	{
		m_DustFlags |= DUSTFLAGS_ON;
	}
}

void CFunc_Dust::InputTurnOff( inputdata_t &inputdata )
{
	if( m_DustFlags & DUSTFLAGS_ON )
	{
		m_DustFlags &= ~DUSTFLAGS_ON;
	}
}

//
// Dust
//

class CTEDust : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEDust" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEDust, CTEParticleSystem );
	DECLARE_SERVERCLASS();

					CTEDust( const char *name );
	virtual			~CTEDust( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles ) { };
	
	CNetworkVar( float, m_flSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkVar( float, m_flSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	CNetworkVector( m_vecDirection, [[= ks::reflect::Net{ .bits = 4, .low = -1.0f, .high = 1.0f, .enc = ks::reflect::WireEnc::Vector } ]] );
};

CTEDust::CTEDust( const char *name ) : BaseClass( name )
{
	m_flSize = 1.0f;
	m_flSpeed = 1.0f;
	m_vecDirection.Init();
}

CTEDust::~CTEDust( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEDust, DT_TEDust )

static CTEDust g_TEDust( "Dust" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&angles - 
//-----------------------------------------------------------------------------
void TE_Dust( IRecipientFilter& filter, float delay,
	const Vector &pos, const Vector &dir, float size, float speed )
{
	g_TEDust.m_vecOrigin	= pos;
	g_TEDust.m_vecDirection	= dir;
	g_TEDust.m_flSize		= size;
	g_TEDust.m_flSpeed		= speed;

	contract_assert( dir.Length() < 1.01 );	// make sure it's a normal

	//Send it
	g_TEDust.Create( filter, delay );
}

class CEnvDustPuff : public CPointEntity
{
	DECLARE_CLASS( CEnvDustPuff, CPointEntity );

public:
	
	DECLARE_DATADESC();

protected:

	// Input handlers
	[[= ks::reflect::Input{ .name = "SpawnDust", .type = FIELD_VOID } ]] void InputSpawnDust( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "scale" } ]] float		m_flScale;
	[[= ks::reflect::Key{ .name = "color" } ]] color32		m_rgbaColor;
};

LINK_ENTITY_TO_CLASS( env_dustpuff, CEnvDustPuff );

IMPLEMENT_REFLECT_DATAMAP( CEnvDustPuff )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvDustPuff::InputSpawnDust( inputdata_t &inputdata )
{
	Vector dir;
	AngleVectors( GetAbsAngles(), &dir );

	VectorNormalize( dir );

	g_pEffects->Dust( GetAbsOrigin(), dir, m_flScale, m_flSpeed );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CFuncInstanceIoProxy : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncInstanceIoProxy, CBaseEntity );

	// Input handlers
	[[= ks::reflect::Input{ .name = "OnProxyRelay1", .type = FIELD_STRING } ]] void InputProxyRelay1( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay2", .type = FIELD_STRING } ]] void InputProxyRelay2( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay3", .type = FIELD_STRING } ]] void InputProxyRelay3( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay4", .type = FIELD_STRING } ]] void InputProxyRelay4( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay5", .type = FIELD_STRING } ]] void InputProxyRelay5( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay6", .type = FIELD_STRING } ]] void InputProxyRelay6( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay7", .type = FIELD_STRING } ]] void InputProxyRelay7( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay8", .type = FIELD_STRING } ]] void InputProxyRelay8( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay9", .type = FIELD_STRING } ]] void InputProxyRelay9( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay10", .type = FIELD_STRING } ]] void InputProxyRelay10( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay11", .type = FIELD_STRING } ]] void InputProxyRelay11( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay12", .type = FIELD_STRING } ]] void InputProxyRelay12( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay13", .type = FIELD_STRING } ]] void InputProxyRelay13( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay14", .type = FIELD_STRING } ]] void InputProxyRelay14( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay15", .type = FIELD_STRING } ]] void InputProxyRelay15( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay16", .type = FIELD_STRING } ]] void InputProxyRelay16( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay17", .type = FIELD_STRING } ]] void InputProxyRelay17( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay18", .type = FIELD_STRING } ]] void InputProxyRelay18( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay19", .type = FIELD_STRING } ]] void InputProxyRelay19( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay20", .type = FIELD_STRING } ]] void InputProxyRelay20( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay21", .type = FIELD_STRING } ]] void InputProxyRelay21( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay22", .type = FIELD_STRING } ]] void InputProxyRelay22( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay23", .type = FIELD_STRING } ]] void InputProxyRelay23( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay24", .type = FIELD_STRING } ]] void InputProxyRelay24( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay25", .type = FIELD_STRING } ]] void InputProxyRelay25( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay26", .type = FIELD_STRING } ]] void InputProxyRelay26( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay27", .type = FIELD_STRING } ]] void InputProxyRelay27( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay28", .type = FIELD_STRING } ]] void InputProxyRelay28( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay29", .type = FIELD_STRING } ]] void InputProxyRelay29( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "OnProxyRelay30", .type = FIELD_STRING } ]] void InputProxyRelay30( inputdata_t &inputdata );
;

	DECLARE_DATADESC();

private:

	[[= ks::reflect::Key{ .name = "OnProxyRelay1" } ]] COutputEvent m_OnProxyRelay1;
	[[= ks::reflect::Key{ .name = "OnProxyRelay2" } ]] COutputEvent m_OnProxyRelay2;
	[[= ks::reflect::Key{ .name = "OnProxyRelay3" } ]] COutputEvent m_OnProxyRelay3;
	[[= ks::reflect::Key{ .name = "OnProxyRelay4" } ]] COutputEvent m_OnProxyRelay4;
	[[= ks::reflect::Key{ .name = "OnProxyRelay5" } ]] COutputEvent m_OnProxyRelay5;
	[[= ks::reflect::Key{ .name = "OnProxyRelay6" } ]] COutputEvent m_OnProxyRelay6;
	[[= ks::reflect::Key{ .name = "OnProxyRelay7" } ]] COutputEvent m_OnProxyRelay7;
	[[= ks::reflect::Key{ .name = "OnProxyRelay8" } ]] COutputEvent m_OnProxyRelay8;
	[[= ks::reflect::Key{ .name = "OnProxyRelay9" } ]] COutputEvent m_OnProxyRelay9;
	[[= ks::reflect::Key{ .name = "OnProxyRelay10" } ]] COutputEvent m_OnProxyRelay10;
	[[= ks::reflect::Key{ .name = "OnProxyRelay11" } ]] COutputEvent m_OnProxyRelay11;
	[[= ks::reflect::Key{ .name = "OnProxyRelay12" } ]] COutputEvent m_OnProxyRelay12;
	[[= ks::reflect::Key{ .name = "OnProxyRelay13" } ]] COutputEvent m_OnProxyRelay13;
	[[= ks::reflect::Key{ .name = "OnProxyRelay14" } ]] COutputEvent m_OnProxyRelay14;
	[[= ks::reflect::Key{ .name = "OnProxyRelay15" } ]] COutputEvent m_OnProxyRelay15;
	[[= ks::reflect::Key{ .name = "OnProxyRelay16" } ]] COutputEvent m_OnProxyRelay16;
	[[= ks::reflect::Key{ .name = "OnProxyRelay17" } ]] COutputEvent m_OnProxyRelay17;
	[[= ks::reflect::Key{ .name = "OnProxyRelay18" } ]] COutputEvent m_OnProxyRelay18;
	[[= ks::reflect::Key{ .name = "OnProxyRelay19" } ]] COutputEvent m_OnProxyRelay19;
	[[= ks::reflect::Key{ .name = "OnProxyRelay20" } ]] COutputEvent m_OnProxyRelay20;
	[[= ks::reflect::Key{ .name = "OnProxyRelay21" } ]] COutputEvent m_OnProxyRelay21;
	[[= ks::reflect::Key{ .name = "OnProxyRelay22" } ]] COutputEvent m_OnProxyRelay22;
	[[= ks::reflect::Key{ .name = "OnProxyRelay23" } ]] COutputEvent m_OnProxyRelay23;
	[[= ks::reflect::Key{ .name = "OnProxyRelay24" } ]] COutputEvent m_OnProxyRelay24;
	[[= ks::reflect::Key{ .name = "OnProxyRelay25" } ]] COutputEvent m_OnProxyRelay25;
	[[= ks::reflect::Key{ .name = "OnProxyRelay26" } ]] COutputEvent m_OnProxyRelay26;
	[[= ks::reflect::Key{ .name = "OnProxyRelay27" } ]] COutputEvent m_OnProxyRelay27;
	[[= ks::reflect::Key{ .name = "OnProxyRelay28" } ]] COutputEvent m_OnProxyRelay28;
	[[= ks::reflect::Key{ .name = "OnProxyRelay29" } ]] COutputEvent m_OnProxyRelay29;
	[[= ks::reflect::Key{ .name = "OnProxyRelay30" } ]] COutputEvent m_OnProxyRelay30;

};

LINK_ENTITY_TO_CLASS( func_instance_io_proxy, CFuncInstanceIoProxy );

IMPLEMENT_REFLECT_DATAMAP( CFuncInstanceIoProxy )

//------------------------------------------------------------------------------
// Purpose : Route the incomming to the outgoing proxy messages. 
//------------------------------------------------------------------------------
void CFuncInstanceIoProxy::InputProxyRelay1( inputdata_t &inputdata )
{
	m_OnProxyRelay1.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay2( inputdata_t &inputdata )
{
	m_OnProxyRelay2.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay3( inputdata_t &inputdata )
{
	m_OnProxyRelay3.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay4( inputdata_t &inputdata )
{
	m_OnProxyRelay4.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay5( inputdata_t &inputdata )
{
	m_OnProxyRelay5.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay6( inputdata_t &inputdata )
{
	m_OnProxyRelay6.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay7( inputdata_t &inputdata )
{
	m_OnProxyRelay7.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay8( inputdata_t &inputdata )
{
	m_OnProxyRelay8.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay9( inputdata_t &inputdata )
{
	m_OnProxyRelay9.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay10( inputdata_t &inputdata )
{
	m_OnProxyRelay10.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay11( inputdata_t &inputdata )
{
	m_OnProxyRelay11.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay12( inputdata_t &inputdata )
{
	m_OnProxyRelay12.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay13( inputdata_t &inputdata )
{
	m_OnProxyRelay13.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay14( inputdata_t &inputdata )
{
	m_OnProxyRelay14.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay15( inputdata_t &inputdata )
{
	m_OnProxyRelay15.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay16( inputdata_t &inputdata )
{
	m_OnProxyRelay16.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay17( inputdata_t &inputdata )
{
	m_OnProxyRelay17.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay18( inputdata_t &inputdata )
{
	m_OnProxyRelay18.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay19( inputdata_t &inputdata )
{
	m_OnProxyRelay19.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay20( inputdata_t &inputdata )
{
	m_OnProxyRelay20.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay21( inputdata_t &inputdata )
{
	m_OnProxyRelay21.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay22( inputdata_t &inputdata )
{
	m_OnProxyRelay22.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay23( inputdata_t &inputdata )
{
	m_OnProxyRelay23.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay24( inputdata_t &inputdata )
{
	m_OnProxyRelay24.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay25( inputdata_t &inputdata )
{
	m_OnProxyRelay25.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay26( inputdata_t &inputdata )
{
	m_OnProxyRelay26.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay27( inputdata_t &inputdata )
{
	m_OnProxyRelay27.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay28( inputdata_t &inputdata )
{
	m_OnProxyRelay28.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay29( inputdata_t &inputdata )
{
	m_OnProxyRelay29.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay30( inputdata_t &inputdata )
{
	m_OnProxyRelay30.FireOutput( inputdata.pActivator, inputdata.pCaller );
	DevWarning( "Maximun Proxy Messages used - ask a programmer for more.\n" );
}

class CBaseEntity;
class CBasePlayer;
class CFuncLadder;
struct inputdata_t;

using namespace ::ks::reflect;

ConVar sv_showladders( side_registry(), "sv_showladders", "0", 0, "Show bbox and dismount points for all ladders (must be set before level load.)\n" );

std::vector< CFuncLadder * >	CFuncLadder::s_Ladders;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncLadder::CFuncLadder() :
	m_bDisabled( false )
{
	s_Ladders.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncLadder::~CFuncLadder()
{
	ks::erase_first( s_Ladders, this );
}

int CFuncLadder::GetLadderCount()
{
	return std::ssize( s_Ladders );
}

CFuncLadder *CFuncLadder::GetLadder( int index )
{
	if ( index < 0 || index >= std::ssize( s_Ladders ) )
		return nullptr;

	return s_Ladders[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

	// Entity is symbolid
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	
	//AddFlag( FL_WORLDBRUSH );
	SetModelName( NULL_STRING );

	// Make entity invisible
	AddEffects( EF_NODRAW );
	// No model but should still network
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs = VEC_HULL_MAX;

	// This will swap them if they are inverted
	SetEndPoints( m_vecPlayerMountPositionTop, m_vecPlayerMountPositionBottom );

	trace_t bottomtrace, toptrace;
	UTIL_TraceHull( m_vecPlayerMountPositionBottom, m_vecPlayerMountPositionBottom, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &bottomtrace );
	UTIL_TraceHull( m_vecPlayerMountPositionTop, m_vecPlayerMountPositionTop, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &toptrace );

	if ( bottomtrace.startsolid || toptrace.startsolid )
	{
		if ( bottomtrace.startsolid )
		{
			DevMsg( 1, "Warning, funcladder with blocked bottom point (%.2f %.2f %.2f) stuck in (%s)\n",
				m_vecPlayerMountPositionBottom.GetX(),
				m_vecPlayerMountPositionBottom.GetY(),
				m_vecPlayerMountPositionBottom.GetZ(),
				bottomtrace.Ent<CBaseEntity>() 
					? 
					UTIL_VarArgs( "%s/%s", bottomtrace.Ent<CBaseEntity>()->GetClassname(), bottomtrace.Ent<CBaseEntity>()->GetEntityName().ToCStr() ) 
					: 
					"NULL" );
		}
		if ( toptrace.startsolid )
		{
			DevMsg( 1, "Warning, funcladder with blocked top point (%.2f %.2f %.2f) stuck in (%s)\n",
				m_vecPlayerMountPositionTop.GetX(),
				m_vecPlayerMountPositionTop.GetY(),
				m_vecPlayerMountPositionTop.GetZ(),
				toptrace.Ent<CBaseEntity>() 
					? 
					UTIL_VarArgs( "%s/%s", toptrace.Ent<CBaseEntity>()->GetClassname(), toptrace.Ent<CBaseEntity>()->GetEntityName().ToCStr() ) 
					: 
					"NULL" );
		}

		// Force geometry overlays on, but only if developer 2 is set...
		if ( developer.GetInt() > 1 )
		{
			m_debugOverlays |= OVERLAY_TEXT_BIT;
		}
	}

	m_vecPlayerMountPositionTop -= GetAbsOrigin();
	m_vecPlayerMountPositionBottom -= GetAbsOrigin();

	// Compute mins, maxs of points
	// 
	Vector mins( MAX_COORD_INTEGER, MAX_COORD_INTEGER, MAX_COORD_INTEGER );
	Vector maxs( -MAX_COORD_INTEGER, -MAX_COORD_INTEGER, -MAX_COORD_INTEGER );
	int i;
	for ( i = 0; i < 3; i++ )
	{
		if ( m_vecPlayerMountPositionBottom.m_Value[ i ] < mins[ i ] )
		{
			mins[ i ] = m_vecPlayerMountPositionBottom.m_Value[ i ];
		}
		if ( m_vecPlayerMountPositionBottom.m_Value[ i ] > maxs[ i ] )
		{
			maxs[ i ] = m_vecPlayerMountPositionBottom.m_Value[ i ];
		}
		if ( m_vecPlayerMountPositionTop.m_Value[ i ] < mins[ i ] )
		{
			mins[ i ] = m_vecPlayerMountPositionTop.m_Value[ i ];
		}
		if ( m_vecPlayerMountPositionTop.m_Value[ i ] > maxs[ i ] )
		{
			maxs[ i ] = m_vecPlayerMountPositionTop.m_Value[ i ];
		}
	}

	// Expand mins/maxs by player hull size
	mins += playerMins;
	maxs += playerMaxs;

	UTIL_SetSize( this, mins, maxs );

	m_bFakeLadder = HasSpawnFlags(SF_LADDER_DONTGETON);
}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned or after reload from .sav file
//-----------------------------------------------------------------------------
void CFuncLadder::Activate_Impl()
{
	// Chain to base class
	BaseClass::Activate_Impl();

	// Re-hook up ladder dismount points
	SearchForDismountPoints();

	// Show debugging UI if it's active
	if ( sv_showladders.GetBool() )
	{
		m_debugOverlays |= OVERLAY_TEXT_BIT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::SearchForDismountPoints()
{
	std::vector< CInfoLadderDismountHandle > allNodes;

	Vector topPos;
	Vector bottomPos;

	GetTopPosition( topPos );
	GetBottomPosition( bottomPos );

	float dismount_radius = 100.0f;

	Vector vecBottomToTop = topPos - bottomPos;
	float ladderLength = VectorNormalize( vecBottomToTop );

	float recheck = 40.0f;

	// add both sets of nodes
	FindNearbyDismountPoints( topPos, dismount_radius, m_Dismounts );
	FindNearbyDismountPoints( bottomPos, dismount_radius, m_Dismounts );

	while ( 1 )
	{
		ladderLength -= recheck;
		if ( ladderLength <= 0.0f )
			break;
		bottomPos += recheck * vecBottomToTop;
		FindNearbyDismountPoints( bottomPos, dismount_radius, m_Dismounts );
	}
}

void CFuncLadder::SetEndPoints( const Vector& p1, const Vector& p2 )
{
	m_vecPlayerMountPositionTop = p1;
	m_vecPlayerMountPositionBottom = p2;

	if ( m_vecPlayerMountPositionBottom.GetZ() > m_vecPlayerMountPositionTop.GetZ() )
	{
		Vector temp = m_vecPlayerMountPositionBottom;
		m_vecPlayerMountPositionBottom = m_vecPlayerMountPositionTop;
		m_vecPlayerMountPositionTop = temp;
	}

	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs = VEC_HULL_MAX;

	trace_t result;
	UTIL_TraceHull( m_vecPlayerMountPositionTop + Vector( 0, 0, 4 ), m_vecPlayerMountPositionTop, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &result );

	if ( !result.startsolid )
	{
		m_vecPlayerMountPositionTop = result.endpos;
	}

	UTIL_TraceHull( m_vecPlayerMountPositionBottom + Vector( 0, 0, 4 ), m_vecPlayerMountPositionBottom, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &result );

	if ( !result.startsolid )
	{
		m_vecPlayerMountPositionBottom = result.endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncLadder::DrawDebugGeometryOverlays_Impl()
{

	BaseClass::DrawDebugGeometryOverlays_Impl();

	Vector playerMins = VEC_HULL_MIN;
	Vector playerMaxs  = VEC_HULL_MAX;

	Vector topPosition;
	Vector bottomPosition;

	GetTopPosition( topPosition );
	GetBottomPosition( bottomPosition );

	NDebugOverlay::Box( topPosition, playerMins, playerMaxs, 255,0,0,127, 0 );
	NDebugOverlay::Box( bottomPosition, playerMins, playerMaxs, 0,0,255,127, 0 );

	NDebugOverlay::EntityBounds(this, 200, 180, 63, 63, 0);

	trace_t bottomtrace;
	UTIL_TraceHull( m_vecPlayerMountPositionBottom, m_vecPlayerMountPositionBottom, 
		playerMins, playerMaxs, MASK_PLAYERSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &bottomtrace );

	int c = std::ssize( m_Dismounts );
	for ( int i = 0 ; i < c ; i++ )
	{
		CInfoLadderDismount *pt = m_Dismounts[ i ];
		if ( !pt )
			continue;

		NDebugOverlay::Box(pt->GetAbsOrigin(),Vector( -16, -16, 0 ), Vector( 16, 16, 8 ), 150,0,0, 63, 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CFuncLadder::GetTopPosition( Vector& org )
{
	ComputeAbsPosition( m_vecPlayerMountPositionTop + GetLocalOrigin(), &org );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CFuncLadder::GetBottomPosition( Vector& org )
{
	ComputeAbsPosition( m_vecPlayerMountPositionBottom + GetLocalOrigin(), &org );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bottomToTopVec - 
//-----------------------------------------------------------------------------
void CFuncLadder::ComputeLadderDir( Vector& bottomToTopVec )
{
	Vector top;
	Vector bottom;

	GetTopPosition( top );
	GetBottomPosition( bottom );

	bottomToTopVec = top - bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CFuncLadder::GetDismountCount() const
{
	return std::ssize( m_Dismounts );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : CInfoLadderDismountHandle
//-----------------------------------------------------------------------------
CInfoLadderDismount *CFuncLadder::GetDismount( int index )
{
	if ( index < 0 || index >= std::ssize( m_Dismounts ) )
		return nullptr;
	return m_Dismounts[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			radius - 
//			list - 
//-----------------------------------------------------------------------------
void CFuncLadder::FindNearbyDismountPoints( const Vector& origin, float radius, std::vector< CInfoLadderDismountHandle >& list )
{
	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = gEntList.FindEntityByClassnameWithin( pEntity, "info_ladder_dismount", origin, radius)) != nullptr )
	{
		CInfoLadderDismount *landingspot = static_cast< CInfoLadderDismount * >( pEntity );
		contract_assert( landingspot );

		// If spot has a target, then if the target is not this ladder, don't add to our list.
		if ( landingspot->m_target != NULL_STRING )
		{
			if ( landingspot->GetNextTarget() != this )
			{
				continue;
			}
		}

		CInfoLadderDismountHandle handle;
		handle = landingspot;
		if ( ks::find_index( list, handle ) == -1 )
		{
			list.push_back( handle );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncLadder::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncLadder::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CFuncLadder::PlayerGotOn( CBasePlayer *pPlayer )
{
	m_OnPlayerGotOnLadder.FireOutput(this, pPlayer);
	pPlayer->EmitSound( "Ladder.StepRight" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CFuncLadder::PlayerGotOff( CBasePlayer *pPlayer )
{
	m_OnPlayerGotOffLadder.FireOutput(this, pPlayer);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFuncLadder::DontGetOnLadder( void ) const
{
	return m_bFakeLadder;
}

const char *CFuncLadder::GetSurfacePropName()
{
	if ( !m_surfacePropName )
		return nullptr;
	return m_surfacePropName.ToCStr();
}

IMPLEMENT_NETWORKCLASS_ALIASED( FuncLadder, DT_FuncLadder );

IMPLEMENT_REFLECT_TABLE( CFuncLadder, DT_FuncLadder );

LINK_ENTITY_TO_CLASS_ALIASED( func_useableladder, FuncLadder );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncLadder )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoLadderDismount::DrawDebugGeometryOverlays_Impl()
{
	BaseClass::DrawDebugGeometryOverlays_Impl();

	if ( developer.GetBool() )
	{
		NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, 0 ), Vector( 16, 16, 8 ), 127, 127, 127, 127, 0 );
	}
}

int CFuncLadder::UpdateTransmitState_Impl()
{
	// transmit if in PVS for clientside prediction
	return SetTransmitState( FL_EDICT_PVSCHECK );
}

IMPLEMENT_NETWORKCLASS_ALIASED( InfoLadderDismount, DT_InfoLadderDismount );

IMPLEMENT_REFLECT_TABLE( CInfoLadderDismount, DT_InfoLadderDismount );

LINK_ENTITY_TO_CLASS_ALIASED( info_ladder_dismount, InfoLadderDismount );

const char *FuncLadder_GetSurfaceprops(CBaseEntity *pLadderEntity)
{
	CFuncLadder *pLadder = dynamic_cast<CFuncLadder *>(pLadderEntity);
	if ( pLadder )
	{
		if ( pLadder->GetSurfacePropName() )
			return pLadder->GetSurfacePropName();
	}
	return "ladder";
}

bool CFuncLadder::IsEnabled() const
{
	return !m_bDisabled;
}

class CBaseEntity;
class CFuncLadder;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: A transient entity used to construct a true CFuncLadder
// FIXME:  THIS ENTITY IS OBSOLETE NOW, SHOULD BE REMOVED FROM HERE AND .FGD AT SOME POINT!!!
//-----------------------------------------------------------------------------
class CFuncLadderEndPoint : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncLadderEndPoint, CBaseEntity );

	void Activate_Impl();

private:
	bool		Validate();
};

LINK_ENTITY_TO_CLASS( func_ladderendpoint, CFuncLadderEndPoint );

void CFuncLadderEndPoint::Activate_Impl()
{
	BaseClass::Activate_Impl();

	if ( IsMarkedForDeletion() )
		return;

	Validate();
}

bool CFuncLadderEndPoint::Validate()
{
	// Find the the other end
	Vector startPos = GetAbsOrigin();
	
	CFuncLadderEndPoint *other = dynamic_cast< CFuncLadderEndPoint * >( GetNextTarget() );
	if ( !other )
	{
		DevMsg( 1, "func_ladderendpoint(%s) without matching target\n", GetEntityName().ToCStr() );
		return false;
	}

	Vector endPos = other->GetAbsOrigin();

	CFuncLadder *ladder = ( CFuncLadder * )CreateEntityByName( "func_useableladder" );
	if ( ladder )
	{
		ladder->SetEndPoints( startPos, endPos );
		ladder->SetAbsOrigin( GetAbsOrigin() );
		ladder->SetParent( GetParent() );
		ladder->SetName( GetEntityName() );
		ladder->Spawn();
	}

	// Delete both endpoints
	UTIL_Remove( other );
	UTIL_Remove( this );

	return true;
}

class CBaseEntity;

using namespace ::ks::reflect;

class CFunc_LOD : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Func_LOD" } ]]
	static constexpr bool ks_class = true;

	DECLARE_DATADESC();
	DECLARE_CLASS( CFunc_LOD, CBaseEntity );
public:
	DECLARE_SERVERCLASS();

					CFunc_LOD();
	virtual 		~CFunc_LOD();

	// When the viewer is between:
	// (0 and m_fNonintrusiveDist):					the bmodel is forced to be visible
	// (m_fNonintrusiveDist and m_fDisappearDist):	the bmodel is trying to appear or disappear nonintrusively
	//												(waits until it's out of the view frustrum or until there's a lot of motion)
	// (m_fDisappearDist+):							the bmodel is forced to be invisible
	CNetworkVar( int, m_nDisappearMinDist, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "DisappearMinDist" } ]] );
	CNetworkVar( int, m_nDisappearMaxDist, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "DisappearMaxDist" } ]] );

// CBaseEntity overrides.
public:

	void	Spawn_Impl();
	bool			CreateVPhysics_Impl();
	void	Activate_Impl();
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );
};

IMPLEMENT_REFLECT_SERVERCLASS( CFunc_LOD, DT_Func_LOD )

LINK_ENTITY_TO_CLASS(func_lod, CFunc_LOD);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFunc_LOD )

// ------------------------------------------------------------------------------------- //
// CFunc_LOD implementation.
// ------------------------------------------------------------------------------------- //
CFunc_LOD::CFunc_LOD()
{
}

CFunc_LOD::~CFunc_LOD()
{
}

void CFunc_LOD::Spawn_Impl()
{
	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );
	SetSolid( SOLID_BSP );
	BaseClass::Spawn_Impl();

	CreateVPhysics();
}

bool CFunc_LOD::CreateVPhysics_Impl()
{
	VPhysicsInitStatic();
	return true;
}

void CFunc_LOD::Activate_Impl()
{
	BaseClass::Activate_Impl();
}

bool CFunc_LOD::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	// NOTE: Backward compat
	if ( FStrEq(szKeyName, "DisappearDist") )
	{
		m_nDisappearMinDist = std::atoi(szValue);
		m_nDisappearMaxDist = m_nDisappearMinDist + 800;
	}
	else if (FStrEq(szKeyName, "Solid"))
	{
		if (std::atoi(szValue) != 0)
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
		}
	}
	else
	{
		return BaseClass::KeyValue_Impl(szKeyName, szValue);
	}

	return true;
}

KS_ENTITY_ABSTRACT_KIND( CFuncAreaPortalBase )

KS_ENTITY_ABSTRACT_KIND( CFunc_Dust )

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements a brush model entity that moves along a linear path.
//			Water whose level can be changed is implemented using the same entity.

// -------------------------------
//  SPAWN_FLAGS
// -------------------------------
static constexpr auto SF_MOVELINEAR_NOTSOLID = 8;

LINK_ENTITY_TO_CLASS( func_movelinear, CFuncMoveLinear );
LINK_ENTITY_TO_CLASS_ALIAS( momentary_door, CFuncMoveLinear );	// For backward compatibility

//
// func_water_analog is implemented as a linear mover so we can raise/lower the water level.
//
LINK_ENTITY_TO_CLASS_ALIAS( func_water_analog, CFuncMoveLinear );

IMPLEMENT_REFLECT_DATAMAP( CFuncMoveLinear )

void SendProxy_CropFlagsToConveyorFlagBitsLength( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID)
{
	int mask = (1<<13) - 1;
	int data = *(int *)pVarData;

	pOut->m_Int = ( data & mask );
}

IMPLEMENT_REFLECT_SERVERCLASS( CFuncMoveLinear, DT_FuncMoveLinear )

//------------------------------------------------------------------------------
// Purpose: Called before spawning, after keyvalues have been parsed.
//------------------------------------------------------------------------------
void CFuncMoveLinear::Spawn_Impl( void )
{
	// Convert movedir from angles to a vector
	QAngle angMoveDir = QAngle( m_vecMoveDir.x, m_vecMoveDir.y, m_vecMoveDir.z );
	AngleVectors( angMoveDir, &m_vecMoveDir );

	SetMoveType( MOVETYPE_PUSH );
	SetModel( STRING( GetModelName() ) );
	
	// Don't allow zero or negative speeds
	if (m_flSpeed <= 0)
	{
		m_flSpeed = 100;
	}
	
	// If move distance is set to zero, use with width of the 
	// brush to determine the size of the move distance
	if (m_flMoveDistance <= 0)
	{
		Vector vecOBB = CollisionProp()->OBBSize();
		vecOBB -= Vector( 2, 2, 2 );
		m_flMoveDistance = DotProductAbs( m_vecMoveDir, vecOBB ) - m_flLip;
	}

	m_vecPosition1 = GetLocalOrigin() - (m_vecMoveDir * m_flMoveDistance * m_flStartPosition);
	m_vecPosition2 = m_vecPosition1 + (m_vecMoveDir * m_flMoveDistance);
	m_vecFinalDest = GetLocalOrigin();

	SetTouch( nullptr );

	Precache();

	// Disabling this flag, since it seems to just make the player move twice as fast when on one of these. -Brett
	//AddFlag( FL_CONVEYOR );

	// It is solid?
	SetSolid( SOLID_VPHYSICS );

	if ( FClassnameIs( this, "func_water_analog" ) )
	{
		AddSolidFlags( FSOLID_VOLUME_CONTENTS );
	}

	if ( !FClassnameIs( this, "func_water_analog" ) && FBitSet (m_spawnflags, SF_MOVELINEAR_NOTSOLID) )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	CreateVPhysics();
}

bool CFuncMoveLinear::ShouldSavePhysics_Impl( void )
{
	// don't save physics for func_water_analog, regen
	return !FClassnameIs( this, "func_water_analog" );
		
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFuncMoveLinear::CreateVPhysics_Impl( void )
{
	if ( !FClassnameIs( this, "func_water_analog" ) )
	{
		//normal door
		if ( !IsSolidFlagSet( FSOLID_NOT_SOLID ) )
		{
			VPhysicsInitShadow( false, false );
		}
	}
	else
	{
		// special contents
		AddSolidFlags( FSOLID_VOLUME_CONTENTS );
		//SETBITS( m_spawnflags, SF_DOOR_SILENT );	// water is silent for now

		IPhysicsObject *pPhysics = VPhysicsInitShadow( false, false );
		fluidparams_t fluid;
		
		contract_assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
		fluid.damping = 0.01f;
		fluid.surfacePlane[0] = 0;
		fluid.surfacePlane[1] = 0;
		fluid.surfacePlane[2] = 1;
		fluid.surfacePlane[3] = CollisionProp()->GetCollisionOrigin().z + CollisionProp()->OBBMaxs().z - 1;
		fluid.currentVelocity.Init(0,0,0);
		fluid.torqueFactor = 0.1f;
		fluid.viscosityFactor = 0.01f;
		fluid.pGameData = static_cast<void *>(this);
		
		//FIXME: Currently there's no way to specify that you want slime
		fluid.contents = CONTENTS_WATER;
		
		m_pFluidController = physenv->CreateFluidController( pPhysics, &fluid );
	}

	return true;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::Precache_Impl( void )
{
	if (m_soundStart != NULL_STRING)
	{
		PrecacheScriptSound( (char *) STRING(m_soundStart) );
	}
	if (m_soundStop != NULL_STRING)
	{
		PrecacheScriptSound( (char *) STRING(m_soundStop) );
	}
	m_currentSound = NULL_STRING;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::MoveTo(Vector vPosition, float flSpeed)
{
	if ( flSpeed != 0 )
	{
		if ( m_soundStart != NULL_STRING )
		{
			if (m_currentSound == m_soundStart)
			{
				StopSound(entindex(), CHAN_BODY, (char*)STRING(m_soundStop));
			}
			else
			{
				m_currentSound = m_soundStart;
				CPASAttenuationFilter filter( this );

				EmitSound_t ep;
				ep.m_nChannel = CHAN_BODY;
				ep.m_pSoundName = (char*)STRING(m_soundStart);
				ep.m_flVolume = 1;
				ep.m_SoundLevel = SNDLVL_NORM;

				EmitSound( filter, entindex(), ep );	
			}
		}

		LinearMove( vPosition, flSpeed );

		if ( m_pFluidController )
		{
			m_pFluidController->WakeAllSleepingObjects();
		}

		// Clear think (that stops sounds)
		SetThink(nullptr);
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::StopMoveSound( void )
{
	if ( m_soundStart != NULL_STRING && ( m_currentSound == m_soundStart ) )
	{
		StopSound(entindex(), CHAN_BODY, (char*)STRING(m_soundStart) );
	}

	if ( m_soundStop != NULL_STRING && ( m_currentSound != m_soundStop ) )
	{
		m_currentSound = m_soundStop;
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_soundStop);
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}

	SetThink(nullptr);
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::MoveDone_Impl( void )
{
	// Stop sounds at the next think, rather than here as another
	// SetPosition call might immediately follow the end of this move
	SetThink(&CFuncMoveLinear::StopMoveSound);
	SetNextThink( gpGlobals->curtime + 0.1f );
	BaseClass::MoveDone_Impl();

	if ( GetLocalOrigin() == m_vecPosition2 )
	{
		m_OnFullyOpen.FireOutput( this, this );
	}
	else if ( GetLocalOrigin() == m_vecPosition1 )
	{
		m_OnFullyClosed.FireOutput( this, this );
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )		// Momentary buttons will pass down a float in here
		return;

	if ( value > 1.0 )
		value = 1.0;
	Vector move = m_vecPosition1 + (value * (m_vecPosition2 - m_vecPosition1));
	
	Vector delta = move - GetLocalOrigin();
	float speed = delta.Length() * 10;

	MoveTo(move, speed);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the position as a value from [0..1].
//-----------------------------------------------------------------------------
void CFuncMoveLinear::SetPosition( float flPosition )
{
	Vector vTargetPos = m_vecPosition1 + ( flPosition * (m_vecPosition2 - m_vecPosition1));
	if ((vTargetPos - GetLocalOrigin()).Length() > 0.001)
	{
		MoveTo(vTargetPos, m_flSpeed);
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::InputOpen( inputdata_t &inputdata )
{
	if (GetLocalOrigin() != m_vecPosition2)
	{
		MoveTo(m_vecPosition2, m_flSpeed);
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CFuncMoveLinear::InputClose( inputdata_t &inputdata )
{
	if (GetLocalOrigin() != m_vecPosition1)
	{
		MoveTo(m_vecPosition1, m_flSpeed);
	}
}

//------------------------------------------------------------------------------
// Purpose: Input handler for setting the position from [0..1].
// Input  : Float position.
//-----------------------------------------------------------------------------
void CFuncMoveLinear::InputSetPosition( inputdata_t &inputdata )
{
	SetPosition( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame when the bruch is blocked while moving
// Input  : pOther - The blocking entity.
//-----------------------------------------------------------------------------
void CFuncMoveLinear::Blocked_Impl( CBaseEntity *pOther )
{
	// Hurt the blocker 
	if ( m_flBlockDamage )
	{
		if ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY )
		{
			if ( FClassnameIs( pOther, "gib" ) )
				UTIL_Remove( pOther );
		}
		else
			pOther->TakeDamage( CTakeDamageInfo( this, this, m_flBlockDamage, DMG_CRUSH ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncMoveLinear::InputSetSpeed( inputdata_t &inputdata )
{
	// Set the new speed
	m_flSpeed = inputdata.value.Float();

	// FIXME: This is a little questionable.  Do we want to fix the speed, or let it continue on at the old speed?
	float flDistToGoalSqr = ( m_vecFinalDest - GetLocalOrigin() ).LengthSqr();
	if ( flDistToGoalSqr > Square( std::numeric_limits<float>::epsilon() ) )
	{
		if ( fabs( m_flSpeed ) > std::numeric_limits<float>::epsilon() )
		{
			// NOTE: We do NOT want to call sound functions here, just vanilla position changes
			LinearMove( m_vecFinalDest, m_flSpeed );
		}
		else
		{
			// If we set our speed to 0, just move to the current position to stop.
			m_flSpeed = 1.0f;
			LinearMove( GetLocalOrigin(), m_flSpeed );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFuncMoveLinear::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		float flTravelDist = (m_vecPosition1 - m_vecPosition2).Length();
		float flCurDist	   = (m_vecPosition1 - GetLocalOrigin()).Length();
		V_snprintf(tempstr,sizeof(tempstr),"Current Pos: %3.3f",flCurDist/flTravelDist);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		float flTargetDist	   = (m_vecPosition1 - m_vecFinalDest).Length();
		V_snprintf(tempstr,sizeof(tempstr),"Target Pos: %3.3f",flTargetDist/flTravelDist);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: area portal entity: toggles visibility areas on/off
// NOTE: These are not really brush entities.  They are brush entities from a
// designer/worldcraft perspective, but by the time they reach the game, the
// brush model is gone and this is, in effect, a point entity.

class CFuncOccluder : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncOccluder", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncOccluder, CBaseEntity );

					CFuncOccluder();

	void	Spawn_Impl( void );
	int		UpdateTransmitState_Impl( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputActivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputDeactivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

private:
	CNetworkVar( bool, m_bActive, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "StartActive" } ]] );
	CNetworkVar( int, m_nOccluderIndex, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "occludernumber" } ]] );
};

LINK_ENTITY_TO_CLASS( func_occluder, CFuncOccluder );

IMPLEMENT_REFLECT_SERVERCLASS( CFuncOccluder, DT_FuncOccluder )

IMPLEMENT_REFLECT_DATAMAP( CFuncOccluder )

//------------------------------------------------------------------------------
// Occluder :
//------------------------------------------------------------------------------
CFuncOccluder::CFuncOccluder()
{
	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncOccluder::Spawn_Impl( void )
{
    Precache( );    

	m_takedamage	= DAMAGE_NO;
	SetSolid( SOLID_NONE );
    SetMoveType( MOVETYPE_NONE );
	
	// set size and link into world.
	SetModel( STRING( GetModelName() ) );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CFuncOccluder::InputDeactivate( inputdata_t &inputdata )
{
	m_bActive = false;
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CFuncOccluder::InputActivate( inputdata_t &inputdata )
{
	m_bActive = true;
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CFuncOccluder::InputToggle( inputdata_t &inputdata )
{
	m_bActive = !m_bActive;
}

//------------------------------------------------------------------------------
// We always want to transmit these bad boys
//------------------------------------------------------------------------------
int CFuncOccluder::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

using namespace ::ks::reflect;

class CFuncReflectiveGlass : public CFuncBrush
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncReflectiveGlass" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CFuncReflectiveGlass, CFuncBrush );
	DECLARE_SERVERCLASS();
};

// automatically hooks in the system's callbacks

LINK_ENTITY_TO_CLASS( func_reflective_glass, CFuncReflectiveGlass );

IMPLEMENT_REFLECT_SERVERCLASS( CFuncReflectiveGlass, DT_FuncReflectiveGlass )

using namespace ::ks::reflect;

class CFuncSmokeVolume : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncSmokeVolume" } ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFuncSmokeVolume, CBaseParticleEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFuncSmokeVolume();
	void Spawn_Impl();
	void Activate_Impl( void );

	// Set the times it fades out at.
	void SetDensity( float density );

private:
	CNetworkVar( color32, m_Color1, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "Color1" } ]] );
	CNetworkVar( color32, m_Color2, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "Color2" } ]] );
	CNetworkString( m_MaterialName, 255, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "Material" } ]] string_t m_String_tMaterialName;
	CNetworkVar( float, m_ParticleDrawWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "ParticleDrawWidth" } ]] );
	CNetworkVar( float, m_ParticleSpacingDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "ParticleSpacingDistance" } ]] );
	CNetworkVar( float, m_DensityRampSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "DensityRampSpeed" } ]] );
	CNetworkVar( float, m_RotationSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "RotationSpeed" } ]] [[= ks::reflect::Key{ .name = "SetRotationSpeed", .input = true } ]] );
	CNetworkVar( float, m_MovementSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "MovementSpeed" } ]] [[= ks::reflect::Key{ .name = "SetMovementSpeed", .input = true } ]] );
	CNetworkVar( float, m_Density, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Density" } ]] [[= ks::reflect::Key{ .name = "SetDensity", .input = true } ]] );
	CNetworkVar( float, m_maxDrawDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "MaxDrawDistance" } ]] );
};

IMPLEMENT_REFLECT_DATAMAP( CFuncSmokeVolume )

IMPLEMENT_REFLECT_SERVERCLASS( CFuncSmokeVolume, DT_FuncSmokeVolume )

LINK_ENTITY_TO_CLASS( func_smokevolume, CFuncSmokeVolume );

CFuncSmokeVolume::CFuncSmokeVolume()
{
	m_Density = 1.0f;
	m_maxDrawDistance = 0.0f;
}

void CFuncSmokeVolume::SetDensity( float density )
{
	m_Density = density;
}

void CFuncSmokeVolume::Spawn_Impl()
{
	std::memset( m_MaterialName.GetForModify(), 0, sizeof( m_MaterialName ) );

	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );

	BaseClass::Spawn_Impl();
}

void CFuncSmokeVolume::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	V_strncpy( m_MaterialName.GetForModify(), STRING( m_String_tMaterialName ), 255 );
}
