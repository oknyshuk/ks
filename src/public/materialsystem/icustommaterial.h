#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Provide interface to custom materials for entities that use them

import std;
import ks.tier1;

class IMaterial;
class ICompositeTexture;

// Non-virtual facade: the only implementation is the material system's
// CCustomMaterial, which is reference counted (CRefCounted<CCustomMaterial>).
class ICustomMaterial
{
public:
	int AddRef();
	int Release();

	IMaterial *GetMaterial();
	void AddTexture( ICompositeTexture *pTexture );
	ICompositeTexture *GetTexture( int nIndex );
	bool IsValid() const;
	bool CheckRegenerate( int nSize );
	const char* GetBaseMaterialName( void );
};
