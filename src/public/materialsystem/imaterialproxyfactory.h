#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

inline constexpr const char *IMATERIAL_PROXY_FACTOR_INTERFACE_VERSION = "IMaterialProxyFactory001";

class IMaterialProxy;

class IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy( const char *proxyName ) = 0;
	virtual void DeleteProxy( IMaterialProxy *pProxy ) = 0;
	virtual CreateInterfaceFn GetFactory() = 0;
};
