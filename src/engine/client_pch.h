//========= Copyright � 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

// main precompiled header for client files

#pragma once

#include "platform.h"
#include "basetypes.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlsymbol.h"
#include "mathlib/mathlib.h"
#include "tier1/fmtstr.h"
#include "tier1/convar.h"

#include "common.h"
#include "qlimits.h"
#include "iprediction.h"
#include "icliententitylist.h"

#include "sysexternal.h"
#include "cmd.h"
#include "protocol.h"
#include "render.h"


#include "screen.h"

#include "gl_shader.h"

#include "cdll_engine_int.h"
#include "client_class.h"
#include "client.h"
#include "cl_main.h"
#include "cl_pred.h"

#include "con_nprint.h"
#include "debugoverlay.h"
#include "demo.h"
#include "host_state.h"
#include "host.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sys.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
