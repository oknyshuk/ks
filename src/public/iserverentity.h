#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.math;

#include "iserverunknown.h"
#include "string_t.h"

struct Ray_t;
class ServerClass;
class ICollideable;
class IServerNetworkable;

// This class is how the engine talks to entities in the game DLL.
// CBaseEntity implements this interface.
class IServerEntity	: public IServerUnknown
{
public:
	virtual					~IServerEntity() {}

// Previously in pev
	virtual int				GetModelIndex( void ) const = 0;
 	virtual string_t		GetModelName( void ) const = 0;

	virtual void			SetModelIndex( int index ) = 0;
};
