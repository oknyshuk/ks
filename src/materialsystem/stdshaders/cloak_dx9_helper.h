#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

import std;
#include "materialsystem/imaterialsystemhardwareconfig.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseVSShader;
class IMaterialVar;
class IShaderDynamicAPI;
class IShaderShadow;

//-----------------------------------------------------------------------------
// Init params/ init/ draw methods
//-----------------------------------------------------------------------------
struct Cloak_DX9_Vars_t
{
	Cloak_DX9_Vars_t() { std::memset( this, 0xFF, sizeof( *this ) ); }

	int m_nBaseTexture;
	int m_nRefractAmount;
	int m_nRefractTint;
	int m_nNormalMap;
	int m_nBumpFrame;
	int m_nBumpTransform;
	int m_nRefractTintTexture;
	int m_nRefractTintTextureFrame;
	int m_nFresnelReflection;
	int m_nMasked;
	int m_nCloakFactor;
	int m_nDiffuseWarpTexture;

	int m_nPhongExponent;
	int m_nPhongTint;
	int m_nPhongAlbedoTint;
	int m_nPhongExponentTexture;
	int m_nPhongBoost;
	int m_nPhongFresnelRanges;

	// Rim lighting parameters
	int m_nRimLight;
	int m_nRimLightPower;
	int m_nRimLightBoost;
	int m_nRimMask;
};

void InitParamsCloak_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, 
						   Cloak_DX9_Vars_t &info );
void InitCloak_DX9( CBaseVSShader *pShader, IMaterialVar** params, Cloak_DX9_Vars_t &info );
void DrawCloak_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				   IShaderShadow* pShaderShadow, Cloak_DX9_Vars_t &info, VertexCompressionType_t vertexCompression );
