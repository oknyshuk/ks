#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "vphysics_interface.h"
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;

struct solid_t
{
	char	name[512];
	char	parent[512];
	char	surfaceprop[512];
	Vector	massCenterOverride;
	int		index;
	int		contents;
	objectparams_t params;
};

struct fluid_t
{
	int index;
	char surfaceprop[512];

	fluidparams_t params;

	fluid_t() {}
	fluid_t( fluid_t const& src ) : params(src.params)
	{
		index = src.index;
	}
};

struct ragdollcollisionrules_t
{
	void Defaults( IPhysics *pPhysics, IPhysicsCollisionSet *pSetIn )
	{
		pCollisionSet = pSetIn;
		bSelfCollisions = true;
	}
	int	   bSelfCollisions;
	IPhysicsCollisionSet *pCollisionSet;
};

struct ragdollanimatedfriction_t
{
	float minFriction;
	float maxFriction;
	float timeIn;
	float timeOut;
	float timeHold;
};

//-----------------------------------------------------------------------------
// Purpose: Pass this into the parser to handle the keys that vphysics does not
// parse.
//-----------------------------------------------------------------------------
class IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) = 0;
	virtual void SetDefaults( void *pData ) = 0;
};


class IVPhysicsKeyParser
{
public:

	const char *GetCurrentBlockName( void );
	bool		Finished( void );
	void		ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseSurfaceTable( int *table, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler );
	void		SkipBlock( void );
	void		ParseCollisionRules( ragdollcollisionrules_t *pRules, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseRagdollAnimatedFriction( ragdollanimatedfriction_t *pFriction, IVPhysicsKeyHandler *unknownKeyHandler	);
};
