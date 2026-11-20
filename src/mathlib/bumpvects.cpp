// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.math.bumpvects;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.transform;
import ks.math.vector;
import ks.platform;
import ks.simd;
import std;

// z is coming out of the face.

void GetBumpNormals( const Vector& sVect, const Vector& tVect, const Vector& flatNormal, 
					 const Vector& phongNormal, Vector bumpNormals[NUM_BUMP_VECTS] )
{
	Vector tmpNormal;
	bool leftHanded;
	int i;

	contract_assert( NUM_BUMP_VECTS == 3 );
	
	// Are we left or right handed?
	CrossProduct( sVect, tVect, tmpNormal );
	if( DotProduct( flatNormal, tmpNormal ) < 0.0f )
	{
		leftHanded = true;
	}
	else
	{
		leftHanded = false;
	}

	// Build a basis for the face around the phong normal
	matrix3x4_t smoothBasis;
	CrossProduct( phongNormal.Base(), sVect.Base(), smoothBasis[1] );
	VectorNormalize( smoothBasis[1] );
	CrossProduct( smoothBasis[1], phongNormal.Base(), smoothBasis[0] );
	VectorNormalize( smoothBasis[0] );
	VectorCopy( phongNormal.Base(), smoothBasis[2] );
	
	if( leftHanded )
	{
		VectorNegate( smoothBasis[1] );
	}
	
	// move the g_localBumpBasis into world space to create bumpNormals
	for( i = 0; i < 3; i++ )
	{
		VectorIRotate( g_localBumpBasis[i], smoothBasis, bumpNormals[i] );
	}
}
