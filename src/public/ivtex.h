#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

class IVTex
{
public:
	// For use by command-line tools
	virtual int VTex( int argc, char **argv ) = 0;

	// For use by engine
	virtual int VTex( CreateInterfaceFn filesystemFactory, const char *pGameDir, int argc, const char **argv ) = 0;
};

inline constexpr const char *IVTEX_VERSION_STRING = "VTEX_003";
