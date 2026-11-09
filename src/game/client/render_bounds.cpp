// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The engine and the shared headers call DefaultRenderBoundsWorldspace(); the game client decides
// what it does (clientleafsystem.cpp).

import std;
import ks.core;
import ks.math;
import ks.tier1;
#include "iclientrenderable.h"

void (*g_pfnClientRenderBoundsWorldspace)( IClientRenderable *pRenderable, Vector &absMins, Vector &absMaxs ) = nullptr;

void DefaultRenderBoundsWorldspace( IClientRenderable *pRenderable, Vector &absMins, Vector &absMaxs )
{
	g_pfnClientRenderBoundsWorldspace( pRenderable, absMins, absMaxs );
}
