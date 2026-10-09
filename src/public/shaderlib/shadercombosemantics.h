#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Vertex/Pixel Shaders

struct ShaderComboInformation_t
{
	const char *m_pComboName;
	int m_nComboMin;
	int m_nComboMax;
};

struct ShaderComboSemantics_t
{
	const char *pShaderName;
	const ShaderComboInformation_t *pDynamicShaderComboArray;
	int nDynamicShaderComboArrayCount;
	const ShaderComboInformation_t *pStaticShaderComboArray;
	int nStaticShaderComboArrayCount;
};
