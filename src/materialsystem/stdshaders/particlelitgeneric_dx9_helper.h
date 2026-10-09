#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;

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
struct ParticleLitGeneric_DX9_Vars_t
{												
	ParticleLitGeneric_DX9_Vars_t() { std::memset( this, 0xFF, sizeof(ParticleLitGeneric_DX9_Vars_t) ); }

	int m_nBaseTexture;
	int m_nBaseTextureFrame;
	int m_nBaseTextureTransform;
	int m_nBumpmap;
	int m_nBumpFrame;
	int m_nBumpTransform;
	int m_nAlphaTestReference;
	int m_nFlashlightTexture;
	int m_nFlashlightTextureFrame;
};

void InitParamsParticleLitGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, ParticleLitGeneric_DX9_Vars_t &info );
void InitParticleLitGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, ParticleLitGeneric_DX9_Vars_t &info );
void DrawParticleLitGeneric_DX9( CBaseVSShader *pShader, IMaterialVar** params, 
	IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow, ParticleLitGeneric_DX9_Vars_t &info );
