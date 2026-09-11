//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RRBASE_H
#define RRBASE_H


#ifdef _DEBUG
#define DEBUG 1
#endif

// Misc C-runtime library headers
#include <math.h>
#include <ctype.h>
#include <stdio.h>

// tier 0
#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "basetypes.h"

// tier 1
#include "tier1/strtools.h"
#include "utlvector.h"
#include "utlsymbol.h"

// tier 2
#include "string_t.h"

// Shared engine/DLL constants
#include "const.h"
#include "edict.h"

// app

#include "responserules/response_types.h"
#include "responserules/response_types_internal.h"
#include "responserules/response_host_interface.h"


#endif // CBASE_H
