#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.tier1;
#include "mathlib/softbodyenvironment.h"
#include "igamesystem.h"
import std;

#ifndef CLIENT_DLL
#error Softbodies should only be used on client
#endif

extern CSoftbodyEnvironment g_SoftbodyEnvironment; // in physics_softbody.cpp
extern ConVar softbody_debug; // in studio.cpp

class CSoftbodyProcess : public CAutoGameSystemPerFrame
{
public:
	CSoftbodyProcess() : CAutoGameSystemPerFrame( "SoftbodyProcess" ) {}
	virtual void OnRestore() override;
	virtual void Update( float frametime ) override;

	virtual void PostRender() override;
};
