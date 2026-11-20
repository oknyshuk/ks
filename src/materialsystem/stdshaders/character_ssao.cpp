// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.platform;
import ks.simd;
#include "BaseVSShader.h"
import std;

#include "character_ssao_vs20.inc"
#include "character_ssao_ps20.inc"
#include "character_ssao_ps20b.inc"
#include "character_ssao_ps30.inc"
#include "shaderlib/commandbuilder.h"

// this shader processes a screen-space normal + depth prepass into screen-space AO for later reference

BEGIN_VS_SHADER( CharacterSSAO, "Help for CharacterSSAO" )

	
		
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_UNLIT );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		if( params[BASETEXTURE]->IsDefined() )
		{
			LoadTexture( BASETEXTURE, 0 );
		}
	}

	inline void DrawCharacterSSAO( IMaterialVar **params, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, int vertexCompression )
	{
		SHADOW_STATE
		{
			SetInitialShadowState( );

			if( params[BASETEXTURE]->IsDefined() )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			}

			unsigned int flags = VERTEX_POSITION;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, nullptr, userDataSize );
			
			DECLARE_STATIC_VERTEX_SHADER( character_ssao_vs20 );
			SET_STATIC_VERTEX_SHADER( character_ssao_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_3_0() )
			{
				DECLARE_STATIC_PIXEL_SHADER( character_ssao_ps30 );
				SET_STATIC_PIXEL_SHADER_COMBO( QUALITY_MODE, 2 );
				SET_STATIC_PIXEL_SHADER( character_ssao_ps30 );
			}
			else if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( character_ssao_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( QUALITY_MODE, 1 );
				SET_STATIC_PIXEL_SHADER( character_ssao_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( character_ssao_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( QUALITY_MODE, 0 );
				SET_STATIC_PIXEL_SHADER( character_ssao_ps20 );
			}
			
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableSRGBWrite( false );

				pShaderShadow->EnableBlending( false );

		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();

			if( params[BASETEXTURE]->IsDefined() )
			{
				BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE, -1 );
			}

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );
			
			DECLARE_DYNAMIC_VERTEX_SHADER( character_ssao_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( character_ssao_vs20 );

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( character_ssao_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( character_ssao_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( character_ssao_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( character_ssao_ps20 );
			}
		}
		Draw();
	}

	SHADER_DRAW
	{
		DrawCharacterSSAO( params, pShaderShadow, pShaderAPI, vertexCompression );
	}
END_SHADER
