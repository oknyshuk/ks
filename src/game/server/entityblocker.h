#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "baseentity.h"

//==================================================================================================================
// Entity Blocker
//==================================================================================================================
class CEntityBlocker : public CBaseEntity
{
	DECLARE_CLASS( CEntityBlocker, CBaseEntity );

public:

	static CEntityBlocker *Create( const Vector &origin, const Vector &mins, const Vector &maxs, CBaseEntity *pOwner = nullptr, bool bBlockPhysics = false );

	void Spawn_Impl( void );
	bool TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );
};
