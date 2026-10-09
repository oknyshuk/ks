#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "materialsystem/imaterialsystemhardwareconfig.h"

//-----------------------------------------------------------------------------
// Material system configuration
//-----------------------------------------------------------------------------
class IHardwareConfigInternal : public IMaterialSystemHardwareConfig
{
public:
	// Gets at the HW specific shader DLL name
	virtual const char *GetHWSpecificShaderDLLName() const = 0;
};
