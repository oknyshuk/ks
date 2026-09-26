//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "shadersystem.h"
#include <malloc.h>
#include "filesystem.h"

int g_FrameNum;

#ifndef SHADERAPI_GLOBAL_H
IShaderAPI *g_pShaderAPI = 0;
IShaderDeviceMgr* g_pShaderDeviceMgr = 0;
IShaderDevice *g_pShaderDevice = 0;
IShaderShadow* g_pShaderShadow = 0;
#endif

IClientMaterialSystem *g_pClientMaterialSystem = 0;
