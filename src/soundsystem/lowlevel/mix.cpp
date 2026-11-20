import std;
import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.fltx4;
import ks.math.ssemath;
import ks.math.ssequaternion;
import ks.platform;
import ks.simd;
#include "soundsystem/lowlevel.h"

// simple inline to test alignemnt of a value
inline bool IsAlign4( uint nAlign )
{
	return ( nAlign & 3 ) == 0;
}

inline bool IsAligned16Bytes( void *p )
{
	return ( uintp( p ) & 0xF ) ? false : true;
}

[[gnu::always_inline]] inline shortx8 ShiftRightShortSIMD( const shortx8 &inputValue, const shortx8 &shiftBitCount )
{
	return _mm_srl_epi16( inputValue, shiftBitCount );
}

[[gnu::always_inline]] inline shortx8 SignedExtractLowAsInt32( const shortx8 &a )
{
	shortx8 signExtend = _mm_cmplt_epi16( a, _mm_setzero_si128() );
	return _mm_unpacklo_epi16( a, signExtend );
}

[[gnu::always_inline]] inline shortx8 SignedExtractHighAsInt32( const shortx8 &a )
{
	shortx8 signExtend = _mm_cmplt_epi16( a, _mm_setzero_si128() );
	return _mm_unpackhi_epi16( a, signExtend );
}

[[gnu::always_inline]] inline shortx8 RoundtFloatToInt32( const fltx4 &input )
{
	return _mm_cvtps_epi32( input );
}

[[gnu::always_inline]] inline shortx8 PackInt32x2ToShortx8( const shortx8 &input0, const shortx8 &input1 )
{
	return _mm_packs_epi32( input0, input1 );
}

// Load 4 aligned words into a SIMD register
[[gnu::always_inline]] inline shortx8 LoadAlignedShortx8SIMD( const void * __restrict__ pSIMD )
{
	return _mm_load_si128( reinterpret_cast<const __m128i *>( pSIMD ) );
}

// Load 4 unaligned words into a SIMD register
[[gnu::always_inline]] inline shortx8 LoadUnalignedShortx8SIMD( const void * __restrict__ pSIMD )
{
	return ks::simd::load_unaligned_i( reinterpret_cast<const __m128i *>( pSIMD ) );
}

// create a stereo interleaved signed-16 buffer from two float-32 buffers
void ConvertFloat32Int16_Clamp_Interleave2_Unaligned( short *pOut, float *pflInputLeft, float *pflInputRight, int nSampleCount )
{
	if ( nSampleCount >= 8 )
	{
		int nSampleQuads = nSampleCount >> 2;
		// truncate sample count to remainder after 4-bundles
		nSampleCount &= 3;

		short *pWrite = pOut;
		for ( int i = 0; i < nSampleQuads; i++ )
		{
			// load 4 samples from left and four from right
			fltx4 leftSamples = LoadAlignedSIMD( pflInputLeft );
			pflInputLeft += 4;
			fltx4 rightSamples = LoadAlignedSIMD( pflInputRight );
			pflInputRight += 4;
			shortx8 nLeft = RoundtFloatToInt32( leftSamples );
			shortx8 nRight = RoundtFloatToInt32( rightSamples );
			// interleave into L/R pairs
			shortx8 nInterleavedLow = _mm_unpacklo_epi32( nLeft, nRight );
			shortx8 nInterleavedHigh = _mm_unpackhi_epi32( nLeft, nRight );
			// pack 
			shortx8 nOut = PackInt32x2ToShortx8( nInterleavedLow, nInterleavedHigh );
			StoreUnalignedSIMD( pWrite, nOut );
			pWrite += 8;
		}
	}

	// now convert and clamp any remaining samples (not in SIMD 4-bundles)
	for ( int i = 0; i < nSampleCount; i++ )
	{
		int l = (int)pflInputLeft[i];
		if ( l < -32768 ) l = -32768;
		if ( l > 32767 ) l = 32767;
		int r = (int)pflInputRight[i];
		if ( r < -32768 ) r = -32768;
		if ( r > 32767 ) r = 32767;
		pOut[0] = l;
		pOut[1] = r;
		pOut += 2;
	}
}

void ConvertFloat32Int16_Clamp_Interleave2( short *pOut, float *pflInputLeft, float *pflInputRight, int nSampleCount )
{
	if ( !IsAligned16Bytes(pOut) )
	{
		ConvertFloat32Int16_Clamp_Interleave2_Unaligned( pOut, pflInputLeft, pflInputRight, nSampleCount );
		return;
	}
	if ( nSampleCount >= 8 )
	{
		int nSampleQuads = nSampleCount >> 2;

		// truncate sample count to remainder after 4-bundles
		nSampleCount &= 3;

		short *pWrite = pOut;
		for ( int i = 0; i < nSampleQuads; i++ )
		{
			// load 4 samples from left and four from right
			fltx4 leftSamples = LoadAlignedSIMD( pflInputLeft );
			pflInputLeft += 4;
			fltx4 rightSamples = LoadAlignedSIMD( pflInputRight );
			pflInputRight += 4;
			shortx8 nLeft = RoundtFloatToInt32( leftSamples );
			shortx8 nRight = RoundtFloatToInt32( rightSamples );
			shortx8 nInterleavedLow = _mm_unpacklo_epi32( nLeft, nRight );
			shortx8 nInterleavedHigh = _mm_unpackhi_epi32( nLeft, nRight );
			shortx8 nOut = PackInt32x2ToShortx8( nInterleavedLow, nInterleavedHigh );
			StoreAlignedSIMD( pWrite, nOut );

			pWrite += 8;
		}
	}

	// now convert and clamp any remaining samples (not in SIMD 4-bundles)
	for ( int i = 0; i < nSampleCount; i++ )
	{
		int l = (int)pflInputLeft[i];
		if ( l < -32768 ) l = -32768;
		if ( l > 32767 ) l = 32767;
		int r = (int)pflInputRight[i];
		if ( r < -32768 ) r = -32768;
		if ( r > 32767 ) r = 32767;
		pOut[0] = l;
		pOut[1] = r;
		pOut += 2;
	}
}

// Faster SIMD version for 6-in, 6-out
void ConvertFloat32Int16_Clamp_Interleave6( short *pOut, int nOutputChannelCount, int nChannelStrideFloats, float *pflChannel0, int nInputChannelCount, int nSampleCount )
{
	contract_assert( nOutputChannelCount == 6 && nInputChannelCount == 6 && IsAligned16Bytes( pflChannel0 ) );
	const float *pInput0 = pflChannel0;
	const float *pInput1 = pflChannel0 + nChannelStrideFloats;
	const float *pInput2 = pflChannel0 + 2*nChannelStrideFloats;
	const float *pInput3 = pflChannel0 + 3*nChannelStrideFloats;
	const float *pInput4 = pflChannel0 + 4*nChannelStrideFloats;
	const float *pInput5 = pflChannel0 + 5*nChannelStrideFloats;
	short *pWrite = pOut;
	// process 24 samples per loop, grab 6 bundles of 4, write out 3 bundles of 8
	for ( int i = 0; i < nSampleCount; i += 4 )
	{
		// grab 6 bundles of 4 samples
		fltx4 fl4Samples0 = LoadAlignedSIMD( pInput0 + i ); //  0  6 12 18
		fltx4 fl4Samples1 = LoadAlignedSIMD( pInput1 + i ); //  1  7 13 19
		fltx4 fl4Samples2 = LoadAlignedSIMD( pInput2 + i ); //  2  8 14 20
		fltx4 fl4Samples3 = LoadAlignedSIMD( pInput3 + i ); //  3  9 15 21
		fltx4 fl4Samples4 = LoadAlignedSIMD( pInput4 + i ); //  4 10 16 22
		fltx4 fl4Samples5 = LoadAlignedSIMD( pInput5 + i ); //  5 11 17 23

		// interleave into pairs
		fltx4 fl4Pair0 = ks::simd::shuffle<0,1,0,1>( fl4Samples0, fl4Samples1 );	//  0  6  1  7
		fltx4 fl4Pair1 = ks::simd::shuffle<2,3,2,3>( fl4Samples0, fl4Samples1 );	// 12 18 13 19
		fltx4 fl4Pair2 = ks::simd::shuffle<0,1,0,1>( fl4Samples2, fl4Samples3 );	//  2  8  3  9
		fltx4 fl4Pair3 = ks::simd::shuffle<2,3,2,3>( fl4Samples2, fl4Samples3 );	// 14 20 15 21
		fltx4 fl4Pair4 = ks::simd::shuffle<0,1,0,1>( fl4Samples4, fl4Samples5 );	//  4 10  5 11
		fltx4 fl4Pair5 = ks::simd::shuffle<2,3,2,3>( fl4Samples4, fl4Samples5 );	// 16 22 17 23

		// now put in final order
		fltx4 fl4Out0 = ks::simd::shuffle<0,2,0,2>( fl4Pair0, fl4Pair2 );	//  0  1  2  3
		fltx4 fl4Out1 = ks::simd::shuffle<0,2,1,3>( fl4Pair4, fl4Pair0 );	//  4  5  6  7
		fltx4 fl4Out2 = ks::simd::shuffle<1,3,1,3>( fl4Pair2, fl4Pair4 );	//  8  9 10 11
		fltx4 fl4Out3 = ks::simd::shuffle<0,2,0,2>( fl4Pair1, fl4Pair3 );	// 12 13 14 15
		fltx4 fl4Out4 = ks::simd::shuffle<0,2,1,3>( fl4Pair5, fl4Pair1 );	// 16 17 18 19
		fltx4 fl4Out5 = ks::simd::shuffle<1,3,1,3>( fl4Pair3, fl4Pair5 );	// 20 21 22 23

		// pack into 3 bundles of 8
		shortx8 nOut0 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out0 ), RoundtFloatToInt32( fl4Out1 ) );
		shortx8 nOut1 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out2 ), RoundtFloatToInt32( fl4Out3 ) );
		shortx8 nOut2 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out4 ), RoundtFloatToInt32( fl4Out5 ) );
		// NOTE: Optimize alignment?
		StoreUnalignedSIMD( pWrite, nOut0 );
		StoreUnalignedSIMD( pWrite + 8, nOut1 );
		StoreUnalignedSIMD( pWrite + 16, nOut2 );
		pWrite += 24;
	}
}


// Faster SIMD version for 8-in, 8-out
void ConvertFloat32Int16_Clamp_Interleave8( short *pOut, int nOutputChannelCount, int nChannelStrideFloats, float *pflChannel0, int nInputChannelCount, int nSampleCount )
{
	contract_assert( nOutputChannelCount == 8 && nInputChannelCount == 8 && IsAligned16Bytes( pflChannel0 ) );
	const float *pInput0 = pflChannel0;
	const float *pInput1 = pflChannel0 + nChannelStrideFloats;
	const float *pInput2 = pflChannel0 + 2 * nChannelStrideFloats;
	const float *pInput3 = pflChannel0 + 3 * nChannelStrideFloats;
	const float *pInput4 = pflChannel0 + 4 * nChannelStrideFloats;
	const float *pInput5 = pflChannel0 + 5 * nChannelStrideFloats;
	const float *pInput6 = pflChannel0 + 6 * nChannelStrideFloats;
	const float *pInput7 = pflChannel0 + 7 * nChannelStrideFloats;
	short *pWrite = pOut;
	// process 32 samples per loop, grab 6 bundles of 4, write out 4 bundles of 8
	for ( int i = 0; i < nSampleCount; i += 4 )
	{
		// grab 8 bundles of 4 samples
		fltx4 fl4Samples0 = LoadAlignedSIMD( pInput0 + i ); //  0  8 16 24
		fltx4 fl4Samples1 = LoadAlignedSIMD( pInput1 + i ); //  1  9 17 25
		fltx4 fl4Samples2 = LoadAlignedSIMD( pInput2 + i ); //  2 10 18 26
		fltx4 fl4Samples3 = LoadAlignedSIMD( pInput3 + i ); //  3 11 19 27
		fltx4 fl4Samples4 = LoadAlignedSIMD( pInput4 + i ); //  4 12 20 28
		fltx4 fl4Samples5 = LoadAlignedSIMD( pInput5 + i ); //  5 13 21 29
		fltx4 fl4Samples6 = LoadAlignedSIMD( pInput6 + i ); //  6 14 22 30 
		fltx4 fl4Samples7 = LoadAlignedSIMD( pInput7 + i ); //  7 15 23 31

		// interleave into pairs
		fltx4 fl4Pair0 = ks::simd::shuffle<0,1,0,1>( fl4Samples0, fl4Samples1 );	//  0  8  1  9
		fltx4 fl4Pair1 = ks::simd::shuffle<2,3,2,3>( fl4Samples0, fl4Samples1 );	// 16 24 17 25
		fltx4 fl4Pair2 = ks::simd::shuffle<0,1,0,1>( fl4Samples2, fl4Samples3 );	//  2 10  3 11
		fltx4 fl4Pair3 = ks::simd::shuffle<2,3,2,3>( fl4Samples2, fl4Samples3 );	// 18 26 19 27
		fltx4 fl4Pair4 = ks::simd::shuffle<0,1,0,1>( fl4Samples4, fl4Samples5 );	//  4 12  5 13
		fltx4 fl4Pair5 = ks::simd::shuffle<2,3,2,3>( fl4Samples4, fl4Samples5 );	// 20 28 21 29
		fltx4 fl4Pair6 = ks::simd::shuffle<0,1,0,1>( fl4Samples6, fl4Samples7 );	//  6 14  7 15
		fltx4 fl4Pair7 = ks::simd::shuffle<2,3,2,3>( fl4Samples6, fl4Samples7 );	// 22 30 23 31

		// now put in final order
		fltx4 fl4Out0 = ks::simd::shuffle<0,2,0,2>( fl4Pair0, fl4Pair2 );	//  0  1  2  3
		fltx4 fl4Out1 = ks::simd::shuffle<0,2,0,2>( fl4Pair4, fl4Pair6 );	//  4  5  6  7
		fltx4 fl4Out2 = ks::simd::shuffle<1,3,1,3>( fl4Pair0, fl4Pair2 );	//  8  9 10 11
		fltx4 fl4Out3 = ks::simd::shuffle<1,3,1,3>( fl4Pair4, fl4Pair6 );	// 12 13 14 15
		fltx4 fl4Out4 = ks::simd::shuffle<0,2,0,2>( fl4Pair1, fl4Pair3 );	// 16 17 18 19
		fltx4 fl4Out5 = ks::simd::shuffle<0,2,0,2>( fl4Pair5, fl4Pair7 );	// 20 21 22 23
		fltx4 fl4Out6 = ks::simd::shuffle<1,3,1,3>( fl4Pair1, fl4Pair3 );	// 24 25 26 27
		fltx4 fl4Out7 = ks::simd::shuffle<1,3,1,3>( fl4Pair5, fl4Pair7 );	// 28 29 30 31

		// pack into 4 bundles of 8
		shortx8 nOut0 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out0 ), RoundtFloatToInt32( fl4Out1 ) );
		shortx8 nOut1 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out2 ), RoundtFloatToInt32( fl4Out3 ) );
		shortx8 nOut2 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out4 ), RoundtFloatToInt32( fl4Out5 ) );
		shortx8 nOut3 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out6 ), RoundtFloatToInt32( fl4Out7 ) );
		// NOTE: Optimize alignment?
		StoreUnalignedSIMD( pWrite, nOut0 );
		StoreUnalignedSIMD( pWrite + 8, nOut1 );
		StoreUnalignedSIMD( pWrite + 16, nOut2 );
		StoreUnalignedSIMD( pWrite + 24, nOut3 );
		pWrite += 32;
	}
}

// slow version to support 4/6/8 channel devices
void ConvertFloat32Int16_Clamp_InterleaveStride( short *pOut, int nOutputChannelCount, int nChannelStrideFloats, float *pflChannel0, int nInputChannelCount, int nSampleCount )
{
	// detect optimizable cases and call fast code
	if ( nInputChannelCount == 6 && nOutputChannelCount == 6 && IsAlign4( nSampleCount ) )
	{
		ConvertFloat32Int16_Clamp_Interleave6( pOut, nOutputChannelCount, nChannelStrideFloats, pflChannel0, nInputChannelCount, nSampleCount );
		return;
	}
	if ( nInputChannelCount == 8 && nOutputChannelCount == 8 && IsAlign4( nSampleCount ) )
	{
		ConvertFloat32Int16_Clamp_Interleave8( pOut, nOutputChannelCount, nChannelStrideFloats, pflChannel0, nInputChannelCount, nSampleCount );
		return;
	}

	// run the slower code in this case
	if ( nOutputChannelCount > nInputChannelCount )
	{
		for ( int i = 0; i < nSampleCount; i++ )
		{
			float *pIn = pflChannel0 + i;
			for ( int j = 0; j < nInputChannelCount; j++ )
			{
				int nOut = int( pIn[0] );
				nOut = clamp( nOut, -32768, 32767 );
				*pOut++ = nOut;
				pIn += nChannelStrideFloats;
			}
			for ( int j = nInputChannelCount; j < nOutputChannelCount; j++ )
			{
				*pOut++ = 0;
			}
		}
	}
	else
	{
		int nCopyChannels = std::min(nOutputChannelCount, nInputChannelCount);
		for ( int i = 0; i < nSampleCount; i++ )
		{
			float *pIn = pflChannel0 + i;
			for ( int j = 0; j < nCopyChannels; j++ )
			{
				int nOut = int( pIn[0] );
				nOut = clamp( nOut, -32768, 32767 );
				*pOut++ = nOut;
				pIn += nChannelStrideFloats;
			}
		}
	}
	contract_assert( nOutputChannelCount >= nInputChannelCount );
}

// constants for linear ramping
const float flMixBufferSizeInv = 1.0f / MIX_BUFFER_SIZE;
const fltx4 g_fl4_MixBufferSizeInv = { flMixBufferSizeInv, flMixBufferSizeInv, flMixBufferSizeInv, flMixBufferSizeInv };
const fltx4 g_fl4_Sequence1234 = { 1.0, 2.0, 3.0, 4.0 };


void ScaleBuffer( float flOutput[MIX_BUFFER_SIZE], const float input[MIX_BUFFER_SIZE], float scale )
{
	fltx4 volume = ReplicateX4(scale);
	fltx4 * __restrict__ pOut = (fltx4 *)&flOutput[0];
	fltx4 * __restrict__ pIn = (fltx4 *)&input[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 sample = LoadAlignedSIMD( pIn );
		StoreAlignedSIMD( (float *)pOut, MulSIMD( volume, sample ) );
		pOut++;
		pIn++;
	}
}

void ScaleBufferRamp( float flOutput[MIX_BUFFER_SIZE], const float flInput[MIX_BUFFER_SIZE], float flScaleStart, float flScaleEnd )
{
	fltx4 fl4Volume = ReplicateX4( flScaleStart );
	fltx4 fl4VolumeStep = MulSIMD( g_fl4_MixBufferSizeInv, SubSIMD( ReplicateX4( flScaleEnd ), fl4Volume ) );

	// offset volume by first ramp steps
	fl4Volume = AddSIMD( fl4Volume, MulSIMD( fl4VolumeStep, g_fl4_Sequence1234 ) );

	fltx4 fl4VolumeInc = MulSIMD( fl4VolumeStep, Four_Fours );

	fltx4 * __restrict__ pOut = (fltx4 *)&flOutput[0];
	fltx4 * __restrict__ pIn = (fltx4 *)&flInput[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE / 4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pIn );
		StoreAlignedSIMD( (float *)pOut, MulSIMD( fl4Volume, fl4Sample ) );
		pOut++;
		pIn++;
		fl4Volume = AddSIMD( fl4VolumeInc, fl4Volume );
	}
}

void SilenceBuffer( float flBuffer[MIX_BUFFER_SIZE] )
{
	fltx4 * __restrict__ pOut = (fltx4 *)&flBuffer[0];
	fltx4 fl4Zero = LoadZeroSIMD();
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		StoreAlignedSIMD( (float *)pOut, fl4Zero );
		pOut++;
	}
}

void SilenceBuffers( CAudioMixBuffer *pBuffers, int nBufferCount )
{
	for ( int i = 0; i < nBufferCount; i++ )
	{
		SilenceBuffer( pBuffers[i].m_flData );
	}
}

void MixBuffer( float flOutput[MIX_BUFFER_SIZE], const float flInput[MIX_BUFFER_SIZE], float scale )
{
	fltx4 fl4Volume = ReplicateX4(scale);
	fltx4 * __restrict__ pOut = (fltx4 *)&flOutput[0];
	fltx4 * __restrict__ pIn = (fltx4 *)&flInput[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pIn );
		fltx4 fl4Mix = LoadAlignedSIMD( pOut );
		StoreAlignedSIMD( (float *)pOut, MaddSIMD( fl4Volume, fl4Sample, fl4Mix ) );
		pOut++;
		pIn++;
	}
}

void MixBufferRamp( float flOutput[MIX_BUFFER_SIZE], const float flInput[MIX_BUFFER_SIZE], float flScaleStart, float flScaleEnd )
{
	fltx4 fl4Volume = ReplicateX4( flScaleStart );
	fltx4 fl4VolumeStep = MulSIMD( g_fl4_MixBufferSizeInv, SubSIMD( ReplicateX4( flScaleEnd ), fl4Volume ) );

	// offset volume by first ramp steps
	fl4Volume = AddSIMD( fl4Volume, MulSIMD( fl4VolumeStep, g_fl4_Sequence1234 ) );

	fltx4 fl4VolumeInc = MulSIMD( fl4VolumeStep, Four_Fours );

	fltx4 * __restrict__ pOut = (fltx4 *)&flOutput[0];
	fltx4 * __restrict__ pIn = (fltx4 *)&flInput[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE / 4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pIn );
		fltx4 fl4Mix = LoadAlignedSIMD( pOut );
		StoreAlignedSIMD( (float *)pOut, MaddSIMD( fl4Volume, fl4Sample, fl4Mix ) );
		pOut++;
		pIn++;
		fl4Volume = AddSIMD( fl4VolumeInc, fl4Volume );
	}
}

void SumBuffer2x1( float flOutput[MIX_BUFFER_SIZE], float flInput0[MIX_BUFFER_SIZE], float flScale0, float flInput1[MIX_BUFFER_SIZE], float flScale1 )
{
	fltx4 fl4Scale0 = ReplicateX4(flScale0);
	fltx4 fl4Scale1 = ReplicateX4(flScale1);
	fltx4 * __restrict__ pOut = (fltx4 *)&flOutput[0];
	fltx4 * __restrict__ pIn0 = (fltx4 *)&flInput0[0];
	fltx4 * __restrict__ pIn1 = (fltx4 *)&flInput1[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample0 = LoadAlignedSIMD( pIn0 );
		fltx4 fl4Sample1 = LoadAlignedSIMD( pIn1 );
		StoreAlignedSIMD( (float *)pOut, MaddSIMD( fl4Scale0, fl4Sample0, MulSIMD( fl4Scale1, fl4Sample1 ) ) );
		pOut++;
		pIn0++;
		pIn1++;
	}
}


void SwapBuffersInPlace( float flInput0[MIX_BUFFER_SIZE], float flInput1[MIX_BUFFER_SIZE] )
{
	fltx4 * __restrict__ pIn0 = (fltx4 *)&flInput0[0];
	fltx4 * __restrict__ pIn1 = (fltx4 *)&flInput1[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample0 = LoadAlignedSIMD( pIn0 );
		fltx4 fl4Sample1 = LoadAlignedSIMD( pIn1 );
		StoreAlignedSIMD( (float *)pIn0, fl4Sample1 );
		StoreAlignedSIMD( (float *)pIn1, fl4Sample0 );
		pIn0++;
		pIn1++;
	}
}

// UNDONE: OPTIMIZE: SIMD implementation
float BufferLevel( float flInput0[MIX_BUFFER_SIZE] )
{
	float flAbsMax = 0.0f;
	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		flAbsMax = Max( flAbsMax, (float)std::fabs(flInput0[i]) );
	}
	return flAbsMax;
}

float AvergeBufferAmplitude( float flInput0[MIX_BUFFER_SIZE] )
{
	float flTotal = 0;
	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		flTotal += std::fabs( flInput0[i] );
	}
	return flTotal * ( 1.0f / MIX_BUFFER_SIZE );
}
