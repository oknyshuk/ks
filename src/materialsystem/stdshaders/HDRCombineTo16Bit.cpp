// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

import ks.platform;
import ks.simd;
#include "BaseVSShader.h"
#include "HDRCombineTo16Bit_ps20.inc"
#include "HDRCombineTo16Bit_ps20b.inc"
#include "HDRCombineTo16Bit_vs20.inc"
import std;

BEGIN_VS_SHADER_FLAGS( HDRCombineTo16Bit, "Help for HDRCombineTo16Bit", SHADER_NOT_EDITABLE )
	
		SHADER_PARAM( SOURCEMRTRENDERTARGET, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	END_SHADER_PARAMS

	SHADER_INIT
	{
		LoadTexture( SOURCEMRTRENDERTARGET );
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
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableDepthTest( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			DECLARE_STATIC_VERTEX_SHADER( hdrcombineto16bit_vs20 );
			SET_STATIC_VERTEX_SHADER( hdrcombineto16bit_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( hdrcombineto16bit_ps20b );
				SET_STATIC_PIXEL_SHADER( hdrcombineto16bit_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( hdrcombineto16bit_ps20 );
				SET_STATIC_PIXEL_SHADER( hdrcombineto16bit_ps20 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, SOURCEMRTRENDERTARGET, -1 );
			DECLARE_DYNAMIC_VERTEX_SHADER( hdrcombineto16bit_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( hdrcombineto16bit_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( hdrcombineto16bit_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( hdrcombineto16bit_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( hdrcombineto16bit_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( hdrcombineto16bit_ps20 );
			}
		}
		Draw();
	}
END_SHADER
