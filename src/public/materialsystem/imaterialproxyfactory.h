#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.createinterface;
import ks.tier1.interfaces;

inline constexpr const char *IMATERIAL_PROXY_FACTOR_INTERFACE_VERSION = "IMaterialProxyFactory001";

class IMaterialProxy;

class IMaterialProxyFactory
{
public:
	IMaterialProxy *CreateProxy( const char *proxyName );
	void DeleteProxy( IMaterialProxy *pProxy );
	CreateInterfaceFn GetFactory();
};
