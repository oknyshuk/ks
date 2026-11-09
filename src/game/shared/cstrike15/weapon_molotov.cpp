// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#endif
#include "reflect_predmap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "weapon_csbase.h"
#include "gamerules.h"
#include "engine/IEngineSound.h"
#include "weapon_molotov.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
	#include "items.h"
	#include "molotov_projectile.h"
	#include "Effects/inferno.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( MolotovGrenade, DT_MolotovGrenade )

IMPLEMENT_REFLECT_TABLE( CMolotovGrenade, DT_MolotovGrenade );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CMolotovGrenade );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_molotov, MolotovGrenade );
PRECACHE_REGISTER( weapon_molotov );

#if !defined( CLIENT_DLL )


void CMolotovGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	// [mlowrance] were throwing the grenade, be sure to remove flame sound effect
	SetLoopingSoundPlaying( false );
	StopSound( "Molotov.IdleLoop" ); 
	//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 2\n" );
	CMolotovProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

void CMolotovGrenade::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
	
	PrecacheScriptSound( "Molotov.IdleLoop" );
}

#else // GAME_DLL

static int s_nMolotovFire;
PRECACHE_REGISTER_BEGIN( GLOBAL, CMolotovGrenade )
	PRECACHE_INDEX( PARTICLE_SYSTEM, "weapon_molotov_held", s_nMolotovFire );
PRECACHE_REGISTER_END()

void CMolotovGrenade::UpdateParticles( void )
{
	// FIXME: This is bogus; we need to make the particle property have particle system types:
	// first person, third person, owner, and logic in the particle property to know whether
	// to render a given system given these rules and knowledge of what mode the owner is in
	C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	int nRenderFlags = 0;

	CWeaponCSBase *pCSWeapon = (CWeaponCSBase*)pPlayer->GetActiveWeapon();
	if ( !pCSWeapon )
		return;

	int iWeaponId = pCSWeapon->GetCSWeaponID();

	if ( iWeaponId == WEAPON_MOLOTOV )
	{
		bool bIsFirstOrThirdpersonMolotovVisible = pCSWeapon->IsWeaponVisible();
		
		CBaseAnimating *pWeaponBaseAnimating = pCSWeapon->GetBaseAnimating();

		CBaseWeaponWorldModel *pWeaponWorldModel = pCSWeapon->GetWeaponWorldModel();
		if ( pWeaponWorldModel && pWeaponWorldModel->ShouldDraw() )
		{
			pWeaponBaseAnimating = pWeaponWorldModel->GetBaseAnimating();
			bIsFirstOrThirdpersonMolotovVisible = true;
		}

		if ( m_molotovParticleEffect.IsValid() ) 
		{
//			m_molotovParticleEffect->SetDormant( pPlayer->GetPlayerAnimState()->ShouldHideWeapon() ); // ShouldHideWeapon is a Terror Codebase function, not CStrike15
			m_molotovParticleEffect->SetDormant( !bIsFirstOrThirdpersonMolotovVisible ); // Is the weapon Hidden?
		}

		if ( bIsFirstOrThirdpersonMolotovVisible )
		{	
			if ( m_bPinPulled )
			{
				if ( !m_molotovParticleEffect() )
				{
					// TEST: [mlowrance] This is to test for attachment.
					int iAttachment = -1;
					if ( pWeaponBaseAnimating )
						iAttachment = pWeaponBaseAnimating->LookupAttachment( "Wick" );

					if ( iAttachment >= 0 )
					{
						// FIXME: Precache 'Wick' attachment index
						m_molotovParticleEffect = pWeaponBaseAnimating->ParticleProp()->CreatePrecached( s_nMolotovFire, PATTACH_POINT_FOLLOW, iAttachment );
						EmitSound( "Molotov.IdleLoop" );
						SetLoopingSoundPlaying( true );

						//DevMsg( 1, "++++++++++>Playing Molotov.IdleLoop 1\n" );

					}
				}

				if ( m_molotovParticleEffect() )
				{
					if ( nRenderFlags != 3 )
					{
						m_molotovParticleEffect->SetDrawOnlyForSplitScreenUser( nRenderFlags - 1 );
					}
					else
					{
						m_molotovParticleEffect->SetDrawOnlyForSplitScreenUser( -1 );
					}
				}
			}
		}
	}
	else
	{
		if ( m_molotovParticleEffect.IsValid() )
		{
			StopSound( "Molotov.IdleLoop" );
			//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 1\n" );
			m_molotovParticleEffect->StopEmission( false, false );
			m_molotovParticleEffect->SetRemoveFlag();
			m_molotovParticleEffect = nullptr;
		}
	}
}

bool CMolotovGrenade::Simulate( void )
{
	UpdateParticles();
	return BaseClass::Simulate();
}

//--------------------------------------------------------------------------------------------------------
void CMolotovGrenade::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_molotovParticleEffect() == pParticleEffect )
	{
		m_molotovParticleEffect = nullptr;
	}
}

#endif // !CLIENT_DLL

void CMolotovGrenade::Drop(const Vector& vecVelocity)
{
	CBaseCSGrenade::Drop(vecVelocity);
	StopSound( "Molotov.IdleLoop" ); 
	SetLoopingSoundPlaying( false );
}


IMPLEMENT_NETWORKCLASS_ALIASED( IncendiaryGrenade, DT_IncendiaryGrenade )

	IMPLEMENT_REFLECT_TABLE( CIncendiaryGrenade, DT_IncendiaryGrenade );

	//BEGIN_PREDICTION_DATA( CIncendiaryGrenade )
	//END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_incgrenade, IncendiaryGrenade );
PRECACHE_REGISTER( weapon_incgrenade );

#if !defined( CLIENT_DLL )

void CIncendiaryGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	// [mlowrance] were throwing the grenade, be sure to remove flame sound effect
	SetLoopingSoundPlaying( false );
	StopSound( "Molotov.IdleLoop" ); 
	//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 2\n" );
	CMolotovProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

void CIncendiaryGrenade::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	PrecacheScriptSound( "Molotov.IdleLoop" );
}

#else // GAME_DLL

#endif // !CLIENT_DLL
