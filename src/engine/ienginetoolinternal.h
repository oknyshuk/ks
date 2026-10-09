#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Methods of IEngineTool visible only inside the engine

#include "toolframework/ienginetool.h"

//-----------------------------------------------------------------------------
// Purpose: Singleton implementation of external tools callback interface
//-----------------------------------------------------------------------------
class IEngineToolInternal : public IEngineTool
{
public:
	virtual void	SetIsInGame( bool bIsInGame ) = 0;
};

extern IEngineToolInternal *g_pEngineToolInternal;
