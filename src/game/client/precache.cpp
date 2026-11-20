// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <bone_setup.h>
#include <vcollide_parse.h>
#include "datacache/iprecachesystem.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "edict.h"
#include "predictable_entity.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "con_nprint.h"
#include "engine/ivmodelinfo.h"
#include "iclientnetworkable.h"
#include "cdll_client_int.h"
#include "prediction_private.h"
#include "predictioncopy.h"
#include "shareddefs.h"
#include "datamap.h"
#include "globalvars_base.h"
#include "iprediction.h"
#include "iclientrenderable.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "model_types.h"
#include "networkvar.h"
#include "const.h"
#include "string_t.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "imaterialproxydict.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :props;
import :baseentity;
import :util;
import :particlemgr;
import :precache;
import :achievements_and_stats_interface;
import :iinput;
import :baseanimatedtextureproxy;
import :sharedvar;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interpolatedvar;
import ks.tier1.registry;
import :cstrike15.cs_player;
import :noopanimcontroller;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import ks.core.scratcharena;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.transform;
import ks.core.platform;
import ks.core.types;
import ks.math.vmatrix;
import ks.tier1.fixed_pool;
import ks.tier1.tokenset;
import ks.tier1.vector_ops;
import ks.core.color;
import ks.tier1.fmtstr;
import ks.tier1.string_ops;
import ks.math.aabb;
import ks.core.random;
import ks.vstdlib.ikeyvaluessystem;
import ks.game.entity_cast;
import :econ.econ_item_schema_minimal;

class CBaseEntity;

using namespace ::ks::reflect;

static const char *s_pPrecacheResourceTypeName[] =
{
	"vgui_resource",
	"material",		
	"model",			
	"scriptsound",		// NOTE: This is named this way for backward compat in reading from kv files	
	"particle_system",	
	"entity",
	"decal",
	"pmaterial",
	"dependency_file",
	"game_material_decals",
	"physics_gamesounds",
	"shared",
};

//-----------------------------------------------------------------------------
// Provides callback to do actual precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheHandler
{
public:
	void CacheResource( PrecacheResourceType_t nType, const char *pName, bool bPrecache, ResourceList_t hResourceList, int *pIndex = nullptr );

private:
	void CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList );
	void PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList );
	void PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList );
};

//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
static CPrecacheRegister	s_PrecacheRegister;
CPrecacheRegister *g_pPrecacheRegister = &s_PrecacheRegister;

static CPrecacheHandler		s_PrecacheHandler;
static auto s_CacheResource = []( PrecacheResourceType_t nType, const char *pName, bool bPrecache, ResourceList_t hResourceList, int *pIndex )
{
	s_PrecacheHandler.CacheResource( nType, pName, bPrecache, hResourceList, pIndex );
};
PrecacheHandler_t g_PrecacheHandler( s_CacheResource );

bool CPrecacheRegister::Init()
{ 
	return true; 
}

//-----------------------------------------------------------------------------
// Level startup, shutdown
//-----------------------------------------------------------------------------
void CPrecacheRegister::LevelInitPreEntity()
{
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Start" );
	g_pPrecacheSystem->Cache( g_PrecacheHandler, GLOBAL, nullptr, true, RESOURCE_LIST_INVALID, false );
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Finish" );

	//FIXME: Double check this
	//Finally, force the cache of these materials
	COM_TimestampedLog( "LevelInitPreEntity - CacheUsedMaterials - Start" );
	materials->CacheUsedMaterials();
	COM_TimestampedLog( "LevelInitPreEntity - CacheUsedMaterials - Finish" );
}

void CPrecacheRegister::LevelShutdownPostEntity()
{
	// FIXME: How to uncache all resources cached during the course of the level?
	g_pPrecacheSystem->UncacheAll( g_PrecacheHandler );

	if ( g_pResourceAccessControl )
	{
		g_pResourceAccessControl->DestroyAllResourceLists();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList )
{
	static_assert( std::size( s_pPrecacheResourceTypeName ) == PRECACHE_RESOURCE_TYPE_COUNT );

	KeyValues *pValues = new KeyValues( "ResourceFile" );

	if ( !pValues->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" ) )
	{
		Warning( "Can't open %s for client precache info.", pFilename );
		pValues->deleteThis();
		return;
	}

	for ( KeyValues *pData = pValues->GetFirstSubKey(); pData != nullptr; pData = pData->GetNextKey() )
	{
		const char *pszType = pData->GetName();
		const char *pszFile = pData->GetString();

		if ( V_strlen( pszType ) == 0 || V_strlen( pszFile ) == 0 )
			continue;

		bool bFoundMatch = false;
		for ( int i = 0; i < PRECACHE_RESOURCE_TYPE_COUNT; ++i )
		{
			if ( !V_stricmp( pData->GetName(), s_pPrecacheResourceTypeName[i] ) )
			{
				CacheResource( (PrecacheResourceType_t)i, pszFile, bPrecache, hResourceList );
				bFoundMatch = true;
				break;
			}
		}

		if ( !bFoundMatch )
		{
			Warning( "Error in precache file \"%s\":\n", pFilename );
			Warning( "\tUnknown resource type specified \"%s\", value \"%s\"\n", pszType, pszFile );
		}
	}

	pValues->deleteThis();
}

//-----------------------------------------------------------------------------
// Precaches game material decals
//-----------------------------------------------------------------------------
void CPrecacheHandler::PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList )
{
}

void CPrecacheHandler::PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList )
{
	if ( !bPrecache )
		return;

	// precache the surface prop sounds
	bool bBulletSounds = !V_stricmp( pName, "BulletSounds" );
	bool bStepSounds = !V_stricmp( pName, "StepSounds" );
	bool bPhysicsImpactSounds = !V_stricmp( pName, "PhysicsImpactSounds" );

	for ( int i = 0; i < physprops->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = physprops->GetSurfaceData( i );
		contract_assert( pprop );

		if ( bBulletSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.bulletImpact );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}

		if ( bStepSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.walkStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.walkStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.runStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.runStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}

		if ( bPhysicsImpactSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.impactSoft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.impactHard );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.scrapeSmooth );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.scrapeRough );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.rolling );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.breakSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.strainSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}
	}
}

//-----------------------------------------------------------------------------
// Caches/uncaches resources
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResource( PrecacheResourceType_t nType, const char *pName, 
	bool bPrecache, ResourceList_t hResourceList, int *pIndex )
{
	if ( bPrecache )
	{
		if ( pIndex )
		{
			*pIndex = 0;
		}

		switch( nType )
		{
		case VGUI_RESOURCE:
			break;

		case MATERIAL:
			PrecacheMaterial( pName ); 
			if ( pIndex )
			{
				*pIndex = GetMaterialIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MATERIAL, pName );
			}
			break;

		case PARTICLE_MATERIAL:
			{
				void* nIndex = ParticleMgr()->GetPMaterial( pName );
				if ( pIndex )
				{
					void** pIndexMaterial = (void**)pIndex;

					*pIndexMaterial = nIndex;
				}
			}
			break;

		case GAME_MATERIAL_DECALS:
			PrecacheGameMaterialDecals( bPrecache, hResourceList );
			break;

		case PHYSICS_GAMESOUNDS:
			PrecachePhysicsSounds( pName, bPrecache, hResourceList );
			break;

		case DECAL:
			{
				int nIndex = UTIL_PrecacheDecal( pName, true );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
			}
			break;

		case MODEL:
			{
				int nIndex = CBaseEntity::PrecacheModel( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MODEL, pName );
				}
			}
			break;

		case GAMESOUND:
			{
				int nIndex = CBaseEntity::PrecacheScriptSound( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_GAMESOUND, pName );
				}
			}
			break;

		case PARTICLE_SYSTEM:
			PrecacheParticleSystem( pName );
			if ( pIndex )
			{
				*pIndex = GetParticleSystemIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_PARTICLE_SYSTEM, pName );
			}
			break;

		case ENTITY:
			UTIL_PrecacheOther( pName );
			break;

		case SHARED:
			g_pPrecacheSystem->Cache( g_PrecacheHandler, SHARED_SYSTEM, pName, bPrecache, hResourceList, false );
			break;

		case KV_DEP_FILE:
			CacheResourceFile( pName, bPrecache, hResourceList );
			break;
		}
		return;
	}

	// Blat out value
	if ( pIndex )
	{
		*pIndex = 0;
	}

	switch( nType )
	{
	case VGUI_RESOURCE:
		break;
	case MATERIAL:
		break;
	case MODEL:
		break;
	case GAMESOUND:
		break;
	case PARTICLE_SYSTEM:
		break;
	case ENTITY:
		break;
	case DECAL:
		break;
	case KV_DEP_FILE:
		break;
	}
}

class CIronSightController;
class CBasePlayer;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( PredictedViewModel, DT_PredictedViewModel )
LINK_ENTITY_TO_CLASS_ALIASED( predicted_viewmodel, PredictedViewModel );

IMPLEMENT_REFLECT_TABLE( CPredictedViewModel, DT_PredictedViewModel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPredictedViewModel::CPredictedViewModel() : m_LagAnglesHistory("CPredictedViewModel::m_LagAnglesHistory")
{
	m_vLagAngles.Init();
	m_LagAnglesHistory.Setup( &m_vLagAngles, INTERPOLATE_LINEAR_ONLY );
	m_vPredictedOffset.Init();
	m_flInaccuracyTilt = 0;
	m_flOldAccuracyDiffSmoothed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPredictedViewModel::~CPredictedViewModel()
{
}

ConVar cl_wpn_sway_interp( side_registry(), "cl_wpn_sway_interp", "0.1", FCVAR_CLIENTDLL );
ConVar cl_wpn_sway_scale( side_registry(), "cl_wpn_sway_scale", "1.6", FCVAR_CLIENTDLL|FCVAR_CHEAT );

extern ConVar	cl_use_new_headbob;

//-----------------------------------------------------------------------------
// Purpose:  Adds head bob for off hand models
//-----------------------------------------------------------------------------
void CPredictedViewModel::AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles )
{
	if ( cl_use_new_headbob.GetBool() == false )
		return;

	// if we are an off hand view model (index 1) and we have a model, add head bob.
	// (Head bob for main hand model added by the weapon itself.)
	if ( ViewModelIndex() == 1 && m_bShouldIgnoreOffsetAndAccuracy )
	{
		CalcViewModelBobHelper( owner, &m_BobState, 1 );
		AddViewModelBobHelper( eyePosition, eyeAngles, &m_BobState );
	}
}

void CPredictedViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& /*original_angles*/ )
{
	float interp = cl_wpn_sway_interp.GetFloat();
	if ( !interp || m_bShouldIgnoreOffsetAndAccuracy )
		return;

	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
	{
		origin += m_vPredictedOffset;
		return;
	}

	// Calculate our drift
	Vector	forward, right, up;
	AngleVectors( angles, &forward, &right, &up );
	
	// Add an entry to the history.
	m_vLagAngles = angles;
	m_LagAnglesHistory.NoteChanged( gpGlobals->curtime, gpGlobals->curtime, interp, false );
	
	// Interpolate back 100ms.
	m_LagAnglesHistory.Interpolate( gpGlobals->curtime, interp );
	
	// Now take the 100ms angle difference and figure out how far the forward vector moved in local space.
	Vector vLaggedForward;
	QAngle angleDiff = m_vLagAngles - angles;
	AngleVectors( -angleDiff, &vLaggedForward, 0, 0 );
	Vector vForwardDiff = Vector(1,0,0) - vLaggedForward;

	if ( ShouldFlipModel() )
		right = -right;

	// Now offset the origin using that.
	vForwardDiff *= cl_wpn_sway_scale.GetFloat();
	m_vPredictedOffset = forward*vForwardDiff.x + right*-vForwardDiff.y + up*vForwardDiff.z;

	// reduce offset as viewmodel angle approaches nearly vertical
	float flMult = clamp( abs(DotProduct(up, Vector(0,0,1))) - 0.02f, 0, 1 );

	origin += (m_vPredictedOffset * flMult);
}

ConVar cl_gunlowerangle( side_registry(), "cl_gunlowerangle", "2", FCVAR_CLIENTDLL );
ConVar cl_gunlowerspeed( side_registry(), "cl_gunlowerspeed", "0.1", FCVAR_CLIENTDLL );

void CPredictedViewModel::ApplyViewModelPitchAndDip( CBasePlayer *owner, Vector& vecNewOrigin, QAngle& vecNewAngles )
{
	//orients and moves weapon to provide visual feedback on weapon accuracy and player motion (like jumping and landing)
	// Check for lowering the weapon
	// CCSPlayer *pPlayer = ToCSPlayer( owner );
	// Assert( pPlayer );

	bool bJumping = !( owner->GetFlags() & FL_ONGROUND );

	bool bLowered = bJumping;//pPlayer->IsWeaponLowered();

	QAngle vecLoweredAngles(0,0,0);

	m_vLoweredWeaponOffset.x = Approach( bLowered ? cl_gunlowerangle.GetFloat() : 0, m_vLoweredWeaponOffset.x, cl_gunlowerspeed.GetFloat() );
	vecLoweredAngles.x += m_vLoweredWeaponOffset.x;
	vecNewAngles -= vecLoweredAngles * 0.2f;
	vecNewOrigin.z -= vecLoweredAngles.x * 0.4f; // translation offset looks more natural than rotation

	float flDipAddAmt = 0.0f;
	float flOldFallVel = owner->m_Local.m_flOldFallVelocity;
	// This does the dip when you land
	// m_Local.m_bInLanding gets set on the client in baseplayer_shared.cpp  -- mtw
	if ( owner && owner->m_Local.m_bInLanding == true )
	{
		float flDipAmt = 0.005;//weapon_land_dip_amt.GetFloat();

		float landseconds = std::max(gpGlobals->curtime - (owner->m_Local.m_flLandingTime - 0.1f), 0.0f);
		float landFraction = SmoothCurve( landseconds / 0.25f );
		clamp( landFraction, 0.0f, 1.0f );

		float flDipAmount = (1 / flOldFallVel) * flDipAmt;

		int dipHighOffset = 64;
		int dipLowOffset = dipHighOffset - flDipAmt;

		Vector temp = owner->GetViewOffset();
		temp.z = ( ( dipLowOffset - flDipAmount ) * landFraction ) +
			( dipHighOffset * ( 1 - landFraction ) );

		if ( temp.z > dipHighOffset )
		{
			temp.z = dipHighOffset;
		}

		flDipAddAmt = ( dipHighOffset - temp.z );
	}

//	Replaced with CL #2056767 code in baseviewmodel_shared.cpp
//	that applies aimpunch to viewmodel angle
//////////////////////////////////////////
// 	CWeaponCSBase *pWeapon = ( CWeaponCSBase * )GetOwningWeapon();
// 	// pitch the viewmodel up to match where the bullets are going
// 	if ( pWeapon )
// 	{
// 		float flMaxMultiplier = 40;
// 		float flMultiplier = flMaxMultiplier;
// 
// 		if ( !m_bShouldIgnoreOffsetAndAccuracy )
// 			flMultiplier -= std::max( 0, (gpGlobals->curtime - pWeapon->m_flNextPrimaryAttack) * flMaxMultiplier );
// 
// 		float flMultMinClamp = 0;
// 		if ( bJumping )
// 			flMultMinClamp = 5.0f;
// 
// 		flMultiplier = clamp( flMultiplier, flMultMinClamp, flMaxMultiplier );
// 
// 		// get the player's default accuracy
// 		float flBaseAccuracy = pWeapon->GetCSWpnData().GetInaccuracyStand( 0, pWeapon->GetAttributeContainer()->GetItem() );
// 		// get the current accuracy of the player
// 		float flAccuracy = m_bShouldIgnoreOffsetAndAccuracy ? 0 : pWeapon->GetInaccuracy();
// 		//float flAccuracyFishtail = m_bShouldIgnoreOffsetAndAccuracy ? 0 : pWeapon->GetAccuracyFishtail();
// 		// get the difference between the two
// 		float flAccuracyDiff = m_bShouldIgnoreOffsetAndAccuracy ? 0 : abs((flAccuracy - flBaseAccuracy) * flMultiplier);
// 
// 		float flApproachSpeed = 25.0f;
// 		// if we are in the air, we just jumped, we are running really fast,
// 		// adjust the appraoch speed based on how fast it changed
// 		// this avoids big visual pops 
// 		if ( bJumping || (!m_bShouldIgnoreOffsetAndAccuracy && owner->GetAbsVelocity().Length2D() > 150.0f) )// && flAccuracyDiff > m_flOldAccuracyDiff )
// 		{
// 			float flMax = 7.5f;
// 			float flDiv = 10.0f;
// 			// if we're on the ground and moving fast
// 			if ( owner->GetAbsVelocity().Length2D() > 150.0f && !bJumping )
// 			{
// 				flMax = 25.0f;
// 				flDiv = 5.0f;
// 			}
// 
// 			float flOldToNewDiff = abs( ((flAccuracyDiff - m_flOldAccuracyDiffSmoothed) / flDiv) * flApproachSpeed );
// 			// small differences, we go faster, large differences and we go slower
// 			flApproachSpeed = clamp( flApproachSpeed - flOldToNewDiff, 5.0f, flMax );
// 		}
// 		
// 		// save off the accuracy difference
// 		m_flOldAccuracyDiffSmoothed = Approach( flAccuracyDiff, m_flOldAccuracyDiffSmoothed, abs( ((flAccuracyDiff)-m_flOldAccuracyDiffSmoothed )*gpGlobals->frametime) * 0.5f );
// 
// 		// smooth out the tilting
// 		m_flInaccuracyTilt = Approach( flAccuracyDiff, m_flInaccuracyTilt, abs( (flAccuracyDiff-m_flInaccuracyTilt)*gpGlobals->frametime) * flApproachSpeed );
// 		m_flInaccuracyTilt = std::min( m_flInaccuracyTilt, 3.0f );
// 		//Msg ( "owner->GetAbsVelocity().Length2D() = %f, flAccuracyDiff = %f,  m_flInaccuracyTilt = %f, flMultiplier = %f\n", owner->GetAbsVelocity().Length2D(), flAccuracyDiff, m_flInaccuracyTilt, flMultiplier );
// 		//Msg ( "flApproachSpeed = %f, flAccuracyDiff = %f,  m_flOldAccuracyDiffSmoothed = %f\n", flApproachSpeed, flAccuracyDiff, m_flOldAccuracyDiffSmoothed );
// 
// 		vecNewAngles.x -= m_flInaccuracyTilt;
// 		//vecNewAngles.y += flAccuracyFishtail;
// 		// dip the gun just a bit as well
// 		vecNewOrigin.z -= (m_flInaccuracyTilt * 0.5f) + flDipAddAmt;
// 	}
}

void CPredictedViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{
	if ( cl_use_new_headbob.GetBool() == false )
		return BaseClass::CalcViewModelView( owner, eyePosition, eyeAngles );

	Vector vecNewOrigin = eyePosition;
	QAngle vecNewAngles = eyeAngles;

	ApplyViewModelPitchAndDip( owner, vecNewOrigin, vecNewAngles );

	BaseClass::CalcViewModelView( owner, vecNewOrigin, vecNewAngles );

	CWeaponCSBase *pWeapon = ( CWeaponCSBase * )GetOwningWeapon();
	if ( pWeapon )
	{
		CIronSightController* pIronSightController = pWeapon->GetIronSightController();
		if ( pIronSightController )
		{
			Vector vecEyeOrigin = eyePosition;
			QAngle angEyeAngles = eyeAngles;

			if ( !prediction->InPrediction() )
			{
				//Retrieve ironsight position and angles, then lerp the weapon there if we're at all ironsighted
				pIronSightController->ApplyIronSightPositioning( vecEyeOrigin, angEyeAngles, GetLocalOrigin(), GetLocalAngles() );
				if ( pIronSightController->IsInIronSight() )
				{
					SetLocalOrigin( Lerp( pIronSightController->GetIronSightAmountGained(), GetLocalOrigin(), vecEyeOrigin ) );
					SetLocalAngles( Lerp( pIronSightController->GetIronSightAmountGained(), GetLocalAngles(), angEyeAngles ) );
				}
			}

		}
	}

	// bias the position of the viewmodel during observer camera interpolation
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverInterpState() != CBasePlayer::OBSERVER_INTERP_NONE )
	{
		CCSPlayer *pCSPlayer = ToCSPlayer( pLocalPlayer );
		if ( pCSPlayer )
		{
			Vector vecOffset = pCSPlayer->GetObserverInterpolatedOffsetVector();

			// pull up from offscreen a little
			vecOffset.z -= vecOffset.Length() * 0.2f;
			
			// dampen overall
			vecOffset *= 0.5f;

			SetLocalOrigin( GetLocalOrigin() + vecOffset );
		}
	}

}

bool CPredictedViewModel::ShouldPredict( void)
{
	if ( CBasePlayer::IsLocalPlayer( GetOwner() ) )
		return true;

	return BaseClass::ShouldPredict();
}

class CPrediction;
class CPredictionCopy;
class CUserCmd;
class CBaseAnimating;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;

IPredictionSystem *IPredictionSystem::g_pPredictionSystems = nullptr;

extern ConVar cl_predictphysics;

ConVar	cl_predictweapons	( side_registry(), "cl_predictweapons","1", FCVAR_USERINFO | FCVAR_NOT_CONNECTED, "Perform client side prediction of weapon effects." );
ConVar	cl_lagcompensation	( side_registry(), "cl_lagcompensation","1", FCVAR_USERINFO | FCVAR_NOT_CONNECTED, "Perform server side lag compensation of weapon firing events." );
ConVar	cl_showerror		( side_registry(), "cl_showerror", "0", FCVAR_RELEASE, "Show prediction errors, 2 for above plus detailed field deltas." );

static ConVar	cl_idealpitchscale	( side_registry(), "cl_idealpitchscale", "0.8", FCVAR_ARCHIVE );
static ConVar	cl_predictionlist	( side_registry(), "cl_predictionlist", "0", FCVAR_CHEAT, "Show which entities are predicting\n" );

static ConVar	cl_predictionentitydump( side_registry(), "cl_pdump", "-1", FCVAR_CHEAT, "Dump info about this entity to screen." );
static ConVar	cl_predictionentitydumpbyclass( side_registry(), "cl_pclass", "", FCVAR_CHEAT, "Dump entity by prediction classname." );
static ConVar	cl_pred_optimize( side_registry(), "cl_pred_optimize", "2", 0, "Optimize for not copying data if didn't receive a network update (1), and also for not repredicting if there were no errors (2)." );

static ConVar	cl_pred_doresetlatch( side_registry(), "cl_pred_doresetlatch", "1", 0 );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void InvalidateEFlagsRecursive( CBaseEntity *pEnt, int nDirtyFlags, int nChildFlags = 0 )
{
	pEnt->AddEFlags( nDirtyFlags );
	nDirtyFlags |= nChildFlags;
	for (CBaseEntity *pChild = pEnt->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer())
	{
		InvalidateEFlagsRecursive( pChild, nDirtyFlags );
	}
}

extern IGameMovement *g_pGameMovement;
extern CMoveData *g_pMoveData;

void COM_Log( const char *pszFile, const char *fmt, ...) [[gnu::format(printf, 2, 3)]]; // Log a debug message to specified file ( if pszFile == NULL uses c:\\hllog.txt )
void PhysicsSimulate( void );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrediction::CPrediction( void ) : m_SavedVars( true )
{
	m_bInPrediction = false;

	m_nPreviousStartFrame = -1;
	m_nIncomingPacketNumber = 0;

	m_bPlayerOriginTypedescriptionSearched = false;
	m_bEnginePaused = false;

	m_flLastServerWorldTimeStamp = -1.0f;
}

CPrediction::~CPrediction( void )
{
}

void CPrediction::Init( void )
{
	m_bOldCLPredictValue = cl_predict->GetBool();
}

void CPrediction::Shutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CPrediction::CheckError( int nSlot, CBasePlayer *player, int commands_acknowledged )
{
	Vector		origin;
	Vector		delta;
	float		len;
	static int	pos[ MAX_SPLITSCREEN_PLAYERS ];

	// Not in the game yet
	if ( !engine->IsInGame() )
		return;

	// Not running prediction
	if ( !cl_predict->GetInt() )
		return;

	if ( !player )
		return;
	
	// Not predictable yet (flush entity packet?)
	if ( !player->IsIntermediateDataAllocated() )
		return;

	origin = player->GetNetworkOrigin();
		
	const void *slot = player->GetPredictedFrame( commands_acknowledged - 1 );
	if ( !slot )
		return;

	if ( !m_bPlayerOriginTypedescriptionSearched )
	{
		m_bPlayerOriginTypedescriptionSearched = true;
		const typedescription_t *td = CPredictionCopy::FindFlatFieldByName( "m_vecNetworkOrigin", player->GetPredDescMap() );
		if ( td ) 
		{
			m_PlayerOriginTypeDescription.push_back( td );
		}
	}

	if ( m_PlayerOriginTypeDescription.empty() )
		return;

	Vector predicted_origin;

	// Find the origin field in the database
	// Splitting m_vecNetworkOrigin into component fields for prediction *
	V_memcpy( (Vector *)&predicted_origin, (Vector *)( (byte *)slot + m_PlayerOriginTypeDescription[ 0 ]->flatOffset[ TD_OFFSET_PACKED ] ), sizeof( Vector ) );

	// Compare what the server returned with what we had predicted it to be
	VectorSubtract ( predicted_origin, origin, delta );

	len = VectorLength( delta );
	if (len > MAX_PREDICTION_ERROR )
	{	
		// A teleport or something, clear out error
		len = 0;
	}
	else
	{
		if ( len > MIN_PREDICTION_EPSILON )
		{
			player->NotePredictionError( delta );

			if ( cl_showerror.GetInt() >= 1 )
			{
				con_nprint_t np;
				np.fixed_width_font = true;
				np.color[0] = 1.0f;
				np.color[1] = 0.95f;
				np.color[2] = 0.7f;
				np.index = 4 + nSlot * 10 + ( ++pos[ nSlot ] % 10 );
				np.time_to_live = 3.0f;

				engine->Con_NXPrintf( &np, "%d len(%6.3f) (%6.3f %6.3f %6.3f)", nSlot, len, delta.x, delta.y, delta.z );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::ShutdownPredictables( void )
{

	int shutdown_count = 0;
	int release_count = 0;

	for ( int nSlot = 0; nSlot < MAX_SPLITSCREEN_PLAYERS; ++nSlot )
	{
		// Transfer intermediate data from other predictables
		int c = GetPredictables( nSlot )->GetPredictableCount();
		for ( int i = c - 1; i >= 0 ; i-- )
		{
			CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent )
				continue;

			// Shutdown predictables
			if ( ent->GetPredictable() )
			{
				ent->ShutdownPredictable();
				shutdown_count++;
			}
			// Otherwise, release client created entities
			else
			{
				ent->Release();
				release_count++;
			}
		}

		// All gone now...
		contract_assert( GetPredictables( nSlot )->GetPredictableCount() == 0 );
	}

	if ( ( release_count > 0 ) || 
		 ( shutdown_count > 0 ) )
	{
		DevMsg( "Shutdown %i predictable entities and %i client-created entities\n",
			shutdown_count,
			release_count );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::ReinitPredictables( void )
{
	// Go through all entities and init any eligible ones
	int i;
	int c = ClientEntityList().GetHighestEntityIndex();
	for ( i = 0; i <= c; i++ )
	{
		CBaseEntity *e = ClientEntityList().GetBaseEntity( i );
		if ( !e )
			continue;
		
		if ( e->GetPredictable() )
			continue;

		e->CheckInitPredictable( "ReinitPredictables" );
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		Msg( "%d:  Reinitialized %i predictable entities\n",
			nSlot, GetPredictables( nSlot )->GetPredictableCount() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::OnReceivedUncompressedPacket( void )
{
	m_nPreviousStartFrame = -1;
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		Split_t &split = m_Split[ i ];
		split.m_nCommandsPredicted = 0;
		split.m_nServerCommandsAcknowledged = 0;
		split.m_nLastCommandAcknowledged = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : commands_acknowledged - 
//			current_world_update_packet - 
// Output : void CPrediction::PreEntityPacketReceived
//-----------------------------------------------------------------------------
void CPrediction::PreEntityPacketReceived ( int commands_acknowledged, int current_world_update_packet, int server_ticks_elapsed )
{
	CVProfScope _( "CPrediction::PreEntityPacketReceived", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Cache off incoming packet #
	m_nIncomingPacketNumber = current_world_update_packet;
	CBaseEntity::s_nIncomingPacketCommandsAcknowledged = commands_acknowledged;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
	{
		ShutdownPredictables();
		return;
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		CBasePlayer *current = CBasePlayer::GetLocalPlayer( nSlot );
		// No local player object?
		if ( !current )
			continue;

		// Transfer intermediate data from other predictables
		int c = GetPredictables( nSlot )->GetPredictableCount();

		int i;
		for ( i = 0; i < c; i++ )
		{
			CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent )
				continue;

			if ( !ent->GetPredictable() )
				continue;

			if ( (commands_acknowledged != server_ticks_elapsed) && ent->PredictionIsPhysicallySimulated() )
			{
				ent->ShiftIntermediateData_TickAdjust( server_ticks_elapsed - commands_acknowledged, m_Split[nSlot].m_nCommandsPredicted );
				m_Split[nSlot].m_bPerformedTickShift = true;
			}

			ent->PreEntityPacketReceived( commands_acknowledged );
			ent->OnPostRestoreData();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called for every packet received( could be multiple times per frame)
//-----------------------------------------------------------------------------
void CPrediction::PostEntityPacketReceived( void )
{
	PREDICTION_TRACKVALUECHANGESCOPE( "postentitypacket" );
	CVProfScope _( "CPrediction::PostEntityPacketReceived", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseEntity::s_nIncomingPacketCommandsAcknowledged = -1;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		CBasePlayer *current = CBasePlayer::GetLocalPlayer( nSlot );
		// No local player object?
		if ( !current )
			continue;

		int c = GetPredictables( nSlot )->GetPredictableCount();

		// Transfer intermediate data from other predictables
		int i;
		for ( i = 0; i < c; i++ )
		{
			CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent )
				continue;

			if ( !ent->GetPredictable() )
				continue;

			// Always mark as changed
			if ( AddDataChangeEvent( ent, DATA_UPDATE_DATATABLE_CHANGED, &ent->m_DataChangeEventRef ) )
			{
				ent->OnPreDataChanged( DATA_UPDATE_DATATABLE_CHANGED );
			}
			ent->PostEntityPacketReceived();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ent - 
// Output : static bool
//-----------------------------------------------------------------------------
bool CPrediction::ShouldDumpEntity( CBaseEntity *ent )
{
	int dump_entity = cl_predictionentitydump.GetInt();
	if ( dump_entity != -1 )
	{
		bool dump = false;
		if ( ent->entindex() == -1 )
		{
			dump = ( dump_entity == ent->entindex() ) ? true : false;
		}
		else
		{
			dump = ( ent->entindex() == dump_entity ) ? true : false;
		}

		if ( !dump )
		{
			return false;
		}
	}
	else
	{
		if ( cl_predictionentitydumpbyclass.GetString()[ 0 ] == 0 )
			return false;

		if ( !FClassnameIs( ent, cl_predictionentitydumpbyclass.GetString() ) )
			return false;
	}
	return true;
}

void CPrediction::ShowPredictionListEntry( int listRow, int showlist, CBaseEntity *ent, int &totalsize, int &totalsize_intermediate )
{
	char sz[ 32 ];
	if ( ent->entindex() == -1 )
	{
		V_snprintf( sz, sizeof( sz ), "handle %u", (unsigned int)ent->GetClientHandle().ToInt() );
	}
	else
	{
		V_snprintf( sz, sizeof( sz ), "%i", ent->entindex() );
	}

	int oIndex = 0;
	if ( ent->GetOwnerEntity() )
	{
		oIndex = ent->GetOwnerEntity()->entindex();
	}
	else if ( ent->IsPlayer() )
	{
		oIndex = ent->entindex();
	}
	else 
	{
		CBaseViewModel *pVM = ToBaseViewModel( ent );
		if ( pVM && pVM->GetOwner() )
		{
			oIndex = pVM->GetOwner()->entindex();
		}
	}

	con_nprint_t np;
	np.fixed_width_font = true;
	np.color[0] = 0.8f;
	np.color[1] = 1.0f;
	np.color[2] = 1.0f;
	np.time_to_live = 2.0f;
	np.index = listRow;
	if ( showlist >= 2 )
	{
		int size = GetClassMap().GetClassSize( ent->GetClassname() );
		int intermediate_size = ent->GetIntermediateDataSize() * ( MULTIPLAYER_BACKUP + 1 );

		engine->Con_NXPrintf( &np, "%15s %30s(%d) (%5i / %5i bytes): %15s", 
			sz, 
			ent->GetClassname(),
			oIndex,
			size,
			intermediate_size,
			ent->GetPredictable() ? "predicted" : "client created" );

		totalsize += size;
		totalsize_intermediate += intermediate_size;
	}
	else
	{
		engine->Con_NXPrintf( &np, "%15s %30s(%d): %15s", 
			sz, 
			ent->GetClassname(),
			oIndex,
			ent->GetPredictable() ? "predicted" : "client created" );
	}
}

void CPrediction::FinishPredictionList( int listRow, int showlist, int totalsize, int totalsize_intermediate )
{
	if ( !showlist )
		return;

	if ( showlist > 1 )
	{
		con_nprint_t np;
		np.fixed_width_font = true;
		np.color[0] = 0.8f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;
		np.time_to_live = 2.0f;
		np.index = listRow++;
		char sz1[32];
		char sz2[32];

		V_strncpy( sz1, V_pretifymem( (float)totalsize ), sizeof( sz1 ) );
		V_strncpy( sz2, V_pretifymem( (float)totalsize_intermediate ), sizeof( sz2 ) );

		engine->Con_NXPrintf( &np, "%15s %27s (%s / %s)  %14s", 
			"totals:", 
			"",
			sz1,
			sz2,
			"" );
	}

	// Zero out rest of list
	while ( listRow < 20 )
	{
		engine->Con_NPrintf( listRow++, "" );
	}
}

void CPrediction::CheckPredictConvar()
{
	if ( cl_predict->GetBool() != m_bOldCLPredictValue )
	{
		if ( !m_bOldCLPredictValue )
		{
			ReinitPredictables();
		}

		for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			Split_t &split = m_Split[ i ];
			split.m_nCommandsPredicted = 0;
			split.m_nServerCommandsAcknowledged = 0;
			split.m_nLastCommandAcknowledged = 0;
		}
		m_nPreviousStartFrame = -1;
	}

	m_bOldCLPredictValue = cl_predict->GetBool();
}

ConVar cl_prediction_error_timestamps( side_registry(), "cl_prediction_error_timestamps", "0" );
//-----------------------------------------------------------------------------
// Purpose: Called at the end of the frame if any packets were received
// Input  : error_check - 
//			last_predicted - 
//-----------------------------------------------------------------------------
void CPrediction::PostNetworkDataReceived( int commands_acknowledged )
{
	ks::ScratchArena<> scratch;
	CVProfScope _( "CPrediction::PostNetworkDataReceived", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	bool error_check = ( commands_acknowledged > 0 ) ? true : false;
	//Msg( "%i/%i ack %i commands/slot\n",
	//	gpGlobals->framecount,
	//	gpGlobals->tickcount,
	//	commands_acknowledged - 1 );

	bool entityDumped = false;
	bool bPredict = cl_predict->GetBool();
	int showlist = cl_predictionlist.GetInt();

	int listRow = 0;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		Split_t &split = m_Split[ nSlot ];
		split.m_nServerCommandsAcknowledged += commands_acknowledged;
		split.m_bPreviousAckHadErrors = false;
		split.m_bPreviousAckErrorTriggersFullLatchReset = false;
		split.m_EntsWithPredictionErrorsInLastAck.clear();

		if ( bPredict )
		{
			// Don't screw up memory of current player from history buffers if not filling in history buffers during prediction!!!
			int totalsize = 0;
			int totalsize_intermediate = 0;

			// Build list of all predictables
			int c = GetPredictables( nSlot )->GetPredictableCount();
			bool *bHadErrors = scratch.Alloc<bool>( c );
			// Transfer intermediate data from other predictables
			int i;
			for ( i = 0; i < c; i++ )
			{
				CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
				if ( !ent )
					continue;

				if ( !ent->GetPredictable() )
					continue;

				bHadErrors[i] = ent->PostNetworkDataReceived( split.m_nServerCommandsAcknowledged );
				if ( bHadErrors[i] )
				{
					split.m_bPreviousAckHadErrors = true;
					split.m_bPreviousAckErrorTriggersFullLatchReset |= ent->PredictionErrorShouldResetLatchedForAllPredictables();
					split.m_EntsWithPredictionErrorsInLastAck.push_back( ent ); 
				}

				if ( !showlist )
				{
					continue;
				}

				ShowPredictionListEntry( listRow, showlist, ent, totalsize, totalsize_intermediate );
				listRow++;
			}

			//Give entities with predicted fields that are not networked a chance to fix their current values for those fields.
			//We do this in two passes. One pass to fix the fields, then another to save off the changes after they've all finished (to handle interdependancies, portals)
			if( split.m_bPreviousAckHadErrors )
			{
				//give each predicted entity a chance to fix up its non-networked predicted fields
				for ( i = 0; i < c; i++ )
				{
					CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
					if ( !ent )
						continue;

					if ( !ent->GetPredictable() )
						continue;

					ent->HandlePredictionError( bHadErrors[i] );
				}

				//save off any changes
				for ( i = 0; i < c; i++ )
				{
					CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
					if ( !ent )
						continue;

					if ( !ent->GetPredictable() )
						continue;

					ent->SaveData( "PostNetworkDataReceived() Ack Errors", CBaseEntity::SLOT_ORIGINALDATA, PC_EVERYTHING );
				}
			}

			for ( i = c; --i >= 0; ) //go backwards to maintain ordering on shutdown
			{
				CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
				if ( !ent )
					continue;

				if ( !ent->GetPredictable() )
					continue;

				ent->CheckShutdownPredictable( "CPrediction::PostNetworkDataReceived" );
			}

			FinishPredictionList( listRow, showlist, totalsize, totalsize_intermediate );

			if ( error_check )
			{
				CBasePlayer *current = CBasePlayer::GetLocalPlayer( nSlot );
				if ( !current )
					continue;
				CheckError( nSlot, current, split.m_nServerCommandsAcknowledged );
			}
		}

		// Can also look at regular entities
		int dumpentindex = cl_predictionentitydump.GetInt();

		if( split.m_bPreviousAckHadErrors && cl_prediction_error_timestamps.GetBool() )
		{
			Warning( "Prediction errors occurred at %i %f\n", gpGlobals->tickcount, gpGlobals->curtime );
		}
	}

	CheckPredictConvar();

}

//-----------------------------------------------------------------------------
// Purpose: Prepare for running prediction code
// Input  : *ucmd - 
//			*from - 
//			*pHelper - 
//			&moveInput - 
//-----------------------------------------------------------------------------
void CPrediction::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move ) 
{
	CVProfScope _( "CPrediction::SetupMove", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	move->m_bFirstRunOfFunctions = IsFirstTimePredicted();
	move->m_bGameCodeMovedPlayer = false;
	if ( player->GetPreviouslyPredictedOrigin() != player->GetNetworkOrigin() )
	{
		move->m_bGameCodeMovedPlayer = true;
	}
	
	move->m_nPlayerHandle = player->GetClientHandle();
	move->m_vecVelocity		= player->GetAbsVelocity();
	move->SetAbsOrigin( player->GetNetworkOrigin() );
	move->m_vecOldAngles	= move->m_vecAngles;
	move->m_nOldButtons		= player->m_Local.m_nOldButtons;
	move->m_flClientMaxSpeed = player->m_flMaxspeed;

	move->m_vecAngles		= ucmd->viewangles;
	move->m_vecViewAngles	= ucmd->viewangles;
	move->m_nImpulseCommand = ucmd->impulse;	
	move->m_nButtons		= ucmd->buttons;

	CBaseEntity *pMoveParent = player->GetMoveParent();
	if (!pMoveParent)
	{
		move->m_vecAbsViewAngles = move->m_vecViewAngles;
	}
	else
	{
		matrix3x4_t viewToParent, viewToWorld;
		AngleMatrix( move->m_vecViewAngles, viewToParent );
		ConcatTransforms( pMoveParent->EntityToWorldTransform(), viewToParent, viewToWorld );
		MatrixAngles( viewToWorld, move->m_vecAbsViewAngles );
	}

	// Ingore buttons for movement if at controls
	if (player->GetFlags() & FL_ATCONTROLS)
	{
		move->m_flForwardMove		= 0;
		move->m_flSideMove			= 0;
		move->m_flUpMove			= 0;
	}
	else
	{
		move->m_flForwardMove		= ucmd->forwardmove;
		move->m_flSideMove			= ucmd->sidemove;
		move->m_flUpMove			= ucmd->upmove;
	}
		
	

	// Copy constraint information
	if ( player->m_hConstraintEntity )
		move->m_vecConstraintCenter = player->m_hConstraintEntity->GetAbsOrigin();
	else
		move->m_vecConstraintCenter = player->m_vecConstraintCenter;

	move->m_flConstraintRadius = player->m_flConstraintRadius;
	move->m_flConstraintWidth = player->m_flConstraintWidth;
	move->m_flConstraintSpeedFactor = player->m_flConstraintSpeedFactor;

	g_pGameMovement->SetupMovementBounds( move );
}

//-----------------------------------------------------------------------------
// Purpose: Finish running prediction code
// Input  : &move - 
//			*to - 
//-----------------------------------------------------------------------------
void CPrediction::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	CVProfScope _( "CPrediction::FinishMove", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	player->m_RefEHandle = move->m_nPlayerHandle;

	player->SetAbsVelocity( move->m_vecVelocity );

	player->m_vecNetworkOrigin = move->GetAbsOrigin();
	player->SetPreviouslyPredictedOrigin( move->GetAbsOrigin() );
	
	player->m_Local.m_nOldButtons = move->m_nButtons;

	player->m_flMaxspeed = move->m_flClientMaxSpeed;
	
	m_hLastGround = player->GetGroundEntity();
 
	player->SetLocalOrigin( move->GetAbsOrigin() );

	

	// Sanity checks
	if ( player->m_hConstraintEntity )
		contract_assert( move->m_vecConstraintCenter == player->m_hConstraintEntity->GetAbsOrigin() );
	else
		contract_assert( move->m_vecConstraintCenter == player->m_vecConstraintCenter );
	contract_assert( move->m_flConstraintRadius == player->m_flConstraintRadius );
	contract_assert( move->m_flConstraintWidth == player->m_flConstraintWidth );
	contract_assert( move->m_flConstraintSpeedFactor == player->m_flConstraintSpeedFactor );
}

//-----------------------------------------------------------------------------
// Purpose: Called before any movement processing
// Input  : *player - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CPrediction::StartCommand( CBasePlayer *player, CUserCmd *cmd )
{
	CVProfScope _( "CPrediction::StartCommand", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	player->m_pCurrentCommand = cmd;
	player->m_LastCmd = *cmd;
	CBaseEntity::SetPredictionRandomSeed( cmd );
	CBaseEntity::SetPredictionPlayer( player );
}

//-----------------------------------------------------------------------------
// Purpose: Called after any movement processing
// Input  : *player - 
//-----------------------------------------------------------------------------
void CPrediction::FinishCommand( CBasePlayer *player )
{
	CVProfScope _( "CPrediction::FinishCommand", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	player->m_pCurrentCommand = nullptr;
	CBaseEntity::SetPredictionRandomSeed( nullptr );
	CBaseEntity::SetPredictionPlayer( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: Called before player thinks
// Input  : *player - 
//			thinktime - 
//-----------------------------------------------------------------------------
void CPrediction::RunPreThink( CBasePlayer *player )
{
	CVProfScope _( "CPrediction::RunPreThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Run think functions on the player
	if ( !player->PhysicsRunThink() )
		return;

	// Called every frame to let game rules do any specific think logic for the player
	// FIXME:  Do we need to set up a client side version of the gamerules???
	// g_pGameRules->PlayerThink( player );

	player->PreThink();
}

//-----------------------------------------------------------------------------
// Purpose: Runs the PLAYER's thinking code if time.  There is some play in the exact time the think
//  function will be called, because it is called before any movement is done
//  in a frame.  Not used for pushmove objects, because they must be exact.
//  Returns false if the entity removed itself.
// Input  : *ent - 
//			frametime - 
//			clienttimebase - 
// Output : void CPlayerMove::RunThink
//-----------------------------------------------------------------------------
void CPrediction::RunThink (CBasePlayer *player, double frametime )
{
	CVProfScope _( "CPrediction::RunThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	int thinktick = player->GetNextThinkTick();

	if ( thinktick <= 0 || thinktick > player->m_nTickBase )
		return;
	
	player->SetNextThink( TICK_NEVER_THINK );

	// Think
	player->Think();
}

//-----------------------------------------------------------------------------
// Purpose: Called after player movement
// Input  : *player - 
//			thinktime - 
//			frametime - 
//-----------------------------------------------------------------------------
void CPrediction::RunPostThink( CBasePlayer *player )
{
	CVProfScope _( "CPrediction::RunPostThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Run post-think
	player->PostThink();
}

//-----------------------------------------------------------------------------
// Purpose: Checks if the player is standing on a moving entity and adjusts velocity and 
//  basevelocity appropriately
// Input  : *player - 
//			frametime - 
//-----------------------------------------------------------------------------
void CPrediction::CheckMovingGround( CBasePlayer *player, double frametime )
{
	CBaseEntity	    *groundentity;

	if ( player->GetFlags() & FL_ONGROUND )
	{
		groundentity = player->GetGroundEntity();
		if ( groundentity && ( groundentity->GetFlags() & FL_CONVEYOR) )
		{
			Vector vecNewVelocity;
			groundentity->GetGroundVelocityToApply( vecNewVelocity );
			if ( player->GetFlags() & FL_BASEVELOCITY )
			{
				vecNewVelocity += player->GetBaseVelocity();
			}
			player->SetBaseVelocity( vecNewVelocity );
			player->AddFlag( FL_BASEVELOCITY );
		}
	}

	if ( !( player->GetFlags() & FL_BASEVELOCITY ) )
	{
		// Apply momentum (add in half of the previous frame of velocity first)
		player->ApplyAbsVelocityImpulse( (1.0 + ( frametime * 0.5 )) * player->GetBaseVelocity() );
		player->SetBaseVelocity( vec3_origin );
	}

	player->RemoveFlag( FL_BASEVELOCITY );
}

//-----------------------------------------------------------------------------
// Purpose: Predicts a single movement command for player
// Input  : *moveHelper - 
//			*player - 
//			*u - 
//-----------------------------------------------------------------------------
void CPrediction::RunCommand( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	CVProfScope _( "CPrediction::RunCommand", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	StartCommand( player, ucmd );

	// Set globals appropriately
	gpGlobals->curtime		= player->m_nTickBase * TICK_INTERVAL;
	gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;

	// Add and subtract buttons we're forcing on the player
	ucmd->buttons |= player->m_afButtonForced;
	// ucmd->buttons &= ~player->m_afButtonDisabled; // MAY WANT TO DO THIS LATER!!!

	g_pGameMovement->StartTrackPredictionErrors( player );

// TODO
// TODO:  Check for impulse predicted?

	// Do weapon selection
	if ( ucmd->weaponselect != 0 )
	{
		CBaseCombatWeapon *weapon = ToBaseCombatWeapon( CBaseEntity::Instance( ucmd->weaponselect ) );
		if ( weapon )
		{
			player->SelectItem( weapon->GetName(), ucmd->weaponsubtype );
		}
	}

	// Latch in impulse.
	if ( ucmd->impulse )
	{
		// Discard impulse commands unless the vehicle allows them.
		// FIXME: UsingStandardWeapons seems like a bad filter for this. 
		// The flashlight is an impulse command, for example.
		{
			player->m_nImpulse = ucmd->impulse;
		}
	}

	// Get button states
	player->UpdateButtonState( ucmd->buttons );

// TODO
	CheckMovingGround( player, gpGlobals->frametime );

// TODO
//	g_pMoveData->m_vecOldAngles = player->pl.v_angle;

	// Copy from command to player unless game .dll has set angle using fixangle
	// if ( !player->pl.fixangle )
	{
		player->SetLocalViewAngles( ucmd->viewangles );
	}

	// Call standard client pre-think
	RunPreThink( player );

	// Call Think if one is set
	RunThink( player, TICK_INTERVAL );

	// Setup input.
	{
	
		SetupMove( player, ucmd, moveHelper, g_pMoveData );
	}

	{
		CVProfScope _( "CPrediction::ProcessMovement", 0, "CPrediction::ProcessMovement", false, BUDGETFLAG_OTHER );

		// RUN MOVEMENT
		{
			contract_assert( g_pGameMovement );
			g_pGameMovement->ProcessMovement( player, g_pMoveData );
		}
	}

	FinishMove( player, ucmd, g_pMoveData );

	// Let server invoke any needed impact functions
	{ CVProfScope _( "moveHelper->ProcessImpacts(cl)", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	moveHelper->ProcessImpacts();
	};

	RunPostThink( player );

	g_pGameMovement->FinishTrackPredictionErrors( player );

	FinishCommand( player );
	g_pGameMovement->Reset();  // fixes a crash: when loading highlights twice or after previously loaded map, there was a dirty player pointer in game movement

	if( !m_bEnginePaused && gpGlobals->frametime > 0 )
	{
		player->m_nTickBase++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: In the forward direction, creates rays straight down and determines the
//  height of the 'floor' hit for each forward test.  Then, if the samples show that the
//  player is about to enter an up/down slope, sets *idealpitch to look up or down that slope
//  as appropriate
//-----------------------------------------------------------------------------
void CPrediction::SetIdealPitch ( int nSlot, CBasePlayer *player, const Vector& origin, const QAngle& angles, const Vector& viewheight )
{
	Vector	forward;
	Vector	top, bottom;
	float	floor_height[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;
	trace_t tr;

	if ( player->GetGroundEntity() == nullptr )
		return;
	
	// Don't do this on the 360..

	AngleVectors( angles, &forward );
	forward[2] = 0;

	MDLCACHE_CRITICAL_SECTION();

	// Now move forward by 36, 48, 60, etc. units from the eye position and drop lines straight down
	//  160 or so units to see what's below
	for (i=0 ; i<MAX_FORWARD ; i++)
	{
		VectorMA( origin, (i+3)*12, forward, top );
		
		top[2] += viewheight[ 2 ];

		VectorCopy( top, bottom );

		bottom[2] -= 160;

		UTIL_TraceLine( top, bottom, MASK_SOLID, nullptr, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );

		// looking at a wall, leave ideal the way it was
		if ( tr.allsolid )
			return;	

		// near a dropoff/ledge
		if ( tr.fraction == 1 )
			return;	
		
		floor_height[i] = top[2] + tr.fraction*( bottom[2] - top[2] );
	}
	
	dir = 0;
	steps = 0;
	for (j=1 ; j<i ; j++)
	{
		step = floor_height[j] - floor_height[j-1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && ( step-dir > ON_EPSILON || step-dir < -ON_EPSILON ) )
			return;		// mixed changes

		steps++;	
		dir = step;
	}
	
	Split_t &split = m_Split[ nSlot ];
	if (!dir)
	{
		split.m_flIdealPitch = 0;
		return;
	}
	
	if (steps < 2)
		return;
	split.m_flIdealPitch = -dir * cl_idealpitchscale.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Walk backward through predictables looking for ClientCreated entities
//  such as projectiles which were
// 1) not actually ack'd by the server or
// 2) were ack'd and made dormant and can now safely be removed
// Input  : last_command_packet - 
//-----------------------------------------------------------------------------
void CPrediction::RemoveStalePredictedEntities( int nSlot, int sequence_number )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::RestoreOriginalEntityState( int nSlot )
{
	CVProfScope _( "CPrediction::RestoreOriginalEntityState", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "restore" );

	contract_assert( CBaseEntity::IsAbsRecomputationsEnabled() );

	// Transfer intermediate data from other predictables
	int c = GetPredictables( nSlot )->GetPredictableCount();
	for ( int i = 0; i < c; ++i )
	{
		CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent || !ent->GetPredictable() )
			continue;

		ent->RestoreData( "RestoreOriginalEntityState", CBaseEntity::SLOT_ORIGINALDATA, PC_EVERYTHING );
		ent->OnPostRestoreData();
	}
}

void CPrediction::ResetSimulationTick()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	// Make sure simulation occurs at most once per entity per usercmd
	for (int  i = 0; i < GetPredictables( nSlot )->GetPredictableCount(); i++ )
	{
		CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
		if ( entity && entity->GetSplitUserPlayerPredictionSlot() == nSlot )
		{
			entity->m_nSimulationTick = -1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : current_command - 
//			curtime - 
//			*cmd - 
//			*tcmd - 
//			*localPlayer - 
//-----------------------------------------------------------------------------
void CPrediction::RunSimulation( int current_command, float curtime, CUserCmd *cmd, CBasePlayer *localPlayer )
{
	CVProfScope _( "CPrediction::RunSimulation", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	contract_assert( localPlayer );
	CCommandContext *ctx = localPlayer->GetCommandContext();
	contract_assert( ctx );
	
	ctx->needsprocessing = true;
	ctx->cmd = *cmd;
	ctx->command_number = current_command;

	IPredictionSystem::SuppressEvents( !IsFirstTimePredicted() );

	ResetSimulationTick();
	
	// Don't used cached numpredictables since entities can be created mid-prediction by the player
	for ( int i = 0; i < GetPredictables( nSlot )->GetPredictableCount(); i++ )
	{
		// Always reset
		gpGlobals->curtime		= curtime;
		gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;

		CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );

		if ( !entity )
			continue;

		if ( entity->GetSplitUserPlayerPredictionSlot() != nSlot )
			continue;

		bool islocal = ( localPlayer == entity ) ? true : false;

		// Local player simulates first, if this assert fires then the predictables list isn't sorted 
		//  correctly (or we started predicting CWorld???)
		if ( islocal && !IsValidSplitScreenSlot( i ) )
			continue;

		// Player can't be this so cull other entities here
		if ( entity->GetFlags() & FL_STATICPROP )
		{
			continue;
		}

		// Player is not actually in the m_SimulatedByThisPlayer list, of course
		if ( entity->IsPlayerSimulated() )
		{
			continue;
		}

		if ( AddDataChangeEvent( entity, DATA_UPDATE_DATATABLE_CHANGED, &entity->m_DataChangeEventRef ) )
		{
			entity->OnPreDataChanged( DATA_UPDATE_DATATABLE_CHANGED );
		}

		// Certain entities can be created locally and if so created, should be 
		//  simulated until a network update arrives
		if ( entity->IsClientCreated() )
		{
			// Only simulate these on new usercmds
			if ( !IsFirstTimePredicted() )
				continue;

			entity->PhysicsSimulate();
		}
		else
		{
			entity->PhysicsSimulate();
		}

		// Don't update last networked data here!!!
		entity->OnLatchInterpolatedVariables( LATCH_SIMULATION_VAR | LATCH_ANIMATION_VAR | INTERPOLATE_OMIT_UPDATE_LAST_NETWORKED );
	}

	// Only call the physics update for the entire physics system if we support predicted physics.
	if ( cl_predictphysics.GetBool() )
	{
		PhysicsSimulate();
	}
	
	// Always reset after running command
	IPredictionSystem::SuppressEvents( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrediction::Untouch( int nSlot )
{
	int numpredictables = GetPredictables( nSlot )->GetPredictableCount();

	// Loop through all entities again, checking their untouch if flagged to do so
	int i;
	for ( i = 0; i < numpredictables; i++ )
	{
		CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
		if ( !entity )
			continue;

		if ( !entity->GetCheckUntouch() )
			continue;

		entity->PhysicsCheckForEntityUntouch();
	}
}

void CPrediction::StorePredictionResults( int nSlot, int predicted_frame )
{
	CVProfScope _( "CPrediction::StorePredictionResults", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "save" );

	int c = GetPredictables( nSlot )->GetPredictableCount();

	// Now save off all of the results
	for ( int i = 0; i < c; ++i )
	{
		CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
		if ( !entity )
			continue;

		// Certain entities can be created locally and if so created, should be 
		//  simulated until a network update arrives
		if ( !entity->GetPredictable() )
			continue;

		// FIXME: The lack of this call inexplicably actually creates prediction errors
		InvalidateEFlagsRecursive( entity, EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSANGVELOCITY );
		entity->SaveData( "StorePredictionResults", predicted_frame, PC_EVERYTHING );
		
		//if we're keeping first frame results, copy them now
		if( m_Split[ nSlot ].m_bFirstTimePredicted && (entity->m_pIntermediateData_FirstPredicted[0] != nullptr) )
		{
			entity->m_nIntermediateData_FirstPredictedShiftMarker = predicted_frame + 1;
			memcpy( entity->m_pIntermediateData_FirstPredicted[predicted_frame + 1], entity->m_pIntermediateData[predicted_frame], entity->GetPredDescMap()->m_nPackedSize );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove - 
//			previous_last_slot - 
//-----------------------------------------------------------------------------
void CPrediction::ShiftIntermediateDataForward( int nSlot, int slots_to_remove, int number_of_commands_run )
{
	CVProfScope _( "CPrediction::ShiftIntermediateDataForward", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "shift" );

	if ( !CBasePlayer::HasAnyLocalPlayer() )
		return;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	int c = GetPredictables( nSlot )->GetPredictableCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent )
			continue;

		if ( !ent->GetPredictable() )
			continue;

		ent->ShiftIntermediateDataForward( slots_to_remove, number_of_commands_run );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slots_to_remove - 
//-----------------------------------------------------------------------------
void CPrediction::ShiftFirstPredictedIntermediateDataForward( int nSlot, int slots_to_remove )
{
	if( (slots_to_remove == 0) || (slots_to_remove >= MULTIPLAYER_BACKUP)  )
		return;

	CVProfScope _( "CPrediction::ShiftFirstPredictedIntermediateDataForward", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "shift" );

	if ( !CBasePlayer::HasAnyLocalPlayer() )
		return;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	int c = GetPredictables( nSlot )->GetPredictableCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent )
			continue;

		if ( !ent->GetPredictable() )
			continue;

		ent->ShiftFirstPredictedIntermediateDataForward( slots_to_remove );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : predicted_frame - 
//-----------------------------------------------------------------------------
void CPrediction::RestoreEntityToPredictedFrame( int nSlot, int predicted_frame )
{
	CVProfScope _( "CPrediction::RestoreEntityToPredictedFrame", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	PREDICTION_TRACKVALUECHANGESCOPE( "restoretopred" );

	if ( !CBasePlayer::GetLocalPlayer( nSlot ) )
		return;

	// Don't screw up memory of current player from history buffers if not filling in history buffers
	//  during prediction!!!
	if ( !cl_predict->GetInt() )
		return;

	int c = GetPredictables( nSlot )->GetPredictableCount();
	int i;
	for ( i = 0; i < c; i++ )
	{
		CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
		if ( !ent )
			continue;

		if ( !ent->GetPredictable() )
			continue;

		ent->RestoreData( "RestoreEntityToPredictedFrame", predicted_frame, PC_EVERYTHING );
		ent->OnPostRestoreData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Computes starting destination for intermediate prediction data results and
//  does any fixups required by network optimization
// Input  : received_new_world_update - 
//			incoming_acknowledged - 
// Output : int
//-----------------------------------------------------------------------------
int CPrediction::ComputeFirstCommandToExecute( int nSlot, bool received_new_world_update, int incoming_acknowledged, int outgoing_command )
{
	int destination_slot = 1;
	int skipahead = 0;

	Split_t &split = m_Split[ nSlot ];

	// If we didn't receive a new update ( or we received an update that didn't ack any new CUserCmds -- 
	//  so for the player it should be just like receiving no update ), just jump right up to the very 
	//  last command we created for this very frame since we probably wouldn't have had any errors without 
	//  being notified by the server of such a case.
	// NOTE:  received_new_world_update only gets set to false if cl_pred_optimize >= 1
	if ( !received_new_world_update || !split.m_nServerCommandsAcknowledged )
	{
		if( !split.m_bPerformedTickShift )
		{
			// this is where we would normally start
			int start = incoming_acknowledged + 1;
			// outgoing_command is where we really want to start
			skipahead = std::max( 0, ( outgoing_command - start ) );
			// Don't start past the last predicted command, though, or we'll get prediction errors
			skipahead = std::min( skipahead, split.m_nCommandsPredicted );

			// Always restore since otherwise we might start prediction using an "interpolated" value instead of a purely predicted value
			RestoreEntityToPredictedFrame( nSlot, skipahead - 1 );

			//Msg( "%i/%i no world, skip to %i restore from slot %i\n", 
			//	gpGlobals->framecount,
			//	gpGlobals->tickcount,
			//	skipahead,
			//	skipahead - 1 );
		}
	}
	else
	{
		// Otherwise, there is a second optimization, wherein if we did receive an update, but no
		//  values differed (or were outside their epsilon) and the server actually acknowledged running
		//  one or more commands, then we can revert the entity to the predicted state from last frame, 
		//  shift the # of commands worth of intermediate state off of front the intermediate state array, and
		//  only predict the usercmd from the latest render frame.
		if ( cl_pred_optimize.GetInt() >= 2 && 
			!split.m_bPreviousAckHadErrors && 
			split.m_nCommandsPredicted > 0 && 
			split.m_nServerCommandsAcknowledged <= split.m_nCommandsPredicted &&
			!split.m_bPerformedTickShift )
		{
			// Copy all of the previously predicted data back into entity so we can skip repredicting it
			// This is the final slot that we previously predicted
			RestoreEntityToPredictedFrame( nSlot, split.m_nCommandsPredicted - 1 );

			// Shift intermediate state blocks down by # of commands ack'd
			ShiftIntermediateDataForward( nSlot, split.m_nServerCommandsAcknowledged, split.m_nCommandsPredicted );
			
			// Only predict new commands (note, this should be the same number that we could compute
			//  above based on outgoing_command - incoming_acknowledged - 1
			skipahead = ( split.m_nCommandsPredicted - split.m_nServerCommandsAcknowledged );

			//Msg( "%i/%i optimize2, skip to %i restore from slot %i\n", 
			//	gpGlobals->framecount,
			//	gpGlobals->tickcount,
			//	skipahead,
			//	split.m_nCommandsPredicted - 1 );
		}
		else
		{
			if( split.m_bPreviousAckHadErrors )
			{
				int count = GetPredictables( nSlot )->GetPredictableCount();
				for ( int i = 0; i < count; i++ )
				{
					CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
					if ( !ent )
						continue;

					ent->m_bEverHadPredictionErrorsForThisCommand = true; //this bool will get pulled forward until a new command is predicted for the first time
				}
			}

			if ( ( split.m_bPreviousAckHadErrors && cl_pred_doresetlatch.GetBool() ) || 
				cl_pred_doresetlatch.GetInt() == 2 )
			{
				// Both players should have == time base, etc.
				CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer( nSlot );
				
				// If an entity gets a prediction error, then we want to clear out its interpolated variables
				// so we don't mix different samples at the same timestamps. We subtract 1 tick interval here because
				// if we don't, we'll have 3 interpolation entries with the same timestamp as this predicted
				// frame, so we won't be able to interpolate (which leads to jerky movement in the player when
				// ANY entity like your gun gets a prediction error).
				float flPrev = gpGlobals->curtime;
				gpGlobals->curtime = pLocalPlayer->GetTimeBase() - TICK_INTERVAL;

				if( split.m_bPreviousAckErrorTriggersFullLatchReset || (cl_pred_doresetlatch.GetInt() == 2) )
				{				
					for ( int i = 0; i < GetPredictables( nSlot )->GetPredictableCount(); i++ )
					{
						CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );
						if ( entity )
						{
							entity->ResetLatched();
						}
					}
				}
				else
				{
					//individual latch resets
					for ( int i = 0; i < std::ssize( split.m_EntsWithPredictionErrorsInLastAck ); i++ )
					{
						CBaseEntity *entity = split.m_EntsWithPredictionErrorsInLastAck[i];
						if( entity )
						{
							//ensure it's still in our predictable list
							for ( int j = 0; j < GetPredictables( nSlot )->GetPredictableCount(); j++ )
							{
								if( entity == GetPredictables( nSlot )->GetPredictable( j ) )
								{
									entity->ResetLatched();
									break;
								}
							}
						}
					}
				}

				gpGlobals->curtime = flPrev;
			}
		}
	}

	ShiftFirstPredictedIntermediateDataForward( nSlot, split.m_nServerCommandsAcknowledged );

	destination_slot += skipahead;

	// Always reset these values now that we handled them
	split.m_nCommandsPredicted	= 0;
	split.m_bPreviousAckHadErrors = false;
	split.m_nServerCommandsAcknowledged = 0;
	split.m_bPerformedTickShift = false;
	return destination_slot;
}

//-----------------------------------------------------------------------------
// Actually does the prediction work, returns false if an error occurred
//-----------------------------------------------------------------------------
bool CPrediction::PerformPrediction( int nSlot, CBasePlayer *localPlayer, bool received_new_world_update, int incoming_acknowledged, int outgoing_command )
{
	MDLCACHE_CRITICAL_SECTION();
	CVProfScope _( "CPrediction::PerformPrediction", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	contract_assert( localPlayer );
	// This makes sure that we are allowed to sample the world when it may not be ready to be sampled
	contract_assert( CBaseEntity::IsAbsQueriesValid() );
	contract_assert( CBaseEntity::IsAbsRecomputationsEnabled() );

	// Start at command after last one server has processed and 
	//  go until we get to targettime or we run out of new commands
	int i = ComputeFirstCommandToExecute( nSlot, received_new_world_update, incoming_acknowledged, outgoing_command );

	contract_assert( i >= 1 );

	// This is a hack to get the CTriggerAutoGameMovement auto duck triggers to correctly deal with prediction.
	// Here we just untouch any triggers the player was touching (since we might have teleported the origin 
	// backward from it's previous position) and then re-touch any triggers it's currently in
	localPlayer->SetCheckUntouch( true );
	localPlayer->PhysicsCheckForEntityUntouch();
	localPlayer->PhysicsTouchTriggers();

	// undo interpolation changes for entities we stand on
	CBaseEntity *ground = localPlayer->GetGroundEntity();

	while ( ground && ground->entindex() > 0 )
	{
		ground->MoveToLastReceivedPosition();
		ground = ground->GetMoveParent();
	}

	Split_t &split = m_Split[ nSlot ];
	
	physenv->DoneReferencingPreviousCommands( incoming_acknowledged - 1 );

	bool bTooMany = outgoing_command - incoming_acknowledged >= MULTIPLAYER_BACKUP;

	while ( !bTooMany )
	{
		// Incoming_acknowledged is the last usercmd the server acknowledged having acted upon
		int current_command		= incoming_acknowledged + i;

		// We've caught up to the current command.
		if ( current_command > outgoing_command )
			break;

		CUserCmd *cmd = input->GetUserCmd( nSlot, current_command );
		if ( !cmd )
		{
			bTooMany = true;
			break;	
		}

		contract_assert( i < MULTIPLAYER_BACKUP );

		// Is this the first time predicting this
		split.m_bFirstTimePredicted = !cmd->hasbeenpredicted;

		// Set globals appropriately
		float curtime		= ( localPlayer->m_nTickBase ) * TICK_INTERVAL;

		if( physenv->IsPredicted() )
		{
			physenv->SetPredictionCommandNum( current_command );

			if( (m_Split[ nSlot ].m_nCommandsPredicted == 0) && (i == 1) )
			{
				if( !m_Split[ nSlot ].m_bFirstTimePredicted )
				{
					for ( int j = 0; j < GetPredictables( nSlot )->GetPredictableCount(); j++ )
					{
						CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( j );

						if( entity->m_bEverHadPredictionErrorsForThisCommand )
						{
							// Always reset
							gpGlobals->curtime		= curtime;
							gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;				

							const byte *predictedFrame = (const byte *)entity->GetFirstPredictedFrame( 0 );
							entity->VPhysicsCompensateForPredictionErrors( predictedFrame );
						}
					}
				}
				//else - if lag is so low that we only predict every command once, do we need to keep around old data to base our corrections on?
			}
		}

		if( !cmd->hasbeenpredicted )
		{
			int count = GetPredictables( nSlot )->GetPredictableCount();
			for ( int i = 0; i < count; ++i )
			{
				CBaseEntity *entity = GetPredictables( nSlot )->GetPredictable( i );

				if( entity )
				{
					entity->m_bEverHadPredictionErrorsForThisCommand = false;
				}
			}
		}

		RunSimulation( current_command, curtime, cmd, localPlayer );

		gpGlobals->curtime		= curtime;
		gpGlobals->frametime	= m_bEnginePaused ? 0 : TICK_INTERVAL;

		// Call untouch on any entities no longer predicted to be touching
		Untouch( nSlot );

		// Store intermediate data into appropriate slot
		StorePredictionResults( nSlot, i - 1 ); // Note that I starts at 1

		split.m_nCommandsPredicted = i;

		if ( current_command == outgoing_command )
		{
			localPlayer->m_nFinalPredictedTick = localPlayer->m_nTickBase;
		}

		// Mark that we issued any needed sounds, of not done already
		cmd->hasbeenpredicted = true;

		// Copy the state over.
		i++;
	}

	// Somehow we looped past the end of the list (severe lag), don't predict at all
	return !bTooMany;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startframe - 
//			validframe - 
//			incoming_acknowledged - 
//			outgoing_command - 
//-----------------------------------------------------------------------------
void CPrediction::Update( int startframe, bool validframe, 
						 int incoming_acknowledged, int outgoing_command )
{
	CVProfScope _( "CPrediction::Update", 0, VPROF_BUDGETGROUP_PREDICTION, false, BUDGETFLAG_OTHER );

	m_bEnginePaused = engine->IsPaused();

	bool received_new_world_update = true;

	// HACK!
	float flTimeStamp = engine->GetLastTimeStamp();
	bool bTimeStampChanged = m_flLastServerWorldTimeStamp != flTimeStamp;
	m_flLastServerWorldTimeStamp = flTimeStamp;

	// Still starting at same frame, so make sure we don't do extra prediction ,etc.
	if ( ( m_nPreviousStartFrame == startframe ) && 
		cl_pred_optimize.GetBool() &&
		cl_predict->GetInt() && bTimeStampChanged )
	{
		received_new_world_update = false;
	}

	m_nPreviousStartFrame = startframe;

	// Save off current timer values, etc.
	m_SavedVars = *gpGlobals;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );
		_Update( nSlot, received_new_world_update, validframe, incoming_acknowledged, outgoing_command );
	}

	// Restore current timer values, etc.
	*gpGlobals = m_SavedVars;
}

//-----------------------------------------------------------------------------
// Do the dirty deed of predicting the local player
//-----------------------------------------------------------------------------
void CPrediction::_Update( int nSlot, bool received_new_world_update, bool validframe, 
						 int incoming_acknowledged, int outgoing_command )
{

	QAngle viewangles;

	CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer( nSlot );
	if ( !localPlayer )
		return;

	m_Split[nSlot].m_nLastCommandAcknowledged = incoming_acknowledged;

	// Always using current view angles no matter what
	// NOTE: ViewAngles are always interpreted as being *relative* to the player
	engine->GetViewAngles( viewangles );
	localPlayer->SetLocalAngles( viewangles );

	if ( !validframe )
	{
		return;
	}

	// If we are not doing prediction, copy authoritative value into velocity and angle.
	if ( !cl_predict->GetInt() )
	{
		// When not predicting, we at least must make sure the player
		// view angles match the view angles...
		localPlayer->SetLocalViewAngles( viewangles );
		return;
	}

	// This is cheesy, but if we have entities that are parented to attachments on other entities, then 
	// it'll wind up needing to get a bone transform.
	{
		CBaseAnimating::AutoAllowBoneAccess boneaccess( true, true );

		// Invalidate bone cache on predictables
		int c = GetPredictables( nSlot )->GetPredictableCount();
		for ( int i = 0; i < c; ++i )
		{
			CBaseEntity *ent = GetPredictables( nSlot )->GetPredictable( i );
			if ( !ent || 
				 !ent->GetBaseAnimating() || 
				 !ent->GetPredictable() )
				continue;
			static_cast< CBaseAnimating * >( ent )->InvalidateBoneCache();
		}

		// Remove any purely client predicted entities that were left "dangling" because the 
		//  server didn't acknowledge them or which can now safely be removed
		RemoveStalePredictedEntities( nSlot, incoming_acknowledged );
		// Restore objects back to "pristine" state from last network/world state update
		if ( received_new_world_update )
		{
			RestoreOriginalEntityState( nSlot );
		}

		m_bInPrediction = true;
		bool bValid = PerformPrediction( nSlot, localPlayer, received_new_world_update, incoming_acknowledged, outgoing_command );
		m_bInPrediction = false;
		if ( !bValid )
		{
			return;
		}
	}

	// Overwrite predicted angles with the actual view angles
	localPlayer->SetLocalAngles( viewangles );

	// This allows us to sample the world when it may not be ready to be sampled
	contract_assert( CBaseEntity::IsAbsQueriesValid() );
	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrediction::IsFirstTimePredicted( void ) const
{
	return m_Split[ GET_ACTIVE_SPLITSCREEN_SLOT() ].m_bFirstTimePredicted;
}

//-----------------------------------------------------------------------------
// Purpose: For verifying/fixing operations that don't save/load in a datatable very well
// Output : Returns the how many commands the server has processed and sent results for
//-----------------------------------------------------------------------------
int CPrediction::GetLastAcknowledgedCommandNumber( void ) const
{
	return m_Split[ GET_ACTIVE_SPLITSCREEN_SLOT() ].m_nLastCommandAcknowledged;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CPrediction::GetViewOrigin( Vector& org )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
	{
		org.Init();
	}
	else 
	{
		org = player->GetLocalOrigin();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : org - 
//-----------------------------------------------------------------------------
void CPrediction::SetViewOrigin( Vector& org )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	player->SetLocalOrigin( org );
	player->m_vecNetworkOrigin = org;

	player->m_iv_vecOrigin.Reset( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::GetViewAngles( QAngle& ang )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
	{
		ang.Init();
	}
	else 
	{
		ang = player->GetLocalAngles();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::SetViewAngles( QAngle& ang )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	player->SetViewAngles( ang );
	player->m_iv_angRotation.Reset( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::GetLocalViewAngles( QAngle& ang )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
	{
		ang.Init();
	}
	else 
	{
		ang = player->pl.v_angle;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void CPrediction::SetLocalViewAngles( QAngle& ang )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	player->SetLocalViewAngles( ang );
}

//-----------------------------------------------------------------------------
// Purpose: For determining that predicted creation entities are un-acked and should
//  be deleted
// Output : int
//-----------------------------------------------------------------------------
int CPrediction::GetIncomingPacketNumber( void ) const
{
	return m_nIncomingPacketNumber;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrediction::InPrediction( void ) const
{
	return m_bInPrediction;
}

float CPrediction::GetSavedTime() const
{
	return m_SavedVars.curtime;
}

float CPrediction::GetIdealPitch( int nSlot) const
{
	if ( nSlot == -1 )
	{
		contract_assert( 0 );
		return 0.0f;
	}
	return m_Split[ nSlot ].m_flIdealPitch;
}

class CPredictionCopy;
class CBaseEntity;

using namespace ::ks::reflect;

ks::object_pool< optimized_datamap_t >	g_OptimizedDataMapPool( 20, ks::fixed_pool::grow::slow );

// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
static const char *g_FieldTypes[ FIELD_TYPECOUNT ] = 
{
	"FIELD_VOID",			// FIELD_VOID
	"FIELD_FLOAT",			// FIELD_FLOAT
	"FIELD_STRING",			// FIELD_STRING
	"FIELD_VECTOR",			// FIELD_VECTOR
	"FIELD_QUATERNION",		// FIELD_QUATERNION
	"FIELD_INTEGER",		// FIELD_INTEGER
	"FIELD_BOOLEAN",		// FIELD_BOOLEAN
	"FIELD_SHORT",			// FIELD_SHORT
	"FIELD_CHARACTER",		// FIELD_CHARACTER
	"FIELD_COLOR32",		// FIELD_COLOR32
	"FIELD_EMBEDDED",		// FIELD_EMBEDDED	(handled specially)
	"FIELD_CUSTOM",			// FIELD_CUSTOM		(handled specially)
	"FIELD_CLASSPTR",		// FIELD_CLASSPTR
	"FIELD_EHANDLE",		// FIELD_EHANDLE
	"FIELD_EDICT",			// FIELD_EDICT
	"FIELD_POSITION_VECTOR",// FIELD_POSITION_VECTOR
	"FIELD_TIME",			// FIELD_TIME
	"FIELD_TICK",			// FIELD_TICK
	"FIELD_MODELNAME",		// FIELD_MODELNAME
	"FIELD_SOUNDNAME",		// FIELD_SOUNDNAME
	"FIELD_INPUT",			// FIELD_INPUT		(uses custom type)
	"FIELD_FUNCTION",		// FIELD_FUNCTION
	"FIELD_VMATRIX",			
	"FIELD_VMATRIX_WORLDSPACE",
	"FIELD_MATRIX3X4_WORLDSPACE",
	"FIELD_INTERVAL"		// FIELD_INTERVAL
	"FIELD_MODELINDEX"		// FIELD_MODELINDEX
};

static int g_FieldSizes[FIELD_TYPECOUNT] = 
{
	0,					// FIELD_VOID
	sizeof(float),		// FIELD_FLOAT
	sizeof(int),		// FIELD_STRING
	sizeof(Vector),		// FIELD_VECTOR
	sizeof(Quaternion),	// FIELD_QUATERNION
	sizeof(int),		// FIELD_INTEGER
	sizeof(char),		// FIELD_BOOLEAN
	sizeof(short),		// FIELD_SHORT
	sizeof(char),		// FIELD_CHARACTER
	sizeof(color32),	// FIELD_COLOR32
	sizeof(int),		// FIELD_EMBEDDED	(handled specially)
	sizeof(int),		// FIELD_CUSTOM		(handled specially)

	//---------------------------------

	sizeof(int),		// FIELD_CLASSPTR
	sizeof(EHANDLE),	// FIELD_EHANDLE
	sizeof(int),		// FIELD_EDICT

	sizeof(Vector),		// FIELD_POSITION_VECTOR
	sizeof(float),		// FIELD_TIME
	sizeof(int),		// FIELD_TICK
	sizeof(int),		// FIELD_MODELNAME
	sizeof(int),		// FIELD_SOUNDNAME

	sizeof(int),		// FIELD_INPUT		(uses custom type)
	sizeof(int *),		// FIELD_FUNCTION
	sizeof(VMatrix),	// FIELD_VMATRIX
	sizeof(VMatrix),	// FIELD_VMATRIX_WORLDSPACE
	sizeof(matrix3x4_t),// FIELD_MATRIX3X4_WORLDSPACE	// NOTE: Use array(FIELD_FLOAT, 12) for matrix3x4_t NOT in worldspace
	sizeof(interval_t), // FIELD_INTERVAL
	sizeof(int),		// FIELD_MODELINDEX
};

#define PREDICTIONCOPY_APPLY( func, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize )		\
	switch( nFieldType )																			\
	{																								\
	case FIELD_EMBEDDED:																			\
		{																							\
			Error( "FIELD_EMBEDDED in flat list!!!" );												\
		}																							\
		break;																						\
	case FIELD_FLOAT:																				\
		func( pCurrentMap, pField, (float *)pOutputData, (const float *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_STRING:																				\
		func( pCurrentMap, pField, (char *)pOutputData, (const char *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_VECTOR:																				\
		func( pCurrentMap, pField, (Vector *)pOutputData, (const Vector *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_QUATERNION:																			\
		func( pCurrentMap, pField, (Quaternion *)pOutputData, (const Quaternion *)pInputData, fieldSize );\
		break;																						\
	case FIELD_COLOR32:																				\
		func( pCurrentMap, pField, (color32 *)pOutputData, (const color32 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_BOOLEAN:																				\
		func( pCurrentMap, pField, (bool *)pOutputData, (const bool *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_INTEGER:																				\
		func( pCurrentMap, pField, (int *)pOutputData, (const int *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_SHORT:																				\
		func( pCurrentMap, pField, (short *)pOutputData, (const short *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_CHARACTER:																			\
		func( pCurrentMap, pField, (uint8 *)pOutputData, (const uint8 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_EHANDLE:																				\
		func( pCurrentMap, pField, (EHANDLE *)pOutputData, (const EHANDLE *)pInputData, fieldSize );\
		break;																						\
	default:																						\
		break;																						\
	}

#define PREDICTIONCOPY_APPLY_NOEMBEDDED( func, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize )		\
	switch( nFieldType )																			\
	{																								\
	case FIELD_FLOAT:																				\
		func( pCurrentMap, pField, (float *)pOutputData, (const float *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_STRING:																				\
		func( pCurrentMap, pField, (char *)pOutputData, (const char *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_VECTOR:																				\
		func( pCurrentMap, pField, (Vector *)pOutputData, (const Vector *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_QUATERNION:																			\
		func( pCurrentMap, pField, (Quaternion *)pOutputData, (const Quaternion *)pInputData, fieldSize );\
		break;																						\
	case FIELD_COLOR32:																				\
		func( pCurrentMap, pField, (color32 *)pOutputData, (const color32 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_BOOLEAN:																				\
		func( pCurrentMap, pField, (bool *)pOutputData, (const bool *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_INTEGER:																				\
		func( pCurrentMap, pField, (int *)pOutputData, (const int *)pInputData, fieldSize );		\
		break;																						\
	case FIELD_SHORT:																				\
		func( pCurrentMap, pField, (short *)pOutputData, (const short *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_CHARACTER:																			\
		func( pCurrentMap, pField, (uint8 *)pOutputData, (const uint8 *)pInputData, fieldSize );	\
		break;																						\
	case FIELD_EHANDLE:																				\
		func( pCurrentMap, pField, (EHANDLE *)pOutputData, (const EHANDLE *)pInputData, fieldSize );\
		break;																						\
	default:																						\
		break;																						\
	}

CPredictionCopy::CPredictionCopy( int type, byte *dest, bool dest_packed, const byte *src, bool src_packed, 
	optype_t opType, FN_FIELD_COMPARE func /*= NULL*/ )
{
	m_OpType			= opType;
	m_nType				= type;
	m_pDest				= dest;
	m_pSrc				= src;
	m_nDestOffsetIndex	= dest_packed ? TD_OFFSET_PACKED : TD_OFFSET_NORMAL;
	m_nSrcOffsetIndex	= src_packed ? TD_OFFSET_PACKED : TD_OFFSET_NORMAL;

	m_nErrorCount		= 0;
	m_nEntIndex			= -1;

	m_pWatchField		= nullptr;
	m_FieldCompareFunc	= func;
}

template< class T >
inline void CPredictionCopy::CopyField( difftype_t difftype, T *outvalue, const T *invalue, int count )
{
	for ( int i = 0; i < count; ++i )
	{
		outvalue[ i ] = invalue[ i ];
	}
}

// specialized for strings
template<>
inline void CPredictionCopy::CopyField( difftype_t difftype, char *outvalue, const char *invalue, int count )
{
	V_strcpy( outvalue, invalue );
}

template< class T >
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const T *outvalue, int count )
{
	contract_assert( 0 );
}

// Short
template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const short *outvalue, int count )
{
	WatchMsg( pField, "short (%i)", (int)(outvalue[0]) );
}

// Int
template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const int *outvalue, int count )
{
	bool described = false;
	if ( pField->flags & FTYPEDESC_MODELINDEX )
	{
		int modelindex = outvalue[0];
		model_t const *m = modelinfo->GetModel( modelindex );
		if ( m )
		{
			described = true;
			char shortfile[ 512 ];
			shortfile[ 0 ] = 0;
			V_FileBase( modelinfo->GetModelName( m ), shortfile, sizeof( shortfile ) );

			WatchMsg( pField, "integer (%i->%s)", outvalue[0], shortfile );
		}
	}

	if ( !described )
	{
		WatchMsg( pField, "integer (%i)", outvalue[0] );
	}
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const bool *outvalue, int count )
{
	WatchMsg( pField, "bool (%s)", (outvalue[0]) ? "true" : "false" );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const float *outvalue, int count )
{
	WatchMsg( pField, "float (%f)", outvalue[ 0 ] );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const char *outstring, int count )
{
	WatchMsg( pField, "string (%s)", outstring );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Vector* outValue, int count )
{
	WatchMsg( pField, "vector (%f %f %f)", outValue[0].x, outValue[0].y, outValue[0].z );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Quaternion* outValue, int count )
{
	WatchMsg( pField, "quaternion (%f %f %f %f)", outValue[0].x, outValue[0].y, outValue[0].z, outValue[0].w );
}

template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const EHANDLE *outvalue, int count )
{
	CBaseEntity *ent = outvalue[0].Get();
	if ( ent )
	{
		const char *classname = ent->GetClassname();
		if ( !classname[0] )
		{
			classname = typeid( *ent ).name();
		}

		WatchMsg( pField, "EHandle (0x%p->%s)", (void *)outvalue[ 0 ], classname );
	}
	else
	{
		WatchMsg( pField, "EHandle (NULL)" );
	}
}

void CPredictionCopy::DumpWatchField( const typedescription_t *pField, const byte *outvalue, int count )
{
	switch ( pField->fieldType )
	{
	case FIELD_FLOAT:
		WatchField( pField, (const float *)outvalue, count );
		break;
	case FIELD_STRING:
		WatchField( pField, (const char *)outvalue, count );
		break;
	case FIELD_VECTOR:
		WatchField( pField, (const Vector *)outvalue, count );
		break;
	case FIELD_QUATERNION:
		WatchField( pField, (const Quaternion *)outvalue, count );
		break;
	case FIELD_COLOR32:
		WatchField( pField, (const color32 *)outvalue, count );
		break;
	case FIELD_BOOLEAN:
		WatchField( pField, (const bool *)outvalue, count );
		break;
	case FIELD_INTEGER:
		WatchField( pField, (const int *)outvalue, count );
		break;
	case FIELD_SHORT:
		WatchField( pField, (const short *)outvalue, count );
		break;
	case FIELD_CHARACTER:
		WatchField( pField, (const uint8 *)outvalue, count );
		break;
	case FIELD_EHANDLE:
		WatchField( pField, (const EHANDLE *)outvalue, count );
		break;
	default:
		break;		
	}
}

// color32
template<>
inline void CPredictionCopy::WatchField( const typedescription_t *pField, const color32 *outvalue, int count )
{
	WatchMsg( pField, "color32 (%d %d %d %d)", outvalue[0].r, outvalue[0].g, outvalue[0].b, outvalue[0].a );
}

inline bool QuaternionCompare( const Quaternion& q1, const Quaternion& q2 )
{
	for ( int i = 0; i < 4; ++i )
	{
		if ( q1[i] != q2[i] )
			return false;
	}
	return true;
}

template< class T >
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const T *outvalue, const T *invalue, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		if ( outvalue[ i ] == invalue[ i ] )
			continue;
		return DIFFERS;
	}
	return IDENTICAL;
}

// float uses tolerance
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const float *outvalue, const float *invalue, int count )
{
	difftype_t retval = IDENTICAL;

	float tolerance = pField->fieldTolerance;
	contract_assert( tolerance >= 0.0f );
	bool usetolerance = tolerance > 0.0f;

	if ( usetolerance )
	{
		for ( int i = 0; i < count; ++i )
		{
			float diff = fabs( outvalue[ i ] - invalue[ i ] );
			if ( diff <= tolerance )
			{
				retval = WITHINTOLERANCE;
				continue;
			}

			return DIFFERS;
		}
	}
	else
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( outvalue[ i ] == invalue[ i ] )
				continue;
			return DIFFERS;
		}
	}
	return retval;
}

// vector uses tolerance
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Vector *outvalue, const Vector *invalue, int count )
{
	difftype_t retval = IDENTICAL;

	float tolerance = pField->fieldTolerance;
	contract_assert( tolerance >= 0.0f );
	bool usetolerance = tolerance > 0.0f;

	if ( usetolerance )
	{
		for ( int i = 0; i < count; ++i )
		{
			Vector delta = outvalue[ i ] - invalue[ i ];

			if ( fabs( delta.x ) <= tolerance &&
				fabs( delta.y ) <= tolerance &&
				fabs( delta.z ) <= tolerance )
			{
				retval = WITHINTOLERANCE;
				continue;
			}

			return DIFFERS;
		}
	}
	else
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( outvalue[ i ] == invalue[ i ] )
				continue;
			return DIFFERS;
		}
	}
	return retval;
}

// quaternion uses tolerance
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Quaternion *outvalue, const Quaternion *invalue, int count )
{
	difftype_t retval = IDENTICAL;

	float tolerance = pField->fieldTolerance;
	contract_assert( tolerance >= 0.0f );
	bool usetolerance = tolerance > 0.0f;

	if ( usetolerance )
	{
		for ( int i = 0; i < count; ++i )
		{
			Quaternion delta;
			for ( int j = 0; j < 4; j++ )
			{
				delta[i] = outvalue[i][j] - invalue[i][j];
			}

			if ( fabs( delta.x ) <= tolerance &&
				fabs( delta.y ) <= tolerance &&
				fabs( delta.z ) <= tolerance &&
				fabs( delta.w ) <= tolerance )
			{
				retval = WITHINTOLERANCE;
				continue;
			}

			return DIFFERS;
		}
	}
	else
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( QuaternionCompare( outvalue[ i ], invalue[ i ] ) )
				continue;
			return DIFFERS;
		}
	}
	return retval;
}

// string
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const char *outvalue, const char *invalue, int count )
{
	if ( V_strcmp( outvalue, invalue ) )
	{
		return DIFFERS;
	}
	return IDENTICAL;
}

// ehandle
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const EHANDLE *outvalue, const EHANDLE *invalue, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		if ( outvalue[ i ].Get() == invalue[ i ].Get() )
			continue;
		return DIFFERS;
	}
	return IDENTICAL;
}

// color32
template<>
inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const color32 *outvalue, const color32 *invalue, int count )
{
	for ( int i = 0; i < count; i++ )
	{
		if ( outvalue[ i ] != invalue[ i ] )
			return DIFFERS;
	}
	return IDENTICAL;
}

template< class T >
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const T *outvalue, const T *invalue, int count )
{
	contract_assert( 0 );
}

// short
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const short *outvalue, const short *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "short differs (net %i pred %i) diff(%i)\n", (int)(invalue[i]), (int)(outvalue[i]), (int)(outvalue[i] - invalue[i]) );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "short (%i)\n", (int)(outvalue[0]) );
}

// int
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const int *outvalue, const int *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "int differs (net %i pred %i) diff(%i)\n", invalue[i], outvalue[i], outvalue[i] - invalue[i] );
	}

	bool described = false;
	if ( pField->flags & FTYPEDESC_MODELINDEX )
	{
		int modelindex = outvalue[0];
		model_t const *m = modelinfo->GetModel( modelindex );
		if ( m )
		{
			described = true;
			char shortfile[ 512 ];
			shortfile[ 0 ] = 0;
			V_FileBase( modelinfo->GetModelName( m ), shortfile, sizeof( shortfile ) );

			OutputFieldDescription( pCurrentMap, pField, difftype, "integer (%i->%s)\n", outvalue[0], shortfile );
		}
	}

	if ( !described )
	{
		OutputFieldDescription( pCurrentMap, pField, difftype, "integer (%i)\n", outvalue[0] );
	}
}

// bool
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const bool *outvalue, const bool *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "bool differs (net %s pred %s)\n", (invalue[i]) ? "true" : "false", (outvalue[i]) ? "true" : "false" );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "bool (%s)\n", (outvalue[0]) ? "true" : "false" );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const float *outvalue, const float *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "float differs (net %f pred %f) diff(%f)\n", invalue[ i ], outvalue[ i ], outvalue[ i ] - invalue[ i ] );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "float (%f)\n", outvalue[ 0 ] );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const char *outstring, const char *instring, int count )
{
	if ( difftype == DIFFERS )
	{
		ReportFieldsDiffer( pCurrentMap, pField, "string differs (net %s pred %s)\n", instring, outstring );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "string (%s)\n", outstring );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Vector *outValue, const Vector *inValue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		Vector delta = outValue[ i ] - inValue[ i ];
		ReportFieldsDiffer( pCurrentMap, pField, "vec[] differs (1st diff) (net %f %f %f - pred %f %f %f) delta(%f %f %f)\n", 
			inValue[i].x, inValue[i].y, inValue[i].z,
			outValue[i].x, outValue[i].y, outValue[i].z,
			delta.x, delta.y, delta.z );
	}
	OutputFieldDescription( pCurrentMap, pField, difftype, "vector (%f %f %f)\n", 
		outValue[0].x, outValue[0].y, outValue[0].z );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Quaternion *outValue, const Quaternion *inValue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		Quaternion delta;
		for ( int j = 0; j < 4; j++ )
		{
			delta[i] = outValue[i][j] - inValue[i][j];
		}

		ReportFieldsDiffer( pCurrentMap, pField, "quaternion[] differs (1st diff) (net %f %f %f %f - pred %f %f %f %f) delta(%f %f %f %f)\n", 
			inValue[i].x, inValue[i].y, inValue[i].z, inValue[i].w,
			outValue[i].x, outValue[i].y, outValue[i].z, outValue[i].w,
			delta[0], delta[1], delta[2], delta[3] );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "quaternion (%f %f %f %f)\n", outValue[0][0], outValue[0][1], outValue[0][2], outValue[0][3] );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const EHANDLE *outvalue, EHANDLE const *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		int i = 0;
		ReportFieldsDiffer( pCurrentMap, pField, "EHandles differ (net) 0x%p (pred) 0x%p\n", (void const *)invalue[ i ].Get(), (void *)outvalue[ i ].Get() );
	}

	CBaseEntity *ent = outvalue[0].Get();
	if ( ent )
	{
		const char *classname = ent->GetClassname();
		if ( !classname[0] )
		{
			classname = typeid( *ent ).name();
		}

		OutputFieldDescription( pCurrentMap, pField, difftype, "EHandle (0x%p->%s)", (void *)outvalue[ 0 ], classname );
	}
	else
	{
		OutputFieldDescription( pCurrentMap, pField, difftype, "EHandle (NULL)" );
	}
}

// color32
template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const color32 *outvalue, const color32 *invalue, int count )
{
	if ( difftype == DIFFERS )
	{
		ReportFieldsDiffer( pCurrentMap, pField, "color differs (net %d %d %d %d pred %d %d %d %d)\n", 
			outvalue[ 0 ].r,
			outvalue[ 0 ].g,
			outvalue[ 0 ].b,
			outvalue[ 0 ].a,
			invalue[ 0 ].r,
			invalue[ 0 ].g,
			invalue[ 0 ].b,
			invalue[ 0 ].a
			);
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "color (%d %d %d %d)\n", 
		outvalue[ 0 ].r,
		outvalue[ 0 ].g,
		outvalue[ 0 ].b,
		outvalue[ 0 ].a );
}

template<>
inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const uint8 *outstring, const uint8 *instring, int count )
{
	if ( difftype == DIFFERS )
	{
		ReportFieldsDiffer( pCurrentMap, pField, "byte differs (net %d pred %d)\n", int(instring[0]), int(outstring[0]) );
	}

	OutputFieldDescription( pCurrentMap, pField, difftype, "byte (%d)\n", int(outstring[0]) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CPredictionCopy::ReportFieldsDiffer( const datamap_t *pCurrentMap, const typedescription_t *pField, const char *fmt, ... )
{
	++m_nErrorCount;

	if ( m_FieldCompareFunc )
		return;

	const char *fieldname = "empty";
	const char *classname = "empty";
	int flags = 0;

	if ( pField )
	{
		flags		= pField->flags;
		fieldname	= pField->fieldName ? pField->fieldName : "NULL";
		classname   = pCurrentMap->dataClassName;
	}

	va_list argptr;
	char data[ 4096 ];
	int len;
	__builtin_va_start(argptr, fmt);
	len = V_vsnprintf(data, sizeof( data ), fmt, argptr);
	__builtin_va_end(argptr);

	Msg( "%2d (%d)%s::%s - %s",
		m_nErrorCount,
		m_nEntIndex,
		classname,
		fieldname,
		data );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CPredictionCopy::OutputFieldDescription( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t dt, const char *fmt, ... )
{
	if ( !m_FieldCompareFunc )
		return;

	const char *fieldname = "empty";
	const char *classname = "empty";
	int flags = 0;

	if ( pField )
	{
		flags		= pField->flags;
		fieldname	= pField->fieldName ? pField->fieldName : "NULL";
		classname   = pCurrentMap->dataClassName;
	}

	va_list argptr;
	char data[ 4096 ];
	int len;
	__builtin_va_start(argptr, fmt);
	len = V_vsnprintf(data, sizeof( data ), fmt, argptr);
	__builtin_va_end(argptr);

	bool isnetworked = ( flags & FTYPEDESC_INSENDTABLE ) ? true : false;
	bool isnoterrorchecked = ( flags & FTYPEDESC_NOERRORCHECK ) ? true : false;

	( *m_FieldCompareFunc )( 
		classname,
		fieldname,
		g_FieldTypes[ pField->fieldType ],
		isnetworked,
		isnoterrorchecked,
		dt != IDENTICAL ? true : false,
		dt == WITHINTOLERANCE ? true : false,
		data 
	);
}

void CPredictionCopy::DescribeFields( const std::vector< const datamap_t * > &vecGroups, const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				i;
	int				flags;
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	int				fieldSize;

	const flattenedoffsets_t &flat = OptimizedDataMap( pCurrentMap )->m_Info[ nPredictionCopyType ].m_Flat;
	int fieldCount = std::ssize( flat.m_Flattened );
	const typedescription_t * __restrict__ pField = &flat.m_Flattened[ 0 ];

	for ( i = 0; i < fieldCount; ++i, pField++ )
	{
		flags = pField->flags;
		int nFieldType = pField->fieldType;

		const byte * __restrict__ pOutputData;
		const byte * __restrict__ pInputData;

		fieldOffsetDest = pField->flatOffset[ m_nSrcOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nDestOffsetIndex ];
		fieldSize = pField->fieldSize;

		pOutputData = (m_pDest + fieldOffsetDest );
		pInputData = (m_pSrc + fieldOffsetSrc );

		const datamap_t *sourceGroup = pCurrentMap;
		if ( pField->flatGroup < std::ssize( vecGroups ) )
		{
			sourceGroup = vecGroups[ pField->flatGroup ];
		}

		PREDICTIONCOPY_APPLY( ProcessField_Describe, nFieldType, sourceGroup, pField, pOutputData, pInputData, fieldSize );
	}
}

//-----------------------------------------------------------------------------
// Purpose: static method
// Input  : *fieldname - 
//			*dmap - 
// Output : typedescription_t
//-----------------------------------------------------------------------------
// static method
const typedescription_t *CPredictionCopy::FindFlatFieldByName( const char *fieldname, const datamap_t *dmap )
{
	PrepareDataMap( const_cast< datamap_t * >( dmap ) );

	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		const flattenedoffsets_t &flat = OptimizedDataMap( dmap )->m_Info[ i ].m_Flat;
		int c = std::ssize( flat.m_Flattened );
		for ( int j = 0; j < c; ++j )
		{
			const typedescription_t *td = &flat.m_Flattened[ j ];
			if ( !V_stricmp( td->fieldName, fieldname ) )
			{
				return td;
			}
		}
	}
	return nullptr;
}

static ConVar pwatchent( side_registry(), "pwatchent", "-1", FCVAR_CHEAT, "Entity to watch for prediction system changes." );
static ConVar pwatchvar( side_registry(), "pwatchvar", "", FCVAR_CHEAT, "Entity variable to watch in prediction system for changes." );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CPredictionCopy::WatchMsg( const typedescription_t *pField, const char *fmt, ... )
{
	contract_assert( pField );
	contract_assert( m_pOperation );

	va_list argptr;
	char data[ 4096 ];
	int len;
	__builtin_va_start(argptr, fmt);
	len = V_vsnprintf(data, sizeof( data ), fmt, argptr);
	__builtin_va_end(argptr);

	Msg( "%i %s %s : %s\n", gpGlobals->tickcount, m_pOperation, pField->fieldName, data );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *operation - 
//			entindex - 
//			*dmap - 
//-----------------------------------------------------------------------------
void CPredictionCopy::DetermineWatchField( const char *operation, int entindex, const datamap_t *dmap )
{
	m_pWatchField = nullptr;
	m_pOperation = operation;
	if ( !m_pOperation || !m_pOperation[0] )
		return;

	int enttowatch = pwatchent.GetInt();
	if ( enttowatch < 0 )
		return;

	if ( entindex != enttowatch )
		return;

	// See if they specified a field
	if ( pwatchvar.GetString()[0] == 0 )
		return;

	m_pWatchField = CPredictionCopy::FindFlatFieldByName( pwatchvar.GetString(), dmap );
}

static void RemoveFieldsByName( char const *pchFieldName, std::vector< typedescription_t > &build )
{
	// Don't start at final field, since it the one we just added with FTYPEDESC_OVERRIDE
	for ( int i = std::ssize( build ) - 2; i >= 0 ; --i )
	{
		// Embedded field "offsets" can be the same as their first data field, so don't remove
		if ( build[ i ].fieldType == FIELD_EMBEDDED )
			continue;

		if ( !V_stricmp( build[ i ].fieldName, pchFieldName ) )
		{
			// Msg( "Removing %s %d due to override\n", build[ i ]->fieldName, build[ i ]->flatOffset[ TD_OFFSET_NORMAL ] );
			build.erase( build.begin() + i );
		}
	}
}

static void BuildGroupList_R( int nPredictionCopyType, int nGroup, const datamap_t *dmap, std::vector< const datamap_t * > &vecGroups )
{
	// Descend to base classes first so FTYPEDESC_OVERRIDE can remove the previous ones
	if ( dmap->baseMap )
	{
		BuildGroupList_R( nPredictionCopyType, nGroup + 1, dmap->baseMap, vecGroups );
	}

	vecGroups.push_back( dmap );
}

static void BuildFlattenedChains_R( 
	int nPredictionCopyType, 
	int &nMaxGroupSeen, 
	int nGroup, 
	datamap_t *dmap, 
	std::vector< typedescription_t > &build, 
	int nBaseOffset )
{
	// Descend to base classes first so FTYPEDESC_OVERRIDE can remove the previous ones
	if ( dmap->baseMap )
	{
		BuildFlattenedChains_R( nPredictionCopyType, nMaxGroupSeen, nGroup + 1, dmap->baseMap, build, nBaseOffset );
	}

	if ( nGroup > nMaxGroupSeen )
	{
		nMaxGroupSeen = nGroup;
	}

	int c = dmap->dataNumFields;
	for ( int i = 0; i < c; i++ )
	{
		typedescription_t *pField = &dmap->dataDesc[ i ];
		if ( pField->fieldType == FIELD_VOID )
			continue;

		bool bAdd = true;
		if ( pField->fieldType != FIELD_EMBEDDED )
		{
			if ( pField->flags & FTYPEDESC_PRIVATE )
			{
				if ( pField->flags & FTYPEDESC_OVERRIDE )
				{
					// Find previous field targeting same offset
					RemoveFieldsByName( pField->fieldName, build );
				}
				continue;
			}

			// For PC_NON_NETWORKED_ONLYs skip any fields that are present in the network send tables
			if ( nPredictionCopyType == PC_NON_NETWORKED_ONLY && ( pField->flags & FTYPEDESC_INSENDTABLE ) )
			{
				bAdd = false;
			}
			// For PC_NETWORKED_ONLYs skip any fields that are not present in the network send tables
			if ( nPredictionCopyType == PC_NETWORKED_ONLY && !( pField->flags & FTYPEDESC_INSENDTABLE ) )
			{
				bAdd = false;
			}
		}
		else
		{
			bAdd = false;
		}

		pField->flatGroup = nGroup;
		pField->flatOffset[ TD_OFFSET_NORMAL ] = nBaseOffset + pField->fieldOffset;

		if ( bAdd )
		{
			build.push_back( *pField );
		}

		//	Msg( "Visit %s offset %d\n", pField->fieldName, pField->flatOffset[ nPackType ] );

		if ( pField->fieldType == FIELD_EMBEDDED )
		{
			contract_assert( !(pField->flags & FTYPEDESC_PTR ) );
			BuildFlattenedChains_R( nPredictionCopyType, nMaxGroupSeen, nGroup, pField->td, build, pField->flatOffset[ TD_OFFSET_NORMAL ] );
		}

		if ( pField->flags & FTYPEDESC_OVERRIDE )
		{
			// Find previous field targeting same offset
			RemoveFieldsByName( pField->fieldName, build );
		}
	}	
}

static int CompareFlattenedOffsets( const void *pv1, const void *pv2 )
{
	const typedescription_t *td1 = (const typedescription_t *)pv1;
	const typedescription_t *td2 = (const typedescription_t *)pv2;
	if ( td1->flatOffset[ TD_OFFSET_NORMAL ] < td2->flatOffset[ TD_OFFSET_NORMAL ] )
		return -1;
	else if ( td1->flatOffset[ TD_OFFSET_NORMAL ] > td2->flatOffset[ TD_OFFSET_NORMAL ] )
		return 1;
	if ( td1->flatGroup < td2->flatGroup )
		return -1;
	else if ( td1->flatGroup > td2->flatGroup )
		return 1;

	return 0;
}

static int BuildPackedFlattenedOffsets( int nStartOffset, flattenedoffsets_t &flat )
{
	int current_position = nStartOffset;

	for ( int i = 0; i < std::ssize( flat.m_Flattened ); ++i )
	{
		typedescription_t *field = &flat.m_Flattened[ i ];

		switch ( field->fieldType )
		{
		default:
		case FIELD_MODELINDEX:
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_TIME:
		case FIELD_TICK:
		case FIELD_CUSTOM:
		case FIELD_CLASSPTR:
		case FIELD_EDICT:
		case FIELD_POSITION_VECTOR:
		case FIELD_FUNCTION:
			contract_assert( 0 );
			break;

		case FIELD_EMBEDDED:
			{
				Error( "Not expecting FIELD_EMBEDDED in flattened list (%s)", field->fieldName );
			}
			break;

		case FIELD_FLOAT:
		case FIELD_VECTOR:
		case FIELD_QUATERNION:
		case FIELD_INTEGER:
		case FIELD_EHANDLE:
		case FIELD_COLOR32:
		case FIELD_VMATRIX:
		case FIELD_VECTOR4D:
			{
				current_position = ALIGN_VALUE( current_position, 4 );
				field->flatOffset[ TD_OFFSET_PACKED ] = current_position;
				current_position += field->fieldSizeInBytes;
			}
			break;
		case FIELD_SHORT:
			{
				current_position = ALIGN_VALUE( current_position, 2 );
				field->flatOffset[ TD_OFFSET_PACKED ] = current_position;
				current_position += field->fieldSizeInBytes;
			}
			break;
		case FIELD_STRING:
		case FIELD_BOOLEAN:
		case FIELD_CHARACTER:
			{
				field->flatOffset[ TD_OFFSET_PACKED ] = current_position;
				current_position += field->fieldSizeInBytes;
			}
			break;
		case FIELD_VOID:
			{
				// Special case, just skip it
			}
			break;
		}

		// Msg( "%s packed to %d size %d\n", field->fieldName, field->flatOffset[ TD_OFFSET_PACKED ], field->fieldSizeInBytes );
	}

	flat.m_nPackedStartOffset = nStartOffset;
	flat.m_nPackedSize = current_position - nStartOffset;

	current_position = ALIGN_VALUE( current_position, 4 );

	return current_position;
}

static void BuildDataRuns( datamap_t *dmap )
{
	for ( int pc = 0; pc < PC_COPYTYPE_COUNT; ++pc )
	{
		datamapinfo_t &info = OptimizedDataMap( dmap )->m_Info[ pc ];
		datacopyruns_t *runs = &info.m_CopyRuns;

		contract_assert( !std::ssize( info.m_CopyRuns.m_vecRuns ) );

		const flattenedoffsets_t &flat = info.m_Flat;

		int nRunStartField = 0;
		int nCurrentRunStartOffset = 0;
		int	nLastFieldEndOffset = 0;

		std::vector< datarun_t > &vecRuns = runs->m_vecRuns;

		int i;
		for ( i = 0; i < std::ssize( flat.m_Flattened ); ++i )
		{
			const typedescription_t *td = &flat.m_Flattened[ i ];
			int offset = td->flatOffset[ TD_OFFSET_NORMAL ];
			if ( i == 0 )
			{
				nRunStartField = i;
				nLastFieldEndOffset = offset;
				nCurrentRunStartOffset = offset;
			}

			if ( td->fieldType == FIELD_EMBEDDED )
			{
				contract_assert( 0 );
				continue;
			}

			if ( nLastFieldEndOffset != offset )
			{
				datarun_t run;
				run.m_nStartFlatField = nRunStartField;
				run.m_nEndFlatField = i - 1;
				contract_assert( nCurrentRunStartOffset == flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_NORMAL ] );
				run.m_nStartOffset[ TD_OFFSET_NORMAL ] = nCurrentRunStartOffset;
				run.m_nStartOffset[ TD_OFFSET_PACKED ] = flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_PACKED ];
				run.m_nLength = nLastFieldEndOffset - nCurrentRunStartOffset;

				vecRuns.push_back( run );

				nRunStartField = i;
				nCurrentRunStartOffset = offset;
			}
			nLastFieldEndOffset = td->flatOffset[ TD_OFFSET_NORMAL ] + td->fieldSizeInBytes;
		}

		// Close off last run
		if ( nLastFieldEndOffset != nCurrentRunStartOffset )
		{
			datarun_t run;
			run.m_nStartFlatField = nRunStartField;
			run.m_nEndFlatField = i - 1;
			contract_assert( nCurrentRunStartOffset == flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_NORMAL ] );
			run.m_nStartOffset[ TD_OFFSET_NORMAL ] = nCurrentRunStartOffset;
			run.m_nStartOffset[ TD_OFFSET_PACKED ] = flat.m_Flattened[ nRunStartField ].flatOffset[ TD_OFFSET_PACKED ];
			run.m_nLength = nLastFieldEndOffset - nCurrentRunStartOffset;

			vecRuns.push_back( run );
		}
	}
}

static void BuildFlattenedChains( datamap_t *dmap )
{
	if ( dmap->m_pOptimizedDataMap )
		return;
	dmap->m_pOptimizedDataMap = g_OptimizedDataMapPool.alloc_zero();

	int nMaxGroupSeen[ PC_COPYTYPE_COUNT ] = { 0 };

	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		std::vector< typedescription_t > &build = OptimizedDataMap( dmap )->m_Info[ nPredictionCopyType ].m_Flat.m_Flattened;

		int nGroupCount = 0;
		int nBaseOffset = 0;
		BuildFlattenedChains_R( 
			nPredictionCopyType, 
			nMaxGroupSeen[ nPredictionCopyType ], 
			nGroupCount, 
			dmap, 
			build, 
			nBaseOffset );
		// Msg( "%d == %d entries\n", nPredictionCopyType, build[ nPredictionCopyType ].Count() );
	}

	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		int nMaxGroup = nMaxGroupSeen[ nPredictionCopyType ];

		std::vector< typedescription_t > &build = OptimizedDataMap( dmap )->m_Info[ nPredictionCopyType ].m_Flat.m_Flattened;

		for ( int i = 0; i < std::ssize( build ); ++i )
		{
			typedescription_t *field = &build[ i ];
			field->flatGroup = nMaxGroup - field->flatGroup;
		}
	}

	int nPackedDataStartOffset = 0;
	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		flattenedoffsets_t &flat = OptimizedDataMap( dmap )->m_Info[ nPredictionCopyType ].m_Flat;
		qsort( flat.m_Flattened.data(), std::ssize( flat.m_Flattened ), sizeof( typedescription_t ), (int (*)(const void *, const void *))CompareFlattenedOffsets );

		nPackedDataStartOffset = BuildPackedFlattenedOffsets( nPackedDataStartOffset, flat );
		dmap->m_nPackedSize = nPackedDataStartOffset;
	}

	BuildDataRuns( dmap );
}

const tokenset_t< int > s_PredCopyType[] =
{
	{ "Non-Sendtable"	, PC_NON_NETWORKED_ONLY },	
	{ "SendTable"		, PC_NETWORKED_ONLY },											
	{ "Everything"		, PC_EVERYTHING },													
	{ nullptr, -1 }
};

const tokenset_t< int > s_PredPackType[] =
{
	{ "Normal"	, TD_OFFSET_NORMAL },	
	{ "Packed"	, TD_OFFSET_PACKED },											
	{ nullptr, -1 }
};

static void DescribeRuns( const datamap_t *dmap, int nPredictionCopyType, int packType )
{
	const flattenedoffsets_t *flat;
	const datarun_t *run;
	const datacopyruns_t &runs = OptimizedDataMap( dmap )->m_Info[ nPredictionCopyType ].m_CopyRuns;
	flat = &OptimizedDataMap( dmap )->m_Info[ nPredictionCopyType ].m_Flat;
	Msg( "   Runs for copy type: %s, packing: %s\n", s_PredCopyType->GetNameByToken( nPredictionCopyType ), s_PredPackType->GetNameByToken( packType ) );
	for ( int i = 0; i < std::ssize( runs.m_vecRuns ); ++i )
	{
		run = &runs.m_vecRuns[ i ];
		Msg( "     %5d:  %5d -> %5d (%5d bytes): %s to %s\n",
			i, run->m_nStartOffset[ packType ], run->m_nStartOffset[ packType ] + run->m_nLength, run->m_nLength,
			flat->m_Flattened[ run->m_nStartFlatField ].fieldName,
			flat->m_Flattened[ run->m_nEndFlatField ].fieldName );
	}
}

static void DescribeFlattenedList( const datamap_t *dmap, int nPredictionCopyType, int packType )
{
	char const *prefix;
	prefix = dmap->dataClassName;

	Msg( "->Sorted %s for copy type: %s, packing: %s\n", prefix, s_PredCopyType->GetNameByToken( nPredictionCopyType ), s_PredPackType->GetNameByToken( packType ) );
	// Now dump the flattened list
	int nLastFieldEnd = 0;
	int nCurrentRunStartOffset = 0;

	int nRuns = 0;
	int nBytesInRuns = 0;
	int offset = 0;

	const flattenedoffsets_t &list = OptimizedDataMap( dmap )->m_Info[ nPredictionCopyType ].m_Flat;
	for ( int i = 0; i < std::ssize( list.m_Flattened ); ++i )
	{
		const typedescription_t *td = &list.m_Flattened[ i ];
		offset = td->flatOffset[ packType ];
		if ( i == 0 )
		{
			nLastFieldEnd = offset;
			nCurrentRunStartOffset = offset;
		}

		if ( td->fieldType != FIELD_EMBEDDED )
		{
			if ( nLastFieldEnd != offset )
			{
				Msg( "  gap of %d bytes [last run %d]\n", offset - nLastFieldEnd, ( nLastFieldEnd - nCurrentRunStartOffset ) );
				++nRuns;
				nBytesInRuns += ( nLastFieldEnd - nCurrentRunStartOffset );

				nCurrentRunStartOffset = offset;
			}
			nLastFieldEnd = td->flatOffset[ packType ] + td->fieldSizeInBytes;
		}

		Msg( "group %s [flat %d] [sort %d] %d bytes\n",
			td->fieldName,
			td->flatOffset[ packType ],
			td->flatOffset[ TD_OFFSET_NORMAL ],
			td->fieldSizeInBytes );
	}

	// Close off last run
	if ( nLastFieldEnd != nCurrentRunStartOffset )
	{
		Msg( "Last run %d\n", nLastFieldEnd - nCurrentRunStartOffset );
		++nRuns;
		nBytesInRuns += ( nLastFieldEnd - nCurrentRunStartOffset );
	}

	if ( nRuns > 0 )
	{
		Msg( "%d runs, %d bytes in runs, %f avg bytes per run\n",
			nRuns, nBytesInRuns, nBytesInRuns/(float)nRuns );
	}
	Msg( "->\n" );

	DescribeRuns( dmap, nPredictionCopyType, packType );
}

void CPredictionCopy::CopyFlatFieldsUsingRuns( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	byte			* __restrict__ pOutputData;
	const byte		* __restrict__ pInputData;

	const datacopyruns_t &runs = OptimizedDataMap( pCurrentMap )->m_Info[ nPredictionCopyType ].m_CopyRuns;

	byte * __restrict__ pDest = ( byte * __restrict__ )m_pDest;
	const byte * __restrict__ pSrc = (const byte * __restrict__)m_pSrc;

	int c = std::ssize( runs.m_vecRuns );
	for ( int i = 0; i < c; ++i )
	{
		const datarun_t * __restrict__ run = &runs.m_vecRuns[ i ];
		fieldOffsetDest = run->m_nStartOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= run->m_nStartOffset[ m_nSrcOffsetIndex ];
		pOutputData = pDest + fieldOffsetDest;
		pInputData = pSrc + fieldOffsetSrc;

		V_memcpy( pOutputData, pInputData, run->m_nLength );
	}
}

void CPredictionCopy::CopyFlatFields( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	byte			* __restrict__ pOutputData;
	const byte		* __restrict__ pInputData;

	byte * __restrict__ pDest = (byte * __restrict__)m_pDest;
	const byte * __restrict__ pSrc = (const byte * __restrict__)m_pSrc;

	const flattenedoffsets_t &flat = OptimizedDataMap( pCurrentMap )->m_Info[ nPredictionCopyType ].m_Flat;

	int fieldCount = std::ssize( flat.m_Flattened );
	for ( int i = 0; i < fieldCount; ++i )
	{
		const typedescription_t *pField = &flat.m_Flattened[ i ];

		fieldOffsetDest = pField->flatOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nSrcOffsetIndex ];
		pOutputData = pDest + fieldOffsetDest;
		pInputData = pSrc + fieldOffsetSrc;

		V_memcpy( pOutputData, pInputData, pField->fieldSizeInBytes );
	}
}

void CPredictionCopy::ErrorCheckFlatFields_NoSpew( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				i;
	int				flags;
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	int				fieldSize;

	const flattenedoffsets_t &flat = OptimizedDataMap( pCurrentMap )->m_Info[ nPredictionCopyType ].m_Flat;
	const typedescription_t *pBase = &flat.m_Flattened[ 0 ];
	int fieldCount = std::ssize( flat.m_Flattened );

	const byte * __restrict__ pDest = (const byte * __restrict__)m_pDest;
	const byte * __restrict__ pSrc = (const byte * __restrict__)m_pSrc;
	
	const byte *pOutputData;
	const byte *pInputData;

	for ( i = 0; i < fieldCount && !m_nErrorCount; ++i )
	{
		const typedescription_t * __restrict__ pField = &pBase[ i ];

		flags = pField->flags;
		if ( flags & FTYPEDESC_NOERRORCHECK )
			continue;

		fieldOffsetDest = pField->flatOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nSrcOffsetIndex ];

		pOutputData = pDest + fieldOffsetDest;
		pInputData = pSrc + fieldOffsetSrc;

		fieldSize = pField->fieldSize;
		int nFieldType = pField->fieldType;

		PREDICTIONCOPY_APPLY( ProcessField_Compare_NoSpew, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize );
	}
}

void CPredictionCopy::ErrorCheckFlatFields_Spew( const datamap_t *pCurrentMap, int nPredictionCopyType )
{
	int				i;
	int				flags;
	int				fieldOffsetSrc;
	int				fieldOffsetDest;
	int				fieldSize;

	const flattenedoffsets_t &flat = OptimizedDataMap( pCurrentMap )->m_Info[ nPredictionCopyType ].m_Flat;
	const typedescription_t *pBase = &flat.m_Flattened[ 0 ];
	int fieldCount = std::ssize( flat.m_Flattened );

	const byte * __restrict__ pDest = m_pDest;
	const byte * __restrict__ pSrc = m_pSrc;

	const byte *pOutputData;
	const byte *pInputData;

	for ( i = 0; i < fieldCount ; ++i )
	{
		const typedescription_t * __restrict__ pField = &pBase[ i ];

		flags = pField->flags;
		if ( flags & FTYPEDESC_NOERRORCHECK )
			continue;

		fieldOffsetDest = pField->flatOffset[ m_nDestOffsetIndex ];
		fieldOffsetSrc	= pField->flatOffset[ m_nSrcOffsetIndex ];

		pOutputData = pDest + fieldOffsetDest;
		pInputData = pSrc + fieldOffsetSrc;

		fieldSize = pField->fieldSize;
		flags = pField->flags;
		int nFieldType = pField->fieldType;

		PREDICTIONCOPY_APPLY( ProcessField_Compare_Spew, nFieldType, pCurrentMap, pField, pOutputData, pInputData, fieldSize );
	}
}

bool CPredictionCopy::PrepareDataMap( datamap_t *dmap )
{
	bool bPerformedPrepare = false;
	if ( dmap && !dmap->m_pOptimizedDataMap )
	{
		bPerformedPrepare = true;
		BuildFlattenedChains( dmap );
		dmap = dmap->baseMap;
	}

	return bPerformedPrepare;
}

static void cl_predictioncopy_describe( const CCommand &args );
static ConCommand cl_predictioncopy_describe_command( side_registry(), "cl_predictioncopy_describe", cl_predictioncopy_describe, "Describe datamap_t for entindex" );
static void cl_predictioncopy_describe( const CCommand &args )
{
	if ( args.ArgC() <= 1 )
	{
		Msg( "Usage:  %s <entindex>", args[ 0 ] );
		return;
	}
	int entindex = V_atoi( args[ 1 ] );
	CBaseEntity *ent = CBaseEntity::Instance( entindex );
	if ( !ent )
	{
		Msg( "cl_predictioncopy_describe:  no such entity %d\n", entindex );
		return;
	}

	datamap_t *dmap = ent->GetPredDescMap();
	if ( !dmap )
	{
		return;
	}
	CPredictionCopy::PrepareDataMap( dmap );

	for ( int nPredictionCopyType = 0; nPredictionCopyType < PC_COPYTYPE_COUNT; ++nPredictionCopyType )
	{
		//for ( int i = 0; i < TD_OFFSET_COUNT; ++i )
		{
			DescribeFlattenedList( dmap, nPredictionCopyType, 0 );
		}
	}
}

// 0 PC_NON_NETWORKED_ONLY = (1<<0) or (1)
// 1 PC_NETWORKED_ONLY = (1<<1) or (2)
// 2 PC_EVERYTHING = ( PC_NON_NETWORKED_ONLY | PC_NETWORKED_ONLY ) or 3
// So for these three options, we just take the type and add one!!!
[[gnu::always_inline]] inline int ComputeTypeMask( int nType )
{
	return nType + 1;
}

void CPredictionCopy::TransferDataCopyOnly( const datamap_t *dmap )
{
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		if ( types & (1<<i) )
		{
			CopyFlatFieldsUsingRuns( dmap, i );
		}
	}
}

// Stop at first error
void CPredictionCopy::TransferDataErrorCheckNoSpew( char const *pchOperation, const datamap_t *dmap )
{
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT && !m_nErrorCount; ++i )
	{
		if ( types & (1<<i) )
		{
			ErrorCheckFlatFields_NoSpew( dmap, i );
		}
	}
}

void CPredictionCopy::TransferDataErrorCheckSpew( char const *pchOperation, const datamap_t *dmap )
{
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		if ( types & (1<<i) )
		{
			ErrorCheckFlatFields_Spew( dmap, i );
		}
	}
}

void CPredictionCopy::TransferDataDescribe( char const *pchOperation, const datamap_t *dmap )
{
	// Copy from here first, then base classes
	int types = ComputeTypeMask( m_nType );
	for ( int i = 0; i < PC_COPYTYPE_COUNT; ++i )
	{
		if ( types & (1<<i) )
		{
			std::vector< const datamap_t * > vecGroups;
			int nGroup = 0;
			BuildGroupList_R( i, nGroup, dmap, vecGroups );
			DescribeFields( vecGroups, dmap, i );
		}
	}
}

int CPredictionCopy::TransferData( const char *operation, int entindex, datamap_t *dmap )
{
	m_nEntIndex = entindex;

	PrepareDataMap( dmap );

	switch ( m_OpType )
	{
	default:
		contract_assert( 0 );
	case TRANSFERDATA_COPYONLY:  // Data copying only (uses runs)
		{
			// Watch is based on "destination" of any write operation
			DetermineWatchField( operation, entindex, dmap );
			TransferDataCopyOnly( dmap );
		}
		break;
	case TRANSFERDATA_ERRORCHECK_NOSPEW: // Checks for errors, returns after first error found
		{
			TransferDataErrorCheckNoSpew( operation, dmap );
		}
		break;
	case TRANSFERDATA_ERRORCHECK_SPEW:   // checks for errors, reports all errors to console
		{
			TransferDataErrorCheckSpew( operation, dmap );
		}
		break;
	case TRANSFERDATA_ERRORCHECK_DESCRIBE:
		{
			TransferDataDescribe( operation, dmap );
		}
		break;
	}

	if ( m_pWatchField ) 
	{
		// Watch is based on "destination" of any write operation
		DumpWatchField( m_pWatchField, m_pDest + m_pWatchField->flatOffset[ m_nDestOffsetIndex ], m_pWatchField->fieldSize );
	}

	return m_nErrorCount;
}

datarun_t::datarun_t() : m_nStartFlatField( 0 ), m_nEndFlatField( 0 ), m_nLength( 0 )
{
	for ( int i = 0 ; i < TD_OFFSET_COUNT; ++i )
	{
		m_nStartOffset[ i ] = 0;
	}
}

CValueChangeTrackerScope::CValueChangeTrackerScope( char const *pchContext)
{
	m_bCallEndTrack = true;
	g_pChangeTracker->StartTrack( pchContext );
}

CValueChangeTrackerScope::~CValueChangeTrackerScope()
{
	if ( m_bCallEndTrack )
	{
		g_pChangeTracker->EndTrack();
	}
}

#undef PREDICTIONCOPY_APPLY
#undef PREDICTIONCOPY_APPLY_NOEMBEDDED

class CPredictionCopy;
class CBaseEntity;

using namespace ::ks::reflect;

CValueChangeTracker::CValueChangeTracker() :
	m_bActive( false ),
	m_bTracking( false ),
	m_pTrackField( nullptr )
{
	V_memset( m_OrigValueBuf, 0, sizeof( m_OrigValueBuf ) );
}

CBaseEntity *CValueChangeTracker::GetEntity()
{
	return m_hEntityToTrack.Get();
}

void CValueChangeTracker::GetValue( char *buf, size_t bufsize )
{
	buf[ 0 ] = 0;

	contract_assert( IsActive() );

	if ( !m_hEntityToTrack.Get() )
		return;

	if ( !m_pTrackField )
		return;

	void const *pInputData = ( const byte * )m_hEntityToTrack.Get() + m_pTrackField->flatOffset[ TD_OFFSET_NORMAL ];
	if ( !pInputData )
		return;

	int fieldType = m_pTrackField->fieldType;

	switch( fieldType )
	{
	default:
	case FIELD_EMBEDDED:
	case FIELD_MODELNAME:
	case FIELD_SOUNDNAME:
	case FIELD_CUSTOM:
	case FIELD_CLASSPTR:
	case FIELD_EDICT:
	case FIELD_POSITION_VECTOR:
	case FIELD_VOID:
	case FIELD_FUNCTION:
		{
			contract_assert( 0 );
		}
		break;
	case FIELD_FLOAT:
	case FIELD_TIME:
		V_snprintf( buf, bufsize, "%f", *(float const *)pInputData );
		break;
	case FIELD_STRING:
		V_snprintf( buf, bufsize, "%s", (char const*)pInputData );
		break;
	case FIELD_VECTOR:
		{
			const Vector *pVec = (const Vector *)pInputData;
			V_snprintf( buf, bufsize, "%f %f %f", pVec->x, pVec->y, pVec->z );
		}
		break;
	case FIELD_QUATERNION:
		{
			const Quaternion *p = ( const Quaternion * )pInputData;
			V_snprintf( buf, bufsize, "%f %f %f %f", p->x, p->y, p->z, p->w );
		}
		break;

	case FIELD_COLOR32:
		{
			const Color *color = ( const Color * )pInputData;
			V_snprintf( buf, bufsize, "%d %d %d %d", color->r(), color->g(), color->b(), color->a() );
		}
		break;

	case FIELD_BOOLEAN:
		V_snprintf( buf, bufsize, "%s", (*(const bool *)pInputData) ? "true" : "false" );
		break;
	case FIELD_INTEGER:
	case FIELD_TICK:
	case FIELD_MODELINDEX:
		V_snprintf( buf, bufsize, "%i", *(const int*)pInputData );
		break;

	case FIELD_SHORT:
		V_snprintf( buf, bufsize, "%i", (int)*(const short*)pInputData );
		break;

	case FIELD_CHARACTER:
		V_snprintf( buf, bufsize, "%c", *(const char *)pInputData );
		break;

	case FIELD_EHANDLE:
		V_snprintf( buf, bufsize, "eh 0x%p", (void const *)((const EHANDLE *)pInputData)->Get() );
		break;
	}
}

void CValueChangeTracker::StartTrack( char const *pchContext )
{
	if ( !IsActive() )
		return;

	m_strContext = ks::nz( pchContext );

	// Grab current value into scratch buffer
	GetValue( m_OrigValueBuf, sizeof( m_OrigValueBuf ) );

	m_bTracking = true;
}

void CValueChangeTracker::EndTrack()
{
	if ( !IsActive() )
		return;

	if ( !m_bTracking )
		return;
	m_bTracking = false;

	char final[ eChangeTrackerBufSize ];
	GetValue( final, sizeof( final ) );

	std::string *history = &m_History.emplace_back();
	if ( V_stricmp( final, m_OrigValueBuf ) )
	{
		history->assign( ks::nz( CFmtStr( "+++ %-20.20s:  %s (was %s)", m_strContext.c_str(), final, m_OrigValueBuf ) ) );
	}
	else
	{
		history->assign( ks::nz( CFmtStr( "    %-20.20s:  %s", m_strContext.c_str(), final ) ) );
	}

	Msg( ":%s\n", history->c_str() );
}

void CValueChangeTracker::ClearTracking()
{
	m_bActive = false;
	m_bTracking = false;
	m_hEntityToTrack = nullptr;
	m_strFieldName = "";
	m_History.clear();
	m_pTrackField = nullptr;
}

void CValueChangeTracker::SetupTracking( CBaseEntity *ent, char const *pchFieldName )
{
	ClearTracking();

	// Find the field
	datamap_t *dmap = ent->GetPredDescMap();
	if ( !dmap )
	{
		Msg( "No prediction datamap_t for entity %d/%s\n", ent->index, ent->GetClassname() );
		return;
	}

	CPredictionCopy::PrepareDataMap( dmap );

	m_pTrackField = CPredictionCopy::FindFlatFieldByName( pchFieldName, dmap );
	if ( !m_pTrackField )
	{
		Msg( "No field '%s' in datamap_t for entity %d/%s\n", pchFieldName, ent->index, ent->GetClassname() );
		return;
	}

	m_hEntityToTrack = ent;
	m_strFieldName = ks::nz( pchFieldName );
	m_bActive = true;
}

void CValueChangeTracker::Reset()
{
	m_History.clear();
}

bool CValueChangeTracker::IsActive() const
{
	return m_bActive;
}

void CValueChangeTracker::Spew()
{
	if ( IsActive() )
	{
		for ( int i = 0 ; i < std::ssize( m_History ); ++i )
		{
			Msg( "%s\n", m_History[ i ].c_str() );
		}
	}

	Reset();
}

static CValueChangeTracker g_ChangeTracker;
CValueChangeTracker *g_pChangeTracker = &g_ChangeTracker;

static void cl_pred_track( const CCommand &args );
static ConCommand cl_pred_track_command( side_registry(), "cl_pred_track", cl_pred_track, "<entindex> <fieldname>:  Track changes to entity index entindex, for field fieldname.", 0 );
static void cl_pred_track( const CCommand &args )
{
	g_pChangeTracker->ClearTracking();

	if ( args.ArgC() != 3 )
	{
		Msg( "cl_pred_track <entindex> <fieldname>\n" );
		return;
	}

	int iEntIndex = V_atoi( args[1] );

	CBaseEntity *ent = cl_entitylist->GetBaseEntity( iEntIndex );
	if ( !ent )
	{
		Msg( "cl_pred_track:  Unknown ent index %d\n", iEntIndex );
		return;
   	}

	g_pChangeTracker->SetupTracking( ent, args[2] );
}

using namespace ::ks::reflect;

class CBaseAnimating;

using namespace ::ks::reflect;

extern view_id_t CurrentViewID();
extern bool IsMainView( view_id_t id );

class CProp_Hallucination : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_Prop_Hallucination" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CProp_Hallucination, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	virtual void	Spawn( void );
	virtual void	UpdateOnRemove( void );
	//virtual bool	ShouldDraw( void );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual ShadowType_t ShadowCastType( void );
	virtual void OnDataChanged( DataUpdateType_t type );
	
	COcclusionQuerySet m_OcclusionSet;
	int m_iLastDrawCallFrame;
	bool m_bVisibleInAnyViewLastFrame;
	//float m_fVisibilityRemaining; //an ill defined measure of how much longer we're visible. When it reaches 0 we no longer show the hallucination until it's recharged to 1. Being occluded before reaching 0 is going to be up in the air on what to do.
	double m_fLastStateChangeTime;
	bool m_bVisibleEligible;

	[[= ks::reflect::Net{} ]] bool m_bEnabled;
	[[= ks::reflect::Net{} ]] float m_fVisibleTime;
	[[= ks::reflect::Net{} ]] float m_fRechargeTime;

	static CMaterialReference sm_OcclusionProxyMaterial;
};

CMaterialReference CProp_Hallucination::sm_OcclusionProxyMaterial;

IMPLEMENT_REFLECT_CLIENTCLASS( CProp_Hallucination, DT_Prop_Hallucination, CProp_Hallucination )

//ConVar cl_hallucination_dischargescale( "cl_hallucination_dischargescale", "3000.0", FCVAR_CHEAT, "(delta time) * (percentage of screen drawn to) * (this scale) is how much visibility charge (0.0-1.0) we use up per frame. When it's 0.0 the hallucination is invisible" );
//ConVar cl_hallucination_visibletime( "cl_hallucination_visibletime", "0.215", FCVAR_CHEAT, "the maximum time (in seconds) from first visible frame to when we stop drawing the hallucination" );
//ConVar cl_hallucination_rechargetime( "cl_hallucination_rechargetime", "2.0", FCVAR_CHEAT, "How much time must pass without drawing the hallucination before it's eligble to draw again" );

void CProp_Hallucination::Spawn( void )
{
	if( !sm_OcclusionProxyMaterial.IsValid() )
	{
		sm_OcclusionProxyMaterial.Init( "engine/occlusionproxy", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	BaseClass::Spawn();
}

void CProp_Hallucination::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
}

void CProp_Hallucination::OnDataChanged( DataUpdateType_t type )
{
	if( !m_bEnabled )
	{
		if( m_bVisibleEligible && (m_fLastStateChangeTime != 0.0f) )
		{
			m_fLastStateChangeTime = Plat_FloatTime();
		}
		m_bVisibleEligible = false;
	}
}

int CProp_Hallucination::DrawModel( int flags, const RenderableInstance_t &instance )
{
	switch( CurrentViewID() )
	{
	case VIEW_SHADOW_DEPTH_TEXTURE:
		if( m_bVisibleEligible )
		{
			return BaseClass::DrawModel( flags, instance );
		}

	default:
		if( IsMainView( CurrentViewID() ) ) //VIEW_MAIN and portal views are true
		{
			break;
		}
		else
		{
			return 0;
		}
	};

	bool bSameFrame = (m_iLastDrawCallFrame == gpGlobals->framecount);

	if( (flags & STUDIO_RENDER) && !bSameFrame )
	{
		bool bDrewLastFrame = (m_iLastDrawCallFrame == (gpGlobals->framecount - 1));
		if( m_bVisibleEligible )
		{
			if( m_fLastStateChangeTime == 0.0 ) //waiting for the first frame of actual visibility mark the state change
			{
				if( bDrewLastFrame && (m_OcclusionSet.QueryNumPixelsRenderedForAllViewsLastFrame() > 0) )
				{
					m_fLastStateChangeTime = Plat_FloatTime();// - gpGlobals->realtime; //state change occurred last frame
				}
			}
			else if( (Plat_FloatTime() - m_fLastStateChangeTime) > m_fVisibleTime )
			{
				m_bVisibleEligible = false;
			}
		}
		else
		{
			if( ((m_fLastStateChangeTime == 0.0f) || (Plat_FloatTime() - m_fLastStateChangeTime) > m_fRechargeTime) && 
				((m_OcclusionSet.QueryNumPixelsRenderedForAllViewsLastFrame() == 0) || !bDrewLastFrame) )
			{
				m_bVisibleEligible = true;
				m_fLastStateChangeTime = 0.0f;
			}
		}
	}
	
	if( flags & STUDIO_RENDER )
	{
		if( !m_bVisibleEligible )
		{
			modelrender->ForcedMaterialOverride( sm_OcclusionProxyMaterial );
		}
		m_iLastDrawCallFrame = gpGlobals->framecount;
		m_OcclusionSet.BeginQueryDrawing();
		int iRetVal = BaseClass::DrawModel( flags, instance );
		m_OcclusionSet.EndQueryDrawing();

		if( !m_bVisibleEligible )
		{
			modelrender->ForcedMaterialOverride( nullptr );
		}

		return iRetVal;
	}
	else
	{
		return BaseClass::DrawModel( flags, instance );
	}
}

ShadowType_t CProp_Hallucination::ShadowCastType( void )
{
	return SHADOWS_NONE;
}

class CBaseEntity;
class CCSPlayer;
class CPhysicsProp;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( DynamicProp, DT_DynamicProp )

IMPLEMENT_REFLECT_TABLE( CDynamicProp, DT_DynamicProp );

CDynamicProp::CDynamicProp( void ) : 
m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false )
{
	m_iCachedFrameCount = -1;
}

CDynamicProp::~CDynamicProp( void )
{
}

bool CDynamicProp::TestBoneFollowers( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	// UNDONE: There is no list of the bone followers that is networked to the client
	// so instead we do a search for solid stuff here.  This is not really great - a list would be
	// preferable.
	CBaseEntity	*pList[128];
	Vector mins, maxs;
	CollisionProp()->WorldSpaceAABB( &mins, &maxs );
	int count = UTIL_EntitiesInBox( pList, std::size( pList ), mins, maxs, 0, PARTITION_CLIENT_SOLID_EDICTS );
	for ( int i = 0; i < count; i++ )
	{
		if ( pList[i]->GetOwnerEntity() == this )
		{
			if ( pList[i]->TestCollision(ray, fContentsMask, tr) )
			{
				return true;
			}
		}
	}
	return false;
}

bool CDynamicProp::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( IsSolidFlagSet(FSOLID_NOT_SOLID) )
	{
		// if this entity is marked non-solid and custom test it must have bone followers
		if ( IsSolidFlagSet( FSOLID_CUSTOMBOXTEST ) && IsSolidFlagSet( FSOLID_CUSTOMRAYTEST ))
		{
			return TestBoneFollowers( ray, fContentsMask, tr );
		}
	}
	return BaseClass::TestCollision( ray, fContentsMask, tr );
}

void CDynamicProp::ClientThink( void )
{
	BaseClass::ClientThink();

	UpdateGlow();
}

void CDynamicProp::UpdateGlow( void )
{
	if ( m_bShouldGlow == false )
	{
		if ( m_GlowObject.IsRendering() )
		{
			m_GlowObject.SetRenderFlags( false, false );
			m_GlowObject.SetAlpha( 0.0f );
		}
		return;
	}

	Vector glowColor;
	glowColor.x = (m_clrGlow.r/255.0f);
	glowColor.y = (m_clrGlow.g/255.0f);
	glowColor.z = (m_clrGlow.b/255.0f);

	float flAlpha = 0.9f;

	// fade the alpha based on distace
	CCSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();
	if ( pPlayer && m_bShouldGlow )
	{
		float flDistance = 0;
		flDistance = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length();
		flAlpha = clamp( 1.0 - ( flDistance / m_flGlowMaxDist ), 0.0, 0.9 );
	}
	//m_nGlowStyle

	GlowRenderStyle_t glowstyle = (GlowRenderStyle_t)m_nGlowStyle;

	// Start glowing
	m_GlowObject.SetRenderFlags( m_bShouldGlow, false );
	m_GlowObject.SetRenderStyle( glowstyle );
	m_GlowObject.SetColor( glowColor );
	m_GlowObject.SetAlpha( m_bShouldGlow ? flAlpha : 0.0f );

	SetNextClientThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// implements these so ragdolls can handle frustum culling & leaf visibility
//-----------------------------------------------------------------------------
void CDynamicProp::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( m_bUseHitboxesForRenderBox )
	{
		if ( GetModel() )
		{
			studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( GetModel() );
			if ( !pStudioHdr || GetSequence() == -1 )
			{
				theMins = vec3_origin;
				theMaxs = vec3_origin;
				return;
			}

			// Only recompute if it's a new frame
			if ( gpGlobals->framecount != m_iCachedFrameCount )
			{
				ComputeEntitySpaceHitboxSurroundingBox( &m_vecCachedRenderMins, &m_vecCachedRenderMaxs );
				m_iCachedFrameCount = gpGlobals->framecount;
			}

			theMins = m_vecCachedRenderMins;
			theMaxs = m_vecCachedRenderMaxs;
			return;
		}
	}

	BaseClass::GetRenderBounds( theMins, theMaxs );
}

unsigned int CDynamicProp::ComputeClientSideAnimationFlags()
{
	if ( GetSequence() != -1 )
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		if ( GetSequenceCycleRate(pStudioHdr, GetSequence()) != 0.0f )
		{
			return BaseClass::ComputeClientSideAnimationFlags();
		}
	}

	// no sequence or no cycle rate, don't do any per-frame calcs
	return 0;
}

void CDynamicProp::ForceTurnOffGlow( void )
{
	m_bShouldGlow = false;

	UpdateGlow();
}

void CDynamicProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

//	if ( type == DATA_UPDATE_CREATED )
	{
//		if ( m_bShouldGlow )
		{
			UpdateGlow();
		}
	}
}

// ------------------------------------------------------------------------------------------ //
// ------------------------------------------------------------------------------------------ //
IMPLEMENT_REFLECT_CLIENTCLASS( CBasePropDoor, DT_BasePropDoor, CBasePropDoor )

CBasePropDoor::CBasePropDoor( void )
{
	m_modelChanged = false;
}

CBasePropDoor::~CBasePropDoor( void )
{
}

void CBasePropDoor::PostDataUpdate( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		BaseClass::PostDataUpdate( updateType );
	}
	else
	{
		const model_t *oldModel = GetModel();
		BaseClass::PostDataUpdate( updateType );
		const model_t *newModel = GetModel();

		if ( oldModel != newModel )
		{
			m_modelChanged = true;
		}
	}
}

void CBasePropDoor::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	if ( VPhysicsGetObject() && m_modelChanged )
	{
		VPhysicsDestroyObject();
		m_modelChanged = false;
		bCreate = true;
	}
	VPhysicsShadowDataChanged(bCreate, this);
}

bool CBasePropDoor::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	if ( !VPhysicsGetObject() )
		return false;

	MDLCACHE_CRITICAL_SECTION();
	CStudioHdr *pStudioHdr = GetModelPtr( );
	if (!pStudioHdr)
		return false;

	physcollision->TraceBox( ray, VPhysicsGetObject()->GetCollide(), GetAbsOrigin(), GetAbsAngles(), &trace );

	if ( trace.DidHit() )
	{
		trace.contents = pStudioHdr->contents();
		// use the default surface properties
		trace.surface.name = "**studio**";
		trace.surface.flags = 0;
		trace.surface.surfaceProps = pStudioHdr->GetSurfaceProp();

		return true;
	}

	return false;
}

//just need to reference by classname in portal
class CPropDoorRotating : public CBasePropDoor
{
	[[= ks::reflect::NetTable{ .name = "DT_PropDoorRotating" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPropDoorRotating, CBasePropDoor );
	DECLARE_CLIENTCLASS();
};

IMPLEMENT_REFLECT_CLIENTCLASS( CPropDoorRotating, DT_PropDoorRotating, CPropDoorRotating )

// ------------------------------------------------------------------------------------------ //
// Special version of func_physbox.
// ------------------------------------------------------------------------------------------ //
class CPhysBoxMultiplayer : public CPhysBox, public IMultiplayerPhysics
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysBoxMultiplayer" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPhysBoxMultiplayer, CPhysBox );

	virtual int GetMultiplayerPhysicsMode()
	{
		return m_iPhysicsMode;
	}

	virtual float GetMass()
	{
		return m_fMass;
	}

	virtual bool IsAsleep()
	{
		contract_assert( 0 );
		return true;
	}

	CNetworkVar( int, m_iPhysicsMode, [[= ks::reflect::Net{} ]] );	// One of the PHYSICS_MULTIPLAYER_ defines.	
	CNetworkVar( float, m_fMass, [[= ks::reflect::Net{} ]] );

	DECLARE_CLIENTCLASS();
};

IMPLEMENT_REFLECT_CLIENTCLASS( CPhysBoxMultiplayer, DT_PhysBoxMultiplayer, CPhysBoxMultiplayer )

class CPhysicsPropMultiplayer : public CPhysicsProp, public IMultiplayerPhysics
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysicsPropMultiplayer" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPhysicsPropMultiplayer, CPhysicsProp );

	virtual int GetMultiplayerPhysicsMode()
	{
		contract_assert( m_iPhysicsMode != PHYSICS_MULTIPLAYER_CLIENTSIDE );
		contract_assert( m_iPhysicsMode != PHYSICS_MULTIPLAYER_AUTODETECT );
		return m_iPhysicsMode;
	}

	virtual float GetMass()
	{
		return m_fMass;
	}

	virtual bool IsAsleep()
	{
		return !m_bAwake;
	}

	virtual void ComputeWorldSpaceSurroundingBox( Vector *mins, Vector *maxs )
	{
		contract_assert( mins != nullptr && maxs != nullptr );
		if ( !mins || !maxs )
			return;

		// Take our saved collision bounds, and transform into world space
		TransformAABB( EntityToWorldTransform(), m_collisionMins, m_collisionMaxs, *mins, *maxs );
	}

	CNetworkVar( int, m_iPhysicsMode, [[= ks::reflect::Net{} ]] );	// One of the PHYSICS_MULTIPLAYER_ defines.	
	CNetworkVar( float, m_fMass, [[= ks::reflect::Net{} ]] );
	CNetworkVector( m_collisionMins, [[= ks::reflect::Net{} ]] );
	CNetworkVector( m_collisionMaxs, [[= ks::reflect::Net{} ]] );

	DECLARE_CLIENTCLASS();
};

IMPLEMENT_REFLECT_CLIENTCLASS( CPhysicsPropMultiplayer, DT_PhysicsPropMultiplayer, CPhysicsPropMultiplayer )

class CBaseAnimating;
class CBaseEntity;
class CPhysicsProp;

using namespace ::ks::reflect;

// Purpose: static_prop - don't move, don't animate, don't do anything.
//			physics_prop - move, take damage, but don't animate

ConVar sv_pushaway_clientside_size( side_registry(), "sv_pushaway_clientside_size", "15", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Minimum size of pushback objects" );
ConVar props_break_max_pieces( side_registry(), "props_break_max_pieces", false ? "32" : "-1", FCVAR_REPLICATED, "Maximum prop breakable piece count (-1 = model default)" );
ConVar props_break_max_pieces_perframe( side_registry(), "props_break_max_pieces_perframe", false ? "10" : "-1", FCVAR_REPLICATED, "Maximum prop breakable piece count per frame (-1 = model default)" );
ConVar cl_burninggibs( side_registry(), "cl_burninggibs", "0", 0, "A burning player that gibs has burning gibs." );

extern bool PropBreakableCapEdictsOnCreateAll( std::vector< breakmodel_t > &list, IPhysicsObject *pPhysics, const breakablepropparams_t &params, CBaseEntity *pEntity, int iPrecomputedBreakableCount = -1 );
extern CBaseEntity *BreakModelCreateSingle( CBaseEntity *pOwner, breakmodel_t *pModel, const Vector &position, 
	const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, int nSkin, const breakablepropparams_t &params );

static int nPropBreakablesPerFrameCount = 0;
static int nFrameNumber = 0;

//=============================================================================================================
// UTILITY FUNCS
//=============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: returns the axis index with the greatest size
// Input  : &vec - 
// Output : static int
//-----------------------------------------------------------------------------
static int GreatestAxis( const Vector &vec )
{
	if ( vec.x > vec.y )
	{
		if ( vec.x > vec.z )
			return 0;
		return 2;
	}
	if ( vec.y > vec.z )
		return 1;
	return 2;
}

//-----------------------------------------------------------------------------
// Purpose: returns the axis index with the smallest size
// Input  : &vec - 
// Output : static int
//-----------------------------------------------------------------------------
static int SmallestAxis( const Vector &vec )
{
	if ( vec.x < vec.y )
	{
		if ( vec.x < vec.z )
			return 0;
		return 2;
	}
	if ( vec.y < vec.z )
		return 1;
	return 2;
}

//-----------------------------------------------------------------------------
// Purpose: Rotates a matrix by 90 degrees in the plane of axis0/axis1
// Input  : &matrix - 
//			axis0 - 
//			axis1 - 
// Output : static void
//-----------------------------------------------------------------------------
static void MatrixRot90( matrix3x4_t &matrix, int axis0, int axis1 )
{
	Vector col0, col1;
	MatrixGetColumn( matrix, axis0, col0 );
	MatrixGetColumn( matrix, axis1, col1 );
	MatrixSetColumn( col1, axis0, matrix );
	MatrixSetColumn( -col0, axis1, matrix );
}
 
//-----------------------------------------------------------------------------
// Purpose: Given two symmetric boxes, rotate the coordinate frame by the necessary
//			90 degree rotations to approximately align them
// Input  : *pInOutMatrix - 
//			&boxExtents1 - 
//			&boxExtents2 - 
//-----------------------------------------------------------------------------
static void AlignBoxes( matrix3x4_t *pInOutMatrix, const Vector &boxExtents1, const Vector &boxExtents2 )
{
	int rotCount = 0;
	struct 
	{
		int axis0;
		int axis1;
	} rotations[2];
	Vector ext1 = boxExtents1;
	Vector ext2 = boxExtents2;

	int axis0 = GreatestAxis( ext1 );
	int axis1 = GreatestAxis( ext2 );
	if ( axis0 != axis1 )
	{
		rotations[rotCount].axis0 = axis0;
		rotations[rotCount].axis1 = axis1;
		rotCount++;
		ext2[axis1] = ext2[axis0];
	}
	ext1[axis0] = 0;
	ext2[axis0] = 0;

	axis0 = GreatestAxis(ext1);
	axis1 = GreatestAxis(ext2);
	if ( axis0 != axis1 )
	{
		rotations[rotCount].axis0 = axis0;
		rotations[rotCount].axis1 = axis1;
		rotCount++;
	}

	while ( rotCount > 0 )
	{
		rotCount--;
		MatrixRot90( *pInOutMatrix, rotations[rotCount].axis0, rotations[rotCount].axis1 );
	}
}

//=============================================================================================================
// PROP DATA
//=============================================================================================================
CPropData g_PropDataSystem;

// Parsing details for each of the propdata interactions
struct propdata_interaction_s
{
	const char *pszSectionName;
	const char *pszKeyName;
	const char *pszValue;
	int m_keySection;
	int m_keyKeyName;
};

propdata_interaction_s sPropdataInteractionSections[PROPINTER_NUM_INTERACTIONS] =
{
	{ "physgun_interactions", "onworldimpact", "stick", -1, -1 },		// PROPINTER_PHYSGUN_WORLD_STICK,
	{ "physgun_interactions", "onfirstimpact", "break", -1, -1 },		// PROPINTER_PHYSGUN_FIRST_BREAK,
	{ "physgun_interactions", "onfirstimpact", "paintsplat", -1, -1 },	// PROPINTER_PHYSGUN_FIRST_PAINT,
	{ "physgun_interactions", "onfirstimpact", "impale", -1, -1 },		// PROPINTER_PHYSGUN_FIRST_IMPALE,
	{ "physgun_interactions", "onlaunch", "spin_none", -1, -1 },		// PROPINTER_PHYSGUN_LAUNCH_SPIN_NONE,
	{ "physgun_interactions", "onlaunch", "spin_zaxis", -1, -1 },		// PROPINTER_PHYSGUN_LAUNCH_SPIN_Z,
	{ "physgun_interactions", "onbreak", "explode_fire", -1, -1 },		// PROPINTER_PHYSGUN_BREAK_EXPLODE,
	{ "physgun_interactions", "onbreak", "explode_ice", -1, -1 },		// PROPINTER_PHYSGUN_BREAK_EXPLODE_ICE,
	{ "physgun_interactions", "damage", "none", -1, -1 },				// PROPINTER_PHYSGUN_DAMAGE_NONE,
	
	{ "fire_interactions", "flammable", "yes", -1, -1 },				// PROPINTER_FIRE_FLAMMABLE,
	{ "fire_interactions", "explosive_resist", "yes", -1, -1 },			// PROPINTER_FIRE_EXPLOSIVE_RESIST,
	{ "fire_interactions", "ignite", "halfhealth", -1, -1 },			// PROPINTER_FIRE_IGNITE_HALFHEALTH,

	{ "physgun_interactions", "onpickup", "create_flare", -1, -1 },		// PROPINTER_PHYSGUN_CREATE_FLARE,

	{ "physgun_interactions", "allow_overhead", "yes", -1, -1 },		// 	PROPINTER_PHYSGUN_ALLOW_OVERHEAD,

	{ "world_interactions", "onworldimpact", "bloodsplat", -1, -1 },	// PROPINTER_WORLD_BLOODSPLAT,
	{ "physgun_interactions", "physgun_notify_children", "yes", -1, -1 },// PROPINTER_PHYSGUN_NOTIFY_CHILDREN,
	{ "fire_interactions", "melee_immune", "yes", -1, -1 },				// PROPINTER_MELEE_IMMUNE,	
};

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CPropData::CPropData( void ) : 
	CAutoGameSystem( "CPropData" )
{
	m_bPropDataLoaded = false;
	m_pKVPropData = nullptr;
	for ( int i = 0; i < PROPINTER_NUM_INTERACTIONS; i++ )
	{
		sPropdataInteractionSections[i].m_keySection = KeyValuesSystem()->GetSymbolForString( sPropdataInteractionSections[i].pszSectionName );
		sPropdataInteractionSections[i].m_keyKeyName = KeyValuesSystem()->GetSymbolForString( sPropdataInteractionSections[i].pszKeyName );
	}
}

//-----------------------------------------------------------------------------
// Inherited from IAutoServerSystem
//-----------------------------------------------------------------------------
void CPropData::LevelInitPreEntity( void )
{
	m_BreakableChunks.clear();
	ParsePropDataFile();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropData::LevelShutdownPostEntity( void )
{
	if ( m_pKVPropData )
	{
		m_pKVPropData->deleteThis();
		m_pKVPropData = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Clear out the stats + their history
//-----------------------------------------------------------------------------
void CPropData::ParsePropDataFile( void )
{
	m_pKVPropData = new KeyValues( "PropDatafile" );
	if ( !m_pKVPropData->LoadFromFile( filesystem, "scripts/propdata.txt" ) )
	{
		m_pKVPropData->deleteThis();
		m_pKVPropData = nullptr;
		return;
	}

	m_bPropDataLoaded = true;

	// Now try and parse out the breakable section
	KeyValues *pBreakableSection = m_pKVPropData->FindKey( "BreakableModels" );
	if ( pBreakableSection )
	{
		KeyValues *pChunkSection = pBreakableSection->GetFirstSubKey();
		while ( pChunkSection )
		{
			// Create a new chunk section and add it to our list
			int index = std::ssize( m_BreakableChunks ); m_BreakableChunks.emplace_back();
			propdata_breakablechunk_t *pBreakableChunk = &m_BreakableChunks[index];
			pBreakableChunk->iszChunkType = AllocPooledString( pChunkSection->GetName() );

			// Read in all the model names
			KeyValues *pModelName = pChunkSection->GetFirstSubKey();
			while ( pModelName )
			{
				const char *pModel = pModelName->GetName();
				string_t pooledName = AllocPooledString( pModel );
				pBreakableChunk->iszChunkModels.push_back( pooledName );
				CBaseEntity::PrecacheModel( STRING( pooledName ) );

				pModelName = pModelName->GetNextKey();
			}

			pChunkSection = pChunkSection->GetNextKey();
		}
	}
}	

//-----------------------------------------------------------------------------
// Purpose: Parse a keyvalues section into the prop
//
//			pInteractionSection is a bit of jiggery-pokery to get around the unfortunate
//			fact that the interaction KV sections ("physgun_interactions", "fire_interactions", etc)
//			are OUTSIDE the "prop_data" KV section in the model, but may be contained WITHIN the 
//			specified Base's "prop_data" section (i.e. in propdata.txt)
//-----------------------------------------------------------------------------
int CPropData::ParsePropFromKV( CBaseEntity *pProp, IBreakableWithPropData *pBreakableInterface, KeyValues *pSection, KeyValues *pInteractionSection )
{
	if ( !pBreakableInterface )
		return PARSE_FAILED_BAD_DATA;

	if ( !pBreakableInterface )
		return PARSE_FAILED_BAD_DATA;

	int iBaseResult = PARSE_SUCCEEDED;

	// OPTIMIZE: keep these static so we don't have to look up these strings every time we create a prop
	static int keyBase = KeyValuesSystem()->GetSymbolForString( "base" );
	static int keyBlockLOS = KeyValuesSystem()->GetSymbolForString( "blockLOS" );
	static int keyAIWalkable = KeyValuesSystem()->GetSymbolForString( "AIWalkable" );
	static int keyDamageTable = KeyValuesSystem()->GetSymbolForString( "damage_table" );
	static int keyPhysicsMode = KeyValuesSystem()->GetSymbolForString( "physicsmode" );
	static int keyMultiplayerBreak = KeyValuesSystem()->GetSymbolForString( "multiplayer_break" );
	static int keyDmgBullets = KeyValuesSystem()->GetSymbolForString( "dmg.bullets" );
	static int keyDmgClub = KeyValuesSystem()->GetSymbolForString( "dmg.club" );
	static int keyDmgExplosive = KeyValuesSystem()->GetSymbolForString( "dmg.explosive" );
	static int keyHealth = KeyValuesSystem()->GetSymbolForString( "health" );
	static int keyBreakableModel = KeyValuesSystem()->GetSymbolForString( "breakable_model" );
	static int keyBreakableSkin = KeyValuesSystem()->GetSymbolForString( "breakable_skin" );
	static int keyBreakableCount = KeyValuesSystem()->GetSymbolForString( "breakable_count" );
	static int keyExplosiveDamage = KeyValuesSystem()->GetSymbolForString( "explosive_damage" );
	static int keyExplosiveRadius = KeyValuesSystem()->GetSymbolForString( "explosive_radius" );
	static int keyAllowStatic = KeyValuesSystem()->GetSymbolForString( "allowstatic" );
	
	// Do we have a base?
	char const *pszBase = pSection->GetString( keyBase );
	if ( pszBase && pszBase[0] )
	{
		iBaseResult = ParsePropFromBase( pProp, pBreakableInterface, pszBase );
		if ( (iBaseResult != PARSE_SUCCEEDED) && (iBaseResult != PARSE_SUCCEEDED_ALLOWED_STATIC) )
			return iBaseResult;
	}

	// Allow overriding of Block LOS
	int iBlockLOS = pSection->GetFloat( keyBlockLOS, -1 );
	if ( iBlockLOS != -1 )
	{
		pBreakableInterface->SetPropDataBlocksLOS( iBlockLOS != 0 );
	}

	// Set whether AI can walk on this prop
	int iIsWalkable = pSection->GetFloat( keyAIWalkable, -1 );
	if ( iIsWalkable != -1 )
	{
		pBreakableInterface->SetPropDataIsAIWalkable( iIsWalkable != 0 );
	}

	// Set custom damage table
	const char *pszTableName;
	if ( pBreakableInterface->GetPhysicsDamageTable() == NULL_STRING )
	{
		pszTableName = pSection->GetString( keyDamageTable, nullptr );
	}
	else
	{
		pszTableName = pSection->GetString( keyDamageTable, STRING(pBreakableInterface->GetPhysicsDamageTable()) );
	}
	if ( pszTableName && pszTableName[0] )
	{
		pBreakableInterface->SetPhysicsDamageTable( AllocPooledString( pszTableName ) );
	}
	else
	{
		pBreakableInterface->SetPhysicsDamageTable( NULL_STRING );
	}

	// Get multiplayer physics mode if not set by map
	pBreakableInterface->SetPhysicsMode( pSection->GetInt( keyPhysicsMode, pBreakableInterface->GetPhysicsMode() ) );

	const char *multiplayer_break = pSection->GetString( keyMultiplayerBreak, nullptr );
	if ( multiplayer_break )
	{
		mp_break_t mode = MULTIPLAYER_BREAK_DEFAULT;
		if ( FStrEq( multiplayer_break, "server" ) )
		{
			mode = MULTIPLAYER_BREAK_SERVERSIDE;
		}
		else if ( FStrEq( multiplayer_break, "client" ) )
		{
			mode = MULTIPLAYER_BREAK_CLIENTSIDE;
		}
		else if ( FStrEq( multiplayer_break, "both" ) )
		{
			mode = MULTIPLAYER_BREAK_BOTH;
		}
		pBreakableInterface->SetMultiplayerBreakMode( mode );
	}

	// Get damage modifiers, but only if they're specified, because our base may have already overridden them.
	pBreakableInterface->SetDmgModBullet( pSection->GetFloat( keyDmgBullets, pBreakableInterface->GetDmgModBullet() ) );
	pBreakableInterface->SetDmgModClub( pSection->GetFloat( keyDmgClub, pBreakableInterface->GetDmgModClub() ) );
	pBreakableInterface->SetDmgModExplosive( pSection->GetFloat( keyDmgExplosive, pBreakableInterface->GetDmgModExplosive() ) );

	// Get the health (unless this is an override prop)
	if ( !FClassnameIs( pProp, "prop_physics_override" ) && !FClassnameIs( pProp, "prop_dynamic_override" ) )
	{
		pProp->SetHealth( pSection->GetInt( keyHealth, pProp->GetHealth() ) );

		// Explosive?
		pBreakableInterface->SetExplosiveDamage( pSection->GetFloat( keyExplosiveDamage, pBreakableInterface->GetExplosiveDamage() ) );
		pBreakableInterface->SetExplosiveRadius( pSection->GetFloat( keyExplosiveRadius, pBreakableInterface->GetExplosiveRadius() ) );

	}

	const char *pszBreakableModel;
	if ( pBreakableInterface->GetBreakableModel() == NULL_STRING )
	{
		pszBreakableModel = pSection->GetString( keyBreakableModel, nullptr );
	}
	else
	{
		pszBreakableModel = pSection->GetString( keyBreakableModel, STRING(pBreakableInterface->GetBreakableModel()) );
	}
	if ( pszBreakableModel && pszBreakableModel[0] )
	{
		pBreakableInterface->SetBreakableModel( AllocPooledString( pszBreakableModel ) );
	}
	else
	{
		pBreakableInterface->SetBreakableModel( NULL_STRING );
	}
	pBreakableInterface->SetBreakableSkin( pSection->GetInt( keyBreakableSkin, pBreakableInterface->GetBreakableSkin() ) );
	pBreakableInterface->SetBreakableCount( pSection->GetInt( keyBreakableCount, pBreakableInterface->GetBreakableCount() ) );

	// Calculate the maximum size of the breakables this breakable will produce
	Vector vecSize = pProp->CollisionProp()->OBBSize();
	// Throw away the smallest coord
	int iSmallest = SmallestAxis(vecSize);
	vecSize[iSmallest] = 1;
	float flVolume = vecSize.x * vecSize.y * vecSize.z;
	int iMaxSize = std::floor( flVolume / (32.0*32.0) );
	pBreakableInterface->SetMaxBreakableSize( iMaxSize );

	// Now parse our interactions
	for ( int i = 0; i < PROPINTER_NUM_INTERACTIONS; i++ )
	{
		// If we hit this assert, we have too many interactions for our current storage solution to handle
		contract_assert( i < 32 );

		propdata_interaction_s *pInteraction = &sPropdataInteractionSections[i];
		if ( !pInteraction->pszSectionName )
			continue;

		KeyValues *pkvCurrentInter = pInteractionSection->FindKey( pInteraction->m_keySection );
		if ( pkvCurrentInter )
		{
			char const *pszInterBase = pkvCurrentInter->GetString( pInteraction->m_keyKeyName );
			if ( pszInterBase && pszInterBase[0] && !stricmp( pszInterBase, pInteraction->pszValue ) )
			{
				pBreakableInterface->SetInteraction( (propdata_interactions_t)i );
			}
		}
	}

	// If the base said we're allowed to be static, return that
	if ( iBaseResult == PARSE_SUCCEEDED_ALLOWED_STATIC )
		return PARSE_SUCCEEDED_ALLOWED_STATIC;

	// Otherwise, see if our propdata says we are allowed to be static
	if ( pSection->GetInt( keyAllowStatic, 0 ) )
		return PARSE_SUCCEEDED_ALLOWED_STATIC;

	return PARSE_SUCCEEDED;
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a prop's with base data parsed from the propdata file
//-----------------------------------------------------------------------------
int CPropData::ParsePropFromBase( CBaseEntity *pProp, IBreakableWithPropData *pBreakableInterface, const char *pszPropData )
{
	if ( !m_bPropDataLoaded )
		return PARSE_FAILED_NO_DATA;

	if ( !pBreakableInterface )
		return PARSE_FAILED_BAD_DATA;

	if ( !m_pKVPropData )
	{
		return PARSE_FAILED_BAD_DATA;
	}
	
	// Find the specified propdata
	KeyValues *pSection = m_pKVPropData->FindKey( pszPropData );
	if ( !pSection )
	{
		Warning("%s '%s' has a base specified as '%s', but there is no matching entry in propdata.txt.\n", pProp->GetClassname(), STRING( pProp->GetModelName() ), pszPropData );
		return PARSE_FAILED_BAD_DATA;
	}

	// Store off the first base data for debugging
	if ( pBreakableInterface->GetBasePropData() == NULL_STRING )
	{
		pBreakableInterface->SetBasePropData( AllocPooledString( pszPropData ) );
	}

	return ParsePropFromKV( pProp, pBreakableInterface, pSection, pSection );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPropData::GetRandomChunkModel( const char *pszBreakableSection, int iMaxSize )
{
	if ( !m_bPropDataLoaded )
		return nullptr;

	// Find the right section
	int iCount = std::ssize( m_BreakableChunks );
	int i;
	for ( i = 0; i < iCount; i++ )
	{
		if ( StringHasPrefixCaseSensitive( STRING(m_BreakableChunks[i].iszChunkType), pszBreakableSection ) )
			break;
	}
	if ( i == iCount )
		return nullptr;

	// Now pick a random one and return it
	int iRandom;
	if ( iMaxSize == -1 )
	{
		iRandom = RandomInt( 0, std::ssize( m_BreakableChunks[i].iszChunkModels )-1 );
	}
	else
	{
		// Don't pick anything over the specified size
		iRandom = RandomInt( 0, std::min<std::common_type_t<int, std::common_type_t<long int, long int>>>(iMaxSize, std::ssize( m_BreakableChunks[i].iszChunkModels )-1) );
	}

	return STRING(m_BreakableChunks[i].iszChunkModels[iRandom]);
}

// ensure that a model name from a qc file is properly formatted
static const char *FixupModelName( char *pOut, int sizeOut, const char *pModelNameIn )
{
	char tmp[1024];

	V_strncpy( tmp, pModelNameIn, sizeof(tmp) );
	if ( !StringHasPrefix( tmp, "models/" ) )
	{
		V_snprintf( pOut, sizeOut, "models/%s", tmp );
	}
	else
	{
		V_strncpy( pOut, tmp, sizeOut);
	}
	int len = V_strlen(pOut);
	if ( len < 4 || V_stricmp( pOut + (len-4), ".mdl" ) )
	{
		V_strncat( pOut, ".mdl", sizeOut, COPY_ALL_CHARACTERS );
	}

	return pOut;
}

//-----------------------------------------------------------------------------
// breakable prop functions
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
// list of models to break into
class CBreakParser : public IVPhysicsKeyHandler
{
public:
	CBreakParser( float defaultBurstScale, int defaultCollisionGroup ) 
		: m_defaultBurstScale(defaultBurstScale), m_defaultCollisionGroup(defaultCollisionGroup) {}

	void ParseModelName( breakmodel_t *pModel, const char *pValue )
	{
		FixupModelName( pModel->modelName, sizeof(pModel->modelName), pValue );
	}
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue )
	{
		breakmodel_t *pModel = (breakmodel_t *)pData;
		if ( !strcmpi( pKey, "model" ) )
		{
			ParseModelName( pModel, pValue );
		}
		else if (!strcmpi( pKey, "ragdoll" ) )
		{
			ParseModelName( pModel, pValue );
			pModel->isRagdoll = true;
		}
		else if (!strcmpi( pKey, "motiondisabled" ) )
		{
			pModel->isMotionDisabled = true;
		}
		else if ( !strcmpi( pKey, "offset" ) )
		{
			UTIL_StringToVector( pModel->offset.Base(), pValue );
		}
		else if ( !strcmpi( pKey, "health" ) )
		{
			pModel->health = std::atof(pValue);
		}
		else if ( !strcmpi( pKey, "fadetime" ) )
		{
			pModel->fadeTime = std::atof(pValue);
			if ( !m_wroteCollisionGroup )
			{
				pModel->collisionGroup = COLLISION_GROUP_DEBRIS;
			}
		}
		else if ( !strcmpi( pKey, "fademindist" ) )
		{
			pModel->fadeMinDist = std::atof(pValue);
		}
		else if ( !strcmpi( pKey, "fademaxdist" ) )
		{
			pModel->fadeMaxDist = std::atof(pValue);
		}
		else if ( !strcmpi( pKey, "debris" ) )
		{
			pModel->collisionGroup = std::atoi(pValue) > 0 ? COLLISION_GROUP_DEBRIS : COLLISION_GROUP_INTERACTIVE;
			m_wroteCollisionGroup = true;
		}
		else if ( !strcmpi( pKey, "burst" ) )
		{
			pModel->burstScale = std::atof( pValue );
		}
		else if ( !strcmpi( pKey, "placementbone" ) )
		{
			V_strncpy( pModel->placementName, pValue, sizeof(pModel->placementName) );
			pModel->placementIsBone = true;
		}
		else if ( !strcmpi( pKey, "placementattachment" ) )
		{
			V_strncpy( pModel->placementName, pValue, sizeof(pModel->placementName) );
			pModel->placementIsBone = false;
		}
		else if ( !strcmpi( pKey, "multiplayer_break" ) )
		{
			if ( FStrEq( pValue, "server" ) )
			{
				pModel->mpBreakMode = MULTIPLAYER_BREAK_SERVERSIDE;
			}
			else if ( FStrEq( pValue, "client" ) )
			{
				pModel->mpBreakMode = MULTIPLAYER_BREAK_CLIENTSIDE;
			}
		}
	}
	virtual void SetDefaults( void *pData ) 
	{
		breakmodel_t *pModel = (breakmodel_t *)pData;
		pModel->modelName[0] = 0;
		pModel->offset = vec3_origin;
		pModel->health = 1;
		pModel->fadeTime = 20.0f;
		pModel->fadeMinDist = 0.0f;
		pModel->fadeMaxDist = 0.0f;
		pModel->burstScale = m_defaultBurstScale;
		pModel->collisionGroup = m_defaultCollisionGroup;
		pModel->isRagdoll = false;
		pModel->isMotionDisabled = false;
		pModel->placementName[0] = 0;
		pModel->placementIsBone = false;
		pModel->mpBreakMode = MULTIPLAYER_BREAK_DEFAULT;
		m_wroteCollisionGroup = false;
	}

private:
	int		m_defaultCollisionGroup;
	float	m_defaultBurstScale;
	bool	m_wroteCollisionGroup;
};

void BreakModelList( std::vector< breakmodel_t > &list, int modelindex, float defBurstScale, int defCollisionGroup )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( modelindex );
	if ( !pCollide )
		return;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		CBreakParser breakParser( defBurstScale, defCollisionGroup );
		
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "break" ) )
		{
			int index = std::ssize( list ); list.emplace_back();
			breakmodel_t &breakModel = list[index];
			pParse->ParseCustom( &breakModel, &breakParser );
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );
}

int GetAutoMultiplayerPhysicsMode( Vector size, float mass )
{
	float volume = size.x * size.y * size.z;

	float minsize = sv_pushaway_clientside_size.GetFloat();

	// if it's too small, client side only
	if ( volume < (minsize*minsize*minsize) )
		return PHYSICS_MULTIPLAYER_CLIENTSIDE;

	// if it's too light, no player pushback
	if ( mass < 8.0 )
		return PHYSICS_MULTIPLAYER_NON_SOLID;

	// full pushbackmode
	return PHYSICS_MULTIPLAYER_SOLID;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string describing a real-world equivalent mass.
// Input  : flMass - mass in kg
//-----------------------------------------------------------------------------
const char *GetMassEquivalent(float flMass)
{
	static struct
	{
		float flMass;
		const char *sz;
	} masstext[] =
	{
		{ 5e-6,		"snowflake" },
		{ 2.5e-3,	"ping-pong ball" },
		{ 5e-3,		"penny" },
		{ 0.05,		"golf ball" },
		{ 0.17,		"billard ball" },
		{ 2,		"bag of sugar" },
		{ 7,		"male cat" },
		{ 10,		"bowling ball" },
		{ 30,		"dog" },
		{ 60,		"cheetah" },
		{ 90,		"adult male human" },
		{ 250,		"refrigerator" },
		{ 600,		"race horse" },
		{ 1000,		"small car" },
		{ 1650,		"medium car" },
		{ 2500,		"large car" },
		{ 6000,		"t-rex" },
		{ 7200,		"elephant" },
		{ 8e4,		"space shuttle" },
		{ 7e5,		"locomotive" },
		{ 9.2e6,	"Eiffel tower" },
		{ 6e24,		"the Earth" },
		{ 7e24,		"really freaking heavy" },
	};

	for (int i = 0; i < sizeof(masstext) / sizeof(masstext[0]) - 1; i++)
	{
		if (flMass < masstext[i].flMass)
		{
			return masstext[i].sz;
		}
	}

	return masstext[ sizeof(masstext) / sizeof(masstext[0]) - 1 ].sz;
}

void PropBreakableCreateAll( int modelindex, IPhysicsObject *pPhysics, const breakablepropparams_t &params, CBaseEntity *pEntity, int iPrecomputedBreakableCount, bool bIgnoreGibLimit, bool defaultLocation )
{
        // Check for prop breakable count reset. 
	int nPropCount = props_break_max_pieces_perframe.GetInt(); 
	if ( nPropCount != -1 ) 
	{ 
		if ( nFrameNumber != gpGlobals->framecount ) 
		{ 
			nPropBreakablesPerFrameCount = 0; 
			nFrameNumber = gpGlobals->framecount; 
		} 
      
		// Check for max breakable count for the frame. 
		if ( nPropBreakablesPerFrameCount >= nPropCount ) 
			return; 
	} 
      
	int iMaxBreakCount = bIgnoreGibLimit ? -1 : props_break_max_pieces.GetInt();
	if ( iMaxBreakCount != -1 )
	{
		if ( iPrecomputedBreakableCount != -1 )
		{
			iPrecomputedBreakableCount = std::min( iMaxBreakCount, iPrecomputedBreakableCount );
		}
		else
		{
			iPrecomputedBreakableCount = iMaxBreakCount;
		}
	}

	vcollide_t *pCollide = modelinfo->GetVCollide( modelindex );
	if ( !pCollide )
		return;

	int nSkin = 0;
	CBaseEntity *pOwnerEntity = pEntity;
	CBaseAnimating *pOwnerAnim = nullptr;
	if ( pPhysics )
	{
		pOwnerEntity = static_cast<CBaseEntity *>(pPhysics->GetGameData());
	}
	if ( pOwnerEntity )
	{
		pOwnerAnim = pOwnerEntity->GetBaseAnimating();
		if ( pOwnerAnim )
		{
			nSkin = pOwnerAnim->GetSkin();
		}
	}
	matrix3x4_t localToWorld;

	CStudioHdr parentStudioHdr;
	const model_t *model = modelinfo->GetModel( modelindex );
	if ( model )
	{
		parentStudioHdr.Init( modelinfo->GetStudiomodel( model ) );
	}

	Vector parentOrigin = vec3_origin;
	int parentAttachment = 	Studio_FindAttachment( &parentStudioHdr, "placementOrigin" ) + 1;
	if ( parentAttachment > 0 )
	{
		GetAttachmentLocalSpace( &parentStudioHdr, parentAttachment-1, localToWorld );
		MatrixGetColumn( localToWorld, 3, parentOrigin );
	}
	else
	{
		AngleMatrix( vec3_angle, localToWorld );
	}

	// Search for a burst center on the parent
	matrix3x4_t matrix;
	AngleMatrix( params.angles, params.origin, matrix );

	Vector burstCenter = params.origin;
	const int parentBurstCenterAttachment = Studio_FindAttachment( &parentStudioHdr, "burstCenter" ) + 1;
	if( parentBurstCenterAttachment > 0 )
	{
		if( pOwnerAnim )
		{
			matrix3x4_t burstCenterTransform;
			pOwnerAnim->GetAttachment( parentBurstCenterAttachment, burstCenterTransform );
			MatrixGetColumn( burstCenterTransform, 3, burstCenter );
		}
		else
		{
			GetAttachmentLocalSpace( &parentStudioHdr, parentBurstCenterAttachment - 1, localToWorld );
			MatrixGetColumn( localToWorld, 3, burstCenter );
			VectorTransform( burstCenter - parentOrigin, matrix, burstCenter );
		}
	}
	
	std::vector< breakmodel_t > list;

	list.reserve( 20 );
	BreakModelList( list, modelindex, params.defBurstScale, params.defCollisionGroup );

	if ( !list.empty() )
	{
		for ( int i = 0; i < std::ssize( list ); i++ )
		{
			const char *modelName = list[i].modelName;
			int modelIndex = modelinfo->GetModelIndex( modelName );
			if ( modelIndex <= 0 )
			{
				Warning( "Unable to create non-precached breakmodel %s\n", modelName );
				continue;
			}

			// Skip multiplayer pieces that should be spawning on the other dll
			if ( gpGlobals->maxClients > 1 )
			{
				if ( list[i].mpBreakMode == MULTIPLAYER_BREAK_SERVERSIDE )
					continue;

				if ( !defaultLocation && list[i].mpBreakMode == MULTIPLAYER_BREAK_DEFAULT )
					continue;
			}

			if ( ( nPropCount != -1 ) && ( nPropBreakablesPerFrameCount > nPropCount ) )
				break;

			if ( ( iPrecomputedBreakableCount != -1 ) && ( i >= iPrecomputedBreakableCount ) )
				break;

			CStudioHdr studioHdr;
			const model_t *model = modelinfo->GetModel( modelIndex );
			if ( model )
			{
				studioHdr.Init( modelinfo->GetStudiomodel( model ) );
			}

			// Increment the number of breakable props this frame.
			++nPropBreakablesPerFrameCount;

			Vector position = vec3_origin;
			QAngle angles = params.angles;
			if ( pOwnerAnim && list[i].placementName[0] )
			{
				if ( list[i].placementIsBone )
				{
					int boneIndex = pOwnerAnim->LookupBone( list[i].placementName );
					if ( boneIndex >= 0 )
					{
						pOwnerAnim->GetBonePosition( boneIndex, position, angles );
						AngleMatrix( angles, position, matrix );
					}
				}
				else
				{
					int attachmentIndex = Studio_FindAttachment( &studioHdr, list[i].placementName ) + 1;
					if ( attachmentIndex > 0 )
					{
						pOwnerAnim->GetAttachment( attachmentIndex, matrix );
						MatrixAngles( matrix, angles );
					}
				}
			}
			else
			{
				int placementIndex = Studio_FindAttachment( &studioHdr, "placementOrigin" ) + 1;
				Vector placementOrigin = parentOrigin;
				if ( placementIndex > 0 )
				{
					GetAttachmentLocalSpace( &studioHdr, placementIndex-1, localToWorld );
					MatrixGetColumn( localToWorld, 3, placementOrigin );
					placementOrigin -= parentOrigin;
				}

				VectorTransform( list[i].offset - placementOrigin, matrix, position );
			}
			Vector objectVelocity = params.velocity;

			if (pPhysics)
			{
				pPhysics->GetVelocityAtPoint( position, &objectVelocity );
			}

			int nActualSkin = nSkin;
			if ( nActualSkin > studioHdr.numskinfamilies() )
				nActualSkin = 0;

			CBaseEntity *pBreakable = nullptr;
			
			{
				pBreakable = BreakModelCreateSingle( pOwnerEntity, &list[i], position, angles, objectVelocity, params.angularVelocity, nActualSkin, params );
			}

			if ( pBreakable )
			{
				if ( pOwnerEntity && pOwnerEntity->IsEffectActive( EF_NOSHADOW ) )
				{
					pBreakable->AddEffects( EF_NOSHADOW );
				}

				// If burst scale is set, this piece should 'burst' away from
				// the origin in addition to travelling in the wished velocity.
				if ( list[i].burstScale != 0.0 )
				{
					Vector vecBurstDir = position - burstCenter;

					// If $autocenter wasn't used, try the center of the piece
					if ( vecBurstDir == vec3_origin )
					{
						vecBurstDir = pBreakable->WorldSpaceCenter() - burstCenter;
					}

					VectorNormalize( vecBurstDir );

					pBreakable->ApplyAbsVelocityImpulse( vecBurstDir * list[i].burstScale );
				}

				// If this piece is supposed to be motion disabled, disable it
				if ( list[i].isMotionDisabled )
				{
					IPhysicsObject *pPhysicsObject = pBreakable->VPhysicsGetObject();
					if ( pPhysicsObject != nullptr )
					{
						pPhysicsObject->EnableMotion( false );
					}
				}

				if ( pEntity )
				{
					// Set the same render color as the parent.
					color24 renderColor = pEntity->GetRenderColor();
					pBreakable->SetRenderColor( renderColor.r, renderColor.g, renderColor.b );
				}

			}
		}
	}
	// Then see if the propdata specifies any breakable pieces
	else if ( pEntity )
	{
		IBreakableWithPropData *pBreakableInterface = dynamic_cast<IBreakableWithPropData*>(pEntity);
		if ( pBreakableInterface && pBreakableInterface->GetBreakableModel() != NULL_STRING && pBreakableInterface->GetBreakableCount() )
		{
			breakmodel_t breakModel;

			for ( int i = 0; i < pBreakableInterface->GetBreakableCount(); i++ )
			{
				if ( ( iPrecomputedBreakableCount != -1 ) && ( i >= iPrecomputedBreakableCount ) )
					break;

				V_strncpy( breakModel.modelName, g_PropDataSystem.GetRandomChunkModel(STRING(pBreakableInterface->GetBreakableModel()), pBreakableInterface->GetMaxBreakableSize()), sizeof(breakModel.modelName) );

				breakModel.health = 1;
				breakModel.fadeTime = RandomFloat(5,10);
				breakModel.fadeMinDist = 0.0f;
				breakModel.fadeMaxDist = 0.0f;
				breakModel.burstScale = params.defBurstScale;
				breakModel.collisionGroup = COLLISION_GROUP_DEBRIS;
				breakModel.isRagdoll = false;
				breakModel.isMotionDisabled = false;
				breakModel.placementName[0] = 0;
				breakModel.placementIsBone = false;

				Vector vecObbSize = pEntity->CollisionProp()->OBBSize();

				// Find a random point on the plane of the original's two largest axis
				int smallestAxis = SmallestAxis( vecObbSize );
				Vector vecMins(0,0,0);
				Vector vecMaxs(1,1,1);
				vecMins[smallestAxis] = 0.5;
				vecMaxs[smallestAxis] = 0.5;
				pEntity->CollisionProp()->RandomPointInBounds( vecMins, vecMaxs, &breakModel.offset );

				// Push all chunks away from the center
				Vector vecBurstDir = breakModel.offset - params.origin;
				VectorNormalize( vecBurstDir );
				Vector vecVelocity = vecBurstDir * params.defBurstScale;

				QAngle vecAngles = pEntity->GetAbsAngles();
				int iSkin = pBreakableInterface->GetBreakableSkin();

				CBaseEntity *pBreakable = nullptr;

				{
					pBreakable = BreakModelCreateSingle( pOwnerEntity, &breakModel, breakModel.offset, vecAngles, vecVelocity, vec3_origin/*params.angularVelocity*/, iSkin, params );
					if ( !pBreakable )
					{
						DevWarning( "PropBreakableCreateAll: Could not create model %s\n", breakModel.modelName );
					}
				}

				if ( pBreakable )
				{
					Vector vecBreakableObbSize = pBreakable->CollisionProp()->OBBSize();

					// Try to align the gibs along the original axis 
					matrix3x4_t matrix;
					AngleMatrix( vecAngles, matrix );
					AlignBoxes( &matrix, vecObbSize, vecBreakableObbSize );
					MatrixAngles( matrix, vecAngles );

					if ( pBreakable->VPhysicsGetObject() )
					{
						Vector pos;
						pBreakable->VPhysicsGetObject()->GetPosition( &pos, nullptr );
						pBreakable->VPhysicsGetObject()->SetPosition( pos, vecAngles, true );
					}

					pBreakable->SetAbsAngles( vecAngles );

					if ( pOwnerEntity->IsEffectActive( EF_NOSHADOW ) )
					{
						pBreakable->AddEffects( EF_NOSHADOW );
					}
					
					// Set the same render color as the parent.
					color24 renderColor = pEntity->GetRenderColor();
					pBreakable->SetRenderColor( renderColor.r, renderColor.g, renderColor.b );
				}
			}
		}
	}
}

void PropBreakableCreateAll( int modelindex, IPhysicsObject *pPhysics, const Vector &origin, const QAngle &angles, const Vector &velocity, const AngularImpulse &angularVelocity, float impactEnergyScale, float defBurstScale, int defCollisionGroup, CBaseEntity *pEntity, bool defaultLocation )
{
	breakablepropparams_t params( origin, angles, velocity, angularVelocity );
	params.impactEnergyScale = impactEnergyScale;
	params.defBurstScale = defBurstScale;
	params.defCollisionGroup = defCollisionGroup;
	PropBreakableCreateAll( modelindex, pPhysics, params, pEntity, -1, false, defaultLocation );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : modelindex - 
//-----------------------------------------------------------------------------
void PrecacheGibsForModel( int iModel )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( iModel );
	if ( !pCollide )
		return;

	// The scale and group doesn't really matter at the moment, we are just using the parser to get the model name to cache.
	CBreakParser breakParser( 1.0, COLLISION_GROUP_NONE );

	// Create a parser.
	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pCollide );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "break" ) )
		{
			breakmodel_t breakModel;
			pParse->ParseCustom( &breakModel, &breakParser );
			CBaseEntity::PrecacheModel( breakModel.modelName );
		}
		else
		{
			pParse->SkipBlock();
		}
	}

	// Destroy the parser.
	physcollision->VPhysicsKeyParserDestroy( pParse );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &list - 
//			modelindex - 
//			defBurstScale - 
//			defCollisionGroup - 
//-----------------------------------------------------------------------------
void BuildGibList( std::vector< breakmodel_t > &list, int modelindex, float defBurstScale, int defCollisionGroup )
{
	BreakModelList( list, modelindex, defBurstScale, defCollisionGroup );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &list - 
//			modelindex - 
//			*pPhysics - 
//			&params - 
//			*pEntity - 
//			iPrecomputedBreakableCount - 
//			bIgnoreGibLImit - 
//			defaultLocation - 
//-----------------------------------------------------------------------------
CBaseEntity *CreateGibsFromList( std::vector< breakmodel_t > &list, int modelindex, IPhysicsObject *pPhysics, const breakablepropparams_t &params, CBaseEntity *pEntity, int iPrecomputedBreakableCount, bool bIgnoreGibLimit, bool defaultLocation, std::vector< EHANDLE > *pGibList, bool bBurning )
{
    // Check for prop breakable count reset. 
	int nPropCount = props_break_max_pieces_perframe.GetInt(); 
	if ( nPropCount != -1 ) 
	{ 
		if ( nFrameNumber != gpGlobals->framecount ) 
		{ 
			nPropBreakablesPerFrameCount = 0; 
			nFrameNumber = gpGlobals->framecount; 
		} 
      
		// Check for max breakable count for the frame. 
		if ( nPropBreakablesPerFrameCount >= nPropCount ) 
			return nullptr; 
	} 

	int iMaxBreakCount = bIgnoreGibLimit ? -1 : props_break_max_pieces.GetInt();
	if ( iMaxBreakCount != -1 )
	{
		if ( iPrecomputedBreakableCount != -1 )
		{
			iPrecomputedBreakableCount = std::min( iMaxBreakCount, iPrecomputedBreakableCount );
		}
		else
		{
			iPrecomputedBreakableCount = iMaxBreakCount;
		}
	}

	vcollide_t *pCollide = modelinfo->GetVCollide( modelindex );
	if ( !pCollide )
		return nullptr;

	int nSkin = 0;
	CBaseEntity *pOwnerEntity = pEntity;
	CBaseAnimating *pOwnerAnim = nullptr;
	if ( pPhysics )
	{
		pOwnerEntity = static_cast<CBaseEntity *>(pPhysics->GetGameData());
	}
	if ( pOwnerEntity )
	{
		pOwnerAnim = dynamic_cast<CBaseAnimating*>(pOwnerEntity);
		if ( pOwnerAnim )
		{
			nSkin = pOwnerAnim->GetSkin();
		}
	}
	matrix3x4_t localToWorld;

	CStudioHdr studioHdr;
	const model_t *model = modelinfo->GetModel( modelindex );
	if ( model )
	{
		studioHdr.Init( modelinfo->GetStudiomodel( model ) );
	}

	Vector parentOrigin = vec3_origin;
	int parentAttachment = 	Studio_FindAttachment( &studioHdr, "placementOrigin" ) + 1;
	if ( parentAttachment > 0 )
	{
		GetAttachmentLocalSpace( &studioHdr, parentAttachment-1, localToWorld );
		MatrixGetColumn( localToWorld, 3, parentOrigin );
	}
	else
	{
		AngleMatrix( vec3_angle, localToWorld );
	}
	
//	std::vector< breakmodel_t > list;
//	BreakModelList( list, modelindex, params.defBurstScale, params.defCollisionGroup );

	CBaseEntity *pFirstBreakable = nullptr;

	if ( !list.empty() )
	{
		for ( int i = 0; i < std::ssize( list ); i++ )
		{
			int modelIndex = modelinfo->GetModelIndex( list[i].modelName );
			if ( modelIndex <= 0 )
				continue;

			// Skip multiplayer pieces that should be spawning on the other dll
			if ( gpGlobals->maxClients > 1 )
			{
				if ( list[i].mpBreakMode == MULTIPLAYER_BREAK_SERVERSIDE )
					continue;

				if ( !defaultLocation && list[i].mpBreakMode == MULTIPLAYER_BREAK_DEFAULT )
					continue;
			}

			if ( ( nPropCount != -1 ) && ( nPropBreakablesPerFrameCount > nPropCount ) )
				break;

			if ( ( iPrecomputedBreakableCount != -1 ) && ( i >= iPrecomputedBreakableCount ) )
				break;

			matrix3x4_t matrix;
			AngleMatrix( params.angles, params.origin, matrix );

			CStudioHdr studioHdr;
			const model_t *model = modelinfo->GetModel( modelIndex );
			if ( model )
			{
				studioHdr.Init( modelinfo->GetStudiomodel( model ) );
			}

			// Increment the number of breakable props this frame.
			++nPropBreakablesPerFrameCount;

			Vector position = vec3_origin;
			QAngle angles = params.angles;
			if ( pOwnerAnim && list[i].placementName[0] )
			{
				if ( list[i].placementIsBone )
				{
					int boneIndex = pOwnerAnim->LookupBone( list[i].placementName );
					if ( boneIndex >= 0 )
					{
						pOwnerAnim->GetBonePosition( boneIndex, position, angles );
						AngleMatrix( angles, position, matrix );
					}
				}
				else
				{
					int attachmentIndex = Studio_FindAttachment( &studioHdr, list[i].placementName ) + 1;
					if ( attachmentIndex > 0 )
					{
						pOwnerAnim->GetAttachment( attachmentIndex, matrix );
						MatrixAngles( matrix, angles );
					}
				}
			}
			else
			{
				int placementIndex = Studio_FindAttachment( &studioHdr, "placementOrigin" ) + 1;
				Vector placementOrigin = parentOrigin;
				if ( placementIndex > 0 )
				{
					GetAttachmentLocalSpace( &studioHdr, placementIndex-1, localToWorld );
					MatrixGetColumn( localToWorld, 3, placementOrigin );
					placementOrigin -= parentOrigin;
				}

				VectorTransform( list[i].offset - placementOrigin, matrix, position );
			}
			Vector objectVelocity = params.velocity;

			float flScale = VectorNormalize( objectVelocity );
			objectVelocity.x += RandomFloat( -1.f, 1.0f );
			objectVelocity.y += RandomFloat( -1.0f, 1.0f );
			objectVelocity.z += RandomFloat( 0.0f, 1.0f );
			VectorNormalize( objectVelocity );
			objectVelocity *= flScale;

			if (pPhysics)
			{
				pPhysics->GetVelocityAtPoint( position, &objectVelocity );
			}

			int nActualSkin = nSkin;
			if ( nActualSkin > studioHdr.numskinfamilies() )
				nActualSkin = 0;

			CBaseEntity *pBreakable = nullptr;
			
			{
				pBreakable = BreakModelCreateSingle( pOwnerEntity, &list[i], position, angles, objectVelocity, params.angularVelocity, nActualSkin, params );
			}

			if ( pBreakable )
			{

				if ( bBurning && cl_burninggibs.GetBool() )
				{
					pBreakable->ParticleProp()->Create( "burninggibs", PATTACH_POINT_FOLLOW, "bloodpoint" );
				}
				if ( pOwnerEntity && pOwnerEntity->IsEffectActive( EF_NOSHADOW ) )
				{
					pBreakable->AddEffects( EF_NOSHADOW );
				}

				// If burst scale is set, this piece should 'burst' away from
				// the origin in addition to travelling in the wished velocity.
				if ( list[i].burstScale != 0.0 )
				{
					Vector vecBurstDir = position - params.origin;

					// If $autocenter wasn't used, try the center of the piece
					if ( vecBurstDir == vec3_origin )
					{
						vecBurstDir = pBreakable->WorldSpaceCenter() - params.origin;
					}

					VectorNormalize( vecBurstDir );

					pBreakable->ApplyAbsVelocityImpulse( vecBurstDir * list[i].burstScale );
				}

				// If this piece is supposed to be motion disabled, disable it
				if ( list[i].isMotionDisabled )
				{
					IPhysicsObject *pPhysicsObject = pBreakable->VPhysicsGetObject();
					if ( pPhysicsObject != nullptr )
					{
						pPhysicsObject->EnableMotion( false );
					}
				}

				if ( !pFirstBreakable )
				{
					pFirstBreakable = pBreakable;
				}

				if ( pGibList )
				{
					pGibList->push_back( pBreakable );
				}
			}
		}
	}
	// Then see if the propdata specifies any breakable pieces
	else if ( pEntity )
	{
		IBreakableWithPropData *pBreakableInterface = dynamic_cast<IBreakableWithPropData*>(pEntity);
		if ( pBreakableInterface && pBreakableInterface->GetBreakableModel() != NULL_STRING && pBreakableInterface->GetBreakableCount() )
		{
			breakmodel_t breakModel;

			for ( int i = 0; i < pBreakableInterface->GetBreakableCount(); i++ )
			{
				if ( ( iPrecomputedBreakableCount != -1 ) && ( i >= iPrecomputedBreakableCount ) )
					break;

				V_strncpy( breakModel.modelName, g_PropDataSystem.GetRandomChunkModel(STRING(pBreakableInterface->GetBreakableModel()), pBreakableInterface->GetMaxBreakableSize()), sizeof(breakModel.modelName) );

				breakModel.health = 1;
				breakModel.fadeTime = RandomFloat(5,10);
				breakModel.fadeMinDist = 0.0f;
				breakModel.fadeMaxDist = 0.0f;
				breakModel.burstScale = params.defBurstScale;
				breakModel.collisionGroup = COLLISION_GROUP_DEBRIS;
				breakModel.isRagdoll = false;
				breakModel.isMotionDisabled = false;
				breakModel.placementName[0] = 0;
				breakModel.placementIsBone = false;

				Vector vecObbSize = pEntity->CollisionProp()->OBBSize();

				// Find a random point on the plane of the original's two largest axis
				int smallestAxis = SmallestAxis( vecObbSize );
				Vector vecMins(0,0,0);
				Vector vecMaxs(1,1,1);
				vecMins[smallestAxis] = 0.5;
				vecMaxs[smallestAxis] = 0.5;
				pEntity->CollisionProp()->RandomPointInBounds( vecMins, vecMaxs, &breakModel.offset );

				// Push all chunks away from the center
				Vector vecBurstDir = breakModel.offset - params.origin;
				VectorNormalize( vecBurstDir );
				Vector vecVelocity = vecBurstDir * params.defBurstScale;

				QAngle vecAngles = pEntity->GetAbsAngles();
				int iSkin = pBreakableInterface->GetBreakableSkin();

				CBaseEntity *pBreakable = nullptr;

				{
					pBreakable = BreakModelCreateSingle( pOwnerEntity, &breakModel, breakModel.offset, vecAngles, vecVelocity, vec3_origin/*params.angularVelocity*/, iSkin, params );
				}

				if( pBreakable )
				{
					Vector vecBreakableObbSize = pBreakable->CollisionProp()->OBBSize();

					// Try to align the gibs along the original axis 
					matrix3x4_t matrix;
					AngleMatrix( vecAngles, matrix );
					AlignBoxes( &matrix, vecObbSize, vecBreakableObbSize );
					MatrixAngles( matrix, vecAngles );

					if ( pBreakable->VPhysicsGetObject() )
					{
						Vector pos;
						pBreakable->VPhysicsGetObject()->GetPosition( &pos, nullptr );
						pBreakable->VPhysicsGetObject()->SetPosition( pos, vecAngles, true );
					}

					pBreakable->SetAbsAngles( vecAngles );

					if ( pOwnerEntity->IsEffectActive( EF_NOSHADOW ) )
					{
						pBreakable->AddEffects( EF_NOSHADOW );
					}

					if ( !pFirstBreakable )
					{
						pFirstBreakable = pBreakable;
					}

					if ( pGibList )
					{
						pGibList->push_back( pBreakable );
					}
				}
				else
				{
					DevWarning( "PropBreakableCreateAll: Could not create model %s\n", breakModel.modelName );
				}
			}
		}
	}

	return pFirstBreakable;
}

//-----------------------------------------------------------------------------
// Shared physics prop methods to help the grab controller
//-----------------------------------------------------------------------------

bool CPhysicsProp::GetPropDataAngles( const char *pKeyName, QAngle &vecAngles )
{
	KeyValues *pModelKV = modelinfo->GetModelKeyValues( GetModel() );
	if ( pModelKV )
	{
		static int keyPhysgunInteractions = KeyValuesSystem()->GetSymbolForString( "physgun_interactions" );
		KeyValues *pkvPropData = pModelKV->FindKey( keyPhysgunInteractions );
		if ( pkvPropData )
		{
			char const *pszBase = pkvPropData->GetString( pKeyName );
			if ( pszBase && pszBase[0] )
			{
				UTIL_StringToVector( vecAngles.Base(), pszBase );
				return true;
			}
		}
	}

	return false;
}

float CPhysicsProp::GetCarryDistanceOffset( void )
{
	KeyValues *pModelKV = modelinfo->GetModelKeyValues( GetModel() );
	if ( pModelKV )
	{
		static int keyPhysgunInteractions = KeyValuesSystem()->GetSymbolForString( "physgun_interactions" );
		KeyValues *pkvPropData = pModelKV->FindKey( keyPhysgunInteractions );
		if ( pkvPropData )
		{
			float flDistance = pkvPropData->GetFloat( "carry_distance_offset", 0 );
			return flDistance;
		}
	}

	return 0;
}

#undef CPhysicsProp

breakablepropparams_t::breakablepropparams_t( const Vector &_origin, const QAngle &_angles, const Vector &_velocity, const AngularImpulse &_angularVelocity)
		: origin(_origin), angles(_angles), velocity(_velocity), angularVelocity(_angularVelocity)
{
	impactEnergyScale = 0;
	defBurstScale = 0;
	defCollisionGroup = COLLISION_GROUP_NONE;
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Cleanup
//-----------------------------------------------------------------------------
void CEntityMaterialProxy::Release( void )
{ 
	delete this; 
}

//-----------------------------------------------------------------------------
// Helper class to deal with floating point inputs
//-----------------------------------------------------------------------------
void CEntityMaterialProxy::OnBind( void *pRenderable )
{
	if( !pRenderable )
		return;

	IClientRenderable *pRend = ( IClientRenderable* )pRenderable;
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
	if ( pEnt )
	{
		OnBind( pEnt );
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Returns the player health (from 0 to 1)
//-----------------------------------------------------------------------------
class CProxyHealth : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CProxyHealth::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CProxyHealth::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	contract_assert( m_pResult );
	SetFloatResult( pEntity->HealthFraction() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CProxyHealth, Health );

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CEconItemView;
class CWeaponCSBase;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Returns the proximity of the player to the entity
//-----------------------------------------------------------------------------

class CPlayerProximityProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerProximityProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 0.002 );
	return true;
}

void CPlayerProximityProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the distance between the player and this entity....
	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	CBaseEntity* pPlayer = CBasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector delta;
	VectorSubtract( pEntity->WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), delta );

	contract_assert( m_pResult );
	SetFloatResult( delta.Length() * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerProximityProxy, PlayerProximity );

//-----------------------------------------------------------------------------
// Returns true if the player's team matches that of the entity the proxy material is attached to
//-----------------------------------------------------------------------------

class CPlayerTeamMatchProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
};

bool CPlayerTeamMatchProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	return true;
}

void CPlayerTeamMatchProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the distance between the player and this entity....
	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	CBaseEntity* pPlayer = CBasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	contract_assert( m_pResult );
	SetFloatResult( (pEntity->GetTeamNumber() == pPlayer->GetTeamNumber()) ? 1.0 : 0.0 );
}

EXPOSE_MATERIAL_PROXY( CPlayerTeamMatchProxy, PlayerTeamMatch );

//-----------------------------------------------------------------------------
// Returns the player view direction
//-----------------------------------------------------------------------------
class CPlayerViewProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerViewProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 2 );
	return true;
}

void CPlayerViewProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	CBaseEntity* pPlayer = CBasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector delta;
	VectorSubtract( pEntity->WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), delta );
	VectorNormalize( delta );

	Vector forward;
	AngleVectors( pPlayer->GetAbsAngles(), &forward );

	contract_assert( m_pResult );
	SetFloatResult( DotProduct( forward, delta ) * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerViewProxy, PlayerView );

//-----------------------------------------------------------------------------
// Returns the player speed
//-----------------------------------------------------------------------------
class CPlayerSpeedProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerSpeedProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 0.005 );
	return true;
}

void CPlayerSpeedProxy::OnBind( void *pC_BaseEntity )
{
	// Find the player speed....
	CBaseEntity* pPlayer = CBasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	contract_assert( m_pResult );
	SetFloatResult( pPlayer->GetLocalVelocity().Length() * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerSpeedProxy, PlayerSpeed );

//-----------------------------------------------------------------------------
// Returns the player position
//-----------------------------------------------------------------------------
class CPlayerPositionProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerPositionProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;
	  
	m_Factor = pKeyValues->GetFloat( "scale", 0.005 );
	return true;
}

void CPlayerPositionProxy::OnBind( void *pC_BaseEntity )
{
	// Find the player speed....
	CBaseEntity* pPlayer = CBasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// This is actually a vector...
	contract_assert( m_pResult );
	Vector res;
	VectorMultiply( pPlayer->WorldSpaceCenter(), m_Factor, res ); 
	m_pResult->SetVecValue( res.Base(), 3 );
}

EXPOSE_MATERIAL_PROXY( CPlayerPositionProxy, PlayerPosition );

//-----------------------------------------------------------------------------
// Returns the entity speed
//-----------------------------------------------------------------------------
class CEntitySpeedProxy : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CEntitySpeedProxy::OnBind( void *pC_BaseEntity )
{
	// Find the view angle between the player and this entity....
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	contract_assert( m_pResult );
	m_pResult->SetFloatValue( pEntity->GetLocalVelocity().Length() );
}

EXPOSE_MATERIAL_PROXY( CEntitySpeedProxy, EntitySpeed );

//-----------------------------------------------------------------------------
// Returns a random # from 0 - 1 specific to the entity it's applied to
//-----------------------------------------------------------------------------
class CEntityRandomProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CEntityRandomProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CEntityRandomProxy::OnBind( void *pC_BaseEntity )
{
	// Find the view angle between the player and this entity....
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	contract_assert( m_pResult );
	m_pResult->SetFloatValue( pEntity->ProxyRandomValue() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CEntityRandomProxy, EntityRandom );

//-----------------------------------------------------------------------------
// StatTrak 'kill odometer' support: given a numerical value expressed as a string, pick a texture frame to represent a given digit
//-----------------------------------------------------------------------------
class CStatTrakDigitProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

	virtual bool HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *piScore );

private:
	CFloatInput	m_flDisplayDigit; // the particular digit we want to display
	CFloatInput	m_flTrimZeros;
};

bool CStatTrakDigitProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ))
		return false;

	if (!m_flTrimZeros.Init( pMaterial, pKeyValues, "trimZeros", 0 ))
		return false;

	return true;
}

bool CStatTrakDigitProxy::HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *piScore )
{
	if ( !pC_BaseEntity )
		return false;

	if ( !piScore )
		return false;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// StatTrak modules are children of their accompanying viewmodels
		CBaseViewModel *pViewModel = dynamic_cast< CBaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			CCSPlayer *pPlayer = ToCSPlayer( pViewModel->GetPredictionOwner() );
			if ( pPlayer )
			{
				CWeaponCSBase *pWeap = pPlayer->GetActiveCSWeapon();
				if ( pWeap )
				{
					if ( CEconItemView *pItemView = pWeap->GetEconItemView() )
					{
						// Always get headshot-trak(TM)
						*piScore = pItemView->GetKillEaterValueByType( 0 );
					}
				}
			}
		}
	}
	return true;
}

void CStatTrakDigitProxy::OnBind( void *pC_BaseEntity )
{
	int nKillEaterAltScore = 0;
	bool bHasScoreToDisplay = HelperOnBindGetStatTrakScore( pC_BaseEntity, &nKillEaterAltScore );
	if ( !bHasScoreToDisplay )
	{	// Force flashing numbers
		SetFloatResult( (int) std::fmod( gpGlobals->curtime, 10.0f ) );
		return;
	}

	int iDesiredDigit = (int)m_flDisplayDigit.GetFloat();

	// trim preceding zeros
	if ( m_flTrimZeros.GetFloat() > 0 )
	{
		if ( std::pow( 10.0f, iDesiredDigit ) > nKillEaterAltScore )
		{
			SetFloatResult( 10.0f ); //assumed blank frame
			return;
		}
	}

	// get the [0-9] value of the digit we want
	int iDigitCount = std::min( iDesiredDigit, 10 );
	for ( int i=0; i<iDigitCount; i++ )
	{
		nKillEaterAltScore /= 10;
	}
	nKillEaterAltScore %= 10;

	SetFloatResult( nKillEaterAltScore );
}

EXPOSE_MATERIAL_PROXY( CStatTrakDigitProxy, StatTrakDigit );

//-----------------------------------------------------------------------------
// StatTrak 'kill odometer' support: given a numerical value expressed as a string, pick a texture frame to represent a given digit
//-----------------------------------------------------------------------------
class CStatTrakDigitProxyForModelWeaponPreviewPanel : public CStatTrakDigitProxy
{
public:
	virtual bool HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *puiScore ) override
	{
		/* Removed for partner depot */
		return false;
	}
};
EXPOSE_MATERIAL_PROXY( CStatTrakDigitProxyForModelWeaponPreviewPanel, StatTrakDigitForModelWeaponPreview );

//-----------------------------------------------------------------------------
// IronSightAmount proxy
//-----------------------------------------------------------------------------
class CIronSightAmountProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);
private:
	bool bInvert;
};

bool CIronSightAmountProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;

	bInvert = false;
	CFloatInput	m_flInvert;
	if ( m_flInvert.Init( pMaterial, pKeyValues, "invert" ) )
		bInvert = ( m_flInvert.GetFloat() > 0 );

	return true;
}

void CIronSightAmountProxy::OnBind(void *pC_BaseEntity)
{

	if (!pC_BaseEntity)
		return;
	
	CBaseEntity *pEntity = BindArgToEntity(pC_BaseEntity);
	if (pEntity)
	{
		CBaseViewModel *pViewModel = dynamic_cast<CBaseViewModel*>(pEntity);
		if (pViewModel)
		{
			CCSPlayer *pPlayer = ToCSPlayer(pViewModel->GetPredictionOwner());
			if (pPlayer)
			{
				CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
				if ( pWeapon && pWeapon->GetIronSightController() )
				{
					if ( bInvert )
					{
						SetFloatResult(Bias( 1.0f - pWeapon->GetIronSightController()->GetIronSightAmount(), 0.2f));
					}
					else
					{
						SetFloatResult(Bias(pWeapon->GetIronSightController()->GetIronSightAmount(), 0.2f));
					}
				}
			}
		}
	}

}

EXPOSE_MATERIAL_PROXY(CIronSightAmountProxy, IronSightAmount);

//-----------------------------------------------------------------------------
// StatTrakIllum proxy
//-----------------------------------------------------------------------------
class CStatTrakIllumProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};

bool CStatTrakIllumProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", 0.5 ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", 1 ))
		return false;

	return true;
}

void CStatTrakIllumProxy::OnBind( void *pC_BaseEntity )
{

	if (!pC_BaseEntity)
		return;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// StatTrak modules are children of their accompanying viewmodels
		CBaseViewModel *pViewModel = dynamic_cast< CBaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			SetFloatResult( Lerp( pViewModel->GetStatTrakGlowMultiplier(), m_flMinVal.GetFloat(), m_flMaxVal.GetFloat() ) );
			return;
		}
	}

}

EXPOSE_MATERIAL_PROXY( CStatTrakIllumProxy, StatTrakIllum );

//-----------------------------------------------------------------------------
// WeaponLabelTextProxy
//-----------------------------------------------------------------------------
class CWeaponLabelTextProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
	virtual bool HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel );

private:
	CFloatInput	m_flDisplayDigit;
	IMaterialVar *m_pTextureOffsetVar;
};

bool CWeaponLabelTextProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{

	if (!m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ))
		return false;

	bool foundVar;
	m_pTextureOffsetVar = pMaterial->FindVar( "$basetexturetransform", &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

bool CWeaponLabelTextProxy::HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel )
{
	if ( !pC_BaseEntity )
		return false;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// uid modules are children of their accompanying viewmodels
		CBaseViewModel *pViewModel = dynamic_cast< CBaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			CBaseCombatWeapon *pWeapon = pViewModel->GetWeapon();
			if ( pWeapon )
			{
				CEconItemView *pItem = pWeapon->GetEconItemView();
				if ( pItem )
				{
					*p_szLabel = pItem->GetCustomName();
					return true;
				}
			}
		}
	}

	return false;
}

void CWeaponLabelTextProxy::OnBind( void *pC_BaseEntity )
{

	const char *p_szLabel = nullptr;
	bool bHasLabel = HelperOnBindGetLabel( pC_BaseEntity, &p_szLabel );
	if (!bHasLabel || !p_szLabel)
		return;

	//get the digit index we need to display
	int nDigit = (int)m_flDisplayDigit.GetFloat();

	//center the text within NUM_UID_CHARS
	int nStrLen = (int)strlen( p_szLabel );
	int nPrependSpaces = ( NUM_UID_CHARS - nStrLen ) / 2;
	nDigit -= nPrependSpaces;

	int nCharIndex = 0;
	if ( nDigit >= 0 && nDigit < nStrLen )
	{
		nCharIndex = p_szLabel[nDigit] - 32;
	}

	int nIndexHoriz = std::fmod( nCharIndex, 12.0f );
	int nIndexVertical = nCharIndex / 12;

	float flOffsetX = 0.083333f * nIndexHoriz;
	float flOffsetY =    0.125f * nIndexVertical;

	VMatrix mat( 1.0f,	0.0f,	0.0f,	flOffsetX,
		0.0f,	1.0f,	0.0f,	flOffsetY,
		0.0f,	0.0f,	1.0f,	0.0f,
		0.0f,	0.0f,	0.0f,	1.0f );

	m_pTextureOffsetVar->SetMatrixValue( mat );
}

EXPOSE_MATERIAL_PROXY( CWeaponLabelTextProxy, WeaponLabelText );

class CWeaponLabelTextProxyForModelWeaponPreviewPanel : public CWeaponLabelTextProxy
{
public:
	virtual bool HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel )
	{
		/* Removed for partner depot */
		return false;
	}
};
EXPOSE_MATERIAL_PROXY( CWeaponLabelTextProxyForModelWeaponPreviewPanel, WeaponLabelTextPreview );

int g_HighlightedSticker = -1;
int g_PeelSticker = -1;

void CC_HighlightSticker(const CCommand& args)
{
	int nParam = atoi(args[1]);
	if ( nParam >= 0 && nParam <= 4 )
	{
		g_HighlightedSticker = nParam;
	}
}
static ConCommand highlight_sticker(side_registry(), "highlight_sticker", CC_HighlightSticker, "", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

void CC_PeelSticker(const CCommand& args)
{
	int nParam = atoi(args[1]);
	if (nParam >= 0 && nParam <= 4)
	{
		g_PeelSticker = nParam;
	}
}
static ConCommand peel_sticker(side_registry(), "peel_sticker", CC_PeelSticker, "", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY);

//-----------------------------------------------------------------------------
// Sticker selection proxy
//-----------------------------------------------------------------------------
class CStickerSelectionProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);
	virtual void CheckMyGlobal( void );
	
	int m_nStickerIndex;
	float m_flSelectedness;
};

bool CStickerSelectionProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;

	CFloatInput flStickerIndex;
	if (!flStickerIndex.Init(pMaterial, pKeyValues, "stickerindex", 0))
		return false;
	m_nStickerIndex = (int)flStickerIndex.GetFloat();

	m_flSelectedness = 0.0f;

	return true;
}

void CStickerSelectionProxy::CheckMyGlobal( void )
{
	if (g_HighlightedSticker > -1 && m_nStickerIndex == g_HighlightedSticker)
	{
		m_flSelectedness = 1.0f;
		g_HighlightedSticker = -1;
	}
}

void CStickerSelectionProxy::OnBind(void *pC_BaseEntity)
{

	//if (!pC_BaseEntity)
	//	return;

	CheckMyGlobal();

	if ( m_flSelectedness > 0.01f )
	{
		m_flSelectedness = Approach( 0.0f, m_flSelectedness, gpGlobals->frametime * 0.5f );
		SetFloatResult( m_flSelectedness );
	}
	else if ( m_flSelectedness > 0.0f )
	{
		SetFloatResult( 0.0f );
	}

}

EXPOSE_MATERIAL_PROXY(CStickerSelectionProxy, StickerSelection);

class CStickerPeelProxy : public CStickerSelectionProxy
{
public:
	virtual void CheckMyGlobal( void );
};

void CStickerPeelProxy::CheckMyGlobal(void)
{
	if (g_PeelSticker > -1 && m_nStickerIndex == g_PeelSticker)
	{
		m_flSelectedness = 1.0f;
		g_PeelSticker = -1;
	}
}

EXPOSE_MATERIAL_PROXY(CStickerPeelProxy, StickerPeel);

//-----------------------------------------------------------------------------
// CrosshairColor proxy
//-----------------------------------------------------------------------------
extern ConVar cl_crosshaircolor_r;
extern ConVar cl_crosshaircolor_g;
extern ConVar cl_crosshaircolor_b;
class CCrossHairColorProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);

	Vector m_vecLocalCrossHairColor;
};

bool CCrossHairColorProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;
	m_vecLocalCrossHairColor.Init();
	return true;
}

void CCrossHairColorProxy::OnBind(void *pC_BaseEntity)
{
	if ( m_vecLocalCrossHairColor.x != cl_crosshaircolor_r.GetFloat() || 
		 m_vecLocalCrossHairColor.y != cl_crosshaircolor_g.GetFloat() || 
		 m_vecLocalCrossHairColor.z != cl_crosshaircolor_b.GetFloat() )
	{

		m_vecLocalCrossHairColor.x = cl_crosshaircolor_r.GetFloat();
		m_vecLocalCrossHairColor.y = cl_crosshaircolor_g.GetFloat();
		m_vecLocalCrossHairColor.z = cl_crosshaircolor_b.GetFloat();

		SetVecResult(   (float)m_vecLocalCrossHairColor.x * 0.0039,
						(float)m_vecLocalCrossHairColor.y * 0.0039,
						(float)m_vecLocalCrossHairColor.z * 0.0039, 1);
	}
}

EXPOSE_MATERIAL_PROXY(CCrossHairColorProxy, CrossHairColor);

float g_flEconInspectPreviewTime = 0;

class CEconInspectPreviewTimeProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CEconInspectPreviewTimeProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

void CEconInspectPreviewTimeProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_pResult );
	SetFloatResult( gpGlobals->curtime - g_flEconInspectPreviewTime );
}

EXPOSE_MATERIAL_PROXY( CEconInspectPreviewTimeProxy, EconInspectPreviewTime );

class CBaseEntity;

using namespace ::ks::reflect;

static constexpr auto DEFAULT_OPEN_PUPIL_RATE = 0.03;
static constexpr auto DEFAULT_CLOSE_PUPIL_RATE = 0.1;

//-----------------------------------------------------------------------------
// Returns the proximity of the player to the entity
//-----------------------------------------------------------------------------
class CPupilProxy : public CEntityMaterialProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( CBaseEntity *pBaseEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pAnimatedTextureVar;
	IMaterialVar *m_pAnimatedTextureFrameNumVar;
	IMaterialVar *m_pLightingVar;

	CFloatInput m_flPupilCloseRate;
	CFloatInput m_flPupilOpenRate;
};

bool CPupilProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pAnimatedTextureVarName = pKeyValues->GetString( "TextureVar" );
	if( !pAnimatedTextureVarName )
		return false;

	bool foundVar;
	m_pAnimatedTextureVar = pMaterial->FindVar( pAnimatedTextureVarName, &foundVar, false );
	if( !foundVar )
		return false;

	char const* pAnimatedTextureFrameNumVarName = pKeyValues->GetString( "TextureFrameNumVar" );
	if( !pAnimatedTextureFrameNumVarName )
		return false;

	m_pAnimatedTextureFrameNumVar = pMaterial->FindVar( pAnimatedTextureFrameNumVarName, &foundVar, false );
	if( !foundVar )
		return false;

	m_pLightingVar = pMaterial->FindVar( "$lighting", &foundVar, false );
	if( !foundVar )
	{
		Warning("Materials using the pupil proxy must have a field called $lighting which has a value of 0.5!\n" );
		return false;
	}

	m_flPupilCloseRate.Init( pMaterial, pKeyValues, "PupilCloseRate", DEFAULT_CLOSE_PUPIL_RATE );
	m_flPupilOpenRate.Init( pMaterial, pKeyValues, "PupilOpenRate", DEFAULT_OPEN_PUPIL_RATE );

	return true;
}

void CPupilProxy::OnBind( CBaseEntity *pBaseEntity )
{
	if (!pBaseEntity || !m_pAnimatedTextureVar )
		return;

	if( m_pAnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
		return;

	ITexture *pTexture = m_pAnimatedTextureVar->GetTextureValue();
	int nFrameCount = pTexture->GetNumAnimationFrames();

	// Compute the lighting at the eye position of the entity; use it to dialate the pupil
	Vector forward;
	pBaseEntity->GetVectors( &forward, nullptr, nullptr );

	Vector eyePt = pBaseEntity->EyePosition();
	Vector color;
	engine->ComputeLighting( eyePt, &forward, false, color );

	// Compute the intensity...
	float flIntensity = ( 0.299f * color[0] + 0.587f * color[1] + 0.114f * color[2] ) * 0.5;
	flIntensity = clamp( flIntensity, 0, 1 );
	float flLastIntensity = m_pLightingVar->GetFloatValue( );
	if ( flIntensity > flLastIntensity )
	{
		float flMaxChange = m_flPupilCloseRate.GetFloat() * gpGlobals->frametime;
		if ( flIntensity > (flMaxChange + flLastIntensity) )
		{
			flIntensity = flLastIntensity + flMaxChange;
		}
	}
	else
	{
		float flMaxChange = m_flPupilOpenRate.GetFloat() * gpGlobals->frametime;
		if ( flIntensity < (flLastIntensity - flMaxChange) )
		{
			flIntensity = flLastIntensity - flMaxChange;
		}
	}

	int nFrame = nFrameCount * flIntensity;
	nFrame = clamp( nFrame, 0, nFrameCount - 1 );

	m_pAnimatedTextureFrameNumVar->SetIntValue( nFrame );
	m_pLightingVar->SetFloatValue( flIntensity );
}

IMaterial *CPupilProxy::GetMaterial()
{
	if ( !m_pAnimatedTextureVar )
		return nullptr;

	return m_pAnimatedTextureVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CPupilProxy, Pupil );
