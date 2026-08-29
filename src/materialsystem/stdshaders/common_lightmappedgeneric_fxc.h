//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================


void GetBaseTextureAndNormal( sampler base, sampler base2, sampler bump, bool bBase2, bool bBump,
							  float3 coords, float3 coords2, float2 bumpcoords, HALF3 vWeights,
							 out HALF4 vResultBase, out HALF4 vResultBase2, out HALF4 vResultBump )
{
	vResultBase = 0;
	vResultBase2 = 0;
	vResultBump = 0;

	if ( !bBump )
	{
		vResultBump = HALF4(0, 0, 1, 1);
	}

#if SEAMLESS

	vResultBase  += vWeights.x * h4tex2D( base, coords.zy );
	if ( bBase2 )
	{
		vResultBase2 += vWeights.x * h4tex2D( base2, coords.zy );
	}
	if ( bBump )
	{
		vResultBump  += vWeights.x * h4tex2D( bump, coords.zy );
	}

	vResultBase  += vWeights.y * h4tex2D( base, coords.xz );
	if ( bBase2 )
	{
		vResultBase2 += vWeights.y * h4tex2D( base2, coords.xz );
	}
	if ( bBump )
	{
		vResultBump  += vWeights.y * h4tex2D( bump, coords.xz );
	}

	vResultBase  += vWeights.z * h4tex2D( base, coords.xy );
	if ( bBase2 )
	{
		vResultBase2 += vWeights.z * h4tex2D( base2, coords.xy );
	}
	if ( bBump )
	{
		vResultBump  += vWeights.z * h4tex2D( bump, coords.xy );
	}

#else  // not seamless

	vResultBase  = h4tex2D( base, coords.xy );
	if ( bBase2 )
	{
		vResultBase2 = h4tex2D( base2, coords2.xy );
	}
	if ( bBump )
	{
		vResultBump  = h4tex2D( bump, bumpcoords.xy );
	}

#endif
}



HALF4 LightMapSample( sampler LightmapSampler, float2 vTexCoord )
{
	{
		HALF4 sample = h4tex2D( LightmapSampler, vTexCoord );
		return sample;
	}
}

#ifdef PIXELSHADER
	#define VS_OUTPUT PS_INPUT
#endif

struct VS_OUTPUT
{
#ifndef PIXELSHADER
	float4 projPos : POSITION;
#if !defined( _X360 ) && !defined( SHADER_MODEL_VS_3_0 )
	float  fog : FOG;
#endif
#endif

#if SEAMLESS
	float3 SeamlessTexCoord							: TEXCOORD0;
#else
	float4 baseTexCoord_blendmodulateTexCoord		: TEXCOORD0;
#endif

	float4 detailTexCoord_EnvmapMaskTexCoord		: TEXCOORD1;
	float4 lightmapTexCoord1And2					: TEXCOORD2_centroid;
	float4 lightmapTexCoord3_bumpTexCoord			: TEXCOORD3_centroid;
	float4 worldPos_projPosZ						: TEXCOORD4;

	float4 tangentSpaceTranspose0_vertexBlendX		: TEXCOORD5;
	float4 tangentSpaceTranspose1_bumpTexCoord2u	: TEXCOORD6;
	float4 tangentSpaceTranspose2_bumpTexCoord2v	: TEXCOORD7;

#if defined ( SHADER_MODEL_VS_3_0 ) || defined ( SHADER_MODEL_PS_3_0 )
	float4 baseTexCoord2_detailTexCoord2			: TEXCOORD8;
#endif

	float4 vertexColor								: COLOR0;

	// Extra iterators on 360, used in flashlight combo

};

// base
#if SEAMLESS
// don't use BASETEXCOORD in the SEAMLESS case
#else
#define BASETEXCOORD baseTexCoord_blendmodulateTexCoord.xy

#if defined ( SHADER_MODEL_VS_3_0 ) || defined ( SHADER_MODEL_PS_3_0 )
#define BASETEXCOORD2 baseTexCoord2_detailTexCoord2.xy
#else
#define BASETEXCOORD2 baseTexCoord_blendmodulateTexCoord.xy
#endif
#endif

// detail
#define DETAILCOORD detailTexCoord_EnvmapMaskTexCoord.xy
#if defined ( SHADER_MODEL_VS_3_0 ) || defined ( SHADER_MODEL_PS_3_0 )
#define DETAILCOORD2 baseTexCoord2_detailTexCoord2.zw
#endif

// bump
#define BUMPCOORD lightmapTexCoord3_bumpTexCoord.zw
#define BUMPCOORD2U tangentSpaceTranspose1_bumpTexCoord2u.w
#define BUMPCOORD2V tangentSpaceTranspose2_bumpTexCoord2v.w

#define ENVMAPMASKCOORD detailTexCoord_EnvmapMaskTexCoord.zw

#if !SEAMLESS
#define BLENDMODULATECOORD baseTexCoord_blendmodulateTexCoord.zw
#endif

