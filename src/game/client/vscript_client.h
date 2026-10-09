#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2008 Valve Corporation

#include "vscript/ivscript.h"
#include "vscript_shared.h"

extern IScriptVM * g_pScriptVM;

// Only allow scripts to create entities during map initialization
bool IsEntityCreationAllowedInScripts( void );
