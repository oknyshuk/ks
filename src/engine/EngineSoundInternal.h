#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "engine/IEngineSound.h"

//-----------------------------------------------------------------------------
// Method to get at the singleton implementations of the engine sound interfaces
//-----------------------------------------------------------------------------
IEngineSound* EngineSoundClient();
IEngineSound* EngineSoundServer();
