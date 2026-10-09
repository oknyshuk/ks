#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;

// angles comes from the "angles" property
//
// yaw and pitch will override the values in angles if they are nonzero
// yaw comes from the (obsolete) "angle" property
// pitch comes from the "pitch" property
void SetupLightNormalFromProps( const QAngle &angles, float yaw, float pitch, Vector &output );
