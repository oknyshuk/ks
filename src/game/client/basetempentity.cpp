// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include "client_class.h"
#include "reflect_recvtable.h"
#include "edict.h"
#include "iclientnetworkable.h"
#include "inputsystem/iinputsystem.h"
#include "model_types.h"
#include "platforminputdevice.h"
#include "toolframework/itoolentity.h"
#include "view_shared.h"
#include "cdll_client_int.h"
#include "shareddefs.h"
#include "predictable_entity.h"
#include "reflect_predmap.h"
#include "engine/ivmodelinfo.h"
#include "vphysics_interface.h"
#include "cdll_util.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "beam_shared.h"
#include "baseentity.h"
#include "iclientrenderable.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "tier2/beamsegdraw.h"
#include "declare_class.h"
#include "dlight.h"
#include "engine/iblackbox.h"
#include "bone_setup.h"
#include "studio.h"
#include "cam_thirdperson.h"
#include "bitmap/tgaloader.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/itexture.h"
#include "worldsize.h"
#include "imaterialproxydict.h"
module ks.client;

import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import :props;
import :iinput;
import :precache;
import :noopanimcontroller;
import :util;
import :baseentity;
import :sharedvar;
import :particlemgr;
import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.transform;
import ks.math.vector;
import ks.tier1.bit_ops;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.interpolatedvar;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
import ks.tier1.registry;
import :baseanimatedtextureproxy;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import ks.tier1.exprevaluator;
import ks.game.mapentities_shared;
import ks.game.beam_flags;
import ks.core.vprof;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.tier1.strtools;
import ks.math.vmatrix;
import ks.tier1.cvar;
import ks.tier1.icvar;

using namespace ::ks::reflect;

// Purpose: Core Temp Entity client implementation.

IMPLEMENT_CLIENTCLASS(CBaseTempEntity, DT_BaseTempEntity, CBaseTempEntity);

IMPLEMENT_REFLECT_TABLE( CBaseTempEntity, DT_BaseTempEntity );

// Global list of temp entity classes
CBaseTempEntity *CBaseTempEntity::s_pTempEntities = nullptr;

// Global list of dynamic temp entities
CBaseTempEntity *CBaseTempEntity::s_pDynamicEntities = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Returns head of list
// Output : CBaseTempEntity * -- head of list
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetDynamicList( void )
{
	return s_pDynamicEntities;
}

//-----------------------------------------------------------------------------
// Purpose: Returns head of list
// Output : CBaseTempEntity * -- head of list
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetList( void )
{
	return s_pTempEntities;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CBaseTempEntity::CBaseTempEntity( void )
{
	// Add to list
	m_pNext			= s_pTempEntities;
	s_pTempEntities = this;
	
	m_pNextDynamic = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CBaseTempEntity::~CBaseTempEntity( void )
{
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
// Purpose: Get next temp ent in chain
// Output : CBaseTempEntity *
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetNextDynamic( void )
{
	return m_pNextDynamic;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTempEntity::Precache( void )
{
	// Nothing...
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

//-----------------------------------------------------------------------------
// Purpose: Called at startup and level load to clear out leftover temp entities
//-----------------------------------------------------------------------------
void CBaseTempEntity::ClearDynamicTempEnts( void )
{
	CBaseTempEntity *next;
	CBaseTempEntity *te = s_pDynamicEntities;
	while ( te )
	{
		next = te->GetNextDynamic();
		delete te;
		te = next;
	}

	s_pDynamicEntities = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Called at startup and level load to clear out leftover temp entities
//-----------------------------------------------------------------------------
void CBaseTempEntity::CheckDynamicTempEnts( void )
{
	CBaseTempEntity *next, *newlist = nullptr;
	CBaseTempEntity *te = s_pDynamicEntities;
	while ( te )
	{
		next = te->GetNextDynamic();
		if ( te->ShouldDestroy() )
		{
			delete te;
		}
		else
		{
			te->m_pNextDynamic = newlist;
			newlist = te;
		}
		te = next;
	}

	s_pDynamicEntities = newlist;
}

//-----------------------------------------------------------------------------
// Purpose: Dynamic/non-singleton temp entities are initialized by
//  calling into here.  They should be added to a list of C_BaseTempEntities so
//  that their memory can be deallocated appropriately.
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTempEntity::Init( int entnum, int iSerialNum )
{
	if ( entnum != -1 )
	{
		contract_assert( 0 );
	}

	// Link into dynamic entity list
	m_pNextDynamic = s_pDynamicEntities;
	s_pDynamicEntities = this;

	return true;
}

void CBaseTempEntity::Release()
{
	contract_assert( !"CBaseTempEntity::Release should never be called" );
}

void CBaseTempEntity::NotifyShouldTransmit( ShouldTransmitState_t state )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CBaseTempEntity::PreDataUpdate( DataUpdateType_t updateType )
{
	// TE's may or may not implement this
}

int CBaseTempEntity::entindex( void ) const { contract_assert( 0 ); return 0; }
void CBaseTempEntity::PostDataUpdate( DataUpdateType_t updateType ) { contract_assert( 0 ); }
void CBaseTempEntity::OnPreDataChanged( DataUpdateType_t updateType ) { contract_assert( 0 ); }
void CBaseTempEntity::OnDataChanged( DataUpdateType_t updateType ) { contract_assert( 0 ); }
void CBaseTempEntity::SetDormant( bool bDormant ) { contract_assert( 0 ); }
bool CBaseTempEntity::IsDormant( void ) const { contract_assert( 0 ); return false; };
void CBaseTempEntity::ReceiveMessage( int classID, bf_read &msg ) { contract_assert( 0 ); }
void CBaseTempEntity::SetDestroyedOnRecreateEntities( void ) { contract_assert( 0 ); }

void* CBaseTempEntity::GetDataTableBasePtr()
{
	return this;
}

class CNewParticleEffect;
class CBaseAnimating;
class CBaseCombatWeapon;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CEconItemView;
class CViewmodelAttachmentModel;
class CWeaponCSBase;
class FileWeaponInfo_t;

using namespace ::ks::reflect;

// Purpose: Client side view model implementation. Responsible for drawing
//			the view model.

	ConVar cl_righthand( side_registry(), "cl_righthand", "1", FCVAR_ARCHIVE | FCVAR_SS, "Use right-handed view models." );
	ConVar vm_draw_addon( side_registry(), "vm_draw_addon", "1" );
	SplitScreenConVarRef ss_righthand( "cl_righthand", true );
ConVar vm_debug( side_registry(), "vm_debug", "0", FCVAR_CHEAT );
ConVar vm_draw_always( side_registry(), "vm_draw_always", "0", FCVAR_CHEAT, "1 - Always draw view models, 2 - Never draw view models.  Should be done before map launches." );
ConVar vm_pointer_pitch_up_scale( side_registry(), "vm_pointer_pitch_up_scale", "0.25", FCVAR_DEVELOPMENTONLY, "Limit how much the view model follows the pointer in looking up." );

void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );
extern float g_flMuzzleFlashScale;
extern ConVar r_drawviewmodel;
extern ConVar cl_righthand;

ConVar mat_preview( side_registry(), "mat_preview", "", FCVAR_CLIENTDLL | FCVAR_CHEAT );

// [mlowrance] used for flame effect when pin pulled
inline constexpr const char *MOLOTOV_PARTICLE_EFFECT_NAME = "weapon_molotov_fp";

void FormatViewModelAttachment( CBasePlayer *pPlayer, Vector &vOrigin, bool bInverse )
{
	int nSlot = 0;
	if ( pPlayer ) 
	{
		int nPlayerSlot = CBasePlayer::GetSplitScreenSlotForPlayer( pPlayer );
		if ( nPlayerSlot == -1 )
		{
			nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		}
		else
		{
			nSlot = nPlayerSlot;
		}
	}

	contract_assert( nSlot != -1 );

	// Presumably, SetUpView has been called so we know our FOV and render origin.
	const CViewSetup *pViewSetup = view->GetPlayerViewSetup( nSlot );
	
	float worldx = std::tan( pViewSetup->fov * std::numbers::pi/360.0 );
	float viewx = tan( pViewSetup->fovViewmodel * std::numbers::pi/360.0 );

	// aspect ratio cancels out, so only need one factor
	// the difference between the screen coordinates of the 2 systems is the ratio
	// of the coefficients of the projection matrices (tan (fov/2) is that coefficient)
	float factorX = worldx / viewx;

	float factorY = factorX;
	
	// Get the coordinates in the viewer's space.
	Vector tmp = vOrigin - pViewSetup->origin;
	Vector vTransformed( MainViewRight(nSlot).Dot( tmp ), MainViewUp(nSlot).Dot( tmp ), MainViewForward(nSlot).Dot( tmp ) );

	// Now squash X and Y.
	if ( bInverse )
	{
		if ( factorX != 0 && factorY != 0 )
		{
			vTransformed.x /= factorX;
			vTransformed.y /= factorY;
		}
		else
		{
			vTransformed.x = 0.0f;
			vTransformed.y = 0.0f;
		}
	}
	else
	{
		vTransformed.x *= factorX;
		vTransformed.y *= factorY;
	}

	// Transform back to world space.
	Vector vOut = (MainViewRight(nSlot) * vTransformed.x) + (MainViewUp(nSlot) * vTransformed.y) + (MainViewForward(nSlot) * vTransformed.z);
	vOrigin = pViewSetup->origin + vOut;
}

void Precache( void )
{
	PrecacheParticleSystem( MOLOTOV_PARTICLE_EFFECT_NAME );

//	BaseClass::Precache();
}

void CBaseViewModel::UpdateStatTrakGlow( void )
{
	//approach the ideal in 2 seconds
	m_flStatTrakGlowMultiplier = Approach( m_flStatTrakGlowMultiplierIdeal, m_flStatTrakGlowMultiplier, (gpGlobals->frametime * 0.5) );
}

void CBaseViewModel::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	// [msmith] We want the split screen visibility of the particles to be the same as the visibility of the view model.
	uint32 visBits = ks::get_word( m_VisibilityBits, 0 ) & 0xff;
	int nSlot = -1;

	if ( visBits < 3 )
	{
		// If visBits is 3 then it is both slots 1 and 2 meaning all users can see this effect.
		// If visBits is less than 3, then it is either slot 1 or 2 (but zero based means slot 0 or 1).
		nSlot = visBits-1;
	}

	pNewParticleEffect->SetDrawOnlyForSplitScreenUser( nSlot );

	if ( FStrEq( pszParticleName, MOLOTOV_PARTICLE_EFFECT_NAME ) )
	{
		m_viewmodelParticleEffect = pNewParticleEffect;
	}
}

void CBaseViewModel::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	BaseClass::OnParticleEffectDeleted( pParticleEffect );

	if ( m_viewmodelParticleEffect == pParticleEffect )
	{
		m_viewmodelParticleEffect = nullptr;
	}
}

void CBaseViewModel::UpdateParticles( int nSlot )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
		return;

	if ( pPlayer->IsPlayerDead() )
		return;

	// Otherwise pass the event to our associated weapon
	CBaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( !pWeapon )
		return;

	CWeaponCSBase *pCSWeapon = ( CWeaponCSBase* )pPlayer->GetActiveWeapon();
	if ( !pCSWeapon )
		return;

	int iWeaponId = pCSWeapon->GetCSWeaponID();

	bool shouldDrawPlayer = ( pPlayer->GetPlayerRenderMode( nSlot ) == PLAYER_RENDER_THIRDPERSON );
	bool visible = r_drawviewmodel.GetBool() && pPlayer && !shouldDrawPlayer;

	if ( visible && iWeaponId == WEAPON_MOLOTOV )
	{
		CBaseCSGrenade *pGren = dynamic_cast<CBaseCSGrenade*>( pPlayer->GetActiveWeapon() );

		if ( pGren->IsPinPulled() )
		{
			//if ( !pGren->IsLoopingSoundPlaying() )
			//{
			//	pGren->SetLoopingSoundPlaying( true );
			//	EmitSound( "Molotov.IdleLoop" );
			//	//DevMsg( 1, "++++++++++>Playing Molotov.IdleLoop 2\n" );
			//}

			// TEST: [mlowrance] This is to test for attachment.
			int iAttachment = -1;
			if ( pWeapon && pWeapon->GetBaseAnimating() )
				iAttachment = pWeapon->GetBaseAnimating()->LookupAttachment( "Wick" );

			if ( iAttachment >= 0 )
			{
				if ( !m_viewmodelParticleEffect.IsValid() )
				{
					DispatchParticleEffect( MOLOTOV_PARTICLE_EFFECT_NAME, PATTACH_POINT_FOLLOW, this, "Wick" );
				}
			}
		}
	}
	else
	{
		if ( m_viewmodelParticleEffect.IsValid() )
		{
			StopSound( "Molotov.IdleLoop" );
			//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 3\n" );
			m_viewmodelParticleEffect->StopEmission( false, true );
			m_viewmodelParticleEffect->SetRemoveFlag();
			m_viewmodelParticleEffect = nullptr;
		}
	}
}

bool CBaseViewModel::Simulate( void )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );
	UpdateParticles( nSlot );
	UpdateStatTrakGlow();
	BaseClass::Simulate();
	return true;
}

void CBaseViewModel::FormatViewModelAttachment( int nAttachment, matrix3x4_t &attachmentToWorld )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	Vector vecOrigin;
	MatrixPosition( attachmentToWorld, vecOrigin );
	::FormatViewModelAttachment( pPlayer, vecOrigin, false );
	PositionMatrix( vecOrigin, attachmentToWorld );
}

void CBaseViewModel::UncorrectViewModelAttachment( Vector &vOrigin )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	// Unformat the attachment.
	::FormatViewModelAttachment( pPlayer, vOrigin, true );
}

//-----------------------------------------------------------------------------
// Purpose
//-----------------------------------------------------------------------------
void CBaseViewModel::FireEvent( const Vector& origin, const QAngle& angles, int eventNum, const char *options )
{
	// We override sound requests so that we can play them locally on the owning player
	if ( ( eventNum == AE_CL_PLAYSOUND ) || ( eventNum == CL_EVENT_SOUND ) )
	{
		// Only do this if we're owned by someone
		if ( GetOwner() != nullptr && GetOwner()->IsAlive() )
		{
			// playing the same sound near-instantly is assumed to be an error and duplicates are skipped. This does NOT apply to weapon firing sounds.
			if ( !IsSoundSameAsPreviousSound( options, 0.1f ) )
			{
				CLocalPlayerFilter filter;
				EmitSound( filter, GetOwner()->GetSoundSourceIndex(), options, &GetAbsOrigin() );
				SetPreviousSoundStr( options );
			}
			ResetTimeSincePreviousSound();
			return;
		}
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pOwner );

	// Otherwise pass the event to our associated weapon
	CBaseCombatWeapon *pWeapon = pOwner->GetActiveWeapon();
	if ( pWeapon )
	{
		bool bResult = pWeapon->OnFireEvent( this, origin, angles, eventNum, options );
		if ( !bResult )
		{
			if ( eventNum == AE_CLIENT_EFFECT_ATTACH && ::input->CAM_IsThirdPerson() )
				return;

			BaseClass::FireEvent( origin, angles, eventNum, options );
		}
	}
}

bool CBaseViewModel::Interpolate( float currentTime )
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	// Make sure we reset our animation information if we've switch sequences
	UpdateAnimationParity();

	bool bret = BaseClass::Interpolate( currentTime );

	// Hack to extrapolate cycle counter for view model
	float elapsed_time = currentTime - m_flAnimTime;
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	// Predicted viewmodels have fixed up interval
	if ( GetPredictable() || IsClientCreated() )
	{
		contract_assert( pPlayer );
		float curtime = pPlayer ? pPlayer->GetFinalPredictedTime() : gpGlobals->curtime;
		elapsed_time = curtime - m_flAnimTime;
		// Adjust for interpolated partial frame
		if ( !engine->IsPaused() )
		{
			elapsed_time += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
		}
	}

	// Prediction errors?	
	if ( elapsed_time < 0 )
	{
		elapsed_time = 0;
	}

	float dt = elapsed_time * (GetPlaybackRate() * GetSequenceCycleRate( pStudioHdr, GetSequence() )) + m_fCycleOffset;

	if ( dt < 0.0f )
	{
		dt = 0.0f;
	}

	if ( dt >= 1.0f )
	{
		if ( !IsSequenceLooping( GetSequence() ) )
		{
			dt = 0.999f;
		}
		else
		{
			dt = std::fmod( dt, 1.0f );
		}
	}

	SetCycle( dt );
	return bret;
}

bool CBaseViewModel::ShouldFlipModel()
{
	// If cl_righthand is set, then we want them all right-handed.
	CBaseCombatWeapon *pWeapon = m_hWeapon.Get();
	if ( pWeapon )
	{
		const FileWeaponInfo_t *pInfo = &pWeapon->GetWpnData();
		if ( pInfo->m_bAllowFlipping )
		{
			if ( !ss_righthand.IsValid() )
			{
				ss_righthand.Init( "cl_righthand", true );
			}

			return  pInfo->m_bBuiltRightHanded != ss_righthand.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() );
		}
	}
	
	return false;
}

void CBaseViewModel::ApplyBoneMatrixTransform( matrix3x4_t& transform )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	bool bUsingMotionController = pPlayer && PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() );

	if ( ShouldFlipModel() || bUsingMotionController )
	{
		matrix3x4_t viewMatrix, viewMatrixInverse;

		// We could get MATERIAL_VIEW here, but this is called sometimes before the renderer
		// has set that matrix. Luckily, this is called AFTER the CViewSetup has been initialized.
		const CViewSetup *pSetup = view->GetPlayerViewSetup();
		AngleMatrix( pSetup->angles, pSetup->origin, viewMatrixInverse );
		MatrixInvert( viewMatrixInverse, viewMatrix );

		// Transform into view space.
		matrix3x4_t temp;
		ConcatTransforms( viewMatrix, transform, temp );
		
		if ( ShouldFlipModel() )
		{
			// Flip it along X.

			// (This is the slower way to do it, and it equates to negating the top row).
			//matrix3x4_t mScale;
			//SetIdentityMatrix( mScale );
			//mScale[0][0] = 1;
			//mScale[1][1] = -1;
			//mScale[2][2] = 1;
			//ConcatTransforms( mScale, temp, temp );
			temp[1][0] = -temp[1][0];
			temp[1][1] = -temp[1][1];
			temp[1][2] = -temp[1][2];
			temp[1][3] = -temp[1][3];

		}

		if ( bUsingMotionController )
		{
			matrix3x4_t localAngleMatrix;
			QAngle localAngles = pPlayer->GetEyeAngleOffset();

			// We want to use negative angles if they're closer to zero. If any of the angles are greater than 180, subtract off 360 to give a negative version of it.
			if ( localAngles[YAW] > 180.0f )
			{
				localAngles[YAW] -= 360.0f;
			}
			if ( localAngles[PITCH] > 180.0f )
			{
				localAngles[PITCH] -= 360.0f;
			}

			// If we're looking up ( negative pitch ), we want to scale back the pitch so that we don't cover too much of the screen or reveal parts of the arms we don't want the player to see.
			if ( localAngles[PITCH] < 0.0f )
			{
				localAngles[PITCH] *= vm_pointer_pitch_up_scale.GetFloat();
			}

			// Since the world and view model FOVs don't match up, scale down the offset angles so they point in the same direction as the cursor in the world.
			float viewmodel_fov_ratio = GetClientMode()->GetViewModelFOV() / pPlayer->GetFOV();
			localAngles *= viewmodel_fov_ratio;

			AngleMatrix(localAngles, localAngleMatrix);
			// We want to tweak the transform so that we include the angles for the pointer.
			// Do this by pre multiplying the angle tweaks.
			ConcatTransforms( localAngleMatrix, temp, temp );
		}

		// Transform back out of view space.
		ConcatTransforms( viewMatrixInverse, temp, transform );
	}
}

//-----------------------------------------------------------------------------
// Purpose: check if weapon viewmodel should be drawn
//-----------------------------------------------------------------------------
bool CBaseViewModel::ShouldDraw()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );

	if ( g_bEngineIsHLTV )
	{
		return ( HLTVCamera()->GetMode() == OBS_MODE_IN_EYE &&
				 HLTVCamera()->GetPrimaryTarget() == GetOwner()	);
	}
	else
	{
		contract_assert( GetRenderMode() != kRenderNone );

		if ( vm_draw_always.GetBool() )
			return true;

		CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();

		CBasePlayer *pObsTarget = nullptr;

		if ( pLocalPlayer )
		{
			pObsTarget = ToBasePlayer( pLocalPlayer->GetObserverTarget() );
		}

		if ( pLocalPlayer )
			if ( ( pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE ) && ( GetOwner() != pLocalPlayer ) )
				return false;

		return BaseClass::ShouldDraw();
	}
}

bool CBaseViewModel::ShouldSuppressForSplitScreenPlayer( int nSlot )
{
	if ( vm_draw_always.GetBool() )
	{
		if ( vm_draw_always.GetInt() == 1 )
		{
			return false;
		}
		return true;
	}

	if ( BaseClass::ShouldSuppressForSplitScreenPlayer( nSlot ) )
	{
		return true;
	}
	
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer( nSlot );
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	// We supress viewing of the view model if we are not looking through the eyes of the player who owns that view model.
	// We still need to call animation updates on this view model.
	if ( pOwner == pLocalPlayer )
	{
		return false;
	}

	CBasePlayer *pObserverTarget = ToBasePlayer( pLocalPlayer->GetObserverTarget() );

	if ( pOwner == pObserverTarget )
	{
		// [msmith] We only ever want to draw viewmodels if the player who owns this split screen is in the OBS_MODE_IN_EYE observer mode for view models.
		return ( OBS_MODE_IN_EYE != pLocalPlayer->GetObserverMode() );
	}

	return true;

}

//-----------------------------------------------------------------------------
// Purpose: Render the weapon. Draw the Viewmodel if the weapon's being carried
//			by this player, otherwise draw the worldmodel.
//-----------------------------------------------------------------------------
int CBaseViewModel::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( !UpdateBlending( flags, instance ) )
		return 0;
	
	CMatRenderContextPtr pRenderContext( materials );
	
	if ( (flags & STUDIO_RENDER) && mat_preview.GetString()[0] )
	{
		int nMatRet = 0;
		if ( m_pMaterialPreviewShape == nullptr )
		{
			MDLCACHE_CRITICAL_SECTION();
			const char *pszMatLibraryModel = "models/matlibrary/matlibrary_default.mdl";
			CBaseAnimating *pPreviewShape = new class CBaseAnimating;
			if ( pPreviewShape && pPreviewShape->InitializeAsClientEntity( pszMatLibraryModel, false ) )
			{
				m_pMaterialPreviewShape = pPreviewShape;
				pPreviewShape->SetParent( this );
				pPreviewShape->SetLocalOrigin( Vector(25,0,-4) );
				pPreviewShape->UpdatePartitionListEntry();
				pPreviewShape->CollisionProp()->MarkPartitionHandleDirty();
				pPreviewShape->UpdateVisibility();
				pPreviewShape->SetUseParentLightingOrigin( true );

				pPreviewShape->SetEFlags( EF_NODRAW );
				pPreviewShape->SetEFlags( EF_NOCSM );
			}
			else
			{
				DevWarning( "Couldn't load material library preview model: %s\n", pszMatLibraryModel );
			}
		}
		else
		{
			IMaterial *pMatPreview = materials->FindMaterial( mat_preview.GetString(), TEXTURE_GROUP_OTHER, true );

			//if ( !pMatPreview )
			//{
			//	for ( MaterialHandle_t i = materials->FirstMaterial(); i != materials->InvalidMaterial(); i = materials->NextMaterial(i) )
			//	{
			//		char const *szMatName = materials->GetMaterial(i)->GetName();
			//		if ( V_stristr( szMatName, mat_preview.GetString() ) )
			//		{
			//			pMatPreview = materials->GetMaterial(i);
			//			break;
			//		}
			
			if ( pMatPreview )
			{
				g_pStudioRender->ForcedMaterialOverride( pMatPreview );

				m_pMaterialPreviewShape->SetAbsAngles( QAngle(0,gpGlobals->curtime*8.0f,0) );
				nMatRet = m_pMaterialPreviewShape->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );

				g_pStudioRender->ForcedMaterialOverride( nullptr );
			}
		}

		return nMatRet;
	}

	if ( ShouldFlipModel() )
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	int ret = 0;
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	CBaseCombatWeapon *pWeapon = GetOwningWeapon();

	if ( pWeapon )
	{
		// clear first because number of materials may differ (or be 0)
		ClearCustomMaterials();

		// set our custom materials to the ones on the world model (they are actually view model res because of the owner being local)
		for ( int i = 0; i < pWeapon->GetCustomMaterialCount(); i++ )
		{
			SetCustomMaterial( pWeapon->GetCustomMaterial( i ), i );
		}
	}

	// try to render the scope lens mask stencil shape
	if ( flags && (pWeapon && GetScopeStencilMaskMode() == true) )
	{
		// first create and bonemerge a new scope lens mask stencil shape if we don't have one
		if ( !m_viewmodelScopeStencilMask )
		{
			if ( pWeapon )
			{
				CEconItemView *pItem = pWeapon->GetEconItemView();
				if ( pItem && pItem->GetScopeLensMaskModel() )
				{
					CViewmodelAttachmentModel *pScopeStencilMask = new class CViewmodelAttachmentModel;
					if ( pScopeStencilMask && pScopeStencilMask->InitializeAsClientEntity( pItem->GetScopeLensMaskModel(), true ) )
					{
						m_viewmodelScopeStencilMask = pScopeStencilMask;
						pScopeStencilMask->SetParent( this );
						pScopeStencilMask->SetLocalOrigin( vec3_origin );
						pScopeStencilMask->UpdatePartitionListEntry();
						pScopeStencilMask->UpdateVisibility();
						pScopeStencilMask->SetViewmodel( this );
					}
				}
			}
		}

		// now render the scope lens mask stencil shape if we have one
		if ( m_viewmodelScopeStencilMask && m_viewmodelScopeStencilMask.Get() )
		{
			render->SetBlend( 0.0f );
			pRenderContext->OverrideColorWriteEnable( true, false );
			pRenderContext->OverrideDepthEnable( true, false, true );

			IMaterial *pMatScopeDummyMaterial = materials->FindMaterial( "dev/scope_mask", TEXTURE_GROUP_OTHER, true );
			g_pStudioRender->ForcedMaterialOverride( pMatScopeDummyMaterial );
			
			m_viewmodelScopeStencilMask->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );

			g_pStudioRender->ForcedMaterialOverride( nullptr );
		}
	}
	else
	{
		//otherwise, render the viewmodel normally

		// If the local player's overriding the viewmodel rendering, let him do it
		if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		{
			ret = pPlayer->DrawOverriddenViewmodel( this, flags, instance );
		}
		else if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		{
			ret = pWeapon->DrawOverriddenViewmodel( this, flags, instance );
		}
		else
		{
			ret = BaseClass::DrawModel( flags, instance );
		}
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	// Now that we've rendered, reset the animation restart flag
	if ( flags & STUDIO_RENDER )
	{
		if ( m_nOldAnimationParity != m_nAnimationParity )
		{
			m_nOldAnimationParity = m_nAnimationParity;
		}

		// Tell the weapon itself that we've rendered, in case it wants to do something
		if ( pWeapon )
		{
			pWeapon->ViewModelDrawn( flags, this );
		}

		if ( vm_debug.GetBool() )
		{
			MDLCACHE_CRITICAL_SECTION();

			int line = 16;
			CStudioHdr *hdr = GetModelPtr();
			engine->Con_NPrintf( line++, "%s: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
				(hdr)?hdr->pszName():"(null)",
				GetSequenceName( GetSequence() ),
				GetSequence(),
				GetCycle(), 
				GetSequenceCycleRate( hdr, GetSequence() ),
				GetPlaybackRate()
				);
			if ( hdr )
			{
				for( int i=0; i < hdr->GetNumPoseParameters(); ++i )
				{
					const mstudioposeparamdesc_t &Pose = hdr->pPoseParameter( i );
					engine->Con_NPrintf( line++, "pose_param %s: %f",
						Pose.pszName(), GetPoseParameter( i ) );
				}
			}

			// Determine blending amount and tell engine
			float blend = (float)( instance.m_nAlpha / 255.0f );
			float color[3];
			GetColorModulation( color );
			engine->Con_NPrintf( line++, "blend=%f, color=%f,%f,%f", blend, color[0], color[1], color[2] );
			engine->Con_NPrintf( line++, "GetRenderMode()=%d", GetRenderMode() );
			engine->Con_NPrintf( line++, "m_nRenderFX=0x%8.8X", GetRenderFX() );

			color24 c = GetRenderColor();
			unsigned char a = GetRenderAlpha();
			engine->Con_NPrintf( line++, "rendercolor=%d,%d,%d,%d", c.r, c.g, c.b, a );

			engine->Con_NPrintf( line++, "origin=%f, %f, %f", GetRenderOrigin().x, GetRenderOrigin().y, GetRenderOrigin().z );
			engine->Con_NPrintf( line++, "angles=%f, %f, %f", GetRenderAngles()[0], GetRenderAngles()[1], GetRenderAngles()[2] );

			if ( IsEffectActive( EF_NODRAW ) )
			{
				engine->Con_NPrintf( line++, "EF_NODRAW" );
			}
		}
	}

	if ( flags && vm_draw_addon.GetBool() 
		&& (GetScopeStencilMaskMode() == false) 
		)
	{
		for ( auto &hArmModel : m_vecViewmodelArmModels )
		{
			if ( hArmModel )
			{

				if ( hArmModel->GetMoveParent() != this )
				{
					hArmModel->SetEFlags( EF_BONEMERGE );
					hArmModel->SetParent( this );
				}

				hArmModel->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );
			}
		}
		for ( int i=0; i < std::ssize( m_hStickerModelAddons ); ++i )
		{
			if ( m_hStickerModelAddons[i] )
			{
				m_hStickerModelAddons[i]->DrawModel( flags, instance );
			}
		}
		if ( m_viewmodelStatTrakAddon )
		{
			m_viewmodelStatTrakAddon->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );
		}
		if ( m_viewmodelUidAddon )
		{
			m_viewmodelUidAddon->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );
		}
	}
	
	//Scope stencil mask mode is automatically turned off after rendering. It needs to be explicitly enabled before each draw.
	if ( flags )
		SetScopeStencilMaskMode( false );

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Called by the player when the player's overriding the viewmodel drawing. Avoids infinite recursion.
//-----------------------------------------------------------------------------
int CBaseViewModel::DrawOverriddenViewmodel( CBaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance )
{
	return BaseClass::DrawModel( flags, instance );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
uint8 CBaseViewModel::OverrideAlphaModulation( uint8 nAlpha )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	// See if the local player wants to override the viewmodel's rendering
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		return pPlayer->AlphaProp()->ComputeRenderAlpha();
	
	CBaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		return pWeapon->AlphaProp()->ComputeRenderAlpha();

	return nAlpha;

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t CBaseViewModel::ComputeTranslucencyType( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	// See if the local player wants to override the viewmodel's rendering
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		return pPlayer->ComputeTranslucencyType();

	CBaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		return pWeapon->ComputeTranslucencyType();

	return BaseClass::ComputeTranslucencyType();

}

//-----------------------------------------------------------------------------
// Purpose: If the animation parity of the weapon has changed, we reset cycle to avoid popping
//-----------------------------------------------------------------------------
void CBaseViewModel::UpdateAnimationParity( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	// If we're predicting, then we don't use animation parity because we change the animations on the clientside
	// while predicting. When not predicting, only the server changes the animations, so a parity mismatch
	// tells us if we need to reset the animation.
	if ( ( m_nOldAnimationParity != m_nAnimationParity && !GetPredictable() ) )
	{
		float curtime = (pPlayer && IsIntermediateDataAllocated()) ? pPlayer->GetFinalPredictedTime() : gpGlobals->curtime;
		// FIXME: this is bad
		// Simulate a networked m_flAnimTime and m_flCycle
		// FIXME:  Do we need the magic 0.1?
		SetCycle( 0.0f ); // GetSequenceCycleRate( GetSequence() ) * 0.1;
		m_flAnimTime = curtime;
		m_fCycleOffset = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update global map state based on data received
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void CBaseViewModel::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		AlphaProp()->EnableAlphaModulationOverride( true );
	}

	SetPredictionEligible( true );
	BaseClass::OnDataChanged(updateType);
}
void CBaseViewModel::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate(updateType);
	OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
CBasePlayer *CBaseViewModel::GetPredictionOwner()
{
	return ToBasePlayer( GetOwner() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS])
{
	BaseClass::GetBoneControllers( controllers );

	// Tell the weapon itself that we've rendered, in case it wants to do something
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->GetViewmodelBoneControllers( this, controllers );
	}
}

void CBaseViewModel::UpdateAllViewmodelAddons( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetOwner() );

	// Remove any view model add ons if we're spectating.
	if ( !pPlayer )
	{
		RemoveViewmodelArmModels();
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}

	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( !pWeapon )
	{
		RemoveViewmodelArmModels();
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}
	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( pWeapon );
	if ( !pCSWeapon )
	{
		RemoveViewmodelArmModels();
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}

	int weaponID = pCSWeapon->GetCSWeaponID();

	// Note: only arms race (gun game) knives change their bodygroup to indicate their team. This will soon go away.
	if ( weaponID == WEAPON_KNIFE_GG )
	{
		int bodyPartID = ( pPlayer->GetTeamNumber() == TEAM_TERRORIST ) ? 0 : 1;
		SetBodygroup( 0 , bodyPartID );
	}

	if ( pPlayer->m_pViewmodelArmConfig == nullptr )
	{
		RemoveViewmodelArmModels();

		CStudioHdr *pHdr = pPlayer->GetModelPtr();
		if ( pHdr )
		{
			pPlayer->m_pViewmodelArmConfig = GetPlayerViewmodelArmConfigForPlayerModel( pHdr->pszName() );
		}
	}
	
	// add gloves and sleeves
	if ( pPlayer->m_pViewmodelArmConfig != nullptr && std::ssize( m_vecViewmodelArmModels ) == 0 )
	{
		{
			AddViewmodelArmModel( pPlayer->m_pViewmodelArmConfig->szAssociatedGloveModel, atoi(pPlayer->m_pViewmodelArmConfig->szSkintoneIndex) );
			AddViewmodelArmModel( pPlayer->m_pViewmodelArmConfig->szAssociatedSleeveModel );
		}
	}

	// econ-related addons follow, so bail out if we can't get at the econitemview
	CEconItemView *pItem = pWeapon->GetEconItemView();
	if ( !pItem )
	{
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}

	// verify weapon label and add if necessary
	AddViewmodelLabel( pItem );

	// verify stattrak module and add if necessary
	std::flat_multiset<uint32> vTypes;

	pItem->GetKillEaterTypes( vTypes );
	if ( (!vTypes.empty()) && ( pItem->GetKillEaterValueByType( *vTypes.rbegin() ) >= 0 ) )
	{
		CPlayerId HolderPlayerID;
		pPlayer->GetPlayerID( &HolderPlayerID );
		AddViewmodelStatTrak( pItem, *vTypes.rbegin(), weaponID, HolderPlayerID.GetAccountID() );
	}
	else
	{
		RemoveViewmodelStatTrak();
	}
	
	// add viewmodel stickers
	AddViewmodelStickers( pItem, weaponID );

	if ( m_viewmodelScopeStencilMask )
	{
		CViewmodelAttachmentModel *pScopeMask = m_viewmodelScopeStencilMask.Get();
		if ( pScopeMask )
			pScopeMask->Remove();
	}

}

CViewmodelAttachmentModel* CBaseViewModel::FindArmModelForLoadoutPosition( loadout_positions_t nPosition ) const
{
	/* Removed for partner depot */
	return nullptr;
}

//--------------------------------------------------------------------------------------------------------
CViewmodelAttachmentModel* CBaseViewModel::AddViewmodelArmModel( const char *pszArmsModel, int nSkintoneIndex )
{
	if ( pszArmsModel == nullptr || pszArmsModel[ 0 ] == '\0' || modelinfo->GetModelIndex( pszArmsModel ) == -1 )
	{
		//pszArmsModel = //g_pGameTypes->GetCTViewModelArmsForMap( engine->GetLevelNameShort() );
		CCSPlayer *pPlayer = ToCSPlayer( GetOwner() );
		pszArmsModel = pPlayer->m_szArmsModel.Get();
	}

	// Only create the view model attachment if we have a valid arm model
	if ( pszArmsModel == nullptr || pszArmsModel[0] == '\0' || modelinfo->GetModelIndex( pszArmsModel ) == -1 )
		return nullptr;

	CViewmodelAttachmentModel *pEnt = new class CViewmodelAttachmentModel;
	if ( pEnt && pEnt->InitializeAsClientEntity( pszArmsModel, true ) )
	{
		m_vecViewmodelArmModels.emplace_back() = pEnt;

		if ( nSkintoneIndex != -1 )
			pEnt->SetSkin( nSkintoneIndex );

		pEnt->SetParent( this );
		pEnt->SetLocalOrigin( vec3_origin );
		pEnt->UpdatePartitionListEntry();
		pEnt->CollisionProp()->MarkPartitionHandleDirty();
		pEnt->UpdateVisibility();
		pEnt->SetViewmodel( this );
		pEnt->SetUseParentLightingOrigin( true );

		RemoveEffects( EF_NODRAW );
		return pEnt;
	}	

	return nullptr;
}

void CBaseViewModel::AddViewmodelLabel( CEconItemView *pItem )
{
	if ( !pItem || !pItem->GetCustomName() )
	{
		RemoveViewmodelLabel();
		return;
	} else if ( m_viewmodelUidAddon && m_viewmodelUidAddon.Get() && m_viewmodelUidAddon->GetMoveParent() )
	{
		return;
	}
	
	RemoveViewmodelLabel();	

	CViewmodelAttachmentModel *pUidEnt = new class CViewmodelAttachmentModel;
	if ( pUidEnt && pUidEnt->InitializeAsClientEntity( pItem->GetUidModel(), true ) )
	{
		m_viewmodelUidAddon = pUidEnt;
		pUidEnt->SetParent( this );
		pUidEnt->SetLocalOrigin( vec3_origin );
		pUidEnt->UpdatePartitionListEntry();
		pUidEnt->CollisionProp()->MarkPartitionHandleDirty();
		pUidEnt->UpdateVisibility();
		pUidEnt->SetViewmodel( this );
		pUidEnt->SetUseParentLightingOrigin( true );

		if ( !cl_righthand.GetBool() )
		{
			pUidEnt->SetBodygroup( 0, 1 ); // use a special mirror-image that appears correct for lefties
		}
	
		RemoveEffects( EF_NODRAW );
	}
}

void CBaseViewModel::AddViewmodelStatTrak( CEconItemView *pItem, int nStatTrakType, int nWeaponID, AccountID_t holderAcctId )
{
	if ( m_viewmodelStatTrakAddon && m_viewmodelStatTrakAddon.Get() && m_viewmodelStatTrakAddon->GetMoveParent() )
		return;

	RemoveViewmodelStatTrak();

	if (!pItem)
		return;

	CViewmodelAttachmentModel *pStatTrakEnt = new class CViewmodelAttachmentModel;
	if ( pStatTrakEnt && pStatTrakEnt->InitializeAsClientEntity( pItem->GetStatTrakModelByType( nStatTrakType ), true ) )
	{
		m_viewmodelStatTrakAddon = pStatTrakEnt;
		pStatTrakEnt->SetParent( this );
		pStatTrakEnt->SetLocalOrigin( vec3_origin );
		pStatTrakEnt->UpdatePartitionListEntry();
		pStatTrakEnt->CollisionProp()->MarkPartitionHandleDirty();
		pStatTrakEnt->UpdateVisibility();
		pStatTrakEnt->SetViewmodel( this );
		pStatTrakEnt->SetUseParentLightingOrigin( true );

		if ( !cl_righthand.GetBool() )
		{
			pStatTrakEnt->SetBodygroup( 0, 1 ); // use a special mirror-image stattrak module that appears correct for lefties
		}

		// this stat trak weapon doesn't belong to the current holder, display error message on the digital display. This is impossible for knives
		if ( nWeaponID != WEAPON_KNIFE && nWeaponID != WEAPON_KNIFE_GG )
		{
			if ( pItem->GetAccountID() != holderAcctId )
			{
				pStatTrakEnt->SetBodygroup( 1, cl_righthand.GetBool() ? 1 : 2 ); // show the error screen bodygroup
			}
		}

		RemoveEffects( EF_NODRAW );
	}
}

bool CBaseViewModel::ViewmodelStickersAreValid( int nWeaponID )
{
	if ( m_hStickerModelAddons.empty() )
	{
		return false;
	}
	// returns true if all viewmodel sticker handles are non-null and appropriate for the given weapon id
	for ( int i=0; i < std::ssize( m_hStickerModelAddons ); ++i )
	{
		if ( !m_hStickerModelAddons[i] || !m_hStickerModelAddons[i].Get() || !m_hStickerModelAddons[i]->GetMoveParent() )
		{
			return false;
		}
	}
	return true;
}

void CBaseViewModel::AddViewmodelStickers( CEconItemView *pItem, int nWeaponID )
{
	/* Removed for partner depot */
}

void CBaseViewModel::RemoveViewmodelArmModels( void )
{
	for ( auto &hArmModel : m_vecViewmodelArmModels | std::views::reverse )
	{
		CViewmodelAttachmentModel *pEnt = hArmModel.Get();
		if ( pEnt )
		{
			pEnt->Remove();
		}
	}
	m_vecViewmodelArmModels.clear();
}

void CBaseViewModel::RemoveViewmodelLabel( void )
{
	CViewmodelAttachmentModel *pUidEnt = m_viewmodelUidAddon.Get();
	if ( pUidEnt )
	{
		pUidEnt->Remove();
	}
}

void CBaseViewModel::RemoveViewmodelStatTrak( void )
{
	CViewmodelAttachmentModel *pStatTrakEnt = m_viewmodelStatTrakAddon.Get();
	if ( pStatTrakEnt )
	{
		pStatTrakEnt->Remove();
	}
}

void CBaseViewModel::RemoveViewmodelStickers( void )
{
	for ( int i=0; i < std::ssize( m_hStickerModelAddons ); ++i )
	{
		CViewmodelAttachmentModel *pStickerAddon = m_hStickerModelAddons[i];
		if ( pStickerAddon )
		{
			pStickerAddon->Remove();
		}
	}
	m_hStickerModelAddons.clear();
}

//--------------------------------------------------------------------------------------------------------
// CViewmodelAttachmentModel - implementations moved from econ_entity.cpp
//--------------------------------------------------------------------------------------------------------
bool CViewmodelAttachmentModel::InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels )
{
	if ( !BaseClass::InitializeAsClientEntity( pszModelName, bRenderWithViewModels ) )
		return false;

	AddEffects( EF_BONEMERGE );
	AddEffects( EF_BONEMERGE_FASTCULL );

	// Invisible by default, and made visible->drawn->made invisible when the viewmodel is drawn
	AddEffects( EF_NODRAW );
	return true;
}

int CViewmodelAttachmentModel::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
	CMatRenderContextPtr pRenderContext( materials );
	CBaseViewModel *pViewmodel = m_hViewmodel;
	if ( pViewmodel && pViewmodel->ShouldFlipModel() )
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	int r = BaseClass::InternalDrawModel( flags, instance );

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	return r;
}

void CViewmodelAttachmentModel::SetViewmodel( CBaseViewModel *pVM )
{
	m_hViewmodel = pVM;
}

class CIronSightController;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CWeaponCSBase;

using namespace ::ks::reflect;

extern ConVar in_forceuser;

static constexpr auto VIEWMODEL_ANIMATION_PARITY_BITS = 3;
inline constexpr const char *SCREEN_OVERLAY_MATERIAL = "vgui/screens/vgui_overlay";

	ConVar viewmodel_offset_x( side_registry(), "viewmodel_offset_x", "0.0", FCVAR_ARCHIVE );	 // the viewmodel offset from default in X
	ConVar viewmodel_offset_y( side_registry(), "viewmodel_offset_y", "0.0", FCVAR_ARCHIVE );	 // the viewmodel offset from default in Y
	ConVar viewmodel_offset_z( side_registry(), "viewmodel_offset_z", "0.0", FCVAR_ARCHIVE );	 // the viewmodel offset from default in Z

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::CBaseViewModel()
{
	// NOTE: We do this here because the color is never transmitted for the view model.
	m_nOldAnimationParity = 0;
	m_EntClientFlags |= ENTCLIENTFLAG_ALWAYS_INTERPOLATE;
	RenderWithViewModels( true );
	m_flStatTrakGlowMultiplier = 0.0f;
	m_flStatTrakGlowMultiplierIdeal = 0.0f;
	m_szLastSound[0] = '\0';
	m_flLastSoundTime = 0.0f;

	m_flCamDriverAppliedTime = 0;
	m_flCamDriverWeight = 0;
	m_vecCamDriverLastPos.Init();
	m_angCamDriverLastAng.Init();

	m_bScopeStencilMaskModeEnabled = false;

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

void CBaseViewModel::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	DestroyControlPanels();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Spawn( void )
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

	SetModel( modelname );
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

	m_nOldAnimationParity = m_nAnimationParity;

	// Force frame interpolation to start at exactly frame zero
	m_flAnimTime			= gpGlobals->curtime;

	// Restart animation at frame 0
	SetCycle( 0 );
	ResetSequenceInfo();
}

void CBaseViewModel::PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] )
{
	int nCamDriverBone = LookupBone( "cam_driver" );
	if ( nCamDriverBone != -1 )
	{
		m_flCamDriverAppliedTime = gpGlobals->curtime;		
		VectorCopy( pos[nCamDriverBone], m_vecCamDriverLastPos );
		QuaternionAngles( q[nCamDriverBone], m_angCamDriverLastAng );

		if ( ShouldFlipModel() )
		{
			m_angCamDriverLastAng[YAW] = -m_angCamDriverLastAng[YAW];
			m_vecCamDriverLastPos.y = -m_vecCamDriverLastPos.y;
		}

	}
}

void CBaseViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{

	// apply viewmodel pose param
	if ( owner )
	{
		CBaseCSGrenade* pGrenade = dynamic_cast<CBaseCSGrenade*>( owner->GetActiveWeapon() );
		if ( pGrenade )
		{
			int iPoseParam = LookupPoseParameter( "throwcharge" );
			if ( iPoseParam != -1 )
				SetPoseParameter( iPoseParam, clamp(pGrenade->ApproachThrownStrength(), 0.0f, 1.0f) );
		}
	}

	// UNDONE: Calc this on the server?  Disabled for now as it seems unnecessary to have this info on the server
	QAngle vmangoriginal = eyeAngles;
	QAngle vmangles = eyeAngles;
	Vector vmorigin = eyePosition;

	Vector vecRight;
	Vector vecUp;
	Vector vecForward;
	AngleVectors( vmangoriginal, &vecForward, &vecRight, &vecUp );
	//Vector vecOffset = Vector( viewmodel_offset_x.GetFloat(), viewmodel_offset_y.GetFloat(), viewmodel_offset_z.GetFloat() ); 
	if ( !m_bShouldIgnoreOffsetAndAccuracy )
	{
		CWeaponCSBase *pIronSightWeapon = (CWeaponCSBase*)owner->GetActiveWeapon();
		if ( pIronSightWeapon )
		{
			CIronSightController* pIronSightController = pIronSightWeapon->GetIronSightController();
			if ( pIronSightController && pIronSightController->IsInIronSight() )
			{
				float flInvIronSightAmount = ( 1.0f - pIronSightController->GetIronSightAmount() );

				vecForward *= flInvIronSightAmount;
				vecUp *= flInvIronSightAmount;
				vecRight *=	flInvIronSightAmount;
			}
		}	
		vmorigin += (vecForward * viewmodel_offset_y.GetFloat()) + (vecUp * viewmodel_offset_z.GetFloat()) + (vecRight * viewmodel_offset_x.GetFloat());
	}

	// TrackIR
	if ( IsHeadTrackingEnabled() )
	{
		vmorigin = owner->EyePosition();
		VectorAngles( owner->GetAutoaimVector( AUTOAIM_5DEGREES ), vmangoriginal );
		vmangles = vmangoriginal;
	}
	// TrackIR

	CBaseCombatWeapon *pWeapon = m_hWeapon.Get();
	//Allow weapon lagging
	if ( pWeapon != nullptr )
	{
		if ( !prediction->InPrediction() )
		{
			// add weapon-specific bob 
			pWeapon->AddViewmodelBob( this, vmorigin, vmangles );
			CalcViewModelLag( vmorigin, vmangles, vmangoriginal );
		}
	}
	// Add model-specific bob even if no weapon associated (for head bob for off hand models)
	AddViewModelBob( owner, vmorigin, vmangles );

	if ( !prediction->InPrediction() )
	{
		// Let the viewmodel shake at about 10% of the amplitude of the player's view
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );
		GetViewEffects()->ApplyShake( vmorigin, vmangles, 0.1 );	
	}

	SetLocalOrigin( vmorigin );
	SetLocalAngles( vmangles );

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
  extern void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );
 void RecvProxy_ViewmodelSequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut );
 void RecvProxy_Viewmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );

//-----------------------------------------------------------------------------
// Purpose: Resets anim cycle when the server changes the weapon on us
//-----------------------------------------------------------------------------
 void RecvProxy_Weapon( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *pViewModel = ((CBaseViewModel*)pStruct);
	CBaseCombatWeapon *pOldWeapon = pViewModel->GetOwningWeapon();
	bool bViewModelWasVisible = pViewModel->IsVisible();

	// Chain through to the default recieve proxy ...
	RecvProxy_IntToEHandle( pData, pStruct, pOut );

	// ... and reset our cycle index if the server is switching weapons on us
	CBaseCombatWeapon *pNewWeapon = pViewModel->GetOwningWeapon();
	if ( pNewWeapon != pOldWeapon || !bViewModelWasVisible )
	{
		// Restart animation at frame 0
		pViewModel->SetCycle( 0 );
		pViewModel->m_flAnimTime = gpGlobals->curtime;
	}
}

 void RecvProxy_Owner( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *pViewModel = ( ( CBaseViewModel* )pStruct );
	//Msg( "BaseViewModel changed from (%d)%x", ( pViewModel->m_hOwner.GetForModify().GetSerialNumber(), pViewModel->m_hOwner.GetForModify().GetEntryIndex() ) );

	// Chain through to the default recieve proxy ...
	RecvProxy_IntToEHandle( pData, pStruct, pOut );
	
	//Msg( " to (%d)%x\n", ( pViewModel->m_hOwner.GetForModify().GetSerialNumber(), pViewModel->m_hOwner.GetForModify().GetEntryIndex() ) );
	pViewModel->UpdateVisibility(); // visibility of a viewmodel is owner-dependant, and other events like SetDormant() may happen out of order with setting owner, especially when doing full frame update after spectator mode, which happens most often (pretty much exclusively) after HLTV replay ends.
}

IMPLEMENT_NETWORKCLASS_ALIASED( BaseViewModel, DT_BaseViewModel )
LINK_ENTITY_TO_CLASS_ALIASED( viewmodel, BaseViewModel );

IMPLEMENT_REFLECT_TABLE( CBaseViewModel, DT_BaseViewModel );

IMPLEMENT_REFLECT_PREDMAP( CBaseViewModel );

// This needed to be done as a proxy for the surrounding box auto update when animations change.
// This doesn't have to be done for view models as they don't affect the bounding box and it was
// causing some timing problems with our world to view model under the covers swap.

// [msmith] Added back in for CS:GO because without this the m_nSequence number gets reset during prediction causing
// view model animations to freeze up.  This issue is probably caused by the fact that prediction doesn't fix up
// m_nSequence, but this fixes it and makes it consistent with CS:S ... which also has the same prediction issues.
void RecvProxy_ViewmodelSequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *model = (CBaseViewModel *)pStruct;
	if (pData->m_Value.m_Int != model->GetSequence())
	{
		MDLCACHE_CRITICAL_SECTION();
		model->SetSequence(pData->m_Value.m_Int);
		model->m_flAnimTime = gpGlobals->curtime;
		model->SetCycle(0);
	}
}

void RecvProxy_Viewmodel( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// We assign the model index via the SetModelByIndex function so that the model pointer gets updated as soon as we change the model index.
	// This is necessary since this new model may be accessed with frame.
	// An example is the SetSequence code in RecvProxy_ViewmodelSequenceNum that checks to make sure the sequence number is in range of those available in
	// model.
	CBaseViewModel *model = (CBaseViewModel *)pStruct;
	if ( model )
	{
		MDLCACHE_CRITICAL_SECTION();
		model->SetModelByIndex( pData->m_Value.m_Int );
	}
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseViewModel::LookupAttachment( const char *pAttachmentName )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->LookupAttachment( pAttachmentName );

	return BaseClass::LookupAttachment( pAttachmentName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, matrix3x4_t &matrix )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, matrix );

	return BaseClass::GetAttachment( number, matrix );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, Vector &origin )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, origin );

	return BaseClass::GetAttachment( number, origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, origin, angles );

	return BaseClass::GetAttachment( number, origin, angles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachmentVelocity( number, originVel, angleVel );

	return BaseClass::GetAttachmentVelocity( number, originVel, angleVel );
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CBeam;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

static constexpr auto BEAM_DEFAULT_HALO_SCALE = 10;

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

extern bool ComputeBeamEntPosition( CBaseEntity *pEnt, int nAttachment, bool bInterpretAttachmentIndexAsHitboxIndex, Vector& pt );
extern void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Beam_ScrollSpeed( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBeam *beam;
	float	val;

	// Unpack the data.
	val	= pData->m_Value.m_Float;
	val *= 0.1;

	beam = ( CBeam * )pStruct;
	contract_assert( pOut == &beam->m_fSpeed );

	beam->m_fSpeed = val;
}

// This table encodes the CBeam data.
IMPLEMENT_NETWORKCLASS_ALIASED( Beam, DT_Beam )
LINK_ENTITY_TO_CLASS_ALIASED( beam, Beam );

IMPLEMENT_REFLECT_TABLE( CBeam, DT_Beam );

IMPLEMENT_REFLECT_PREDMAP( CBeam );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBeam::CBeam( void )
{

	m_flHDRColorScale = 1.0f; // default value.

	m_queryHandleHalo = 0;
	AddToEntityList(ENTITY_LIST_SIMULATE);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szModelName - 
//-----------------------------------------------------------------------------
void CBeam::SetModel( const char *szModelName )
{
	int modelIndex = modelinfo->GetModelIndex( szModelName );
	const model_t *model = modelinfo->GetModel( modelIndex );
	if ( model && modelinfo->GetModelType( model ) != mod_sprite )
	{
		Msg( "Setting CBeam to non-sprite model %s\n", szModelName );
	}
	BaseClass::SetModel( szModelName );
}

void CBeam::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetRenderMode( kRenderTransTexture );

	// Opt out of all shadow routines
	// Beams are highly visible; always render these into reflection buffer even when using fast reflection
	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW | EF_MARKED_FOR_FAST_REFLECTION );

	Precache( );
}

void CBeam::Precache( void )
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

//-----------------------------------------------------------------------------
// Unlike the server, these take attachments into account
//-----------------------------------------------------------------------------
const Vector &CBeam::GetAbsStartPos( void ) const
{
	static Vector vecStartAbsPosition;
	if ( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE ) 
	{
		if (ComputeBeamEntPosition( m_hAttachEntity[0], m_nAttachIndex[0], false, vecStartAbsPosition ))
			return vecStartAbsPosition;
	}

	return GetAbsOrigin();
}

const Vector &CBeam::GetAbsEndPos( void ) const
{
	static Vector vecEndAbsPosition;
	if ( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE ) 
	{
		if (ComputeBeamEntPosition( m_hAttachEntity[m_nNumBeamEnts-1], m_nAttachIndex[m_nNumBeamEnts-1], false, vecEndAbsPosition ))
			return vecEndAbsPosition;
	}

	if (!const_cast<CBeam*>(this)->GetMoveParent())
		return m_vecEndPos.Get();

	// FIXME: Cache this off?
	VectorTransform( m_vecEndPos, EntityToWorldTransform(), vecEndAbsPosition );
	return vecEndAbsPosition;
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
	return nullptr;
}

void CBeam::DoSparks( const Vector &start, const Vector &end )
{
}

//-----------------------------------------------------------------------------
// Purpose: Damages anything in the beam.
// Input  : ptr - 
//-----------------------------------------------------------------------------
void CBeam::BeamDamage( trace_t *ptr )
{
	RelinkBeam();
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
// Purpose: Draw any debug text overlays.
// Output : Returns the new text offset from the top.
//-----------------------------------------------------------------------------
int CBeam::DrawDebugTextOverlays(void)
{
	return 0;
}

// Purpose: 
// Input  : isbeingremoved - 
//			*predicted - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBeam::OnPredictedEntityRemove( bool isbeingremoved, CBaseEntity *predicted )
{
	BaseClass::OnPredictedEntityRemove( isbeingremoved, predicted );

	CBeam *beam = dynamic_cast< CBeam * >( predicted );
	if ( !beam )
	{
		// Hrm, we didn't link up to correct type!!!
		contract_assert( 0 );
		// Delete right away since it's fucked up
		return true;
	}

	if ( beam->IsEFlagSet( EFL_KILLME ) )
	{
		// Don't delete right away
		AddEFlags( EFL_KILLME );
		return false;
	}

	// Go ahead and delete if it's not short-lived
	return true;
}

extern bool g_bRenderingScreenshot;
extern ConVar r_drawviewmodel;

int CBeam::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( IsMarkedForDeletion() )
		return 0;

	if ( CurrentViewID() == VIEW_SHADOW_DEPTH_TEXTURE )
		return 0;

	// Tracker 16432:  If rendering a savegame screenshot don't draw beams 
	//   who have viewmodels as their attached entity
	if ( g_bRenderingScreenshot || !r_drawviewmodel.GetBool() )
	{
		// If the beam is attached
		for (int i=0;i<MAX_BEAM_ENTS;i++)
		{
			CBaseViewModel *vm = ToBaseViewModel(m_hAttachEntity[i].Get());
			if ( vm )
			{
				return 0;
			}
		}
	}

	beams->DrawBeam( this, instance );
	return 0;
}

void CBeam::OnDataChanged( DataUpdateType_t updateType )
{
	MarkMessageReceived();

	// Make sure that the correct model is referenced for this entity
	SetModelPointer( modelinfo->GetModel( GetModelIndex() ) );

	// Convert weapon world models to viewmodels if they're weapons being carried by the local player
	for (int i=0;i<MAX_BEAM_ENTS;i++)
	{
		CBaseEntity *pEnt = m_hAttachEntity[i].Get();
		if ( pEnt )
		{
			CBaseCombatWeapon *pWpn = pEnt->MyCombatWeaponPointer();
			if ( pWpn && pWpn->IsCarriedByLocalPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( pWpn->GetOwner() );

				CBaseViewModel *pViewModel = player ? player->GetViewModel( 0 ) : nullptr;
				if ( pViewModel )
				{
					// Get the viewmodel and use it instead
					m_hAttachEntity.Set( i, pViewModel );
				}
			}
		}
	}

	// Compute the bounds here...
	Vector mins, maxs;
	ComputeBounds( mins, maxs );
	SetCollisionBounds( mins, maxs );
	AddToEntityList( ENTITY_LIST_SIMULATE );
}

RenderableTranslucencyType_t CBeam::ComputeTranslucencyType() 
{ 
	return RENDERABLE_IS_TRANSLUCENT; 
}

//-----------------------------------------------------------------------------
// Purpose: Adds to beam entity list
//-----------------------------------------------------------------------------
bool CBeam::Simulate( void )
{
	bool bRet = false;
	// If set to invisible, skip. Do this before resetting the entity pointer so it has 
	// valid data to decide whether it's visible.
	if ( ShouldDraw() )
	{

		//FIXME: If we're hooked up to an attachment point, then recompute our bounds every frame
		if ( m_hAttachEntity[0].Get() || m_hAttachEntity[1].Get() )
		{
			// Compute the bounds here...
			Vector mins, maxs;
			ComputeBounds( mins, maxs );
			SetCollisionBounds( mins, maxs );
			bRet = true;
		}

		MoveToLastReceivedPosition();
	}
	BaseClass::Simulate();
	return bRet;
}

//-----------------------------------------------------------------------------
// Computes the bounding box of a beam local to the origin of the beam
//-----------------------------------------------------------------------------
void CBeam::ComputeBounds( Vector& mins, Vector& maxs )
{
	Vector vecAbsStart = GetAbsStartPos();
	Vector vecAbsEnd = GetAbsEndPos();

	switch( GetType() )
	{
	case BEAM_LASER:
	case BEAM_ENTS:
	case BEAM_SPLINE:
		{
			// Compute the bounds here...
			Vector attachmentPoint( 0, 0, 0 );
			mins.Init( 99999, 99999, 99999 );
			maxs.Init( -99999, -99999, -99999 );
			for (int i = 0; i < m_nNumBeamEnts; ++i )
			{
				CBaseEntity *pTestEnt = m_hAttachEntity[i].Get();
				if ( pTestEnt )
				{
					if ( pTestEnt == this )
					{
						mins = maxs = GetAbsOrigin();
					}
					else
					{
						// We do this so we don't have to calculate attachments (and do expensive bone-setup calculations) on our attachments.
						Vector attMins, attMaxs;
						m_hAttachEntity[i]->GetRenderBoundsWorldspace( attMins, attMaxs );

						mins = mins.Min( attMins );
						mins = mins.Min( attMaxs );
						
						maxs = maxs.Max( attMins );
						maxs = maxs.Max( attMaxs );
					}
					
					//ASSERT_COORD( mins );
					//ASSERT_COORD( maxs );
				}
				else
				{
					if (i == 0)
					{
						VectorCopy( vecAbsStart, attachmentPoint );
					}
					else if (i == 1)
					{
						VectorCopy( vecAbsEnd, attachmentPoint );
					}
					else
					{
						contract_assert( 0 );
					}

					mins = mins.Min( attachmentPoint );
					maxs = maxs.Max( attachmentPoint );
				}
			}
		}
		break;
	case BEAM_POINTS:
	default:
		{
			for (int i = 0; i < 3; ++i)
			{
				if (vecAbsStart[i] < vecAbsEnd[i])
				{
					mins[i] = vecAbsStart[i];
					maxs[i] = vecAbsEnd[i];
				}
				else
				{
					mins[i] = vecAbsEnd[i];
					maxs[i] = vecAbsStart[i];
				}
			}
		}
		break;
	}

	// bloat the bounding box by the width of the beam
	float rad = 0.5f * std::max( m_fWidth.Get(), m_fEndWidth.Get() );
	Vector vecRad( rad, rad, rad );
	mins -= vecRad;
	maxs += vecRad;

	// Make sure the bounds are measured in *relative coords*
	Vector vecAbsOrigin = GetAbsOrigin();
	mins -= vecAbsOrigin;
	maxs -= vecAbsOrigin;
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
	SetNetworkOrigin( pos );
	SetLocalOrigin( pos );
}

void CBeam::SetEndPos( const Vector &pos ) 
{ 
	m_vecEndPos = pos; 
}

 // center point of beam
const Vector &CBeam::WorldSpaceCenter( void ) const 
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

struct BeamTrail_t;
class CEngineSprite;

using namespace ::ks::reflect;

extern ConVar r_drawsprites;
extern ConVar r_DrawBeams;

static IMaterial *g_pBeamWireframeMaterial;

//-----------------------------------------------------------------------------
// Purpose: Retrieve sprite object and set it up for rendering
// Input  : *pSpriteModel - 
//			frame - 
//			rendermode - 
// Output : CEngineSprite
//-----------------------------------------------------------------------------
CEngineSprite *Draw_SetSpriteTexture( const model_t *pSpriteModel, int frame, int rendermode )
{
	CEngineSprite			*psprite;
	IMaterial		*material;

	psprite = ( CEngineSprite * )modelinfo->GetModelExtraData( pSpriteModel );
	contract_assert( psprite );

	material = psprite->GetMaterial( (RenderMode_t)rendermode, frame );
	if( !material )
		return nullptr;
	
	CMatRenderContextPtr pRenderContext( materials );
	if ( ShouldDrawInWireFrameMode() || r_DrawBeams.GetInt() == 2 )
	{
		if ( !g_pBeamWireframeMaterial )
			g_pBeamWireframeMaterial = materials->FindMaterial( "debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER );
		pRenderContext->Bind( g_pBeamWireframeMaterial, nullptr );
		return psprite;
	}
	
	pRenderContext->Bind( material );
	return psprite;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pMaterial - 
//			source - 
//			color - 
//-----------------------------------------------------------------------------
void DrawHalo(IMaterial* pMaterial, const Vector& source, float scale, float const* color, float flHDRColorScale )
{
	static unsigned int nHDRColorScaleCache = 0;
	Vector		point, screen;
	
	if( pMaterial )
	{
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	// Transform source into screen space
	ScreenTransform( source, screen );

	meshBuilder.Color3fv (color);
	meshBuilder.TexCoord2f (0, 0, 1);
	VectorMA (source, -scale, CurrentViewUp(), point);
	VectorMA (point, -scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3fv (color);
	meshBuilder.TexCoord2f (0, 0, 0);
	VectorMA (source, scale, CurrentViewUp(), point);
	VectorMA (point, -scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3fv (color);
	meshBuilder.TexCoord2f (0, 1, 0);
	VectorMA (source, scale, CurrentViewUp(), point);
	VectorMA (point, scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3fv (color);
	meshBuilder.TexCoord2f (0, 1, 1);
	VectorMA (source, -scale, CurrentViewUp(), point);
	VectorMA (point, scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Assumes the material has already been bound
//-----------------------------------------------------------------------------
void DrawSprite( const Vector &vecOrigin, float flWidth, float flHeight, color32 color )
{
	unsigned char pColor[4] = { color.r, color.g, color.b, color.a };

	// Generate half-widths
	flWidth *= 0.5f;
	flHeight *= 0.5f;

	// Compute direction vectors for the sprite
	Vector fwd, right( 1, 0, 0 ), up( 0, 1, 0 );
	VectorSubtract( CurrentViewOrigin(), vecOrigin, fwd );
	float flDist = VectorNormalize( fwd );
	if (flDist >= 1e-3)
	{
		CrossProduct( CurrentViewUp(), fwd, right );
		flDist = VectorNormalize( right );
		if (flDist >= 1e-3)
		{
			CrossProduct( fwd, right, up );
		}
		else
		{
			// In this case, fwd == g_vecVUp, it's right above or 
			// below us in screen space
			CrossProduct( fwd, CurrentViewRight(), up );
			VectorNormalize( up );
			CrossProduct( up, fwd, right );
		}
	}

	CMeshBuilder meshBuilder;
	Vector point;
	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 0, 1);
	VectorMA (vecOrigin, -flHeight, up, point);
	VectorMA (point, -flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 0, 0);
	VectorMA (vecOrigin, flHeight, up, point);
	VectorMA (point, -flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 1, 0);
	VectorMA (vecOrigin, flHeight, up, point);
	VectorMA (point, flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv (pColor);
	meshBuilder.TexCoord2f (0, 1, 1);
	VectorMA (vecOrigin, -flHeight, up, point);
	VectorMA (point, flWidth, right, point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Compute vectors perpendicular to the beam
//-----------------------------------------------------------------------------
static void ComputeBeamPerpendicular( const Vector &vecBeamDelta, Vector *pPerp )
{
	// Direction in worldspace of the center of the beam
	Vector vecBeamCenter = vecBeamDelta;
	VectorNormalize( vecBeamCenter );

	CrossProduct( CurrentViewForward(), vecBeamCenter, *pPerp );
	VectorNormalize( *pPerp );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			flags - 
//			*color - 
//			fadescale - 
//-----------------------------------------------------------------------------
void DrawSegs( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				float frame, int rendermode, const Vector& source, const Vector& delta, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale )
{
	int				i, noiseIndex, noiseStep;
	float			div, length, fraction, factor, vLast, vStep, brightness;
	
	contract_assert( fadeLength >= 0.0f );
	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	if ( segments < 2 )
		return;

	IMaterial *pMaterial = pSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		static unsigned int nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}
	
	length = VectorLength( delta );
	float flMaxWidth = std::max(startWidth, endWidth) * 0.5f;
	div = 1.0 / (segments-1);

	if ( length*div < flMaxWidth * 1.414 )
	{
		// Here, we have too many segments; we could get overlap... so lets have less segments
		segments = (int)(length / (flMaxWidth * 1.414)) + 1;
		if ( segments < 2 )
		{
			segments = 2;
		}
	}

	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
	{
		segments = noise_divisions;
	}

	div = 1.0 / (segments-1);
	length *= 0.01;

	// UNDONE: Expose texture length scale factor to control "fuzziness"

	if ( flags & FBEAM_NOTILE )
	{
		// Don't tile
		vStep = div;
	}
	else
	{
		// Texture length texels per space pixel
		vStep = length*div;	
	}
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	vLast = std::fmod(freq*speed,1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)

	if ( flags & FBEAM_SINENOISE )
	{
		if ( segments < 16 )
		{
			segments = 16;
			div = 1.0 / (segments-1);
		}
		scale *= 100;
		length = segments * (1.0/10);
	}
	else
	{
		scale *= length;
	}

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((float)(noise_divisions-1) * div * 65536.0f);
	noiseIndex = 0;
	
	if ( flags & FBEAM_SINENOISE )
	{
		noiseIndex = 0;
	}

	brightness = 1.0;
	if ( flags & FBEAM_SHADEIN )
	{
		brightness = 0;
	}

	// What fraction of beam should be faded
	contract_assert( fadeLength >= 0.0f );
	float fadeFraction = fadeLength/ delta.Length();
	
	// BUGBUG: This code generates NANs when fadeFraction is zero! REVIST!
	fadeFraction = clamp(fadeFraction,1e-6,1);

	// Choose two vectors that are perpendicular to the beam
	Vector perp1;
	ComputeBeamPerpendicular( delta, &perp1 );

	// Specify all the segments.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw segDraw;
	segDraw.Start( pRenderContext, segments, nullptr );

	for ( i = 0; i < segments; i++ )
	{
		contract_assert( noiseIndex < (noise_divisions<<16) );
		BeamSeg_t curSeg;

		fraction = i * div;

		// Fade in our out beam to fadeLength

		if ( (flags & FBEAM_SHADEIN) && (flags & FBEAM_SHADEOUT) )
		{
			if (fraction < 0.5)
			{
				brightness = 2*(fraction/fadeFraction);
			}
			else
			{
				brightness = 2*(1.0 - (fraction/fadeFraction));
			}
		}
		else if ( flags & FBEAM_SHADEIN )
		{
			brightness = fraction/fadeFraction;
		}
		else if ( flags & FBEAM_SHADEOUT )
		{
			brightness = 1.0 - (fraction/fadeFraction);
		}

		// clamps
		if (brightness < 0 )		
		{
			brightness = 0;
		}
		else if (brightness > 1)		
		{
			brightness = 1;
		}

		Vector vecTemp;
		VectorScale( *((Vector*)color), brightness, vecTemp );
		curSeg.SetColor( vecTemp, 1.0f );

		// UNDONE: Make this a spline instead of just a line?
		VectorMA( source, fraction, delta, curSeg.m_vPos );
 
		// Distort using noise
		if ( scale != 0 )
		{
			factor = prgNoise[noiseIndex>>16] * scale;
			if ( flags & FBEAM_SINENOISE )
			{
				float	s, c;
				SinCos( fraction*std::numbers::pi*length + freq, &s, &c );
				VectorMA( curSeg.m_vPos, factor * s, CurrentViewUp(), curSeg.m_vPos );
				// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
				VectorMA( curSeg.m_vPos, factor * c, CurrentViewRight(), curSeg.m_vPos );
			}
			else
			{
				VectorMA( curSeg.m_vPos, factor, perp1, curSeg.m_vPos );
			}
		}

		// Specify the next segment.
		if( endWidth == startWidth )
		{
			curSeg.m_flWidth = startWidth * 2;
		}
		else
		{
			curSeg.m_flWidth = ((fraction*(endWidth-startWidth))+startWidth) * 2;
		}
		
		curSeg.m_flTexCoord = vLast;
		segDraw.NextSeg( &curSeg );

		vLast += vStep;	// Advance texture scroll (v axis only)
		noiseIndex += noiseStep;
	}

	segDraw.End();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CalcSegOrigin( Vector *vecOut, int iPoint, int noise_divisions, float *prgNoise, 
	const Vector &source, const Vector& delta, const Vector &perp, int segments, 
	float freq, float scale, float fraction, int flags )
{
	contract_assert( segments > 1 );

	float factor;
	float length = VectorLength( delta ) * 0.01;
	float div = 1.0 / (segments-1);

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	int noiseStep = (int)((float)(noise_divisions-1) * div * 65536.0f);
	int noiseIndex = (iPoint) * noiseStep;

	// Sine noise beams have different length calculations
	if ( flags & FBEAM_SINENOISE )
	{
		length = segments * (1.0/10);
		noiseIndex = 0;
	}

	// UNDONE: Make this a spline instead of just a line?
	VectorMA( source, fraction, delta, *vecOut );

	// Distort using noise
	if ( scale != 0 )
	{
		factor = prgNoise[noiseIndex>>16] * scale;
		if ( flags & FBEAM_SINENOISE )
		{
			float	s, c;
			SinCos( fraction*std::numbers::pi*length + freq, &s, &c );
			VectorMA( *vecOut, factor * s, CurrentViewUp(), *vecOut );
			// Rotate the noise along the perpendicular axis a bit to keep the bolt from looking diagonal
			VectorMA( *vecOut, factor * c, CurrentViewRight(), *vecOut );
		}
		else
		{
			VectorMA( *vecOut, factor, perp, *vecOut );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			flags - 
//			*color - 
//			fadescale - 
//-----------------------------------------------------------------------------
void DrawTeslaSegs( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				float frame, int rendermode, const Vector&  source, const Vector&  delta, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale )
{
	int				i;
	float			div, length, fraction, vLast, vStep, brightness;
	
	contract_assert( fadeLength >= 0.0f );
	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	if ( segments < 2 )
		return;
	
	IMaterial *pMaterial = pSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		static unsigned int nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
		segments = noise_divisions;

	length = VectorLength( delta ) * 0.01;
	div = 1.0 / (segments-1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length*div;	// Texture length texels per space pixel

	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	vLast = std::fmod(freq*speed,1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)

	brightness = 1.0;
	if ( flags & FBEAM_SHADEIN )
		brightness = 0;

	// What fraction of beam should be faded
	contract_assert( fadeLength >= 0.0f );
	float fadeFraction = fadeLength/ delta.Length();
	
	// BUGBUG: This code generates NANs when fadeFraction is zero! REVIST!
	fadeFraction = clamp(fadeFraction,1e-6,1);

	Vector perp;
	ComputeBeamPerpendicular( delta, &perp );

	// Specify all the segments.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw segDraw;
	segDraw.Start( pRenderContext, segments, nullptr );

	// Keep track of how many times we've branched
	int iBranches = 0;

	Vector vecStart, vecEnd;
	float flWidth = 0;
	float flEndWidth = 0;

	for ( i = 0; i < segments; i++ )
	{
		BeamSeg_t curSeg;

		fraction = i * div;

		// Fade in our out beam to fadeLength

		if ( (flags & FBEAM_SHADEIN) && (flags & FBEAM_SHADEOUT) )
		{
			if (fraction < 0.5)
			{
				brightness = 2*(fraction/fadeFraction);
			}
			else
			{
				brightness = 2*(1.0 - (fraction/fadeFraction));
			}
		}
		else if ( flags & FBEAM_SHADEIN )
		{
			brightness = fraction/fadeFraction;
		}
		else if ( flags & FBEAM_SHADEOUT )
		{
			brightness = 1.0 - (fraction/fadeFraction);
		}

		// clamps
		if (brightness < 0 )		
		{
			brightness = 0;
		}
		else if (brightness > 1)		
		{
			brightness = 1;
		}

		Vector vecTemp;
		VectorScale( *((Vector*)color), brightness, vecTemp );
		curSeg.SetColor( vecTemp, 1.0f );

		CalcSegOrigin( &curSeg.m_vPos, i, noise_divisions, prgNoise, source, delta, perp, segments, freq, scale, fraction, flags );

		// Specify the next segment.
		if( endWidth == startWidth )
			curSeg.m_flWidth = startWidth * 2;
		else
			curSeg.m_flWidth = ((fraction*(endWidth-startWidth))+startWidth) * 2;

		// Reduce the width by the current number of branches we've had
		for ( int j = 0; i < iBranches; j++ )
		{
			curSeg.m_flWidth *= 0.5;
		}
		
		curSeg.m_flTexCoord = vLast;

		segDraw.NextSeg( &curSeg );

		vLast += vStep;	// Advance texture scroll (v axis only)

		// Now see if we'd like to branch here
		// For now, always branch at the midpoint.
		// We could branch randomly, and multiple times per beam
		if ( i == (segments * 0.5) )
		{
			// Figure out what the new width would be
			// Halve the width because the beam is breaking in two, and halve it again because width is doubled above
			flWidth = curSeg.m_flWidth * 0.25;
			if ( flWidth > 1 )
			{
				iBranches++;

				// Get an endpoint for the new branch
				vecStart = curSeg.m_vPos;
				vecEnd = source + delta + (CurrentViewUp() * 32) + (CurrentViewRight() * 32);
				vecEnd -= vecStart;

				// Reduce the end width by the current number of branches we've had
				flEndWidth = endWidth;
				for ( int j = 0; i < iBranches; j++ )
				{
					flEndWidth *= 0.5;
				}
			}
		}
	}

	segDraw.End();

	// If we branched, draw the new beam too
	if ( iBranches )
	{
		DrawTeslaSegs( noise_divisions, prgNoise, spritemodel, frame, rendermode, 
			vecStart, vecEnd, flWidth, flEndWidth, scale, freq, speed, segments,
			flags, color, fadeLength, flHDRColorScale );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*beammodel - 
//			*halomodel - 
//			flHaloScale - 
//			startWidth - 
//			endWidth - 
//			scale - 
//			freq - 
//			speed - 
//			segments - 
//			* - 
//-----------------------------------------------------------------------------

void DrawSplineSegs( int noise_divisions, float *prgNoise, 
				const model_t* beammodel, const model_t* halomodel, float flHaloScale,
				float frame, int rendermode, int numAttachments, Vector* attachment, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale )
{
	int				noiseIndex, noiseStep;
	float			div, length, fraction, factor, vLast, vStep, brightness;
	float			scaledColor[3];

	model_t *beamsprite = ( model_t *)beammodel;
	model_t *halosprite = ( model_t *)halomodel;

	CEngineSprite *pBeamSprite = Draw_SetSpriteTexture( beamsprite, frame, rendermode );
	if ( !pBeamSprite )
		return;

	// Figure out the number of segments.
	if ( segments < 2 )
		return;
	
	IMaterial *pMaterial = pBeamSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		static unsigned int		nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
		segments = noise_divisions;

	if ( flags & FBEAM_SINENOISE )
	{
		if ( segments < 16 )
			segments = 16;
	}
	
	IMaterial *pBeamMaterial = pBeamSprite->GetMaterial( (RenderMode_t)rendermode );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw segDraw;
	segDraw.Start( pRenderContext, (segments-1)*(numAttachments-1), pBeamMaterial );

	CEngineSprite *pHaloSprite = (CEngineSprite *)modelinfo->GetModelExtraData( halosprite );
	IMaterial *pHaloMaterial = nullptr;
	if ( pHaloSprite )
	{
		pHaloMaterial = pHaloSprite->GetMaterial( kRenderGlow );
	}
	
	//-----------------------------------------------------------
	//  Calculate widthStep if start and end width are different
	//-----------------------------------------------------------
	float widthStep;
	if (startWidth != endWidth)
	{
		widthStep		= (endWidth - startWidth)/numAttachments;
	}
	else
	{
		widthStep		= 0;
	}
	
	// Calculate total length of beam 
	float flBeamLength = (attachment[0]-attachment[numAttachments-1]).Length();

	// What fraction of beam should be faded
	float fadeFraction = fadeLength/flBeamLength;
	if (fadeFraction > 1) 
	{
		fadeFraction = 1;
	}
	//---------------------------------------------------------------
	// Go through each attachment drawing spline beams between them
	//---------------------------------------------------------------
	Vector vLastPoint(0,0,0);
	Vector pPre;	// attachment point before the current beam
	Vector pStart;	// start of current beam
	Vector pEnd;	// end of current beam
	Vector pNext;	// attachment point after the current beam

	for (int j=0;j<numAttachments-1;j++)
	{
		if (j==0)
		{
			VectorCopy(attachment[0],pPre);
			VectorCopy(pPre,vLastPoint);
		}
		else
		{
			VectorCopy(attachment[j-1],pPre);
		}

		VectorCopy(attachment[j],	pStart);
		VectorCopy(attachment[j+1],	pEnd);

		if (j+2 >= numAttachments-1)
		{
			VectorCopy(attachment[j+1],pNext);
		}
		else
		{
			VectorCopy(attachment[j+2],pNext);
		}

		Vector vDelta;
		VectorSubtract(pEnd,pStart,vDelta);
		length = VectorLength( vDelta ) * 0.01;
		if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
			length = 0.5;
		div = 1.0 / (segments-1);

		// UNDONE: Expose texture length scale factor to control "fuzziness"
		vStep = length*div;	// Texture length texels per space pixel

		// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
		vLast = std::fmod(freq*speed,1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)

		if ( flags & FBEAM_SINENOISE )
		{
			scale = scale * 100;
			length = segments * (1.0/10);
		}
		else
			scale = scale * length;
		
		// -----------------------------------------------------------------------------
		// Iterator to resample noise waveform (it needs to be generated in powers of 2)
		// -----------------------------------------------------------------------------
		noiseStep = (int)((float)(noise_divisions-1) * div * 65536.0f);
		noiseIndex = noiseStep;
		
		if ( flags & FBEAM_SINENOISE )
			noiseIndex = 0;

		brightness = 1.0;
		if ( flags & FBEAM_SHADEIN )
			brightness = 0;

		BeamSeg_t seg;

		VectorScale( color, brightness, scaledColor );
		seg.SetColor( scaledColor[0], scaledColor[1], scaledColor[2], 1.0f );
		
		// -------------------------------------------------
		//  Calc start and end widths for this segment
		// -------------------------------------------------
		float startSegWidth = startWidth + (widthStep*j);
		float endSegWidth	= startWidth + (widthStep*(j+1));

		// -------------------------------------------------
		//  Now draw each segment
		// -------------------------------------------------
		float	fBestFraction	= -1;
		float	bestDot			= 0;
		for (int i = 1; i < segments; i++ )
		{
			fraction = i * div;

			// Fade in our out beam to fadeLength
			// BUG BUG: should be based on total lengh of beam not this particular fraction
			if ( flags & FBEAM_SHADEIN )
			{
				brightness = fraction/fadeFraction;
				if (brightness > 1)
				{
					brightness = 1;
				}
			}
			else if ( flags & FBEAM_SHADEOUT )
			{
				float fadeFraction = fadeLength/length;
				brightness = 1.0 - (fraction/fadeFraction);
				if (brightness < 0)
				{
					brightness = 0;
				}
			}

			// -----------------------------------------------------------
			//  Calculate spline position
			// -----------------------------------------------------------
			Vector vTarget(0,0,0);
			
			Catmull_Rom_Spline(pPre, pStart, pEnd, pNext, fraction, vTarget );
			
			seg.m_vPos[0] = vTarget.x;
			seg.m_vPos[1] = vTarget.y;
			seg.m_vPos[2] = vTarget.z;

			// --------------------------------------------------------------
			//  Keep track of segment most facing the player for halo effect
			// --------------------------------------------------------------
			if (pHaloMaterial)
			{
				Vector vBeamDir1;
				VectorSubtract(seg.m_vPos,vLastPoint,vBeamDir1);
				VectorNormalize(vBeamDir1);

				Vector vLookDir;
				VectorSubtract(CurrentViewOrigin(),seg.m_vPos,vLookDir);
				VectorNormalize(vLookDir);

				float	dotpr		= fabs(DotProduct(vBeamDir1,vLookDir));
				static float thresh = 0.85;
				if (dotpr > thresh && dotpr > bestDot)
				{
					bestDot		  = dotpr;
					fBestFraction = fraction;
				}	
				VectorCopy(seg.m_vPos,vLastPoint);
			}

			// ----------------------
			// Distort using noise
			// ----------------------
			if ( scale != 0 )
			{
				factor = prgNoise[noiseIndex>>16] * scale;
				if ( flags & FBEAM_SINENOISE )
				{
					float	s, c;
					SinCos( fraction*std::numbers::pi*length + freq, &s, &c );
					VectorMA( seg.m_vPos, factor * s, CurrentViewUp(), seg.m_vPos );
					// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
					VectorMA( seg.m_vPos, factor * c, CurrentViewRight(), seg.m_vPos );
				}
				else
				{
					VectorMA( seg.m_vPos, factor, CurrentViewUp(), seg.m_vPos );
					// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
					factor = prgNoise[noiseIndex>>16] * scale * std::cos(fraction*std::numbers::pi*3+freq);
					VectorMA( seg.m_vPos, factor, CurrentViewRight(), seg.m_vPos );
				}
			}

			// Scale width if non-zero spread
			if (startWidth != endWidth)
				seg.m_flWidth = ((fraction*(endSegWidth-startSegWidth))+startSegWidth)*2;
			else
				seg.m_flWidth = startWidth*2;

			seg.m_flTexCoord = vLast;
			segDraw.NextSeg( &seg );

			vLast += vStep;	// Advance texture scroll (v axis only)
			noiseIndex += noiseStep;
		}

		// --------------------------------------------------------------
		//  Draw halo on segment most facing the player 
		// --------------------------------------------------------------
	}
	
	segDraw.End();

	// ------------------------
	// Draw halo at end of beam
	// ------------------------
	if (pHaloMaterial)
	{
		pRenderContext->Bind(pHaloMaterial);
		DrawHalo(pHaloMaterial,pEnd,flHaloScale,scaledColor, flHDRColorScale);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			scale - 
//			*color - 
//-----------------------------------------------------------------------------
void BeamDrawHalo( const model_t* spritemodel, float frame, int rendermode, 
				  const Vector& source, float scale, float* color, float flHDRColorScale )
{
	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	DrawHalo( pSprite->GetMaterial( (RenderMode_t)rendermode ), source, scale, color, flHDRColorScale );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			width - 
//			scale - 
//			freq - 
//			speed - 
//			segments - 
//			*color - 
//-----------------------------------------------------------------------------
void DrawDisk( int noise_divisions, float *prgNoise, const model_t* spritemodel, 
			  float frame, int rendermode, const Vector&  source, const Vector& delta, 
			  float width, float scale, float freq, float speed, int segments, float* color, float flHDRColorScale )
{
	int				i;
	float			div, length, fraction, vLast, vStep;
	Vector			point;
	float			w;
	static unsigned int		nHDRColorScaleCache = 0;

	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	if ( segments < 2 )
		return;
	
	IMaterial *pMaterial = pSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
		segments = noise_divisions;

	length = VectorLength( delta ) * 0.01;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / (segments-1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length*div;	// Texture length texels per space pixel
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	vLast = std::fmod(freq*speed,1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = scale * length;

	w = freq * delta[2];

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, (segments - 1) * 2 );

	// NOTE: We must force the degenerate triangles to be on the edge
	for ( i = 0; i < segments; i++ )
	{
		float	s, c;
		fraction = i * div;

		point[0] = source[0];
		point[1] = source[1];
		point[2] = source[2];

		meshBuilder.Color3fv( color );
		meshBuilder.TexCoord2f( 0, 1.0, vLast );
		meshBuilder.Position3fv( point.Base() );
		meshBuilder.AdvanceVertex();

		SinCos( fraction * 2 * std::numbers::pi, &s, &c );
		point[0] = s * w + source[0];
		point[1] = c * w + source[1];
		point[2] = source[2];

		meshBuilder.Color3fv( color );
		meshBuilder.TexCoord2f( 0, 0.0, vLast );
		meshBuilder.Position3fv( point.Base() );
		meshBuilder.AdvanceVertex();

		vLast += vStep;	// Advance texture scroll (v axis only)
	}

	meshBuilder.End( );
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			width - 
//			scale - 
//			freq - 
//			speed - 
//			segments - 
//			*color - 
//-----------------------------------------------------------------------------
void DrawCylinder( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				  float frame, int rendermode, const Vector&  source, const Vector& delta, 
				  float width, float scale, float freq, float speed, int segments, 
				  float* color, float flHDRColorScale )
{
	int				i;
	float			div, length, fraction, vLast, vStep;
	Vector			point;

	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	if ( segments < 2 )
		return;
	
	IMaterial *pMaterial = pSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		static unsigned int		nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
		segments = noise_divisions;

	length = VectorLength( delta ) * 0.01;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / (segments-1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length*div;	// Texture length texels per space pixel
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	vLast = std::fmod(freq*speed,1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = scale * length;
	
	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, (segments - 1) * 2 );

	float radius = delta[2];
	for ( i = 0; i < segments; i++ )
	{
		float	s, c;
		fraction = i * div;
		SinCos( fraction * 2 * std::numbers::pi, &s, &c );

		point[0] = s * freq * radius + source[0];
		point[1] = c * freq * radius + source[1];
		point[2] = source[2] + width;

		meshBuilder.Color3f( 0.0f, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, vLast );
		meshBuilder.Position3fv( point.Base() );
		meshBuilder.AdvanceVertex();

		point[0] = s * freq * (radius + width) + source[0];
		point[1] = c * freq * (radius + width) + source[1];
		point[2] = source[2] - width;

		meshBuilder.Color3fv( color );
		meshBuilder.TexCoord2f( 0, 0.0f, vLast );
		meshBuilder.Position3fv( point.Base() );
		meshBuilder.AdvanceVertex();

		vLast += vStep;	// Advance texture scroll (v axis only)
	}
	
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			(*pfnNoise - 
//-----------------------------------------------------------------------------
void DrawRing( int noise_divisions, float *prgNoise, void (*pfnNoise)( float *noise, int divs, float scale ), 
			  const model_t* spritemodel, float frame, int rendermode,
			  const Vector& source, const Vector& delta, float width, 
			  float amplitude, float freq, float speed, int segments, float *color, float flHDRColorScale )
{
	int				i, j, noiseIndex, noiseStep;
	float			div, length, fraction, factor, vLast, vStep;
	Vector			last1, last2, point, screen, screenLast(0,0,0), tmp, normal;
	Vector			center, xaxis, yaxis, zaxis;
	float			radius, x, y, scale;
	Vector			d;

	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	IMaterial *pMaterial = pSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		static unsigned int		nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	VectorCopy( delta, d );

	if ( segments < 2 )
		return;

	segments = segments * std::numbers::pi;
	
	if ( segments > noise_divisions * 8 )		// UNDONE: Allow more segments?
		segments = noise_divisions * 8;

	length = VectorLength( d ) * 0.01 * std::numbers::pi;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / (segments-1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length*div/8.0;	// Texture length texels per space pixel
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	vLast = std::fmod(freq*speed,1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = amplitude * length / 8.0;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((noise_divisions-1) * div * 65536.0) * 8;
	noiseIndex = 0;

	VectorScale( d, 0.5, d );
	VectorAdd( source, d, center );
	zaxis[0] = 0; zaxis[1] = 0; zaxis[2] = 1;

	VectorCopy( d, xaxis );
	radius = VectorLength( xaxis );
	
	// cull beamring
	// --------------------------------
	// Compute box center +/- radius
	last1[0] = radius;
	last1[1] = radius;
	last1[2] = scale;
	VectorAdd( center, last1, tmp );	// maxs
	VectorSubtract( center, last1, screen ); // mins

	// Is that box in PVS && frustum?
	if ( !engine->IsBoxVisible( screen, tmp ) || engine->CullBox( screen, tmp ) )	
	{
		return;
	}

	yaxis[0] = xaxis[1]; yaxis[1] = -xaxis[0]; yaxis[2] = 0;
	VectorNormalize( yaxis );
	VectorScale( yaxis, radius, yaxis );

	j = segments / 8;

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, (segments) * 2 );

	for ( i = 0; i < segments + 1; i++ )
	{
		fraction = i * div;
		SinCos( fraction * 2 * std::numbers::pi, &x, &y );

		point[0] = xaxis[0] * x + yaxis[0] * y + center[0];
		point[1] = xaxis[1] * x + yaxis[1] * y + center[1];
		point[2] = xaxis[2] * x + yaxis[2] * y + center[2];

		// Distort using noise
		if ( scale != 0.0f )
		{
			factor = prgNoise[(noiseIndex>>16) & 0x7F] * scale;
			VectorMA( point, factor, CurrentViewUp(), point );

			// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
			factor = prgNoise[(noiseIndex>>16) & 0x7F] * scale * std::cos(fraction*std::numbers::pi*3*8+freq);
			VectorMA( point, factor, CurrentViewRight(), point );
		}
		
		// Transform point into screen space
		ScreenTransform( point, screen );

		if (i != 0)
		{
			// Build world-space normal to screen-space direction vector
			VectorSubtract( screen, screenLast, tmp );
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( CurrentViewUp(), tmp[0], normal );	// Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], CurrentViewRight(), normal );
			
			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep;	// Advance texture scroll (v axis only)
			meshBuilder.Color3fv( color );
			meshBuilder.TexCoord2f( 0, 1.0f, vLast );
			meshBuilder.Position3fv( last2.Base() );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color3fv( color );
			meshBuilder.TexCoord2f( 0, 0.0f, vLast );
			meshBuilder.Position3fv( last1.Base() );
			meshBuilder.AdvanceVertex();
		}
		VectorCopy( screen, screenLast );
		noiseIndex += noiseStep;

		j--;
		if (j == 0 && amplitude != 0 )
		{
			j = segments / 8;
			(*pfnNoise)( prgNoise, noise_divisions, 1.0f );
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : spritemodel - 
//			*pHead - 
//			delta - 
//			*screen - 
//			*screenLast - 
//			die - 
//			source - 
//			flags - 
//			width - 
//			amplitude - 
//			freq - 
//			*color - 
//-----------------------------------------------------------------------------
void DrawBeamFollow( const model_t* spritemodel, BeamTrail_t* pHead, int frame, int rendermode, 
					Vector& delta, Vector& screen, Vector& screenLast, float die,
					const Vector& source, int flags, float width, float amplitude, 
					float freq, float* color, float flHDRColorScale )
{
	float			fraction;
	float			div;
	float			vLast = 0.0;
	float			vStep = 1.0;
	Vector			last1, last2, tmp, normal;
	float			scaledColor[3];

	CEngineSprite *pSprite = Draw_SetSpriteTexture( spritemodel, frame, rendermode );
	if ( !pSprite )
		return;

	IMaterial *pMaterial = pSprite->GetMaterial( (RenderMode_t)rendermode );
	if( pMaterial )
	{
		static unsigned int		nHDRColorScaleCache = 0;
		IMaterialVar *pHDRColorScaleVar = pMaterial->FindVarFast( "$hdrcolorscale", &nHDRColorScaleCache );
		if( pHDRColorScaleVar )
		{
			pHDRColorScaleVar->SetFloatValue( flHDRColorScale );
		}
	}

	// UNDONE: This won't work, screen and screenLast must be extrapolated here to fix the
	// first beam segment for this trail

	// Build world-space normal to screen-space direction vector
	VectorSubtract( screen, screenLast, tmp );
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( CurrentViewUp(), tmp[0], normal );	// Build point along noraml line (normal is -y, x)
	VectorMA( normal, -tmp[1], CurrentViewRight(), normal );
	
	// Make a wide line
	VectorMA( delta, width, normal, last1 );
	VectorMA( delta, -width, normal, last2 );

	div = 1.0 / amplitude;
	fraction = ( die - gpGlobals->curtime ) * div;
	unsigned char nColor[3];

	VectorScale( color, fraction, scaledColor );
	nColor[0] = (unsigned char)clamp( (int)(scaledColor[0] * 255.0f), 0, 255 );
	nColor[1] = (unsigned char)clamp( (int)(scaledColor[1] * 255.0f), 0, 255 );
	nColor[2] = (unsigned char)clamp( (int)(scaledColor[2] * 255.0f), 0, 255 );
	
	// need to count the segments
	int count = 0;
	BeamTrail_t* pTraverse = pHead;
	while ( pTraverse )
	{
		++count;
		pTraverse = pTraverse->next;
	}

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, count );

	while (pHead)
	{
		// Msg("%.2f ", fraction );
		meshBuilder.Position3fv( last1.Base() );
		meshBuilder.Color3ubv( nColor );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( last2.Base() );
		meshBuilder.Color3ubv( nColor );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		// Transform point into screen space
		ScreenTransform( pHead->org, screen );
		// Build world-space normal to screen-space direction vector
		VectorSubtract( screen, screenLast, tmp );
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( CurrentViewUp(), tmp[0], normal );	// Build point along noraml line (normal is -y, x)
		VectorMA( normal, -tmp[1], CurrentViewRight(), normal );
		
		// Make a wide line
		VectorMA( pHead->org, width, normal, last1 );
		VectorMA( pHead->org, -width, normal, last2 );

		vLast += vStep;	// Advance texture scroll (v axis only)

		if (pHead->next != nullptr)
		{
			fraction = (pHead->die - gpGlobals->curtime) * div;
			VectorScale( color, fraction, scaledColor );
			nColor[0] = (unsigned char)clamp( (int)(scaledColor[0] * 255.0f), 0, 255 );
			nColor[1] = (unsigned char)clamp( (int)(scaledColor[1] * 255.0f), 0, 255 );
			nColor[2] = (unsigned char)clamp( (int)(scaledColor[2] * 255.0f), 0, 255 );
		}
		else
		{
			fraction = 0.0;
			nColor[0] = nColor[1] = nColor[2] = 0;
		}

		meshBuilder.Position3fv( last2.Base() );
		meshBuilder.Color3ubv( nColor );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( last1.Base() );
		meshBuilder.Color3ubv( nColor );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.AdvanceVertex();

		VectorCopy( screen, screenLast );

		pHead = pHead->next;
	}

	meshBuilder.End();
	pMesh->Draw();
}

/*
P0 = start
P1 = control
P2 = end
P(t) = (1-t)^2 * P0 + 2t(1-t)*P1 + t^2 * P2
*/
void DrawBeamQuadratic( const Vector &start, const Vector &control, const Vector &end, float width, const Vector &color, float scrollOffset, float flHDRColorScale )
{
	int subdivisions = 16;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw beamDraw;
	beamDraw.Start( pRenderContext, subdivisions+1, nullptr );

	BeamSeg_t seg;
	seg.m_flWidth = width;
	
	float t = 0;
	float u = std::fmod( scrollOffset, 1 );
	float dt = 1.0 / (float)subdivisions;
	for( int i = 0; i <= subdivisions; i++, t += dt )
	{
		float omt = (1-t);
		float p0 = omt*omt;
		float p1 = 2*t*omt;
		float p2 = t*t;

		seg.m_vPos = p0 * start + p1 * control + p2 * end;
		seg.m_flTexCoord = u - t;
		if ( i == 0 || i == subdivisions )
		{
			// HACK: fade out the ends a bit
			seg.m_color.r = seg.m_color.g = seg.m_color.b = 0;
			seg.m_color.a = 255;
		}
		else
		{
			seg.SetColor( color, 1.0f );
		}
		beamDraw.NextSeg( &seg );
	}

	beamDraw.End();
}

class CBaseEntity;
class CBeam;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSpotlightTraceCacheEntry
{
public:
	CSpotlightTraceCacheEntry()
	{
		m_origin.Init();
		m_radius = -1.0f;
	}
	bool IsValidFor( const Vector &origin )
	{
		if ( m_radius > 0 && m_origin.DistToSqr(origin) < 1.0f )
			return true;
		return false;
	}
	void Cache( const Vector &origin, const trace_t &tr )
	{
		m_radius = (tr.endpos - origin).Length();
		m_origin = origin;
	}

	Vector	m_origin;
	float	m_radius;
};

static const int NUM_CACHE_ENTRIES = 64;

class CBeamSpotLight : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BeamSpotlight" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBeamSpotLight, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CBeamSpotLight();
	~CBeamSpotLight();

	bool ShouldDraw();
	void ClientThink( void );
	void OnDataChanged( DataUpdateType_t updateType );
	void Release( void );

private:

	Vector SpotlightCurrentPos(void);
	void SpotlightCreate(void);
	void SpotlightDestroy(void);

	// Computes render info for a spotlight
	void ComputeRenderInfo();

private:

	[[= ks::reflect::Net{} ]] int		m_nHaloIndex;
	[[= ks::reflect::Net{} ]] int		m_nRotationAxis;
	[[= ks::reflect::Net{} ]] float	m_flRotationSpeed;
	
	[[= ks::reflect::Net{} ]] bool m_bSpotlightOn;
	[[= ks::reflect::Net{} ]] bool m_bHasDynamicLight;

	[[= ks::reflect::Net{} ]] float m_flSpotlightMaxLength;
	[[= ks::reflect::Net{} ]] float m_flSpotlightGoalWidth;
	[[= ks::reflect::Net{} ]] float m_flHDRColorScale;

	Vector	m_vSpotlightTargetPos;
	Vector	m_vSpotlightCurrentPos;
	Vector	m_vSpotlightDir;

	CHandle<CBeam>	m_hSpotlight;
	
	float	m_flSpotlightCurLength;

	float	m_flLightScale;

	dlight_t*	m_pDynamicLight;

	float m_lastTime;
	CSpotlightTraceCacheEntry *m_pCache;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CBeamSpotLight, DT_BeamSpotlight, CBeamSpotlight )

LINK_ENTITY_TO_CLASS( beam_spotlight, CBeamSpotLight );

//-----------------------------------------------------------------------------
CBeamSpotLight::CBeamSpotLight()
: m_vSpotlightTargetPos( vec3_origin )
, m_vSpotlightCurrentPos( vec3_origin )
, m_vSpotlightDir( vec3_origin )
, m_flSpotlightCurLength( 0.0f )
, m_flLightScale( 100.0f )
, m_pDynamicLight( nullptr )
, m_lastTime( 0.0f )
, m_pCache(nullptr)
{
}

CBeamSpotLight::~CBeamSpotLight()
{
	delete[] m_pCache;
}

//-----------------------------------------------------------------------------
bool CBeamSpotLight::ShouldDraw()
{
	return false;
}

//-----------------------------------------------------------------------------
void CBeamSpotLight::ClientThink( void )
{
 	float dt = gpGlobals->curtime - m_lastTime;
 	if ( !m_lastTime )
 	{
 		dt = 0.0f;
 	}
 	m_lastTime = gpGlobals->curtime;

	// ---------------------------------------------------
	//  If I don't have a spotlight attempt to create one
	// ---------------------------------------------------
	if ( !m_hSpotlight )
	{
		if ( m_bSpotlightOn )
		{
			// Make the spotlight
			SpotlightCreate();
		}
		else
		{
			SetNextClientThink( CLIENT_THINK_NEVER );
			return;
		}
	}
	else if ( !m_bSpotlightOn )
	{
		SpotlightDestroy();
		SetNextClientThink( CLIENT_THINK_NEVER );
		return;
	}

	// update rotation
	if ( m_flRotationSpeed != 0.0f )
	{
		QAngle angles = GetAbsAngles();
		angles[m_nRotationAxis] += m_flRotationSpeed * dt;
		angles[m_nRotationAxis] = anglemod(angles[m_nRotationAxis]);
		if ( !m_pCache )
		{
			m_pCache = new CSpotlightTraceCacheEntry[NUM_CACHE_ENTRIES];
		}

		SetAbsAngles( angles );
	}
	m_vSpotlightCurrentPos = SpotlightCurrentPos();

	contract_assert( m_hSpotlight );

	m_hSpotlight->SetStartPos( GetAbsOrigin() );
	m_hSpotlight->SetEndPos( m_vSpotlightCurrentPos );

	// Avoid sudden change in where beam fades out when cross disconinuities
	Vector dir = m_vSpotlightCurrentPos - GetAbsOrigin();
	float flBeamLength	= VectorNormalize( dir );
	m_flSpotlightCurLength = (0.60*m_flSpotlightCurLength) + (0.4*flBeamLength);

	ComputeRenderInfo();

	m_hSpotlight->RelinkBeam();

	//NDebugOverlay::Cross3D(GetAbsOrigin(),Vector(-5,-5,-5),Vector(5,5,5),0,255,0,true,0.1);
	//NDebugOverlay::Cross3D(m_vSpotlightCurrentPos,Vector(-5,-5,-5),Vector(5,5,5),0,255,0,true,0.1);
	//NDebugOverlay::Cross3D(m_vSpotlightTargetPos,Vector(-5,-5,-5),Vector(5,5,5),255,0,0,true,0.1);

	// Do we need to keep updating?
	if ( !GetMoveParent() && m_flRotationSpeed == 0 )
	{
		// No reason to think again, we're not going to move unless there's a data change
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotLight::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flSpotlightCurLength = m_flSpotlightMaxLength;
	}

	// On a data change always think again
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//------------------------------------------------------------------------------
void CBeamSpotLight::Release()
{
	SpotlightDestroy();
	BaseClass::Release();
}

//------------------------------------------------------------------------------
void CBeamSpotLight::SpotlightCreate(void)
{
	m_vSpotlightTargetPos = SpotlightCurrentPos();

	{
		//CBeam *beam = CBeam::BeamCreate( "sprites/spotlight.vmt", m_flSpotlightGoalWidth );
		CBeam *beam = CBeam::BeamCreate( "sprites/glow_test02.vmt", m_flSpotlightGoalWidth );
		// Beam only exists client side
		ClientEntityList().AddNonNetworkableEntity( beam );
		m_hSpotlight = beam;
	}

	// Set the temporary spawnflag on the beam so it doesn't save (we'll recreate it on restore)
	m_hSpotlight->SetHDRColorScale( m_flHDRColorScale );
	const color24 c = GetRenderColor();
	m_hSpotlight->SetColor( c.r, c.g, c.b ); 
	m_hSpotlight->SetHaloTexture(m_nHaloIndex);
	m_hSpotlight->SetHaloScale(60);
	m_hSpotlight->SetEndWidth(m_flSpotlightGoalWidth);
	m_hSpotlight->SetBeamFlags( (FBEAM_SHADEOUT|FBEAM_NOTILE) );
	m_hSpotlight->SetBrightness( 64 );
	m_hSpotlight->SetNoise( 0 );

	m_hSpotlight->PointsInit( GetAbsOrigin(), m_vSpotlightTargetPos );
}

//------------------------------------------------------------------------------
void CBeamSpotLight::SpotlightDestroy(void)
{
	if ( m_hSpotlight )
	{
		UTIL_Remove( m_hSpotlight );
		m_hSpotlight.Term();
	}
}

//------------------------------------------------------------------------------
Vector CBeamSpotLight::SpotlightCurrentPos(void)
{
	QAngle angles = GetAbsAngles();
	GetVectors( &m_vSpotlightDir, nullptr, nullptr );
	Vector position = GetAbsOrigin();
	int cacheIndex = -1;
	if ( m_pCache )
	{
		cacheIndex = int( angles[m_nRotationAxis] * float(NUM_CACHE_ENTRIES) * (1.0f / 360.0f)) & (NUM_CACHE_ENTRIES - 1);
		if ( m_pCache[cacheIndex].IsValidFor(GetAbsOrigin()) )
		{
			return position + m_vSpotlightDir * m_pCache[cacheIndex].m_radius;
		}
	}

	//	Get beam end point.  Only collide with solid objects, not npcs
	trace_t tr;
	UTIL_TraceLine( position, position + (m_vSpotlightDir * 2 * m_flSpotlightMaxLength), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	if ( cacheIndex >= 0 )
	{
		m_pCache[cacheIndex].Cache(position, tr);
	}

	return tr.endpos;
}

//-----------------------------------------------------------------------------
// Computes render info for a spotlight
//-----------------------------------------------------------------------------
void CBeamSpotLight::ComputeRenderInfo()
{
	// Fade out spotlight end if past max length.  
	if ( m_flSpotlightCurLength > 2*m_flSpotlightMaxLength )
	{
		SetRenderAlpha( 0 );
		m_hSpotlight->SetFadeLength( m_flSpotlightMaxLength );
	}
	else if ( m_flSpotlightCurLength > m_flSpotlightMaxLength )		
	{
		SetRenderAlpha( (1-((m_flSpotlightCurLength-m_flSpotlightMaxLength)/m_flSpotlightMaxLength)) );
		m_hSpotlight->SetFadeLength( m_flSpotlightMaxLength );
	}
	else
	{
		SetRenderAlpha( 1.0 );
		m_hSpotlight->SetFadeLength( m_flSpotlightCurLength );
	}

	// Adjust end width to keep beam width constant
	float flNewWidth = m_flSpotlightGoalWidth * (m_flSpotlightCurLength / m_flSpotlightMaxLength);
	flNewWidth = clamp(flNewWidth, 0, MAX_BEAM_WIDTH );
	m_hSpotlight->SetEndWidth(flNewWidth);

	if ( m_bHasDynamicLight )
	{
		// <<TODO>> - magic number 1.8 depends on sprite size
		m_flLightScale = 1.8*flNewWidth;

		if ( m_flLightScale > 0 ) 
		{
			const color24 c = GetRenderColor();
			float a = GetRenderAlpha() / 255.0f;
			ColorRGBExp32 color;
			color.r	= c.r * a;
			color.g	= c.g * a;
			color.b	= c.b * a;
			color.exponent = 0;
			if ( color.r == 0 && color.g == 0 && color.b == 0 )
				return;
		
			// Deal with the environment light
			if ( !m_pDynamicLight || (m_pDynamicLight->key != index) )
			{
				m_pDynamicLight = effects->CL_AllocDlight( index );
				contract_assert( m_pDynamicLight );
			}
		
			//m_pDynamicLight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
			m_pDynamicLight->radius		= m_flLightScale*3.0f;
			m_pDynamicLight->origin		= GetAbsOrigin() + Vector(0,0,5);
			m_pDynamicLight->die		= gpGlobals->curtime + 0.05f;
			m_pDynamicLight->color		= color;
		}
	}
}

using namespace ::ks::reflect;

extern IBlackBox *blackboxrecorder;

void BlackBox_Record( const char *type, const char *pFormat, ... )
{
	static ConVarRef blackbox( "blackbox" );

	if ( !blackbox.IsValid() || !blackbox.GetBool() )
		 return;

	int type_num;
	for ( type_num = 0; type_num < blackboxrecorder->GetTypeCount(); type_num++ )
	{
		if ( !V_strcasecmp( blackboxrecorder->GetTypeName( type_num ), type ) )
			break;
	}

	if ( type_num >= blackboxrecorder->GetTypeCount() )
	{
		Msg( "Invalid blackbox type: %s\n", type );
		return;
	}

	char szMessage[1024];	
	va_list marker;

	__builtin_va_start( marker, pFormat);
	V_vsnprintf( szMessage, sizeof( szMessage ), pFormat, marker);
	__builtin_va_end( marker );	

	//Msg( "Record: %s: %s\n", type, szMessage );
	blackboxrecorder->Record( type_num, szMessage );
}

class CBoneMergeCache;
class CBaseAnimating;

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

void CBoneMergeCache::MergeMatchingBones( int boneMask )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || std::ssize( m_MergedBones ) == 0 )
		return;

	// Have the entity we're following setup its bones.

	int nTempMask = m_nFollowBoneSetupMask;
	if ( m_pFollow->IsPlayer() )
	{
		// if the parent is a player, respect the incoming bone mask plus attachments,
		nTempMask = ( boneMask | BONE_USED_BY_ATTACHMENT );
		
		// but only use the custom blending rule portion
		if ( m_pFollow->m_nCustomBlendingRuleMask != -1 )
			nTempMask &= m_pFollow->m_nCustomBlendingRuleMask;
	}
	
	m_pFollow->SetupBones( nullptr, -1, nTempMask, gpGlobals->curtime );

	// Now copy the bone matrices.
	for ( int i=0; i < std::ssize( m_MergedBones ); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		MatrixCopy( m_pFollow->GetBone( iParentBone ), m_pOwner->GetBoneForWrite( iOwnerBone ) );
	}
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
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	m_pFollow->SetupBones( nullptr, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );
	const matrix3x4_t &mFollowBone = m_pFollow->GetBone( m_MergedBones[0].m_iParentBone );

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
	m_pFollow->SetupBones( nullptr, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );
	rootBone = m_pFollow->GetBone( m_MergedBones[0].m_iParentBone );

	return true;
}

int CBoneMergeCache::IsBoneMerged( int iBone ) const
{
	if ( m_pOwnerHdr )
		return m_BoneMergeBits[iBone];
	else
		return 0;
}

class CBasePlayer;

using namespace ::ks::reflect;

void RecvProxy_UnmodifiedQAngles( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	const float *v = pData->m_Value.m_Vector;

	((float*)pOut)[0] = v[0];
	((float*)pOut)[1] = v[1];
	((float*)pOut)[2] = v[2];
}

IMPLEMENT_REFLECT_CLIENTCLASS( CBreakableProp, DT_BreakableProp, CBreakableProp )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBreakableProp::CBreakableProp( void )
{
	m_takedamage = DAMAGE_YES;
}

//-----------------------------------------------------------------------------
// Copy fade from another breakable prop
//-----------------------------------------------------------------------------
void CBreakableProp::CopyFadeFrom( CBreakableProp *pSource )
{
	SetGlobalFadeScale( pSource->GetGlobalFadeScale() );
	SetDistanceFade( pSource->GetMinFadeDist(), pSource->GetMaxFadeDist() );
}

void CBreakableProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	if ( m_bClientPhysics )
	{
		bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
		VPhysicsShadowDataChanged(bCreate, this);
	}
}

//IPlayerPickupVPhysics
bool CBreakableProp::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	return (m_qPreferredPlayerCarryAngles.x < std::numeric_limits<float>::max());
}

QAngle CBreakableProp::PreferredCarryAngles( void )
{
	return (m_qPreferredPlayerCarryAngles.x < std::numeric_limits<float>::max()) ? m_qPreferredPlayerCarryAngles : vec3_angle;
}

bool CBreakableProp::ShouldPredict( void )
{
	return false;
}

CBasePlayer *CBreakableProp::GetPredictionOwner( void )
{
	return nullptr;
}

class CBasePlayer;

using namespace ::ks::reflect;

static Vector CAM_HULL_MIN(-CAM_HULL_OFFSET,-CAM_HULL_OFFSET,-CAM_HULL_OFFSET);
static Vector CAM_HULL_MAX( CAM_HULL_OFFSET, CAM_HULL_OFFSET, CAM_HULL_OFFSET);

extern const ConVar *sv_cheats;

extern ConVar cam_idealdist;
extern ConVar cam_idealdistright;
extern ConVar cam_idealdistup;

void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);

void ToggleThirdPerson( bool bValue )
{
	if ( bValue == true )
	{
		CAM_ToThirdPerson();
	}
	else
	{
		CAM_ToFirstPerson();
	}
}

void ThirdPersonChange( ConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );

	ToggleThirdPerson( var.GetBool() );
}

ConVar cl_thirdperson( side_registry(), "cl_thirdperson", "0", FCVAR_NOT_CONNECTED | FCVAR_ARCHIVE | FCVAR_DEVELOPMENTONLY, "Enables/Disables third person", ThirdPersonChange  );

CThirdPersonManager::CThirdPersonManager( void )
{
}

void CThirdPersonManager::Init( void )
{
	m_bOverrideThirdPerson = false;
	m_bForced = false;
	m_flUpFraction = 0.0f;
	m_flFraction = 1.0f;

	m_flUpLerpTime = 0.0f;
	m_flLerpTime = 0.0f;

	m_flUpOffset = CAMERA_UP_OFFSET;

	if ( input )
	{
		input->CAM_SetCameraThirdData( nullptr, vec3_angle );
	}
}

void CThirdPersonManager::Update( void )
{

	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	bool bIsThirdPerson = input->CAM_IsThirdPerson() != 0;

	// If cheats have been disabled, pull us back out of third-person view.
	if ( sv_cheats && !sv_cheats->GetBool() && GameRules() && GameRules()->AllowThirdPersonCamera() == false )
	{
		if ( bIsThirdPerson )
		{
			input->CAM_ToFirstPerson();
		}
		return;
	}

	if ( IsOverridingThirdPerson() == false )
	{
		
		if ( bIsThirdPerson != ( cl_thirdperson.GetBool() || m_bForced ) && GameRules() && GameRules()->AllowThirdPersonCamera() == true )
		{
			ToggleThirdPerson( m_bForced || cl_thirdperson.GetBool() );
		}
	}

}

Vector CThirdPersonManager::GetDesiredCameraOffset( void )
{ 
	if ( IsOverridingThirdPerson() == true )
	{
		return Vector( cam_idealdist.GetFloat(), cam_idealdistright.GetFloat(), cam_idealdistup.GetFloat() );
	}

	return m_vecDesiredCameraOffset; 
}

Vector CThirdPersonManager::GetFinalCameraOffset( void )
{
	Vector vDesired = GetDesiredCameraOffset();

	if ( m_flUpFraction != 1.0f )
	{
		vDesired.z += m_flUpOffset;
	}

	return vDesired;

}

Vector CThirdPersonManager::GetDistanceFraction( void )
{
	if ( IsOverridingThirdPerson() == true )
	{
		return Vector( m_flTargetFraction, m_flTargetFraction, m_flTargetFraction );
	}

	float flFraction = m_flFraction;
	float flUpFraction = m_flUpFraction;

	float flFrac = RemapValClamped( gpGlobals->curtime - m_flLerpTime, 0, CAMERA_OFFSET_LERP_TIME, 0, 1 );

	flFraction = Lerp( flFrac, m_flFraction, m_flTargetFraction );

	if ( flFrac == 1.0f )
	{
		m_flFraction = m_flTargetFraction;
	}

	flFrac = RemapValClamped( gpGlobals->curtime - m_flUpLerpTime, 0, CAMERA_UP_OFFSET_LERP_TIME, 0, 1 );

	flUpFraction = 1.0f - Lerp( flFrac, m_flUpFraction, m_flTargetUpFraction );

	if ( flFrac == 1.0f )
	{
		m_flUpFraction = m_flTargetUpFraction;
	}

	return Vector( flFraction, flFraction, flUpFraction );
}

void CThirdPersonManager::PositionCamera( CBasePlayer *pPlayer, QAngle angles )
{
	if ( pPlayer )
	{
		trace_t trace;

		Vector camForward, camRight, camUp;

		// find our player's origin, and from there, the eye position
		Vector origin = pPlayer->GetLocalOrigin();
		origin += pPlayer->GetViewOffset();

		AngleVectors( angles, &camForward, &camRight, &camUp );

		Vector endPos = origin;

		Vector vecCamOffset = endPos + (camForward * - GetDesiredCameraOffset()[DIST_FORWARD]) + (camRight * GetDesiredCameraOffset()[ DIST_RIGHT ]) + (camUp * GetDesiredCameraOffset()[ DIST_UP ] );

		// use our previously #defined hull to collision trace
		CTraceFilterSimple traceFilter( pPlayer, COLLISION_GROUP_NONE );
		UTIL_TraceHull( endPos, vecCamOffset, CAM_HULL_MIN, CAM_HULL_MAX, MASK_SOLID & ~CONTENTS_MONSTER, &traceFilter, &trace );

		if ( trace.fraction != m_flTargetFraction )
		{
			m_flLerpTime = gpGlobals->curtime;
		}

		m_flTargetFraction = trace.fraction;
		m_flTargetUpFraction = 1.0f;

		//If we're getting closer to a wall snap the fraction right away.
		if ( m_flTargetFraction < m_flFraction )
		{
			m_flFraction = m_flTargetFraction;
			m_flLerpTime = gpGlobals->curtime;
		}

		// move the camera closer if it hit something
		if( trace.fraction < 1.0  )
		{
			m_vecCameraOffset[ DIST ] *= trace.fraction;

			UTIL_TraceHull( endPos, endPos + (camForward * - GetDesiredCameraOffset()[DIST_FORWARD]), CAM_HULL_MIN, CAM_HULL_MAX, MASK_SOLID & ~CONTENTS_MONSTER, &traceFilter, &trace );

			if ( trace.fraction != 1.0f )
			{
				if ( trace.fraction != m_flTargetUpFraction )
				{
					m_flUpLerpTime = gpGlobals->curtime;
				}

				m_flTargetUpFraction = trace.fraction;

				if ( m_flTargetUpFraction < m_flUpFraction )
				{
					m_flUpFraction = trace.fraction;
					m_flUpLerpTime = gpGlobals->curtime;
				}
			}
		}
	}
}

bool CThirdPersonManager::WantToUseGameThirdPerson( void )
{
	return cl_thirdperson.GetBool() && GameRules() && GameRules()->AllowThirdPersonCamera() && IsOverridingThirdPerson() == false;
}

CThirdPersonManager g_ThirdPersonManager;

class CBaseEntity;

using namespace ::ks::reflect;

// identifier was truncated to '255' characters in the debug information

class CCamoMaterialProxy;

class CCamoTextureRegen
{
public:
	CCamoTextureRegen( CCamoMaterialProxy *pProxy ) : m_pProxy(pProxy) {} 
	void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect );

private:
	CCamoMaterialProxy *m_pProxy;
};

class CCamoMaterialProxy : public CEntityMaterialProxy
{
public:
	CCamoMaterialProxy();
	virtual ~CCamoMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind(CBaseEntity *pC_BaseEntity );
	virtual IMaterial *GetMaterial();

	// Procedurally generates the camo texture...
	void GenerateCamoTexture( ITexture* pTexture, IVTFTexture *pVTFTexture );

protected:

private:
	void LoadCamoPattern( void );
	void GenerateRandomPointsInNormalizedCube( void );
	void GetColors( Vector &lighting, Vector &base, int index, 
						  const Vector &boxMin, const Vector &boxExtents, 
						  const Vector &forward, const Vector &right, const Vector &up,
						  const Vector& entityPosition );
	// this needs to go in a base class

private:
	
	unsigned char *m_pCamoPatternImage;

	IMaterial *m_pMaterial;
	IMaterialVar *m_pCamoTextureVar;
	IMaterialVar *m_pCamoPatternTextureVar;
	Vector *m_pointsInNormalizedBox; // [m_CamoPatternNumColors]

	int m_CamoPatternNumColors;
	int m_CamoPatternWidth;
	int m_CamoPatternHeight;
	unsigned char m_CamoPalette[256][3];
	// these represent that part of the entitiy's bounding box that we 
	// want to cast rays through to get colors for the camo
	Vector m_SubBoundingBoxMin; // normalized
	Vector m_SubBoundingBoxMax; // normalized

	CCamoTextureRegen m_TextureRegen;
	CBaseEntity *m_pEnt;
};

void CCamoTextureRegen::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
{
	m_pProxy->GenerateCamoTexture( pTexture, pVTFTexture );
}

CCamoMaterialProxy::CCamoMaterialProxy() : m_TextureRegen(this)
{
	m_pointsInNormalizedBox = nullptr;
	m_pCamoPatternImage = nullptr;
	m_pMaterial = nullptr;
	m_pCamoTextureVar = nullptr;
	m_pCamoPatternTextureVar = nullptr;
	m_pointsInNormalizedBox = nullptr;
	m_pEnt = nullptr;
}

CCamoMaterialProxy::~CCamoMaterialProxy()
{

	// Disconnect the texture regenerator...
	if (m_pCamoTextureVar)
	{
		ITexture *pCamoTexture = m_pCamoTextureVar->GetTextureValue();
		if (pCamoTexture)
			pCamoTexture->SetTextureRegenerator( nullptr );
	}

	delete[] m_pCamoPatternImage;
	delete[] m_pointsInNormalizedBox;
}

bool CCamoMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	return false; // hack!  Need to make sure that the TGA loader has a valid filesystem before trying
}

void CCamoMaterialProxy::GetColors( Vector &diffuseColor, Vector &baseColor, int index, 
										  const Vector &boxMin, const Vector &boxExtents, 
										  const Vector &forward, const Vector &right, const Vector &up,
										  const Vector& entityPosition )
{
	Vector position, transformedPosition;
	
	// hack
//	m_pointsInNormalizedBox[index] = Vector( 0.5f, 0.5f, 1.0f );
	
	position[0] = m_pointsInNormalizedBox[index][0] * boxExtents[0] + boxMin[0];
	position[1] = m_pointsInNormalizedBox[index][1] * boxExtents[1] + boxMin[1];
	position[2] = m_pointsInNormalizedBox[index][2] * boxExtents[2] + boxMin[2];
	transformedPosition[0] = right[0] * position[0] + forward[0] * position[1] + up[0] * position[2];
	transformedPosition[1] = right[1] * position[0] + forward[1] * position[1] + up[1] * position[2];
	transformedPosition[2] = right[2] * position[0] + forward[2] * position[1] + up[2] * position[2];
	transformedPosition = transformedPosition + entityPosition;
	Vector direction = transformedPosition - CurrentViewOrigin();
	VectorNormalize( direction );
	direction = direction * ( COORD_EXTENT * 1.74f );
	Vector endPoint = position + direction;
	
	// baseColor is already in gamma space
//	engine->TraceLineMaterialAndLighting( g_vecInstantaneousRenderOrigin, endPoint, diffuseColor, baseColor );
	engine->TraceLineMaterialAndLighting( transformedPosition, endPoint, diffuseColor, baseColor );

	// hack - optimize! - convert from linear to gamma space - this should be hidden
	diffuseColor[0] = std::pow( diffuseColor[0], 1.0f / 2.2f );
	diffuseColor[1] = pow( diffuseColor[1], 1.0f / 2.2f );
	diffuseColor[2] = pow( diffuseColor[2], 1.0f / 2.2f );

	if( diffuseColor[0] > 1.0f )
	{
		diffuseColor[0] = 1.0f;
	}
	if( diffuseColor[1] > 1.0f )
	{
		diffuseColor[1] = 1.0f;
	}
	if( diffuseColor[2] > 1.0f )
	{
		diffuseColor[2] = 1.0f;
	}
	// hack
	//baseColor = Vector( 1.0f, 1.0f, 1.0f );
	//diffuseColor = Vector( 1.0f, 1.0f, 1.0f );
}

//-----------------------------------------------------------------------------
// Procedurally generates the camo texture...
//-----------------------------------------------------------------------------
void CCamoMaterialProxy::GenerateCamoTexture( ITexture* pTexture, IVTFTexture *pVTFTexture )
{
	if (!m_pEnt)
		return;

	Vector entityPosition;
	entityPosition = m_pEnt->GetAbsOrigin();

	QAngle entityAngles;
	entityAngles = m_pEnt->GetAbsAngles();

	// Get the bounding box for the entity
	Vector mins, maxs;
	mins = m_pEnt->WorldAlignMins();
	maxs = m_pEnt->WorldAlignMaxs();
	
	Vector traceDirection;
	Vector traceEnd;
	trace_t	traceResult;
	
	Vector forward, right, up;
	AngleVectors( entityAngles, &forward, &right, &up );
	
	Vector position, transformedPosition;
	Vector maxsMinusMins = maxs - mins;

	Vector diffuseColor[256];
	Vector baseColor;

	unsigned char camoPalette[256][3];
	// Calculate the camo palette
	//Msg( "start of loop\n" );
	int i;
	for( i = 0; i < m_CamoPatternNumColors; i++ )
	{
		GetColors( diffuseColor[i], baseColor, i,
			mins, maxsMinusMins, forward, right, up, entityPosition );
		camoPalette[i][0] = diffuseColor[i][0] * baseColor[0] * 255.0f;
		camoPalette[i][1] = diffuseColor[i][1] * baseColor[1] * 255.0f;
		camoPalette[i][2] = diffuseColor[i][2] * baseColor[2] * 255.0f;
	}
	
	int width = pVTFTexture->Width();
	int height = pVTFTexture->Height();
	if( width != m_CamoPatternWidth || height != m_CamoPatternHeight )
	{
		return;
	}
	
	unsigned char *imageData = pVTFTexture->ImageData( 0, 0, 0 );
	enum ImageFormat imageFormat = pVTFTexture->Format();
	if( imageFormat != IMAGE_FORMAT_RGB888 )
	{
		return;
	}
	// optimize
	int x, y;
	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			int offset = 3 * ( x + y * width );
			contract_assert( offset < width * height * 3 );
			int paletteID = m_pCamoPatternImage[x + y * width];
			contract_assert( paletteID < 256 );
			imageData[offset + 0] = camoPalette[paletteID][0];
			imageData[offset + 1] = camoPalette[paletteID][1];
			imageData[offset + 2] = camoPalette[paletteID][2];
		}
	}
}

//-----------------------------------------------------------------------------
// Called when the texture is bound...
//-----------------------------------------------------------------------------
void CCamoMaterialProxy::OnBind( CBaseEntity *pEntity )
{
	if( !m_pCamoTextureVar )
	{
		return;
	}
	
	m_pEnt = pEntity;	
	ITexture *pCamoTexture = m_pCamoTextureVar->GetTextureValue();
	pCamoTexture->Download();

	// Mark it so it doesn't get regenerated on task switch
	m_pEnt = nullptr;
}

void CCamoMaterialProxy::LoadCamoPattern( void )
{
	
	enum ImageFormat indexImageFormat;
	int indexImageSize;
	float dummyGamma;
	if( !TGALoader::GetInfo( m_pCamoPatternTextureVar->GetStringValue(), 
		&m_CamoPatternWidth, &m_CamoPatternHeight, &indexImageFormat, &dummyGamma ) )
	{
		//Warning( "Can't get tga info for hl2/materials/models/combine_elite/camo7paletted.tga for camo material\n" );
		m_pCamoTextureVar = nullptr;
		return;
	}
	
	if( indexImageFormat != IMAGE_FORMAT_I8 )
	{
		//	Warning( "Camo material texture hl2/materials/models/combine_elite/camo7paletted.tga must be 8-bit greyscale\n" );
		m_pCamoTextureVar = nullptr;
		return;
	}
	
	indexImageSize = ImageLoader::GetMemRequired( m_CamoPatternWidth, m_CamoPatternHeight, 1, indexImageFormat, false );
	m_pCamoPatternImage = ( unsigned char * )new unsigned char[indexImageSize];
	if( !m_pCamoPatternImage )
	{
		m_pCamoTextureVar = nullptr;
		return;
	}
	
	if( !TGALoader::Load( m_pCamoPatternImage, m_pCamoPatternTextureVar->GetStringValue(),
		m_CamoPatternWidth, m_CamoPatternHeight, IMAGE_FORMAT_I8, dummyGamma, false ) )
	{
		//			Warning( "camo texture hl2/materials/models/combine_elite/camo7paletted.tga must be grey-scale" );
		m_pCamoTextureVar = nullptr;
		return;
	}
	
	bool colorUsed[256];
	int colorRemap[256];
	// count the number of colors used in the image.
	int i;
	for( i = 0; i < 256; i++ )
	{
		colorUsed[i] = false;
	}
	for( i = 0; i < indexImageSize; i++ )
	{
		colorUsed[m_pCamoPatternImage[i]] = true;
	}
	m_CamoPatternNumColors = 0;
	for( i = 0; i < 256; i++ )
	{
		if( colorUsed[i] )
		{
			colorRemap[i] = m_CamoPatternNumColors;
			m_CamoPatternNumColors++;
		}
	}
	// remap the color to the beginning of the palette.
	for( i = 0; i < indexImageSize; i++ )
	{
		m_pCamoPatternImage[i] = colorRemap[m_pCamoPatternImage[i]];
		// hack
//		m_pCamoPatternImage[i] = 0;
	}
}

void CCamoMaterialProxy::GenerateRandomPointsInNormalizedCube( void )
{
	m_pointsInNormalizedBox = new Vector[m_CamoPatternNumColors];
	if( !m_pointsInNormalizedBox )
	{
		m_pCamoTextureVar = nullptr;
		return;
	}
	
	int i;
	for( i = 0; i < m_CamoPatternNumColors; i++ )
	{
		m_pointsInNormalizedBox[i][0] = random_valve->RandomFloat( m_SubBoundingBoxMin[0], m_SubBoundingBoxMax[0] );
		m_pointsInNormalizedBox[i][1] = random_valve->RandomFloat( m_SubBoundingBoxMin[1], m_SubBoundingBoxMax[1] );
		m_pointsInNormalizedBox[i][2] = random_valve->RandomFloat( m_SubBoundingBoxMin[2], m_SubBoundingBoxMax[2] );
	}
}

IMaterial *CCamoMaterialProxy::GetMaterial()
{
	return m_pMaterial;
}

EXPOSE_MATERIAL_PROXY( CCamoMaterialProxy, Camo );
