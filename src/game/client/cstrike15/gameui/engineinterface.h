#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Includes all the headers/declarations necessary to access the
//			engine interface

// these stupid set of includes are required to use the cdll_int interface
//#include "wrect.h"

// engine interface
#include "cdll_int.h"
#include "eiface.h"
#include "tier2/tier2.h"
#include "matchmaking/imatchframework.h"

// engine interface singleton accessors
extern IVEngineClient *engine;
extern class IBik *bik;
extern class IEngineUI *engineuifuncs;
extern class IGameUIFuncs *gameuifuncs;
extern class IEngineSound *enginesound;
extern class IAchievementMgr *achievementmgr; 
