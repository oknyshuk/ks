// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interpolatedvar;
#include "vphysics_interface.h"



// warning C4660: template-class specialization 'CInterpolatedVar<float>' is already instantiated

template class CInterpolatedVar<Vector>;
template class CInterpolatedVar<QAngle>;

CInterpolationContext *CInterpolationContext::s_pHead = nullptr;
bool CInterpolationContext::s_bAllowExtrapolation = false;
float CInterpolationContext::s_flLastTimeStamp = 0;

float g_flLastPacketTimestamp = 0;
bool g_bHermiteFix = true;

