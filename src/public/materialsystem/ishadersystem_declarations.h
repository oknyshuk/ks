#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Modulation flags

enum
{
	SHADER_USING_ALPHA_MODULATION				= 0x01,
	SHADER_USING_FLASHLIGHT						= 0x02,
	SHADER_USING_PAINT							= 0x04,
	SHADER_USING_EDITOR							= 0x08,

	// the BUFFER0 and GBUFFER1 bits provide 3 g-buffermodes plus the normal modes.
	// the modes are:
	// Normal rendering = ( gbuffer1 = 0, gbuffer0 = 0 )
	// Output pos, normal, albedo via mrts = (0,1)
	// output fixed lighted single image = (1,0)
	// output the normal = (1,1)
	SHADER_USING_GBUFFER0                       = 0x10,
	SHADER_USING_GBUFFER1                       = 0x20,
};
