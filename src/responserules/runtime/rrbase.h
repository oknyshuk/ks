#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#ifdef _DEBUG
#define DEBUG 1
#endif

// Misc C-runtime library headers
import std;

// tier 0

// tier 1
import ks.tier1;

// tier 2
#include "string_t.h"

// Shared engine/DLL constants
#include "const.h"
#include "edict.h"

// app

#include "responserules/response_types.h"
#include "responserules/response_types_internal.h"
#include "responserules/response_host_interface.h"
