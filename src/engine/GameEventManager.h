#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: gameeventmanager.h: the engine's CGameEventManager instance. The class itself
// lives in public/igameevents.h so that client and server can call it directly.

#if !defined ( GAMEEVENTMANAGER_H )
#define GAMEEVENTMANAGER_H

#include <igameevents.h>
import std;
import ks.tier1;
#include <networkstringtabledefs.h>
#include "netmessages.h"

extern CGameEventManager &g_GameEventManager;

#endif 
