// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core;
import ks.math;
import ks.tier1;
#include "vphysics_interface.h"

// cl_extrapolate_amount is the client's ConVar: it registers with the client, like the rest of the client.
static const bool s_bEnterClientRegistry = ks::enter_registry( ks::client_registry() );


// warning C4660: template-class specialization 'CInterpolatedVar<float>' is already instantiated

template class CInterpolatedVar<Vector>;
template class CInterpolatedVar<QAngle>;

CInterpolationContext *CInterpolationContext::s_pHead = nullptr;
bool CInterpolationContext::s_bAllowExtrapolation = false;
float CInterpolationContext::s_flLastTimeStamp = 0;

float g_flLastPacketTimestamp = 0;
bool g_bHermiteFix = true;


ConVar cl_extrapolate_amount( "cl_extrapolate_amount", "0.25", FCVAR_CHEAT, "Set how many seconds the client will extrapolate entities for." );

static const bool s_bLeaveClientRegistry = ks::leave_registry();
