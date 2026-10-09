#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Provide interface to custom materials for entities that use them

import std;
import ks.tier1;

class IMaterial;
class ICompositeTexture;

class ICustomMaterial : public CRefCounted<>
{
public:
	virtual IMaterial *GetMaterial() = 0;
	virtual void AddTexture( ICompositeTexture *pTexture ) = 0;
	virtual ICompositeTexture *GetTexture( int nIndex ) = 0;
	virtual bool IsValid() const = 0;
	virtual bool CheckRegenerate( int nSize ) = 0;
	virtual const char* GetBaseMaterialName( void ) = 0;
};
