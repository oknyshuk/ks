#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

//----------------------------------------------------------------------------------------

#include "engine/ireplayhistorymanager.h"

//----------------------------------------------------------------------------------------

extern IReplayHistoryManager *g_pClientReplayHistoryManager;
extern IReplayHistoryManager *g_pServerReplayHistoryManager;

IReplayHistoryManager *CreateServerReplayHistoryManager();

//----------------------------------------------------------------------------------------
