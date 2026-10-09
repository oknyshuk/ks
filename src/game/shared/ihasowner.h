#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

class CBaseEntity;

//-----------------------------------------------------------------------------
// Purpose: Allows an entity to access its owner regardless of entity type 
//-----------------------------------------------------------------------------
class IHasOwner
{
public:
	virtual CBaseEntity			*GetOwnerViaInterface( void ) = 0;
};
