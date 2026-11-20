#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
#include "const.h"

extern bool g_bThreadedEngine;
extern int g_nMaterialSystemThread;
extern int g_nServerThread;

#define IsEngineThreaded() (g_bThreadedEngine)
