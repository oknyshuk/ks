#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "baseparticleentity.h"

inline constexpr const char *PARTICLESMOKEGRENADE_ENTITYNAME = "env_particlesmokegrenade";

inline constexpr auto MIN_SMOKE_TINT = 0.5f;
inline constexpr auto MAX_SMOKE_TINT = 0.6f;

class [[= ks::reflect::NetTable{ .name = "DT_ParticleSmokeGrenade" } ]]
      ParticleSmokeGrenade : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( ParticleSmokeGrenade, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

						ParticleSmokeGrenade();
	
	virtual				~ParticleSmokeGrenade();

	virtual void		Spawn( void );
	virtual int			UpdateTransmitState( void );
	void				SetCreator(CBasePlayer *creator);
	CBasePlayer*		GetCreator();

public:

	// Tell the client entity to start filling the volume.
	void				FillVolume();

	// Set the times it fades out at.
	void				SetFadeTime(float startTime, float endTime);

	// Set time to fade out relative to current time
	void				SetRelativeFadeTime(float startTime, float endTime);

	// Set the tint color of the grenade smoke.
	void				SetSmokeColor(Vector color);

	void				Think( void );

public:
	
	// Stage 0 (default): make a smoke trail that follows the entity it's following.
	// Stage 1          : fill a volume with smoke.
	CNetworkVar( unsigned char, m_CurrentStage, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( float, m_flSpawnTime, [[= ks::reflect::Net{} ]] );

	// When to fade in and out.
	CNetworkVar( float, m_FadeStartTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_FadeEndTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( Vector, m_MinColor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( Vector, m_MaxColor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

protected:
    
	EHANDLE m_creatorPlayer;
};
