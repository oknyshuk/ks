#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Utility class for building command buffers into memory

import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vector2d;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
#include "shaderapi/commandbuffer.h"
#include "shaderapi/ishaderapi.h"
#include "shaderlib/BaseShader.h"
import std;
#include "shaderlib/cshader.h"

//-----------------------------------------------------------------------------
// Buffer for storing commands into
//-----------------------------------------------------------------------------
template<int N> class CFixedCommandStorageBuffer
{
public:
	uint8 m_Data[N];

	uint8 *m_pDataOut;
	
	[[gnu::always_inline]] inline CFixedCommandStorageBuffer( void )
	{
		m_pDataOut = m_Data;

    }

	[[gnu::always_inline]] inline void EnsureCapacity( std::size_t sz )
	{
	}

	template<class T> [[gnu::always_inline]] inline void Put( T const &nValue )
	{
		EnsureCapacity( sizeof( T ) );
		*( reinterpret_cast<T *>( m_pDataOut ) ) = nValue;
		m_pDataOut += sizeof( nValue );
	}

	[[gnu::always_inline]] inline void PutInt( int nValue )
	{
		Put( nValue );
	}

	[[gnu::always_inline]] inline void PutFloat( float nValue )
	{
		Put( nValue );
	}

	[[gnu::always_inline]] inline void PutPtr( void * pPtr )
	{
		Put( pPtr );
	}
	
	[[gnu::always_inline]] inline void PutMemory( const void *pMemory, std::size_t nBytes )
	{
		EnsureCapacity( nBytes );
		std::memcpy( m_pDataOut, pMemory, nBytes );
		m_pDataOut += nBytes;
	}

	[[gnu::always_inline]] inline uint8 *Base( void )
	{
		return m_Data;
	}

	[[gnu::always_inline]] inline void Reset( void )
	{
		m_pDataOut = m_Data;
	}

	[[gnu::always_inline]] inline std::size_t Size( void ) const
	{
		return m_pDataOut - m_Data;
	}

};


//-----------------------------------------------------------------------------
// Base class used to build up command buffers
//-----------------------------------------------------------------------------
template<class S> class CBaseCommandBufferBuilder
{
public:
    S m_Storage;

	[[gnu::always_inline]] inline void End( void )
	{
		m_Storage.PutInt( CBCMD_END );
	}

	[[gnu::always_inline]] inline IMaterialVar *Param( int nVar ) const
	{
		return CBaseShader::s_ppParams[nVar];
	}

	[[gnu::always_inline]] inline void Reset( void )
	{
		m_Storage.Reset();
	}

	[[gnu::always_inline]] inline std::size_t Size( void ) const
	{
		return m_Storage.Size();
	}

	[[gnu::always_inline]] inline uint8 *Base( void )
	{
		return m_Storage.Base();
	}

	[[gnu::always_inline]] inline void OutputConstantData( float const *pSrcData )
	{
		m_Storage.PutFloat( pSrcData[0] );
		m_Storage.PutFloat( pSrcData[1] );
		m_Storage.PutFloat( pSrcData[2] );
		m_Storage.PutFloat( pSrcData[3] );
	}

	[[gnu::always_inline]] inline void OutputConstantData4( float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		m_Storage.PutFloat( flVal0 );
		m_Storage.PutFloat( flVal1 );
		m_Storage.PutFloat( flVal2 );
		m_Storage.PutFloat( flVal3 );
	}
};


//-----------------------------------------------------------------------------
// Used by SetPixelShaderFlashlightState
//-----------------------------------------------------------------------------
struct CBCmdSetPixelShaderFlashlightState_t
{
	Sampler_t m_LightSampler;
	Sampler_t m_DepthSampler;
	Sampler_t m_ShadowNoiseSampler;
	int m_nColorConstant;
	int m_nAttenConstant;
	int m_nOriginConstant;
	int m_nDepthTweakConstant;
	int m_nScreenScaleConstant;
	int m_nWorldToTextureConstant;
	bool m_bFlashlightNoLambert;
	bool m_bSinglePassFlashlight;
};


//-----------------------------------------------------------------------------
// Used to build a per-pass command buffer
//-----------------------------------------------------------------------------
template<class S> class CCommandBufferBuilder : public CBaseCommandBufferBuilder<S>
{
	typedef CBaseCommandBufferBuilder<S> PARENT;


public:


	[[gnu::always_inline]] inline void End( void )
	{
		this->m_Storage.PutInt( CBCMD_END );

	}

	[[gnu::always_inline]] inline void SetPixelShaderConstants( int nFirstConstant, int nConstants )
	{
		this->m_Storage.PutInt( CBCMD_SET_PIXEL_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( nConstants );
	}

	[[gnu::always_inline]] inline void OutputConstantData( float const *pSrcData )
	{
		this->m_Storage.PutFloat( pSrcData[0] );
		this->m_Storage.PutFloat( pSrcData[1] );
		this->m_Storage.PutFloat( pSrcData[2] );
		this->m_Storage.PutFloat( pSrcData[3] );
	}
	
	[[gnu::always_inline]] inline void OutputConstantData4( float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		this->m_Storage.PutFloat( flVal0 );
		this->m_Storage.PutFloat( flVal1 );
		this->m_Storage.PutFloat( flVal2 );
		this->m_Storage.PutFloat( flVal3 );
	}

	[[gnu::always_inline]] inline void SetPixelShaderConstant( int nFirstConstant, float const *pSrcData, int nNumConstantsToSet )
	{
		SetPixelShaderConstants( nFirstConstant, nNumConstantsToSet );
		this->m_Storage.PutMemory( pSrcData, 4 * sizeof( float ) * nNumConstantsToSet );
	}

	[[gnu::always_inline]] inline void SetPixelShaderConstant( int nFirstConstant, int nVar )
	{
		SetPixelShaderConstant( nFirstConstant, this->Param( nVar )->GetVecValue() );
	}

	void SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar )
	{
		float val[4];
		this->Param(constantVar)->GetVecValue( val, 3 );
		val[0] = val[0] > 1.0f ? val[0] : GammaToLinear( val[0] );
		val[1] = val[1] > 1.0f ? val[1] : GammaToLinear( val[1] );
		val[2] = val[2] > 1.0f ? val[2] : GammaToLinear( val[2] );
		val[3] = 1.0;
		SetPixelShaderConstant( pixelReg, val );
	}

	[[gnu::always_inline]] inline void SetPixelShaderConstant( int nFirstConstant, float const *pSrcData )
	{
		SetPixelShaderConstants( nFirstConstant, 1 );
		OutputConstantData( pSrcData );
	}

	[[gnu::always_inline]] inline void SetPixelShaderConstant4( int nFirstConstant, float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		SetPixelShaderConstants( nFirstConstant, 1 );
		OutputConstantData4( flVal0, flVal1, flVal2, flVal3 );
	}

	[[gnu::always_inline]] inline void SetPixelShaderConstant_W( int pixelReg, int constantVar, float fWValue )
	{
		if ( constantVar != -1 )
		{
			float val[3];
			this->Param(constantVar)->GetVecValue( val, 3);
			SetPixelShaderConstant4( pixelReg, val[0], val[1], val[2], fWValue );
		}
	}

	void SetPixelShaderTextureTransform( int vertexReg, int transformVar )
	{
		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = ( transformVar >= 0 ) ? this->Param( transformVar ) : nullptr;
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			const VMatrix &mat = pTransformationVar->GetMatrixValue();
			transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
			transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
		}
		else
		{
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		}
		SetPixelShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
	}

	[[gnu::always_inline]] inline void SetVertexShaderConstant( int nFirstConstant, float const *pSrcData )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( 1 );
		OutputConstantData( pSrcData );
	}

	[[gnu::always_inline]] inline void SetVertexShaderConstant( int nFirstConstant, float const *pSrcData, int nConsts )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( nConsts );
		this->m_Storage.PutMemory( pSrcData, 4 * nConsts * sizeof( float ) );
	}


	[[gnu::always_inline]] inline void SetVertexShaderConstant4( int nFirstConstant, float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( 1 );
		this->m_Storage.PutFloat( flVal0 );
		this->m_Storage.PutFloat( flVal1 );
		this->m_Storage.PutFloat( flVal2 );
		this->m_Storage.PutFloat( flVal3 );
	}

	void SetVertexShaderTextureTransform( int vertexReg, int transformVar )
	{
		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = ( transformVar >= 0 ) ? this->Param( transformVar ) : nullptr;
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			const VMatrix &mat = pTransformationVar->GetMatrixValue();
			transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
			transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
		}
		else
		{
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		}
		SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
	}


	void SetVertexShaderTextureScaledTransformRotate( int vertexReg, int transformVar, int scaleVar, int rotateVar )
	{
		Vector2D scale( 1, 1 );
		IMaterialVar* pScaleVar = this->Param( scaleVar );
		if (pScaleVar)
		{
			if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
				pScaleVar->GetVecValue( scale.Base(), 2 );
			else if (pScaleVar->IsDefined())
				scale[0] = scale[1] = pScaleVar->GetFloatValue();
		}

		float flRotateVar = 0.0f;
		IMaterialVar* pRotateVar = this->Param( rotateVar );
		if ( pRotateVar && pRotateVar->IsDefined() )
		{
			flRotateVar = pRotateVar->GetFloatValue();
		}

		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = this->Param( transformVar );
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			VMatrix matRot = pTransformationVar->GetMatrixValue();
			MatrixTranslate( matRot, Vector( 0.5, 0.5, 0 ) );
			MatrixRotate(	 matRot, Vector( 0, 0, 1), flRotateVar );
			MatrixTranslate( matRot, Vector( -0.5 * scale[0], -0.5 * scale[1], 0 ) );
			matRot = matRot.Scale( Vector(scale[0], scale[1], 1) );

			transformation[0].Init( matRot[0][0], matRot[0][1], matRot[0][2], matRot[0][3] );
			transformation[1].Init( matRot[1][0], matRot[1][1], matRot[1][2], matRot[1][3] );

			SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
		}
	}


	void SetVertexShaderTextureScaledTransform( int vertexReg, int transformVar, int scaleVar )
	{
		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = this->Param( transformVar );
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			const VMatrix &mat = pTransformationVar->GetMatrixValue();
			transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
			transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
		}
		else
		{
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		}

		Vector2D scale( 1, 1 );
		IMaterialVar* pScaleVar = this->Param( scaleVar );
		if (pScaleVar)
		{
			if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
				pScaleVar->GetVecValue( scale.Base(), 2 );
			else if (pScaleVar->IsDefined())
				scale[0] = scale[1] = pScaleVar->GetFloatValue();
		}
		
		// Apply the scaling
		transformation[0][0] *= scale[0];
		transformation[0][1] *= scale[1];
		transformation[1][0] *= scale[0];
		transformation[1][1] *= scale[1];
		transformation[0][3] *= scale[0];
		transformation[1][3] *= scale[1];
		SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
	}

	[[gnu::always_inline]] inline void SetEnvMapTintPixelShaderDynamicState( int pixelReg, int tintVar )
	{
		if( g_pConfig->bShowSpecular && g_pConfig->nFullbright != 2 )
		{
			SetPixelShaderConstant( pixelReg, this->Param( tintVar)->GetVecValue() );
		}
		else
		{
			SetPixelShaderConstant4( pixelReg, 0.0, 0.0, 0.0, 0.0 );
		}
	}

	[[gnu::always_inline]] inline void SetEnvMapTintPixelShaderDynamicStateGammaToLinear( int pixelReg, int tintVar, float fAlphaVal = 1.0f )
	{
		if( g_pConfig->bShowSpecular && g_pConfig->nFullbright != 2 )
		{
			float color[4];
			color[3] = fAlphaVal;

			//this->Param( tintVar)->GetLinearVecValue( color, 3 );
			// (wills) converted this line to the following so that envmaptint can be over-driven beyond 0-1 range

			this->Param( tintVar)->GetVecValue( color, 3 );
			color[0] = GammaToLinearFullRange( color[0] );
			color[1] = GammaToLinearFullRange( color[1] );
			color[2] = GammaToLinearFullRange( color[2] );

			SetPixelShaderConstant( pixelReg, color );
		}
		else
		{
			SetPixelShaderConstant4( pixelReg, 0.0, 0.0, 0.0, fAlphaVal );
		}
	}

	[[gnu::always_inline]] inline void StoreEyePosInPixelShaderConstant( int nConst, float wValue = 1.0f )
	{
		this->m_Storage.PutInt( CBCMD_STORE_EYE_POS_IN_PSCONST );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.PutFloat( wValue );
	}

	[[gnu::always_inline]] inline void SetPixelShaderFogParams( int nReg )
	{
		this->m_Storage.PutInt( CBCMD_SETPIXELSHADERFOGPARAMS );
		this->m_Storage.PutInt( nReg );
	}


	[[gnu::always_inline]] inline void BindStandardTexture( Sampler_t nSampler, TextureBindFlags_t nBindFlags, StandardTextureId_t nTextureId )
	{
		this->m_Storage.PutInt( CBCMD_BIND_STANDARD_TEXTURE );
		this->m_Storage.PutInt( nSampler | nBindFlags );
		this->m_Storage.PutInt( nTextureId );
	}

	[[gnu::always_inline]] inline void BindTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( pTexture, nFrame );
		contract_assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( nSampler | nBindFlags );
		this->m_Storage.Put( hTexture );
	}

[[gnu::always_inline]] inline void BindTexture( Sampler_t nSampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t hTexture )
	{
		contract_assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( nSampler | nBindFlags );
		this->m_Storage.Put( hTexture );
	}



	[[gnu::always_inline]] inline void BindTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar = -1 )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar );
		BindTexture( nSampler, nBindFlags, hTexture );
	}

	// Same as BindTexture, except it checks to see if the texture handle is actually the "internal" env_cubemap. If so, it binds it as a standard texture so the proper texture bind flags are 
	// recorded during instance rendering in CShaderAPIDX8.
	[[gnu::always_inline]] inline void BindEnvCubemapTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar = -1 )
	{
		contract_assert( nTextureVar != -1 );
		contract_assert( CBaseShader::GetPPParams() );
		if ( CBaseShader::GetPPParams()[nTextureVar]->IsTextureValueInternalEnvCubemap() )
		{
			BindStandardTexture( nSampler, nBindFlags, TEXTURE_LOCAL_ENV_CUBEMAP );
		}
		else
		{
			ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar );
			BindTexture( nSampler, nBindFlags, hTexture );
		}
	}

	[[gnu::always_inline]] inline void BindMultiTexture( CBaseShader *pShader, Sampler_t nSampler1, Sampler_t nSampler2, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar, 0 );
		BindTexture( nSampler1, nBindFlags, hTexture );
		hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar, 1 );
		BindTexture( nSampler2, nBindFlags, hTexture );
	}

	[[gnu::always_inline]] inline void SetPixelShaderIndex( int nIndex )
	{
		this->m_Storage.PutInt( CBCMD_SET_PSHINDEX );
		this->m_Storage.PutInt( nIndex );
	}

	[[gnu::always_inline]] inline void SetVertexShaderIndex( int nIndex )
	{
		this->m_Storage.PutInt( CBCMD_SET_VSHINDEX );
		this->m_Storage.PutInt( nIndex );
	}

	[[gnu::always_inline]] inline void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale )
	{
		this->m_Storage.PutInt( CBCMD_SET_DEPTH_FEATHERING_CONST );
		this->m_Storage.PutInt( iConstant );
		this->m_Storage.PutFloat( fDepthBlendScale );
	}

	[[gnu::always_inline]] inline void SetVertexShaderFlashlightState( int iConstant )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLASHLIGHT_STATE );
		this->m_Storage.PutInt( iConstant );
	}

	[[gnu::always_inline]] inline void SetPixelShaderFlashlightState( const CBCmdSetPixelShaderFlashlightState_t &state )
	{
		this->m_Storage.PutInt( CBCMD_SET_PIXEL_SHADER_FLASHLIGHT_STATE );
		this->m_Storage.PutInt( state.m_LightSampler );
		this->m_Storage.PutInt( state.m_DepthSampler );
		this->m_Storage.PutInt( state.m_ShadowNoiseSampler );
		this->m_Storage.PutInt( state.m_nColorConstant );
		this->m_Storage.PutInt( state.m_nAttenConstant );
		this->m_Storage.PutInt( state.m_nOriginConstant );
		this->m_Storage.PutInt( state.m_nDepthTweakConstant );
		this->m_Storage.PutInt( state.m_nScreenScaleConstant );
		this->m_Storage.PutInt( state.m_nWorldToTextureConstant );
		this->m_Storage.PutInt( state.m_bFlashlightNoLambert );
		this->m_Storage.PutInt( state.m_bSinglePassFlashlight );
	}

	[[gnu::always_inline]] inline void SetPixelShaderUberLightState( int iEdge0Const, int iEdge1Const, int iEdgeOOWConst, int iShearRoundConst, int iAABBConst, int iWorldToLightConst )
	{
		this->m_Storage.PutInt( CBCMD_SET_PIXEL_SHADER_UBERLIGHT_STATE );
		this->m_Storage.PutInt( iEdge0Const );
		this->m_Storage.PutInt( iEdge1Const );
		this->m_Storage.PutInt( iEdgeOOWConst );
		this->m_Storage.PutInt( iShearRoundConst );
		this->m_Storage.PutInt( iAABBConst );
		this->m_Storage.PutInt( iWorldToLightConst );
	}

	[[gnu::always_inline]] inline void Goto( uint8 *pCmdBuf )
	{
		this->m_Storage.PutInt( CBCMD_JUMP );
		this->m_Storage.PutPtr( pCmdBuf );
	}

	[[gnu::always_inline]] inline void Call( uint8 *pCmdBuf )
	{
		this->m_Storage.PutInt( CBCMD_JSR );
		this->m_Storage.PutPtr( pCmdBuf );
	}

	[[gnu::always_inline]] inline void Reset( void )
	{
		this->m_Storage.Reset();
	}

	[[gnu::always_inline]] inline std::size_t Size( void ) const
	{
		return this->m_Storage.Size();
	}

	[[gnu::always_inline]] inline uint8 *Base( void )
	{
		return this->m_Storage.Base();
	}

	[[gnu::always_inline]] inline void SetVertexShaderNearAndFarZ( int iRegNum )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_NEARZFARZ_STATE );
		this->m_Storage.PutInt( iRegNum );
	}
};


//-----------------------------------------------------------------------------
// Builds a command buffer specifically for per-instance state setting
//-----------------------------------------------------------------------------
template<class S> class CInstanceCommandBufferBuilder : public CBaseCommandBufferBuilder< S >
{
	typedef CBaseCommandBufferBuilder< S > PARENT;

public:
	[[gnu::always_inline]] inline void End( void )
	{
		this->m_Storage.PutInt( CBICMD_END );
	}

	[[gnu::always_inline]] inline void SetPixelShaderLocalLighting( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERLOCALLIGHTING );
		this->m_Storage.PutInt( nConst );
	}

	[[gnu::always_inline]] inline void SetPixelShaderAmbientLightCube( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBE );
		this->m_Storage.PutInt( nConst );
	}

	[[gnu::always_inline]] inline void SetVertexShaderLocalLighting( )
	{
		this->m_Storage.PutInt( CBICMD_SETVERTEXSHADERLOCALLIGHTING );
	}

	[[gnu::always_inline]] inline void SetVertexShaderAmbientLightCube( void )
	{
		this->m_Storage.PutInt( CBICMD_SETVERTEXSHADERAMBIENTLIGHTCUBE );
	}

	[[gnu::always_inline]] inline void SetSkinningMatrices( void )
	{
		this->m_Storage.PutInt( CBICMD_SETSKINNINGMATRICES );
	}

	[[gnu::always_inline]] inline void SetPixelShaderAmbientLightCubeLuminance( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBELUMINANCE );
		this->m_Storage.PutInt( nConst );
	}

	[[gnu::always_inline]] inline void SetPixelShaderGlintDamping( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERGLINTDAMPING );
		this->m_Storage.PutInt( nConst );
	}

	[[gnu::always_inline]] inline void SetModulationPixelShaderDynamicState_LinearColorSpace_LinearScale( int nConst, const Vector &vecGammaSpaceColor2Factor, float scale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE_LINEARSCALE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( 1.0 );					// pad for vector4
		this->m_Storage.PutFloat( scale );
	}

	[[gnu::always_inline]] inline void SetModulationPixelShaderDynamicState_LinearScale( int nConst, const Vector &vecGammaSpaceColor2Factor, float scale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( 1.0 );					// alpha modulation wants 1 1.0 here for simd
		this->m_Storage.PutFloat( scale );
	}

	[[gnu::always_inline]] inline void SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( int nConst, const Vector &vecGammaSpaceColor2Factor, float scale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE_SCALEINW );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( scale );
	}

	[[gnu::always_inline]] inline void SetModulationPixelShaderDynamicState_LinearColorSpace( int nConst, const Vector &vecGammaSpaceColor2Factor )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( 1.0 );						// pad with a 1 for vector4d simd access. Important that this be a 1 because alpha is multipled by it.
	}

	[[gnu::always_inline]] inline void SetModulationPixelShaderDynamicState( int nConst, const Vector &vecGammaSpaceColor2Factor )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
	}

	[[gnu::always_inline]] inline void SetModulationPixelShaderDynamicState_Identity( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_IDENTITY );
		this->m_Storage.PutInt( nConst );
	}

	[[gnu::always_inline]] inline void SetModulationVertexShaderDynamicState( int nConst, const Vector &vecGammaSpaceColor2Factor )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
	}
	
	[[gnu::always_inline]] inline void SetModulationVertexShaderDynamicState_LinearScale( int nConst, const Vector &vecGammaSpaceColor2Factor, float flScale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE_LINEARSCALE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( flScale );
	}
};
