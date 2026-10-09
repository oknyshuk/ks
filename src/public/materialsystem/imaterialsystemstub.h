#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "materialsystem/imaterialsystem.h"

// If you get this interface out of the material system, it'll return an IMaterialSystem
// with everything stubbed. This is used for running the client in text mode.
inline constexpr const char *MATERIAL_SYSTEM_STUB_INTERFACE_VERSION = "VMaterialSystemStub001";


class IMaterialSystemStub : public IMaterialSystem
{
public:
	// If this is called, then the stub will call through to the real material 
	// system in some functions.
	virtual void	SetRealMaterialSystem( IMaterialSystem *pSys ) = 0;
};
