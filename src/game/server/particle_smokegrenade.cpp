// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "particle_smokegrenade.h"

IMPLEMENT_REFLECT_SERVERCLASS( ParticleSmokeGrenade, DT_ParticleSmokeGrenade )

LINK_ENTITY_TO_CLASS( env_particlesmokegrenade, ParticleSmokeGrenade );



ParticleSmokeGrenade::ParticleSmokeGrenade()
{
	m_CurrentStage = 0;
	m_FadeStartTime = 17;
	m_FadeEndTime = 22;
	
	m_MinColor = Vector(MIN_SMOKE_TINT, MIN_SMOKE_TINT, MIN_SMOKE_TINT);
	m_MaxColor = Vector(MAX_SMOKE_TINT, MAX_SMOKE_TINT, MAX_SMOKE_TINT);

	m_flSpawnTime = gpGlobals->curtime;
}

ParticleSmokeGrenade::~ParticleSmokeGrenade()
{
}

// Smoke grenade particles should always transmitted to clients.  If not, a client who
// enters the PVS late will see the smoke start billowing from then, allowing better vision.
int ParticleSmokeGrenade::UpdateTransmitState_Impl( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return SetTransmitState( FL_EDICT_DONTSEND );

	return SetTransmitState( FL_EDICT_ALWAYS );
}

void ParticleSmokeGrenade::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

	SetNextThink( gpGlobals->curtime );
	m_creatorPlayer = nullptr;
}

void ParticleSmokeGrenade::SetCreator(CBasePlayer *creator)
{
	m_creatorPlayer = creator;
}

CBasePlayer* ParticleSmokeGrenade::GetCreator()
{
	return (CBasePlayer*)m_creatorPlayer.Get();
}

void ParticleSmokeGrenade::FillVolume()
{
	m_CurrentStage = 1;
	CollisionProp()->SetCollisionBounds( Vector( -50, -50, -50 ), Vector( 50, 50, 50 ) );
}


void ParticleSmokeGrenade::SetFadeTime(float startTime, float endTime)
{
	m_FadeStartTime = startTime;
	m_FadeEndTime = endTime;
}

// Fade start and end are relative to current time
void ParticleSmokeGrenade::SetRelativeFadeTime(float startTime, float endTime)
{
	float flCurrentTime = gpGlobals->curtime - m_flSpawnTime;

	m_FadeStartTime = flCurrentTime + startTime;
	m_FadeEndTime = flCurrentTime + endTime;
}

// Fade start and end are relative to current time
void ParticleSmokeGrenade::SetSmokeColor( Vector color )
{
	m_MinColor = color * MIN_SMOKE_TINT;
	m_MaxColor = color * MAX_SMOKE_TINT;
}

void ParticleSmokeGrenade::Think_Impl()
{
	// Override, don't extend.  (Baseclass's Think just deletes.)

	float now = gpGlobals->curtime;
	if( now >= (m_flSpawnTime + m_FadeEndTime) )
	{
		// We are done
		UTIL_Remove(this);
		return;
	}

	const float PSG_THINK_DELAY = 1.0f;
	SetNextThink(now + PSG_THINK_DELAY);
}
