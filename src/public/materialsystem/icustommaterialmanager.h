#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation
import ks.tier1;

// Purpose: Provide interface to custom material manager

#include "materialsystem/icustommaterial.h"
#include "materialsystem/icompositetexturegenerator.h"

class IVisualsDataCompare;

class ICustomMaterialManager
{
public:
	virtual bool Process() = 0;

	// get or create a custom material with the given attributes
	virtual ICustomMaterial *GetOrCreateCustomMaterial( KeyValues *pKeyValues, const std::vector< SCompositeTextureInfo > &vecTextureInfos, bool bIgnorePicMip = false ) = 0;

	virtual int DebugGetNumActiveCustomMaterials() = 0;
	virtual bool GetVMTKeyValues( const char *pszVMTName, KeyValues **ppVMTKeyValues ) = 0;
};
