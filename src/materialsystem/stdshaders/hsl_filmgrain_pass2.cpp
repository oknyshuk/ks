// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "BaseVSShader.h"
import std;
#include "filmgrain_vs20.inc"
#include "hsl_filmgrain_pass2_ps20.inc"
#include "hsl_filmgrain_pass2_ps20b.inc"

//
// Second pass merely converts from HSL back to RGB space, noise was already applied in first pass
//

BEGIN_VS_SHADER( hsl_filmgrain_pass2, "Help for Film Grain" )
	
		SHADER_PARAM( INPUT, SHADER_PARAM_TYPE_TEXTURE, "", "" )

	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( INPUT );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableDepthTest( false );
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableBlending( false );
			pShaderShadow->EnableCulling( false );
//			pShaderShadow->PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_LINE );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			DECLARE_STATIC_VERTEX_SHADER( filmgrain_vs20 );
			SET_STATIC_VERTEX_SHADER( filmgrain_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20b );
				SET_STATIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20 );
				SET_STATIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, INPUT, -1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( filmgrain_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( filmgrain_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( hsl_filmgrain_pass2_ps20 );
			}
		}
		Draw();
	}
END_SHADER
