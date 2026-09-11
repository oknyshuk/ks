//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYHISTORYMANAGER_H
#define REPLAYHISTORYMANAGER_H

//----------------------------------------------------------------------------------------

#include "engine/ireplayhistorymanager.h"

//----------------------------------------------------------------------------------------

extern IReplayHistoryManager *g_pClientReplayHistoryManager;
extern IReplayHistoryManager *g_pServerReplayHistoryManager;

IReplayHistoryManager *CreateServerReplayHistoryManager();

//----------------------------------------------------------------------------------------

#endif // REPLAYHISTORYMANAGER_H
