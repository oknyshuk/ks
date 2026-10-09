#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

inline constexpr const char *LAUNCHABLE_DLL_INTERFACE_VERSION = "launchable_dll_1";


// vmpi_service can use this to debug worker apps in-process,
// and some of the launchers (like texturecompile) use this.
class ILaunchableDLL
{
public:
	// All vrad.exe does is load the VRAD DLL and run this.
	virtual int			main( int argc, char **argv ) = 0;
};
