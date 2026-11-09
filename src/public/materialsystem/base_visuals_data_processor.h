#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation

// Purpose: Provide custom texture generation (compositing)

import ks.tier1;
#include "ivisualsdataprocessor.h"

import std;

enum MaterialParamID_t
{
	MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE = 0,
	MATERIAL_PARAM_ID_PHONG_EXPONENT_TEXTURE,
	MATERIAL_PARAM_ID_BUMP_MAP,
	MATERIAL_PARAM_ID_ANISOTROPY_MAP,
	MATERIAL_PARAM_ID_MASKS1_MAP,

	MATERIAL_PARAM_ID_COUNT
};

extern const char g_szMaterialParamNames[MATERIAL_PARAM_ID_COUNT][32];
