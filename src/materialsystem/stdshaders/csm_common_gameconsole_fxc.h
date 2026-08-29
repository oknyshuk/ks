//========== Copyright (c) Valve Corporation, All rights reserved. ==========//

// X360 and PS3 cascaded shadow mapping

// 7LS TODO - PS3

	#error Unsupported


float CSMSampleShadowBuffer1Tap( float2 vPositionLs, float flComparisonDepth )
{
	#error Unsupported
}

float CSMSampleShadowBuffer( float2 vPositionLs, float flComparisonDepth )
{
	return CSMSampleShadowBuffer1Tap( vPositionLs, flComparisonDepth );
}

int CSMRangeTestExpanded( float2 vCoords )
{
	// Returns true if the coordinates are within [.02,.98] - purposely a little sloppy to prevent the shadow filter kernel from leaking outside the cascade's portion of the atlas.
	vCoords = vCoords * ( 1.0f / .96f ) - float2( .02f / .96f, .02f / .96f );
	return ( dot( saturate( vCoords.xy ) - vCoords.xy, float2( 1, 1 ) ) == 0.0f );
}

int CSMRangeTestNonExpanded( float2 vCoords )
{
	return ( dot( saturate( vCoords.xy ) - vCoords.xy, float2( 1, 1 ) ) == 0.0f );
}

float CSMComputeSplitLerpFactor( float2 vPositionToSampleLs )
{
	float2 vSplitLerpFactorTemp = float2( 1.0f, 1.0f ) - saturate( ( abs( vPositionToSampleLs.xy - float2( .5f, .5f ) ) - float2( g_flSunShadowingSplitLerpFactorBase, g_flSunShadowingSplitLerpFactorBase ) ) * float2( g_flSunShadowingSplitLerpFactorInvRange, g_flSunShadowingSplitLerpFactorInvRange ) );
	return vSplitLerpFactorTemp.x * vSplitLerpFactorTemp.y;
}

float4 CSMTransformLightToTexture( float4 pos, float4x4 mat )
{
	return mul( pos, mat );
}

#if ( CASCADE_SIZE == 0 )
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		return 1.0f;
	}
#elif ( CSM_MODE >= 1 )

	#error Invalid CSM_MODE
	
#else
	// CSM shader quality level 0 (the only supported level on gameconsole)
	float CSMComputeShadowing( float3 vPositionWs ) 
	{
		float flShadowScalar = 1.0f;

		float4 vPosition4Ws = float4( vPositionWs.xyz, 1.0f );

		float3 vPositionToSampleLs = float3( 0.0f, 0.0f, CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).z );

#if ( CSM_VIEWMODELQUALITY == 0 )

		// only consider cascade 1 and 2 for console perf

		int nCascadeIndex = 1;

		vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[1] ).xy;

		if ( !CSMRangeTestExpanded( vPositionToSampleLs.xy ) )
		{
			vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[2] ).xy;
			nCascadeIndex = 2;
		}

		vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * g_vCascadeAtlasUVOffsets[nCascadeIndex].zw + g_vCascadeAtlasUVOffsets[nCascadeIndex].xy;
		float3 vCamDelta = vPositionWs - g_vCamPosition.xyz;
		float flZLerpFactor = saturate( dot( vCamDelta, vCamDelta ) * g_flSunShadowingZLerpFactorRange + g_flSunShadowingZLerpFactorBase );

		flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
		flShadowScalar = lerp( flShadowScalar, 1.0f, flZLerpFactor );
#else 
    	// Viewmodel shadowing
		// only use cascade 0 for viewmodel rendering

		vPositionToSampleLs.xy = CSMTransformLightToTexture( vPosition4Ws.xyzw, g_matWorldToShadowTexMatrices[0] ).xy;

		vPositionToSampleLs.xy = saturate( vPositionToSampleLs.xy ) * g_vCascadeAtlasUVOffsets[0].zw + g_vCascadeAtlasUVOffsets[0].xy;

		flShadowScalar = CSMSampleShadowBuffer( vPositionToSampleLs.xy, vPositionToSampleLs.z );
#endif // CSM_VIEWMODELQUALITY == 0

		return flShadowScalar;
	}

#endif // #if ( CSM_MODE == 0 )
