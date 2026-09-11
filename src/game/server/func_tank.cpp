//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "func_tank.h"
#include "Sprite.h"
#include "EnvLaser.h"
#include "basecombatweapon.h"
#include "explode.h"
#include "eventqueue.h"
#include "gamerules.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "physics_cannister.h"
#include "decals.h"
#include "shake.h"
#include "particle_smokegrenade.h"
#include "player.h"
#include "entitylist.h"
#include "IEffects.h"
#include "ai_basenpc.h"
#include "effects.h"
#include "iservervehicle.h"
#include "soundenvelope.h"
#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"
#include "props.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mortar_visualize("mortar_visualize", "0" );

IMPLEMENT_REFLECT_DATAMAP( CFuncTank )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncTank::CFuncTank()
{
	m_nBulletCount = 0;

	m_bNPCInRoute = false;
	m_flNextControllerSearch = 0;
	m_bShouldFindNPCs = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncTank::~CFuncTank( void )
{
	if ( m_soundLoopRotate != NULL_STRING && ( m_spawnflags & SF_TANK_SOUNDON ) )
	{
		StopSound( entindex(), CHAN_STATIC, STRING(m_soundLoopRotate) );
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
inline bool CFuncTank::CanFire( void )
{ 
	float flTimeDelay = gpGlobals->curtime - m_lastSightTime;

	// Fire when can't see enemy if time is less that persistence time
	if ( flTimeDelay <= m_persist )
		return true;

	// Fire when I'm in a persistence2 burst
	if ( flTimeDelay <= m_persist2burst )
		return true;

	// If less than persistence2, occasionally do another burst
	if ( flTimeDelay <= m_persist2 )
	{
		if ( random->RandomInt( 0, 30 ) == 0 )
		{
			m_persist2burst = flTimeDelay + 0.5f;
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Purpose: Input handler for activating the tank.
//------------------------------------------------------------------------------
void CFuncTank::InputActivate( inputdata_t &inputdata )
{	
	TankActivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::TankActivate( void )
{
	m_spawnflags |= SF_TANK_ACTIVE; 
	SetNextThink( gpGlobals->curtime + 0.1f ); 
	m_fireLast = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for deactivating the tank.
//-----------------------------------------------------------------------------
void CFuncTank::InputDeactivate( inputdata_t &inputdata )
{
	TankDeactivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::TankDeactivate( void )
{
	m_spawnflags &= ~SF_TANK_ACTIVE; 
	m_fireLast = 0; 
	StopRotSound();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for changing the name of the tank's target entity.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetEntityName( inputdata_t &inputdata )
{
	m_targetEntityName = inputdata.value.StringID();
	m_hTarget = FindTarget( m_targetEntityName, inputdata.pActivator );

	// No longer aim at target position if have one
	m_spawnflags &= ~SF_TANK_AIM_AT_POS; 
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting a new target entity by ehandle.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetEntity( inputdata_t &inputdata )
{
	if ( inputdata.value.Entity() != NULL )
	{
		m_targetEntityName = inputdata.value.Entity()->GetEntityName();
	}
	else
	{
		m_targetEntityName = NULL_STRING;
	}
	m_hTarget = inputdata.value.Entity();

	// No longer aim at target position if have one
	m_spawnflags &= ~SF_TANK_AIM_AT_POS; 
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for clearing the tank's target entity
//-----------------------------------------------------------------------------
void CFuncTank::InputClearTargetEntity( inputdata_t &inputdata )
{
	m_targetEntityName = NULL_STRING;
	m_hTarget = NULL;

	// No longer aim at target position if have one
	m_spawnflags &= ~SF_TANK_AIM_AT_POS; 
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the rate of fire in shots per second.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetFireRate( inputdata_t &inputdata )
{
	m_fireRate = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the damage
//-----------------------------------------------------------------------------
void CFuncTank::InputSetDamage( inputdata_t &inputdata )
{
	m_iBulletDamage = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the target as a position.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetPosition( inputdata_t &inputdata )
{
	m_spawnflags |= SF_TANK_AIM_AT_POS; 
	m_hTarget = NULL;

	inputdata.value.Vector3D( m_vTargetPosition );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the target as a position.
//-----------------------------------------------------------------------------
void CFuncTank::InputSetTargetDir( inputdata_t &inputdata )
{
	m_spawnflags |= SF_TANK_AIM_AT_POS; 
	m_hTarget = NULL;

	Vector vecTargetDir;
	inputdata.value.Vector3D( vecTargetDir );
	m_vTargetPosition = GetAbsOrigin() + m_barrelPos.LengthSqr() * vecTargetDir;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for telling the func_tank to find an NPC to man it.
//-----------------------------------------------------------------------------
void CFuncTank::InputFindNPCToManTank( inputdata_t &inputdata )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncTank::InputStopFindingNPCs( inputdata_t &inputdata )
{
	m_bShouldFindNPCs = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncTank::InputStartFindingNPCs( inputdata_t &inputdata )
{
	m_bShouldFindNPCs = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncTank::InputForceNPCOff( inputdata_t &inputdata )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFuncTank::InputSetMaxRange( inputdata_t &inputdata )
{
	m_maxRange = inputdata.value.Float();
	m_flMaxRange2 = m_maxRange * m_maxRange;
}

//-----------------------------------------------------------------------------
// Purpose: Find the closest NPC with the func_tank behavior.
//-----------------------------------------------------------------------------
void CFuncTank::NPC_FindController( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFuncTank::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// --------------
		// State
		// --------------
		char tempstr[255];
		if (IsActive()) 
		{
			Q_strncpy(tempstr,"State: Active",sizeof(tempstr));
		}
		else 
		{
			Q_strncpy(tempstr,"State: Inactive",sizeof(tempstr));
		}
		NDebugOverlay::EntityText(entindex(),text_offset,tempstr,0);
		text_offset++;
		
		// -------------------
		// Print Firing Speed
		// --------------------
		Q_snprintf(tempstr,sizeof(tempstr),"Fire Rate: %f",m_fireRate);

		NDebugOverlay::EntityText(entindex(),text_offset,tempstr,0);
		text_offset++;
		
		// --------------
		// Print Target
		// --------------
		if (m_hTarget!=NULL) 
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Target: %s",m_hTarget->GetDebugName());
		}
		else
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Target:   -  ");
		}
		NDebugOverlay::EntityText(entindex(),text_offset,tempstr,0);
		text_offset++;

		// --------------
		// Target Pos
		// --------------
		if (m_spawnflags & SF_TANK_AIM_AT_POS) 
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Aim Pos: %3.0f %3.0f %3.0f",m_vTargetPosition.x,m_vTargetPosition.y,m_vTargetPosition.z);
		}
		else
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Aim Pos:    -  ");
		}
		NDebugOverlay::EntityText(entindex(),text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Override base class to add display of fly direction
// Input  :
// Output : 
//-----------------------------------------------------------------------------
void CFuncTank::DrawDebugGeometryOverlays(void) 
{
	// Center
	QAngle angCenter;
	Vector vecForward;
	angCenter = QAngle( 0, YawCenterWorld(), 0 );
	AngleVectors( angCenter, &vecForward );
	NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (vecForward * 64), 255,255,255, true, 0.1);

	// Draw the yaw ranges
	angCenter = QAngle( 0, YawCenterWorld() + m_yawRange, 0 );
	AngleVectors( angCenter, &vecForward );
	NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (vecForward * 128), 0,255,0, true, 0.1);
	angCenter = QAngle( 0, YawCenterWorld() - m_yawRange, 0 );
	AngleVectors( angCenter, &vecForward );
	NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (vecForward * 128), 0,255,0, true, 0.1);

	// Draw the pitch ranges
	angCenter = QAngle( PitchCenterWorld() + m_pitchRange, 0, 0 );
	AngleVectors( angCenter, &vecForward );
	NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (vecForward * 128), 255,0,0, true, 0.1);
	angCenter = QAngle( PitchCenterWorld() - m_pitchRange, 0, 0 );
	AngleVectors( angCenter, &vecForward );
	NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (vecForward * 128), 255,0,0, true, 0.1);

	BaseClass::DrawDebugGeometryOverlays();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pAttacker - 
//			flDamage - 
//			vecDir - 
//			ptr - 
//			bitsDamageType - 
//-----------------------------------------------------------------------------
void CFuncTank::TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType)
{
	if (m_spawnflags & SF_TANK_DAMAGE_KICK)
	{
		// Deflect the func_tank
		// Only adjust yaw for now
		if (pAttacker)
		{
			Vector vFromAttacker = (pAttacker->EyePosition()-GetAbsOrigin());
			vFromAttacker.z = 0;
			VectorNormalize(vFromAttacker);

			Vector vFromAttacker2 = (ptr->endpos-GetAbsOrigin());
			vFromAttacker2.z = 0;
			VectorNormalize(vFromAttacker2);


			Vector vCrossProduct;
			CrossProduct(vFromAttacker,vFromAttacker2, vCrossProduct);

			QAngle angles;
			angles = GetLocalAngles();
			if (vCrossProduct.z > 0)
			{
				angles.y		+= 10;
			}
			else
			{
				angles.y		-= 10;
			}

			// Limit against range in y
			if ( angles.y > m_yawCenter + m_yawRange )
			{
				angles.y = m_yawCenter + m_yawRange;
			}
			else if ( angles.y < (m_yawCenter - m_yawRange) )
			{
				angles.y = (m_yawCenter - m_yawRange);
			}

			SetLocalAngles( angles );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : targetName - 
//			pActivator - 
//-----------------------------------------------------------------------------
CBaseEntity *CFuncTank::FindTarget( string_t targetName, CBaseEntity *pActivator ) 
{
	return gEntList.FindEntityGenericNearest( STRING( targetName ), GetAbsOrigin(), 0, this, pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Caches entity key values until spawn is called.
// Input  : szKeyName - 
//			szValue - 
// Output : 
//-----------------------------------------------------------------------------
bool CFuncTank::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "barrel"))
	{
		m_barrelPos.x = atof(szValue);
		return true;
	}
	
	if (FStrEq(szKeyName, "barrely"))
	{
		m_barrelPos.y = atof(szValue);
		return true;
	}
	
	if (FStrEq(szKeyName, "barrelz"))
	{
		m_barrelPos.z = atof(szValue);
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),		// perfect
	Vector( 0.025, 0.025, 0.025 ),	// small cone
	Vector( 0.05, 0.05, 0.05 ),  // medium cone
	Vector( 0.1, 0.1, 0.1 ),	// large cone
	Vector( 0.25, 0.25, 0.25 ),	// extra-large cone
};
#define MAX_FIRING_SPREADS ARRAYSIZE(gTankSpread)


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::Spawn( void )
{
	Precache();

	m_iSmallAmmoType	= GetAmmoDef()->Index("Pistol");
	m_iMediumAmmoType	= GetAmmoDef()->Index("SMG1");
	m_iLargeAmmoType	= GetAmmoDef()->Index("AR2");

	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetSolid( SOLID_VPHYSICS );
	SetModel( STRING( GetModelName() ) );

	if ( HasSpawnFlags(SF_TANK_NOTSOLID) )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	m_hControlVolume	= NULL;

	if ( GetParent() && GetParent()->GetBaseAnimating() )
	{
		CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
		if ( m_iszBaseAttachment != NULL_STRING )
		{
			int nAttachment = pAnim->LookupAttachment( STRING( m_iszBaseAttachment ) );
			if ( nAttachment != 0 )
			{
				SetParent( pAnim, nAttachment );
				SetLocalOrigin( vec3_origin );
				SetLocalAngles( vec3_angle );
			}
		}

		m_bUsePoseParameters = (m_iszYawPoseParam != NULL_STRING) && (m_iszPitchPoseParam != NULL_STRING);

		if ( m_iszBarrelAttachment != NULL_STRING )
		{
			if ( m_bUsePoseParameters )
			{
				pAnim->SetPoseParameter( STRING( m_iszYawPoseParam ), 0 );
				pAnim->SetPoseParameter( STRING( m_iszPitchPoseParam ), 0 );
				pAnim->InvalidateBoneCache();
			}

			m_nBarrelAttachment = pAnim->LookupAttachment( STRING(m_iszBarrelAttachment) );

			Vector vecWorldBarrelPos;
			QAngle worldBarrelAngle;
			pAnim->GetAttachment( m_nBarrelAttachment, vecWorldBarrelPos, worldBarrelAngle );
			VectorITransform( vecWorldBarrelPos, EntityToWorldTransform( ), m_barrelPos );
		}

		if ( m_bUsePoseParameters )
		{
			// In this case, we're relying on the parent to have the gun model
			AddEffects( EF_NODRAW );
			QAngle localAngles( m_flPitchPoseCenter, m_flYawPoseCenter, 0 );
			SetLocalAngles( localAngles );
			SetSolid( SOLID_NONE );
			SetMoveType( MOVETYPE_NOCLIP );

			// If our parent is a prop_dynamic, make it use hitboxes for renderbox
			CDynamicProp *pProp = dynamic_cast<CDynamicProp*>(GetParent());
			if ( pProp )
			{
				pProp->m_bUseHitboxesForRenderBox = true;
			}
		}
	}

	// For smoothing out leading
	m_flStartLeadFactor = 1.0f;
	m_flNextLeadFactor = 1.0f;
	m_flStartLeadFactorTime = gpGlobals->curtime;
	m_flNextLeadFactorTime = gpGlobals->curtime + 1.0f;

	m_yawCenter			= GetLocalAngles().y;
	m_yawCenterWorld	= GetAbsAngles().y;
	m_pitchCenter		= GetLocalAngles().x;
	m_pitchCenterWorld	= GetAbsAngles().y;
	m_vTargetPosition	= vec3_origin;

	if ( IsActive() || (IsControllable() && !HasController()) )
	{
		// Think to find controllers.
		SetNextThink( gpGlobals->curtime + 1.0f );
		m_flNextControllerSearch = gpGlobals->curtime + 1.0f;
	}

	UpdateMatrix();

	m_sightOrigin = WorldBarrelPosition(); // Point at the end of the barrel

	if ( m_spread > MAX_FIRING_SPREADS )
	{
		m_spread = 0;
	}

	// No longer aim at target position if have one
	m_spawnflags		&= ~SF_TANK_AIM_AT_POS; 

	if (m_spawnflags & SF_TANK_DAMAGE_KICK)
	{
		m_takedamage = DAMAGE_YES;
	}

	// UNDONE: Do this?
	//m_targetEntityName = m_target;
	if ( GetSolid() != SOLID_NONE )
	{
		CreateVPhysics();
	}

	// Setup squared min/max range.
	m_flMinRange2 = m_minRange * m_minRange;
	m_flMaxRange2 = m_maxRange * m_maxRange;
	m_flIgnoreGraceUpto *= m_flIgnoreGraceUpto;

	m_flLastSawNonPlayer = 0;

	if( IsActive() )
	{
		m_OnReadyToFire.FireOutput( this, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::Activate( void )
{
	BaseClass::Activate();

	// Necessary for save/load
	if ( (m_iszBarrelAttachment != NULL_STRING) && (m_nBarrelAttachment == 0) )
	{
		if ( GetParent() && GetParent()->GetBaseAnimating() )
		{
			CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
			m_nBarrelAttachment = pAnim->LookupAttachment( STRING(m_iszBarrelAttachment) );
		}
	}
}

bool CFuncTank::CreateVPhysics()
{
	VPhysicsInitShadow( false, false );
	return true;
}


void CFuncTank::Precache( void )
{
	if ( m_iszSpriteSmoke != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteSmoke) );
	if ( m_iszSpriteFlash != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteFlash) );

	if ( m_soundStartRotate != NULL_STRING )
		PrecacheScriptSound( STRING(m_soundStartRotate) );
	if ( m_soundStopRotate != NULL_STRING )
		PrecacheScriptSound( STRING(m_soundStopRotate) );
	if ( m_soundLoopRotate != NULL_STRING )
		PrecacheScriptSound( STRING(m_soundLoopRotate) );

	PrecacheScriptSound( "Func_Tank.BeginUse" );
}

void CFuncTank::UpdateOnRemove( void )
{
	if ( HasController() )
	{
		StopControl();
	}
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Barrel position
//-----------------------------------------------------------------------------
void CFuncTank::UpdateMatrix( void )
{
	m_parentMatrix.InitFromEntity( GetParent(), GetParentAttachment() );
}

	
//-----------------------------------------------------------------------------
// Barrel position
//-----------------------------------------------------------------------------
Vector CFuncTank::WorldBarrelPosition( void )
{
	if ( (m_nBarrelAttachment == 0) || !GetParent() )
	{
		EntityMatrix tmp;
		tmp.InitFromEntity( this );
		return tmp.LocalToWorld( m_barrelPos );
	}

	Vector vecOrigin;
	QAngle vecAngles;
	CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
	pAnim->GetAttachment( m_nBarrelAttachment, vecOrigin, vecAngles );
	return vecOrigin;
}


//-----------------------------------------------------------------------------
// Make the parent's pose parameters match the func_tank 
//-----------------------------------------------------------------------------
void CFuncTank::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();

	if ( m_bUsePoseParameters && GetParent() )
	{
		const QAngle &angles = GetLocalAngles();
		CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
		pAnim->SetPoseParameter( STRING( m_iszYawPoseParam ), angles.y );
		pAnim->SetPoseParameter( STRING( m_iszPitchPoseParam ), angles.x );
		pAnim->StudioFrameAdvance();
	}
}

//=============================================================================
//
// TANK CONTROLLING
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFuncTank::OnControls( CBaseEntity *pTest )
{
	// Is the tank controllable.
	if ( !IsControllable() )
		return false;

	if ( !m_hControlVolume )
	{
		// Find our control volume
		if ( m_iszControlVolume != NULL_STRING )
		{
			m_hControlVolume = dynamic_cast<CBaseTrigger*>( gEntList.FindEntityByName( NULL, m_iszControlVolume, NULL ) );
		}

		if (( !m_hControlVolume ) && IsControllable() )
		{
			Msg( "ERROR: Couldn't find control volume for player-controllable func_tank %s.\n", STRING(GetEntityName()) );
			return false;
		}
	}

	if ( m_hControlVolume->IsTouching( pTest ) )
		return true;
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFuncTank::StartControl( CBaseCombatCharacter *pController )
{
	// Check to see if we have a controller.
	if ( HasController() && GetController() != pController )
		return false;

	// Team only or disabled?
	if ( m_iszMaster != NULL_STRING )
	{
		if ( !UTIL_IsMasterTriggered( m_iszMaster, pController ) )
			return false;
	}

	// Set func_tank as manned by player/npc.
	m_hController = pController;
	if ( pController->IsPlayer() )
	{
		m_spawnflags |= SF_TANK_PLAYER; 

		CBasePlayer *pPlayer = static_cast<CBasePlayer*>( m_hController.Get() );
		pPlayer->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	}
	else
	{
		m_spawnflags |= SF_TANK_NPC;
		NPC_SetInRoute( false );
	}

	// Holster player/npc weapon
	if ( m_hController->GetActiveWeapon() )
	{
		m_hController->GetActiveWeapon()->Holster();
	}

	// Set the controller's position to be the use position.
	m_vecControllerUsePos = m_hController->GetLocalOrigin();

	EmitSound( "Func_Tank.BeginUse" );
	
	SetNextThink( gpGlobals->curtime + 0.1f );
	
	// Let the map maker know a controller has been found
	if ( m_hController->IsPlayer() )
	{
		m_OnGotPlayerController.FireOutput( this, this );
	}
	else
	{
		m_OnGotController.FireOutput( this, this );
	}

	OnStartControlled();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// TODO: bring back the controllers current weapon
//-----------------------------------------------------------------------------
void CFuncTank::StopControl()
{
	// Do we have a controller?
	if ( !m_hController )
		return;

	OnStopControlled();

	// Arm player/npc weapon.
	if ( m_hController->GetActiveWeapon() )
	{
		m_hController->GetActiveWeapon()->Deploy();
	}

	if ( m_hController->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer*>( m_hController.Get() );
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
	}

	// Stop thinking.
	SetNextThink( TICK_NEVER_THINK );
	
	// Let the map maker know a controller has been lost.
	if ( m_hController->IsPlayer() )
	{
		m_OnLostPlayerController.FireOutput( this, this );
	}
	else
	{
		m_OnLostController.FireOutput( this, this );
	}

	// Reset the func_tank as unmanned (player/npc).
	if ( m_hController->IsPlayer() )
	{
		m_spawnflags &= ~SF_TANK_PLAYER;
	}
	else
	{		
		m_spawnflags &= ~SF_TANK_NPC;
	}

	m_hController = NULL;

	// Set think, if the func_tank can think on its own.
	if ( IsActive() || (IsControllable() && !HasController()) )
	{
		// Delay the think to find controllers a bit
		m_flNextControllerSearch = gpGlobals->curtime + 5.0f;
		SetNextThink( m_flNextControllerSearch );
	}

	SetLocalAngularVelocity( vec3_angle );
}

//-----------------------------------------------------------------------------
// Purpose:
// Called each frame by the player's ItemPostFrame
//-----------------------------------------------------------------------------
void CFuncTank::ControllerPostFrame( void )
{
	// Make sure we have a contoller.
	Assert( m_hController != NULL );

	// Control the firing rate.
	if ( gpGlobals->curtime < m_flNextAttack )
		return;

	if ( !IsPlayerManned() )
		return;

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( m_hController.Get() );
	if ( ( pPlayer->m_nButtons & IN_ATTACK ) == 0 )
		return;

	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	m_fireLast = gpGlobals->curtime - (1/m_fireRate) - 0.01;  // to make sure the gun doesn't fire too many bullets
	
	int bulletCount = (gpGlobals->curtime - m_fireLast) * m_fireRate;
	
	if( HasSpawnFlags( SF_TANK_AIM_ASSISTANCE ) )
	{
		// Trace out a hull and if it hits something, adjust the shot to hit that thing.
		trace_t tr;
		Vector start = WorldBarrelPosition();
		Vector dir = forward;
		
		UTIL_TraceHull( start, start + forward * 8192, -Vector(8,8,8), Vector(8,8,8), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		
		if( tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->m_takedamage != DAMAGE_NO && (tr.Ent<CBaseEntity>()->GetFlags() & FL_AIMTARGET) )
		{
			forward = tr.Ent<CBaseEntity>()->WorldSpaceCenter() - start;
			VectorNormalize( forward );
		}
	}
	
	Fire( bulletCount, WorldBarrelPosition(), forward, pPlayer );
	
	// HACKHACK -- make some noise (that the AI can hear)
	CSoundEnt::InsertSound( SOUND_COMBAT, WorldSpaceCenter(), FUNCTANK_FIREVOLUME, 0.2 );
	
	if( m_iAmmoCount > -1 )
	{
		if( !(m_iAmmoCount % 10) )
		{
			Msg("Ammo Remaining: %d\n", m_iAmmoCount );
		}
		
		if( --m_iAmmoCount == 0 )
		{
			// Kick the player off the gun, and make myself not usable.
			m_spawnflags &= ~SF_TANK_CANCONTROL;
			StopControl();
			return;				
		}
	}
	
	SetNextAttack( gpGlobals->curtime + (1/m_fireRate) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFuncTank::HasController( void )
{ 
	return (m_hController != NULL); 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatCharacter
//-----------------------------------------------------------------------------
CBaseCombatCharacter *CFuncTank::GetController( void ) 
{ 
	return m_hController; 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFuncTank::NPC_FindManPoint( Vector &vecPos )
{
	if ( m_iszNPCManPoint != NULL_STRING )
	{	
		CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_iszNPCManPoint, NULL );
		if ( pEntity )
		{
			vecPos = pEntity->GetAbsOrigin();
			return true;
		}
	}

	return false; 
}

//-----------------------------------------------------------------------------
// Purpose: The NPC manning this gun just saw a player for the first time since he left cover
//-----------------------------------------------------------------------------
void CFuncTank::NPC_JustSawPlayer( CBaseEntity *pTarget )
{
	SetNextAttack( gpGlobals->curtime + m_flPlayerLockTimeBeforeFire );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFuncTank::NPC_Fire( void )
{
	// Control the firing rate.
	if ( gpGlobals->curtime < m_flNextAttack )
		return;

	// Check for a valid npc controller.
	if ( !m_hController )
		return;

	CAI_BaseNPC *pNPC = m_hController->MyNPCPointer();
	if ( !pNPC )
		return;

	// Setup for next round of firing.
	if ( m_nBulletCount == 0 )
	{
		m_nBulletCount = GetRandomBurst();
		m_fireTime = 1.0f;
	}

	// m_fireLast looks like it is only needed for Active non-controlled func_tank.
//		m_fireLast = gpGlobals->curtime - (1/m_fireRate) - 0.01;  // to make sure the gun doesn't fire too many bullets		

	Vector vecBarrelEnd = WorldBarrelPosition();		
	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );

	if ( (pNPC->CapabilitiesGet() & bits_CAP_NO_HIT_SQUADMATES) && pNPC->IsInSquad() )
	{
		// Avoid shooting squadmates.
		if ( pNPC->IsSquadmateInSpread( vecBarrelEnd, vecBarrelEnd + vecForward * 2048, gTankSpread[m_spread].x, 8*12 ) )
		{
			return;
		}
	}

	if ( !HasSpawnFlags( SF_TANK_ALLOW_PLAYER_HITS ) && (pNPC->CapabilitiesGet() & bits_CAP_NO_HIT_PLAYER) )
	{
		// Avoid shooting player.
		if ( pNPC->PlayerInSpread( vecBarrelEnd, vecBarrelEnd + vecForward * 2048, gTankSpread[m_spread].x, 8*12 ) )
		{
			return;
		}
	}

	// Fire the bullet(s).
	Fire( 1, vecBarrelEnd, vecForward, m_hController );
	--m_nBulletCount;

	// Check ammo counts and dismount when empty.
	if( m_iAmmoCount > -1 )
	{
		if( --m_iAmmoCount == 0 )
		{
			// Disable the func_tank.
			m_spawnflags &= ~SF_TANK_CANCONTROL;

			// Remove the npc.
			StopControl();
			return;				
		}
	}
	
	float flFireTime = GetRandomFireTime();
	if ( m_nBulletCount != 0 )
	{	
		m_fireTime -= flFireTime;
		SetNextAttack( gpGlobals->curtime + flFireTime );
	}
	else
	{
		SetNextAttack( gpGlobals->curtime + m_fireTime );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFuncTank::NPC_HasEnemy( void )
{
	if ( !IsNPCManned() )
		return false;

	CAI_BaseNPC *pNPC = m_hController->MyNPCPointer();
	Assert( pNPC );

	return ( pNPC->GetEnemy() != NULL );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFuncTank::NPC_InterruptRoute( void )
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFuncTank::NPC_InterruptController( void )
{
	m_hController = NULL;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CFuncTank::GetRandomFireTime( void )
{
	Assert( m_fireRate != 0 );
	float flOOFireRate = 1.0f / m_fireRate;
	float flOOFireRateBy2 = flOOFireRate * 0.5f;
	float flOOFireRateBy4 = flOOFireRate * 0.25f;
	return random->RandomFloat( flOOFireRateBy4, flOOFireRateBy2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CFuncTank::GetRandomBurst( void )
{
	return random->RandomInt( m_fireRate-2, m_fireRate+2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CFuncTank::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !IsControllable() )
		return;

	// player controlled turret
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( !pPlayer )
		return;

	if ( value == 2 && useType == USE_SET )
	{
		ControllerPostFrame();
	}
	else if ( m_hController != pPlayer && useType != USE_OFF )
	{
		// The player must be within the func_tank controls
		if ( !m_hControlVolume )
		{
			// Find our control volume
			if ( m_iszControlVolume != NULL_STRING )
			{
				m_hControlVolume = dynamic_cast<CBaseTrigger*>( gEntList.FindEntityByName( NULL, m_iszControlVolume, NULL ) );
			}

			if (( !m_hControlVolume ) && IsControllable() )
			{
				Msg( "ERROR: Couldn't find control volume for player-controllable func_tank %s.\n", STRING(GetEntityName()) );
				return;
			}
		}

		if ( !m_hControlVolume->IsTouching( pPlayer ) )
			return;

		// Interrupt any npc in route (ally or not).
		if ( NPC_InRoute() )
		{
			// Interrupt the npc's route.
			NPC_InterruptRoute();
		}

		// Interrupt NPC - if possible (they must be allies).
		if ( IsNPCControllable() && HasController() )
		{
			if ( !NPC_InterruptController() )
				return;
		}

		pPlayer->SetUseEntity( this );
		StartControl( pPlayer );
	}
	else 
	{
		StopControl();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : range - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFuncTank::InRange( float range )
{
	if ( range < m_minRange )
		return FALSE;
	if ( (m_maxRange > 0) && (range > m_maxRange) )
		return FALSE;

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFuncTank::InRange2( float flRange2 )
{
	if ( flRange2 < m_flMinRange2 )
		return false;

	if ( ( m_flMaxRange2 > 0.0f ) && ( flRange2 > m_flMaxRange2 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFuncTank::Think( void )
{
	m_hFuncTankTarget = NULL;

	// Look for a new controller?
	if ( IsControllable() && !HasController() && (m_flNextControllerSearch <= gpGlobals->curtime) )
	{
		if ( m_bShouldFindNPCs && gpGlobals->curtime > 5.0f )
		{
			// Check for in route and timer.
			if ( !NPC_InRoute() )
			{
				NPC_FindController();
			}
		}

		// Keep thinking, in case they turn NPC finding back on
		if ( !HasController() )
		{
			SetNextThink( gpGlobals->curtime + 2.0f );
		}

		m_flNextControllerSearch = gpGlobals->curtime + 2.0f;
	}

	// refresh the matrix
	UpdateMatrix();

	SetLocalAngularVelocity( vec3_angle );
	TrackTarget();

	if ( fabs(GetLocalAngularVelocity().x) > 1 || fabs(GetLocalAngularVelocity().y) > 1 )
	{
		StartRotSound();
	}
	else
	{
		StopRotSound();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Aim the offset barrel at a position in parent space
// Input  : parentTarget - the position of the target in parent space
// Output : Vector - angles in local space
//-----------------------------------------------------------------------------
QAngle CFuncTank::AimBarrelAt( const Vector &parentTarget )
{
	Vector target = parentTarget - GetLocalOrigin();
	float quadTarget = target.LengthSqr();
	float quadTargetXY = target.x*target.x + target.y*target.y;

	// Target is too close!  Can't aim at it
	if ( quadTarget <= m_barrelPos.LengthSqr() )
	{
		return GetLocalAngles();
	}
	else
	{
		// We're trying to aim the offset barrel at an arbitrary point.
		// To calculate this, I think of the target as being on a sphere with 
		// it's center at the origin of the gun.
		// The rotation we need is the opposite of the rotation that moves the target 
		// along the surface of that sphere to intersect with the gun's shooting direction
		// To calculate that rotation, we simply calculate the intersection of the ray 
		// coming out of the barrel with the target sphere (that's the new target position)
		// and use atan2() to get angles

		// angles from target pos to center
		float targetToCenterYaw = atan2( target.y, target.x );
		float centerToGunYaw = atan2( m_barrelPos.y, sqrt( quadTarget - (m_barrelPos.y*m_barrelPos.y) ) );

		float targetToCenterPitch = atan2( target.z, sqrt( quadTargetXY ) );
		float centerToGunPitch = atan2( -m_barrelPos.z, sqrt( quadTarget - (m_barrelPos.z*m_barrelPos.z) ) );
		return QAngle( -RAD2DEG(targetToCenterPitch+centerToGunPitch), RAD2DEG( targetToCenterYaw + centerToGunYaw ), 0 );
	}
}


//-----------------------------------------------------------------------------
// Aim the tank at the player crosshair 
//-----------------------------------------------------------------------------
void CFuncTank::CalcPlayerCrosshairTarget( Vector *pVecTarget )
{
	// Get the player.
	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( m_hController.Get() );

	// Tank aims at player's crosshair.
	Vector vecStart, vecDir;
	trace_t	tr;
	
	vecStart = pPlayer->EyePosition();
	vecDir = pPlayer->EyeDirection3D();
	
	// Make sure to start the trace outside of the player's bbox!
	UTIL_TraceLine( vecStart + vecDir * 24, vecStart + vecDir * 8192, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );

	*pVecTarget = tr.endpos;
}

//-----------------------------------------------------------------------------
// Aim the tank at the player crosshair 
//-----------------------------------------------------------------------------
void CFuncTank::AimBarrelAtPlayerCrosshair( QAngle *pAngles )
{
	Vector vecTarget;
	CalcPlayerCrosshairTarget( &vecTarget );
	*pAngles = AimBarrelAt( m_parentMatrix.WorldToLocal( vecTarget ) );
}


//-----------------------------------------------------------------------------
// Aim the tank at the NPC's enemy
//-----------------------------------------------------------------------------
void CFuncTank::CalcNPCEnemyTarget( Vector *pVecTarget )
{
	Vector vecTarget;
	CAI_BaseNPC *pNPC = m_hController->MyNPCPointer();

	// Aim the barrel at the npc's enemy, or where the npc is looking.
	CBaseEntity *pEnemy = pNPC->GetEnemy();
	if ( pEnemy )
	{
		// Clear the idle target
		*pVecTarget = pEnemy->BodyTarget( GetAbsOrigin(), false );
		m_vecNPCIdleTarget = *pVecTarget;
	}
	else
	{
		if ( m_vecNPCIdleTarget != vec3_origin )
		{
			*pVecTarget = m_vecNPCIdleTarget;
		}
		else
		{
			Vector vecForward;
			QAngle angCenter( 0, m_yawCenterWorld, 0 );
			AngleVectors( angCenter, &vecForward );
			trace_t tr;
			Vector vecBarrel = GetAbsOrigin() + m_barrelPos;
			UTIL_TraceLine( vecBarrel, vecBarrel + vecForward * 8192, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
			*pVecTarget = tr.endpos;
		}
	}
}

	
//-----------------------------------------------------------------------------
// Aim the tank at the NPC's enemy
//-----------------------------------------------------------------------------
void CFuncTank::AimBarrelAtNPCEnemy( QAngle *pAngles )
{
	Vector vecTarget;
	CalcNPCEnemyTarget( &vecTarget );
	*pAngles = AimBarrelAt( m_parentMatrix.WorldToLocal( vecTarget ) );
}

//-----------------------------------------------------------------------------
// Returns true if the desired angles are out of range 
//-----------------------------------------------------------------------------
bool CFuncTank::RotateTankToAngles( const QAngle &angles, float *pDistX, float *pDistY )
{
	bool bClamped = false;

	// Force the angles to be relative to the center position
	float offsetY = UTIL_AngleDistance( angles.y, m_yawCenter );
	float offsetX = UTIL_AngleDistance( angles.x, m_pitchCenter );

	float flActualYaw = m_yawCenter + offsetY;
	float flActualPitch = m_pitchCenter + offsetX;

	if ( ( fabs( offsetY ) > m_yawRange + m_yawTolerance ) ||
		 ( fabs( offsetX ) > m_pitchRange + m_pitchTolerance ) )
	{
		// Limit against range in x
		flActualYaw = clamp( flActualYaw, m_yawCenter - m_yawRange, m_yawCenter + m_yawRange );
		flActualPitch = clamp( flActualPitch, m_pitchCenter - m_pitchRange, m_pitchCenter + m_pitchRange );

		bClamped = true;
	}

	// Get at the angular vel
	QAngle vecAngVel = GetLocalAngularVelocity();

	// Move toward target at rate or less
	float distY = UTIL_AngleDistance( flActualYaw, GetLocalAngles().y );
	vecAngVel.y = distY * 10;
	vecAngVel.y = clamp( vecAngVel.y, -m_yawRate, m_yawRate );

	// Move toward target at rate or less
	float distX = UTIL_AngleDistance( flActualPitch, GetLocalAngles().x );
	vecAngVel.x = distX  * 10;
	vecAngVel.x = clamp( vecAngVel.x, -m_pitchRate, m_pitchRate );

	// How exciting! We're done
	SetLocalAngularVelocity( vecAngVel );

	if ( pDistX && pDistY )
	{
		*pDistX = distX;
		*pDistY = distY;
	}

	return bClamped;
}


//-----------------------------------------------------------------------------
// We lost our target! 
//-----------------------------------------------------------------------------
void CFuncTank::LostTarget( void )
{
	if (m_fireLast != 0)
	{
		m_OnLoseTarget.FireOutput(this, this);
		m_fireLast = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::ComputeLeadingPosition( const Vector &vecShootPosition, CBaseEntity *pTarget, Vector *pLeadPosition )
{
	Vector vecTarget = pTarget->BodyTarget( vecShootPosition, false );
	float flShotSpeed = GetShotSpeed();
	if ( flShotSpeed == 0 )
	{
		*pLeadPosition = vecTarget;
		return;
	}

	Vector vecVelocity = pTarget->GetSmoothedVelocity();
	vecVelocity.z = 0.0f;
	float flTargetSpeed = VectorNormalize( vecVelocity );

	// Guesstimate...
	if ( m_flNextLeadFactorTime < gpGlobals->curtime )
	{
		m_flStartLeadFactor = m_flNextLeadFactor;
		m_flStartLeadFactorTime = gpGlobals->curtime;
		m_flNextLeadFactor = random->RandomFloat( 0.8f, 1.3f );
		m_flNextLeadFactorTime = gpGlobals->curtime + random->RandomFloat( 2.0f, 4.0f );
	}

	float flFactor = (gpGlobals->curtime - m_flStartLeadFactorTime) / (m_flNextLeadFactorTime - m_flStartLeadFactorTime);
	float flLeadFactor = SimpleSplineRemapVal( flFactor, 0.0f, 1.0f, m_flStartLeadFactor, m_flNextLeadFactor );
	flTargetSpeed *= flLeadFactor;

	Vector vecDelta;
	VectorSubtract( vecShootPosition, vecTarget, vecDelta );
	float flTargetToShooter = VectorNormalize( vecDelta );
	float flCosTheta = DotProduct( vecDelta, vecVelocity );

	// Law of cosines... z^2 = x^2 + y^2 - 2xy cos Theta
	// where z = flShooterToPredictedTargetPosition = flShotSpeed * predicted time
	// x = flTargetSpeed * predicted time
	// y = flTargetToShooter
	// solve for predicted time using at^2 + bt + c = 0, t = (-b +/- sqrt( b^2 - 4ac )) / 2a
	float a = flTargetSpeed * flTargetSpeed - flShotSpeed * flShotSpeed;
	float b = -2.0f * flTargetToShooter * flCosTheta * flTargetSpeed;
	float c = flTargetToShooter * flTargetToShooter;
	
	float flDiscrim = b*b - 4*a*c;
	if (flDiscrim < 0)
	{
		*pLeadPosition = vecTarget;
		return;
	}

	flDiscrim = sqrt(flDiscrim);
	float t = (-b + flDiscrim) / (2.0f * a);
	float t2 = (-b - flDiscrim) / (2.0f * a);
	if ( t < t2 )
	{
		t = t2;
	}

	if ( t <= 0.0f )
	{
		*pLeadPosition = vecTarget;
		return;
	}

	VectorMA( vecTarget, flTargetSpeed * t, vecVelocity, *pLeadPosition );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::AimFuncTankAtTarget( void )
{
	// Get world target position
	CBaseEntity *pTarget = NULL;
	trace_t tr;
	QAngle angles;
	bool bUpdateTime = false;

	CBaseEntity *pTargetVehicle = NULL;
	Vector barrelEnd = WorldBarrelPosition();
	Vector worldTargetPosition;
	if (m_spawnflags & SF_TANK_AIM_AT_POS)
	{
		worldTargetPosition = m_vTargetPosition;
	}
	else
	{
		CBaseEntity *pEntity = (CBaseEntity *)m_hTarget;
		if ( !pEntity || ( pEntity->GetFlags() & FL_NOTARGET ) )
		{
			if( m_targetEntityName != NULL_STRING )
			{
				m_hTarget = FindTarget( m_targetEntityName, NULL );
			}
			
			LostTarget();
			return;
		}

		pTarget = pEntity;

		// Calculate angle needed to aim at target
		worldTargetPosition = pEntity->EyePosition();
		if ( pEntity->IsPlayer() )
		{
			CBasePlayer *pPlayer = assert_cast<CBasePlayer*>(pEntity);
			pTargetVehicle = pPlayer->GetVehicleEntity();
			if ( pTargetVehicle )
			{
				worldTargetPosition = pTargetVehicle->BodyTarget( GetAbsOrigin(), false );
			}
		}
	}

	float range2 = worldTargetPosition.DistToSqr( barrelEnd );
	if ( !InRange2( range2 ) )
	{
		if ( m_hTarget )
		{
			m_hTarget = NULL;
			LostTarget();
		}
		return;
	}

	Vector vecAimOrigin = m_sightOrigin;
	if (m_spawnflags & SF_TANK_AIM_AT_POS)
	{
		bUpdateTime		= true;
		m_sightOrigin	= m_vTargetPosition;
		vecAimOrigin = m_sightOrigin;
	}
	else
	{
		if ( m_spawnflags & SF_TANK_LINEOFSIGHT )
		{
			AI_TraceLOS( barrelEnd, worldTargetPosition, this, &tr );
		}
		else
		{
			tr.fraction = 1.0f;
			tr.m_pEnt = pTarget;
		}

		// No line of sight, don't track
		if ( tr.fraction == 1.0 || tr.Ent<CBaseEntity>() == pTarget || (pTargetVehicle && (tr.Ent<CBaseEntity>() == pTargetVehicle)) )
		{
			if ( InRange2( range2 ) && pTarget && pTarget->IsAlive() )
			{
				bUpdateTime = true;

				// Sight position is BodyTarget with no noise (so gun doesn't bob up and down)
				CBaseEntity *pInstance = pTargetVehicle ? pTargetVehicle : pTarget;
				m_hFuncTankTarget = pInstance;

				m_sightOrigin = pInstance->BodyTarget( GetAbsOrigin(), false );
				if ( m_bPerformLeading )
				{
					ComputeLeadingPosition( barrelEnd, pInstance, &vecAimOrigin );
				}
				else
				{
					vecAimOrigin = m_sightOrigin;
				}
			}
		}
	}

	// Convert targetPosition to parent
	Vector vecLocalOrigin = m_parentMatrix.WorldToLocal( vecAimOrigin );
	angles = AimBarrelAt( vecLocalOrigin );

	// FIXME: These need to be the clamped angles
	float distX, distY;
	bool bClamped = RotateTankToAngles( angles, &distX, &distY );
	if ( bClamped )
	{
		bUpdateTime = false;
	}

	if ( bUpdateTime )
	{
		if( (gpGlobals->curtime - m_lastSightTime >= 1.0) && (gpGlobals->curtime > m_flNextAttack) )
		{
			// Enemy was hidden for a while, and I COULD fire right now. Instead, tack a delay on.
			m_flNextAttack = gpGlobals->curtime + 0.5;
		}

		m_lastSightTime = gpGlobals->curtime;
		m_persist2burst = 0;
	}

	SetMoveDoneTime( 0.1 );

	if ( CanFire() && ( (fabs(distX) <= m_pitchTolerance) && (fabs(distY) <= m_yawTolerance) || (m_spawnflags & SF_TANK_LINEOFSIGHT) ) )
	{
		bool fire = false;
		Vector forward;
		AngleVectors( GetLocalAngles(), &forward );
		forward = m_parentMatrix.ApplyRotation( forward );

		if ( m_spawnflags & SF_TANK_LINEOFSIGHT )
		{
			AI_TraceLine( barrelEnd, pTarget->WorldSpaceCenter(), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction == 1.0f || (tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>() == pTarget) )
			{
				fire = true;
			}
		}
		else
		{
			fire = true;
		}

		if ( fire )
		{
			if (m_fireLast == 0)
			{
				m_OnAquireTarget.FireOutput(this, this);
			}
			FiringSequence( barrelEnd, forward, this );
		}
		else 
		{
			LostTarget();
		}
	}
	else 
	{
		LostTarget();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::TrackTarget( void )
{
	QAngle angles;

	if( !m_bReadyToFire && m_flNextAttack <= gpGlobals->curtime )
	{
		m_OnReadyToFire.FireOutput( this, this );
		m_bReadyToFire = true;
	}

	if ( IsPlayerManned() )
	{
		AimBarrelAtPlayerCrosshair( &angles );
		RotateTankToAngles( angles );
		SetNextThink( gpGlobals->curtime + 0.05f );
		SetMoveDoneTime( 0.1 );
		return;
	}

	if ( IsNPCManned() )
	{
		AimBarrelAtNPCEnemy( &angles );
		RotateTankToAngles( angles );
		SetNextThink( gpGlobals->curtime + 0.05f );
		SetMoveDoneTime( 0.1 );
		return;
	}

	if ( !IsActive() )
	{
		// If we're not active, but we're controllable, we need to keep thinking
		if ( IsControllable() && !HasController() )
		{
			// Think to find controllers.
			SetNextThink( m_flNextControllerSearch );
		}
		return;
	}

	// Clean room for unnecessarily complicated old code
	SetNextThink( gpGlobals->curtime + 0.1f );
	AimFuncTankAtTarget();
}


//-----------------------------------------------------------------------------
// Purpose: Start of firing sequence.  By default, just fire now.
// Input  : &barrelEnd - 
//			&forward - 
//			*pAttacker - 
//-----------------------------------------------------------------------------
void CFuncTank::FiringSequence( const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->curtime - m_fireLast) * m_fireRate;
		
		if ( bulletCount > 0 )
		{
			// NOTE: Set m_fireLast first so that Fire can adjust it
			m_fireLast = gpGlobals->curtime;
			Fire( bulletCount, barrelEnd, forward, pAttacker );
		}
	}
	else
	{
		m_fireLast = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTank::DoMuzzleFlash( void )
{
	// If we're parented to something, make it play the muzzleflash
	if ( m_bUsePoseParameters && GetParent() )
	{
		CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
		pAnim->DoMuzzleFlash();

		// Do the AR2 muzzle flash
		CEffectData data;
		data.m_nEntIndex = pAnim->entindex();
		data.m_nAttachmentIndex = m_nBarrelAttachment;
		data.m_flScale = 1.0f;
		data.m_fFlags = MUZZLEFLASH_COMBINE;
		DispatchEffect( "MuzzleFlash", data );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CFuncTank::GetTracerType( void )
{
	if ( m_iEffectHandling == EH_AR2 )
		return "AR2Tracer";

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Fire targets and spawn sprites.
// Input  : bulletCount - 
//			barrelEnd - 
//			forward - 
//			pAttacker - 
//-----------------------------------------------------------------------------
void CFuncTank::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	// If we have a specific effect handler, apply it's effects
	if ( m_iEffectHandling == EH_AR2 )
	{
		DoMuzzleFlash();

		// Play the AR2 sound
		EmitSound( "Weapon_functank.Single" );
	}
	else
	{
		if ( m_iszSpriteSmoke != NULL_STRING )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteSmoke), barrelEnd, TRUE );
			pSprite->AnimateAndDie( random->RandomFloat( 15.0, 20.0 ) );
			pSprite->SetTransparency( kRenderTransAlpha, m_clrRender->r, m_clrRender->g, m_clrRender->b, 255, kRenderFxNone );

			Vector vecVelocity( 0, 0, random->RandomFloat(40, 80) ); 
			pSprite->SetAbsVelocity( vecVelocity );
			pSprite->SetScale( m_spriteScale );
		}
		if ( m_iszSpriteFlash != NULL_STRING )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteFlash), barrelEnd, TRUE );
			pSprite->AnimateAndDie( 5 );
			pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->SetScale( m_spriteScale );
		}
	}

	m_OnFire.FireOutput(this, this);
	m_bReadyToFire = false;
}


void CFuncTank::TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, trace_t &tr )
{
	Vector forward, right, up;

	AngleVectors( GetAbsAngles(), &forward, &right, &up );
	// get circular gaussian spread
	float x, y, z;
	do {
		x = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		y = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecForward +
		x * vecSpread.x * right +
		y * vecSpread.y * up;
	Vector vecEnd;
	
	vecEnd = vecStart + vecDir * MAX_TRACE_LENGTH;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
}

	
void CFuncTank::StartRotSound( void )
{
	if ( m_spawnflags & SF_TANK_SOUNDON )
		return;
	m_spawnflags |= SF_TANK_SOUNDON;
	
	if ( m_soundLoopRotate != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );
		filter.MakeReliable();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = (char*)STRING(m_soundLoopRotate);
		ep.m_flVolume = 0.85;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}
	
	if ( m_soundStartRotate != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_soundStartRotate);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}
}


void CFuncTank::StopRotSound( void )
{
	if ( m_spawnflags & SF_TANK_SOUNDON )
	{
		if ( m_soundLoopRotate != NULL_STRING )
		{
			StopSound( entindex(), CHAN_STATIC, (char*)STRING(m_soundLoopRotate) );
		}
		if ( m_soundStopRotate != NULL_STRING )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_BODY;
			ep.m_pSoundName = (char*)STRING(m_soundStopRotate);
			ep.m_flVolume = 1.0f;
			ep.m_SoundLevel = SNDLVL_NORM;

			EmitSound( filter, entindex(), ep );
		}
	}
	m_spawnflags &= ~SF_TANK_SOUNDON;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CFuncTank::IsEntityInViewCone( CBaseEntity *pEntity )
{
	// First check to see if the enemy is in range.
	Vector vecBarrelEnd = WorldBarrelPosition();
	float flRange2 = ( pEntity->GetAbsOrigin() - vecBarrelEnd ).LengthSqr();

	if( !(GetSpawnFlags() & SF_TANK_IGNORE_RANGE_IN_VIEWCONE) )
	{
		if ( !InRange2( flRange2 ) )
			return false;
	}

	// If we're trying to shoot at a player, and we've seen a non-player recently, check the grace period
	if ( m_flPlayerGracePeriod && pEntity->IsPlayer() && (gpGlobals->curtime - m_flLastSawNonPlayer) < m_flPlayerGracePeriod )
	{
		// Grace period is ignored under a certain distance
		if ( flRange2 > m_flIgnoreGraceUpto )
			return false;
	}

	// Check to see if the entity center lies within the yaw and pitch constraints.
	// This isn't horribly accurate, but should do for now.
	QAngle angGun;
	angGun = AimBarrelAt( m_parentMatrix.WorldToLocal( pEntity->GetAbsOrigin() ) );
	
	// Force the angles to be relative to the center position
	float flOffsetY = UTIL_AngleDistance( angGun.y, m_yawCenter );
	float flOffsetX = UTIL_AngleDistance( angGun.x, m_pitchCenter );
	angGun.y = m_yawCenter + flOffsetY;
	angGun.x = m_pitchCenter + flOffsetX;

	if ( ( fabs( flOffsetY ) > m_yawRange + m_yawTolerance ) || ( fabs( flOffsetX ) > m_pitchRange + m_pitchTolerance ) )
		return false;

	// Remember the last time we saw a non-player
	if ( !pEntity->IsPlayer() )
	{
		m_flLastSawNonPlayer = gpGlobals->curtime;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this func tank can see the enemy
//-----------------------------------------------------------------------------
bool CFuncTank::HasLOSTo( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return false;

	// Get the barrel position
	Vector vecBarrelEnd = WorldBarrelPosition();
	Vector vecTarget = pEntity->BodyTarget( GetAbsOrigin(), false );
	trace_t tr;

	// Ignore the func_tank and any prop it's parented to
	CTraceFilterSkipTwoEntities traceFilter( this, GetParent(), COLLISION_GROUP_NONE );
	AI_TraceLine( vecBarrelEnd, vecTarget, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	
	CBaseEntity	*pHitEntity = tr.Ent<CBaseEntity>();
	
	CBasePlayer *pPlayer = ToBasePlayer( pEntity );

	// Is player in a vehicle? if so, verify vehicle is target and return if so (so npc shoots at vehicle)
	if ( pPlayer && pPlayer->IsInAVehicle() && pHitEntity )
	{
		// Ok, player in vehicle, check if vehicle is target we're looking at, fire if it is
		// Also, check to see if the owner of the entity is the vehicle, in which case it's valid too.
		// This catches vehicles that use bone followers.
		CBaseEntity	*pVehicle  = pPlayer->GetVehicle()->GetVehicleEnt();
		if ( pHitEntity == pVehicle || pHitEntity->GetOwnerEntity() == pVehicle )
			return true;
	}

	return ( tr.fraction == 1.0 || tr.Ent<CBaseEntity>() == pEntity );
}

// #############################################################################
//   CFuncTankGun
// #############################################################################
class CFuncTankGun : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankGun, CFuncTank );

	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
};
LINK_ENTITY_TO_CLASS( func_tank, CFuncTankGun );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTankGun::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
}

// #############################################################################
//   CFuncTankPulseLaser
// #############################################################################
class CFuncTankPulseLaser : public CFuncTankGun
{
public:
	DECLARE_CLASS( CFuncTankPulseLaser, CFuncTankGun );
	DECLARE_DATADESC();

	void Precache();
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );

	[[= ks::reflect::Key{ .name = "PulseSpeed" } ]] float		m_flPulseSpeed;
	[[= ks::reflect::Key{ .name = "PulseWidth" } ]] float		m_flPulseWidth;
	[[= ks::reflect::Key{ .name = "PulseColor" } ]] color32		m_flPulseColor;
	[[= ks::reflect::Key{ .name = "PulseLife" } ]] float		m_flPulseLife;
	[[= ks::reflect::Key{ .name = "PulseLag" } ]] float		m_flPulseLag;
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "PulseFireSound" } ]] string_t	m_sPulseFireSound;
};
LINK_ENTITY_TO_CLASS( func_tankpulselaser, CFuncTankPulseLaser );

IMPLEMENT_REFLECT_DATAMAP( CFuncTankPulseLaser )

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CFuncTankPulseLaser::Precache(void)
{
	UTIL_PrecacheOther( "grenade_beam" );

	if ( m_sPulseFireSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_sPulseFireSound) );
	}
	BaseClass::Precache();
}
//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CFuncTankPulseLaser::Fire( int bulletCount, const Vector &barrelEnd, const Vector &vecForward, CBaseEntity *pAttacker )
{
}

// #############################################################################
//   CFuncTankLaser
// #############################################################################
class CFuncTankLaser : public CFuncTank
{
	DECLARE_CLASS( CFuncTankLaser, CFuncTank );
public:
	void	Activate( void );
	void	Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	void	Think( void );
	CEnvLaser *GetLaser( void );

	DECLARE_DATADESC();

private:
	CEnvLaser	*m_pLaser;
	float	m_laserTime;
	[[= ks::reflect::Key{ .name = "laserentity" } ]] string_t m_iszLaserName;
};
LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );

IMPLEMENT_REFLECT_DATAMAP( CFuncTankLaser )


void CFuncTankLaser::Activate( void )
{
	BaseClass::Activate();

	if ( !GetLaser() )
	{
		UTIL_Remove(this);
		Warning( "Laser tank with no env_laser!\n" );
	}
	else
	{
		m_pLaser->TurnOff();
	}
}


CEnvLaser *CFuncTankLaser::GetLaser( void )
{
	if ( m_pLaser )
		return m_pLaser;

	CBaseEntity *pLaser = gEntList.FindEntityByName( NULL, m_iszLaserName, NULL );
	while ( pLaser )
	{
		// Found the landmark
		if ( FClassnameIs( pLaser, "env_laser" ) )
		{
			m_pLaser = (CEnvLaser *)pLaser;
			break;
		}
		else
		{
			pLaser = gEntList.FindEntityByName( pLaser, m_iszLaserName, NULL );
		}
	}

	return m_pLaser;
}


void CFuncTankLaser::Think( void )
{
	if ( m_pLaser && (gpGlobals->curtime > m_laserTime) )
		m_pLaser->TurnOff();

	CFuncTank::Think();
}


void CFuncTankLaser::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	int i;
	trace_t tr;

	if ( GetLaser() )
	{
		for ( i = 0; i < bulletCount; i++ )
		{
			m_pLaser->SetLocalOrigin( barrelEnd );
			TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
			
			m_laserTime = gpGlobals->curtime;
			m_pLaser->TurnOn();
			m_pLaser->SetFireTime( gpGlobals->curtime - 1.0 );
			m_pLaser->FireAtPoint( tr );
			m_pLaser->SetNextThink( TICK_NEVER_THINK );
		}
		CFuncTank::Fire( bulletCount, barrelEnd, forward, this );
	}
}

class CFuncTankRocket : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankRocket, CFuncTank );

	void Precache( void );
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	virtual float GetShotSpeed() { return m_flRocketSpeed; }

protected:
	[[= ks::reflect::Key{ .name = "rocketspeed" } ]] float	m_flRocketSpeed;

	DECLARE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP( CFuncTankRocket )

LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket::Precache( void )
{
	UTIL_PrecacheOther( "rpg_missile" );
	CFuncTank::Precache();
}

void CFuncTankRocket::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
}


//-----------------------------------------------------------------------------
// Airboat gun
//-----------------------------------------------------------------------------
class CFuncTankAirboatGun : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankAirboatGun, CFuncTank );
 	DECLARE_DATADESC();

	void Precache( void );
	virtual void Spawn();
	virtual void Activate();
	virtual void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	virtual void ControllerPostFrame();
	virtual void OnStopControlled();
	virtual const char *GetTracerType( void );
	virtual Vector WorldBarrelPosition( void );
	virtual void DoImpactEffect( trace_t &tr, int nDamageType );

private:
	void CreateSounds();
	void DestroySounds();
	void DoMuzzleFlash( );
	void StartFiring();
	void StopFiring();

	CSoundPatch *m_pGunFiringSound;
    float		m_flNextHeavyShotTime;
	bool		m_bIsFiring;

	[[= ks::reflect::Key{ .name = "airboat_gun_model" } ]] string_t	m_iszAirboatGunModel;
	CHandle<CBaseAnimating> m_hAirboatGunModel;
	int			m_nGunBarrelAttachment;
	float		m_flLastImpactEffectTime;
};


//-----------------------------------------------------------------------------
// Save/load: 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFuncTankAirboatGun )

LINK_ENTITY_TO_CLASS( func_tankairboatgun, CFuncTankAirboatGun );


//-----------------------------------------------------------------------------
// Precache: 
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::Precache( void )
{
	BaseClass::Precache();
	PrecacheScriptSound( "Airboat.FireGunLoop" );
	PrecacheScriptSound( "Airboat.FireGunRevDown");
	CreateSounds();
}


//-----------------------------------------------------------------------------
// Precache: 
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::Spawn( void )
{
	BaseClass::Spawn();
	m_flNextHeavyShotTime = 0.0f;
	m_bIsFiring = false;
	m_flLastImpactEffectTime = -1;
}


//-----------------------------------------------------------------------------
// Attachment indices
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::Activate()
{
	BaseClass::Activate();

	if ( m_iszAirboatGunModel != NULL_STRING )
	{
		m_hAirboatGunModel = dynamic_cast<CBaseAnimating*>( gEntList.FindEntityByName( NULL, m_iszAirboatGunModel, NULL ) );
		if ( m_hAirboatGunModel )
		{
			m_nGunBarrelAttachment = m_hAirboatGunModel->LookupAttachment( "muzzle" );
		}
	}
}


//-----------------------------------------------------------------------------
// Create/destroy looping sounds 
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::CreateSounds()
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	CPASAttenuationFilter filter( this );
	if (!m_pGunFiringSound)
	{
		m_pGunFiringSound = controller.SoundCreate( filter, entindex(), "Airboat.FireGunLoop" );
		controller.Play( m_pGunFiringSound, 0, 100 );
	}
}

void CFuncTankAirboatGun::DestroySounds()
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	controller.SoundDestroy( m_pGunFiringSound );
	m_pGunFiringSound = NULL;
}


//-----------------------------------------------------------------------------
// Stop Firing
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::StartFiring()
{
	if ( !m_bIsFiring )
	{
		CSoundEnvelopeController *pController = &CSoundEnvelopeController::GetController();
		float flVolume = pController->SoundGetVolume( m_pGunFiringSound );
		pController->SoundChangeVolume( m_pGunFiringSound, 1.0f, 0.1f * (1.0f - flVolume) );
		m_bIsFiring = true;
	}
}

void CFuncTankAirboatGun::StopFiring()
{
	if ( m_bIsFiring )
	{
		CSoundEnvelopeController *pController = &CSoundEnvelopeController::GetController();
		float flVolume = pController->SoundGetVolume( m_pGunFiringSound );
		pController->SoundChangeVolume( m_pGunFiringSound, 0.0f, 0.1f * flVolume );
		EmitSound( "Airboat.FireGunRevDown" );
		m_bIsFiring = false;
	}
}


//-----------------------------------------------------------------------------
// Maintains airboat gun sounds
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::ControllerPostFrame( void )
{
	if ( IsPlayerManned() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer*>( GetController() );
		if ( pPlayer->m_nButtons & IN_ATTACK )
		{
			StartFiring();
		}
		else
		{
			StopFiring();
		}
	}

	BaseClass::ControllerPostFrame();
}


//-----------------------------------------------------------------------------
// Stop controlled
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::OnStopControlled()
{
	StopFiring();
	BaseClass::OnStopControlled();
}


//-----------------------------------------------------------------------------
// Barrel position
//-----------------------------------------------------------------------------
Vector CFuncTankAirboatGun::WorldBarrelPosition( void )
{
	if ( !m_hAirboatGunModel || (m_nGunBarrelAttachment == 0) )
	{
		return BaseClass::WorldBarrelPosition();
	}

	Vector vecOrigin;
	QAngle vecAngles;
	m_hAirboatGunModel->GetAttachment( m_nGunBarrelAttachment, vecOrigin, vecAngles );
	return vecOrigin;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CFuncTankAirboatGun::GetTracerType( void ) 
{
	if ( gpGlobals->curtime >= m_flNextHeavyShotTime )
		return "AirboatGunHeavyTracer";

	return "AirboatGunTracer"; 
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::DoMuzzleFlash( void )
{
	if ( m_hAirboatGunModel && (m_nGunBarrelAttachment != 0) )
	{
		CEffectData data;
		data.m_nEntIndex = m_hAirboatGunModel->entindex();
		data.m_nAttachmentIndex = m_nGunBarrelAttachment;
		data.m_flScale = 1.0f;
		DispatchEffect( "AirboatMuzzleFlash", data );
	}
}


//-----------------------------------------------------------------------------
// Allows the shooter to change the impact effect of his bullets
//-----------------------------------------------------------------------------
void CFuncTankAirboatGun::DoImpactEffect( trace_t &tr, int nDamageType )
{
	// The airboat spits out so much crap that we need to do cheaper versions
	// of the impact effects. Also, we need to do less of them.
	if ( m_flLastImpactEffectTime == gpGlobals->curtime )
		return;

	m_flLastImpactEffectTime = gpGlobals->curtime;
	UTIL_ImpactTrace( &tr, nDamageType, "AirboatGunImpact" );
} 


//-----------------------------------------------------------------------------
// Fires bullets
//-----------------------------------------------------------------------------
#define AIRBOAT_GUN_HEAVY_SHOT_INTERVAL	0.2f

void CFuncTankAirboatGun::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
	CAmmoDef *pAmmoDef = GetAmmoDef();
	int ammoType = pAmmoDef->Index( "AirboatGun" );

	FireBulletsInfo_t info;
	info.m_vecSrc = barrelEnd;
	info.m_vecDirShooting = forward;
	info.m_flDistance = 4096;
	info.m_iAmmoType = ammoType;

	if ( gpGlobals->curtime >= m_flNextHeavyShotTime )
	{
		info.m_iShots = 1;
		info.m_vecSpread = VECTOR_CONE_PRECALCULATED;
		info.m_flDamageForceScale = 1000.0f;
	}
	else
	{
		info.m_iShots = 2;
		info.m_vecSpread = VECTOR_CONE_5DEGREES;
	}

	FireBullets( info );

	DoMuzzleFlash();

	// NOTE: This must occur after FireBullets
	if ( gpGlobals->curtime >= m_flNextHeavyShotTime )
	{
		m_flNextHeavyShotTime = gpGlobals->curtime + AIRBOAT_GUN_HEAVY_SHOT_INTERVAL; 
	}
}


//-----------------------------------------------------------------------------
// APC Rocket 
//-----------------------------------------------------------------------------
#define DEATH_VOLLEY_MISSILE_COUNT 10
#define DEATH_VOLLEY_MIN_FIRE_RATE 3
#define DEATH_VOLLEY_MAX_FIRE_RATE 6

class CFuncTankAPCRocket : public CFuncTank
{
public:
	DECLARE_CLASS( CFuncTankAPCRocket, CFuncTank );

	void Precache( void );
	virtual void Spawn();
	virtual void UpdateOnRemove();
	void Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker );
	virtual void Think();
	virtual float GetShotSpeed() { return m_flRocketSpeed; }

protected:
	[[= ks::reflect::Input{ .name = "DeathVolley", .type = FIELD_VOID } ]] void InputDeathVolley( inputdata_t &inputdata );
	void FireDying( const Vector &barrelEnd );

	EHANDLE	m_hLaserDot;
	[[= ks::reflect::Key{ .name = "rocketspeed" } ]] float	m_flRocketSpeed;
	int 	m_nSide;
	[[= ks::reflect::Key{ .name = "burstcount" } ]] int		m_nBurstCount;
	bool	m_bDying;

	DECLARE_DATADESC();
};


IMPLEMENT_REFLECT_DATAMAP( CFuncTankAPCRocket )

LINK_ENTITY_TO_CLASS( func_tankapcrocket, CFuncTankAPCRocket );

void CFuncTankAPCRocket::Precache( void )
{
	UTIL_PrecacheOther( "apc_missile" );

	PrecacheScriptSound( "PropAPC.FireCannon" );

	CFuncTank::Precache();
}

void CFuncTankAPCRocket::Spawn( void )
{
	BaseClass::Spawn();
	AddEffects( EF_NODRAW );
	m_nSide = 0;
	m_bDying = false;
	m_nBulletCount = m_nBurstCount;
	SetSolid( SOLID_NONE );
	SetLocalVelocity( vec3_origin );
}

void CFuncTankAPCRocket::UpdateOnRemove( void )
{
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
	BaseClass::UpdateOnRemove();
}

void CFuncTankAPCRocket::FireDying( const Vector &barrelEnd )
{
}

void CFuncTankAPCRocket::Fire( int bulletCount, const Vector &barrelEnd, const Vector &forward, CBaseEntity *pAttacker )
{
}

void CFuncTankAPCRocket::Think()
{
	// Inert if we're carried...
	if ( GetMoveParent() && GetMoveParent()->GetMoveParent() )
	{
		SetNextThink( gpGlobals->curtime + 0.5f );
		return;
	}

	BaseClass::Think();
	if ( m_bDying )
	{
		FireDying( WorldBarrelPosition() );
		return;
	}
}


void CFuncTankAPCRocket::InputDeathVolley( inputdata_t &inputdata )
{
	if ( !m_bDying )
	{
		m_fireRate = random->RandomFloat( DEATH_VOLLEY_MIN_FIRE_RATE, DEATH_VOLLEY_MAX_FIRE_RATE );
		SetNextAttack( gpGlobals->curtime + (1.0f / m_fireRate ) );
		m_nBulletCount = DEATH_VOLLEY_MISSILE_COUNT;
		m_bDying = true;
	}
}

