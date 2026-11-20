#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
#include "materialsystem/imaterialproxyfactory.h"
import std;

class CMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	IMaterialProxy *CreateProxy( const char *proxyName );
	void DeleteProxy( IMaterialProxy *pProxy );
	CreateInterfaceFn GetFactory();
};
