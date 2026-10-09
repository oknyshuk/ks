#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "entitylist.h"
#include "shareddefs.h"

inline constexpr auto SF_ENVEXPLOSION_NODAMAGE = 0x00000001; // when set, ENV_EXPLOSION will not actually inflict damage
inline constexpr auto SF_ENVEXPLOSION_REPEATABLE = 0x00000002; // can this entity be refired?
inline constexpr auto SF_ENVEXPLOSION_NOFIREBALL = 0x00000004; // don't draw the fireball
inline constexpr auto SF_ENVEXPLOSION_NOSMOKE = 0x00000008; // don't draw the smoke
inline constexpr auto SF_ENVEXPLOSION_NODECAL = 0x00000010; // don't make a scorch mark
inline constexpr auto SF_ENVEXPLOSION_NOSPARKS = 0x00000020; // don't make sparks
inline constexpr auto SF_ENVEXPLOSION_NOSOUND = 0x00000040; // don't play explosion sound.
inline constexpr auto SF_ENVEXPLOSION_RND_ORIENT = 0x00000080; // randomly oriented sprites
inline constexpr auto SF_ENVEXPLOSION_NOFIREBALLSMOKE = 0x0100;
inline constexpr auto SF_ENVEXPLOSION_NOPARTICLES = 0x00000200;
inline constexpr auto SF_ENVEXPLOSION_NODLIGHTS = 0x00000400;
inline constexpr auto SF_ENVEXPLOSION_NOCLAMPMIN = 0x00000800; // don't clamp the minimum size of the fireball sprite
inline constexpr auto SF_ENVEXPLOSION_NOCLAMPMAX = 0x00001000; // don't clamp the maximum size of the fireball sprite
inline constexpr auto SF_ENVEXPLOSION_SURFACEONLY = 0x00002000; // don't damage the player if he's underwater.
inline constexpr auto SF_ENVEXPLOSION_GENERIC_DAMAGE = 0x00004000; // don't do BLAST damage
inline constexpr auto SF_ENVEXPLOSION_ICE = 0x00008000; // freeze stuff and do ice type effects

extern int	g_sModelIndexFireball;
extern int	g_sModelIndexSmoke;

void ExplosionCreate( const Vector &center, const QAngle &angles, 
	CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, float flExplosionForce = 0.0f, bool bSurfaceOnly = false, bool bSilent = false, int iCustomDamageType = -1 );

void ExplosionCreate( const Vector &center, const QAngle &angles, 
					 CBaseEntity *pOwner, int magnitude, int radius, int nSpawnFlags, 
					 float flExplosionForce = 0.0f, CBaseEntity *pInflictor = nullptr, int iCustomDamageType = -1,  const EHANDLE *ignoredEntity = nullptr, Class_T ignoredClass = CLASS_NONE);

// this version lets you specify classes or entities to be ignored
void ExplosionCreate( const Vector &center, const QAngle &angles, 
					 CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, 
					 const EHANDLE *ignoredEntity, Class_T ignoredClass,
					 float flExplosionForce = 0.0f, bool bSurfaceOnly = false, bool bSilent = false, int iCustomDamageType = -1 );
