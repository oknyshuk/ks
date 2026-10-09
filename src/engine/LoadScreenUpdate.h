#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.

#include "materialsystem/imaterialsystem.h"

//-----------------------------------------------------------------------------
// Activate, deactivate loader updates
//-----------------------------------------------------------------------------
void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode );
void RefreshScreenIfNecessary();
void EndLoadingUpdates();
void PauseLoadingUpdates( bool bPause );
