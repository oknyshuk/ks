#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "igamesystem.h"
#include "datacache/iprecachesystem.h"
#include "tier2/tier2.h"

//-----------------------------------------------------------------------------
// Responsible for kicking off the precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheRegister : public IGameSystem
{
	// Inherited from IGameSystem
public:
	virtual char const *Name() { return "PrecacheRegister"; }
	virtual bool IsPerFrame() { return false; }
	virtual bool Init();
	virtual void PostInit() {}
	virtual void Shutdown() {}
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity();
	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	// Other public methods
public:
	// constructor, destructor
	CPrecacheRegister() {}
	virtual ~CPrecacheRegister() {}

private:
};


//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
extern IPrecacheHandler *g_pPrecacheHandler;
extern CPrecacheRegister *g_pPrecacheRegister;
