// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Loading-screen update entry points. They do nothing: the loading screen is not
// refreshed while loading.

import ks.core;
#include "LoadScreenUpdate.h"
import std;
#include "tier2/tier2.h"
#include "materialsystem/imaterialsystem.h"

static bool s_bLoadingUpdatesEnabled = true;

void AbortLoadingUpdatesDueToShutdown()
{
	s_bLoadingUpdatesEnabled = false;
}

void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode )
{
}

void RefreshScreenIfNecessary()
{
}

void PauseLoadingUpdates( bool bPause )
{
}

void EndLoadingUpdates()
{
}
