#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Singleton manager for spatial entities on the client

#include "igamesystem.h"
import std;

class C_SpatialEntity;


//------------------------------------------------------------------------------
// Purpose : Singleton manager for spatial entities on the client
//------------------------------------------------------------------------------
class CSpatialEntityMgr : public CAutoGameSystemPerFrame
{
	// Inherited from IGameSystemPerFrame
public:
	virtual char const *Name() { return "Spatial Entity Mgr"; }

	// Gets called each frame
	virtual void Update( float frametime );

	// Other public methods
public:

	// Create, destroy spatial entity
	void AddSpatialEntity( C_SpatialEntity *pSpatialEntity );
	void RemoveSpatialEntity( C_SpatialEntity *pSpatialEntity );

	std::vector< C_SpatialEntity* > m_SpatialEntities;
};
