#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2002 Valve Corporation

#include "lightmappedgeneric_dx9_helper.h"

void DrawLightmappedPaint_DX9( CBaseVSShader *pShader, IMaterialVar** params, 
								 IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, 
								 LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr );


void DrawLightmappedPaint_DX9_FastPath( CBaseVSShader *pShader, IMaterialVar** params, 
							  IShaderDynamicAPI *pShaderAPI, LightmappedGeneric_DX9_Vars_t &info, CBasePerMaterialContextData **pContextDataPtr );
