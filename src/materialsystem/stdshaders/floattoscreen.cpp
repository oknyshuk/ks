// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.scratcharena;
import ks.platform;
import ks.simd;
import ks.tier1.strtools;
#include "BaseVSShader.h"

#include "screenspaceeffect_vs20.inc"
#include "floattoscreen_ps20.inc"
#include "floattoscreen_ps20b.inc"
import std;

BEGIN_VS_SHADER_FLAGS( floattoscreen, "Help for floattoscreen", SHADER_NOT_EDITABLE )
	
		SHADER_PARAM( FBTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( PIXSHADER, SHADER_PARAM_TYPE_STRING, "floattoscreen_ps20", "Name of the pixel shader to use" )
	END_SHADER_PARAMS

	SHADER_INIT
	{
		if( params[FBTEXTURE]->IsDefined() )
		{
			LoadTexture( FBTEXTURE );
		}
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

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			// convert from linear to gamma on write.
			pShaderShadow->EnableSRGBWrite( true );

			// Pre-cache shaders
			DECLARE_STATIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			SET_STATIC_VERTEX_SHADER( screenspaceeffect_vs20 );

//			DECLARE_STATIC_PIXEL_SHADER( floattoscreen_ps20 );
//			SET_STATIC_PIXEL_SHADER( floattoscreen_ps20 );
			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				const char *szPixelShader = params[PIXSHADER]->GetStringValue();
				std::size_t iLength = V_strlen( szPixelShader );

				if( (iLength > 5) && (V_stricmp( &szPixelShader[iLength - 5], "_ps20" ) == 0) ) //detect if it's trying to load a ps20 shader
				{
					//replace it with the ps20b shader
					ks::ScratchArena<> scratch;
					char *szNewName = scratch.Alloc<char>( (iLength + 2) );
					std::memcpy( szNewName, szPixelShader, sizeof( char ) * iLength );
					szNewName[iLength] = 'b';
					szNewName[iLength + 1] = '\0';
					pShaderShadow->SetPixelShader( szNewName, 0 );
				}
				else
				{
					pShaderShadow->SetPixelShader( params[PIXSHADER]->GetStringValue(), 0 );
				}
			}
			else
			{
				pShaderShadow->SetPixelShader( params[PIXSHADER]->GetStringValue(), 0 );
			}
		}

		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, FBTEXTURE, -1 );
			DECLARE_DYNAMIC_VERTEX_SHADER( screenspaceeffect_vs20 );
			SET_DYNAMIC_VERTEX_SHADER( screenspaceeffect_vs20 );

//			DECLARE_DYNAMIC_PIXEL_SHADER( floattoscreen_ps20 );
//			SET_DYNAMIC_PIXEL_SHADER( floattoscreen_ps20 );
			pShaderAPI->SetPixelShaderIndex( 0 );
		}
		Draw();
	}
END_SHADER
