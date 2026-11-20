#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;

class IDedicatedExports : public IAppSystem
{
public:
	virtual void Sys_Printf( const char *text ) = 0;
	virtual void RunServer() = 0;
	virtual bool IsGuiDedicatedServer() = 0;
};

inline constexpr const char *VENGINE_DEDICATEDEXPORTS_API_VERSION = "VENGINE_DEDICATEDEXPORTS_API_VERSION003";
