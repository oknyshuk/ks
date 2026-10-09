// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: TF2 specific input handling

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#include "kbutton.h"
#include "input.h"

//-----------------------------------------------------------------------------
// Purpose: TF Input interface
//-----------------------------------------------------------------------------
class CCSInput : public CInput
{
public:
};

static CCSInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;
