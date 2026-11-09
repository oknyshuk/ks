// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "shadersystem.h"

int g_FrameNum;

IShaderAPI *g_pShaderAPI = 0;
IShaderDeviceMgr* g_pShaderDeviceMgr = 0;
IShaderDevice *g_pShaderDevice = 0;
IShaderShadow* g_pShaderShadow = 0;

