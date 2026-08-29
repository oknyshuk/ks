//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: Common pixel shader code specific to flashlights
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef COMMON_FLASHLIGHT_FXC_H_
#define COMMON_FLASHLIGHT_FXC_H_

#include "common_ps_fxc.h"


// Superellipse soft clipping
//
// Input:
//   - Point Q on the x-y plane
//   - The equations of two superellipses (with major/minor axes given by
//     a,b and A,B for the inner and outer ellipses, respectively)
//   - This is changed a bit from the original RenderMan code to be better vectorized
//
// Return value:
//   - 0 if Q was inside the inner ellipse
//   - 1 if Q was outside the outer ellipse
//   - smoothly varying from 0 to 1 in between
float2 ClipSuperellipse( float2 Q,			// Point on the xy plane
						 float4 aAbB,		// Dimensions of superellipses
						 float2 rounds )	// Same roundness for both ellipses
{
	float2 qr, Qabs = abs(Q);				// Project to +x +y quadrant

	float2 bx_Bx = Qabs.x * aAbB.zw;
	float2 ay_Ay = Qabs.y * aAbB.xy;

	qr.x = pow( pow( bx_Bx.x, rounds.x ) + pow( ay_Ay.x, rounds.x ), rounds.y );  // rounds.x = 2 / roundness
	qr.y = pow( pow( bx_Bx.y, rounds.x ) + pow( ay_Ay.y, rounds.x ), rounds.y );  // rounds.y = -roundness/2

	return qr * aAbB.xy * aAbB.zw;
}

// Volumetric light shaping
//
// Inputs:
//   - the point being shaded, in the local light space
//   - all information about the light shaping, including z smooth depth
//     clipping, superellipse xy shaping, and distance falloff.
// Return value:
//   - attenuation factor based on the falloff and shaping
float uberlight(float3 PL,					// Point in light space

				float3 smoothEdge0,			// edge0 for three smooth steps
				float3 smoothEdge1,			// edge1 for three smooth steps
				float3 smoothOneOverWidth,	// width of three smooth steps

				float2 shear,				// shear in X and Y
				float4 aAbB,				// Superellipse dimensions
				float2 rounds )				// two functions of roundness packed together
{
	float2 qr = ClipSuperellipse( (PL.xy / PL.z) - shear, aAbB, rounds );

	smoothEdge0.x = qr.x;					// Fill in the dynamic parts of the smoothsteps
	smoothEdge1.x = qr.y;					// The other components are pre-computed outside of the shader
	smoothOneOverWidth.x = 1.0f / ( qr.y - qr.x );
	float3 x = float3( 1, PL.z, PL.z );

	float3 atten3 = smoothstep3( smoothEdge0, smoothEdge1, smoothOneOverWidth, x );

	// Modulate the three resulting attenuations (flipping the sense of the attenuation from the superellipse and the far clip)
	return (1.0f - atten3.x) * atten3.y * (1.0f - atten3.z);
}

	#define FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION ( 1024.0f )

// JasonM - TODO: remove this simpleton version
float DoShadow( sampler DepthSampler, float4 texCoord )
{
	const float g_flShadowBias = 0.0005f;
	float2 uoffset = float2( 0.5f/FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0f );
	float2 voffset = float2( 0.0f, 0.5f/FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );
	float3 projTexCoord = texCoord.xyz / texCoord.w;
	float4 flashlightDepth = float4(	tex2D( DepthSampler, projTexCoord.xy + uoffset + voffset ).x,
										tex2D( DepthSampler, projTexCoord.xy + uoffset - voffset ).x,
										tex2D( DepthSampler, projTexCoord.xy - uoffset + voffset ).x,
										tex2D( DepthSampler, projTexCoord.xy - uoffset - voffset ).x	);

#	if ( defined( REVERSE_DEPTH_ON_X360 ) )
	{
		flashlightDepth = 1.0f - flashlightDepth;
	}
#	endif

	float shadowed = 0.0f;
	float z = texCoord.z/texCoord.w;
	float4 dz = float4(z,z,z,z) - (flashlightDepth + float4( g_flShadowBias, g_flShadowBias, g_flShadowBias, g_flShadowBias));
	float4 shadow = float4(0.25f,0.25f,0.25f,0.25f);

	if( dz.x <= 0.0f )
		shadowed += shadow.x;
	if( dz.y <= 0.0f )
		shadowed += shadow.y;
	if( dz.z <= 0.0f )
		shadowed += shadow.z;
	if( dz.w <= 0.0f )
		shadowed += shadow.w;

	return shadowed;
}


float DoShadowNvidiaRAWZOneTap( sampler DepthSampler, const float4 shadowMapPos )
{
	float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float fDepth = dot(tex2D(DepthSampler, shadowMapCenter).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));

	return fDepth > objDepth;
}


float DoShadowNvidiaRAWZ( sampler DepthSampler, const float4 shadowMapPos )
{
	float fE = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;	 // Epsilon

	float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vDepths;
	vDepths.x = dot(tex2D(DepthSampler, shadowMapCenter + float2(  fE,  fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));
	vDepths.y = dot(tex2D(DepthSampler, shadowMapCenter + float2( -fE,  fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));
	vDepths.z = dot(tex2D(DepthSampler, shadowMapCenter + float2(  fE, -fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));
	vDepths.w = dot(tex2D(DepthSampler, shadowMapCenter + float2( -fE, -fE )).arg, float3(0.996093809371817670572857294849, 0.0038909914428586627756752238080039, 1.5199185323666651467481343000015e-5));

	return dot(vDepths > objDepth.xxxx, float4(0.25, 0.25, 0.25, 0.25));
}

float DoShadowNvidiaCheap( sampler DepthSampler, const float4 shadowMapPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vTaps;
	vTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  fTexelEpsilon), objDepth, 1 ) ).x;
	vTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  fTexelEpsilon), objDepth, 1 ) ).x;
	vTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon, -fTexelEpsilon), objDepth, 1 ) ).x;
	vTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon, -fTexelEpsilon), objDepth, 1 ) ).x;

	return dot(vTaps, float4(0.25, 0.25, 0.25, 0.25));
}

float DoShadowNvidiaPCF3x3Box( sampler DepthSampler, const float3 vProjCoords )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	//float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	//float3 shadowMapCenter_objDepth = shadowMapPos.xyz * ooW;		// Do both projections at once
	float3 shadowMapCenter_objDepth = vProjCoords;

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	float flOneTaps = dot( vOneTaps, float4(1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f));

	float4 vTwoTaps;
	vTwoTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vTwoTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0, -fTexelEpsilon ), objDepth, 1 ) ).x;
	float flTwoTaps = dot( vTwoTaps, float4(1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f));

	float flCenterTap = tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x * (1.0f / 9.0f);

	// Sum all 9 Taps
	return flOneTaps + flTwoTaps + flCenterTap;
}

// 1 2 1
// 2 4 2
// 1 2 1
float DoShadowNvidiaPCF3x3Gaussian( sampler DepthSampler, const float3 shadowMapPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float3 shadowMapCenter_objDepth = shadowMapPos.xyz;

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vOneTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon, -fTexelEpsilon ), objDepth, 1 ) ).x;
	float flOneTaps = dot( vOneTaps, float4(1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f));

	float4 vTwoTaps;
	vTwoTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -fTexelEpsilon,  0 ), objDepth, 1 ) ).x;
	vTwoTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0, -fTexelEpsilon ), objDepth, 1 ) ).x;
	vTwoTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  0,  fTexelEpsilon ), objDepth, 1 ) ).x;
	float flTwoTaps = dot( vTwoTaps, float4(2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f, 2.0f / 16.0f));

	float flCenterTap = tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x * float(4.0f / 16.0f);

	// Sum all 9 Taps
	return flOneTaps + flTwoTaps + flCenterTap;
}

//
//	1	4	7	4	1
//	4	20	33	20	4
//	7	33	55	33	7
//	4	20	33	20	4
//	1	4	7	4	1
//
float DoShadowNvidiaPCF5x5GaussianPC( sampler DepthSampler, const float3 vProjCoords )
{
	float flTexelEpsilon    = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flTwoTexelEpsilon = 2.0f * flTexelEpsilon;

	//float ooW = 1.0f / shadowMapPos.w;								// 1 / w
	float3 shadowMapCenter_objDepth = vProjCoords;//shadowMapPos.xyz * ooW;		// Do both projections at once

	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space

	float4 c0 = float4( 1.0f / 331.0f, 7.0f / 331.0f, 4.0f / 331.0f, 20.0f / 331.0f );
	float4 c1 = float4( 33.0f / 331.0f, 55.0f / 331.0f, -flTexelEpsilon, 0.0f );
	float4 c2 = float4( flTwoTexelEpsilon, -flTwoTexelEpsilon, 0.0f, flTexelEpsilon );
	float4 c3 = float4( flTexelEpsilon, -flTexelEpsilon, flTwoTexelEpsilon, -flTwoTexelEpsilon );

	float4 vOneTaps;
	vOneTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xx, objDepth, 1 ) ).x;	//  2  2
	vOneTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.yx, objDepth, 1 ) ).x;	// -2  2
	vOneTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xy, objDepth, 1 ) ).x;	//  2 -2
	vOneTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.yy, objDepth, 1 ) ).x;	// -2 -2
	float flSum = dot( vOneTaps, c0.xxxx );

	float4 vSevenTaps;
	vSevenTaps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xz, objDepth, 1 ) ).x;	//  2 0
	vSevenTaps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.yz, objDepth, 1 ) ).x;	// -2 0
	vSevenTaps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.zx, objDepth, 1 ) ).x;	// 0 2
	vSevenTaps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.zy, objDepth, 1 ) ).x;	// 0 -2
	flSum += dot( vSevenTaps, c0.yyyy );

	float4 vFourTapsA, vFourTapsB;
	vFourTapsA.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.xw, objDepth, 1 ) ).x;	// 2 1
	vFourTapsA.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.wx, objDepth, 1 ) ).x;	// 1 2
	vFourTapsA.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yz, objDepth, 1 ) ).x;	// -1 2
	vFourTapsA.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.wx, objDepth, 1 ) ).x;	// -2 1
	vFourTapsB.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.wy, objDepth, 1 ) ).x;	// -2 -1
	vFourTapsB.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yw, objDepth, 1 ) ).x;	// -1 -2
	vFourTapsB.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.xw, objDepth, 1 ) ).x;	// 1 -2
	vFourTapsB.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.zy, objDepth, 1 ) ).x;	// 2 -1
	flSum += dot( vFourTapsA, c0.zzzz );
	flSum += dot( vFourTapsB, c0.zzzz );

	float4 v20Taps;
	v20Taps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.xx, objDepth, 1 ) ).x;	// 1 1
	v20Taps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yx, objDepth, 1 ) ).x;	// -1 1
	v20Taps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.xy, objDepth, 1 ) ).x;	// 1 -1
	v20Taps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c3.yy, objDepth, 1 ) ).x;	// -1 -1
	flSum += dot( v20Taps, c0.wwww );

	float4 v33Taps;
	v33Taps.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.wz, objDepth, 1 ) ).x;	// 1 0
	v33Taps.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + c1.zw, objDepth, 1 ) ).x;	// -1 0
	v33Taps.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + c1.wz, objDepth, 1 ) ).x;	// 0 -1
	v33Taps.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + c2.zw, objDepth, 1 ) ).x;	// 0 1
	flSum += dot( v33Taps, c1.xxxx );

	flSum += tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x * c1.y;
	
	flSum = pow( flSum, 1.4f );

	return flSum;
}

float DoShadowATICheap( sampler DepthSampler, const float4 shadowMapPos )
{
    float2 shadowMapCenter = shadowMapPos.xy/shadowMapPos.w;
	float objDepth = shadowMapPos.z / shadowMapPos.w;
	float fSampleDepth = tex2D( DepthSampler, shadowMapCenter ).x;

	objDepth = min( objDepth, 0.99999 ); //HACKHACK: On 360, surfaces at or past the far flashlight plane have an abrupt cutoff. This is temp until a smooth falloff is implemented

	return fSampleDepth > objDepth;
}


// Smooth filter using ATI Fetch 4
float DoShadowATIFetch4( sampler DepthSampler, const float3 vProjCoords )
{
	// This should only ever get run on a ps_3_0 part
	#if ( !defined( SHADER_MODEL_PS_3_0 ) )
	{
		return 1.0f;
	}
	#endif

	float4  shadowMapVals[ 4 ];
	float4	shadowMapWeights[ 4 ];
	// Important: This shader was originally in DoTA. To get this shader working in Portal2, I had to eliminate the -.5f offsets and weird swizzle.
	// I'm not positive, but the min/mag filter settings must differ between the two titles, which might account for the difference?
	float4  quadOffsets[ 4 ] = 
	{
		{ -1.0f, -1.0f, 0, 0 },
		{  1.0f, -1.0f, 0, 0 },
		{ -1.0f,  1.0f, 0, 0 },
		{  1.0f,  1.0f, 0, 0 },		
	};

	float3 shadowMapCenter_objDepth = vProjCoords;
	float2 shadowMapCenter = shadowMapCenter_objDepth.xy;			// Center of shadow filter
	float objDepth = shadowMapCenter_objDepth.z;					// Object depth in shadow space
	float4 vFullTexelOffset = float4( 1.0 / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 1.0 / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0, 0.0 );

	float2 vTexRes = float2( FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION );

	// Fetch 4 2x2 quads
	shadowMapVals[ 0 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 0 ].xy ) );
	shadowMapVals[ 1 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 1 ].xy ) );
	shadowMapVals[ 2 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 2 ].xy ) );
	shadowMapVals[ 3 ] = tex2D( DepthSampler, shadowMapCenter + ( vFullTexelOffset.xy * quadOffsets[ 3 ].xy ) );

	// Fraction component of projected coordinates
	float2 pFrac = frac( shadowMapCenter * vTexRes );

	shadowMapWeights[ 0 ] = float4(	1,			1 - pFrac.x,	1,			1 - pFrac.x );
	shadowMapWeights[ 1 ] = float4( pFrac.x,	1,				pFrac.x,	1 );
	shadowMapWeights[ 2 ] = float4( 1,			1 - pFrac.x,	1,			1 - pFrac.x);
	shadowMapWeights[ 3 ] = float4( pFrac.x,	1,				pFrac.x,	1 );

	shadowMapWeights[ 0 ] *= float4(  1 - pFrac.y,  1,       1,       1 - pFrac.y  );
	shadowMapWeights[ 1 ] *= float4(  1 - pFrac.y,  1,       1,       1 - pFrac.y  );
	shadowMapWeights[ 2 ] *= float4(  1,			pFrac.y, pFrac.y, 1			   );
	shadowMapWeights[ 3 ] *= float4(  1,			pFrac.y, pFrac.y, 1            );

	// Projective distance from z plane in view coords
	float flDist = objDepth - 0.005;
	float4 dist = float4( flDist, flDist, flDist, flDist );

	float4 inLight = ( dist < shadowMapVals[ 0 ] );
	float percentInLight = dot( inLight, shadowMapWeights[ 0 ] );

	inLight = ( dist < shadowMapVals[ 1 ] );
	percentInLight += dot( inLight, shadowMapWeights[ 1 ] );

	inLight = ( dist < shadowMapVals[ 2 ] );
	percentInLight += dot( inLight, shadowMapWeights[ 2 ] );

	inLight = ( dist < shadowMapVals[ 3 ] );
	percentInLight += dot( inLight, shadowMapWeights[ 3 ] );

	// Sum of weights is 9 since border taps are bilinearly filtered
	return ( 1.0f / 9.0f ) * percentInLight;
}

// Bilinear Percentage Closer Filtering, fetching depths and manually doing the four compares and the bilerp
float DoShadowATIBilinear( sampler DepthSampler, const float3 vProjCoords )
{
	float2 vPositionLs = vProjCoords.xy;
	float flComparisonDepth = vProjCoords.z;
	
	// Emulate bilinear PCF - shader originally from source2 (src/shaders/include/sun_shadowing.fxc).
	float flSunShadowingShadowTextureWidth = FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flSunShadowingShadowTextureHeight = FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flSunShadowingInvShadowTextureWidth = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	float flSunShadowingInvShadowTextureHeight = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;
	
	float2 vFracPositionLs = frac( vPositionLs * float2( flSunShadowingShadowTextureWidth, flSunShadowingShadowTextureHeight ) );
	float2 vSamplePositionLs = vPositionLs - vFracPositionLs * float2( flSunShadowingInvShadowTextureWidth, flSunShadowingInvShadowTextureHeight );
	
	float4 vCmpSamples;		
	vCmpSamples.x = tex2D( DepthSampler, vSamplePositionLs + float2( 0.0f * flSunShadowingInvShadowTextureWidth, 0.0f * flSunShadowingInvShadowTextureHeight ) ).x;
	vCmpSamples.y = tex2D( DepthSampler, vSamplePositionLs + float2( 1.0f * flSunShadowingInvShadowTextureWidth, 0.0f * flSunShadowingInvShadowTextureHeight ) ).x;
	vCmpSamples.z = tex2D( DepthSampler, vSamplePositionLs + float2( 0.0f * flSunShadowingInvShadowTextureWidth, 1.0f * flSunShadowingInvShadowTextureHeight ) ).x;
	vCmpSamples.w = tex2D( DepthSampler, vSamplePositionLs + float2( 1.0f * flSunShadowingInvShadowTextureWidth, 1.0f * flSunShadowingInvShadowTextureHeight ) ).x;

	vCmpSamples = vCmpSamples > flComparisonDepth;
	
	float4 vFactors = float4( ( 1.0f - vFracPositionLs.x ) * ( 1.0f - vFracPositionLs.y ), vFracPositionLs.x   * ( 1.0f - vFracPositionLs.y ),
							  ( 1.0f - vFracPositionLs.x ) *          vFracPositionLs.y,   vFracPositionLs.x   *          vFracPositionLs.y );
		 		
	return dot( vCmpSamples, vFactors );
}

// Poisson disc, randomly rotated at different UVs
float DoShadowPoisson16Sample( sampler DepthSampler, sampler RandomRotationSampler, const float3 vProjCoords, const float2 vScreenPos, const float4 vShadowTweaks, bool bNvidiaHardwarePCF, bool bFetch4 )
{
	float2 vPoissonOffset[8] = { float2(  0.3475f,  0.0042f ), float2(  0.8806f,  0.3430f ), float2( -0.0041f, -0.6197f ), float2(  0.0472f,  0.4964f ),
								 float2( -0.3730f,  0.0874f ), float2( -0.9217f, -0.3177f ), float2( -0.6289f,  0.7388f ), float2(  0.5744f, -0.7741f ) };

	float flScaleOverMapSize = vShadowTweaks.x * 2;		// Tweak parameters to shader
	float2 vNoiseOffset = vShadowTweaks.zw;
	float4 vLightDepths = 0, accum = 0.0f;
	float2 rotOffset = 0;

	float2 shadowMapCenter = vProjCoords.xy;			// Center of shadow filter
	float objDepth = min( vProjCoords.z, 0.99999 );		// Object depth in shadow space

	// 2D Rotation Matrix setup
	float3 RMatTop = 0, RMatBottom = 0;
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	RMatTop.xy = tex2D( RandomRotationSampler, cFlashlightScreenScale.xy * (vScreenPos * 0.5 + 0.5) + vNoiseOffset).xy * 2.0 - 1.0;
	RMatBottom.xy = float2(-1.0, 1.0) * RMatTop.yx;	// 2x2 rotation matrix in 4-tuple
#endif

	RMatTop *= flScaleOverMapSize;				// Scale up kernel while accounting for texture resolution
	RMatBottom *= flScaleOverMapSize;

	RMatTop.z = shadowMapCenter.x;				// To be added in d2adds generated below
	RMatBottom.z = shadowMapCenter.y;
	
	float fResult = 0.0f;

	if ( bNvidiaHardwarePCF )
	{
		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[0].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[0].xy ) + RMatBottom.z;
		vLightDepths.x += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[1].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[1].xy ) + RMatBottom.z;
		vLightDepths.y += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[2].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[2].xy ) + RMatBottom.z;
		vLightDepths.z += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[3].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[3].xy ) + RMatBottom.z;
		vLightDepths.w += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4].xy ) + RMatBottom.z;
		vLightDepths.x += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[5].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[5].xy ) + RMatBottom.z;
		vLightDepths.y += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[6].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[6].xy ) + RMatBottom.z;
		vLightDepths.z += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[7].xy ) + RMatTop.z;
		rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[7].xy ) + RMatBottom.z;
		vLightDepths.w += tex2Dproj( DepthSampler, float4(rotOffset, objDepth, 1) ).x;

		// This should actually be float4( 0.125, 0.125, 0.125, 0.125) but we've tuned so many shots in the SFM
		// relying on this bug that it doesn't seem right to fix until we have done something like move
		// this code out to a staging branch for a shipping game.
		// This is certainly one source of difference between ATI and nVidia in SFM layoffs
		return dot( vLightDepths, float4( 0.25, 0.25, 0.25, 0.25) );
	}
	else if ( bFetch4 )
	{
		for( int i=0; i<8; i++ )
		{
			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[i].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[i].xy ) + RMatBottom.z;
			vLightDepths = tex2D( DepthSampler, rotOffset.xy );
			accum += (vLightDepths > objDepth.xxxx);
		}

		return dot( accum, float4( 1.0f/32.0f, 1.0f/32.0f, 1.0f/32.0f, 1.0f/32.0f) );
	}
	else	// ATI vanilla hardware shadow mapping
	{
		for( int i=0; i<2; i++ )
		{
			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+0].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+0].xy ) + RMatBottom.z;
			vLightDepths.x = tex2D( DepthSampler, rotOffset.xy ).x;

			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+1].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+1].xy ) + RMatBottom.z;
			vLightDepths.y = tex2D( DepthSampler, rotOffset.xy ).x;

			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+2].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+2].xy ) + RMatBottom.z;
			vLightDepths.z = tex2D( DepthSampler, rotOffset.xy ).x;

			rotOffset.x = dot( RMatTop.xy,    vPoissonOffset[4*i+3].xy ) + RMatTop.z;
			rotOffset.y = dot( RMatBottom.xy, vPoissonOffset[4*i+3].xy ) + RMatBottom.z;
			vLightDepths.w = tex2D( DepthSampler, rotOffset.xy ).x;

			accum += (vLightDepths > objDepth.xxxx);
		}

		return dot( accum, float4( 0.125, 0.125, 0.125, 0.125 ) );
	}
}


float AmountShadowed_1Tap_NVidiaPCF( sampler DepthSampler, float3 vProjPos )
{
	float2 shadowMapCenter = vProjPos.xy;
	float objDepth = vProjPos.z;

	return tex2Dproj( DepthSampler, float4( shadowMapCenter, objDepth, 1 ) ).x;
}

float AmountShadowed_4Tap_NVidiaPCF( sampler DepthSampler, float3 vProjPos )
{
	float fTexelEpsilon = 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION;

	float2 shadowMapCenter = vProjPos.xy;
	float objDepth = vProjPos.z;

	float4 s;
	s.x = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2( -1.0f,   .5f ) * fTexelEpsilon, objDepth, 1 ) ).x;
	s.y = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  -.5f, -1.0f ) * fTexelEpsilon, objDepth, 1 ) ).x;
	s.z = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(   .5f, -1.0f ) * fTexelEpsilon, objDepth, 1 ) ).x;
	s.w = tex2Dproj( DepthSampler, float4( shadowMapCenter + float2(  1.0f,  -.5f ) * fTexelEpsilon, objDepth, 1 ) ).x;
		
	return dot( s, float4( .25f, .25f, .25f, .25f ) );
}

float AmountShadowed_5Tap_NVidiaPCF( sampler DepthSampler, float3 vProjPos )
{
	float2 vShadowMapCenter = vProjPos.xy;
	float4 vTexelEpsilon = float4( -1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 1.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0f, 1.0f );
		
	HALF4 r;
	r.x = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.xz, vProjPos.z, 1.0f ) ).x;
	r.y = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.yz, vProjPos.z, 1.0f ) ).y;
	r.z = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.zx, vProjPos.z, 1.0f ) ).z;
	r.w = tex2Dproj( DepthSampler, float4( vShadowMapCenter + vTexelEpsilon.zy, vProjPos.z, 1.0f ) ).w;
	HALF flSum = dot( r, HALF4( .175f, .175f, .175f, .175f ) );
	
	flSum += (HALF)( tex2Dproj( DepthSampler, float4( vShadowMapCenter, vProjPos.z, 1.0f ) ).x * (HALF)( .3f ) );

	return flSum;
}



HALF DoFlashlightShadow( sampler DepthSampler, sampler RandomRotationSampler, float3 vProjCoords, float2 vScreenPos, int nShadowLevel, float4 vShadowTweaks, float nDotL = 1.0f )
{
	HALF flShadow = 1.0h;

	if ( nShadowLevel == NVIDIA_PCF )
	{
#if defined( SHADER_MODEL_PS_2_0 ) || defined( SHADER_MODEL_PS_2_B )
		flShadow = AmountShadowed_4Tap_NVidiaPCF( DepthSampler, vProjCoords );	// This is pretty much just high-end Macs
#else
		flShadow = DoShadowNvidiaPCF5x5GaussianPC( DepthSampler, vProjCoords ); // NVIDIA ps_3 parts and ATI DX10 parts
#endif
	}
	else if( nShadowLevel == ATI_NO_PCF_FETCH4 )
	{
		flShadow = DoShadowATIFetch4( DepthSampler, vProjCoords );				// ATI DX9 ps_3_0 parts 
	}
	else if ( nShadowLevel == NVIDIA_PCF_CHEAP )
	{
		flShadow = AmountShadowed_1Tap_NVidiaPCF( DepthSampler, vProjCoords );	// Low-end NVIDIA parts and low-end Macs
	}
	else if( nShadowLevel == ATI_NOPCF )
	{
		flShadow = DoShadowATIBilinear( DepthSampler, vProjCoords );			// ATI ps_2_b parts
	}

	return flShadow;
}

float3 SpecularLight( const float3 vWorldNormal, const float3 vLightDir, const float fSpecularExponent,
					  const float3 vEyeDir, const bool bDoSpecularWarp, in sampler specularWarpSampler, float fFresnel )
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	float3 vReflect = reflect( -vEyeDir, vWorldNormal );			// Reflect view through normal
	float3 vSpecular = saturate(dot( vReflect, vLightDir ));		// L.R	(use half-angle instead?)
	vSpecular = pow( vSpecular.x, fSpecularExponent );				// Raise to specular power

	// Optionally warp as function of scalar specular and fresnel
	if ( bDoSpecularWarp )
		vSpecular *= tex2D( specularWarpSampler, float2(vSpecular.x, fFresnel) ).rgb; // Sample at { (L.R)^k, fresnel }

	return vSpecular;
}

float RemapNormalizedValClamped( float val, float A, float B)
{
	return saturate( (val - A) / (B - A) );
}

void DoSpecularFlashlight( float3 flashlightPos, float3 worldPos, float4 flashlightSpacePosition, float3 worldNormal,  
					float3 attenuationFactors, float farZ, sampler FlashlightSampler, sampler FlashlightDepthSampler, sampler RandomRotationSampler,
					int nShadowLevel, bool bDoShadows, const float2 vScreenPos, const float fSpecularExponent, const float3 vEyeDir,
					const bool bDoDiffuseWarp, sampler DiffuseWarpSampler, const bool bDoSpecularWarp, sampler specularWarpSampler, float fFresnel, float4 vShadowTweaks,

					// Outputs of this shader...separate shadowed diffuse and specular from the flashlight
					out float3 diffuseLighting, out float3 specularLighting )
{
	float3 vProjCoords = flashlightSpacePosition.xyz / flashlightSpacePosition.w;
	float3 flashlightColor = float3(1,1,1);

	flashlightColor = tex2D( FlashlightSampler, vProjCoords.xy ).rgb;

#if defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	flashlightColor *= flashlightSpacePosition.www > float3(0,0,0);	// Catch back projection (ps2b and up)
#endif

#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	flashlightColor *= cFlashlightColor.xyz;						// Flashlight color
#endif

	float3 delta = flashlightPos - worldPos;
	float3 L = normalize( delta );
	float distSquared = dot( delta, delta );
	float dist = sqrt( distSquared );

	float endFalloffFactor = RemapNormalizedValClamped( dist, farZ, 0.6f * farZ );

	// Attenuation for light and to fade out shadow over distance
	float fAtten = saturate( dot( attenuationFactors, float3( 1.0f, 1.0f/dist, 1.0f/distSquared ) ) );
	
	float NdotL = dot( L.xyz, worldNormal.xyz );

	// Shadowing and coloring terms
#if (defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0))
	if ( bDoShadows )
	{
		float flShadow = DoFlashlightShadow( FlashlightDepthSampler, RandomRotationSampler, vProjCoords, vScreenPos, nShadowLevel, vShadowTweaks, NdotL );
		float flAttenuated = lerp( flShadow, 1.0f, vShadowTweaks.y );	// Blend between fully attenuated and not attenuated
		flShadow = saturate( lerp( flAttenuated, flShadow, fAtten ) );	// Blend between shadow and above, according to light attenuation
		flashlightColor *= flShadow;									// Shadow term
	}
#endif

	diffuseLighting = fAtten;
	
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	NdotL += flFlashlightNoLambertValue;
#endif
	diffuseLighting *= saturate( NdotL ); // Lambertian term

	diffuseLighting *= flashlightColor;
	diffuseLighting *= endFalloffFactor;

	// Specular term (masked by diffuse)
	specularLighting = diffuseLighting * SpecularLight ( worldNormal, L, fSpecularExponent, vEyeDir, bDoSpecularWarp, specularWarpSampler, fFresnel );
}

// Diffuse only version
HALF3 DoFlashlight( float3 flashlightPos, float3 worldPos, float4 flashlightSpacePosition, float3 worldNormal, 
					float3 attenuationFactors, float farZ, sampler FlashlightSampler, sampler FlashlightDepthSampler,
					sampler RandomRotationSampler, int nShadowLevel, bool bDoShadows,
					const float2 vScreenPos, bool bClip, float4 vShadowTweaks = float4( 3.0f / FLASHLIGHT_SHADOW_TEXTURE_RESOLUTION, 0.0005f, 0.0f, 0.0f), bool bHasNormal = true )
{
	float3 vProjCoords = flashlightSpacePosition.xyz / flashlightSpacePosition.w;
	// rg - Always fetching the flashlight texture on X360 so the GPU computes the LOD factors used to select the mipmap level properly. 
	// Otherwise, if we fetch after the [branch] we'll sometimes get edge artifacts because the GPU fetches from a lower mipmap level when it shouldn't.
	HALF3 flashlightColor = h3tex2D( FlashlightSampler, vProjCoords.xy ).rgb;
    	
			
	#if	( defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0) )
		flashlightColor *= (HALF)(flashlightSpacePosition.www > float3(0,0,0));	// Catch back projection (ps2b and up)
	#endif

	#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	{
		flashlightColor *= (HALF3)cFlashlightColor.xyz;						// Flashlight color
	}
	#endif

	float3 delta = flashlightPos - worldPos;
	HALF3 L = normalize( delta );
	float distSquared = dot( delta, delta );
	float dist = sqrt( distSquared );

	HALF endFalloffFactor = RemapNormalizedValClamped( dist, farZ, 0.6f * farZ );

	// Attenuation for light and to fade out shadow over distance
	HALF fAtten = saturate( dot( attenuationFactors, float3( 1.0f, 1.0f/dist, 1.0f/distSquared ) ) );
	
	HALF flLDotWorldNormal;
	if ( bHasNormal )
	{
		flLDotWorldNormal = dot( L.xyz, (HALF3)worldNormal.xyz );
	}
	else
	{
		flLDotWorldNormal = 1.0h;
	}

	// Shadowing and coloring terms
#if (defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0))
	if ( bDoShadows )
	{
		HALF flShadow = DoFlashlightShadow( FlashlightDepthSampler, RandomRotationSampler, vProjCoords, vScreenPos, nShadowLevel, vShadowTweaks, flLDotWorldNormal );
		HALF flAttenuated = lerp( saturate( flShadow ), 1.0h, (HALF)vShadowTweaks.y );	// Blend between fully attenuated and not attenuated
		flShadow = saturate( lerp( flAttenuated, flShadow, (HALF)fAtten ) );	// Blend between shadow and above, according to light attenuation
		flashlightColor *= flShadow;									// Shadow term
	}
#endif

	HALF3 diffuseLighting = fAtten;
    	
#if defined(SHADER_MODEL_PS_2_0) || defined(SHADER_MODEL_PS_2_B) || defined(SHADER_MODEL_PS_3_0)
	diffuseLighting *= saturate( flLDotWorldNormal + (HALF)flFlashlightNoLambertValue ); // Lambertian term
#else
	diffuseLighting *= saturate( flLDotWorldNormal ); // Lambertian (not Half-Lambert) term
#endif

	diffuseLighting *= flashlightColor;
	diffuseLighting *= endFalloffFactor;

	return diffuseLighting;
}

#endif //#ifndef COMMON_FLASHLIGHT_FXC_H_
