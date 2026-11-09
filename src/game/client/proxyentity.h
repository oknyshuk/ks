#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A base class for all material proxies in the client dll

#include "materialsystem/imaterialproxy.h"

class C_BaseEntity;


//-----------------------------------------------------------------------------
// Base class all material proxies should inherit from
//-----------------------------------------------------------------------------
class CEntityMaterialProxy : public IMaterialProxy
{
public:
	virtual void Release( void );
	void OnBind( void *pC_BaseEntity );

protected:
	// base classes should implement these
	virtual void OnBind( C_BaseEntity *pBaseEntity ) = 0;
};
