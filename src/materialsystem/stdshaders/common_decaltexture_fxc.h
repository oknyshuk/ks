#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Common pixel shader code for decaltexture usage

// decal blend modes
float3 TextureCombineDecal( float3 baseColor, float4 decalColor, float3 decalLighting )
{
	#if ( DECAL_BLEND_MODE == 0 )
	{
		baseColor.rgb = ( decalColor.rgb * decalLighting * decalColor.a ) + ( baseColor.rgb * ( 1.0f - decalColor.a) );
	}
	#elif ( DECAL_BLEND_MODE == 1 )
	{
		baseColor.rgb = baseColor.rgb * decalColor.rgb;
	}
	#endif

	return baseColor;
}
