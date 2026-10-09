#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Expose the IVEngineServer implementation to engine code.

#include "eiface.h"

// The engine can call its own exposed functions in here rather than 
// splitting them into naked functions and sharing.
extern IVEngineServer *g_pVEngineServer;

// Used to seed the random # stream
void SeedRandomNumberGenerator( bool random_invariant );

void InvalidateSharedEdictChangeInfos();
