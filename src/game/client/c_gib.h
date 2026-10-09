#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


#include "reflect_annotations.h"
#include "dt_recv.h"
#include "client_class.h"
#include "c_baseanimating.h"

inline constexpr auto DEFAULT_GIB_LIFETIME = 4.0f;

// Base client gibs

class C_Gib : public C_BaseAnimating
{
	typedef C_BaseAnimating BaseClass;
public:

	~C_Gib( void );

	static C_Gib	*CreateClientsideGib( const char *pszModelName, Vector vecOrigin, Vector vecForceDir, AngularImpulse vecAngularImp, float flLifetime = DEFAULT_GIB_LIFETIME );
	
	bool	InitializeGib( const char *pszModelName, Vector vecOrigin, Vector vecForceDir, AngularImpulse vecAngularImp, float flLifetime = DEFAULT_GIB_LIFETIME );
	void	ClientThink( void );
	void	StartTouch( C_BaseEntity *pOther );

	virtual	void HitSurface( C_BaseEntity *pOther );

protected:

	float	m_flTouchDelta;		// Amount of time that must pass before another touch function can be called
};
