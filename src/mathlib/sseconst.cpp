//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#include "mathlib/ssemath.h"
#include "mathlib/ssequaternion.h"
#include "mathlib/compressed_vector.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

const fltx4 Four_PointFives={0.5,0.5,0.5,0.5};
const fltx4 Four_Zeros={0.0,0.0,0.0,0.0};
const fltx4 Four_Ones={1.0,1.0,1.0,1.0};
const fltx4 Four_Twos={2.0,2.0,2.0,2.0};
const fltx4 Four_Threes={3.0,3.0,3.0,3.0};
const fltx4 Four_Fours={4.0,4.0,4.0,4.0};
const fltx4 Four_Origin={0,0,0,1};
const fltx4 Four_NegativeOnes={-1,-1,-1,-1};

const fltx4 Four_2ToThe21s={ (float) (1<<21), (float) (1<<21), (float) (1<<21), (float)(1<<21) };
const fltx4 Four_2ToThe22s={ (float) (1<<22), (float) (1<<22), (float) (1<<22), (float)(1<<22) };
const fltx4 Four_2ToThe23s={ (float) (1<<23), (float) (1<<23), (float) (1<<23), (float)(1<<23) };
const fltx4 Four_2ToThe24s={ (float) (1<<24), (float) (1<<24), (float) (1<<24), (float)(1<<24) };
const fltx4 Four_Thirds={ 0.33333333, 0.33333333, 0.33333333, 0.33333333 };
const fltx4 Four_TwoThirds={ 0.66666666, 0.66666666, 0.66666666, 0.66666666 };
const fltx4 Four_Point225s={ .225, .225, .225, .225 };
const fltx4 Four_Epsilons={FLT_EPSILON,FLT_EPSILON,FLT_EPSILON,FLT_EPSILON};
const fltx4 Four_DegToRad= { ((float)(M_PI_F / 180.f)), ((float)(M_PI_F / 180.f)), ((float)(M_PI_F / 180.f)), ((float)(M_PI_F / 180.f))};

const fltx4 Four_FLT_MAX={FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};
const fltx4 Four_Negative_FLT_MAX={-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX};
const fltx4 g_SIMD_0123 = { 0., 1., 2., 3. };

const fltx4 Four_LinearToGammaCoefficients_A = { -3.7295, -3.7295, -3.7295, -3.7295 };
const fltx4 Four_LinearToGammaCoefficients_B = { 8.9635,  8.9635,  8.9635,  8.9635 };
const fltx4 Four_LinearToGammaCoefficients_C = { -7.7397,  -7.7397,  -7.7397,  -7.7397 };
const fltx4 Four_LinearToGammaCoefficients_D = {3.443, 3.443, 3.443, 3.443 };
const fltx4 Four_LinearToGammaCoefficients_E = { 0.048, 0.048, 0.048, 0.048 };

const fltx4 Four_GammaToLinearCoefficients_A = { .1731, .1731, .1731, .1731 };
const fltx4 Four_GammaToLinearCoefficients_B = { .8717, .8717, .8717, .8717 };
const fltx4 Four_GammaToLinearCoefficients_C = { -.0452, -.0452, -.0452, -.0452 };
const fltx4 Four_GammaToLinearCoefficients_D = { .0012, .0012, .0012, .0012 };

const fltx4 g_QuatMultRowSign[4] =
{
	{  1.0f,  1.0f, -1.0f, 1.0f },
	{ -1.0f,  1.0f,  1.0f, 1.0f },
	{  1.0f, -1.0f,  1.0f, 1.0f },
	{ -1.0f, -1.0f, -1.0f, 1.0f }
};


const int32 ALIGN16 g_SIMD_clear_signmask[4] ALIGN16_POST = { (int)0x7fffffff, (int)0x7fffffff, (int)0x7fffffff, (int)0x7fffffff };
const int32 ALIGN16 g_SIMD_signmask[4] ALIGN16_POST = { (int)0x80000000, (int)0x80000000, (int)0x80000000, (int)0x80000000 };
const int32 ALIGN16 g_SIMD_lsbmask[4] ALIGN16_POST = { (int)0xfffffffe, (int)0xfffffffe, (int)0xfffffffe, (int)0xfffffffe };
const int32 ALIGN16 g_SIMD_clear_wmask[4] ALIGN16_POST = { (int)0xffffffff, (int)0xffffffff, (int)0xffffffff, 0 };
const int32 ALIGN16 g_SIMD_AllOnesMask[4] ALIGN16_POST = { (int)0xffffffff, (int)0xffffffff, (int)0xffffffff, (int)0xffffffff }; // ~0,~0,~0,~0
const int32 ALIGN16 g_SIMD_Low16BitsMask[4] ALIGN16_POST = { 0xffff, 0xffff, 0xffff, 0xffff }; // 0xffff x 4


const int32 ALIGN16 g_SIMD_ComponentMask[4][4] ALIGN16_POST =
{
	{ (int)0xFFFFFFFF, 0, 0, 0 }, { 0, (int)0xFFFFFFFF, 0, 0 }, { 0, 0, (int)0xFFFFFFFF, 0 }, { 0, 0, 0, (int)0xFFFFFFFF }
};

const fltx4 g_SIMD_Identity[4] =
{
	{ 1.0, 0, 0, 0 }, { 0, 1.0, 0, 0 }, { 0, 0, 1.0, 0 }, { 0, 0, 0, 1.0 }
};

const int32 ALIGN16 g_SIMD_SkipTailMask[4][4] ALIGN16_POST =
{
	{ (int)0xffffffff, (int)0xffffffff, (int)0xffffffff, (int)0xffffffff },
	{ (int)0xffffffff, 0x00000000, 0x00000000, 0x00000000 },
	{ (int)0xffffffff, (int)0xffffffff, 0x00000000, 0x00000000 },
	{ (int)0xffffffff, (int)0xffffffff, (int)0xffffffff, 0x00000000 },
};

const int32 ALIGN16 g_SIMD_EveryOtherMask[4] = { 0, ~0, 0, ~0 };



#ifdef PLATFORM_PPC

/// Passed as a parameter to vslh, shuffles the z component of a quat48 stored in the zw words left by one bit.
const uint16 ALIGN16 g_SIMD_Quat48_Unpack_Shift[] = { 
	0x00, 0x00,												// x word
	0x00, 0x00,												// y word
	0x00, 0x01,												// z word 
	0x00, 0x00 };											// w word 

// this permutes uint16's x,y,z packed in the most significant four halfwords of a fltx4 
// so that each gets its own word in the output. expected use is // __vperm( XX, Four_Threes, permute )
// -- that way each int is represented as 3.0 + n * 2^-22 , which we can pull into the 
// appropriate range with a single madd!
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute0[16] = 
{ 
	16, 17, 0, 1,											// word one:   00XX
	16, 17, 2, 3,											// word two:   00YY
	16, 17, 4, 5,											// word three: 00ZZ
	16, 17, 6, 7											// word four:  00WW
};

// the other permutes are a little trickier. note: I'm defining them out of order.
// 2 and 5 blend together prior results, rather than a source with 3.0f

// out1 = __vperm( x0y0z0x1y1z1x2y2, Four_Threes, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute1) ); // __x1__y1__z1____
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute1[16] = 
{	
	16, 17, 6, 7,											// word one:   00XX
	16, 17, 8, 9,											// word two:   00YY
	16, 17, 10, 11,											// word three: 00ZZ
	16, 17, 12, 13											// word four:  00WW
};

// out3 = __vperm( z2x3y3z3x4y4z4x5, Four_Threes, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute3) ); // __x3__y3__z3__z2  // z2 is important, goes into out2
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute3[16] = 
{	
	16, 17, 2, 3,   
	16, 17, 4, 5,    
	16, 17, 6, 7,  
	16, 17, 0, 1  
};

// out4 = __vperm( z2x3y3z3x4y4z4x5, Four_Threes, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute4) ); // __x4__y4__z4__x5  // x5 is important, goes into out5
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute4[16] = 
{	
	16, 17, 8, 9,    
	16, 17, 10, 11,  
	16, 17, 12, 13, 
	16, 17, 14, 15   
};

// out6 = __vperm( y5z5x6y6z6x7y7z7, Four_Threes, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute6) ); // __x6__y6__z6____
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute6[16] = 
{	
	16, 17, 4, 5,    // word one
	16, 17, 6, 7,  // word two
	16, 17, 8, 9,  // word three
	16, 17, 10, 11   // word four  (garbage)
};

// out7 = __vperm( y5z5x6y6z6x7y7z7, Four_Threes, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute7) ); // __x7__y7__z7____
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute7[16] = 
{	
	16, 17, 10, 11,    // word one
	16, 17, 12, 13,  // word two
	16, 17, 14, 15,  // word three
	16, 17, 16, 17   // word four  (garbage)
};

// these last two are tricky because we mix old output with source input. we get the 3.0f
// from the old output.
// out2 = __vperm( x0y0z0x1y1z1x2y2, out3, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute2)  ); // __x2__y2__z2____
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute2[16] = 
{	
	16, 17, 12, 13,  // 3.x2   
	16, 17, 14, 15,  // 3.y2
	16, 17, 30, 31,  // 3.z2 (from out2)
	16, 17, 16, 17   
};

// out5 = __vperm( y5z5x6y6z6x7y7z7, out4, *reinterpret_cast<const fltx4 *>(g_SIMD_Quat48_Unpack_Permute5)  ) // __x5__y5__z5____
const uint8 ALIGN16 g_SIMD_Quat48_Unpack_Permute5[16] = 
{	
	16, 17, 30, 31,  // 3.x5  (from out5)  
	16, 17,  0,  1,  // 3.y5
	16, 17,  2,  3,  // 3.z5 
	16, 17, 16, 17   // garbage   
};


// magic constants that we use to convert the unpacked q48 components from 2 + n * 2^-22 (where n = 0 .. 65535)
// to -1.0 .. 1
#define UnpackMul16s ( (1 << 22) / 32767.5 )
#define UnpackAdd16s ( ( -UnpackMul16s * 3.0 ) - 1 )
// we put the constants all into one word to save a little memory bandwidth
// but otherwise it would look like this:
// static const fltx4 vUpkMul = { UnpackMul16s, UnpackMul16s, UnpackMul16s, UnpackMul16s };
// static const fltx4 vUpkAdd = { UnpackAdd16s , UnpackAdd16s , UnpackAdd16s , UnpackAdd16s  };
const fltx4 g_SIMD_Quat48_Unpack_Magic_Constants = { UnpackMul16s , UnpackAdd16s, 0, 0 };
#undef UnpackMul16s
#undef UnpackAdd16s

#endif


	// FUNCTIONS
	// NOTE: WHY YOU **DO NOT** WANT TO PUT FUNCTIONS HERE
// Generally speaking, you want to make sure SIMD math functions
// are inlined, because that gives the compiler much more latitude
// in instruction scheduling. It's not that the overhead of calling
// the function is particularly great; rather, many of the SIMD 
// opcodes have long latencies, and if you have a sequence of 
// several dependent ones inside a function call, the latencies 
// stack up to create a big penalty. If the function is inlined,
// the compiler can interleave its operations with ones from the
// caller to better hide those latencies. Finally, on the 360,
// putting parameters or return values on the stack, and then 
// reading them back within the next forty cycles, is a very 
// severe penalty. So, as much as possible, you want to leave your
// data on the registers.

// That said, there are certain occasions where it is appropriate
// to call into functions -- particularly for very large blocks
// of code that will spill most of the registers anyway. Unless your
// function is more than one screen long, yours is probably not one
// of those occasions.


/// You can use this to rotate a long array of FourVectors all by the same
/// matrix. The first parameter is the head of the array. The second is the
/// number of vectors to rotate. The third is the matrix.
void FourVectors::RotateManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix )
{
	Assert(numVectors > 0);
	if ( numVectors == 0 )
		return;

	// Splat out each of the entries in the matrix to a fltx4. Do this
	// in the order that we will need them, to hide latency. I'm
	// avoiding making an array of them, so that they'll remain in 
	// registers.
	fltx4 matSplat00, matSplat01, matSplat02,
		matSplat10, matSplat11, matSplat12,
		matSplat20, matSplat21, matSplat22;

	{
		// Load the matrix into local vectors. Sadly, matrix3x4_ts are 
		// often unaligned. The w components will be the tranpose row of
		// the matrix, but we don't really care about that.
		fltx4 matCol0 = LoadUnalignedSIMD(rotationMatrix[0]);
		fltx4 matCol1 = LoadUnalignedSIMD(rotationMatrix[1]);
		fltx4 matCol2 = LoadUnalignedSIMD(rotationMatrix[2]);

		matSplat00 = SplatXSIMD(matCol0);
		matSplat01 = SplatYSIMD(matCol0);
		matSplat02 = SplatZSIMD(matCol0);

		matSplat10 = SplatXSIMD(matCol1);
		matSplat11 = SplatYSIMD(matCol1);
		matSplat12 = SplatZSIMD(matCol1);

		matSplat20 = SplatXSIMD(matCol2);
		matSplat21 = SplatYSIMD(matCol2);
		matSplat22 = SplatZSIMD(matCol2);
	}

	// PC does not benefit from the unroll/scheduling above
	fltx4 outX0, outY0, outZ0; // bank one of outputs


	// Because of instruction latencies and scheduling, it's actually faster to use adds and muls
	// rather than madds. (Empirically determined by timing.)
	const FourVectors * stop = pVectors + numVectors;

	// perform an even number of iterations through this loop.
	while (pVectors < stop)
	{
		outX0 = MaddSIMD( pVectors->z, matSplat02, AddSIMD( MulSIMD( pVectors->x, matSplat00 ), MulSIMD( pVectors->y, matSplat01 ) ) );
		outY0 = MaddSIMD( pVectors->z, matSplat12, AddSIMD( MulSIMD( pVectors->x, matSplat10 ), MulSIMD( pVectors->y, matSplat11 ) ) );
		outZ0 = MaddSIMD( pVectors->z, matSplat22, AddSIMD( MulSIMD( pVectors->x, matSplat20 ), MulSIMD( pVectors->y, matSplat21 ) ) );

		pVectors->x = outX0;
		pVectors->y = outY0;
		pVectors->z = outZ0;
		pVectors++;
	}
}

// Get the closest point from P to the (infinite) line through vLineA and vLineB and
// calculate the shortest distance from P to the line.
// If you pass in a value for t, it will tell you the t for (A + (B-A)t) to get the closest point.
// If the closest point lies on the segment between A and B, then 0 <= t <= 1.
void FourVectors::CalcClosestPointOnLineSIMD( const FourVectors &P, const FourVectors &vLineA, const FourVectors &vLineB, FourVectors &vClosest, fltx4 *outT)
{
	FourVectors vDir;
	fltx4 t = CalcClosestPointToLineTSIMD( P, vLineA, vLineB, vDir );
	if ( outT ) *outT = t;
	vClosest =  vDir;
	vClosest *= t;
	vClosest += vLineA;
}

fltx4 FourVectors::CalcClosestPointToLineTSIMD( const FourVectors &P, const FourVectors &vLineA, const FourVectors &vLineB, FourVectors &vDir )
{
	Assert( s_bMathlibInitialized );
	vDir = vLineB;
	vDir -= vLineA;

	fltx4 div = vDir * vDir;
	bi32x4 Mask;
	fltx4 Compare = ReplicateX4( 0.00001f );
	fltx4 result;
	Mask = CmpLtSIMD( div, Compare );

	result = DivSIMD( SubSIMD( vDir * P, vDir * vLineA ), div );

	MaskedAssign( Mask, Four_Zeros, result );
	return result;
}

void FourVectors::RotateManyBy(FourVectors * RESTRICT pVectors, unsigned int numVectors, const matrix3x4_t& rotationMatrix, FourVectors * RESTRICT pOut )
{
	Assert(numVectors > 0);
	if ( numVectors == 0 )
		return;

	// Splat out each of the entries in the matrix to a fltx4. Do this
	// in the order that we will need them, to hide latency. I'm
	// avoiding making an array of them, so that they'll remain in 
	// registers.
	fltx4 matSplat00, matSplat01, matSplat02,
		matSplat10, matSplat11, matSplat12,
		matSplat20, matSplat21, matSplat22;

	{
		// Load the matrix into local vectors. Sadly, matrix3x4_ts are 
		// often unaligned. The w components will be the tranpose row of
		// the matrix, but we don't really care about that.
		fltx4 matCol0 = LoadUnalignedSIMD(rotationMatrix[0]);
		fltx4 matCol1 = LoadUnalignedSIMD(rotationMatrix[1]);
		fltx4 matCol2 = LoadUnalignedSIMD(rotationMatrix[2]);

		matSplat00 = SplatXSIMD(matCol0);
		matSplat01 = SplatYSIMD(matCol0);
		matSplat02 = SplatZSIMD(matCol0);

		matSplat10 = SplatXSIMD(matCol1);
		matSplat11 = SplatYSIMD(matCol1);
		matSplat12 = SplatZSIMD(matCol1);

		matSplat20 = SplatXSIMD(matCol2);
		matSplat21 = SplatYSIMD(matCol2);
		matSplat22 = SplatZSIMD(matCol2);
	}

	// PC does not benefit from the unroll/scheduling above
	fltx4 outX0, outY0, outZ0; // bank one of outputs


	// Because of instruction latencies and scheduling, it's actually faster to use adds and muls
	// rather than madds. (Empirically determined by timing.)
	const FourVectors * stop = pVectors + numVectors;

	// perform an even number of iterations through this loop.
	while (pVectors < stop)
	{
		outX0 = MaddSIMD( pVectors->z, matSplat02, AddSIMD( MulSIMD( pVectors->x, matSplat00 ), MulSIMD( pVectors->y, matSplat01 ) ) );
		outY0 = MaddSIMD( pVectors->z, matSplat12, AddSIMD( MulSIMD( pVectors->x, matSplat10 ), MulSIMD( pVectors->y, matSplat11 ) ) );
		outZ0 = MaddSIMD( pVectors->z, matSplat22, AddSIMD( MulSIMD( pVectors->x, matSplat20 ), MulSIMD( pVectors->y, matSplat21 ) ) );

		pOut->x = outX0;
		pOut->y = outY0;
		pOut->z = outZ0;
		pVectors++;
		pOut++;
	}
}


// Transform many (horizontal) points in-place by a 3x4 matrix,
// here already loaded onto three fltx4 registers but not transposed. 
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1. 



