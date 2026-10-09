#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
class CBaseEntity;

void DoSpark( CBaseEntity *ent, const Vector &location, int nMagnitude, int nTrailLength, bool bPlaySound, const Vector &vecDir );
