// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: random stream class

import std;
import ks.core.random;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
#include "cdll_int.h"

//-----------------------------------------------------------------------------
// The client and the game draw from the library's global uniform stream, the same one the
// Random*() functions use.
//-----------------------------------------------------------------------------
static InterfaceReg s_Reg_GlobalUniformRandomStream( []() -> void * { return &GlobalUniformRandomStream(); }, VENGINE_CLIENT_RANDOM_INTERFACE_VERSION );
