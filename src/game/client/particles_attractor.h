#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "particles_simple.h"

class CParticleAttractor : public CSimpleEmitter
{
public:

	explicit CParticleAttractor( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}

	//Create
	static CParticleAttractor *Create( const Vector &center, const char *pDebugName = "attractor" );

	void	UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	float	UpdateScale( const SimpleParticle *pParticle );
	float	UpdateAlpha( const SimpleParticle *pParticle );
	void	SetAttractorOrigin( const Vector &origin );

private:

	CParticleAttractor( const CParticleAttractor & ); // not defined, not accessible

	Vector	m_vecAttractorOrigin;
};
