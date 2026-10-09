#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "vcollide_parse.h"

// solid_t parsing
class CSolidSetDefaults : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) {}
	virtual void SetDefaults( void *pData );
};

extern CSolidSetDefaults g_SolidSetup;
