#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "engine/ivmodelinfo.h"

extern IVModelInfo *modelinfo;			// server version
#ifndef DEDICATED
extern IVModelInfoClient *modelinfoclient;	// client version
#endif
