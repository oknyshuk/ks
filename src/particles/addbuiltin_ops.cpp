// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

// Purpose: particle system code

import ks.platform;
import ks.simd;
#include "particles/particles.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void AddBuiltInParticleOperators( void );
void AddBuiltInParticleRenderers( void );
void AddBuiltInParticleInitializers( void );
void AddBuiltInParticleEmitters( void );
void AddBuiltInParticleForceGenerators( void );
void AddBuiltInParticleConstraints( void );

void CParticleSystemMgr::AddBuiltinSimulationOperators( void )
{
	static bool s_DidAddSim = false;
	if ( ! s_DidAddSim )
	{
		s_DidAddSim = true;
		AddBuiltInParticleOperators();
		AddBuiltInParticleInitializers();
		AddBuiltInParticleEmitters();
		AddBuiltInParticleForceGenerators();
		AddBuiltInParticleConstraints();
	}
}

void CParticleSystemMgr::AddBuiltinRenderingOperators( void )
{
	static bool s_DidAddRenderers = false;
	if ( ! s_DidAddRenderers )
	{
		s_DidAddRenderers = true;
		AddBuiltInParticleRenderers();
	}
}
